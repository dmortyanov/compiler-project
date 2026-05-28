#include <catch2/catch_test_macros.hpp>
#include "lexer/scanner.h"
#include "lexer/token.h"
#include "parser/parser.h"
#include "parser/ast.h"
#include "preprocessor/preprocessor.h"
#include "semantic/analyzer.h"
#include "semantic/errors.h"

#include <memory>
#include <vector>

// Helper: analyze source and return errors
static std::vector<SemanticError> analyze(const std::string& source) {
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
    if (!parser.errors().empty()) return {}; // parse failed

    SemanticAnalyzer analyzer;
    analyzer.analyze(*ast);
    return analyzer.get_errors();
}

// ---- Valid programs ----

TEST_CASE("Semantic: valid simple program", "[semantic]") {
    auto errors = analyze(R"(
        fn main() -> int {
            int x = 42;
            return x;
        }
    )");
    CHECK(errors.empty());
}

TEST_CASE("Semantic: valid function call", "[semantic]") {
    auto errors = analyze(R"(
        fn add(int a, int b) -> int { return a + b; }
        fn main() -> int { return add(3, 4); }
    )");
    CHECK(errors.empty());
}

TEST_CASE("Semantic: valid nested scopes", "[semantic]") {
    auto errors = analyze(R"(
        fn main() -> int {
            int x = 1;
            {
                int y = 2;
                x = x + y;
            }
            return x;
        }
    )");
    CHECK(errors.empty());
}

TEST_CASE("Semantic: valid boolean expressions", "[semantic]") {
    auto errors = analyze(R"(
        fn main() -> int {
            bool a = true;
            bool b = false;
            bool c = a && b;
            bool d = a || b;
            bool e = !a;
            if (c || d) { return 1; }
            return 0;
        }
    )");
    CHECK(errors.empty());
}

TEST_CASE("Semantic: valid while and for loops", "[semantic]") {
    auto errors = analyze(R"(
        fn main() -> int {
            int sum = 0;
            int i = 0;
            while (i < 10) { sum = sum + i; i = i + 1; }
            for (int j = 0; j < 5; j = j + 1) { sum = sum + j; }
            return sum;
        }
    )");
    CHECK(errors.empty());
}

// ---- Invalid programs: expected errors ----

TEST_CASE("Semantic: undeclared variable", "[semantic]") {
    auto errors = analyze(R"(
        fn main() -> int { return x; }
    )");
    CHECK(!errors.empty());
}

TEST_CASE("Semantic: type mismatch in init", "[semantic]") {
    auto errors = analyze(R"(
        fn main() -> int {
            int x = true;
            return x;
        }
    )");
    CHECK(!errors.empty());
}

TEST_CASE("Semantic: duplicate declaration", "[semantic]") {
    auto errors = analyze(R"(
        fn main() -> int {
            int x = 1;
            int x = 2;
            return x;
        }
    )");
    CHECK(!errors.empty());
}

TEST_CASE("Semantic: wrong argument count", "[semantic]") {
    auto errors = analyze(R"(
        fn add(int a, int b) -> int { return a + b; }
        fn main() -> int { return add(1); }
    )");
    CHECK(!errors.empty());
}

TEST_CASE("Semantic: return type mismatch", "[semantic]") {
    auto errors = analyze(R"(
        fn foo() -> int { return true; }
        fn main() -> int { return foo(); }
    )");
    CHECK(!errors.empty());
}

TEST_CASE("Semantic: variable out of scope", "[semantic]") {
    auto errors = analyze(R"(
        fn main() -> int {
            { int x = 10; }
            return x;
        }
    )");
    CHECK(!errors.empty());
}

TEST_CASE("Semantic: call non-function", "[semantic]") {
    auto errors = analyze(R"(
        fn main() -> int {
            int x = 42;
            return x(1);
        }
    )");
    CHECK(!errors.empty());
}

TEST_CASE("Semantic: valid recursive function", "[semantic]") {
    auto errors = analyze(R"(
        fn factorial(int n) -> int {
            if (n <= 1) { return 1; }
            return n * factorial(n - 1);
        }
        fn main() -> int { return factorial(5); }
    )");
    CHECK(errors.empty());
}
