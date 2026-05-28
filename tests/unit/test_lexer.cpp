#include <catch2/catch_test_macros.hpp>
#include "lexer/scanner.h"
#include "lexer/token.h"
#include "preprocessor/preprocessor.h"

#include <vector>
#include "utils/file_utils.h"
#include <cstdio>

// Helper: tokenize source string
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

// ---- Keywords ----

TEST_CASE("Lexer: keywords", "[lexer]") {
    auto tokens = tokenize("if else while for int float bool return void struct fn extern");
    REQUIRE(tokens.size() == 13); // 12 keywords + EOF
    CHECK(tokens[0].type == TokenType::KW_IF);
    CHECK(tokens[1].type == TokenType::KW_ELSE);
    CHECK(tokens[2].type == TokenType::KW_WHILE);
    CHECK(tokens[3].type == TokenType::KW_FOR);
    CHECK(tokens[4].type == TokenType::KW_INT);
    CHECK(tokens[5].type == TokenType::KW_FLOAT);
    CHECK(tokens[6].type == TokenType::KW_BOOL);
    CHECK(tokens[7].type == TokenType::KW_RETURN);
    CHECK(tokens[8].type == TokenType::KW_VOID);
    CHECK(tokens[9].type == TokenType::KW_STRUCT);
    CHECK(tokens[10].type == TokenType::KW_FN);
    CHECK(tokens[11].type == TokenType::KW_EXTERN);
    CHECK(tokens[12].type == TokenType::END_OF_FILE);
}

// ---- Identifiers ----

TEST_CASE("Lexer: identifiers", "[lexer]") {
    auto tokens = tokenize("x foo bar a123");
    REQUIRE(tokens.size() == 5);
    CHECK(tokens[0].type == TokenType::IDENTIFIER);
    CHECK(tokens[0].lexeme == "x");
    CHECK(tokens[1].lexeme == "foo");
    CHECK(tokens[2].lexeme == "bar");
    CHECK(tokens[3].lexeme == "a123");
}

// ---- Integer literals ----

TEST_CASE("Lexer: integer literals", "[lexer]") {
    auto tokens = tokenize("0 42 12345");
    REQUIRE(tokens.size() == 4);
    CHECK(tokens[0].type == TokenType::INT_LITERAL);
    CHECK(std::get<std::int32_t>(tokens[0].literal) == 0);
    CHECK(std::get<std::int32_t>(tokens[1].literal) == 42);
    CHECK(std::get<std::int32_t>(tokens[2].literal) == 12345);
}

// ---- String literals ----

TEST_CASE("Lexer: string literals", "[lexer]") {
    auto tokens = tokenize("\"hello\" \"world\"");
    REQUIRE(tokens.size() == 3);
    CHECK(tokens[0].type == TokenType::STRING_LITERAL);
    CHECK(std::get<std::string>(tokens[0].literal) == "hello");
    CHECK(std::get<std::string>(tokens[1].literal) == "world");
}

// ---- Boolean literals ----

TEST_CASE("Lexer: boolean literals", "[lexer]") {
    auto tokens = tokenize("true false");
    REQUIRE(tokens.size() == 3);
    CHECK(tokens[0].type == TokenType::BOOL_LITERAL);
    CHECK(std::get<bool>(tokens[0].literal) == true);
    CHECK(std::get<bool>(tokens[1].literal) == false);
}

// ---- Operators ----

TEST_CASE("Lexer: operators", "[lexer]") {
    auto tokens = tokenize("+ - * / % == != < <= > >= && || ! = ->(){}[];,");
    CHECK(tokens[0].type == TokenType::PLUS);
    CHECK(tokens[1].type == TokenType::MINUS);
    CHECK(tokens[2].type == TokenType::STAR);
    CHECK(tokens[3].type == TokenType::SLASH);
    CHECK(tokens[4].type == TokenType::PERCENT);
    CHECK(tokens[5].type == TokenType::EQ);
    CHECK(tokens[6].type == TokenType::NEQ);
    CHECK(tokens[7].type == TokenType::LT);
    CHECK(tokens[8].type == TokenType::LTE);
    CHECK(tokens[9].type == TokenType::GT);
    CHECK(tokens[10].type == TokenType::GTE);
    CHECK(tokens[11].type == TokenType::AND);
    CHECK(tokens[12].type == TokenType::OR);
    CHECK(tokens[13].type == TokenType::NOT);
    CHECK(tokens[14].type == TokenType::ASSIGN);
    CHECK(tokens[15].type == TokenType::ARROW);
}

// ---- Comments ----

TEST_CASE("Lexer: comments ignored", "[lexer]") {
    auto tokens = tokenize("int x = 5; // this is a comment\nint y = 10;");
    // Should see: KW_INT IDENT ASSIGN INT_LIT SEMI KW_INT IDENT ASSIGN INT_LIT SEMI EOF
    CHECK(tokens[0].type == TokenType::KW_INT);
    CHECK(tokens[4].type == TokenType::SEMICOLON);
    CHECK(tokens[5].type == TokenType::KW_INT);
}

