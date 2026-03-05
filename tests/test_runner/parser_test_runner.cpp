#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "lexer/scanner.h"
#include "lexer/token.h"
#include "parser/ast.h"
#include "parser/ast_printer.h"
#include "parser/parser.h"
#include "preprocessor/preprocessor.h"
#include "utils/file_utils.h"

namespace fs = std::filesystem;

static std::string normalize(const std::string& s) {
    std::string out;
    out.reserve(s.size());
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\r') {
            if (i + 1 < s.size() && s[i + 1] == '\n') ++i;
            out.push_back('\n');
        } else {
            out.push_back(s[i]);
        }
    }
    while (!out.empty() && out.back() == '\n') out.pop_back();
    return out;
}

static std::string read_opt(const fs::path& p) {
    if (!fs::exists(p)) return "";
    return utils::read_file(p.string());
}

static bool compare(const std::string& expected, const std::string& actual,
                    const std::string& label) {
    if (expected == actual) return true;
    std::cerr << "Mismatch in " << label << ":\n";
    std::istringstream es(expected), as(actual);
    std::string el, al;
    int n = 1;
    while (true) {
        bool eo = static_cast<bool>(std::getline(es, el));
        bool ao = static_cast<bool>(std::getline(as, al));
        if (!eo && !ao) break;
        if (el != al) {
            std::cerr << "Line " << n << ":\n  expected: " << el
                      << "\n  actual  : " << al << "\n";
            return false;
        }
        ++n;
    }
    return true;
}

static bool run_test(const fs::path& src) {
    std::string source = utils::read_file(src.string());

    Preprocessor pre(source);
    std::string processed = pre.process();

    Scanner scanner(processed);
    std::vector<Token> tokens;
    while (true) {
        Token t = scanner.next_token();
        tokens.push_back(t);
        if (t.type == TokenType::END_OF_FILE) break;
    }

    Parser parser(tokens);
    auto ast = parser.parse();

    ASTPrettyPrinter pp;
    ast->accept(pp);
    std::string ast_out = normalize(pp.result());

    std::string errors_out;
    for (const auto& e : pre.errors()) {
        errors_out += std::to_string(e.line) + ":" + std::to_string(e.column) +
                      " ERROR " + e.message + "\n";
    }
    for (const auto& e : scanner.errors()) {
        errors_out += std::to_string(e.line) + ":" + std::to_string(e.column) +
                      " ERROR " + e.message + "\n";
    }
    for (const auto& e : parser.errors()) {
        errors_out += std::to_string(e.line) + ":" + std::to_string(e.column) +
                      " ERROR " + e.message + "\n";
    }
    errors_out = normalize(errors_out);

    fs::path exp_ast = src;
    exp_ast.replace_extension(".expected");
    fs::path exp_err = src;
    exp_err.replace_extension(".err");

    std::string expected_ast = normalize(read_opt(exp_ast));
    std::string expected_err = normalize(read_opt(exp_err));

    bool ok = true;
    if (!expected_ast.empty()) {
        ok &= compare(expected_ast, ast_out, exp_ast.string());
    }
    if (!expected_err.empty()) {
        ok &= compare(expected_err, errors_out, exp_err.string());
    }
    if (expected_ast.empty() && expected_err.empty()) {
        if (!parser.errors().empty()) {
            ok = false;
            std::cerr << "Unexpected parse errors in " << src.string() << "\n";
        }
    }
    return ok;
}

static void scan_dir(const fs::path& dir, bool& all_ok) {
    if (!fs::exists(dir)) return;
    for (const auto& entry : fs::recursive_directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".src") {
            bool ok = run_test(entry.path());
            std::cout << (ok ? "[PASS] " : "[FAIL] ")
                      << entry.path().filename().string() << "\n";
            all_ok &= ok;
        }
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: parser_test_runner <valid_dir> <invalid_dir>\n";
        return 1;
    }
    bool all_ok = true;
    scan_dir(argv[1], all_ok);
    scan_dir(argv[2], all_ok);
    return all_ok ? 0 : 1;
}
