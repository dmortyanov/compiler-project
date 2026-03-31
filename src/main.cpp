#include <fstream>
#include <iostream>
#include <iterator>
#include <vector>

#include "lexer/scanner.h"
#include "lexer/token.h"
#include "parser/ast.h"
#include "parser/ast_printer.h"
#include "parser/parser.h"
#include "parser/symbol_table.h"
#include "preprocessor/preprocessor.h"
#include "semantic/analyzer.h"
#include "semantic/errors.h"

static void print_usage() {
    std::cout << "Usage:\n";
    std::cout << "  compiler lex      --input <file> [--output <file>]\n";
    std::cout << "  compiler parse    --input <file> [--output <file>] [--format text|dot|json] [--verbose]\n";
    std::cout << "  compiler check    --input <file> [--output <file>] [--verbose] [--show-types]\n";
    std::cout << "  compiler symbols  --input <file> [--format text|json] [--output <file>]\n";
}

static std::string read_source(const std::string& path) {
    std::ifstream in(path, std::ios::in | std::ios::binary);
    if (!in)
        return "";
    return std::string((std::istreambuf_iterator<char>(in)),
                       std::istreambuf_iterator<char>());
}

static bool write_output(const std::string& path, const std::string& data) {
    std::ofstream out(path, std::ios::out | std::ios::binary);
    if (!out) return false;
    out << data;
    return true;
}

static std::vector<Token> tokenize(const std::string& source,
                                   bool report_errors) {
    Preprocessor preprocessor(source);
    std::string processed = preprocessor.process();
    if (report_errors) {
        for (const auto& err : preprocessor.errors()) {
            std::cerr << err.line << ":" << err.column << " ERROR "
                      << err.message << "\n";
        }
    }

    Scanner scanner(processed);
    std::vector<Token> tokens;
    while (true) {
        Token tok = scanner.next_token();
        tokens.push_back(tok);
        if (tok.type == TokenType::END_OF_FILE) break;
    }
    if (report_errors) {
        for (const auto& err : scanner.errors()) {
            std::cerr << err.line << ":" << err.column << " ERROR "
                      << err.message << "\n";
        }
    }
    return tokens;
}

static int cmd_lex(const std::string& input_path,
                   const std::string& output_path) {
    std::string source = read_source(input_path);
    if (source.empty()) {
        std::ifstream test(input_path);
        if (!test) {
            std::cerr << "Failed to read input file: " << input_path << "\n";
            return 1;
        }
    }
    auto tokens = tokenize(source, true);
    std::string output;
    for (const auto& tok : tokens) {
        output += format_token(tok);
        output += "\n";
    }
    if (output_path.empty()) {
        std::cout << output;
    } else if (!write_output(output_path, output)) {
        std::cerr << "Failed to write output file: " << output_path << "\n";
        return 1;
    }
    return 0;
}

static int cmd_parse(const std::string& input_path,
                     const std::string& output_path,
                     const std::string& format, bool verbose) {
    std::string source = read_source(input_path);
    if (source.empty()) {
        std::ifstream test(input_path);
        if (!test) {
            std::cerr << "Failed to read input file: " << input_path << "\n";
            return 1;
        }
    }
    auto tokens = tokenize(source, true);

    if (verbose) {
        std::cerr << "Tokens: " << tokens.size() << "\n";
    }

    Parser parser(tokens);
    auto ast = parser.parse();

    for (const auto& err : parser.errors()) {
        std::cerr << err.line << ":" << err.column << " ERROR "
                  << err.message << "\n";
    }

    for (const auto& err : parser.errors()) {
    if (!err.suggestion.empty()) {
        std::cerr << err.line << ":" << err.column << " SUGGESTION: " 
                  << err.suggestion << "\n";
    }
}

    if (verbose) {
        const auto& m = parser.metrics();
        std::cerr << "Parse errors: " << m.total_errors << "\n";
        std::cerr << "Recovered: " << m.recovered << "\n";
        std::cerr << "Tokens skipped: " << m.tokens_skipped << "\n";
        std::cerr << "Recovery rate: " << (m.recovery_rate() * 100) << "%\n";

        auto sym = build_symbol_tables(*ast);
        for (const auto& e : sym.errors) {
            std::cerr << "SEMANTIC " << e << "\n";
        }
        std::cerr << "Scopes: " << sym.scopes.size() << "\n";
        for (const auto& scope : sym.scopes) {
            std::cerr << "  scope#" << scope.id << " parent=" << scope.parent_id
                      << " symbols=" << scope.symbols.size() << "\n";
        }
    }

    std::string output;
    if (format == "dot") {
        ASTDotPrinter dot;
        ast->accept(dot);
        output = dot.result();
    } else if (format == "json") {
        ASTJsonPrinter json;
        ast->accept(json);
        output = json.result();
        output += "\n";
    } else {
        ASTPrettyPrinter pp;
        ast->accept(pp);
        output = pp.result();
    }

    if (output_path.empty()) {
        std::cout << output;
    } else if (!write_output(output_path, output)) {
        std::cerr << "Failed to write output file: " << output_path << "\n";
        return 1;
    }
    return 0;
}

