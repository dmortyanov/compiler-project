#include <filesystem>
#include <iostream>
#include <sstream>
#include <string>

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
            std::cerr << "Line " << n << ":\n";
            std::cerr << "  expected: " << el << "\n";
            std::cerr << "  actual  : " << al << "\n";
            return false;
        }
        ++n;
    }
    return true;
}

static std::vector<Token> tokenize(const std::string& source) {
    Preprocessor pre(source);
    std::string processed = pre.process();
    Scanner scanner(processed);
    std::vector<Token> tokens;
    while (true) {
        Token t = scanner.next_token();
        tokens.push_back(t);
        if (t.type == TokenType::END_OF_FILE) break;
    }
    return tokens;
}

static bool run_one(const fs::path& src) {
    fs::path expected = src;
    expected.replace_extension(".expected");
    std::string source = utils::read_file(src.string());
    std::string exp = normalize(utils::read_file(expected.string()));

    auto tokens = tokenize(source);
    Parser parser(tokens);
    auto ast = parser.parse();

    ASTPrettyPrinter pp;
    ast->accept(pp);
    std::string got = normalize(pp.result());

    bool ok = compare(exp, got, expected.string());
    std::cout << (ok ? "[PASS] " : "[FAIL] ") << src.filename().string()
              << "\n";
    return ok;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: integration_test_runner <examples_dir>\n";
        return 1;
    }
    fs::path dir = argv[1];
    bool all_ok = true;
    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".src") {
            all_ok &= run_one(entry.path());
        }
    }
    return all_ok ? 0 : 1;
}

