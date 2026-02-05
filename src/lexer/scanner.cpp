#include "lexer/scanner.h"

#include <cctype>
#include <limits>

Scanner::Scanner(const std::string& source)
    : source_(source), current_(0), line_(1), column_(1) {}

Token Scanner::next_token() {
    if (peeked_) {
        Token tok = *peeked_;
        peeked_.reset();
        return tok;
    }
    return scan_token();
}

Token Scanner::peek_token() {
    if (!peeked_) {
        peeked_ = scan_token();
    }
    return *peeked_;
}

bool Scanner::is_at_end() {
    return peek_token().type == TokenType::END_OF_FILE;
}

int Scanner::get_line() const { return line_; }
int Scanner::get_column() const { return column_; }

const std::vector<ScanError>& Scanner::errors() const { return errors_; }

namespace {
void compute_eof_position(const std::string& source, int& out_line,
                          int& out_col) {
    std::size_t last_non_ws = std::string::npos;
    for (std::size_t i = source.size(); i > 0; --i) {
        char c = source[i - 1];
        if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
            last_non_ws = i - 1;
            break;
        }
    }
    if (last_non_ws == std::string::npos) {
        out_line = 1;
        out_col = 1;
        return;
    }

    int line = 1;
    int col = 1;
    for (std::size_t i = 0; i <= last_non_ws; ++i) {
        char c = source[i];
        if (c == '\r') {
            if (i + 1 < source.size() && source[i + 1] == '\n') {
                ++i;
            }
            line++;
            col = 1;
            continue;
        }
        if (c == '\n') {
            line++;
            col = 1;
            continue;
        }
        col++;
    }
    out_line = line;
    out_col = col;
}
} // namespace

Token Scanner::scan_token() {
    skip_whitespace_and_comments();
    if (current_ >= source_.size()) {
        int eof_line = 1;
        int eof_col = 1;
        compute_eof_position(source_, eof_line, eof_col);
        return Token{TokenType::END_OF_FILE, "", eof_line, eof_col, {}};
    }

    int start_line = line_;
    int start_col = column_;
    char c = advance();

    switch (c) {
    case '(':
        return Token{TokenType::LPAREN, "(", start_line, start_col, {}};
    case ')':
        return Token{TokenType::RPAREN, ")", start_line, start_col, {}};
    case '{':
        return Token{TokenType::LBRACE, "{", start_line, start_col, {}};
    case '}':
        return Token{TokenType::RBRACE, "}", start_line, start_col, {}};
    case '[':
        return Token{TokenType::LBRACKET, "[", start_line, start_col, {}};
    case ']':
        return Token{TokenType::RBRACKET, "]", start_line, start_col, {}};
    case ';':
        return Token{TokenType::SEMICOLON, ";", start_line, start_col, {}};
    case ',':
        return Token{TokenType::COMMA, ",", start_line, start_col, {}};
    case ':':
        return Token{TokenType::COLON, ":", start_line, start_col, {}};
    case '+':
        if (match('=')) {
            return Token{TokenType::PLUS_ASSIGN, "+=", start_line, start_col, {}};
        }
        return Token{TokenType::PLUS, "+", start_line, start_col, {}};
    case '-':
        if (match('=')) {
            return Token{TokenType::MINUS_ASSIGN, "-=", start_line, start_col, {}};
        }
        return Token{TokenType::MINUS, "-", start_line, start_col, {}};
    case '*':
        if (match('=')) {
            return Token{TokenType::STAR_ASSIGN, "*=", start_line, start_col, {}};
        }
        return Token{TokenType::STAR, "*", start_line, start_col, {}};
    case '/':
        if (match('=')) {
            return Token{TokenType::SLASH_ASSIGN, "/=", start_line, start_col, {}};
        }
        return Token{TokenType::SLASH, "/", start_line, start_col, {}};
    case '%':
        return Token{TokenType::PERCENT, "%", start_line, start_col, {}};
    case '=':
        if (match('=')) {
            return Token{TokenType::EQ, "==", start_line, start_col, {}};
        }
        return Token{TokenType::ASSIGN, "=", start_line, start_col, {}};
    case '!':
        if (match('=')) {
            return Token{TokenType::NEQ, "!=", start_line, start_col, {}};
        }
        return Token{TokenType::NOT, "!", start_line, start_col, {}};
    case '<':
        if (match('=')) {
            return Token{TokenType::LTE, "<=", start_line, start_col, {}};
        }
        return Token{TokenType::LT, "<", start_line, start_col, {}};
    case '>':
        if (match('=')) {
            return Token{TokenType::GTE, ">=", start_line, start_col, {}};
        }
        return Token{TokenType::GT, ">", start_line, start_col, {}};
    case '&':
        if (match('&')) {
            return Token{TokenType::AND, "&&", start_line, start_col, {}};
        }
        report_error(start_line, start_col, "Invalid character '&'");
        return scan_token();
    case '|':
        if (match('|')) {
            return Token{TokenType::OR, "||", start_line, start_col, {}};
        }
        report_error(start_line, start_col, "Invalid character '|'");
        return scan_token();
    case '"':
        return string_literal(start_line, start_col);
    default:
        break;
    }

    if (std::isdigit(static_cast<unsigned char>(c))) {
        return number_literal(start_line, start_col);
    }
    if (is_alpha(c)) {
        return identifier_or_keyword(start_line, start_col);
    }

    std::string msg = "Invalid character '";
    msg += c;
    msg += "'";
    report_error(start_line, start_col, msg);
    return scan_token();
}

