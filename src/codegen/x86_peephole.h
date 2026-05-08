#pragma once

#include <string>

// ---------------------------------------------------------------
// X86Peephole — оконная оптимизация на уровне x86-ассемблера
//
// Работает на текстовых строках сгенерированного NASM-кода.
// Применяет паттерны скользящего окна (размер 2-3 строки)
// для удаления/замены избыточных инструкций.
//
// Паттерны:
//   1. mov X, Y; mov Y, X     → удалить вторую (redundant store-back)
//   2. mov X, X                → удалить (identity mov)
//   3. mov reg, 0              → xor reg, reg (shorter encoding)
//   4. add X, 0 / sub X, 0    → удалить (identity arithmetic)
//   5. imul X, 1               → удалить (identity multiply)
//   6. jmp .Lnext (→ следующая метка) → удалить (fall-through)
// ---------------------------------------------------------------
class X86Peephole {
public:
    /// Оптимизировать NASM-текст, вернуть результат.
    std::string optimize(const std::string& asm_text);

    /// Отчёт об оптимизациях.
    std::string report() const;

    int removed()  const { return removed_; }
    int replaced() const { return replaced_; }

private:
    int removed_  = 0;   // удалённых инструкций
    int replaced_ = 0;   // заменённых инструкций
};
