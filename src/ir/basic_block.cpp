#include "ir/basic_block.h"

#include <algorithm>

// ---------------------------------------------------------------
// BasicBlock
// ---------------------------------------------------------------
void BasicBlock::add_instruction(const IRInstruction& instr) {
    instructions.push_back(instr);
}

bool BasicBlock::has_terminator() const {
    if (instructions.empty()) return false;
    return is_terminator(instructions.back().opcode);
}

const IRInstruction& BasicBlock::terminator() const {
    return instructions.back();
}

// ---------------------------------------------------------------
// IRFunction
// ---------------------------------------------------------------
Operand IRFunction::new_temp(const std::string& type) {
    return Operand::temp(temp_counter++, type);
}

std::string IRFunction::new_label(const std::string& prefix) {
    return prefix + "_" + std::to_string(label_counter++);
}

BasicBlock& IRFunction::add_block(const std::string& lbl) {
    blocks.push_back(BasicBlock{});
    blocks.back().label = lbl;
    return blocks.back();
}

BasicBlock* IRFunction::find_block(const std::string& lbl) {
    for (auto& b : blocks)
        if (b.label == lbl) return &b;
    return nullptr;
}

const BasicBlock* IRFunction::find_block(const std::string& lbl) const {
    for (const auto& b : blocks)
        if (b.label == lbl) return &b;
    return nullptr;
}

void IRFunction::link_blocks(const std::string& from, const std::string& to) {
    BasicBlock* fb = find_block(from);
    BasicBlock* tb = find_block(to);
    if (!fb || !tb) return;

    auto& s = fb->successors;
    if (std::find(s.begin(), s.end(), to) == s.end())
        s.push_back(to);

    auto& p = tb->predecessors;
    if (std::find(p.begin(), p.end(), from) == p.end())
        p.push_back(from);
}

// ---------------------------------------------------------------
// IRProgram
// ---------------------------------------------------------------
IRFunction& IRProgram::add_function(const std::string& name,
                                     const std::string& return_type) {
    functions.push_back(IRFunction{});
    functions.back().name = name;
    functions.back().return_type = return_type;
    return functions.back();
}

const IRFunction* IRProgram::find_function(const std::string& name) const {
    for (const auto& f : functions)
        if (f.name == name) return &f;
    return nullptr;
}
