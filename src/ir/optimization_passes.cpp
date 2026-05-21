#include "ir/optimization_passes.h"
#include <map>

FunctionInliner::FunctionInliner(IRProgram& program) : program_(program) {}

void FunctionInliner::run() {
    bool changed = true;
    while (changed) {
        changed = false;
        int prev = functions_inlined_;
        for (auto& caller : program_.functions) {
            if (caller.blocks.empty()) continue;
            inline_calls(caller);
        }
        if (functions_inlined_ > prev) {
            changed = true;
        }
    }
}

bool FunctionInliner::should_inline(const IRFunction& func) const {
    if (func.blocks.empty()) return false;
    int instr_count = 0;
    for (const auto& b : func.blocks) {
        for (const auto& i : b.instructions) {
            instr_count++;
            if (i.opcode == IROpcode::CALL) return false; // Don't inline functions that call other functions (avoid recursion/complexity)
        }
    }
    if (instr_count > 10) return false;
    if (func.params.size() > 4) return false;
    return true;
}

void FunctionInliner::inline_calls(IRFunction& caller) {
    std::vector<BasicBlock> new_blocks;
    for (size_t b_idx = 0; b_idx < caller.blocks.size(); ++b_idx) {
        auto& block = caller.blocks[b_idx];
        std::vector<IRInstruction> current_instrs;
        
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& instr = block.instructions[i];
            
            if (instr.opcode == IROpcode::CALL) {
                std::string func_name = instr.srcs[0].name;
                const IRFunction* callee = program_.find_function(func_name);
                
                if (callee && callee->name != caller.name && should_inline(*callee)) {
                    // Extract params
                    std::map<int, Operand> args;
                    for (int j = (int)current_instrs.size() - 1; j >= 0; --j) {
                        if (current_instrs[j].opcode == IROpcode::PARAM) {
                            args[current_instrs[j].dest.int_val] = current_instrs[j].srcs[0];
                            current_instrs.erase(current_instrs.begin() + j);
                        } else {
                            break;
                        }
                    }
                    
                    inline_counter_++;
                    std::string suffix = "_inl" + std::to_string(inline_counter_);
                    
                    std::string after_label = caller.new_label("L_after_inline");
                    
                    // 1. Jump to inlined start
                    current_instrs.push_back(IRInstruction::make_jump(callee->blocks[0].label + suffix));
                    
                    BasicBlock bb_copy = block;
                    bb_copy.instructions = current_instrs;
                    new_blocks.push_back(bb_copy);
                    
                    // 2. Append callee blocks
                    for (size_t cb = 0; cb < callee->blocks.size(); ++cb) {
                        BasicBlock inlined_block = callee->blocks[cb];
                        inlined_block.label += suffix;
                        
                        for (auto& cinstr : inlined_block.instructions) {
                            if (cinstr.opcode == IROpcode::JUMP || cinstr.opcode == IROpcode::JUMP_IF || cinstr.opcode == IROpcode::JUMP_IF_NOT) {
                                cinstr.dest.name += suffix;
                            }
                            if (cinstr.opcode == IROpcode::LABEL) {
                                cinstr.dest.name += suffix;
                            }
                            
                            auto rename_op = [&](Operand& op) {
                                if (op.is_temp() || op.kind == OperandKind::Variable) {
                                    bool is_param = false;
                                    for (size_t p = 0; p < callee->params.size(); ++p) {
                                        if (op.name == callee->params[p].first) {
                                            op = args[p];
                                            is_param = true;
                                            break;
                                        }
                                    }
                                    if (!is_param) {
                                        op.name += suffix;
                                    }
                                }
                            };
                            
                            if (!cinstr.dest.is_none()) rename_op(cinstr.dest);
                            for (auto& src : cinstr.srcs) rename_op(src);
                            
                            if (cinstr.opcode == IROpcode::RETURN) {
                                if (!instr.dest.is_none() && !cinstr.srcs.empty()) {
                                    cinstr = IRInstruction::make_move(instr.dest, cinstr.srcs[0]);
                                    inlined_block.instructions.push_back(IRInstruction::make_jump(after_label));
                                } else {
                                    cinstr = IRInstruction::make_jump(after_label);
                                }
                            }
                        }
                        new_blocks.push_back(inlined_block);
                    }
                    
                    // 3. Create the "after" block
                    current_instrs.clear();
                    block.label = after_label;
                    functions_inlined_++;
                } else {
                    current_instrs.push_back(instr);
                }
            } else {
                current_instrs.push_back(instr);
            }
        }
        
        BasicBlock bb_final = block;
        bb_final.instructions = current_instrs;
        if (!current_instrs.empty() || bb_final.label.find("L_after_inline") != std::string::npos) {
            new_blocks.push_back(bb_final);
        }
    }
    caller.blocks = new_blocks;
}