void Scanner::skip_whitespace_and_comments() {
    while (current_ < source_.size()) {
        char c = peek();
        if (c == ' ' || c == '\t' || c == '\n' || c == '\r') {
            advance();
            continue;
        }
        if (c == '/' && peek_next() == '/') {
            advance();
            advance();
            while (current_ < source_.size() && peek() != '\n' && peek() != '\r') {
                advance();
            }
            continue;
        }
        if (c == '/' && peek_next() == '*') {
            int start_line = line_;
            int start_col = column_;
            advance();
            advance();
            bool closed = false;
            while (current_ < source_.size()) {
                if (peek() == '*' && peek_next() == '/') {
                    advance();
                    advance();
                    closed = true;
                    break;
                }
                advance();
            }
            if (!closed) {
                report_error(start_line, start_col,
                             "Unterminated multi-line comment");
            }
            continue;
        }
        break;
    }
}

char Scanner::advance() {
    if (current_ >= source_.size()) {
        return '\0';
    }
    char c = source_[current_++];
    if (c == '\r') {
        if (current_ < source_.size() && source_[current_] == '\n') {
            current_++;
        }
        line_++;
        column_ = 1;
        return '\n';
    }
    if (c == '\n') {
        line_++;
        column_ = 1;
        return '\n';
    }
    column_++;
    return c;
}

char Scanner::peek() const {
    if (current_ >= source_.size()) {
        return '\0';
    }
    return source_[current_];
}

char Scanner::peek_next() const {
    if (current_ + 1 >= source_.size()) {
        return '\0';
    }
    return source_[current_ + 1];
}

bool Scanner::match(char expected) {
    if (current_ >= source_.size()) {
        return false;
    }
    if (source_[current_] != expected) {
        return false;
    }
    advance();
    return true;
}

bool Scanner::is_alpha(char c) const {
    return std::isalpha(static_cast<unsigned char>(c)) != 0;
}

bool Scanner::is_alnum_or_underscore(char c) const {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
}

