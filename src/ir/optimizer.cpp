#include "ir/optimizer.h"

#include <algorithm>
#include <map>
#include <set>
#include <sstream>

// ---------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------
PeepholeOptimizer::PeepholeOptimizer(IRProgram& program)
    : program_(program) {}

// ---------------------------------------------------------------
// optimize — run all passes
// ---------------------------------------------------------------
void PeepholeOptimizer::optimize() {
    int prev_modified = -1;
    while (prev_modified != metrics_.instructions_modified) {
        prev_modified = metrics_.instructions_modified;
        for (auto& func : program_.functions) {
            propagate_copies(func);
            fold_constants(func);
            simplify_algebraic(func);
            reduce_strength(func);
            eliminate_common_subexpressions(func);
            eliminate_dead_code(func);
            chain_jumps(func);
        }
    }
}

// ---------------------------------------------------------------
// add_entry
// ---------------------------------------------------------------
void PeepholeOptimizer::add_entry(const std::string& func,
                                   const std::string& block,
                                   int idx, const std::string& desc) {
    log_.push_back({func, block, desc, idx});
}

// ---------------------------------------------------------------
// fold_constants — evaluate constant expressions at compile time
//   e.g. ADD t1, 3, 4 → MOVE t1, 7
// ---------------------------------------------------------------
void PeepholeOptimizer::fold_constants(IRFunction& func) {
    for (auto& block : func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& instr = block.instructions[i];

            // Only binary ops with two int literal sources
            if (instr.srcs.size() != 2) continue;
            if (instr.srcs[0].kind != OperandKind::IntLiteral) continue;
            if (instr.srcs[1].kind != OperandKind::IntLiteral) continue;

            int a = instr.srcs[0].int_val;
            int b = instr.srcs[1].int_val;
            int result = 0;
            bool fold = true;

            switch (instr.opcode) {
                case IROpcode::ADD: result = a + b; break;
                case IROpcode::SUB: result = a - b; break;
                case IROpcode::MUL: result = a * b; break;
                case IROpcode::DIV:
                    if (b == 0) { fold = false; break; }
                    result = a / b; break;
                case IROpcode::MOD:
                    if (b == 0) { fold = false; break; }
                    result = a % b; break;
                case IROpcode::CMP_EQ: result = (a == b) ? 1 : 0; break;
                case IROpcode::CMP_NE: result = (a != b) ? 1 : 0; break;
                case IROpcode::CMP_LT: result = (a < b)  ? 1 : 0; break;
                case IROpcode::CMP_LE: result = (a <= b) ? 1 : 0; break;
                case IROpcode::CMP_GT: result = (a > b)  ? 1 : 0; break;
                case IROpcode::CMP_GE: result = (a >= b) ? 1 : 0; break;
                default: fold = false; break;
            }

            if (fold) {
                std::string old_str = instruction_to_string(instr);
                instr = IRInstruction::make_move(instr.dest, Operand::int_lit(result));
                instr.comment = "folded: " + old_str;
                metrics_.constants_folded++;
                metrics_.instructions_modified++;
                add_entry(func.name, block.label, static_cast<int>(i),
                         "constant fold: " + old_str + " → " + std::to_string(result));
            }
        }
    }
}

// ---------------------------------------------------------------
// simplify_algebraic — identity/zero simplifications
//   x + 0 → x,  x * 1 → x,  x * 0 → 0,  x - 0 → x
// ---------------------------------------------------------------
void PeepholeOptimizer::simplify_algebraic(IRFunction& func) {
    for (auto& block : func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& instr = block.instructions[i];
            if (instr.srcs.size() != 2) continue;

            auto& s0 = instr.srcs[0];
            auto& s1 = instr.srcs[1];

            bool simplified = false;
            Operand replacement = Operand::none();

            // x + 0 → x  or  0 + x → x
            if (instr.opcode == IROpcode::ADD) {
                if (s1.kind == OperandKind::IntLiteral && s1.int_val == 0) {
                    replacement = s0; simplified = true;
                } else if (s0.kind == OperandKind::IntLiteral && s0.int_val == 0) {
                    replacement = s1; simplified = true;
                }
            }
            // x - 0 → x
            if (instr.opcode == IROpcode::SUB) {
                if (s1.kind == OperandKind::IntLiteral && s1.int_val == 0) {
                    replacement = s0; simplified = true;
                }
            }
            // x * 1 → x  or  1 * x → x
            if (instr.opcode == IROpcode::MUL) {
                if (s1.kind == OperandKind::IntLiteral && s1.int_val == 1) {
                    replacement = s0; simplified = true;
                } else if (s0.kind == OperandKind::IntLiteral && s0.int_val == 1) {
                    replacement = s1; simplified = true;
                }
                // x * 0 → 0  or  0 * x → 0
                else if (s1.kind == OperandKind::IntLiteral && s1.int_val == 0) {
                    replacement = Operand::int_lit(0); simplified = true;
                } else if (s0.kind == OperandKind::IntLiteral && s0.int_val == 0) {
                    replacement = Operand::int_lit(0); simplified = true;
                }
            }
            // x / 1 → x
            if (instr.opcode == IROpcode::DIV) {
                if (s1.kind == OperandKind::IntLiteral && s1.int_val == 1) {
                    replacement = s0; simplified = true;
                }
            }

            if (simplified) {
                Operand dest = instr.dest;
                instr = IRInstruction::make_move(dest, replacement);
                instr.comment = "algebraic simplification";
                metrics_.algebraic_simplifications++;
                metrics_.instructions_modified++;
                add_entry(func.name, block.label, static_cast<int>(i),
                         "algebraic simplification");
            }
        }
    }
}

