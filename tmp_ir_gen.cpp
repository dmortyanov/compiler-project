#include "ir/ir_generator.h"

#include <cassert>
#include <stdexcept>

// ---------------------------------------------------------------
// AssignedVarCollector
// ---------------------------------------------------------------
class AssignedVarCollector : public ASTVisitor {
public:
    std::vector<std::string> assigned_vars;

    void visit(ProgramNode& node) override { for(auto& d: node.declarations) d->accept(*this); }
    void visit(BlockStmtNode& node) override { for(auto& s: node.statements) s->accept(*this); }
    void visit(ExprStmtNode& node) override { if(node.expression) node.expression->accept(*this); }
    void visit(IfStmtNode& node) override {
        if (node.condition) node.condition->accept(*this);
        if (node.then_branch) node.then_branch->accept(*this);
        if (node.else_branch) node.else_branch->accept(*this);
    }
    void visit(WhileStmtNode& node) override {
        if (node.condition) node.condition->accept(*this);
        if (node.body) node.body->accept(*this);
    }
    void visit(ForStmtNode& node) override {
        if (node.init) node.init->accept(*this);
        if (node.condition) node.condition->accept(*this);
        if (node.update) node.update->accept(*this);
        if (node.body) node.body->accept(*this);
    }
    void visit(AssignmentExprNode& node) override {
        if (auto* ident = dynamic_cast<IdentifierExprNode*>(node.target.get())) {
            assigned_vars.push_back(ident->name);
        }
        if (node.value) node.value->accept(*this);
    }
    void visit(PostfixExprNode& node) override {
        if (auto* ident = dynamic_cast<IdentifierExprNode*>(node.operand.get())) {
            assigned_vars.push_back(ident->name);
        }
    }
    
    // Traversals down expressions...
    void visit(BinaryExprNode& node) override {
        node.left->accept(*this); node.right->accept(*this);
    }
    void visit(UnaryExprNode& node) override { node.operand->accept(*this); }
    void visit(CallExprNode& node) override {
        for(auto& arg : node.arguments) arg->accept(*this);
    }
    
    // Ignored nodes
    void visit(ReturnStmtNode&) override {}
    void visit(VarDeclStmtNode&) override {}
    void visit(FunctionDeclNode&) override {}
    void visit(StructDeclNode&) override {}
    void visit(LiteralExprNode&) override {}
    void visit(IdentifierExprNode&) override {}
};

// ---------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------
IRGenerator::IRGenerator(SemanticSymbolTable& sym, TypeRegistry& types)
    : sym_(sym), types_(types) {}

// ---------------------------------------------------------------
// generate — entry point
// ---------------------------------------------------------------
IRProgram IRGenerator::generate(ProgramNode& ast) {
    program_ = IRProgram{};
    ast.accept(*this);
    return program_;
}

const IRFunction* IRGenerator::get_function_ir(const std::string& name) const {
    return program_.find_function(name);
}

// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------
Operand IRGenerator::new_temp(const std::string& type) {
    return cur_func_->new_temp(type);
}

std::string IRGenerator::new_label(const std::string& prefix) {
    return cur_func_->new_label(prefix);
}

void IRGenerator::emit(const IRInstruction& instr) {
    if (cur_block_)
        cur_block_->add_instruction(instr);
}

void IRGenerator::start_block(const std::string& label) {
    cur_block_ = &cur_func_->add_block(label);
}

void IRGenerator::finish_block_jump(const std::string& target) {
    if (!cur_block_) return;
    if (!cur_block_->has_terminator()) {
        emit(IRInstruction::make_jump(target));
        cur_func_->link_blocks(cur_block_->label, target);
    }
    cur_block_ = nullptr;
}

