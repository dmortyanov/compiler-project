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
#include "semantic/errors.h"

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

// Run a valid test: expect no semantic errors
static TestResult run_valid_test(const fs::path& src_path,
                                  const fs::path& expected_path) {
    TestResult result;
    result.name = src_path.string();

    std::string source = read_file(src_path);
    if (source.empty()) {
        result.passed = false;
        result.details = "Failed to read source file";
        return result;
    }

    auto tokens = tokenize(source);
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

    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);

    std::string output = format_error_report(analyzer.get_errors());
    std::string expected = read_file(expected_path);

    if (trim(output) == trim(expected)) {
        result.passed = true;
    } else {
        result.passed = false;
        result.details = "Output mismatch:\n  Expected: " + trim(expected) +
                        "\n  Got:      " + trim(output);
    }
    return result;
}

// Run an invalid test: expect at least one error containing the expected kind
static TestResult run_invalid_test(const fs::path& src_path,
                                    const fs::path& expected_path) {
    TestResult result;
    result.name = src_path.string();

    std::string source = read_file(src_path);
    if (source.empty()) {
        result.passed = false;
        result.details = "Failed to read source file";
        return result;
    }

    auto tokens = tokenize(source);
    Parser parser(tokens);
    auto ast = parser.parse();

    if (!parser.errors().empty()) {
        result.passed = false;
        result.details = "Parse errors (test should be syntactically valid):";
        for (const auto& e : parser.errors())
            result.details += "\n  " + std::to_string(e.line) + ":" +
                             std::to_string(e.column) + " " + e.message;
        return result;
    }

    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);

    if (analyzer.get_errors().empty()) {
        result.passed = false;
        result.details = "Expected semantic error but got none";
        return result;
    }

    // Read expected full output
    std::string output = format_error_report(analyzer.get_errors());
    std::string expected = read_file(expected_path);

    if (trim(output) == trim(expected)) {
        result.passed = true;
    } else {
        result.passed = false;
        result.details = "Output mismatch:\n  Expected:\n" + trim(expected) +
                        "\n  Got:\n" + trim(output);
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
        std::cerr << "Usage: semantic_test_runner <valid_dir> <invalid_dir>\n";
        return 1;
    }

    fs::path valid_dir = argv[1];
    fs::path invalid_dir = argv[2];

    std::vector<std::pair<fs::path, fs::path>> valid_tests;
    std::vector<std::pair<fs::path, fs::path>> invalid_tests;

    collect_tests(valid_dir, valid_tests);
    collect_tests(invalid_dir, invalid_tests);

    int passed = 0;
    int failed = 0;
    std::vector<TestResult> failures;

    std::cout << "=== Semantic Tests ===\n\n";

    // Valid tests
    std::cout << "--- Valid programs (should have no errors) ---\n";
    for (const auto& [src, exp] : valid_tests) {
        auto result = run_valid_test(src, exp);
        if (result.passed) {
            std::cout << "  PASS: " << src.filename() << "\n";
            passed++;
        } else {
            std::cout << "  FAIL: " << src.filename() << "\n";
            failed++;
            failures.push_back(result);
        }
    }

    // Invalid tests
    std::cout << "\n--- Invalid programs (should produce errors) ---\n";
    for (const auto& [src, exp] : invalid_tests) {
        auto result = run_invalid_test(src, exp);
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
