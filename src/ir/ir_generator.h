#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "ir/basic_block.h"
#include "ir/ir_instructions.h"
#include "parser/ast.h"
#include "semantic/symbol_table.h"
#include "semantic/type_system.h"

// ---------------------------------------------------------------
// IRGenerator — traverses the decorated AST to produce IR
// ---------------------------------------------------------------
class IRGenerator : public ASTVisitor {
public:
    IRGenerator(SemanticSymbolTable& sym, TypeRegistry& types);

    /// Generate IR for the entire program.
    IRProgram generate(ProgramNode& ast);

    /// Retrieve IR for a specific function.
    const IRFunction* get_function_ir(const std::string& name) const;

    /// Retrieve the complete IR program.
    const IRProgram& get_all_ir() const { return program_; }

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
    SemanticSymbolTable& sym_;
    TypeRegistry& types_;
    IRProgram program_;

    // Current generation state
    IRFunction* cur_func_ = nullptr;
    BasicBlock* cur_block_ = nullptr;

    // Last expression result (set after visiting any expression node)
    Operand last_result_;

    // Variable name → operand mapping per scope level
    std::vector<std::unordered_map<std::string, Operand>> scope_stack_;

    // ----- helpers -----
    Operand new_temp(const std::string& type = "");
    std::string new_label(const std::string& prefix = "L");
    void emit(const IRInstruction& instr);

    void start_block(const std::string& label);
    void finish_block_jump(const std::string& target);
    void finish_block_cond(Operand cond, const std::string& true_label,
                           const std::string& false_label);
    void finish_block_return(Operand value);
    void finish_block_return_void();

    void enter_scope();
    void exit_scope();
    void bind_variable(const std::string& name, const Operand& operand);
    Operand lookup_variable(const std::string& name);

    std::string type_string(const std::string& resolved_type);

    // Map binary operator string → IROpcode
    IROpcode binary_op_to_opcode(const std::string& op);
};
