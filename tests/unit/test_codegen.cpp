#include <catch2/catch_test_macros.hpp>
#include "lexer/scanner.h"
#include "lexer/token.h"
#include "parser/parser.h"
#include "parser/ast.h"
#include "preprocessor/preprocessor.h"
#include "semantic/analyzer.h"
#include "ir/ir_generator.h"
#include "codegen/x86_generator.h"

#include <string>
#include <vector>

// Helper: compile source to asm string
static std::string compile_to_asm(const std::string& source) {
    Preprocessor pp(source);
    std::string processed = pp.process();
    Scanner scanner(processed);
    std::vector<Token> tokens;
    while (true) {
        Token tok = scanner.next_token();
        tokens.push_back(tok);
        if (tok.type == TokenType::END_OF_FILE) break;
    }
    Parser parser(tokens);
    auto ast = parser.parse();

    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);

    IRGenerator gen(analyzer.get_symbol_table(), analyzer.get_type_registry());
    IRProgram program = gen.generate(*ast);

    X86Generator x86gen;
    return x86gen.generate(program);
}

// ---- Basic codegen ----

TEST_CASE("Codegen: produces non-empty output", "[codegen]") {
    auto asm_code = compile_to_asm("fn main() -> int { return 0; }");
    CHECK(!asm_code.empty());
}

TEST_CASE("Codegen: contains section .text", "[codegen]") {
    auto asm_code = compile_to_asm("fn main() -> int { return 0; }");
    CHECK(asm_code.find("section .text") != std::string::npos);
}

TEST_CASE("Codegen: contains function label", "[codegen]") {
    auto asm_code = compile_to_asm("fn main() -> int { return 0; }");
    CHECK(asm_code.find("main:") != std::string::npos);
}

TEST_CASE("Codegen: contains global main", "[codegen]") {
    auto asm_code = compile_to_asm("fn main() -> int { return 0; }");
    CHECK(asm_code.find("global main") != std::string::npos);
}

TEST_CASE("Codegen: prologue push rbp", "[codegen]") {
    auto asm_code = compile_to_asm("fn main() -> int { return 0; }");
    CHECK(asm_code.find("push rbp") != std::string::npos);
    CHECK(asm_code.find("mov rbp, rsp") != std::string::npos);
}

TEST_CASE("Codegen: return uses leave", "[codegen]") {
    auto asm_code = compile_to_asm("fn main() -> int { return 0; }");
    CHECK(asm_code.find("leave") != std::string::npos);
    CHECK(asm_code.find("ret") != std::string::npos);
}

TEST_CASE("Codegen: multiple functions", "[codegen]") {
    auto asm_code = compile_to_asm(R"(
        fn foo() -> int { return 1; }
        fn main() -> int { return foo(); }
    )");
    CHECK(asm_code.find("foo:") != std::string::npos);
    CHECK(asm_code.find("main:") != std::string::npos);
    CHECK(asm_code.find("call foo") != std::string::npos);
}

TEST_CASE("Codegen: LSRA strategy compiles", "[codegen]") {
    Preprocessor pp("fn main() -> int { return 42; }");
    std::string processed = pp.process();
    Scanner scanner(processed);
    std::vector<Token> tokens;
    while (true) {
        Token tok = scanner.next_token();
        tokens.push_back(tok);
        if (tok.type == TokenType::END_OF_FILE) break;
    }
    Parser parser(tokens);
    auto ast = parser.parse();
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    IRGenerator gen(analyzer.get_symbol_table(), analyzer.get_type_registry());
    IRProgram program = gen.generate(*ast);

    X86Generator x86gen;
    x86gen.set_regalloc_strategy(RegAllocStrategy::LinearScan);
    auto asm_code = x86gen.generate(program);
    CHECK(!asm_code.empty());
    CHECK(asm_code.find("main:") != std::string::npos);
}

// ---- DWARF mode ----

TEST_CASE("Codegen: DWARF mode outputs GAS syntax", "[codegen][dwarf]") {
    Preprocessor pp("fn main() -> int { return 0; }");
    std::string processed = pp.process();
    Scanner scanner(processed);
    std::vector<Token> tokens;
    while (true) {
        Token tok = scanner.next_token();
        tokens.push_back(tok);
        if (tok.type == TokenType::END_OF_FILE) break;
    }
    Parser parser(tokens);
    auto ast = parser.parse();
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    IRGenerator gen(analyzer.get_symbol_table(), analyzer.get_type_registry());
    IRProgram program = gen.generate(*ast);

    X86Generator x86gen;
    x86gen.set_dwarf(true);
    x86gen.set_source_file("test.src");
    auto asm_code = x86gen.generate(program);

    CHECK(asm_code.find(".intel_syntax noprefix") != std::string::npos);
    CHECK(asm_code.find(".file 1 \"test.src\"") != std::string::npos);
    CHECK(asm_code.find(".globl main") != std::string::npos);
    CHECK(asm_code.find(".text") != std::string::npos);
    // Should NOT have NASM-specific syntax
    CHECK(asm_code.find("section .text") == std::string::npos);
}

TEST_CASE("Codegen: DWARF mode emits .loc", "[codegen][dwarf]") {
    Preprocessor pp("fn main() -> int { int x = 42; return x; }");
    std::string processed = pp.process();
    Scanner scanner(processed);
    std::vector<Token> tokens;
    while (true) {
        Token tok = scanner.next_token();
        tokens.push_back(tok);
        if (tok.type == TokenType::END_OF_FILE) break;
    }
    Parser parser(tokens);
    auto ast = parser.parse();
    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    IRGenerator gen(analyzer.get_symbol_table(), analyzer.get_type_registry());
    IRProgram program = gen.generate(*ast);

    X86Generator x86gen;
    x86gen.set_dwarf(true);
    x86gen.set_source_file("test.src");
    auto asm_code = x86gen.generate(program);

    // Should contain .loc directives
    CHECK(asm_code.find(".loc 1") != std::string::npos);
}
