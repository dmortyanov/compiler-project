#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "ir/ir_instructions.h"

// ---------------------------------------------------------------
// BasicBlock — a sequence of IR instructions ending with a
//              control-flow terminator
// ---------------------------------------------------------------
struct BasicBlock {
    std::string label;
    std::vector<IRInstruction> instructions;
    std::vector<std::string> successors;
    std::vector<std::string> predecessors;

    void add_instruction(const IRInstruction& instr);
    bool has_terminator() const;
    const IRInstruction& terminator() const;
};

// ---------------------------------------------------------------
// IRFunction — a single function in the IR program
// ---------------------------------------------------------------
struct IRFunction {
    std::string name;
    std::string return_type;
    std::vector<std::pair<std::string, std::string>> params;  // (name, type)

    std::vector<BasicBlock> blocks;

    // Variable → memory location mapping
    std::unordered_map<std::string, std::string> var_to_location;

    // Counters
    int temp_counter = 0;
    int label_counter = 0;

    // ----- helpers -----
    Operand new_temp(const std::string& type = "");
    std::string new_label(const std::string& prefix = "L");
    BasicBlock& add_block(const std::string& label);
    BasicBlock* find_block(const std::string& label);
    const BasicBlock* find_block(const std::string& label) const;
    void link_blocks(const std::string& from, const std::string& to);
};

// ---------------------------------------------------------------
// IRProgram — the complete IR representation of a program
// ---------------------------------------------------------------
struct IRProgram {
    std::vector<IRFunction> functions;

    IRFunction& add_function(const std::string& name,
                             const std::string& return_type);
    const IRFunction* find_function(const std::string& name) const;
};
