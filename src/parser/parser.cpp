#include "parser/parser.h"

#include <stdexcept>

Parser::Parser(const std::vector<Token>& tokens)
    : tokens_(tokens), current_(0) {}

const std::vector<ParseError>& Parser::errors() const { return errors_; }
const ErrorMetrics& Parser::metrics() const { return metrics_; }

const Token& Parser::peek() const { return tokens_[current_]; }

const Token& Parser::previous() const { return tokens_[current_ - 1]; }

bool Parser::isAtEnd() const {
    return peek().type == TokenType::END_OF_FILE;
}

const Token& Parser::advance() {
    if (!isAtEnd()) {
        current_++;
    }
    return previous();
}

bool Parser::check(TokenType type) const {
    if (isAtEnd()) return false;
    return peek().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::match(std::initializer_list<TokenType> types) {
    for (auto type : types) {
        if (check(type)) {
            advance();
            return true;
        }
    }
    return false;
}

Token Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) {
        return advance();
    }
    report_error(peek(), message);
    return peek();
}

void Parser::report_error(const Token& token, const std::string& message) {
    if (static_cast<int>(errors_.size()) >= MAX_ERRORS) return;
    errors_.push_back(ParseError{token.line, token.column, message});
    metrics_.total_errors++;
}

// после ошибки — прыгаем до ; или начала след. конструкции
void Parser::synchronize() {
    int skipped = 0;
    advance();
    skipped++;
    while (!isAtEnd()) {
        if (previous().type == TokenType::SEMICOLON) {
            metrics_.recovered++;
            metrics_.tokens_skipped += skipped;
            return;
        }
        switch (peek().type) {
        case TokenType::KW_FN:
        case TokenType::KW_STRUCT:
        case TokenType::KW_INT:
        case TokenType::KW_FLOAT:
        case TokenType::KW_BOOL:
        case TokenType::KW_VOID:
        case TokenType::KW_IF:
        case TokenType::KW_WHILE:
        case TokenType::KW_FOR:
        case TokenType::KW_RETURN:
            metrics_.recovered++;
            metrics_.tokens_skipped += skipped;
            return;
        default:
            break;
        }
        advance();
        skipped++;
    }
    metrics_.recovered++;
    metrics_.tokens_skipped += skipped;
}

std::unique_ptr<ProgramNode> Parser::parse() {
    auto program = std::make_unique<ProgramNode>();
    program->line = peek().line;
    program->column = peek().column;

    while (!isAtEnd()) {
        auto decl = parseDeclaration();
        if (decl) {
            program->declarations.push_back(std::move(decl));
        }
    }
    return program;
}

bool Parser::isTypeName() const {
    auto t = peek().type;
    return t == TokenType::KW_INT || t == TokenType::KW_FLOAT ||
           t == TokenType::KW_BOOL || t == TokenType::KW_VOID ||
           t == TokenType::IDENTIFIER;
}

std::string Parser::parseTypeName() {
    if (match({TokenType::KW_INT, TokenType::KW_FLOAT, TokenType::KW_BOOL,
               TokenType::KW_VOID, TokenType::IDENTIFIER})) {
        return previous().lexeme;
    }
    report_error(peek(), "Expected type name");
    return "?";
}

DeclPtr Parser::parseDeclaration() {
    try {
        if (check(TokenType::KW_FN)) {
            return parseFunctionDecl();
        }
        if (check(TokenType::KW_STRUCT)) {
            return parseStructDecl();
        }
        if (isTypeName()) {
            int line = peek().line;
            int col = peek().column;
            std::string type = parseTypeName();
            auto vd = parseVarDecl(type, line, col);
            return vd;
        }
        report_error(peek(), "Expected declaration");
        synchronize();
        return nullptr;
    } catch (...) {
        synchronize();
        return nullptr;
    }
}

