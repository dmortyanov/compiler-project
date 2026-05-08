#include "codegen/stack_frame.h"
#include "codegen/abi.h"

// ---------------------------------------------------------------
// alloc_slot — выделяет DWORD-слот на стеке
//
// Слоты растут вниз от rbp:
//   первый  → [rbp - 4]
//   второй  → [rbp - 8]
//   ...
// ---------------------------------------------------------------
int StackFrame::alloc_slot(const std::string& name, int size) {
    next_offset_ += size;
    int offset = -next_offset_;   // отрицательное смещение от rbp

    StackSlot slot;
    slot.offset = offset;
    slot.size   = size;
    slot.name   = name;
    slots_[name] = slot;

    return offset;
}

// ---------------------------------------------------------------
// build — сканирует IRFunction и назначает слоты
//
// Порядок:
//   1) Параметры (по порядку объявления): a, b, ... → [rbp-4], [rbp-8], ...
//   2) Все Temp-операнды, встреченные в инструкциях: t0, t1, ...
//   3) Variable-операнды, не являющиеся параметрами (fallback)
//   4) Выравнивание общего размера до 16 байт
// ---------------------------------------------------------------
void StackFrame::build(const IRFunction& func) {
    slots_.clear();
    next_offset_ = 0;
    param_names_.clear();

    // 1. Параметры
    for (const auto& param : func.params) {
        alloc_slot(param.first, x86abi::DWORD_SIZE);
        param_names_.push_back(param.first);
    }
    param_count_ = static_cast<int>(func.params.size());

    // 2–3. Сканируем все инструкции: ищем Temp и Variable-операнды
    for (const auto& block : func.blocks) {
        for (const auto& instr : block.instructions) {
            // Destination
            if (instr.dest.is_temp() && !has_slot(instr.dest.name)) {
                alloc_slot(instr.dest.name, x86abi::DWORD_SIZE);
            }
            // Sources
            for (const auto& src : instr.srcs) {
                if (src.is_temp() && !has_slot(src.name)) {
                    alloc_slot(src.name, x86abi::DWORD_SIZE);
                }
                if (src.kind == OperandKind::Variable && !has_slot(src.name)) {
                    alloc_slot(src.name, x86abi::DWORD_SIZE);
                }
            }
        }
    }

    // 4. Выравнивание до 16 байт
    //    Если слотов 0, размер фрейма = 0 (sub rsp не нужен)
    if (next_offset_ > 0) {
        frame_size_ = x86abi::align_to(next_offset_, x86abi::STACK_ALIGNMENT);
    } else {
        frame_size_ = 0;
    }
}

// ---------------------------------------------------------------
// slot_ref — NASM-ссылка на слот, например "dword [rbp-8]"
// ---------------------------------------------------------------
std::string StackFrame::slot_ref(const std::string& name) const {
    auto it = slots_.find(name);
    if (it == slots_.end()) {
        return "dword [UNKNOWN_SLOT_" + name + "]";
    }
    // offset отрицательный → to_string даст "-8", итого "dword [rbp-8]"
    return "dword [rbp" + std::to_string(it->second.offset) + "]";
}

bool StackFrame::has_slot(const std::string& name) const {
    return slots_.find(name) != slots_.end();
}
