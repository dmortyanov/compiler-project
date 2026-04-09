#include "ir/ir_generator.h"

#include <cassert>
#include <stdexcept>
#include <set>
#include <iostream>

// ---------------------------------------------------------------
// AssignedVarCollector
// ---------------------------------------------------------------
class AssignedVarCollector : public ASTVisitor {
public:
    std::set<std::string> assigned_vars;

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
            assigned_vars.insert(ident->name);
        }
        if (node.value) node.value->accept(*this);
    }
    void visit(PostfixExprNode& node) override {
        if (auto* ident = dynamic_cast<IdentifierExprNode*>(node.operand.get())) {
            assigned_vars.insert(ident->name);
        }
    }
    void visit(BinaryExprNode& node) override {
        node.left->accept(*this); node.right->accept(*this);
    }
    void visit(UnaryExprNode& node) override { node.operand->accept(*this); }
    void visit(CallExprNode& node) override {
        for(auto& arg : node.arguments) arg->accept(*this);
    }
    void visit(ReturnStmtNode&) override {}
    void visit(VarDeclStmtNode&) override {}
    void visit(FunctionDeclNode&) override {}
    void visit(StructDeclNode&) override {}
    void visit(LiteralExprNode&) override {}
    void visit(IdentifierExprNode&) override {}
};

static bool operands_equal(const Operand& a, const Operand& b) {
    if (a.kind != b.kind) return false;
    switch(a.kind) {
        case OperandKind::Temp:
        case OperandKind::Variable:
        case OperandKind::Label:
        case OperandKind::StringLiteral:
            return a.name == b.name;
        case OperandKind::IntLiteral:
        case OperandKind::BoolLiteral:
            return a.int_val == b.int_val;
        case OperandKind::FloatLiteral:
            return a.float_val == b.float_val;
        case OperandKind::None:
            return true;
    }
    return false;
}

static std::unordered_map<std::string, Operand> flatten_scopes(const std::vector<std::unordered_map<std::string, Operand>>& scopes) {
    std::unordered_map<std::string, Operand> res;
    for (const auto& s : scopes) {
        for (const auto& kv : s) {
            res[kv.first] = kv.second;
        }
    }
    return res;
}

// ---------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------
IRGenerator::IRGenerator(SemanticSymbolTable& sym, TypeRegistry& types)
    : sym_(sym), types_(types) {}

IRProgram IRGenerator::generate(ProgramNode& ast) {
    program_ = IRProgram{};
    ast.accept(*this);
    return program_;
}

const IRFunction* IRGenerator::get_function_ir(const std::string& name) const {
    return program_.find_function(name);
}

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
    last_finished_block_ = cur_block_->label;
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
    last_finished_block_ = cur_block_->label;
    emit(IRInstruction::make_jump_if(cond, true_label));
    emit(IRInstruction::make_jump(false_label));
    cur_func_->link_blocks(cur_block_->label, true_label);
    cur_func_->link_blocks(cur_block_->label, false_label);
    cur_block_ = nullptr;
}

void IRGenerator::finish_block_return(Operand value) {
    if (!cur_block_) return;
    last_finished_block_ = cur_block_->label;
    emit(IRInstruction::make_return(value));
    cur_block_ = nullptr;
}

void IRGenerator::finish_block_return_void() {
    if (!cur_block_) return;
    last_finished_block_ = cur_block_->label;
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
    for (int i = static_cast<int>(scope_stack_.size()) - 1; i >= 0; --i) {
        if (scope_stack_[i].find(name) != scope_stack_[i].end()) {
            scope_stack_[i][name] = operand;
            return;
        }
    }
    if (!scope_stack_.empty())
        scope_stack_.back()[name] = operand;
}

