#pragma once

#include <cstdint>
#include <string>
#include <variant>

enum class TokenType {
    // Keywords
    KW_IF,
    KW_ELSE,
    KW_WHILE,
    KW_FOR,
    KW_INT,
    KW_FLOAT,
    KW_BOOL,
    KW_RETURN,
    KW_VOID,
    KW_STRUCT,
    KW_FN,

    // Identifiers and literals
    IDENTIFIER,
    INT_LITERAL,
    FLOAT_LITERAL,
    STRING_LITERAL,
    BOOL_LITERAL,

    // Operators
    PLUS,
    MINUS,
    STAR,
    SLASH,
    PERCENT,
    EQ,
    NEQ,
    LT,
    LTE,
    GT,
    GTE,
    AND,
    OR,
    NOT,
    ASSIGN,
    PLUS_ASSIGN,
    MINUS_ASSIGN,
    STAR_ASSIGN,
    SLASH_ASSIGN,

    // Delimiters
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    LBRACKET,
    RBRACKET,
    SEMICOLON,
    COMMA,
    COLON,

    END_OF_FILE
};

using LiteralValue =
    std::variant<std::monostate, std::int32_t, double, bool, std::string>;

struct Token {
    TokenType type;
    std::string lexeme;
    int line;
    int column;
    LiteralValue literal;
};

std::string token_type_to_string(TokenType type);
std::string literal_to_string(const LiteralValue& literal);
std::string escape_lexeme(const std::string& lexeme);
std::string format_token(const Token& token);