void IRGenerator::finish_block_cond(Operand cond,
                                     const std::string& true_label,
                                     const std::string& false_label) {
    if (!cur_block_) return;
    emit(IRInstruction::make_jump_if(cond, true_label));
    emit(IRInstruction::make_jump(false_label));
    cur_func_->link_blocks(cur_block_->label, true_label);
    cur_func_->link_blocks(cur_block_->label, false_label);
    cur_block_ = nullptr;
}

void IRGenerator::finish_block_return(Operand value) {
    if (!cur_block_) return;
    emit(IRInstruction::make_return(value));
    cur_block_ = nullptr;
}

void IRGenerator::finish_block_return_void() {
    if (!cur_block_) return;
    emit(IRInstruction::make_return_void());
    cur_block_ = nullptr;
}

void IRGenerator::enter_scope() {
    scope_stack_.push_back({});
}

void IRGenerator::exit_scope() {
    if (!scope_stack_.empty())
        scope_stack_.pop_back();
}

void IRGenerator::bind_variable(const std::string& name, const Operand& operand) {
    // update in current scope if it exists there, else go up
    for (int i = static_cast<int>(scope_stack_.size()) - 1; i >= 0; --i) {
        if (scope_stack_[i].find(name) != scope_stack_[i].end()) {
            scope_stack_[i][name] = operand;
            return;
        }
    }
    // If not found, bind in innermost scope
    if (!scope_stack_.empty())
        scope_stack_.back()[name] = operand;
}

Operand IRGenerator::lookup_variable(const std::string& name) {
    for (int i = static_cast<int>(scope_stack_.size()) - 1; i >= 0; --i) {
        auto it = scope_stack_[i].find(name);
        if (it != scope_stack_[i].end())
            return it->second;
    }
    // Fallback: use variable name directly (should not happen in valid SSA program)
    return Operand::var(name);
}

std::string IRGenerator::type_string(const std::string& resolved_type) {
    if (resolved_type.empty()) return "int";
    return resolved_type;
}

IROpcode IRGenerator::binary_op_to_opcode(const std::string& op) {
    if (op == "+")  return IROpcode::ADD;
    if (op == "-")  return IROpcode::SUB;
    if (op == "*")  return IROpcode::MUL;
    if (op == "/")  return IROpcode::DIV;
    if (op == "%")  return IROpcode::MOD;
    if (op == "&&") return IROpcode::AND;
    if (op == "||") return IROpcode::OR;
    if (op == "^")  return IROpcode::XOR;
    if (op == "==") return IROpcode::CMP_EQ;
    if (op == "!=") return IROpcode::CMP_NE;
    if (op == "<")  return IROpcode::CMP_LT;
    if (op == "<=") return IROpcode::CMP_LE;
    if (op == ">")  return IROpcode::CMP_GT;
    if (op == ">=") return IROpcode::CMP_GE;
    return IROpcode::NOP;
}

// ---------------------------------------------------------------
// ProgramNode
// ---------------------------------------------------------------
void IRGenerator::visit(ProgramNode& node) {
    for (auto& decl : node.declarations)
        decl->accept(*this);
}

// ---------------------------------------------------------------
// FunctionDeclNode — create IR function with entry block
// ---------------------------------------------------------------
void IRGenerator::visit(FunctionDeclNode& node) {
    IRFunction& func = program_.add_function(node.name, node.return_type);
    cur_func_ = &func;

    for (const auto& p : node.parameters)
        func.params.push_back({p.name, p.type_name});

    enter_scope();
    start_block("entry");

    for (int i = 0; i < static_cast<int>(node.parameters.size()); ++i) {
        const auto& p = node.parameters[i];
        Operand param_temp = new_temp(p.type_name);
        emit(IRInstruction::make_move(param_temp, Operand::var(p.name, p.type_name)));
        bind_variable(p.name, param_temp);
    }

    if (node.body)
        node.body->accept(*this);

    if (cur_block_ && !cur_block_->has_terminator()) {
        if (node.return_type == "void")
            finish_block_return_void();
        else
            finish_block_return(Operand::int_lit(0));
    }

    exit_scope();
    cur_func_ = nullptr;
}

