#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include "lexer/scanner.h"
#include "lexer/token.h"
#include "utils/file_utils.h"

namespace fs = std::filesystem;

static std::string read_optional_file(const fs::path& path) {
    if (!fs::exists(path)) {
        return "";
    }
    return utils::read_file(path.string());
}

static std::string normalize_newlines(const std::string& input) {
    std::string out;
    out.reserve(input.size());
    for (std::size_t i = 0; i < input.size(); ++i) {
        if (input[i] == '\r') {
            if (i + 1 < input.size() && input[i + 1] == '\n') {
                ++i;
            }
            out.push_back('\n');
        } else {
            out.push_back(input[i]);
        }
    }
    return out;
}

static bool compare_text(const std::string& expected, const std::string& actual,
                         const std::string& label) {
    if (expected == actual) {
        return true;
    }
    std::cerr << "Mismatch in " << label << ":\n";
    std::istringstream exp_stream(expected);
    std::istringstream act_stream(actual);
    std::string exp_line;
    std::string act_line;
    int line_no = 1;
    while (true) {
        bool exp_ok = static_cast<bool>(std::getline(exp_stream, exp_line));
        bool act_ok = static_cast<bool>(std::getline(act_stream, act_line));
        if (!exp_ok && !act_ok) {
            break;
        }
        if (exp_line != act_line) {
            std::cerr << "Line " << line_no << ":\n";
            std::cerr << "  expected: " << exp_line << "\n";
            std::cerr << "  actual  : " << act_line << "\n";
            return false;
        }
        ++line_no;
    }
    return true;
}

static bool run_test_file(const fs::path& src_path) {
    std::string source = utils::read_file(src_path.string());
    Scanner scanner(source);

    std::string tokens_out;
    while (true) {
        Token token = scanner.next_token();
        tokens_out += format_token(token);
        tokens_out += "\n";
        if (token.type == TokenType::END_OF_FILE) {
            break;
        }
    }

    std::string errors_out;
    for (const auto& err : scanner.errors()) {
        errors_out += std::to_string(err.line) + ":" +
                      std::to_string(err.column) + " ERROR " + err.message +
                      "\n";
    }

    fs::path expected_tokens = src_path;
    expected_tokens.replace_extension(".tok");
    fs::path expected_errors = src_path;
    expected_errors.replace_extension(".err");

    std::string expected_tokens_content =
        normalize_newlines(read_optional_file(expected_tokens));
    std::string expected_errors_content =
        normalize_newlines(read_optional_file(expected_errors));

    bool ok = true;
    ok &= compare_text(expected_tokens_content, normalize_newlines(tokens_out),
                       expected_tokens.string());
    ok &= compare_text(expected_errors_content, normalize_newlines(errors_out),
                       expected_errors.string());
    return ok;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cerr << "Usage: lexer_test_runner <valid_dir> <invalid_dir>\n";
        return 1;
    }

    fs::path valid_dir = argv[1];
    fs::path invalid_dir = argv[2];

    bool all_ok = true;
    for (const auto& entry : fs::directory_iterator(valid_dir)) {
        if (entry.is_regular_file() &&
            entry.path().extension() == ".src") {
            bool ok = run_test_file(entry.path());
            std::cout << (ok ? "[PASS] " : "[FAIL] ")
                      << entry.path().filename().string() << "\n";
            all_ok &= ok;
        }
    }
    for (const auto& entry : fs::directory_iterator(invalid_dir)) {
        if (entry.is_regular_file() &&
            entry.path().extension() == ".src") {
            bool ok = run_test_file(entry.path());
            std::cout << (ok ? "[PASS] " : "[FAIL] ")
                      << entry.path().filename().string() << "\n";
            all_ok &= ok;
        }
    }

    return all_ok ? 0 : 1;
}
