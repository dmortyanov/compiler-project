#include <catch2/catch_test_macros.hpp>
#include "lexer/scanner.h"
#include "lexer/token.h"
#include "parser/parser.h"
#include "parser/ast.h"
#include "preprocessor/preprocessor.h"
#include "semantic/analyzer.h"
#include "semantic/errors.h"
#include "semantic/type_system.h"

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

// ---- More semantic edge cases ----

TEST_CASE("Semantic: invalid operators", "[semantic]") {
    CHECK(!analyze("fn main() -> int { bool x = true + 1; return 0; }").empty());
    CHECK(!analyze("fn main() -> int { bool x = true && 1; return 0; }").empty());
    CHECK(!analyze("fn main() -> int { bool x = !1; return 0; }").empty());
}

TEST_CASE("Semantic: invalid condition types", "[semantic]") {
    CHECK(!analyze("fn main() -> int { if (\"hello\") { return 1; } return 0; }").empty());
    CHECK(!analyze("fn main() -> int { while (\"hello\") {} return 0; }").empty());
    CHECK(!analyze("fn main() -> int { for (int i=0; \"hello\"; i=i+1) {} return 0; }").empty());
}

TEST_CASE("Semantic: invalid assignment target", "[semantic]") {
    CHECK(!analyze("fn main() -> int { 5 = 10; return 0; }").empty());
}

TEST_CASE("Semantic: unknown types", "[semantic]") {
    CHECK(!analyze("fn main(UnknownType x) -> int { return 0; }").empty());
}

TEST_CASE("Semantic: array semantic errors", "[semantic]") {
    CHECK(!analyze("fn foo(int arr[]) -> void {} fn main() -> int { foo(10); return 0; }").empty());
    CHECK(!analyze("fn main() -> int { int x = 0; int y = x[0]; return 0; }").empty());
    CHECK(!analyze("fn main() -> int { int arr[5] = {1, 2, 3, 4, 5}; int y = arr[true]; return 0; }").empty());
}

TEST_CASE("Semantic: struct duplicate and type errors", "[semantic]") {
    CHECK(!analyze("struct Point { int x; int x; }\nfn main() -> int { return 0; }").empty());
}

TEST_CASE("TypeSystem: TypeRegistry edge cases", "[semantic]") {
    TypeRegistry reg;
    
    // Test base types resolution
    CHECK(reg.type_int()->kind == TypeKind::Int);
    CHECK(reg.type_float()->kind == TypeKind::Float);
    CHECK(reg.type_bool()->kind == TypeKind::Bool);
    CHECK(reg.type_void()->kind == TypeKind::Void);
    CHECK(reg.type_string()->kind == TypeKind::String);
    CHECK(reg.type_error()->kind == TypeKind::Error);
    CHECK(reg.resolve("") == reg.type_void());
    CHECK(reg.resolve("non_existent_type_123") == nullptr);

    // Test struct fields resolution with non-existent types
    std::vector<StructField> fields = {
        {"field1", "int", 1, 1},
        {"field2", "non_existent", 1, 1}
    };
    Type* st = reg.register_struct("MyStruct", fields);
    CHECK(st->size_bytes == 8); // 4 for int + 4 fallback for non_existent

    // Test array registration null cases and existing sizes
    CHECK(reg.register_array(nullptr, {}) == reg.type_error());
    Type* arr1 = reg.register_array(reg.type_int(), {5});
    Type* arr2 = reg.register_array(reg.type_int(), {5});
    CHECK(arr1 == arr2); // returns existing

    // Test compatibility
    CHECK(!reg.is_compatible(nullptr, reg.type_int()));
    CHECK(!reg.is_compatible(reg.type_int(), nullptr));
    CHECK(reg.is_compatible(reg.type_error(), reg.type_int()));
    CHECK(reg.is_compatible(reg.type_int(), reg.type_float())); // int is compatible to float
    CHECK(!reg.is_compatible(reg.type_int(), reg.type_bool()));

    // Array compatibility with mismatched element types or sizes
    Type* float_arr = reg.register_array(reg.type_float(), {5});
    Type* int_arr_unsized = reg.register_array(reg.type_int(), {0});
    CHECK(!reg.is_compatible(arr1, float_arr)); // mismatched element types
    CHECK(reg.is_compatible(arr1, int_arr_unsized)); // unsized accepts any size
    
    Type* arr_2d_1 = reg.register_array(reg.type_int(), {5, 5});
    CHECK(!reg.is_compatible(arr1, arr_2d_1)); // mismatched rank

    // Test common numeric type
    CHECK(reg.common_numeric_type(nullptr, reg.type_int()) == reg.type_error());
    CHECK(reg.common_numeric_type(reg.type_int(), nullptr) == reg.type_error());
    CHECK(reg.common_numeric_type(reg.type_error(), reg.type_int()) == reg.type_error());
    CHECK(reg.common_numeric_type(reg.type_int(), reg.type_float()) == reg.type_float());
    CHECK(reg.common_numeric_type(reg.type_int(), reg.type_int()) == reg.type_int());
    CHECK(reg.common_numeric_type(reg.type_int(), reg.type_bool()) == reg.type_error());
}

