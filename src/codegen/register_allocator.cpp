#include "codegen/register_allocator.h"
#include "codegen/liveness.h"

#include <algorithm>
#include <set>
#include <sstream>

// ---------------------------------------------------------------
// Пул callee-saved регистров для LSRA
// Эти регистры сохраняются вызываемой функцией (callee-saved),
// поэтому мы должны push/pop их в прологе/эпилоге если используем.
// ---------------------------------------------------------------
const std::vector<RegisterAllocator::PhysReg>& RegisterAllocator::reg_pool() {
    static const std::vector<PhysReg> pool = {
        {"ebx",  "rbx"},
        {"r12d", "r12"},
        {"r13d", "r13"},
        {"r14d", "r14"},
        {"r15d", "r15"}
    };
    return pool;
}

// ---------------------------------------------------------------
// reset — сброс состояния
// ---------------------------------------------------------------
void RegisterAllocator::reset() {
    loads  = 0;
    stores = 0;
    total_instructions = 0;
    reg_allocated = 0;
    spilled = 0;
    allocations_.clear();
    used_callee_saved_.clear();
}

// ---------------------------------------------------------------
// allocate — точка входа для аллокации
// ---------------------------------------------------------------
void RegisterAllocator::allocate(const IRFunction& func, StackFrame& /* frame */) {
    allocations_.clear();
    used_callee_saved_.clear();
    reg_allocated = 0;
    spilled = 0;

    if (strategy_ == RegAllocStrategy::LinearScan) {
        run_linear_scan(func);
    }
    // При StackOnly — allocations_ остаётся пустым,
    // get_allocation() вернёт {in_register=false} для всех temps
}

// ---------------------------------------------------------------
// run_linear_scan — алгоритм Полетто-Сарнака (Linear Scan, 1999)
//
// 1. Вычислить live intervals для всех temps
// 2. Отсортировать по start (уже сделано в compute_live_intervals)
// 3. Линейный проход:
//    - expire_old: убрать из active все интервалы, чей end < текущий start
//    - если есть свободный регистр → назначить
//    - если нет → spill: выбрать из active интервал с наибольшим end
//      * если его end > текущего end → спиллим его, назначаем текущему
//      * иначе → спиллим текущий
// ---------------------------------------------------------------
void RegisterAllocator::run_linear_scan(const IRFunction& func) {
    auto intervals = compute_live_intervals(func);

    if (intervals.empty()) return;

    const auto& pool = reg_pool();
    int num_regs = static_cast<int>(pool.size());

    // Множество свободных регистров (по индексу в pool)
    std::set<int> free_regs;
    for (int i = 0; i < num_regs; ++i) {
        free_regs.insert(i);
    }

    // Active list: интервалы, которые сейчас живы и занимают регистр
    // Каждый элемент: {interval_index, reg_index}
    struct ActiveEntry {
        int interval_idx;
        int reg_idx;
        int end_point;
    };

    // Сортируем active по end_point для быстрого expire
    std::vector<ActiveEntry> active;

    // Результат: interval_index -> reg_index (-1 = spilled)
    std::vector<int> assignment(intervals.size(), -1);

    // Множество использованных регистров (для callee-saved push/pop)
    std::set<int> used_regs;

    for (int i = 0; i < static_cast<int>(intervals.size()); ++i) {
        const auto& cur = intervals[i];

        // Expire old intervals
        auto it = active.begin();
        while (it != active.end()) {
            if (it->end_point < cur.start) {
                // Этот интервал больше не жив — освобождаем регистр
                free_regs.insert(it->reg_idx);
                it = active.erase(it);
            } else {
                ++it;
            }
        }

        if (!free_regs.empty()) {
            // Назначаем свободный регистр
            int reg_idx = *free_regs.begin();
            free_regs.erase(free_regs.begin());

            assignment[i] = reg_idx;
            used_regs.insert(reg_idx);
            reg_allocated++;

            ActiveEntry ae;
            ae.interval_idx = i;
            ae.reg_idx = reg_idx;
            ae.end_point = cur.end;
            active.push_back(ae);

            // Поддерживаем active отсортированным по end_point
            std::sort(active.begin(), active.end(),
                      [](const ActiveEntry& a, const ActiveEntry& b) {
                          return a.end_point < b.end_point;
                      });
        } else {
            // Все регистры заняты — нужен spill
            // Находим active с наибольшим end_point (последний в списке)
            if (!active.empty() && active.back().end_point > cur.end) {
                // Спиллим того, кто живёт дольше, назначаем его регистр текущему
                ActiveEntry victim = active.back();
                active.pop_back();

                // Отнимаем регистр у victim
                assignment[victim.interval_idx] = -1;  // spilled
                spilled++;
                reg_allocated--;  // корректируем

                // Назначаем текущему
                assignment[i] = victim.reg_idx;
                reg_allocated++;

                ActiveEntry ae;
                ae.interval_idx = i;
                ae.reg_idx = victim.reg_idx;
                ae.end_point = cur.end;
                active.push_back(ae);

                std::sort(active.begin(), active.end(),
                          [](const ActiveEntry& a, const ActiveEntry& b) {
                              return a.end_point < b.end_point;
                          });
            } else {
                // Текущий сам спиллится
                assignment[i] = -1;
                spilled++;
            }
        }
    }

    // Строим allocations_ map
    for (int i = 0; i < static_cast<int>(intervals.size()); ++i) {
        Allocation alloc;
        if (assignment[i] >= 0) {
            alloc.in_register = true;
            alloc.phys_reg = pool[assignment[i]].name_32;
            alloc.phys_reg_64 = pool[assignment[i]].name_64;
        } else {
            alloc.in_register = false;
        }
        allocations_[intervals[i].name] = alloc;
    }

    // Собираем список использованных callee-saved (64-bit)
    used_callee_saved_.clear();
    for (int idx : used_regs) {
        used_callee_saved_.push_back(pool[idx].name_64);
    }
}

// ---------------------------------------------------------------
// get_allocation — запрос результата для конкретного temp
// ---------------------------------------------------------------
Allocation RegisterAllocator::get_allocation(const std::string& temp_name) const {
    auto it = allocations_.find(temp_name);
    if (it != allocations_.end()) {
        return it->second;
    }
    // Не найден — значит на стеке (StackOnly или не был в intervals)
    Allocation a;
    a.in_register = false;
    return a;
}

// ---------------------------------------------------------------
// stats_report — отчёт о работе аллокатора
// ---------------------------------------------------------------
std::string RegisterAllocator::stats_report() const {
    std::ostringstream out;
    out << "=== Register Allocator Statistics ===\n";

    if (strategy_ == RegAllocStrategy::LinearScan) {
        out << "Strategy:        LSRA (Linear Scan)\n";
        out << "Reg allocated:   " << reg_allocated << "\n";
        out << "Spilled:         " << spilled << "\n";
        out << "Callee-saved:    " << used_callee_saved_.size() << " registers\n";
    } else {
        out << "Strategy:        stack-based (all values on stack)\n";
    }

    out << "Loads:           " << loads  << "\n";
    out << "Stores:          " << stores << "\n";
    out << "Instructions:    " << total_instructions << "\n";

    if (total_instructions > 0) {
        double ratio = static_cast<double>(loads + stores) / total_instructions * 100.0;
        out << "Memory ratio:    " << ratio << "%\n";
    }
    return out.str();
}
