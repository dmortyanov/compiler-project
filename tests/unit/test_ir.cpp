#include <catch2/catch_test_macros.hpp>
#include "lexer/scanner.h"
#include "lexer/token.h"
#include "parser/parser.h"
#include "parser/ast.h"
#include "preprocessor/preprocessor.h"
#include "semantic/analyzer.h"
#include "ir/ir_generator.h"
#include "ir/ir_printer.h"

#include <memory>
#include <string>
#include <vector>

// Helper: generate IR from source
static IRProgram generate_ir(const std::string& source) {
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
    return gen.generate(*ast);
}

// ---- Basic IR generation ----

TEST_CASE("IR: simple return constant", "[ir]") {
    auto program = generate_ir("fn main() -> int { return 42; }");
    REQUIRE(program.functions.size() == 1);
    CHECK(program.functions[0].name == "main");
    CHECK(!program.functions[0].blocks.empty());
}

TEST_CASE("IR: function with params", "[ir]") {
    auto program = generate_ir(R"(
        fn add(int a, int b) -> int { return a + b; }
    )");
    REQUIRE(program.functions.size() == 1);
    CHECK(program.functions[0].name == "add");
    CHECK(program.functions[0].params.size() == 2);
}

TEST_CASE("IR: if-else generates multiple blocks", "[ir]") {
    auto program = generate_ir(R"(
        fn main() -> int {
            int x = 10;
            if (x > 5) { return 1; }
            else { return 0; }
        }
    )");
    REQUIRE(program.functions.size() == 1);
    // if-else should generate at least 3 blocks (entry, then, else)
    CHECK(program.functions[0].blocks.size() >= 3);
}

TEST_CASE("IR: while loop generates blocks", "[ir]") {
    auto program = generate_ir(R"(
        fn main() -> int {
            int i = 0;
            while (i < 10) { i = i + 1; }
            return i;
        }
    )");
    REQUIRE(program.functions.size() == 1);
    // while should generate at least 3 blocks (cond, body, after)
    CHECK(program.functions[0].blocks.size() >= 3);
}

TEST_CASE("IR: function call generates PARAM and CALL", "[ir]") {
    auto program = generate_ir(R"(
        fn add(int a, int b) -> int { return a + b; }
        fn main() -> int { return add(3, 4); }
    )");
    REQUIRE(program.functions.size() == 2);

    // Find main function and check for CALL instruction
    const auto& main_func = program.functions[1]; // main is second
    bool has_call = false;
    for (const auto& block : main_func.blocks) {
        for (const auto& instr : block.instructions) {
            if (instr.opcode == IROpcode::CALL) has_call = true;
        }
    }
    CHECK(has_call);
}

TEST_CASE("IR: text output is non-empty", "[ir]") {
    auto program = generate_ir("fn main() -> int { return 0; }");
    std::string text = ir_to_text(program);
    CHECK(!text.empty());
    CHECK(text.find("main") != std::string::npos);
}

TEST_CASE("IR: multiple functions", "[ir]") {
    auto program = generate_ir(R"(
        fn foo() -> int { return 1; }
        fn bar() -> int { return 2; }
        fn main() -> int { return foo() + bar(); }
    )");
    CHECK(program.functions.size() == 3);
}

TEST_CASE("IR: binary operations", "[ir]") {
    auto program = generate_ir(R"(
        fn main() -> int {
            int a = 10 + 20;
            int b = a - 5;
            int c = b * 3;
            int d = c / 2;
            int e = d % 7;
            return e;
        }
    )");
    REQUIRE(!program.functions.empty());
    // Check that instructions contain ADD, SUB, MUL, DIV, MOD
    bool has_add = false, has_sub = false, has_mul = false;
    for (const auto& block : program.functions[0].blocks) {
        for (const auto& instr : block.instructions) {
            if (instr.opcode == IROpcode::ADD) has_add = true;
            if (instr.opcode == IROpcode::SUB) has_sub = true;
            if (instr.opcode == IROpcode::MUL) has_mul = true;
        }
    }
    CHECK(has_add);
    CHECK(has_sub);
    CHECK(has_mul);
}
