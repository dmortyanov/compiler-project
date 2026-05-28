#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <algorithm>

#include "lexer/scanner.h"
#include "lexer/token.h"
#include "parser/parser.h"
#include "preprocessor/preprocessor.h"
#include "semantic/analyzer.h"
#include "ir/ir_generator.h"
#include "ir/ir_printer.h"
#include "codegen/x86_generator.h"

namespace fs = std::filesystem;

static std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) return "";
    return std::string((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
}

static std::vector<Token> tokenize(const std::string& source) {
    Preprocessor pp(source);
    std::string processed = pp.process();
    Scanner scanner(processed);
    std::vector<Token> tokens;
    while (true) {
        Token tok = scanner.next_token();
        tokens.push_back(tok);
        if (tok.type == TokenType::END_OF_FILE) break;
    }
    return tokens;
}

struct TestResult {
    std::string name;
    bool passed;
    std::string details;
};

// ---------------------------------------------------------------
// run_codegen_test — компилирует .src в ассемблер и проверяет:
//   1) Нет ошибок на стадиях lex/parse/semantic/IR
//   2) Генерируется непустой ассемблерный выход
//   3) Ассемблер содержит ключевые маркеры (section .text, push rbp)
//   4) Если есть .expected — сравнивает с ожидаемым выводом
// ---------------------------------------------------------------
static TestResult run_codegen_test(const fs::path& src_path) {
    TestResult result;
    result.name = src_path.filename().string();

    std::string source = read_file(src_path);
    if (source.empty()) {
        result.passed = false;
        result.details = "Failed to read source file";
        return result;
    }

    // Lex
    auto tokens = tokenize(source);

    // Parse
    Parser parser(tokens);
    auto ast = parser.parse();
    if (!parser.errors().empty()) {
        result.passed = false;
        result.details = "Parse errors:";
        for (const auto& e : parser.errors())
            result.details += "\n  " + std::to_string(e.line) + ":" +
                             std::to_string(e.column) + " " + e.message;
        return result;
    }

    // Semantic
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    if (!analyzer.get_errors().empty()) {
        result.passed = false;
        result.details = "Semantic errors:";
        for (const auto& e : analyzer.get_errors())
            result.details += "\n  " + e.message;
        return result;
    }

    // IR
    IRGenerator gen(analyzer.get_symbol_table(), analyzer.get_type_registry());
    IRProgram program = gen.generate(*ast);

    // Codegen
    X86Generator x86gen;
    std::string asm_output = x86gen.generate(program);

    if (asm_output.empty()) {
        result.passed = false;
        result.details = "Empty assembly output";
        return result;
    }

    // Проверяем наличие ключевых маркеров корректного NASM-файла
    bool has_text_section = asm_output.find("section .text") != std::string::npos;
    bool has_prologue     = asm_output.find("push rbp") != std::string::npos;
    bool has_ret          = asm_output.find("ret") != std::string::npos;
    bool has_main         = asm_output.find("global main") != std::string::npos;

    if (!has_text_section || !has_prologue || !has_ret || !has_main) {
        result.passed = false;
        result.details = "Missing assembly markers:";
        if (!has_text_section) result.details += " [section .text]";
        if (!has_prologue)     result.details += " [push rbp]";
        if (!has_ret)          result.details += " [ret]";
        if (!has_main)         result.details += " [global main]";
        result.details += "\n\nGenerated:\n" + asm_output.substr(0, 500);
        return result;
    }

    // Если есть .expected — сравнить побайтово (golden test)
    fs::path expected_path = src_path;
    expected_path.replace_extension(".expected");
    if (fs::exists(expected_path)) {
        std::string expected = read_file(expected_path);
        // Нормализуем: убираем trailing whitespace
        auto trim = [](const std::string& s) {
            size_t end = s.find_last_not_of(" \t\r\n");
            return (end == std::string::npos) ? "" : s.substr(0, end + 1);
        };
        if (trim(asm_output) != trim(expected)) {
            result.passed = false;
            result.details = "Assembly output mismatch with .expected";
            return result;
        }
    }

    result.passed = true;
    result.details = "OK (" + std::to_string(asm_output.size()) + " bytes)";
    return result;
}

static void collect_tests(const fs::path& dir, std::vector<fs::path>& tests) {
    if (!fs::exists(dir)) return;
    for (auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".src") {
            tests.push_back(entry.path());
        }
    }
    std::sort(tests.begin(), tests.end());
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: codegen_test_runner <test_dir>\n";
        return 1;
    }

    fs::path test_dir = argv[1];

    std::vector<fs::path> tests;
    collect_tests(test_dir, tests);

    int passed = 0;
    int failed = 0;
    std::vector<TestResult> failures;

    std::cout << "=== Codegen Tests (x86-64) ===\n\n";

    for (const auto& src : tests) {
        auto result = run_codegen_test(src);
        if (result.passed) {
            std::cout << "  PASS: " << result.name << " — " << result.details << "\n";
            passed++;
        } else {
            std::cout << "  FAIL: " << result.name << "\n";
            failed++;
            failures.push_back(result);
        }
    }

    std::cout << "\n=== Summary ===\n";
    std::cout << "Passed: " << passed << "  Failed: " << failed
              << "  Total: " << (passed + failed) << "\n";

    if (!failures.empty()) {
        std::cout << "\nFailure details:\n";
        for (const auto& f : failures) {
            std::cout << "  " << f.name << ":\n    " << f.details << "\n\n";
        }
    }

    return failed > 0 ? 1 : 0;
}
