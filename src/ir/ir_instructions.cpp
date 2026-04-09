#include "ir/ir_instructions.h"

#include <sstream>

// ---------------------------------------------------------------
// opcode_to_string
// ---------------------------------------------------------------
std::string opcode_to_string(IROpcode op) {
    switch (op) {
        case IROpcode::ADD:          return "ADD";
        case IROpcode::SUB:          return "SUB";
        case IROpcode::MUL:          return "MUL";
        case IROpcode::DIV:          return "DIV";
        case IROpcode::MOD:          return "MOD";
        case IROpcode::NEG:          return "NEG";
        case IROpcode::AND:          return "AND";
        case IROpcode::OR:           return "OR";
        case IROpcode::NOT:          return "NOT";
        case IROpcode::XOR:          return "XOR";
        case IROpcode::CMP_EQ:       return "CMP_EQ";
        case IROpcode::CMP_NE:       return "CMP_NE";
        case IROpcode::CMP_LT:       return "CMP_LT";
        case IROpcode::CMP_LE:       return "CMP_LE";
        case IROpcode::CMP_GT:       return "CMP_GT";
        case IROpcode::CMP_GE:       return "CMP_GE";
        case IROpcode::LOAD:         return "LOAD";
        case IROpcode::STORE:        return "STORE";
        case IROpcode::ALLOCA:       return "ALLOCA";
        case IROpcode::MOVE:         return "MOVE";
        case IROpcode::JUMP:         return "JUMP";
        case IROpcode::JUMP_IF:      return "JUMP_IF";
        case IROpcode::JUMP_IF_NOT:  return "JUMP_IF_NOT";
        case IROpcode::LABEL:        return "LABEL";
        case IROpcode::CALL:         return "CALL";
        case IROpcode::RETURN:       return "RETURN";
        case IROpcode::PARAM:        return "PARAM";
        case IROpcode::PHI:          return "PHI";
        case IROpcode::NOP:          return "NOP";
    }
    return "???";
}

// ---------------------------------------------------------------
// operand_to_string
// ---------------------------------------------------------------
std::string operand_to_string(const Operand& op) {
    switch (op.kind) {
        case OperandKind::Temp:
            return op.name;
        case OperandKind::Variable:
            return "[" + op.name + "]";
        case OperandKind::IntLiteral:
            return std::to_string(op.int_val);
        case OperandKind::FloatLiteral: {
            std::ostringstream oss;
            oss << op.float_val;
            return oss.str();
        }
        case OperandKind::BoolLiteral:
            return op.int_val ? "true" : "false";
        case OperandKind::StringLiteral:
            return "\"" + op.name + "\"";
        case OperandKind::Label:
            return op.name;
        case OperandKind::None:
            return "<none>";
    }
    return "???";
}

// ---------------------------------------------------------------
// Operand factory helpers
// ---------------------------------------------------------------
Operand Operand::temp(int id, const std::string& type) {
    Operand o;
    o.kind = OperandKind::Temp;
    o.name = "t" + std::to_string(id);
    o.type_annotation = type;
    return o;
}

Operand Operand::var(const std::string& name, const std::string& type) {
    Operand o;
    o.kind = OperandKind::Variable;
    o.name = name;
    o.type_annotation = type;
    return o;
}

Operand Operand::int_lit(int value) {
    Operand o;
    o.kind = OperandKind::IntLiteral;
    o.int_val = value;
    o.type_annotation = "int";
    return o;
}

Operand Operand::float_lit(double value) {
    Operand o;
    o.kind = OperandKind::FloatLiteral;
    o.float_val = value;
    o.type_annotation = "float";
    return o;
}

Operand Operand::bool_lit(bool value) {
    Operand o;
    o.kind = OperandKind::BoolLiteral;
    o.int_val = value ? 1 : 0;
    o.type_annotation = "bool";
    return o;
}

Operand Operand::string_lit(const std::string& value) {
    Operand o;
    o.kind = OperandKind::StringLiteral;
    o.name = value;
    o.type_annotation = "string";
    return o;
}

Operand Operand::label(const std::string& name) {
    Operand o;
    o.kind = OperandKind::Label;
    o.name = name;
    return o;
}

Operand Operand::none() {
    Operand o;
    o.kind = OperandKind::None;
    return o;
}

// ---------------------------------------------------------------
// IRInstruction factory helpers
// ---------------------------------------------------------------
IRInstruction IRInstruction::make_nop() {
    IRInstruction i;
    i.opcode = IROpcode::NOP;
    return i;
}

IRInstruction IRInstruction::make_label(const std::string& label) {
    IRInstruction i;
    i.opcode = IROpcode::LABEL;
    i.dest = Operand::label(label);
    return i;
}

IRInstruction IRInstruction::make_unary(IROpcode op, Operand dest, Operand src) {
    IRInstruction i;
    i.opcode = op;
    i.dest = dest;
    i.srcs.push_back(src);
    return i;
}

IRInstruction IRInstruction::make_jump(const std::string& label) {
    IRInstruction i;
    i.opcode = IROpcode::JUMP;
    i.dest = Operand::label(label);
    return i;
}

IRInstruction IRInstruction::make_jump_if(Operand cond, const std::string& label) {
    IRInstruction i;
    i.opcode = IROpcode::JUMP_IF;
    i.dest = Operand::label(label);
    i.srcs.push_back(cond);
    return i;
}

