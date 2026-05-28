#include <catch2/catch_test_macros.hpp>
#include "lexer/scanner.h"
#include "lexer/token.h"
#include "parser/parser.h"
#include "parser/ast.h"
#include "preprocessor/preprocessor.h"

#include <memory>
#include <vector>

// Helper: parse source string
static std::pair<std::unique_ptr<ProgramNode>, std::vector<ParseError>>
parse_source(const std::string& source) {
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
    return {std::move(ast), parser.errors()};
}

// ---- Basic function ----

TEST_CASE("Parser: empty function", "[parser]") {
    auto [ast, errors] = parse_source("fn main() -> int { return 0; }");
    CHECK(errors.empty());
    REQUIRE(ast != nullptr);
    CHECK(!ast->declarations.empty());
}

// ---- Variable declaration ----

TEST_CASE("Parser: variable declaration", "[parser]") {
    auto [ast, errors] = parse_source("fn main() -> int { int x = 42; return x; }");
    CHECK(errors.empty());
    REQUIRE(ast != nullptr);
}

// ---- If-else ----

TEST_CASE("Parser: if-else", "[parser]") {
    auto [ast, errors] = parse_source(R"(
        fn main() -> int {
            if (1 > 0) { return 1; }
            else { return 0; }
        }
    )");
    CHECK(errors.empty());
}

// ---- While loop ----

TEST_CASE("Parser: while loop", "[parser]") {
    auto [ast, errors] = parse_source(R"(
        fn main() -> int {
            int i = 0;
            while (i < 10) { i = i + 1; }
            return i;
        }
    )");
    CHECK(errors.empty());
}

// ---- For loop ----

TEST_CASE("Parser: for loop", "[parser]") {
    auto [ast, errors] = parse_source(R"(
        fn main() -> int {
            int sum = 0;
            for (int i = 0; i < 5; i = i + 1) {
                sum = sum + i;
            }
            return sum;
        }
    )");
    CHECK(errors.empty());
}

// ---- Function with parameters ----

TEST_CASE("Parser: function with params", "[parser]") {
    auto [ast, errors] = parse_source(R"(
        fn add(int a, int b) -> int {
            return a + b;
        }
        fn main() -> int {
            return add(3, 4);
        }
    )");
    CHECK(errors.empty());
}

// ---- Nested expressions ----

TEST_CASE("Parser: nested expressions", "[parser]") {
    auto [ast, errors] = parse_source(R"(
        fn main() -> int {
            int x = (1 + 2) * (3 - 4) / 5;
            return x;
        }
    )");
    CHECK(errors.empty());
}

// ---- Parse error: missing semicolon ----

TEST_CASE("Parser: error on missing semicolon", "[parser]") {
    auto [ast, errors] = parse_source("fn main() -> int { int x = 5 return x; }");
    CHECK(!errors.empty());
}

// ---- Parse error: missing closing brace ----

TEST_CASE("Parser: error on missing brace", "[parser]") {
    auto [ast, errors] = parse_source("fn main() -> int { int x = 5;");
    CHECK(!errors.empty());
}

// ---- Array declaration ----

TEST_CASE("Parser: array declaration", "[parser]") {
    auto [ast, errors] = parse_source(R"(
        fn main() -> int {
            int arr[5] = {1, 2, 3, 4, 5};
            return arr[0];
        }
    )");
    CHECK(errors.empty());
}

// ---- Extern function ----

TEST_CASE("Parser: extern function", "[parser]") {
    auto [ast, errors] = parse_source(R"(
        extern fn printf(string format) -> int;
        fn main() -> int {
            return 0;
        }
    )");
    CHECK(errors.empty());
}

// ---- Struct ----

TEST_CASE("Parser: struct declaration", "[parser]") {
    auto [ast, errors] = parse_source(R"(
        struct Point {
            int x;
            int y;
        }
        fn main() -> int { return 0; }
    )");
    CHECK(errors.empty());
}

// ---- Boolean operators ----

TEST_CASE("Parser: boolean operators", "[parser]") {
    auto [ast, errors] = parse_source(R"(
        fn main() -> int {
            bool a = true && false;
            bool b = true || false;
            bool c = !true;
            if (a || b && c) { return 1; }
            return 0;
        }
    )");
    CHECK(errors.empty());
}
