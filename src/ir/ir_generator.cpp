#include "ir/ir_generator.h"

#include <cassert>
#include <stdexcept>

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
    if (!scope_stack_.empty())
        scope_stack_.back()[name] = operand;
}

Operand IRGenerator::lookup_variable(const std::string& name) {
    for (int i = static_cast<int>(scope_stack_.size()) - 1; i >= 0; --i) {
        auto it = scope_stack_[i].find(name);
        if (it != scope_stack_[i].end())
            return it->second;
    }
    // Fallback: use variable name directly
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
// ProgramNode — iterate over declarations
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

    // Add parameters
    for (const auto& p : node.parameters)
        func.params.push_back({p.name, p.type_name});

    enter_scope();

    // Entry block
    start_block("entry");

    // Bind parameters as variables
    for (int i = 0; i < static_cast<int>(node.parameters.size()); ++i) {
        const auto& p = node.parameters[i];
        Operand param_var = Operand::var(p.name, p.type_name);
        bind_variable(p.name, param_var);
    }

    // Generate body
    if (node.body)
        node.body->accept(*this);

    // Ensure function ends with a return
    if (cur_block_ && !cur_block_->has_terminator()) {
        if (node.return_type == "void")
            finish_block_return_void();
        else
            finish_block_return(Operand::int_lit(0));  // implicit return 0
    }

    exit_scope();
    cur_func_ = nullptr;
}

// ---------------------------------------------------------------
// StructDeclNode — no IR generated (type info only)
// ---------------------------------------------------------------
void IRGenerator::visit(StructDeclNode& /*node*/) {
    // Struct declarations don't produce IR instructions
}

// ---------------------------------------------------------------
// BlockStmtNode — new scope, generate all statements
// ---------------------------------------------------------------
void IRGenerator::visit(BlockStmtNode& node) {
    enter_scope();
    for (auto& stmt : node.statements)
        stmt->accept(*this);
    exit_scope();
}

// ---------------------------------------------------------------
// VarDeclStmtNode — allocate + optional init
// ---------------------------------------------------------------
void IRGenerator::visit(VarDeclStmtNode& node) {
    Operand var = Operand::var(node.name, node.type_name);
    bind_variable(node.name, var);

    if (node.initializer) {
        node.initializer->accept(*this);
        Operand val = last_result_;

        auto instr = IRInstruction::make_store(var, val);
        instr.source_line = node.line;
        instr.comment = node.type_name + " " + node.name + " = ...";
        emit(instr);
    }
}

// ---------------------------------------------------------------
// ExprStmtNode — generate expression, discard result
// ---------------------------------------------------------------
void IRGenerator::visit(ExprStmtNode& node) {
    if (node.expression)
        node.expression->accept(*this);
}

// ---------------------------------------------------------------
// ReturnStmtNode
// ---------------------------------------------------------------
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
    cur_block_ = nullptr;  // unreachable after return
}

// ---------------------------------------------------------------
// IfStmtNode — conditional branches
// ---------------------------------------------------------------
void IRGenerator::visit(IfStmtNode& node) {
    std::string then_label = new_label("L_then");
    std::string else_label = node.else_branch ? new_label("L_else") : "";
    std::string end_label = new_label("L_endif");

    // Evaluate condition
    node.condition->accept(*this);
    Operand cond = last_result_;

    if (node.else_branch) {
        finish_block_cond(cond, then_label, else_label);
    } else {
        finish_block_cond(cond, then_label, end_label);
    }

    // Then branch
    start_block(then_label);
    node.then_branch->accept(*this);
    finish_block_jump(end_label);

    // Else branch
    if (node.else_branch) {
        start_block(else_label);
        node.else_branch->accept(*this);
        finish_block_jump(end_label);
    }

    // Continuation
    start_block(end_label);
}

// ---------------------------------------------------------------
// WhileStmtNode — loop header + body + back edge
// ---------------------------------------------------------------
void IRGenerator::visit(WhileStmtNode& node) {
    std::string header_label = new_label("L_while");
    std::string body_label = new_label("L_body");
    std::string end_label = new_label("L_endwhile");

    // Jump to header
    finish_block_jump(header_label);

    // Header: evaluate condition
    start_block(header_label);
    node.condition->accept(*this);
    Operand cond = last_result_;
    finish_block_cond(cond, body_label, end_label);

    // Body
    start_block(body_label);
    node.body->accept(*this);
    finish_block_jump(header_label);  // back edge

    // Exit
    start_block(end_label);
}

// ---------------------------------------------------------------
// ForStmtNode — init + header + body + update + back edge
// ---------------------------------------------------------------
void IRGenerator::visit(ForStmtNode& node) {
    std::string header_label = new_label("L_for");
    std::string body_label = new_label("L_forbody");
    std::string update_label = new_label("L_forupd");
    std::string end_label = new_label("L_endfor");

    // Init
    enter_scope();
    if (node.init)
        node.init->accept(*this);

    finish_block_jump(header_label);

    // Header: evaluate condition
    start_block(header_label);
    if (node.condition) {
        node.condition->accept(*this);
        Operand cond = last_result_;
        finish_block_cond(cond, body_label, end_label);
    } else {
        finish_block_jump(body_label);
    }

    // Body
    start_block(body_label);
    node.body->accept(*this);
    finish_block_jump(update_label);

    // Update
    start_block(update_label);
    if (node.update)
        node.update->accept(*this);
    finish_block_jump(header_label);  // back edge

    // Exit
    start_block(end_label);
    exit_scope();
}

// ---------------------------------------------------------------
// LiteralExprNode — produce constant operand
// ---------------------------------------------------------------
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

