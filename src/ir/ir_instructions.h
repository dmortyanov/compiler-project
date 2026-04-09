#pragma once

#include <string>
#include <variant>
#include <vector>

// ---------------------------------------------------------------
// IROpcode — all three-address code operation codes
// ---------------------------------------------------------------
enum class IROpcode {
    // Arithmetic
    ADD, SUB, MUL, DIV, MOD, NEG,
    // Logical
    AND, OR, NOT, XOR,
    // Comparison
    CMP_EQ, CMP_NE, CMP_LT, CMP_LE, CMP_GT, CMP_GE,
    // Memory
    LOAD, STORE, ALLOCA,
    // Data movement
    MOVE,
    // Control flow
    JUMP, JUMP_IF, JUMP_IF_NOT, LABEL,
    // Function operations
    CALL, RETURN, PARAM,
    // SSA (defined but not auto-inserted)
    PHI,
    // No-op
    NOP
};

std::string opcode_to_string(IROpcode op);

// ---------------------------------------------------------------
// OperandKind — type tag for operands
// ---------------------------------------------------------------
enum class OperandKind {
    Temp,           // virtual register: t0, t1, ...
    Variable,       // source variable name
    IntLiteral,     // integer constant
    FloatLiteral,   // floating-point constant
    BoolLiteral,    // boolean constant
    StringLiteral,  // string constant
    Label,          // basic block label
    None            // empty / unused
};

// ---------------------------------------------------------------
// Operand — a single operand in an IR instruction
// ---------------------------------------------------------------
struct Operand {
    OperandKind kind = OperandKind::None;
    std::string name;           // for Temp, Variable, Label, StringLiteral
    int int_val = 0;            // for IntLiteral, BoolLiteral (0/1)
    double float_val = 0.0;     // for FloatLiteral
    std::string type_annotation; // optional type info: "int", "float", etc.

    // ----- factory helpers -----
    static Operand temp(int id, const std::string& type = "");
    static Operand var(const std::string& name, const std::string& type = "");
    static Operand int_lit(int value);
    static Operand float_lit(double value);
    static Operand bool_lit(bool value);
    static Operand string_lit(const std::string& value);
    static Operand label(const std::string& name);
    static Operand none();

    bool is_none() const { return kind == OperandKind::None; }
    bool is_temp() const { return kind == OperandKind::Temp; }
    bool is_literal() const {
        return kind == OperandKind::IntLiteral ||
               kind == OperandKind::FloatLiteral ||
               kind == OperandKind::BoolLiteral ||
               kind == OperandKind::StringLiteral;
    }
};

std::string operand_to_string(const Operand& op);

// ---------------------------------------------------------------
// IRInstruction — a single three-address code instruction
// ---------------------------------------------------------------
struct IRInstruction {
    IROpcode opcode = IROpcode::NOP;
    Operand dest;                       // destination operand
    std::vector<Operand> srcs;          // source operands

    int source_line = 0;                // corresponding source line
    std::string comment;                // optional comment

    // ----- convenience constructors -----

    // Nullary: NOP, LABEL
    static IRInstruction make_nop();
    static IRInstruction make_label(const std::string& label);

    // Unary: NEG, NOT, MOVE, RETURN, JUMP, JUMP_IF, JUMP_IF_NOT
    static IRInstruction make_unary(IROpcode op, Operand dest, Operand src);
    static IRInstruction make_jump(const std::string& label);
    static IRInstruction make_jump_if(Operand cond, const std::string& label);
    static IRInstruction make_jump_if_not(Operand cond, const std::string& label);
    static IRInstruction make_return(Operand value);
    static IRInstruction make_return_void();
    static IRInstruction make_move(Operand dest, Operand src);

    // Binary: ADD, SUB, MUL, ...
    static IRInstruction make_binary(IROpcode op, Operand dest,
                                     Operand src1, Operand src2);

    // Memory
    static IRInstruction make_load(Operand dest, Operand addr);
    static IRInstruction make_store(Operand addr, Operand value);
    static IRInstruction make_alloca(Operand dest, int size = 4);

    // Function
    static IRInstruction make_param(int index, Operand value);
    static IRInstruction make_call(Operand dest, const std::string& func,
                                   int arg_count);
    static IRInstruction make_call_void(const std::string& func,
                                        int arg_count);
};

std::string instruction_to_string(const IRInstruction& instr);

// ---------------------------------------------------------------
// Check if an opcode is a terminator (ends a basic block)
// ---------------------------------------------------------------
bool is_terminator(IROpcode op);
