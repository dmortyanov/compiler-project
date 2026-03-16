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

void Parser::report_error(const Token& token, const std::string& message,
                          const std::string& suggestion) {
    if (static_cast<int>(errors_.size()) >= MAX_ERRORS) return;
    errors_.push_back(ParseError{token.line, token.column, message, suggestion}); 
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
           t == TokenType::KW_BOOL || t == TokenType::KW_VOID;
}

std::string Parser::parseTypeName() {
    if (match({TokenType::KW_INT, TokenType::KW_FLOAT, TokenType::KW_BOOL,
               TokenType::KW_VOID})) {
        return previous().lexeme;
    }
    report_error(peek(), "Ожидается имя типа");
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
        report_error(peek(), "Ожидается объявление");
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
    consume(TokenType::KW_FN, "Ожидается 'fn'");
    Token name_tok = consume(TokenType::IDENTIFIER, "Ожидается имя функции");
    node->name = name_tok.lexeme;
    consume(TokenType::LPAREN, "Ожидается '(' после имени функции");

    if (!check(TokenType::RPAREN)) {
        do {
            ParamNode p;
            p.line = peek().line;
            p.column = peek().column;
            p.type_name = parseTypeName();
            Token pname = consume(TokenType::IDENTIFIER, "Ожидается имя параметра");
            p.name = pname.lexeme;
            node->parameters.push_back(std::move(p));
        } while (match(TokenType::COMMA));
    }
    consume(TokenType::RPAREN, "Ожидается ')' после имени функции");

    if (match(TokenType::ARROW)) {
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
    consume(TokenType::KW_STRUCT, "Ожидается 'struct'");
    Token name_tok = consume(TokenType::IDENTIFIER, "Ожидается имя структуры");
    node->name = name_tok.lexeme;
    consume(TokenType::LBRACE, "Ожидается '{' после имени структуры");

    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        int l = peek().line;
        int c = peek().column;
        std::string type = parseTypeName();
        auto field = parseVarDecl(type, l, c);
        if (field) {
            node->fields.push_back(std::move(field));
        }
    }
    consume(TokenType::RBRACE, "Ожидается '}' после тела структуры");
    match(TokenType::SEMICOLON);
    return node;
}

std::unique_ptr<VarDeclStmtNode> Parser::parseVarDecl(
    const std::string& type_name, int line, int col) {
    auto node = std::make_unique<VarDeclStmtNode>();
    node->line = line;
    node->column = col;
    node->type_name = type_name;
    Token name_tok = consume(TokenType::IDENTIFIER, "Ожидается имя функции");
    node->name = name_tok.lexeme;
    if (match(TokenType::ASSIGN)) {
        node->initializer = parseExpression();
    }
    consume(TokenType::SEMICOLON, "Ожидается ';' после объявления переменной");
    return node;
}

StmtPtr Parser::parseStatement() {
    if (check(TokenType::LBRACE)) return parseBlock();
    if (check(TokenType::KW_IF)) return parseIfStmtUnmatched();
    if (check(TokenType::KW_WHILE)) return parseWhileStmt();
    if (check(TokenType::KW_FOR)) return parseForStmt();
    if (check(TokenType::KW_RETURN)) return parseReturnStmt();

    if (match(TokenType::SEMICOLON)) {
        auto empty = std::make_unique<ExprStmtNode>();
        empty->line = previous().line;
        empty->column = previous().column;
        return empty;
    }

    if (isTypeName()) {
        int l = peek().line;
        int c = peek().column;
        std::string type = parseTypeName();
        return parseVarDecl(type, l, c);
    }

    auto expr = parseExpression();
    auto stmt = std::make_unique<ExprStmtNode>();
    stmt->line = expr->line;
    stmt->column = expr->column;
    stmt->expression = std::move(expr);
    consume(TokenType::SEMICOLON, "Ожидается ';' после выражения");
    return stmt;
}

