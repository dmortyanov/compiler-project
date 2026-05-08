#pragma once

#include <string>
#include <vector>

#include "ir/basic_block.h"

// ---------------------------------------------------------------
// LiveInterval — интервал жизни одного виртуального регистра (temp)
//
// start — номер program point, в котором temp впервые определён
// end   — номер program point, в котором temp последний раз используется
// ---------------------------------------------------------------
struct LiveInterval {
    std::string name;       // имя temp-а (t0, t1, ...)
    int start = 0;          // первая точка определения
    int end   = 0;          // последняя точка использования

    // Сортировка по start (для LSRA)
    bool operator<(const LiveInterval& other) const {
        return start < other.start;
    }
};

// ---------------------------------------------------------------
// compute_live_intervals — вычисляет интервалы жизни для всех
// temp-операндов в функции.
//
// Каждой IR-инструкции присваивается линейный номер (program point).
// Для каждого temp определяется [первое определение, последнее
// использование]. Результат отсортирован по start.
// ---------------------------------------------------------------
std::vector<LiveInterval> compute_live_intervals(const IRFunction& func);
