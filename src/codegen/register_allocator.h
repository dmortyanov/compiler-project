#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "ir/basic_block.h"
#include "codegen/stack_frame.h"
#include "codegen/liveness.h"

// ---------------------------------------------------------------
// RegAllocStrategy — выбор стратегии распределения регистров
// ---------------------------------------------------------------
enum class RegAllocStrategy {
    StackOnly,      // Все значения на стеке (как в Sprint 5)
    LinearScan      // LSRA — Полетто-Сарнак (1999)
};

// ---------------------------------------------------------------
// Allocation — результат назначения для одного virtual register
// ---------------------------------------------------------------
struct Allocation {
    bool in_register = false;   // true = в физическом регистре
    std::string phys_reg;       // имя регистра (e.g. "ebx", "r12d")
    std::string phys_reg_64;    // 64-битная версия (e.g. "rbx", "r12")
    // Если in_register == false, значение на стеке (через StackFrame)
};

// ---------------------------------------------------------------
// RegisterAllocator — распределитель регистров
//
// Поддерживает две стратегии:
//   1) StackOnly — все значения на стеке, eax/ecx = scratch
//   2) LinearScan — LSRA: долгоживущие temps назначаются в
//      callee-saved регистры (ebx, r12d-r15d), остальные спиллятся
//
// Пул регистров для LSRA (callee-saved по System V AMD64 ABI):
//   ebx (rbx), r12d (r12), r13d (r13), r14d (r14), r15d (r15)
//
// eax, ecx, edx остаются scratch для промежуточных вычислений.
// ---------------------------------------------------------------
class RegisterAllocator {
public:
    // Имена scratch-регистров (не меняются)
    static constexpr const char* A64 = "rax";
    static constexpr const char* A32 = "eax";
    static constexpr const char* A8  = "al";

    static constexpr const char* B64 = "rcx";
    static constexpr const char* B32 = "ecx";

    static constexpr const char* D64 = "rdx";
    static constexpr const char* D32 = "edx";

    // Стратегия
    void set_strategy(RegAllocStrategy s) { strategy_ = s; }
    RegAllocStrategy strategy() const { return strategy_; }

    // Запуск аллокации для функции (вызывается перед генерацией кода)
    void allocate(const IRFunction& func, StackFrame& frame);

    // Запрос: где живёт данный temp?
    // Возвращает Allocation (in_register + phys_reg или stack)
    Allocation get_allocation(const std::string& temp_name) const;

    // Список callee-saved регистров, которые реально были использованы
    // (нужны для push/pop в прологе/эпилоге)
    const std::vector<std::string>& used_callee_saved_64() const { return used_callee_saved_; }

    // Статистика
    int loads  = 0;
    int stores = 0;
    int total_instructions = 0;
    int reg_allocated = 0;   // сколько temps получили регистр
    int spilled = 0;         // сколько temps спиллились на стек

    void reset();
    std::string stats_report() const;

private:
    RegAllocStrategy strategy_ = RegAllocStrategy::StackOnly;

    // Результат LSRA: temp_name -> Allocation
    std::unordered_map<std::string, Allocation> allocations_;

    // Какие callee-saved регистры реально использованы (64-bit имена для push/pop)
    std::vector<std::string> used_callee_saved_;

    // Пул доступных регистров
    struct PhysReg {
        std::string name_32;  // "ebx", "r12d", ...
        std::string name_64;  // "rbx", "r12", ...
    };
    static const std::vector<PhysReg>& reg_pool();

    // Внутренний метод: запуск линейного сканирования
    void run_linear_scan(const IRFunction& func);
};