// ---------------------------------------------------------------
// reduce_strength — replace expensive ops with cheaper ones
//   x * 2 → x + x,   x * power_of_2 → x << log2(n)
// ---------------------------------------------------------------
void PeepholeOptimizer::reduce_strength(IRFunction& func) {
    for (auto& block : func.blocks) {
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& instr = block.instructions[i];
            if (instr.opcode != IROpcode::MUL) continue;
            if (instr.srcs.size() != 2) continue;

            // x * 2 → x + x
            auto& s0 = instr.srcs[0];
            auto& s1 = instr.srcs[1];

            if (s1.kind == OperandKind::IntLiteral && s1.int_val == 2) {
                Operand dest = instr.dest;
                instr = IRInstruction::make_binary(IROpcode::ADD, dest, s0, s0);
                instr.comment = "strength reduction: * 2 → + self";
                metrics_.strength_reductions++;
                metrics_.instructions_modified++;
                add_entry(func.name, block.label, static_cast<int>(i),
                         "strength reduction: MUL x, 2 → ADD x, x");
            } else if (s0.kind == OperandKind::IntLiteral && s0.int_val == 2) {
                Operand dest = instr.dest;
                instr = IRInstruction::make_binary(IROpcode::ADD, dest, s1, s1);
                instr.comment = "strength reduction: 2 * x → x + x";
                metrics_.strength_reductions++;
                metrics_.instructions_modified++;
                add_entry(func.name, block.label, static_cast<int>(i),
                         "strength reduction: MUL 2, x → ADD x, x");
            }
        }
    }
}

// ---------------------------------------------------------------
// eliminate_dead_code — remove unused MOVE/LOAD assignments
// Simple: if a temp is defined but never used, remove it
// ---------------------------------------------------------------
void PeepholeOptimizer::eliminate_dead_code(IRFunction& func) {
    // Collect all used operands
    std::set<std::string> used;
    for (const auto& block : func.blocks) {
        for (const auto& instr : block.instructions) {
            for (const auto& src : instr.srcs) {
                if (src.kind == OperandKind::Temp)
                    used.insert(src.name);
            }
            // Jump conditions also use operands
            if (instr.opcode == IROpcode::JUMP_IF ||
                instr.opcode == IROpcode::JUMP_IF_NOT) {
                if (!instr.srcs.empty() && instr.srcs[0].kind == OperandKind::Temp)
                    used.insert(instr.srcs[0].name);
            }
            // Return value
            if (instr.opcode == IROpcode::RETURN) {
                if (!instr.srcs.empty() && instr.srcs[0].kind == OperandKind::Temp)
                    used.insert(instr.srcs[0].name);
            }
        }
    }

    // Remove instructions whose dest temp is never used
    for (auto& block : func.blocks) {
        auto it = block.instructions.begin();
        while (it != block.instructions.end()) {
            if (it->dest.kind == OperandKind::Temp &&
                !it->dest.is_none() &&
                used.find(it->dest.name) == used.end() &&
                it->opcode != IROpcode::CALL &&
                it->opcode != IROpcode::STORE &&
                !is_terminator(it->opcode)) {
                add_entry(func.name, block.label, 0,
                         "dead code: removed unused " + it->dest.name);
                it = block.instructions.erase(it);
                metrics_.dead_code_eliminated++;
                metrics_.instructions_removed++;
            } else {
                ++it;
            }
        }
    }
}

