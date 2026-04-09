#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "lexer/scanner.h"
#include "lexer/token.h"
#include "parser/parser.h"
#include "preprocessor/preprocessor.h"
#include "semantic/analyzer.h"
#include "ir/ir_generator.h"
#include "ir/ir_printer.h"

namespace fs = std::filesystem;

static std::string read_file(const fs::path& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in) return "";
    return std::string((std::istreambuf_iterator<char>(in)),
                        std::istreambuf_iterator<char>());
}

static std::string trim(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
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

// Run a golden test: generate IR, compare with expected output
static TestResult run_ir_test(const fs::path& src_path,
                               const fs::path& expected_path) {
    TestResult result;
    result.name = src_path.string();

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

    // Semantic analysis
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    if (!analyzer.get_errors().empty()) {
        result.passed = false;
        result.details = "Semantic errors:";
        for (const auto& e : analyzer.get_errors())
            result.details += "\n  " + e.message;
        return result;
    }

    // IR generation
    IRGenerator gen(analyzer.get_symbol_table(), analyzer.get_type_registry());
    IRProgram program = gen.generate(*ast);

    std::string output = ir_to_text(program);
    std::string expected = read_file(expected_path);

    if (trim(output) == trim(expected)) {
        result.passed = true;
    } else {
        result.passed = false;
        result.details = "IR output mismatch:\n--- Expected ---\n" + trim(expected) +
                        "\n--- Got ---\n" + trim(output);
    }
    return result;
}

static void collect_tests(const fs::path& dir,
                           std::vector<std::pair<fs::path, fs::path>>& tests) {
    if (!fs::exists(dir)) return;
    for (auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".src") {
            fs::path expected = entry.path();
            expected.replace_extension(".expected");
            if (fs::exists(expected)) {
                tests.push_back({entry.path(), expected});
            }
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: ir_test_runner <generation_dir> <validation_dir>\n";
        return 1;
    }

    fs::path gen_dir = argv[1];
    fs::path val_dir = argv[2];

    std::vector<std::pair<fs::path, fs::path>> gen_tests;
    std::vector<std::pair<fs::path, fs::path>> val_tests;

    collect_tests(gen_dir, gen_tests);
    collect_tests(val_dir, val_tests);

    int passed = 0;
    int failed = 0;
    std::vector<TestResult> failures;

    std::cout << "=== IR Tests ===\n\n";

    // Generation tests
    std::cout << "--- Generation tests ---\n";
    for (const auto& [src, exp] : gen_tests) {
        auto result = run_ir_test(src, exp);
        if (result.passed) {
            std::cout << "  PASS: " << src.filename() << "\n";
            passed++;
        } else {
            std::cout << "  FAIL: " << src.filename() << "\n";
            failed++;
            failures.push_back(result);
        }
    }

    // Validation tests
    std::cout << "\n--- Validation tests ---\n";
    for (const auto& [src, exp] : val_tests) {
        auto result = run_ir_test(src, exp);
        if (result.passed) {
            std::cout << "  PASS: " << src.filename() << "\n";
            passed++;
        } else {
            std::cout << "  FAIL: " << src.filename() << "\n";
            failed++;
            failures.push_back(result);
        }
    }

    // Summary
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