// ---------------------------------------------------------------
// IdentifierExprNode — load variable into temp
// ---------------------------------------------------------------
void IRGenerator::visit(IdentifierExprNode& node) {
    Operand var = lookup_variable(node.name);
    Operand tmp = new_temp(type_string(node.resolved_type));

    auto instr = IRInstruction::make_load(tmp, var);
    instr.source_line = node.line;
    emit(instr);

    last_result_ = tmp;
}

// ---------------------------------------------------------------
// BinaryExprNode — translate to 3-address operation
// ---------------------------------------------------------------
void IRGenerator::visit(BinaryExprNode& node) {
    // Short-circuit for && and ||
    if (node.op == "&&") {
        std::string rhs_label = new_label("L_and_rhs");
        std::string end_label = new_label("L_and_end");
        Operand result = new_temp("bool");

        // Evaluate LHS
        node.left->accept(*this);
        Operand lhs = last_result_;

        // If LHS is false, short-circuit to false
        emit(IRInstruction::make_move(result, Operand::bool_lit(false)));
        finish_block_cond(lhs, rhs_label, end_label);

        // Evaluate RHS
        start_block(rhs_label);
        node.right->accept(*this);
        emit(IRInstruction::make_move(result, last_result_));
        finish_block_jump(end_label);

        start_block(end_label);
        last_result_ = result;
        return;
    }

    if (node.op == "||") {
        std::string rhs_label = new_label("L_or_rhs");
        std::string end_label = new_label("L_or_end");
        Operand result = new_temp("bool");

        // Evaluate LHS
        node.left->accept(*this);
        Operand lhs = last_result_;

        // If LHS is true, short-circuit to true
        emit(IRInstruction::make_move(result, Operand::bool_lit(true)));
        finish_block_cond(lhs, end_label, rhs_label);

        // Evaluate RHS
        start_block(rhs_label);
        node.right->accept(*this);
        emit(IRInstruction::make_move(result, last_result_));
        finish_block_jump(end_label);

        start_block(end_label);
        last_result_ = result;
        return;
    }

    // Normal binary operations
    node.left->accept(*this);
    Operand lhs = last_result_;

    node.right->accept(*this);
    Operand rhs = last_result_;

    IROpcode opcode = binary_op_to_opcode(node.op);
    Operand dest = new_temp(type_string(node.resolved_type));

    auto instr = IRInstruction::make_binary(opcode, dest, lhs, rhs);
    instr.source_line = node.line;
    instr.comment = node.op;
    emit(instr);

    last_result_ = dest;
}

// ---------------------------------------------------------------
// UnaryExprNode — NEG or NOT
// ---------------------------------------------------------------
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

// ---------------------------------------------------------------
// CallExprNode — PARAM setup + CALL
// ---------------------------------------------------------------
void IRGenerator::visit(CallExprNode& node) {
    // Evaluate and emit PARAM for each argument
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

    // CALL
    Operand dest = new_temp(type_string(node.resolved_type));
    auto instr = IRInstruction::make_call(dest, node.callee,
                                           static_cast<int>(node.arguments.size()));
    instr.source_line = node.line;
    instr.comment = "call " + node.callee;
    emit(instr);

    last_result_ = dest;
}

// ---------------------------------------------------------------
// PostfixExprNode — x++ or x--
// ---------------------------------------------------------------
void IRGenerator::visit(PostfixExprNode& node) {
    // Get the variable
    auto* ident = dynamic_cast<IdentifierExprNode*>(node.operand.get());
    if (!ident) {
        // Fallback: just evaluate the operand
        node.operand->accept(*this);
        return;
    }

    Operand var = lookup_variable(ident->name);
    Operand old_val = new_temp(type_string(node.resolved_type));
    emit(IRInstruction::make_load(old_val, var));

    IROpcode op = (node.op == "++") ? IROpcode::ADD : IROpcode::SUB;
    Operand new_val = new_temp(type_string(node.resolved_type));

    auto instr = IRInstruction::make_binary(op, new_val, old_val, Operand::int_lit(1));
    instr.source_line = node.line;
    instr.comment = ident->name + node.op;
    emit(instr);

    emit(IRInstruction::make_store(var, new_val));

    // Postfix returns the OLD value
    last_result_ = old_val;
}

// ---------------------------------------------------------------
// AssignmentExprNode — compute value + store
// ---------------------------------------------------------------
void IRGenerator::visit(AssignmentExprNode& node) {
    // Evaluate the right-hand side
    node.value->accept(*this);
    Operand rhs = last_result_;

    // Get the target variable
    auto* ident = dynamic_cast<IdentifierExprNode*>(node.target.get());
    if (!ident) {
        // Not a simple variable — just set result for now
        last_result_ = rhs;
        return;
    }

    Operand var = lookup_variable(ident->name);

    if (node.op == "=") {
        auto instr = IRInstruction::make_store(var, rhs);
        instr.source_line = node.line;
        instr.comment = ident->name + " = ...";
        emit(instr);
        last_result_ = rhs;
    } else {
        // Compound assignment: +=, -=, *=, /=
        Operand old_val = new_temp(type_string(node.resolved_type));
        emit(IRInstruction::make_load(old_val, var));

        std::string base_op = node.op.substr(0, node.op.size() - 1);
        IROpcode opcode = binary_op_to_opcode(base_op);
        Operand result = new_temp(type_string(node.resolved_type));

        auto binop = IRInstruction::make_binary(opcode, result, old_val, rhs);
        binop.source_line = node.line;
        emit(binop);

        auto store = IRInstruction::make_store(var, result);
        store.source_line = node.line;
        store.comment = ident->name + " " + node.op + " ...";
        emit(store);

        last_result_ = result;
    }
}
