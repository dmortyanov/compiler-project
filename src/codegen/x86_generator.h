#pragma once

#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ir/basic_block.h"
#include "codegen/stack_frame.h"
#include "codegen/register_allocator.h"
#include "codegen/x86_peephole.h"

// ---------------------------------------------------------------
// X86Generator — транслирует IRProgram в NASM x86-64 ассемблер
//
// Стратегия: stack-based codegen
//   - Каждый Temp/параметр → слот [rbp-N]
//   - eax/ecx — scratch-регистры для вычислений
//   - PHI-узлы → move-инструкции в конце предшественника
//   - Пролог/эпилог по System V AMD64 ABI
// ---------------------------------------------------------------
class X86Generator {
public:
    /// Сгенерировать полный NASM-файл для IR-программы.
    std::string generate(const IRProgram& program);

    /// Получить статистику кодогенерации.
    std::string statistics() const;

    /// Установить стратегию распределения регистров.
    void set_regalloc_strategy(RegAllocStrategy s) { regalloc_.set_strategy(s); }

    /// Включить/выключить x86 peephole оптимизацию.
    void set_peephole(bool enable) { peephole_enabled_ = enable; }

private:
    std::ostringstream out_;          // итоговый выходной буфер
    StackFrame frame_;
    RegisterAllocator regalloc_;
    bool peephole_enabled_ = false;
    X86Peephole peephole_;

    // Строковые литералы: label → value
    std::vector<std::pair<std::string, std::string>> string_literals_;
    int string_counter_ = 0;

    // Для PHI-разрешения:
    //   phi_moves_[dest_block][pred_block] = [{dest_name, source_operand}, ...]
    struct PhiMove {
        std::string dest_name;
        Operand source;
    };
    using PhiMoveMap = std::unordered_map<
        std::string, // dest_block
        std::unordered_map<
            std::string, // pred_block
            std::vector<PhiMove>>>;
    PhiMoveMap phi_moves_;

    // Буфер PARAM-операндов перед CALL
    std::vector<Operand> pending_params_;

    // Имя текущей функции (для контекста ошибок)
    std::string cur_func_name_;

    // Счётчик вспомогательных меток (для условных переходов с PHI)
    int aux_label_counter_ = 0;

    // Множество внешних символов, на которые есть ссылки
    std::set<std::string> extern_symbols_;

    // Множество функций, определённых в программе
    std::set<std::string> defined_functions_;

    // Известные runtime-функции
    static const std::set<std::string>& runtime_functions();

    // ---- генерация функции ----
    void gen_function(const IRFunction& func);
    void gen_prologue(const IRFunction& func);
    void gen_block(const BasicBlock& block, const IRFunction& func);

    // ---- генерация инструкций ----
    void gen_instruction(const IRInstruction& instr);
    void gen_binary(const IRInstruction& instr);
    void gen_unary(const IRInstruction& instr);
    void gen_comparison(const IRInstruction& instr);
    void gen_move(const IRInstruction& instr);
    void gen_return(const IRInstruction& instr);
    void gen_param(const IRInstruction& instr);
    void gen_call(const IRInstruction& instr);

    // ---- терминатор блока ----
    void gen_terminator(const BasicBlock& block);
    void gen_cond_branch(const std::string& cur_block_label,
                         const Operand& cond,
                         const std::string& true_target,
                         const std::string& false_target);

    // ---- PHI-разрешение ----
    void build_phi_map(const IRFunction& func);
    void emit_phi_moves(const std::string& from_block,
                        const std::string& to_block);
    bool has_phi_moves(const std::string& from_block,
                       const std::string& to_block) const;

    // ---- операнды ----
    void load_operand(const Operand& op, const char* reg32, const char* reg64);
    void store_to_dest(const Operand& dest, const char* reg32);

    // ---- вспомогательные ----
    void emit(const std::string& line);
    void emit_blank();
    std::string new_aux_label(const std::string& hint);
    std::string intern_string(const std::string& value);
};
