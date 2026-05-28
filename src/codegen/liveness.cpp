#include "codegen/liveness.h"

#include <algorithm>
#include <unordered_map>
#include <set>

// ---------------------------------------------------------------
// compute_live_intervals
//
// Вычисляет интервалы жизни виртуальных регистров (Temp/Variable)
// с использованием классического итеративного алгоритма dataflow.
// ---------------------------------------------------------------
std::vector<LiveInterval> compute_live_intervals(const IRFunction& func) {
    struct Range {
        int first_def = -1;
        int last_use  = -1;
    };
    std::unordered_map<std::string, Range> ranges;

    // 1. Строим карту преемников (successors) на основе актуальных инструкций перехода.
    // Это необходимо, так как оптимизационные проходы (inlining, jump chaining)
    // могут менять переходы, не обновляя block.successors.
    std::unordered_map<std::string, std::vector<std::string>> successors;
    for (const auto& block : func.blocks) {
        auto& succs = successors[block.label];
        for (const auto& instr : block.instructions) {
            if (instr.opcode == IROpcode::JUMP ||
                instr.opcode == IROpcode::JUMP_IF ||
                instr.opcode == IROpcode::JUMP_IF_NOT) {
                std::string target = instr.dest.name;
                if (std::find(succs.begin(), succs.end(), target) == succs.end()) {
                    succs.push_back(target);
                }
            }
        }
    }

    // 2. Вычисляем множества Use и Def для каждого базового блока
    std::unordered_map<std::string, std::set<std::string>> use;
    std::unordered_map<std::string, std::set<std::string>> def;
    for (const auto& block : func.blocks) {
        std::set<std::string>& use_B = use[block.label];
        std::set<std::string>& def_B = def[block.label];
        for (const auto& instr : block.instructions) {
            for (const auto& src : instr.srcs) {
                if (src.kind == OperandKind::Temp || src.kind == OperandKind::Variable) {
                    if (def_B.find(src.name) == def_B.end()) {
                        use_B.insert(src.name);
                    }
                }
            }
            if (instr.dest.kind == OperandKind::Temp || instr.dest.kind == OperandKind::Variable) {
                if (use_B.find(instr.dest.name) == use_B.end()) {
                    def_B.insert(instr.dest.name);
                }
            }
        }
    }

    // 3. Итеративный dataflow-решатель для LiveIn и LiveOut
    std::unordered_map<std::string, std::set<std::string>> live_in;
    std::unordered_map<std::string, std::set<std::string>> live_out;
    bool changed = true;
    while (changed) {
        changed = false;
        for (auto it = func.blocks.rbegin(); it != func.blocks.rend(); ++it) {
            const auto& B = *it;

            std::set<std::string> out_B;
            for (const auto& succ_label : successors[B.label]) {
                const auto& in_succ = live_in[succ_label];
                out_B.insert(in_succ.begin(), in_succ.end());
            }

            std::set<std::string> in_B = use[B.label];
            for (const auto& v : out_B) {
                if (def[B.label].find(v) == def[B.label].end()) {
                    in_B.insert(v);
                }
            }

            if (in_B != live_in[B.label] || out_B != live_out[B.label]) {
                live_in[B.label] = in_B;
                live_out[B.label] = out_B;
                changed = true;
            }
        }
    }

    // 4. Вычисляем границы блоков и точки программы
    std::unordered_map<std::string, int> start_idx;
    std::unordered_map<std::string, int> end_idx;
    int point = 1;

    // Вспомогательная функция расширения диапазона
    auto update_range = [&](const std::string& name, int p) {
        auto it = ranges.find(name);
        if (it != ranges.end()) {
            if (it->second.first_def == -1 || p < it->second.first_def) {
                it->second.first_def = p;
            }
            if (it->second.last_use == -1 || p > it->second.last_use) {
                it->second.last_use = p;
            }
        } else {
            ranges[name] = {p, p};
        }
    };

    // Параметры функции изначально определены в точке 0
    for (const auto& param : func.params) {
        ranges[param.first] = {0, 0};
    }

    // Проход по инструкциям для фиксации локальных появлений
    for (const auto& block : func.blocks) {
        start_idx[block.label] = point;
        for (const auto& instr : block.instructions) {
            for (const auto& src : instr.srcs) {
                if (src.kind == OperandKind::Temp || src.kind == OperandKind::Variable) {
                    update_range(src.name, point);
                }
            }
            if (instr.dest.kind == OperandKind::Temp || instr.dest.kind == OperandKind::Variable) {
                update_range(instr.dest.name, point);
            }
            point++;
        }
        end_idx[block.label] = point - 1;
    }

    // Расширяем диапазоны с учетом LiveIn и LiveOut базовых блоков
    for (const auto& block : func.blocks) {
        int start = start_idx[block.label];
        int end = end_idx[block.label];

        for (const auto& v : live_in[block.label]) {
            update_range(v, start);
        }
        for (const auto& v : live_out[block.label]) {
            update_range(v, end);
        }
    }

    // 5. Формируем итоговый список интервалов
    std::vector<LiveInterval> intervals;
    intervals.reserve(ranges.size());
    for (const auto& [name, range] : ranges) {
        LiveInterval li;
        li.name  = name;
        li.start = range.first_def;
        li.end   = range.last_use;
        intervals.push_back(li);
    }

    std::sort(intervals.begin(), intervals.end());
    return intervals;
}
