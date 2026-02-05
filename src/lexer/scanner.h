#pragma once

#include <optional>
#include <string>
#include <vector>

#include "lexer/token.h"

struct ScanError {
    int line;
    int column;
    std::string message;
};

class Scanner {
public:
    explicit Scanner(const std::string& source);

    Token next_token();
    Token peek_token();
    bool is_at_end();
    int get_line() const;
    int get_column() const;

    const std::vector<ScanError>& errors() const;

private:
    const std::string source_;
    std::size_t current_;
    int line_;
    int column_;
    std::optional<Token> peeked_;
    std::vector<ScanError> errors_;

    Token scan_token();
    void skip_whitespace_and_comments();
    char advance();
    char peek() const;
    char peek_next() const;
    bool match(char expected);
    bool is_alpha(char c) const;
    bool is_alnum_or_underscore(char c) const;

    Token identifier_or_keyword(int start_line, int start_col);
    Token number_literal(int start_line, int start_col);
    Token string_literal(int start_line, int start_col);
    void report_error(int line, int col, const std::string& message);
};