// ---------------------------------------------------------------
// Sprint 3: semantic check command
// ---------------------------------------------------------------
static int cmd_check(const std::string& input_path,
                     const std::string& output_path,
                     bool verbose, bool show_types) {
    std::string source = read_source(input_path);
    if (source.empty()) {
        std::ifstream test(input_path);
        if (!test) {
            std::cerr << "Failed to read input file: " << input_path << "\n";
            return 1;
        }
    }

    auto tokens = tokenize(source, true);

    Parser parser(tokens);
    auto ast = parser.parse();

    if (!parser.errors().empty()) {
        for (const auto& err : parser.errors()) {
            std::cerr << err.line << ":" << err.column << " PARSE ERROR: "
                      << err.message << "\n";
        }
        std::cerr << "Cannot run semantic analysis: parse errors present\n";
        return 1;
    }

    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);

    std::string output;

    // Error report
    output += format_error_report(analyzer.get_errors());

    // Verbose: validation report
    if (verbose) {
        output += "\n" + analyzer.validation_report();
    }

    // Decorated AST with types
    if (show_types) {
        output += "\n=== Decorated AST ===\n";
        output += analyzer.dump_decorated_ast(*ast);
    }

    if (output_path.empty()) {
        std::cout << output;
    } else if (!write_output(output_path, output)) {
        std::cerr << "Failed to write output file: " << output_path << "\n";
        return 1;
    }

    return analyzer.get_errors().empty() ? 0 : 1;
}

// ---------------------------------------------------------------
// Sprint 3: symbol table dump command
// ---------------------------------------------------------------
static int cmd_symbols(const std::string& input_path,
                       const std::string& output_path,
                       const std::string& format) {
    std::string source = read_source(input_path);
    if (source.empty()) {
        std::ifstream test(input_path);
        if (!test) {
            std::cerr << "Failed to read input file: " << input_path << "\n";
            return 1;
        }
    }

    auto tokens = tokenize(source, true);

    Parser parser(tokens);
    auto ast = parser.parse();

    if (!parser.errors().empty()) {
        for (const auto& err : parser.errors()) {
            std::cerr << err.line << ":" << err.column << " PARSE ERROR: "
                      << err.message << "\n";
        }
        return 1;
    }

    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);

    std::string output;
    if (format == "json") {
        output = analyzer.get_symbol_table().dump_json();
        output += "\n";
    } else {
        output = analyzer.get_symbol_table().dump_text();
    }

    if (output_path.empty()) {
        std::cout << output;
    } else if (!write_output(output_path, output)) {
        std::cerr << "Failed to write output file: " << output_path << "\n";
        return 1;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return 1;
    }

    std::string command = argv[1];
    std::string input_path;
    std::string output_path;
    std::string format = "text";
    bool verbose = false;
    bool show_types = false;

    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--input" && i + 1 < argc) {
            input_path = argv[++i];
        } else if (arg == "--output" && i + 1 < argc) {
            output_path = argv[++i];
        } else if (arg == "--format" && i + 1 < argc) {
            format = argv[++i];
        } else if (arg == "--verbose") {
            verbose = true;
        } else if (arg == "--show-types") {
            show_types = true;
        }
    }

    if (input_path.empty()) {
        print_usage();
        return 1;
    }

    if (command == "lex") {
        return cmd_lex(input_path, output_path);
    }
    if (command == "parse") {
        return cmd_parse(input_path, output_path, format, verbose);
    }
    if (command == "check") {
        return cmd_check(input_path, output_path, verbose, show_types);
    }
    if (command == "symbols") {
        return cmd_symbols(input_path, output_path, format);
    }

    print_usage();
    return 1;
}