std::unique_ptr<BlockStmtNode> Parser::parseBlock() {
    auto node = std::make_unique<BlockStmtNode>();
    node->line = peek().line;
    node->column = peek().column;
    consume(TokenType::LBRACE, "Ожидается '{'");
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        auto s = parseStatement();
        if (s) {
            node->statements.push_back(std::move(s));
        }
    }
    consume(TokenType::RBRACE, "Ожидается '}'");
    return node;
}

// StmtPtr Parser::parseIfStmt() {
//     auto node = std::make_unique<IfStmtNode>();
//     node->line = peek().line;
//     node->column = peek().column;
//     consume(TokenType::KW_IF, "Expected 'if'");
//     consume(TokenType::LPAREN, "Expected '(' after 'if'");
//     node->condition = parseExpression();
//     consume(TokenType::RPAREN, "Expected ')' after if condition");
//     node->then_branch = parseStatement();
//     if (match(TokenType::KW_ELSE)) {
//         node->else_branch = parseStatement();
//     }
//     return node;
// }

StmtPtr Parser::parseMatchedStmt() {
    if (check(TokenType::LBRACE)) return parseBlock();
    if (check(TokenType::KW_WHILE)) return parseWhileStmt();
    if (check(TokenType::KW_FOR)) return parseForStmt();
    if (check(TokenType::KW_RETURN)) return parseReturnStmt();
    if (check(TokenType::KW_IF)) return parseIfStmtMatched();
    if (isTypeName()) {
        int line = peek().line;
        int col = peek().column;
        std::string type = parseTypeName();
        return parseVarDecl(type, line, col);
    }
    if (check(TokenType::SEMICOLON)) {
        advance();
        auto empty = std::make_unique<ExprStmtNode>();
        empty->line = previous().line;
        empty->column = previous().column;
        empty->expression = nullptr;
        return empty;
    }
    return parseExprStmt();
}

StmtPtr Parser::parseUnmatchedStmt() {
    if (check(TokenType::KW_IF)) return parseIfStmtUnmatched(); // if без else или с else-unmatched
    // остальное не может быть unmatched
    return nullptr;
}

StmtPtr Parser::parseIfStmtMatched() {
    auto node = std::make_unique<IfStmtNode>();
    node->line = peek().line;
    node->column = peek().column;
    node->is_matched = true;  
    
    consume(TokenType::KW_IF, "Ожидается 'if'");
    consume(TokenType::LPAREN, "Ожидается '(' после 'if'");
    node->condition = parseExpression();
    consume(TokenType::RPAREN, "Ожидается ')' после условия");
    
    node->then_branch = parseMatchedStmt();
    consume(TokenType::KW_ELSE, "Ожидается 'else' в полном if");
    node->else_branch = parseMatchedStmt();
    
    return node;
}

StmtPtr Parser::parseExprStmt() {
    auto expr = parseExpression();
    auto node = std::make_unique<ExprStmtNode>();
    node->line = expr->line;
    node->column = expr->column;
    node->expression = std::move(expr);
    consume(TokenType::SEMICOLON, "Ожидается ';' после выражения");
    return node;
}

StmtPtr Parser::parseIfStmtUnmatched() {
    auto node = std::make_unique<IfStmtNode>();
    node->line = peek().line;
    node->column = peek().column;
    node->is_matched = false;  
    
    consume(TokenType::KW_IF, "Ожидается 'if'");
    consume(TokenType::LPAREN, "Ожидается '(' после 'if'");
    node->condition = parseExpression();
    consume(TokenType::RPAREN, "Ожидается ')' после условия");
    
    // Сохраняем позицию перед разбором then
    std::size_t after_then_pos = current_;
    
    // Пробуем разобрать then как matched
    auto then_matched = parseMatchedStmt();
    
    // Проверяем, есть ли else после then
    if (check(TokenType::KW_ELSE)) {
        // Вариант 2: if (expr) MatchedStmt else UnmatchedStmt
        node->then_branch = std::move(then_matched);
        consume(TokenType::KW_ELSE, "Ожидается 'else'");
        node->else_branch = parseUnmatchedStmt();
    } else {
        // Вариант 1: if (expr) Statement (без else)
        // Откатываемся и парсим then как любой statement
        current_ = after_then_pos;
        node->then_branch = parseStatement();
        node->else_branch = nullptr;
    }
    
    return node;
}