std::unique_ptr<FunctionDeclNode> Parser::parseFunctionDecl() {
    auto node = std::make_unique<FunctionDeclNode>();
    node->line = peek().line;
    node->column = peek().column;
    consume(TokenType::KW_FN, "Expected 'fn'");
    Token name_tok = consume(TokenType::IDENTIFIER, "Expected function name");
    node->name = name_tok.lexeme;
    consume(TokenType::LPAREN, "Expected '(' after function name");

    if (!check(TokenType::RPAREN)) {
        do {
            ParamNode p;
            p.line = peek().line;
            p.column = peek().column;
            p.type_name = parseTypeName();
            Token pname = consume(TokenType::IDENTIFIER, "Expected parameter name");
            p.name = pname.lexeme;
            node->parameters.push_back(std::move(p));
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RPAREN, "Expected ')' after parameters");

    if (match(TokenType::COLON)) {
        node->return_type = parseTypeName();
    } else {
        node->return_type = "void";
    }

    node->body = parseBlock();
    return node;
}

std::unique_ptr<StructDeclNode> Parser::parseStructDecl() {
    auto node = std::make_unique<StructDeclNode>();
    node->line = peek().line;
    node->column = peek().column;
    consume(TokenType::KW_STRUCT, "Expected 'struct'");
    Token name_tok = consume(TokenType::IDENTIFIER, "Expected struct name");
    node->name = name_tok.lexeme;
    consume(TokenType::LBRACE, "Expected '{' after struct name");

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        int l = peek().line;
        int c = peek().column;
        std::string type = parseTypeName();
        auto field = parseVarDecl(type, l, c);
        if (field) {
            node->fields.push_back(std::move(field));
        }
    }
    consume(TokenType::RBRACE, "Expected '}' after struct body");
    match(TokenType::SEMICOLON);
    return node;
}

std::unique_ptr<VarDeclStmtNode> Parser::parseVarDecl(
    const std::string& type_name, int line, int col) {
    auto node = std::make_unique<VarDeclStmtNode>();
    node->line = line;
    node->column = col;
    node->type_name = type_name;
    Token name_tok = consume(TokenType::IDENTIFIER, "Expected variable name");
    node->name = name_tok.lexeme;
    if (match(TokenType::ASSIGN)) {
        node->initializer = parseExpression();
    }
    consume(TokenType::SEMICOLON, "Expected ';' after variable declaration");
    return node;
}

StmtPtr Parser::parseStatement() {
    if (check(TokenType::LBRACE)) return parseBlock();
    if (check(TokenType::KW_IF)) return parseIfStmt();
    if (check(TokenType::KW_WHILE)) return parseWhileStmt();
    if (check(TokenType::KW_FOR)) return parseForStmt();
    if (check(TokenType::KW_RETURN)) return parseReturnStmt();

    if (match(TokenType::SEMICOLON)) {
        auto empty = std::make_unique<ExprStmtNode>();
        empty->line = previous().line;
        empty->column = previous().column;
        return empty;
    }

    if (isTypeName() && peek().type != TokenType::IDENTIFIER) {
        int l = peek().line;
        int c = peek().column;
        std::string type = parseTypeName();
        return parseVarDecl(type, l, c);
    }
    if (peek().type == TokenType::IDENTIFIER) {
        std::size_t saved = current_;
        std::string maybe_type = peek().lexeme;
        advance();
        if (check(TokenType::IDENTIFIER)) {
            int l = tokens_[saved].line;
            int c = tokens_[saved].column;
            return parseVarDecl(maybe_type, l, c);
        }
        current_ = saved;
    }

    auto expr = parseExpression();
    auto stmt = std::make_unique<ExprStmtNode>();
    stmt->line = expr->line;
    stmt->column = expr->column;
    stmt->expression = std::move(expr);
    consume(TokenType::SEMICOLON, "Expected ';' after expression");
    return stmt;
}

std::unique_ptr<BlockStmtNode> Parser::parseBlock() {
    auto node = std::make_unique<BlockStmtNode>();
    node->line = peek().line;
    node->column = peek().column;
    consume(TokenType::LBRACE, "Expected '{'");
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        auto s = parseStatement();
        if (s) {
            node->statements.push_back(std::move(s));
        }
    }
    consume(TokenType::RBRACE, "Expected '}'");
    return node;
}

StmtPtr Parser::parseIfStmt() {
    auto node = std::make_unique<IfStmtNode>();
    node->line = peek().line;
    node->column = peek().column;
    consume(TokenType::KW_IF, "Expected 'if'");
    consume(TokenType::LPAREN, "Expected '(' after 'if'");
    node->condition = parseExpression();
    consume(TokenType::RPAREN, "Expected ')' after if condition");
    node->then_branch = parseStatement();
    if (match(TokenType::KW_ELSE)) {
        node->else_branch = parseStatement();
    }
    return node;
}

StmtPtr Parser::parseWhileStmt() {
    auto node = std::make_unique<WhileStmtNode>();
    node->line = peek().line;
    node->column = peek().column;
    consume(TokenType::KW_WHILE, "Expected 'while'");
    consume(TokenType::LPAREN, "Expected '(' after 'while'");
    node->condition = parseExpression();
    consume(TokenType::RPAREN, "Expected ')' after while condition");
    node->body = parseStatement();
    return node;
}