// ---- Empty input ----

TEST_CASE("Lexer: empty input", "[lexer]") {
    auto tokens = tokenize("");
    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].type == TokenType::END_OF_FILE);
}

// ---- Whitespace only ----

TEST_CASE("Lexer: whitespace only", "[lexer]") {
    auto tokens = tokenize("   \n\t\r\n  ");
    REQUIRE(tokens.size() == 1);
    CHECK(tokens[0].type == TokenType::END_OF_FILE);
}

// ---- Line/column tracking ----

TEST_CASE("Lexer: line tracking", "[lexer]") {
    auto tokens = tokenize("int\nx");
    CHECK(tokens[0].line == 1);
    CHECK(tokens[1].line == 2);
}

// ---- Scanner errors on unknown characters ----

TEST_CASE("Lexer: error on unknown char", "[lexer]") {
    Preprocessor pp("int a = 10 $ 5;");
    std::string processed = pp.process();
    Scanner scanner(processed);
    std::vector<Token> tokens;
    while (true) {
        Token tok = scanner.next_token();
        tokens.push_back(tok);
        if (tok.type == TokenType::END_OF_FILE) break;
    }
    // Scanner should report errors but still continue
    CHECK(!scanner.errors().empty());
}

// ---- Preprocessor tests ----

TEST_CASE("Preprocessor: macro definition and expansion", "[preprocessor]") {
    // 1. Basic define and undefine
    {
        Preprocessor pp("#define MAX 100\nint a = MAX;\n#undef MAX\nint b = MAX;");
        std::string processed = pp.process();
        CHECK(pp.errors().empty());
        // MAX should be expanded to 100 in first line, but not in second line
        CHECK(processed.find("100") != std::string::npos);
        // After undef it should remain MAX
        CHECK(processed.find("MAX") != std::string::npos);
    }

    // 2. Conditionals #ifdef and #ifndef
    {
        Preprocessor pp("#define OPT\n#ifdef OPT\nint a = 1;\n#endif\n#ifndef OPT\nint b = 2;\n#endif");
        std::string processed = pp.process();
        CHECK(pp.errors().empty());
        CHECK(processed.find("int a = 1;") != std::string::npos);
        CHECK(processed.find("int b = 2;") == std::string::npos);
    }
    {
        Preprocessor pp("#ifndef OPT\nint a = 1;\n#endif\n#ifdef OPT\nint b = 2;\n#endif");
        std::string processed = pp.process();
        CHECK(pp.errors().empty());
        CHECK(processed.find("int a = 1;") != std::string::npos);
        CHECK(processed.find("int b = 2;") == std::string::npos);
    }

    // 3. Nested conditionals
    {
        Preprocessor pp("#define OPT1\n#define OPT2\n#ifdef OPT1\n#ifdef OPT2\nint a = 1;\n#endif\n#endif");
        std::string processed = pp.process();
        CHECK(pp.errors().empty());
        CHECK(processed.find("int a = 1;") != std::string::npos);
    }

    // 4. Macro recursion error
    {
        Preprocessor pp("#define A B\n#define B A\nint x = A;");
        std::string processed = pp.process();
        CHECK(!pp.errors().empty());
        CHECK(pp.errors()[0].message.find("Macro recursion detected") != std::string::npos);
    }

    // 5. Unmatched endif error
    {
        Preprocessor pp("#endif");
        std::string processed = pp.process();
        CHECK(!pp.errors().empty());
        CHECK(pp.errors()[0].message.find("Unmatched #endif") != std::string::npos);
    }

    // 6. Unterminated block comment error
    {
        Preprocessor pp("/* unterminated block comment");
        std::string processed = pp.process();
        CHECK(!pp.errors().empty());
        CHECK(pp.errors()[0].message.find("Unterminated multi-line comment") != std::string::npos);
    }

    // 7. Unterminated conditional block error
    {
        Preprocessor pp("#ifdef OPT");
        std::string processed = pp.process();
        CHECK(!pp.errors().empty());
        CHECK(pp.errors()[0].message.find("Unterminated conditional block") != std::string::npos);
    }
}

TEST_CASE("Utils: file utils read and write", "[utils]") {
    // Write success
    CHECK(utils::write_file("temp_test.txt", "hello"));
    // Read success
    CHECK(utils::read_file("temp_test.txt") == "hello");
    // Read failure (non-existent file)
    CHECK(utils::read_file("non_existent_file_12345.txt").empty());
    // Write failure (invalid directory path)
    CHECK(!utils::write_file("invalid_dir_123/file.txt", "hello"));
    
    // Cleanup
    std::remove("temp_test.txt");
}