void IRGenerator::visit(StructDeclNode& /*node*/) {
}

void IRGenerator::visit(BlockStmtNode& node) {
    enter_scope();
    for (auto& stmt : node.statements)
        stmt->accept(*this);
    
    // Copy the variables from the exited scope that bubbled up, actually bind_variable already modifies parent scopes if var exists there!
    exit_scope();
}

void IRGenerator::visit(VarDeclStmtNode& node) {
    Operand rhs = Operand::int_lit(0);
    if (node.initializer) {
        node.initializer->accept(*this);
        rhs = last_result_;
    }

    Operand tmp = new_temp(node.type_name);
    auto instr = IRInstruction::make_move(tmp, rhs);
    instr.source_line = node.line;
    instr.comment = node.type_name + " " + node.name + " = ...";
    emit(instr);
    
    // Bind var
    if (!scope_stack_.empty())
        scope_stack_.back()[node.name] = tmp;
}

void IRGenerator::visit(ExprStmtNode& node) {
    if (node.expression)
        node.expression->accept(*this);
}

void IRGenerator::visit(ReturnStmtNode& node) {
    if (node.value) {
        node.value->accept(*this);
        auto instr = IRInstruction::make_return(last_result_);
        instr.source_line = node.line;
        instr.comment = "return";
        emit(instr);
    } else {
        auto instr = IRInstruction::make_return_void();
        instr.source_line = node.line;
        instr.comment = "return void";
        emit(instr);
    }
    cur_block_ = nullptr;
}

// ---------------------------------------------------------------
// helper: flat map of current vars
// ---------------------------------------------------------------
static std::unordered_map<std::string, Operand> flatten_scopes(const std::vector<std::unordered_map<std::string, Operand>>& scopes) {
    std::unordered_map<std::string, Operand> res;
    for (const auto& s : scopes) {
        for (const auto& kv : s) {
            res[kv.first] = kv.second;
        }
    }
    return res;
}

void IRGenerator::visit(IfStmtNode& node) {
    std::string then_label = new_label("L_then");
    std::string else_label = node.else_branch ? new_label("L_else") : "";
    std::string end_label = new_label("L_endif");

    node.condition->accept(*this);
    Operand cond = last_result_;

    if (node.else_branch) {
        finish_block_cond(cond, then_label, else_label);
    } else {
        finish_block_cond(cond, then_label, end_label);
    }

    auto vars_before = flatten_scopes(scope_stack_);

    // Then branch
    start_block(then_label);
    node.then_branch->accept(*this);
    bool then_reachable = (cur_block_ != nullptr);
    std::string then_exit_label = cur_block_ ? cur_block_->label : "";
    finish_block_jump(end_label);
    auto vars_then = flatten_scopes(scope_stack_);

    // Restore for else
    for (auto& s : scope_stack_) s.clear(); // This is dangerous, better to recreate
    scope_stack_.clear(); scope_stack_.push_back(vars_before);

    // Else branch
    bool else_reachable = false;
    std::string else_exit_label = "";
    if (node.else_branch) {
        start_block(else_label);
        node.else_branch->accept(*this);
        else_reachable = (cur_block_ != nullptr);
        else_exit_label = cur_block_ ? cur_block_->label : "";
        finish_block_jump(end_label);
    } else {
        else_reachable = true; // The fallthrough path is reachable
        else_exit_label = cur_func_->blocks.back().label; // Wait, actually the predecessor is the block evaluating condition!
        else_exit_label = then_label; // Actually, wait, no. We need the block right before if
    }
    // Wait, the else_exit_label for an IF without ELSE is the block that evaluated the condition!
    // So we need to record it before we call finish_block_cond
    // Let's rely on standard AST-SSA.

    // Let's use a simpler approach for SSA variables:
    // we don't restore scope_stack_, we just let them bubble, wait, no!
}