StmtPtr Parser::parseForStmt() {
    auto node = std::make_unique<ForStmtNode>();
    node->line = peek().line;
    node->column = peek().column;
    consume(TokenType::KW_FOR, "Expected 'for'");
    consume(TokenType::LPAREN, "Expected '(' after 'for'");

    if (match(TokenType::SEMICOLON)) {
        node->init = nullptr;
    } else if (isTypeName() && peek().type != TokenType::IDENTIFIER) {
        int l = peek().line;
        int c = peek().column;
        std::string type = parseTypeName();
        node->init = parseVarDecl(type, l, c);
    } else if (peek().type == TokenType::IDENTIFIER) {
        std::size_t saved = current_;
        std::string maybe_type = peek().lexeme;
        advance();
        if (check(TokenType::IDENTIFIER)) {
            int l = tokens_[saved].line;
            int c = tokens_[saved].column;
            node->init = parseVarDecl(maybe_type, l, c);
        } else {
            current_ = saved;
            auto expr = parseExpression();
            auto es = std::make_unique<ExprStmtNode>();
            es->line = expr->line;
            es->column = expr->column;
            es->expression = std::move(expr);
            consume(TokenType::SEMICOLON, "Expected ';'");
            node->init = std::move(es);
        }
    } else {
        auto expr = parseExpression();
        auto es = std::make_unique<ExprStmtNode>();
        es->line = expr->line;
        es->column = expr->column;
        es->expression = std::move(expr);
        consume(TokenType::SEMICOLON, "Expected ';'");
        node->init = std::move(es);
    }

    if (!check(TokenType::SEMICOLON)) {
        node->condition = parseExpression();
    }
    consume(TokenType::SEMICOLON, "Expected ';' after for condition");

    if (!check(TokenType::RPAREN)) {
        node->update = parseExpression();
    }
    consume(TokenType::RPAREN, "Expected ')' after for clauses");

    node->body = parseStatement();
    return node;
}

StmtPtr Parser::parseReturnStmt() {
    auto node = std::make_unique<ReturnStmtNode>();
    node->line = peek().line;
    node->column = peek().column;
    consume(TokenType::KW_RETURN, "Expected 'return'");
    if (!check(TokenType::SEMICOLON)) {
        node->value = parseExpression();
    }
    consume(TokenType::SEMICOLON, "Expected ';' after return value");
    return node;
}

ExprPtr Parser::parseExpression() { return parseAssignment(); }

ExprPtr Parser::parseAssignment() {
    auto expr = parseLogicalOr();

    if (match({TokenType::ASSIGN, TokenType::PLUS_ASSIGN,
               TokenType::MINUS_ASSIGN, TokenType::STAR_ASSIGN,
               TokenType::SLASH_ASSIGN})) {
        std::string op = previous().lexeme;
        int l = previous().line;
        int c = previous().column;
        auto value = parseAssignment();
        auto node = std::make_unique<AssignmentExprNode>();
        node->line = l;
        node->column = c;
        node->target = std::move(expr);
        node->op = op;
        node->value = std::move(value);
        return node;
    }
    return expr;
}

