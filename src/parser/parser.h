#pragma once

#include <memory>
#include <string>
#include <vector>

#include "lexer/token.h"
#include "parser/ast.h"

struct ParseError {
    int line;
    int column;
    std::string message;
    std::string suggestion; 
};

struct ErrorMetrics {
    int total_errors = 0;
    int recovered = 0;
    int tokens_skipped = 0;
    double recovery_rate() const {
        return total_errors == 0 ? 1.0
             : static_cast<double>(recovered) / total_errors;
    }
};
class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);

    std::unique_ptr<ProgramNode> parse();

    const std::vector<ParseError>& errors() const;
    const ErrorMetrics& metrics() const;

private:
    const std::vector<Token>& tokens_;
    std::size_t current_;
    std::vector<ParseError> errors_;
    ErrorMetrics metrics_;
    static constexpr int MAX_ERRORS = 50;

    // Utility
    const Token& peek() const;
    const Token& previous() const;
    bool isAtEnd() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool match(std::initializer_list<TokenType> types);
    Token consume(TokenType type, const std::string& message);
    void report_error(const Token& token, const std::string& message, 
                  const std::string& suggestion = ""); 

    // Error recovery
    void synchronize();

    // Grammar rules
    DeclPtr parseDeclaration();
    std::unique_ptr<FunctionDeclNode> parseFunctionDecl();
    std::unique_ptr<StructDeclNode> parseStructDecl();
    std::unique_ptr<VarDeclStmtNode> parseVarDecl(const std::string& type_name,
                                                    int line, int col);

    StmtPtr parseStatement();
    std::unique_ptr<BlockStmtNode> parseBlock();
    StmtPtr parseMatchedStmt();
    StmtPtr parseUnmatchedStmt();
    StmtPtr parseIfStmtMatched();  // для matched if
    StmtPtr parseIfStmtUnmatched(); // для unmatched if
    StmtPtr parseIfStmt();
    StmtPtr parseWhileStmt();
    StmtPtr parseForStmt();
    StmtPtr parseReturnStmt();

    ExprPtr parseExpression();
    ExprPtr parseAssignment();
    ExprPtr parseLogicalOr();
    ExprPtr parseLogicalAnd();
    StmtPtr parseExprStmt();
    ExprPtr parseEquality();
    ExprPtr parseRelational();
    ExprPtr parseAdditive();
    ExprPtr parseMultiplicative();
    ExprPtr parseUnary();
    ExprPtr parsePrimary();

    bool isTypeName() const;
    std::string parseTypeName();
};