// ---------------------------------------------------------------
// chain_jumps — JUMP L1; L1: JUMP L2 → JUMP L2
// ---------------------------------------------------------------
void PeepholeOptimizer::chain_jumps(IRFunction& func) {
    // Build map: label → target (if the block only contains a JUMP)
    std::unordered_map<std::string, std::string> redirect;
    for (const auto& block : func.blocks) {
        // Block that only has a single JUMP (possibly after a LABEL)
        int real_count = 0;
        std::string jump_target;
        for (const auto& instr : block.instructions) {
            if (instr.opcode == IROpcode::LABEL) continue;
            real_count++;
            if (instr.opcode == IROpcode::JUMP) {
                jump_target = instr.dest.name;
            }
        }
        if (real_count == 1 && !jump_target.empty()) {
            redirect[block.label] = jump_target;
        }
    }

    // Follow chains
    auto resolve = [&](const std::string& lbl) -> std::string {
        std::string cur = lbl;
        std::set<std::string> visited;
        while (redirect.count(cur) && !visited.count(cur)) {
            visited.insert(cur);
            cur = redirect[cur];
        }
        return cur;
    };

    // Rewrite jump targets
    for (auto& block : func.blocks) {
        for (auto& instr : block.instructions) {
            if (instr.opcode == IROpcode::JUMP ||
                instr.opcode == IROpcode::JUMP_IF ||
                instr.opcode == IROpcode::JUMP_IF_NOT) {
                std::string old_target = instr.dest.name;
                std::string new_target = resolve(old_target);
                if (new_target != old_target) {
                    instr.dest = Operand::label(new_target);
                    metrics_.jumps_chained++;
                    metrics_.instructions_modified++;
                    add_entry(func.name, block.label, 0,
                             "jump chain: " + old_target + " → " + new_target);
                }
            }
        }
    }
}

// ---------------------------------------------------------------
// get_optimization_report
// ---------------------------------------------------------------
std::string PeepholeOptimizer::get_optimization_report() const {
    std::ostringstream out;

    out << "=== Optimization Report ===\n";
    out << "Constants folded:          " << metrics_.constants_folded << "\n";
    out << "Algebraic simplifications: " << metrics_.algebraic_simplifications << "\n";
    out << "Strength reductions:       " << metrics_.strength_reductions << "\n";
    out << "Dead code eliminated:      " << metrics_.dead_code_eliminated << "\n";
    out << "Jumps chained:             " << metrics_.jumps_chained << "\n";
    out << "Copies propagated:         " << metrics_.copies_propagated << "\n";
    out << "CSEs eliminated:           " << metrics_.common_subexpressions_eliminated << "\n";
    out << "Total modified:            " << metrics_.instructions_modified << "\n";
    out << "Total removed:             " << metrics_.instructions_removed << "\n";

    if (!log_.empty()) {
        out << "\nDetails:\n";
        for (const auto& e : log_) {
            out << "  [" << e.function << "/" << e.block << "] " << e.description << "\n";
        }
    }

    return out.str();
}

// ---------------------------------------------------------------
// propagate_copies
// ---------------------------------------------------------------
void PeepholeOptimizer::propagate_copies(IRFunction& func) {
    for (auto& block : func.blocks) {
        std::map<std::string, Operand> copies;
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& instr = block.instructions[i];

            for (auto& src : instr.srcs) {
                if (src.is_temp() || src.kind == OperandKind::Variable) {
                    if (copies.find(src.name) != copies.end()) {
                        src = copies[src.name];
                        metrics_.copies_propagated++;
                        metrics_.instructions_modified++;
                    }
                }
            }

            if (!instr.dest.is_none()) {
                for (auto it = copies.begin(); it != copies.end(); ) {
                    if (it->second.name == instr.dest.name || it->first == instr.dest.name) {
                        it = copies.erase(it);
                    } else {
                        ++it;
                    }
                }

                if (instr.opcode == IROpcode::MOVE &&
                    (instr.srcs[0].is_literal() || instr.srcs[0].is_temp() || instr.srcs[0].kind == OperandKind::Variable)) {
                    copies[instr.dest.name] = instr.srcs[0];
                }
            }
        }
    }
}

// ---------------------------------------------------------------
// eliminate_common_subexpressions
// ---------------------------------------------------------------
void PeepholeOptimizer::eliminate_common_subexpressions(IRFunction& func) {
    for (auto& block : func.blocks) {
        std::map<std::string, Operand> expressions;
        for (size_t i = 0; i < block.instructions.size(); ++i) {
            auto& instr = block.instructions[i];

            if (instr.opcode >= IROpcode::ADD && instr.opcode <= IROpcode::CMP_GE) {
                std::string expr = opcode_to_string(instr.opcode) + " ";
                for (size_t j = 0; j < instr.srcs.size(); ++j) {
                    expr += operand_to_string(instr.srcs[j]) + (j == 0 ? ", " : "");
                }
                
                if (expressions.find(expr) != expressions.end()) {
                    Operand prev_dest = expressions[expr];
                    instr = IRInstruction::make_move(instr.dest, prev_dest);
                    metrics_.common_subexpressions_eliminated++;
                    metrics_.instructions_modified++;
                } else {
                    expressions[expr] = instr.dest;
                }
            }

            if (!instr.dest.is_none()) {
                for (auto it = expressions.begin(); it != expressions.end(); ) {
                    if (it->first.find(instr.dest.name) != std::string::npos) {
                        it = expressions.erase(it);
                    } else {
                        ++it;
                    }
                }
            }
        }
    }
}