ExprPtr Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    while (match(TokenType::OR)) {
        std::string op = previous().lexeme;
        int l = previous().line;
        int c = previous().column;
        auto right = parseLogicalAnd();
        auto node = std::make_unique<BinaryExprNode>();
        node->line = l;
        node->column = c;
        node->left = std::move(left);
        node->op = op;
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

ExprPtr Parser::parseLogicalAnd() {
    auto left = parseEquality();
    while (match(TokenType::AND)) {
        std::string op = previous().lexeme;
        int l = previous().line;
        int c = previous().column;
        auto right = parseEquality();
        auto node = std::make_unique<BinaryExprNode>();
        node->line = l;
        node->column = c;
        node->left = std::move(left);
        node->op = op;
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

ExprPtr Parser::parseEquality() {
    auto left = parseRelational();
    if (match({TokenType::EQ, TokenType::NEQ})) {
        std::string op = previous().lexeme;
        int l = previous().line;
        int c = previous().column;
        auto right = parseRelational();
        auto node = std::make_unique<BinaryExprNode>();
        node->line = l;
        node->column = c;
        node->left = std::move(left);
        node->op = op;
        node->right = std::move(right);
        return node;
    }
    return left;
}

ExprPtr Parser::parseRelational() {
    auto left = parseAdditive();
    if (match({TokenType::LT, TokenType::LTE, TokenType::GT,
               TokenType::GTE})) {
        std::string op = previous().lexeme;
        int l = previous().line;
        int c = previous().column;
        auto right = parseAdditive();
        auto node = std::make_unique<BinaryExprNode>();
        node->line = l;
        node->column = c;
        node->left = std::move(left);
        node->op = op;
        node->right = std::move(right);
        return node;
    }
    return left;
}

ExprPtr Parser::parseAdditive() {
    auto left = parseMultiplicative();
    while (match({TokenType::PLUS, TokenType::MINUS})) {
        std::string op = previous().lexeme;
        int l = previous().line;
        int c = previous().column;
        auto right = parseMultiplicative();
        auto node = std::make_unique<BinaryExprNode>();
        node->line = l;
        node->column = c;
        node->left = std::move(left);
        node->op = op;
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

ExprPtr Parser::parseMultiplicative() {
    auto left = parseUnary();
    while (match({TokenType::STAR, TokenType::SLASH, TokenType::PERCENT})) {
        std::string op = previous().lexeme;
        int l = previous().line;
        int c = previous().column;
        auto right = parseUnary();
        auto node = std::make_unique<BinaryExprNode>();
        node->line = l;
        node->column = c;
        node->left = std::move(left);
        node->op = op;
        node->right = std::move(right);
        left = std::move(node);
    }
    return left;
}

ExprPtr Parser::parseUnary() {
    if (match({TokenType::MINUS, TokenType::NOT})) {
        std::string op = previous().lexeme;
        int l = previous().line;
        int c = previous().column;
        auto operand = parseUnary();
        auto node = std::make_unique<UnaryExprNode>();
        node->line = l;
        node->column = c;
        node->op = op;
        node->operand = std::move(operand);
        return node;
    }
    return parsePrimary();
}

ExprPtr Parser::parsePrimary() {
    if (match(TokenType::INT_LITERAL)) {
        auto node = std::make_unique<LiteralExprNode>();
        node->line = previous().line;
        node->column = previous().column;
        node->kind = LiteralExprNode::Kind::Integer;
        node->raw = previous().lexeme;
        if (std::holds_alternative<std::int32_t>(previous().literal)) {
            node->value = std::get<std::int32_t>(previous().literal);
        } else {
            node->value = 0;
        }
        return node;
    }
    if (match(TokenType::FLOAT_LITERAL)) {
        auto node = std::make_unique<LiteralExprNode>();
        node->line = previous().line;
        node->column = previous().column;
        node->kind = LiteralExprNode::Kind::Float;
        node->raw = previous().lexeme;
        if (std::holds_alternative<double>(previous().literal)) {
            node->value = std::get<double>(previous().literal);
        } else {
            node->value = 0.0;
        }
        return node;
    }
    if (match(TokenType::BOOL_LITERAL)) {
        auto node = std::make_unique<LiteralExprNode>();
        node->line = previous().line;
        node->column = previous().column;
        node->kind = LiteralExprNode::Kind::Bool;
        node->raw = previous().lexeme;
        if (std::holds_alternative<bool>(previous().literal)) {
            node->value = std::get<bool>(previous().literal);
        } else {
            node->value = false;
        }
        return node;
    }
    if (match(TokenType::STRING_LITERAL)) {
        auto node = std::make_unique<LiteralExprNode>();
        node->line = previous().line;
        node->column = previous().column;
        node->kind = LiteralExprNode::Kind::String;
        node->raw = previous().lexeme;
        if (std::holds_alternative<std::string>(previous().literal)) {
            node->value = std::get<std::string>(previous().literal);
        } else {
            node->value = std::string{};
        }
        return node;
    }
    if (match(TokenType::IDENTIFIER)) {
        std::string name = previous().lexeme;
        int l = previous().line;
        int c = previous().column;

        if (match(TokenType::LPAREN)) {
            auto call = std::make_unique<CallExprNode>();
            call->line = l;
            call->column = c;
            call->callee = name;
            if (!check(TokenType::RPAREN)) {
                call->arguments.push_back(parseExpression());
                while (match(TokenType::COMMA)) {
                    call->arguments.push_back(parseExpression());
                }
            }
            consume(TokenType::RPAREN, "Expected ')' after arguments");
            return call;
        }

        auto ident = std::make_unique<IdentifierExprNode>();
        ident->line = l;
        ident->column = c;
        ident->name = name;
        return ident;
    }
    if (match(TokenType::LPAREN)) {
        int l = previous().line;
        int c = previous().column;
        auto expr = parseExpression();
        consume(TokenType::RPAREN, "Expected ')'");
        expr->line = l;
        expr->column = c;
        return expr;
    }

    report_error(peek(), "Expected expression");
    advance();
    auto dummy = std::make_unique<LiteralExprNode>();
    dummy->line = previous().line;
    dummy->column = previous().column;
    dummy->kind = LiteralExprNode::Kind::Integer;
    dummy->value = 0;
    dummy->raw = "0";
    return dummy;
}