IRInstruction IRInstruction::make_jump_if_not(Operand cond, const std::string& label) {
    IRInstruction i;
    i.opcode = IROpcode::JUMP_IF_NOT;
    i.dest = Operand::label(label);
    i.srcs.push_back(cond);
    return i;
}

IRInstruction IRInstruction::make_return(Operand value) {
    IRInstruction i;
    i.opcode = IROpcode::RETURN;
    i.srcs.push_back(value);
    return i;
}

IRInstruction IRInstruction::make_return_void() {
    IRInstruction i;
    i.opcode = IROpcode::RETURN;
    return i;
}

IRInstruction IRInstruction::make_move(Operand dest, Operand src) {
    IRInstruction i;
    i.opcode = IROpcode::MOVE;
    i.dest = dest;
    i.srcs.push_back(src);
    return i;
}

IRInstruction IRInstruction::make_binary(IROpcode op, Operand dest,
                                         Operand src1, Operand src2) {
    IRInstruction i;
    i.opcode = op;
    i.dest = dest;
    i.srcs.push_back(src1);
    i.srcs.push_back(src2);
    return i;
}

IRInstruction IRInstruction::make_load(Operand dest, Operand addr) {
    IRInstruction i;
    i.opcode = IROpcode::LOAD;
    i.dest = dest;
    i.srcs.push_back(addr);
    return i;
}

IRInstruction IRInstruction::make_store(Operand addr, Operand value) {
    IRInstruction i;
    i.opcode = IROpcode::STORE;
    i.dest = addr;
    i.srcs.push_back(value);
    return i;
}

IRInstruction IRInstruction::make_alloca(Operand dest, int size) {
    IRInstruction i;
    i.opcode = IROpcode::ALLOCA;
    i.dest = dest;
    i.srcs.push_back(Operand::int_lit(size));
    return i;
}

IRInstruction IRInstruction::make_param(int index, Operand value) {
    IRInstruction i;
    i.opcode = IROpcode::PARAM;
    i.dest = Operand::int_lit(index);
    i.srcs.push_back(value);
    return i;
}

IRInstruction IRInstruction::make_call(Operand dest, const std::string& func,
                                       int arg_count) {
    IRInstruction i;
    i.opcode = IROpcode::CALL;
    i.dest = dest;
    i.srcs.push_back(Operand::label(func));
    i.srcs.push_back(Operand::int_lit(arg_count));
    return i;
}

IRInstruction IRInstruction::make_call_void(const std::string& func,
                                             int arg_count) {
    IRInstruction i;
    i.opcode = IROpcode::CALL;
    i.dest = Operand::none();
    i.srcs.push_back(Operand::label(func));
    i.srcs.push_back(Operand::int_lit(arg_count));
    return i;
}

// ---------------------------------------------------------------
// instruction_to_string
// ---------------------------------------------------------------
std::string instruction_to_string(const IRInstruction& instr) {
    std::string result = "    ";

    switch (instr.opcode) {
        case IROpcode::LABEL:
            return "  " + operand_to_string(instr.dest) + ":";

        case IROpcode::JUMP:
            result += "JUMP " + operand_to_string(instr.dest);
            break;

        case IROpcode::JUMP_IF:
            result += "JUMP_IF " + operand_to_string(instr.srcs[0])
                    + ", " + operand_to_string(instr.dest);
            break;

        case IROpcode::JUMP_IF_NOT:
            result += "JUMP_IF_NOT " + operand_to_string(instr.srcs[0])
                    + ", " + operand_to_string(instr.dest);
            break;

        case IROpcode::RETURN:
            result += "RETURN";
            if (!instr.srcs.empty())
                result += " " + operand_to_string(instr.srcs[0]);
            break;

        case IROpcode::PARAM:
            result += "PARAM " + operand_to_string(instr.dest)
                    + ", " + operand_to_string(instr.srcs[0]);
            break;

        case IROpcode::CALL:
            if (!instr.dest.is_none())
                result += operand_to_string(instr.dest) + " = ";
            result += "CALL " + operand_to_string(instr.srcs[0])
                    + ", " + operand_to_string(instr.srcs[1]);
            break;

        case IROpcode::STORE:
            result += "STORE " + operand_to_string(instr.dest)
                    + ", " + operand_to_string(instr.srcs[0]);
            break;

        case IROpcode::LOAD:
            result += operand_to_string(instr.dest) + " = LOAD "
                    + operand_to_string(instr.srcs[0]);
            break;

        case IROpcode::ALLOCA:
            result += operand_to_string(instr.dest) + " = ALLOCA "
                    + operand_to_string(instr.srcs[0]);
            break;

        case IROpcode::MOVE:
            result += operand_to_string(instr.dest) + " = MOVE "
                    + operand_to_string(instr.srcs[0]);
            break;

        case IROpcode::NOP:
            result += "NOP";
            break;

        default:
            // Binary/unary ops: dest = OP src1 [, src2]
            result += operand_to_string(instr.dest) + " = "
                    + opcode_to_string(instr.opcode);
            for (size_t j = 0; j < instr.srcs.size(); ++j) {
                result += (j == 0 ? " " : ", ");
                result += operand_to_string(instr.srcs[j]);
            }
            break;
    }

    if (!instr.comment.empty())
        result += "    # " + instr.comment;

    return result;
}

// ---------------------------------------------------------------
// is_terminator
// ---------------------------------------------------------------
bool is_terminator(IROpcode op) {
    return op == IROpcode::JUMP ||
           op == IROpcode::JUMP_IF ||
           op == IROpcode::JUMP_IF_NOT ||
           op == IROpcode::RETURN;
}