StmtPtr Parser::parseWhileStmt() {
    auto node = std::make_unique<WhileStmtNode>();
    node->line = peek().line;
    node->column = peek().column;
    consume(TokenType::KW_WHILE, "Ожидается 'while'");
    consume(TokenType::LPAREN, "Ожидается '(' после 'while'");
    node->condition = parseExpression();
    consume(TokenType::RPAREN, "Ожидается ')' после условия");
    node->body = parseStatement();
    return node;
}

StmtPtr Parser::parseForStmt() {
    auto node = std::make_unique<ForStmtNode>();
    node->line = peek().line;
    node->column = peek().column;
    consume(TokenType::KW_FOR, "Ожидается 'for'");
    consume(TokenType::LPAREN, "Ожидается '(' после 'for'");

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
            consume(TokenType::SEMICOLON, "Ожидается ';'");
            node->init = std::move(es);
        }
    } else {
        auto expr = parseExpression();
        auto es = std::make_unique<ExprStmtNode>();
        es->line = expr->line;
        es->column = expr->column;
        es->expression = std::move(expr);
        consume(TokenType::SEMICOLON, "Ожидается ';'");
        node->init = std::move(es);
    }

    if (!check(TokenType::SEMICOLON)) {
        node->condition = parseExpression();
    }
    consume(TokenType::SEMICOLON, "Ожидается ';' после условия");

    if (!check(TokenType::RPAREN)) {
        node->update = parseExpression();
    }
    consume(TokenType::RPAREN, "Ожидается ')' после заголовка for");

    node->body = parseStatement();
    return node;
}

StmtPtr Parser::parseReturnStmt() {
    auto node = std::make_unique<ReturnStmtNode>();
    node->line = peek().line;
    node->column = peek().column;
    consume(TokenType::KW_RETURN, "Ожидается 'return'");
    if (!check(TokenType::SEMICOLON)) {
        node->value = parseExpression();
    }
    consume(TokenType::SEMICOLON, "Ожидается ';' после return");
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
    if (match({TokenType::MINUS, TokenType::NOT, TokenType::INC, TokenType::DEC})) {
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
            consume(TokenType::RPAREN, "Ожидается ')' после аргументов");
            ExprPtr base = std::move(call);
            if (match({TokenType::INC, TokenType::DEC})) {
                auto post = std::make_unique<PostfixExprNode>();
                post->line = previous().line;
                post->column = previous().column;
                post->operand = std::move(base);
                post->op = previous().lexeme;
                return post;
            }
            return base;
        }

        auto ident = std::make_unique<IdentifierExprNode>();
        ident->line = l;
        ident->column = c;
        ident->name = name;
        ExprPtr base = std::move(ident);
        if (match({TokenType::INC, TokenType::DEC})) {
            auto post = std::make_unique<PostfixExprNode>();
            post->line = previous().line;
            post->column = previous().column;
            post->operand = std::move(base);
            post->op = previous().lexeme;
            return post;
        }
        return base;
    }
    if (match(TokenType::LPAREN)) {
        int l = previous().line;
        int c = previous().column;
        auto expr = parseExpression();
        consume(TokenType::RPAREN, "Ожидается ')'");
        expr->line = l;
        expr->column = c;
        return expr;
    }

    report_error(peek(), "Ожидается выражение");
    advance();
    auto dummy = std::make_unique<LiteralExprNode>();
    dummy->line = previous().line;
    dummy->column = previous().column;
    dummy->kind = LiteralExprNode::Kind::Integer;
    dummy->value = 0;
    dummy->raw = "0";
    return dummy;
}
