#pragma once

#include <string>
#include <vector>

#include "parser/ast.h"
#include "semantic/errors.h"
#include "semantic/symbol_table.h"
#include "semantic/type_system.h"

// ---------------------------------------------------------------
// SemanticAnalyzer — two-pass AST traversal
//
// Pass 1: collect top-level function/struct declarations
//         (enables forward references for function calls)
// Pass 2: full type-checking, scope validation, AST decoration
// ---------------------------------------------------------------
class SemanticAnalyzer : public ASTVisitor {
public:
    SemanticAnalyzer();

    /// Run full semantic analysis; decorates the AST in-place.
    void analyze(ProgramNode& ast);

    const std::vector<SemanticError>& get_errors() const { return errors_; }
    SemanticSymbolTable& get_symbol_table()                { return sym_; }
    const SemanticSymbolTable& get_symbol_table() const    { return sym_; }
    TypeRegistry& get_type_registry()                      { return types_; }

    /// Produce a type-annotated AST dump (text).
    std::string dump_decorated_ast(ProgramNode& ast);

    /// Produce a validation report.
    std::string validation_report() const;

    // ---- ASTVisitor overrides ----
    void visit(ProgramNode& node) override;
    void visit(LiteralExprNode& node) override;
    void visit(IdentifierExprNode& node) override;
    void visit(BinaryExprNode& node) override;
    void visit(UnaryExprNode& node) override;
    void visit(CallExprNode& node) override;
    void visit(PostfixExprNode& node) override;
    void visit(AssignmentExprNode& node) override;
    void visit(BlockStmtNode& node) override;
    void visit(ExprStmtNode& node) override;
    void visit(IfStmtNode& node) override;
    void visit(WhileStmtNode& node) override;
    void visit(ForStmtNode& node) override;
    void visit(ReturnStmtNode& node) override;
    void visit(VarDeclStmtNode& node) override;
    void visit(FunctionDeclNode& node) override;
    void visit(StructDeclNode& node) override;

private:
    TypeRegistry types_;
    SemanticSymbolTable sym_;
    std::vector<SemanticError> errors_;

    std::string current_filename_;
    std::string current_function_;      // for context in errors
    Type* current_return_type_ = nullptr;
    int loop_depth_ = 0;

    // Last inferred type for expression nodes (set after visiting an expr)
    Type* last_expr_type_ = nullptr;

    // ----- helpers -----
    void error(SemanticErrorKind kind, int line, int col,
               const std::string& msg,
               const std::string& expected = "",
               const std::string& actual = "",
               const std::string& suggestion = "");

    Type* resolve_type_name(const std::string& name, int line, int col);

    // Pass 1
    void collect_declarations(ProgramNode& ast);

    // Find similar names for suggestions
    std::string find_similar(const std::string& name) const;
};
