#include <fstream>
#include <iostream>
#include <iterator>

#include "lexer/scanner.h"
#include "lexer/token.h"
#include "preprocessor/preprocessor.h"

static void print_usage() {
    std::cout << "Usage:\n"
              << "  compiler lex --input <file> [--output <file>]\n";
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];
    if (command != "lex") {
        print_usage();
        return 1;
    }

    std::string input_path;
    std::string output_path;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            input_path = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        }
    }

    if (input_path.empty()) {
        print_usage();
        return 1;
    }

    std::ifstream in(input_path, std::ios::in | std::ios::binary);
    if (!in) {
        std::cerr << "Failed to read input file: " << input_path << "\n";
        return 1;
    }
    std::string source((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());

    Preprocessor preprocessor(source);
    std::string processed = preprocessor.process();

    for (const auto& err : preprocessor.errors()) {
        std::cerr << err.line << ":" << err.column << " ERROR "
                  << err.message << "\n";
    }

    Scanner scanner(processed);
    std::string output;
    while (true) {
        Token token = scanner.next_token();
        output += format_token(token);
        output += "\n";
        if (token.type == TokenType::END_OF_FILE) {
            break;
        }
    }

    for (const auto& err : scanner.errors()) {
        std::cerr << err.line << ":" << err.column << " ERROR "
                  << err.message << "\n";
    }

    if (output_path.empty()) {
        std::cout << output;
        return 0;
    }

    std::ofstream out(output_path, std::ios::out | std::ios::binary);
    if (!out) {
        std::cerr << "Failed to write output file: " << output_path << "\n";
        return 1;
    }
    out << output;
    return 0;
}