Token Scanner::identifier_or_keyword(int start_line, int start_col) {
    std::size_t start = current_ - 1;
    while (current_ < source_.size() && is_alnum_or_underscore(peek())) {
        advance();
    }
    std::string lexeme = source_.substr(start, current_ - start);
    if (lexeme.size() > 255) {
        report_error(start_line, start_col, "Identifier exceeds 255 characters");
    }

    if (lexeme == "if")
        return Token{TokenType::KW_IF, lexeme, start_line, start_col, {}};
    if (lexeme == "else")
        return Token{TokenType::KW_ELSE, lexeme, start_line, start_col, {}};
    if (lexeme == "while")
        return Token{TokenType::KW_WHILE, lexeme, start_line, start_col, {}};
    if (lexeme == "for")
        return Token{TokenType::KW_FOR, lexeme, start_line, start_col, {}};
    if (lexeme == "int")
        return Token{TokenType::KW_INT, lexeme, start_line, start_col, {}};
    if (lexeme == "float")
        return Token{TokenType::KW_FLOAT, lexeme, start_line, start_col, {}};
    if (lexeme == "bool")
        return Token{TokenType::KW_BOOL, lexeme, start_line, start_col, {}};
    if (lexeme == "return")
        return Token{TokenType::KW_RETURN, lexeme, start_line, start_col, {}};
    if (lexeme == "void")
        return Token{TokenType::KW_VOID, lexeme, start_line, start_col, {}};
    if (lexeme == "struct")
        return Token{TokenType::KW_STRUCT, lexeme, start_line, start_col, {}};
    if (lexeme == "fn")
        return Token{TokenType::KW_FN, lexeme, start_line, start_col, {}};
    if (lexeme == "true")
        return Token{TokenType::BOOL_LITERAL, lexeme, start_line, start_col,
                     true};
    if (lexeme == "false")
        return Token{TokenType::BOOL_LITERAL, lexeme, start_line, start_col,
                     false};

    return Token{TokenType::IDENTIFIER, lexeme, start_line, start_col, {}};
}

Token Scanner::number_literal(int start_line, int start_col) {
    std::size_t start = current_ - 1;
    while (current_ < source_.size() &&
           std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }

    bool is_float = false;
    if (peek() == '.' &&
        std::isdigit(static_cast<unsigned char>(peek_next()))) {
        is_float = true;
        advance();
        while (current_ < source_.size() &&
               std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }

    std::string lexeme = source_.substr(start, current_ - start);
    if (is_float) {
        double value = std::stod(lexeme);
        return Token{TokenType::FLOAT_LITERAL, lexeme, start_line, start_col,
                     value};
    }

    long long value = 0;
    try {
        value = std::stoll(lexeme);
    } catch (...) {
        report_error(start_line, start_col, "Malformed number literal");
        return Token{TokenType::INT_LITERAL, lexeme, start_line, start_col,
                     std::int32_t{0}};
    }

    std::int32_t final_value = static_cast<std::int32_t>(value);
    if (value < std::numeric_limits<std::int32_t>::min() ||
        value > std::numeric_limits<std::int32_t>::max()) {
        report_error(start_line, start_col, "Integer literal out of range");
        final_value = 0;
    }

    return Token{TokenType::INT_LITERAL, lexeme, start_line, start_col,
                 final_value};
}

Token Scanner::string_literal(int start_line, int start_col) {
    std::size_t start = current_ - 1;
    std::string value;
    bool terminated = false;

    while (current_ < source_.size()) {
        char c = peek();
        if (c == '\n' || c == '\r') {
            break;
        }
        if (c == '"') {
            advance();
            terminated = true;
            break;
        }
        if (c == '\\') {
            advance();
            if (current_ >= source_.size()) {
                break;
            }
            char esc = advance();
            switch (esc) {
            case 'n':
                value.push_back('\n');
                break;
            case 't':
                value.push_back('\t');
                break;
            case 'r':
                value.push_back('\r');
                break;
            case '"':
                value.push_back('"');
                break;
            case '\\':
                value.push_back('\\');
                break;
            default:
                value.push_back(esc);
                break;
            }
            continue;
        }
        value.push_back(advance());
    }

    std::string lexeme = source_.substr(start, current_ - start);
    if (!terminated) {
        report_error(start_line, start_col, "Unterminated string literal");
    }
    return Token{TokenType::STRING_LITERAL, lexeme, start_line, start_col,
                 value};
}

void Scanner::report_error(int line, int col, const std::string& message) {
    errors_.push_back(ScanError{line, col, message});
}