Operand IRGenerator::lookup_variable(const std::string& name) {
    for (int i = static_cast<int>(scope_stack_.size()) - 1; i >= 0; --i) {
        auto it = scope_stack_[i].find(name);
        if (it != scope_stack_[i].end())
            return it->second;
    }
    return Operand::var(name); // fallback
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

void IRGenerator::visit(ProgramNode& node) {
    for (auto& decl : node.declarations)
        decl->accept(*this);
}

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

void IRGenerator::visit(StructDeclNode& /*node*/) {}

void IRGenerator::visit(BlockStmtNode& node) {
    std::unordered_map<std::string, Operand> pre_block_vars = flatten_scopes(scope_stack_);
    enter_scope();
    for (auto& stmt : node.statements) stmt->accept(*this);
    
    // Bubble up modifications to outer scopes before destroying the block
    auto inner_end_vars = flatten_scopes(scope_stack_);
    exit_scope();
    for (const auto& kv : inner_end_vars) {
        if (pre_block_vars.find(kv.first) != pre_block_vars.end()) {
            if (!operands_equal(pre_block_vars[kv.first], kv.second)) {
                bind_variable(kv.first, kv.second);
            }
        }
    }
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
    instr.comment = node.type_name + " " + node.name;
    emit(instr);
    
    if (!scope_stack_.empty())
        scope_stack_.back()[node.name] = tmp;
}

void IRGenerator::visit(ExprStmtNode& node) {
    if (node.expression) node.expression->accept(*this);
}

void IRGenerator::visit(ReturnStmtNode& node) {
    if (node.value) {
        node.value->accept(*this);
        auto instr = IRInstruction::make_return(last_result_);
        instr.source_line = node.line;
        emit(instr);
    } else {
        auto instr = IRInstruction::make_return_void();
        instr.source_line = node.line;
        emit(instr);
    }
    last_finished_block_ = cur_block_->label;
    cur_block_ = nullptr;
}

void IRGenerator::visit(IfStmtNode& node) {
    std::string then_label = new_label("L_then");
    std::string else_label = node.else_branch ? new_label("L_else") : "";
    std::string end_label = new_label("L_endif");

    node.condition->accept(*this);
    Operand cond = last_result_;
    std::string cond_block_label = cur_block_->label;

    if (node.else_branch) finish_block_cond(cond, then_label, else_label);
    else finish_block_cond(cond, then_label, end_label);

    auto defs_before = scope_stack_;

    // Then
    start_block(then_label);
    node.then_branch->accept(*this);
    bool then_reachable = (cur_block_ != nullptr);
    std::string then_exit_label = cur_block_ ? cur_block_->label : last_finished_block_;
    finish_block_jump(end_label);
    auto defs_then = scope_stack_;

    // Restore
    scope_stack_ = defs_before;

    // Else
    bool else_reachable = false;
    std::string else_exit_label;
    if (node.else_branch) {
        start_block(else_label);
        node.else_branch->accept(*this);
        else_reachable = (cur_block_ != nullptr);
        else_exit_label = cur_block_ ? cur_block_->label : last_finished_block_;
        finish_block_jump(end_label);
    } else {
        else_reachable = true;
        else_exit_label = cond_block_label;
    }

    auto defs_else = scope_stack_;

    start_block(end_label);

    auto flat_then = flatten_scopes(defs_then);
    auto flat_else = flatten_scopes(defs_else);
    
    for (int i = 0; i < static_cast<int>(scope_stack_.size()); ++i) {
        for (auto& kv : scope_stack_[i]) {
            const std::string& var = kv.first;
            Operand t_val = flat_then[var];
            Operand e_val = flat_else[var];
            
            if (then_reachable && else_reachable && !operands_equal(t_val, e_val)) {
                Operand phi_dest = new_temp(t_val.type_annotation.empty() ? e_val.type_annotation : t_val.type_annotation);
                auto phi_inst = IRInstruction::make_phi(phi_dest);
                phi_inst.srcs.push_back(t_val);
                phi_inst.srcs.push_back(Operand::label(then_exit_label));
                phi_inst.srcs.push_back(e_val);
                phi_inst.srcs.push_back(Operand::label(else_exit_label));
                emit(phi_inst);
                kv.second = phi_dest;
            } else if (then_reachable) {
                kv.second = t_val;
            } else if (else_reachable) {
                kv.second = e_val;
            }
        }
    }
}

void IRGenerator::visit(WhileStmtNode& node) {
    std::string header_label = new_label("L_while");
    std::string body_label = new_label("L_body");
    std::string end_label = new_label("L_endwhile");

    std::string pre_header_label = cur_block_->label;
    finish_block_jump(header_label);

    start_block(header_label);

    AssignedVarCollector collector;
    node.body->accept(collector);
    
    auto flat_before = flatten_scopes(scope_stack_);
    std::vector<std::pair<std::string, size_t>> phi_nodes;

    for (const std::string& var : collector.assigned_vars) {
        if (flat_before.find(var) != flat_before.end()) {
            Operand before_val = flat_before[var];
            Operand phi_dest = new_temp(before_val.type_annotation);
            
            auto phi_inst = IRInstruction::make_phi(phi_dest);
            phi_inst.srcs.push_back(before_val);
            phi_inst.srcs.push_back(Operand::label(pre_header_label));
            phi_inst.srcs.push_back(Operand::none());
            phi_inst.srcs.push_back(Operand::none());
            emit(phi_inst);
            
            bind_variable(var, phi_dest);
            phi_nodes.push_back({var, cur_block_->instructions.size() - 1});
        }
    }

    node.condition->accept(*this);
    Operand cond = last_result_;
    finish_block_cond(cond, body_label, end_label);

    start_block(body_label);
    node.body->accept(*this);
    std::string body_exit_label = cur_block_ ? cur_block_->label : last_finished_block_;
    bool body_reachable = (cur_block_ != nullptr);
    finish_block_jump(header_label);

    auto flat_after_body = flatten_scopes(scope_stack_);
    BasicBlock* header_block = cur_func_->find_block(header_label);
    for (auto& pair : phi_nodes) {
        std::string var = pair.first;
        IRInstruction& inst = header_block->instructions[pair.second];
        if (body_reachable) {
            inst.srcs[2] = flat_after_body[var];
            inst.srcs[3] = Operand::label(body_exit_label);
        } else {
            inst.srcs.pop_back(); inst.srcs.pop_back();
        }
    }

    start_block(end_label);
}

void IRGenerator::visit(ForStmtNode& node) {
    std::string header_label = new_label("L_for");
    std::string body_label = new_label("L_forbody");
    std::string update_label = new_label("L_forupd");
    std::string end_label = new_label("L_endfor");

    enter_scope();
    if (node.init) node.init->accept(*this);

    std::string pre_header_label = cur_block_->label;
    finish_block_jump(header_label);

    start_block(header_label);

    AssignedVarCollector collector;
    if (node.update) node.update->accept(collector);
    if (node.body) node.body->accept(collector);

    auto flat_before = flatten_scopes(scope_stack_);
    std::vector<std::pair<std::string, size_t>> phi_nodes;

    for (const std::string& var : collector.assigned_vars) {
        if (flat_before.find(var) != flat_before.end()) {
            Operand before_val = flat_before[var];
            Operand phi_dest = new_temp(before_val.type_annotation);
            auto phi_inst = IRInstruction::make_phi(phi_dest);
            phi_inst.srcs.push_back(before_val);
            phi_inst.srcs.push_back(Operand::label(pre_header_label));
            phi_inst.srcs.push_back(Operand::none());
            phi_inst.srcs.push_back(Operand::none());
            emit(phi_inst);
            
            bind_variable(var, phi_dest);
            phi_nodes.push_back({var, cur_block_->instructions.size() - 1});
        }
    }

    if (node.condition) {
        node.condition->accept(*this);
        finish_block_cond(last_result_, body_label, end_label);
    } else {
        finish_block_jump(body_label);
    }

    start_block(body_label);
    if (node.body) node.body->accept(*this);
    finish_block_jump(update_label);

    start_block(update_label);
    if (node.update) node.update->accept(*this);
    std::string update_exit_label = cur_block_ ? cur_block_->label : last_finished_block_;
    bool update_reachable = (cur_block_ != nullptr);
    finish_block_jump(header_label);

    auto flat_after_body = flatten_scopes(scope_stack_);
    BasicBlock* header_block = cur_func_->find_block(header_label);
    for (auto& pair : phi_nodes) {
        std::string var = pair.first;
        IRInstruction& inst = header_block->instructions[pair.second];
        if (update_reachable) {
            inst.srcs[2] = flat_after_body[var];
            inst.srcs[3] = Operand::label(update_exit_label);
        } else {
            inst.srcs.pop_back(); inst.srcs.pop_back();
        }
    }

    start_block(end_label);
    exit_scope();
}

void IRGenerator::visit(LiteralExprNode& node) {
    switch (node.kind) {
        case LiteralExprNode::Kind::Integer:
            last_result_ = Operand::int_lit(std::get<int>(node.value));
            break;
        case LiteralExprNode::Kind::Float:
            last_result_ = Operand::float_lit(std::get<double>(node.value));
            break;
        case LiteralExprNode::Kind::Bool:
            last_result_ = Operand::bool_lit(std::get<bool>(node.value));
            break;
        case LiteralExprNode::Kind::String:
            last_result_ = Operand::string_lit(std::get<std::string>(node.value));
            break;
    }
}

void IRGenerator::visit(IdentifierExprNode& node) {
    last_result_ = lookup_variable(node.name);
}

void IRGenerator::visit(BinaryExprNode& node) {
    if (node.op == "&&") {
        std::string rhs_label = new_label("L_and_rhs");
        std::string end_label = new_label("L_and_end");

        node.left->accept(*this);
        Operand lhs = last_result_;
        std::string lhs_block = cur_block_->label;
        finish_block_cond(lhs, rhs_label, end_label);

        start_block(rhs_label);
        node.right->accept(*this);
        Operand rhs = last_result_;
        std::string rhs_block = cur_block_ ? cur_block_->label : last_finished_block_;
        finish_block_jump(end_label);

        start_block(end_label);
        Operand result = new_temp("bool");
        auto phi = IRInstruction::make_phi(result);
        phi.srcs.push_back(Operand::bool_lit(false));
        phi.srcs.push_back(Operand::label(lhs_block));
        phi.srcs.push_back(rhs);
        phi.srcs.push_back(Operand::label(rhs_block));
        emit(phi);
        last_result_ = result;
        return;
    }

    if (node.op == "||") {
        std::string rhs_label = new_label("L_or_rhs");
        std::string end_label = new_label("L_or_end");

        node.left->accept(*this);
        Operand lhs = last_result_;
        std::string lhs_block = cur_block_->label;
        finish_block_cond(lhs, end_label, rhs_label);

        start_block(rhs_label);
        node.right->accept(*this);
        Operand rhs = last_result_;
        std::string rhs_block = cur_block_ ? cur_block_->label : last_finished_block_;
        finish_block_jump(end_label);

        start_block(end_label);
        Operand result = new_temp("bool");
        auto phi = IRInstruction::make_phi(result);
        phi.srcs.push_back(Operand::bool_lit(true));
        phi.srcs.push_back(Operand::label(lhs_block));
        phi.srcs.push_back(rhs);
        phi.srcs.push_back(Operand::label(rhs_block));
        emit(phi);
        last_result_ = result;
        return;
    }

    node.left->accept(*this);
    Operand lhs = last_result_;
    node.right->accept(*this);
    Operand rhs = last_result_;

    IROpcode opcode = binary_op_to_opcode(node.op);
    Operand dest = new_temp(type_string(node.resolved_type));

    auto instr = IRInstruction::make_binary(opcode, dest, lhs, rhs);
    instr.source_line = node.line;
    emit(instr);
    last_result_ = dest;
}

void IRGenerator::visit(UnaryExprNode& node) {
    node.operand->accept(*this);
    Operand src = last_result_;

    IROpcode opcode = IROpcode::NOP;
    if (node.op == "-")  opcode = IROpcode::NEG;
    if (node.op == "!")  opcode = IROpcode::NOT;

    Operand dest = new_temp(type_string(node.resolved_type));
    auto instr = IRInstruction::make_unary(opcode, dest, src);
    instr.source_line = node.line;
    emit(instr);
    last_result_ = dest;
}

void IRGenerator::visit(CallExprNode& node) {
    std::vector<Operand> arg_operands;
    for (auto& arg : node.arguments) {
        arg->accept(*this);
        arg_operands.push_back(last_result_);
    }

    for (int i = 0; i < static_cast<int>(arg_operands.size()); ++i) {
        auto instr = IRInstruction::make_param(i, arg_operands[i]);
        instr.source_line = node.line;
        emit(instr);
    }

    Operand dest = new_temp(type_string(node.resolved_type));
    auto instr = IRInstruction::make_call(dest, node.callee, static_cast<int>(node.arguments.size()));
    instr.source_line = node.line;
    emit(instr);
    last_result_ = dest;
}

void IRGenerator::visit(PostfixExprNode& node) {
    auto* ident = dynamic_cast<IdentifierExprNode*>(node.operand.get());
    if (!ident) {
        node.operand->accept(*this);
        return;
    }

    Operand old_val = lookup_variable(ident->name);
    IROpcode op = (node.op == "++") ? IROpcode::ADD : IROpcode::SUB;
    Operand new_val = new_temp(type_string(node.resolved_type));

    auto instr = IRInstruction::make_binary(op, new_val, old_val, Operand::int_lit(1));
    instr.source_line = node.line;
    emit(instr);

    bind_variable(ident->name, new_val);
    last_result_ = old_val;
}

void IRGenerator::visit(AssignmentExprNode& node) {
    node.value->accept(*this);
    Operand rhs = last_result_;

    auto* ident = dynamic_cast<IdentifierExprNode*>(node.target.get());
    if (!ident) {
        last_result_ = rhs;
        return;
    }

    if (node.op == "=") {
        Operand result = new_temp(type_string(node.resolved_type));
        auto instr = IRInstruction::make_move(result, rhs);
        instr.source_line = node.line;
        emit(instr);
        bind_variable(ident->name, result);
        last_result_ = rhs; // assignment evaluates to assigned value
    } else {
        Operand old_val = lookup_variable(ident->name);
        std::string base_op = node.op.substr(0, node.op.size() - 1);
        IROpcode opcode = binary_op_to_opcode(base_op);
        
        Operand result = new_temp(type_string(node.resolved_type));
        auto binop = IRInstruction::make_binary(opcode, result, old_val, rhs);
        binop.source_line = node.line;
        emit(binop);
        
        bind_variable(ident->name, result);
        last_result_ = result;
    }
}
