#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "ir/basic_block.h"

// ---------------------------------------------------------------
// StackSlot — один слот на стеке функции
// ---------------------------------------------------------------
struct StackSlot {
    int    offset;   // смещение от rbp (отрицательное для локалов)
    int    size;     // размер в байтах (4 для int)
    std::string name;   // отладочное имя (имя переменной или t0, t1, ...)
};

// ---------------------------------------------------------------
// StackFrame — управление стековым фреймом функции
//
// Все IR-temporary и параметры получают фиксированный слот
// вида [rbp - N].  Размер фрейма выравнивается до 16 байт
// (ABI-требование: стек должен быть выровнен по 16 перед call).
//
// Последовательность build():
//   1) Выделить слоты для формальных параметров
//   2) Просканировать все инструкции, выделить слоты для каждого
//      уникального Temp-операнда
//   3) Выровнять общий размер до 16
// ---------------------------------------------------------------
class StackFrame {
public:
    /// Построить раскладку фрейма по IR-функции.
    void build(const IRFunction& func);

    /// Получить NASM-ссылку на слот: "dword [rbp-8]"
    std::string slot_ref(const std::string& name) const;

    /// Есть ли слот с таким именем?
    bool has_slot(const std::string& name) const;

    /// Общий размер фрейма (уже выровнен до 16).
    int frame_size() const { return frame_size_; }

    /// Количество параметров функции.
    int param_count() const { return param_count_; }

    /// Имена параметров (в порядке объявления).
    const std::vector<std::string>& param_names() const { return param_names_; }

private:
    std::unordered_map<std::string, StackSlot> slots_;
    int frame_size_  = 0;
    int next_offset_ = 0;    // текущий конец занятого пространства
    int param_count_ = 0;
    std::vector<std::string> param_names_;

    /// Выделить новый слот. Возвращает смещение от rbp.
    int alloc_slot(const std::string& name, int size = 4);
};
