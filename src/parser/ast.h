#pragma once

#include <memory>
#include <string>
#include <variant>
#include <vector>

struct ASTNode;
struct ExpressionNode;
struct StatementNode;
struct DeclarationNode;
struct ProgramNode;

struct LiteralExprNode;
struct IdentifierExprNode;
struct BinaryExprNode;
struct UnaryExprNode;
struct CallExprNode;
struct PostfixExprNode;
struct AssignmentExprNode;

struct BlockStmtNode;
struct ExprStmtNode;
struct IfStmtNode;
struct WhileStmtNode;
struct ForStmtNode;
struct ReturnStmtNode;
struct VarDeclStmtNode;

struct FunctionDeclNode;
struct StructDeclNode;
struct ParamNode;

class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;
    virtual void visit(ProgramNode& node) = 0;
    virtual void visit(LiteralExprNode& node) = 0;
    virtual void visit(IdentifierExprNode& node) = 0;
    virtual void visit(BinaryExprNode& node) = 0;
    virtual void visit(UnaryExprNode& node) = 0;
    virtual void visit(CallExprNode& node) = 0;
    virtual void visit(PostfixExprNode& node) = 0;
    virtual void visit(AssignmentExprNode& node) = 0;
    virtual void visit(BlockStmtNode& node) = 0;
    virtual void visit(ExprStmtNode& node) = 0;
    virtual void visit(IfStmtNode& node) = 0;
    virtual void visit(WhileStmtNode& node) = 0;
    virtual void visit(ForStmtNode& node) = 0;
    virtual void visit(ReturnStmtNode& node) = 0;
    virtual void visit(VarDeclStmtNode& node) = 0;
    virtual void visit(FunctionDeclNode& node) = 0;
    virtual void visit(StructDeclNode& node) = 0;
};

struct ASTNode {
    int line = 0;
    int column = 0;
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
};

struct ExpressionNode : virtual ASTNode {
    std::string resolved_type;  // filled by semantic analyzer (Sprint 3)
};
struct StatementNode : virtual ASTNode {};
struct DeclarationNode : virtual ASTNode {};

using ExprPtr = std::unique_ptr<ExpressionNode>;
using StmtPtr = std::unique_ptr<StatementNode>;
using DeclPtr = std::unique_ptr<DeclarationNode>;

struct LiteralExprNode : ExpressionNode {
    enum class Kind { Integer, Float, Bool, String };
    Kind kind;
    std::variant<int, double, bool, std::string> value;
    std::string raw;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct IdentifierExprNode : ExpressionNode {
    std::string name;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct BinaryExprNode : ExpressionNode {
    ExprPtr left;
    std::string op;
    ExprPtr right;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct UnaryExprNode : ExpressionNode {
    std::string op;
    ExprPtr operand;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct CallExprNode : ExpressionNode {
    std::string callee;
    std::vector<ExprPtr> arguments;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct PostfixExprNode : ExpressionNode {
    ExprPtr operand;
    std::string op; // "++" or "--"
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct AssignmentExprNode : ExpressionNode {
    ExprPtr target;
    std::string op;
    ExprPtr value;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct BlockStmtNode : StatementNode {
    std::vector<StmtPtr> statements;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct ExprStmtNode : StatementNode {
    ExprPtr expression;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct IfStmtNode : StatementNode {
    ExprPtr condition;
    StmtPtr then_branch;
    StmtPtr else_branch;
    bool is_matched = false;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct WhileStmtNode : StatementNode {
    ExprPtr condition;
    StmtPtr body;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct ForStmtNode : StatementNode {
    StmtPtr init;
    ExprPtr condition;
    ExprPtr update;
    StmtPtr body;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct ReturnStmtNode : StatementNode {
    ExprPtr value;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct VarDeclStmtNode : StatementNode, DeclarationNode {
    std::string type_name;
    std::string name;
    ExprPtr initializer;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct ParamNode {
    std::string type_name;
    std::string name;
    int line = 0;
    int column = 0;
};

struct FunctionDeclNode : DeclarationNode {
    std::string name;
    std::vector<ParamNode> parameters;
    std::string return_type;
    std::unique_ptr<BlockStmtNode> body;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct StructDeclNode : DeclarationNode {
    std::string name;
    std::vector<std::unique_ptr<VarDeclStmtNode>> fields;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};

struct ProgramNode : ASTNode {
    std::vector<DeclPtr> declarations;
    void accept(ASTVisitor& v) override { v.visit(*this); }
};
