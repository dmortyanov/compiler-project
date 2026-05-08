#include "codegen/liveness.h"

#include <algorithm>
#include <unordered_map>

// ---------------------------------------------------------------
// compute_live_intervals
//
// Стратегия: линейная нумерация всех инструкций во всех блоках
// функции (в порядке обхода блоков). Для каждого Temp-операнда
// отслеживаем первую точку определения (def) и последнюю точку
// использования (use). Интервал жизни = [def, last_use].
//
// Особые случаи:
//   - PHI-операнды: их source-использования логически находятся
//     в конце предшественника, но для простоты мы расширяем
//     интервал до точки PHI-инструкции.
//   - Параметры функции: считаются определёнными в точке 0.
// ---------------------------------------------------------------
std::vector<LiveInterval> compute_live_intervals(const IRFunction& func) {
    // Карта: temp_name -> {first_def, last_use}
    struct Range {
        int first_def = -1;
        int last_use  = -1;
    };
    std::unordered_map<std::string, Range> ranges;

    // Параметры функции определены в точке 0
    for (const auto& param : func.params) {
        ranges[param.first] = {0, 0};
    }

    // Линейная нумерация инструкций
    int point = 1;  // 0 зарезервировано для параметров

    for (const auto& block : func.blocks) {
        for (const auto& instr : block.instructions) {
            // Обработка источников (uses)
            for (const auto& src : instr.srcs) {
                if (src.kind == OperandKind::Temp) {
                    auto it = ranges.find(src.name);
                    if (it != ranges.end()) {
                        it->second.last_use = point;
                    } else {
                        // Использование до определения (может быть из PHI)
                        ranges[src.name] = {point, point};
                    }
                }
            }

            // Обработка назначения (def)
            if (instr.dest.kind == OperandKind::Temp) {
                auto it = ranges.find(instr.dest.name);
                if (it != ranges.end()) {
                    // Уже видели — обновляем last_use если нужно
                    if (it->second.last_use < point) {
                        it->second.last_use = point;
                    }
                } else {
                    ranges[instr.dest.name] = {point, point};
                }
            }

            ++point;
        }
    }

    // Формируем вектор интервалов
    std::vector<LiveInterval> intervals;
    intervals.reserve(ranges.size());

    for (const auto& [name, range] : ranges) {
        LiveInterval li;
        li.name  = name;
        li.start = range.first_def;
        li.end   = range.last_use;
        intervals.push_back(li);
    }

    // Сортируем по start (требование LSRA)
    std::sort(intervals.begin(), intervals.end());

    return intervals;
}
