#include "lexer/token.h"

#include <sstream>

std::string token_type_to_string(TokenType type) {
    switch (type) {
    case TokenType::KW_IF:
        return "KW_IF";
    case TokenType::KW_ELSE:
        return "KW_ELSE";
    case TokenType::KW_WHILE:
        return "KW_WHILE";
    case TokenType::KW_FOR:
        return "KW_FOR";
    case TokenType::KW_INT:
        return "KW_INT";
    case TokenType::KW_FLOAT:
        return "KW_FLOAT";
    case TokenType::KW_BOOL:
        return "KW_BOOL";
    case TokenType::KW_RETURN:
        return "KW_RETURN";
    case TokenType::KW_VOID:
        return "KW_VOID";
    case TokenType::KW_STRUCT:
        return "KW_STRUCT";
    case TokenType::KW_FN:
        return "KW_FN";
    case TokenType::IDENTIFIER:
        return "IDENTIFIER";
    case TokenType::INT_LITERAL:
        return "INT_LITERAL";
    case TokenType::FLOAT_LITERAL:
        return "FLOAT_LITERAL";
    case TokenType::STRING_LITERAL:
        return "STRING_LITERAL";
    case TokenType::BOOL_LITERAL:
        return "BOOL_LITERAL";
    case TokenType::PLUS:
        return "PLUS";
    case TokenType::MINUS:
        return "MINUS";
    case TokenType::STAR:
        return "STAR";
    case TokenType::SLASH:
        return "SLASH";
    case TokenType::PERCENT:
        return "PERCENT";
    case TokenType::EQ:
        return "EQ";
    case TokenType::NEQ:
        return "NEQ";
    case TokenType::LT:
        return "LT";
    case TokenType::LTE:
        return "LTE";
    case TokenType::GT:
        return "GT";
    case TokenType::GTE:
        return "GTE";
    case TokenType::AND:
        return "AND";
    case TokenType::OR:
        return "OR";
    case TokenType::NOT:
        return "NOT";
    case TokenType::ASSIGN:
        return "ASSIGN";
    case TokenType::PLUS_ASSIGN:
        return "PLUS_ASSIGN";
    case TokenType::MINUS_ASSIGN:
        return "MINUS_ASSIGN";
    case TokenType::STAR_ASSIGN:
        return "STAR_ASSIGN";
    case TokenType::SLASH_ASSIGN:
        return "SLASH_ASSIGN";
    case TokenType::LPAREN:
        return "LPAREN";
    case TokenType::RPAREN:
        return "RPAREN";
    case TokenType::LBRACE:
        return "LBRACE";
    case TokenType::RBRACE:
        return "RBRACE";
    case TokenType::LBRACKET:
        return "LBRACKET";
    case TokenType::RBRACKET:
        return "RBRACKET";
    case TokenType::SEMICOLON:
        return "SEMICOLON";
    case TokenType::COMMA:
        return "COMMA";
    case TokenType::COLON:
        return "COLON";
    case TokenType::END_OF_FILE:
        return "END_OF_FILE";
    default:
        return "UNKNOWN";
    }
}

std::string literal_to_string(const LiteralValue& literal) {
    if (std::holds_alternative<std::monostate>(literal)) {
        return "";
    }
    if (std::holds_alternative<std::int32_t>(literal)) {
        return std::to_string(std::get<std::int32_t>(literal));
    }
    if (std::holds_alternative<double>(literal)) {
        std::ostringstream ss;
        ss << std::get<double>(literal);
        return ss.str();
    }
    if (std::holds_alternative<bool>(literal)) {
        return std::get<bool>(literal) ? "true" : "false";
    }
    return std::get<std::string>(literal);
}

std::string escape_lexeme(const std::string& lexeme) {
    std::string out;
    out.reserve(lexeme.size());
    for (char c : lexeme) {
        if (c == '\\') {
            out += "\\\\";
        } else if (c == '"') {
            out += "\\\"";
        } else if (c == '\n') {
            out += "\\n";
        } else if (c == '\r') {
            out += "\\r";
        } else if (c == '\t') {
            out += "\\t";
        } else {
            out += c;
        }
    }
    return out;
}

std::string format_token(const Token& token) {
    std::ostringstream ss;
    ss << token.line << ":" << token.column << " "
       << token_type_to_string(token.type) << " "
       << "\"" << escape_lexeme(token.lexeme) << "\"";
    std::string lit = literal_to_string(token.literal);
    if (!lit.empty()) {
        ss << " " << lit;
    }
    return ss.str();
}
