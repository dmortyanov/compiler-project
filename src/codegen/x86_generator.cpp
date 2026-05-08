#include "codegen/x86_generator.h"
#include "codegen/abi.h"

#include <algorithm>
#include <cassert>
#include <sstream>

// ---------------------------------------------------------------
// Runtime-функции, предоставляемые runtime.asm
// ---------------------------------------------------------------
const std::set<std::string>& X86Generator::runtime_functions() {
    static const std::set<std::string> funcs = {
        "print_int", "read_int", "print_string", "exit_program"
    };
    return funcs;
}

// ---------------------------------------------------------------
// Вспомогательные методы вывода
// ---------------------------------------------------------------
void X86Generator::emit(const std::string& line) {
    out_ << line << "\n";
    regalloc_.total_instructions++;
}

void X86Generator::emit_blank() {
    out_ << "\n";
}

std::string X86Generator::new_aux_label(const std::string& hint) {
    return ".Laux_" + hint + "_" + std::to_string(aux_label_counter_++);
}

std::string X86Generator::intern_string(const std::string& value) {
    // Проверяем, не была ли строка уже интернирована
    for (const auto& pair : string_literals_) {
        if (pair.second == value) return pair.first;
    }
    std::string label = ".Lstr_" + std::to_string(string_counter_++);
    string_literals_.push_back({label, value});
    return label;
}

// ---------------------------------------------------------------
// generate — точка входа: генерирует весь NASM-файл
// ---------------------------------------------------------------
std::string X86Generator::generate(const IRProgram& program) {
    out_.str("");
    out_.clear();
    string_literals_.clear();
    string_counter_ = 0;
    aux_label_counter_ = 0;
    extern_symbols_.clear();
    defined_functions_.clear();
    regalloc_.reset();

    // Собираем имена определённых функций
    for (const auto& func : program.functions) {
        defined_functions_.insert(func.name);
    }

    // ---- Заголовок ----
    emit("; ============================================================");
    emit("; MiniCompiler — x86-64 NASM output");
    emit("; Target: Linux x86-64, System V AMD64 ABI");
    emit("; ============================================================");
    emit_blank();

    // ---- Секция .text ----
    emit("section .text");
    emit_blank();

    // Все пользовательские функции — global (для линковки)
    for (const auto& func : program.functions) {
        emit("global " + func.name);
    }
    emit_blank();

    // Генерируем код каждой функции
    for (const auto& func : program.functions) {
        gen_function(func);
    }

    // ---- Секция .data / .rodata (строковые литералы) ----
    if (!string_literals_.empty()) {
        emit_blank();
        emit("section .rodata");
        for (const auto& [label, value] : string_literals_) {
            // NASM: label db "text", 0
            emit(label + ":");
            emit("    db `" + value + "`, 0");
        }
    }

    // ---- extern-объявления (вставляем в начало) ----
    std::ostringstream result;

    // Собираем extern для нерезолвленных символов
    for (const auto& sym : extern_symbols_) {
        if (defined_functions_.find(sym) == defined_functions_.end()) {
            result << "extern " << sym << "\n";
        }
    }
    if (!extern_symbols_.empty()) {
        result << "\n";
    }

    result << out_.str();
    std::string final_asm = result.str();

    // x86 peephole optimization (post-pass)
    if (peephole_enabled_) {
        final_asm = peephole_.optimize(final_asm);
    }

    return final_asm;
}

std::string X86Generator::statistics() const {
    std::string s = regalloc_.stats_report();
    if (peephole_enabled_) {
        s += peephole_.report();
    }
    return s;
}

// ---------------------------------------------------------------
// gen_function — генерация одной функции
//
// Структура:
//   func_name:
//       push rbp
//       mov rbp, rsp
//       sub rsp, N
//       ; сохранение параметров из регистров на стек
//   .entry:
//       ...инструкции...
//   .L_then_0:
//       ...
// ---------------------------------------------------------------
void X86Generator::gen_function(const IRFunction& func) {
    cur_func_name_ = func.name;
    pending_params_.clear();

    // Построить стековый фрейм
    frame_.build(func);

    // Запустить аллокацию регистров (LSRA или noop для StackOnly)
    regalloc_.allocate(func, frame_);

    // Построить карту PHI-разрешений
    build_phi_map(func);

    // Метка функции
    emit("; ---- function " + func.name + " ----");
    emit(func.name + ":");

    // Пролог
    gen_prologue(func);

    // Генерация каждого базового блока
    for (size_t i = 0; i < func.blocks.size(); ++i) {
        gen_block(func.blocks[i], func);
    }

    emit_blank();

    // Очистка
    phi_moves_.clear();
}

// ---------------------------------------------------------------
// gen_prologue — пролог функции + сохранение параметров
//
// Пример для add(int a, int b):
//   push rbp
//   mov rbp, rsp
//   sub rsp, 16          ; (2 params + N temps) aligned to 16
//   mov dword [rbp-4], edi   ; сохранить param a
//   mov dword [rbp-8], esi   ; сохранить param b
// ---------------------------------------------------------------
void X86Generator::gen_prologue(const IRFunction& /* func */) {
    emit("    push rbp");
    emit("    mov rbp, rsp");

    // Сохраняем callee-saved регистры, используемые LSRA
    const auto& callee_saved = regalloc_.used_callee_saved_64();
    for (const auto& reg : callee_saved) {
        emit("    push " + reg + "    ; save callee-saved");
    }

    // Вычисляем полный размер фрейма:
    //   frame_.frame_size() для локалов + поправка на callee-saved pushes
    int total_frame = frame_.frame_size();
    // Callee-saved pushes сдвигают rsp, но мы уже выровняли frame_size.
    // Нужно обеспечить 16-byte alignment: push rbp (8) + N*push (8*N) + sub rsp, X
    // Total below return address = 8(rbp) + 8*N(callee) + X(frame)
    // Должно быть кратно 16.
    int pushes = 1 + static_cast<int>(callee_saved.size()); // rbp + callee-saved
    int push_bytes = pushes * 8;
    // Return address уже на стеке (8 bytes), итого перед sub:
    // stack = return_addr(8) + pushes*8 = 8 + push_bytes
    // После sub rsp, X: stack = 8 + push_bytes + X
    // Нужно: (8 + push_bytes + X) % 16 == 0
    // => X = align_to(frame_size, 16) с учётом push_bytes
    if (total_frame > 0) {
        int current_offset = 8 + push_bytes; // return addr + pushes
        int needed = total_frame;
        // Если (current_offset + needed) не кратно 16, добавляем 8
        if ((current_offset + needed) % 16 != 0) {
            needed += 8;
        }
        emit("    sub rsp, " + std::to_string(needed));
    }

    // Сохраняем параметры из ABI-регистров в стековые слоты.
    // System V AMD64: первые 6 целочисленных → rdi, rsi, rdx, rcx, r8, r9
    const auto& pnames = frame_.param_names();
    for (int i = 0; i < static_cast<int>(pnames.size()) && i < x86abi::MAX_REG_ARGS; ++i) {
        // Если параметр назначен в регистр LSRA, кладём туда напрямую
        auto alloc = regalloc_.get_allocation(pnames[i]);
        if (alloc.in_register) {
            emit("    mov " + alloc.phys_reg + ", " + x86abi::ARG_REGS_32[i]
                 + "    ; param " + pnames[i] + " -> " + alloc.phys_reg);
        } else {
            emit("    mov " + frame_.slot_ref(pnames[i]) + ", " + x86abi::ARG_REGS_32[i]
                 + "    ; param " + pnames[i]);
        }
    }
}

// ---------------------------------------------------------------
// gen_block — генерация одного базового блока
// ---------------------------------------------------------------
void X86Generator::gen_block(const BasicBlock& block, const IRFunction& /* func */) {
    // Метка блока (NASM local label)
    emit("." + block.label + ":");

    // Находим индекс первого терминатора
    size_t term_start = block.instructions.size();
    for (size_t i = 0; i < block.instructions.size(); ++i) {
        if (is_terminator(block.instructions[i].opcode)) {
            term_start = i;
            break;
        }
    }

    // Генерируем не-терминаторные инструкции
    for (size_t i = 0; i < term_start; ++i) {
        const auto& instr = block.instructions[i];
        if (instr.opcode == IROpcode::LABEL) continue;   // метки уже обработаны
        if (instr.opcode == IROpcode::PHI)   continue;   // PHI разрешаются в предшественниках

        // Комментарий с номером строки исходника
        if (instr.source_line > 0) {
            std::string cmt = "    ; line " + std::to_string(instr.source_line);
            if (!instr.comment.empty()) cmt += ": " + instr.comment;
            emit(cmt);
        }

        gen_instruction(instr);
    }

    // Генерируем терминатор (JUMP / JUMP_IF / RETURN)
    if (term_start < block.instructions.size()) {
        // Сбрасываем pending_params перед терминатором
        // (PARAM/CALL всегда до терминатора)
        gen_terminator(block);
    }
}

// ---------------------------------------------------------------
// gen_instruction — диспетчер по opcode
// ---------------------------------------------------------------
void X86Generator::gen_instruction(const IRInstruction& instr) {
    switch (instr.opcode) {
        case IROpcode::ADD: case IROpcode::SUB:
        case IROpcode::MUL: case IROpcode::DIV: case IROpcode::MOD:
        case IROpcode::AND: case IROpcode::OR:  case IROpcode::XOR:
            gen_binary(instr);
            break;

        case IROpcode::NEG: case IROpcode::NOT:
            gen_unary(instr);
            break;

        case IROpcode::CMP_EQ: case IROpcode::CMP_NE:
        case IROpcode::CMP_LT: case IROpcode::CMP_LE:
        case IROpcode::CMP_GT: case IROpcode::CMP_GE:
            gen_comparison(instr);
            break;

        case IROpcode::MOVE:
            gen_move(instr);
            break;

        case IROpcode::PARAM:
            gen_param(instr);
            break;

        case IROpcode::CALL:
            gen_call(instr);
            break;

        case IROpcode::RETURN:
            gen_return(instr);
            break;

        // Инструкции, обрабатываемые в других местах или не нужные
        case IROpcode::JUMP:
        case IROpcode::JUMP_IF:
        case IROpcode::JUMP_IF_NOT:
        case IROpcode::LABEL:
        case IROpcode::PHI:
        case IROpcode::NOP:
            break;

        // LOAD/STORE/ALLOCA — не генерируются текущим IR, заглушки
        case IROpcode::LOAD:
        case IROpcode::STORE:
        case IROpcode::ALLOCA:
            emit("    ; TODO: " + opcode_to_string(instr.opcode));
            break;
    }
}

// ===============================================================
//  Загрузка / сохранение операндов
// ===============================================================

// ---------------------------------------------------------------
// load_operand — загрузить значение операнда в указанный регистр
//
// Temp / Variable → mov reg32, dword [rbp-N]
// IntLiteral      → mov reg32, imm
// BoolLiteral     → mov reg32, 0/1
// StringLiteral   → lea reg64, [rel .Lstr_N]
// ---------------------------------------------------------------
void X86Generator::load_operand(const Operand& op,
                                const char* reg32,
                                const char* reg64) {
    switch (op.kind) {
        case OperandKind::Temp: {
            // LSRA: проверяем, есть ли temp в регистре
            auto alloc = regalloc_.get_allocation(op.name);
            if (alloc.in_register) {
                // Temp уже в физическом регистре
                if (alloc.phys_reg != std::string(reg32)) {
                    emit("    mov " + std::string(reg32) + ", " + alloc.phys_reg);
                }
                // Если совпадают — mov не нужен
            } else {
                emit("    mov " + std::string(reg32) + ", " + frame_.slot_ref(op.name));
                regalloc_.loads++;
            }
            break;
        }

        case OperandKind::Variable: {
            auto alloc = regalloc_.get_allocation(op.name);
            if (alloc.in_register) {
                if (alloc.phys_reg != std::string(reg32)) {
                    emit("    mov " + std::string(reg32) + ", " + alloc.phys_reg);
                }
            } else if (frame_.has_slot(op.name)) {
                emit("    mov " + std::string(reg32) + ", " + frame_.slot_ref(op.name));
                regalloc_.loads++;
            } else {
                emit("    ; WARNING: unknown variable " + op.name);
                emit("    xor " + std::string(reg32) + ", " + std::string(reg32));
            }
            break;
        }

        case OperandKind::IntLiteral:
            if (op.int_val == 0) {
                emit("    xor " + std::string(reg32) + ", " + std::string(reg32));
            } else {
                emit("    mov " + std::string(reg32) + ", " + std::to_string(op.int_val));
            }
            break;

        case OperandKind::BoolLiteral:
            if (op.int_val == 0) {
                emit("    xor " + std::string(reg32) + ", " + std::string(reg32));
            } else {
                emit("    mov " + std::string(reg32) + ", 1");
            }
            break;

        case OperandKind::FloatLiteral:
            emit("    ; TODO: float operand (SSE)");
            emit("    xor " + std::string(reg32) + ", " + std::string(reg32));
            break;

        case OperandKind::StringLiteral: {
            std::string label = intern_string(op.name);
            emit("    lea " + std::string(reg64) + ", [rel " + label + "]");
            break;
        }

        case OperandKind::Label:
        case OperandKind::None:
            break;
    }
}

// ---------------------------------------------------------------
// store_to_dest — сохранить значение из регистра в слот dest
// ---------------------------------------------------------------
void X86Generator::store_to_dest(const Operand& dest, const char* reg32) {
    if (dest.is_temp() || dest.kind == OperandKind::Variable) {
        auto alloc = regalloc_.get_allocation(dest.name);
        if (alloc.in_register) {
            // Записываем в физический регистр
            if (alloc.phys_reg != std::string(reg32)) {
                emit("    mov " + alloc.phys_reg + ", " + std::string(reg32));
            }
            // Если совпадают — mov не нужен
        } else if (frame_.has_slot(dest.name)) {
            emit("    mov " + frame_.slot_ref(dest.name) + ", " + std::string(reg32));
            regalloc_.stores++;
        }
    }
}

// ===============================================================
//  Генерация инструкций
// ===============================================================

// ---------------------------------------------------------------
// gen_binary — бинарные арифметические / логические операции
//
// Паттерн:
//   mov eax, <src1>
//   OP  eax, <src2>     (или специальная последовательность для div)
//   mov <dest>, eax
//
// Для DIV/MOD:
//   mov eax, <src1>
//   cdq                  ; sign-extend eax → edx:eax
//   mov ecx, <src2>
//   idiv ecx             ; eax = частное, edx = остаток
// ---------------------------------------------------------------
void X86Generator::gen_binary(const IRInstruction& instr) {
    load_operand(instr.srcs[0], "eax", "rax");

    switch (instr.opcode) {
        case IROpcode::ADD:
            load_operand(instr.srcs[1], "ecx", "rcx");
            emit("    add eax, ecx");
            break;

        case IROpcode::SUB:
            load_operand(instr.srcs[1], "ecx", "rcx");
            emit("    sub eax, ecx");
            break;

        case IROpcode::MUL:
            load_operand(instr.srcs[1], "ecx", "rcx");
            emit("    imul eax, ecx");
            break;

        case IROpcode::DIV:
            // cdq ПЕРЕД загрузкой src2 в ecx — не затирает eax/edx
            emit("    cdq");
            load_operand(instr.srcs[1], "ecx", "rcx");
            emit("    idiv ecx");
            // Результат (частное) уже в eax
            break;

        case IROpcode::MOD:
            emit("    cdq");
            load_operand(instr.srcs[1], "ecx", "rcx");
            emit("    idiv ecx");
            // Остаток в edx → переносим в eax
            emit("    mov eax, edx");
            break;

        case IROpcode::AND:
            load_operand(instr.srcs[1], "ecx", "rcx");
            emit("    and eax, ecx");
            break;

        case IROpcode::OR:
            load_operand(instr.srcs[1], "ecx", "rcx");
            emit("    or eax, ecx");
            break;

        case IROpcode::XOR:
            load_operand(instr.srcs[1], "ecx", "rcx");
            emit("    xor eax, ecx");
            break;

        default:
            break;
    }

    store_to_dest(instr.dest, "eax");
}

// ---------------------------------------------------------------
// gen_unary — NEG / NOT
//
// NEG: арифметическое отрицание (neg eax)
// NOT: логическое отрицание  (test + sete + movzx)
//      0 → 1, nonzero → 0
// ---------------------------------------------------------------
void X86Generator::gen_unary(const IRInstruction& instr) {
    load_operand(instr.srcs[0], "eax", "rax");

    switch (instr.opcode) {
        case IROpcode::NEG:
            emit("    neg eax");
            break;

        case IROpcode::NOT:
            // Логическое NOT: результат 0 или 1
            emit("    test eax, eax");
            emit("    sete al");
            emit("    movzx eax, al");
            break;

        default:
            break;
    }

    store_to_dest(instr.dest, "eax");
}

// ---------------------------------------------------------------
// gen_comparison — CMP_EQ / CMP_NE / CMP_LT / CMP_LE / CMP_GT / CMP_GE
//
//   mov eax, <src1>
//   cmp eax, <src2>
//   setCC al
//   movzx eax, al        ; zero-extend byte → dword
//   mov <dest>, eax
// ---------------------------------------------------------------
void X86Generator::gen_comparison(const IRInstruction& instr) {
    load_operand(instr.srcs[0], "eax", "rax");
    load_operand(instr.srcs[1], "ecx", "rcx");
    emit("    cmp eax, ecx");

    const char* setcc = "sete";
    switch (instr.opcode) {
        case IROpcode::CMP_EQ: setcc = "sete";  break;
        case IROpcode::CMP_NE: setcc = "setne"; break;
        case IROpcode::CMP_LT: setcc = "setl";  break;
        case IROpcode::CMP_LE: setcc = "setle"; break;
        case IROpcode::CMP_GT: setcc = "setg";  break;
        case IROpcode::CMP_GE: setcc = "setge"; break;
        default: break;
    }

    emit(std::string("    ") + setcc + " al");
    emit("    movzx eax, al");
    store_to_dest(instr.dest, "eax");
}

// ---------------------------------------------------------------
// gen_move — MOVE dest, src
// ---------------------------------------------------------------
void X86Generator::gen_move(const IRInstruction& instr) {
    load_operand(instr.srcs[0], "eax", "rax");
    store_to_dest(instr.dest, "eax");
}

// ---------------------------------------------------------------
// gen_return — RETURN [value]
//
//   mov eax, <value>     ; (только если есть значение)
//   leave                ; mov rsp, rbp; pop rbp
//   ret
// ---------------------------------------------------------------
void X86Generator::gen_return(const IRInstruction& instr) {
    if (!instr.srcs.empty()) {
        load_operand(instr.srcs[0], "eax", "rax");
    }
    // Восстанавливаем callee-saved регистры перед выходом
    const auto& callee_saved = regalloc_.used_callee_saved_64();
    if (!callee_saved.empty()) {
        // Восстанавливаем rsp до позиции callee-saved pushes
        emit("    mov rsp, rbp");
        // pop callee-saved в обратном порядке
        // Но callee-saved были push сразу после push rbp,
        // поэтому они находятся по [rbp-8], [rbp-16], ...
        // Проще: mov rsp, rbp; sub rsp, N_callee*8; pop...
        // Ещё проще: используем leave + восстанавливаем до leave
        // На самом деле, надо pop callee-saved до leave.
        // Правильная последовательность:
        //   lea rsp, [rbp - N_callee*8]  ; указатель на первый push
        //   pop r15; pop r14; ...; pop rbx
        //   leave; ret
        int n = static_cast<int>(callee_saved.size());
        emit("    lea rsp, [rbp-" + std::to_string(n * 8) + "]");
        for (int i = n - 1; i >= 0; --i) {
            emit("    pop " + callee_saved[i]);
        }
    }
    emit("    leave");
    emit("    ret");
}

// ---------------------------------------------------------------
// gen_param — PARAM index, value
//
// Буферизуем операнд для последующего CALL.
// PARAM 0, t2  →  pending_params_[0] = t2
// ---------------------------------------------------------------
void X86Generator::gen_param(const IRInstruction& instr) {
    int index = instr.dest.int_val;
    if (index >= static_cast<int>(pending_params_.size())) {
        pending_params_.resize(index + 1);
    }
    pending_params_[index] = instr.srcs[0];
}

// ---------------------------------------------------------------
// gen_call — dest = CALL func, arg_count
//
// Последовательность:
//   1) Загрузить аргументы в ABI-регистры (edi, esi, edx, ecx, r8d, r9d)
//   2) call func
//   3) Сохранить eax в слот dest (если dest не None)
//
// Примечание: аргументы загружаются из стековых слотов, поэтому
// порядок загрузки в регистры не вызывает конфликтов (mov edi, [rbp-N]
// не затирает esi, и наоборот).
// ---------------------------------------------------------------
void X86Generator::gen_call(const IRInstruction& instr) {
    std::string func_name = instr.srcs[0].name;   // имя функции
    int arg_count = instr.srcs[1].int_val;

    // Отмечаем extern, если функция не определена в программе
    if (defined_functions_.find(func_name) == defined_functions_.end()) {
        extern_symbols_.insert(func_name);
    }

    // Загружаем аргументы 0..5 в регистры ABI
    for (int i = 0; i < arg_count && i < x86abi::MAX_REG_ARGS; ++i) {
        const char* r32 = x86abi::arg_reg_32(i);
        const char* r64 = x86abi::arg_reg_64(i);
        if (r32 && r64) {
            load_operand(pending_params_[i], r32, r64);
        }
    }

    // Аргументы 6+ — через стек (push справа налево)
    if (arg_count > x86abi::MAX_REG_ARGS) {
        int stack_args = arg_count - x86abi::MAX_REG_ARGS;
        // Выравнивание: если нечётное число stack-аргументов,
        // нужен дополнительный sub rsp, 8 чтобы стек остался aligned
        bool need_pad = (stack_args % 2 != 0);
        if (need_pad) {
            emit("    sub rsp, 8");
        }
        for (int i = arg_count - 1; i >= x86abi::MAX_REG_ARGS; --i) {
            load_operand(pending_params_[i], "eax", "rax");
            emit("    push rax");
        }
    }

    emit("    call " + func_name);

    // Очистка стека после stack-аргументов
    if (arg_count > x86abi::MAX_REG_ARGS) {
        int stack_args = arg_count - x86abi::MAX_REG_ARGS;
        bool need_pad = (stack_args % 2 != 0);
        int cleanup = stack_args * x86abi::QWORD_SIZE;
        if (need_pad) cleanup += x86abi::QWORD_SIZE;
        emit("    add rsp, " + std::to_string(cleanup));
    }

    // Результат в eax → dest
    if (!instr.dest.is_none()) {
        store_to_dest(instr.dest, "eax");
    }

    pending_params_.clear();
}

// ===============================================================
//  Терминаторы блоков
// ===============================================================

// ---------------------------------------------------------------
// gen_terminator — обрабатывает терминатор(ы) блока
//
// Паттерны IR-терминаторов:
//   1) RETURN [val]
//   2) JUMP target
//   3) JUMP_IF cond, true_target  +  JUMP false_target
//   4) JUMP_IF_NOT cond, true_target  +  JUMP false_target
// ---------------------------------------------------------------
void X86Generator::gen_terminator(const BasicBlock& block) {
    // Находим первый терминатор
    size_t ti = 0;
    for (ti = 0; ti < block.instructions.size(); ++ti) {
        if (is_terminator(block.instructions[ti].opcode)) break;
    }
    if (ti >= block.instructions.size()) return;

    const auto& first = block.instructions[ti];

    // --- RETURN ---
    if (first.opcode == IROpcode::RETURN) {
        gen_return(first);
        return;
    }

    // --- JUMP (безусловный) ---
    if (first.opcode == IROpcode::JUMP) {
        std::string target = first.dest.name;
        emit_phi_moves(block.label, target);
        emit("    jmp ." + target);
        return;
    }

    // --- JUMP_IF / JUMP_IF_NOT + JUMP ---
    if (first.opcode == IROpcode::JUMP_IF || first.opcode == IROpcode::JUMP_IF_NOT) {
        std::string true_target = first.dest.name;
        std::string false_target;

        // Следующий терминатор — JUMP (false path)
        if (ti + 1 < block.instructions.size() &&
            block.instructions[ti + 1].opcode == IROpcode::JUMP) {
            false_target = block.instructions[ti + 1].dest.name;
        }

        // Для JUMP_IF_NOT инвертируем логику
        if (first.opcode == IROpcode::JUMP_IF_NOT) {
            std::swap(true_target, false_target);
            // Теперь: true_target = куда прыгаем если cond != 0 (оригинальный false)
            //         false_target = куда прыгаем если cond == 0 (оригинальный true → JUMP_IF_NOT)
            // Поскольку JUMP_IF_NOT прыгает когда cond == 0, а мы инвертировали targets:
            // Нет, лучше не инвертировать — обработаем JUMP_IF_NOT напрямую
            std::swap(true_target, false_target);  // отменяем swap

            // JUMP_IF_NOT cond, target: прыгнуть если cond == 0
            load_operand(first.srcs[0], "eax", "rax");
            emit("    test eax, eax");

            bool true_phi = has_phi_moves(block.label, true_target);
            bool false_phi = has_phi_moves(block.label, false_target);

            if (!true_phi && !false_phi) {
                emit("    jz ." + true_target);
                emit("    jmp ." + false_target);
            } else if (true_phi && !false_phi) {
                // Нужен trampoline для true path
                emit("    jnz ." + false_target);
                emit_phi_moves(block.label, true_target);
                emit("    jmp ." + true_target);
            } else if (!true_phi && false_phi) {
                emit("    jz ." + true_target);
                emit_phi_moves(block.label, false_target);
                emit("    jmp ." + false_target);
            } else {
                std::string skip = new_aux_label("false");
                emit("    jnz " + skip);
                emit_phi_moves(block.label, true_target);
                emit("    jmp ." + true_target);
                emit(skip + ":");
                emit_phi_moves(block.label, false_target);
                emit("    jmp ." + false_target);
            }
            return;
        }

        // JUMP_IF cond, true_target: прыгнуть если cond != 0
        gen_cond_branch(block.label, first.srcs[0], true_target, false_target);
    }
}

// ---------------------------------------------------------------
// gen_cond_branch — условный переход JUMP_IF с PHI-разрешением
//
// Четыре случая в зависимости от наличия PHI-moves:
//
// 1) Нет PHI ни на одном пути:
//    test eax, eax
//    jnz .true_target
//    jmp .false_target
//
// 2) PHI только на true path (пример: || short-circuit):
//    test eax, eax
//    jz .false_target      ; если false — сразу туда
//    <phi moves для true>
//    jmp .true_target
//
// 3) PHI только на false path (пример: && short-circuit):
//    test eax, eax
//    jnz .true_target
//    <phi moves для false>
//    jmp .false_target
//
// 4) PHI на обоих путях:
//    test eax, eax
//    jz .Laux_false_N
//    <phi moves для true>
//    jmp .true_target
//    .Laux_false_N:
//    <phi moves для false>
//    jmp .false_target
// ---------------------------------------------------------------
void X86Generator::gen_cond_branch(const std::string& cur_block_label,
                                   const Operand& cond,
                                   const std::string& true_target,
                                   const std::string& false_target) {
    load_operand(cond, "eax", "rax");
    emit("    test eax, eax");

    bool true_phi  = has_phi_moves(cur_block_label, true_target);
    bool false_phi = has_phi_moves(cur_block_label, false_target);

    if (!true_phi && !false_phi) {
        // Случай 1: простой
        emit("    jnz ." + true_target);
        emit("    jmp ." + false_target);

    } else if (true_phi && !false_phi) {
        // Случай 2: phi на true. Если false — прыгаем мимо phi-кода
        emit("    jz ." + false_target);
        emit_phi_moves(cur_block_label, true_target);
        emit("    jmp ." + true_target);

    } else if (!true_phi && false_phi) {
        // Случай 3: phi на false
        emit("    jnz ." + true_target);
        emit_phi_moves(cur_block_label, false_target);
        emit("    jmp ." + false_target);

    } else {
        // Случай 4: phi на обоих путях — нужна доп. метка
        std::string false_label = new_aux_label("false");
        emit("    jz " + false_label);
        emit_phi_moves(cur_block_label, true_target);
        emit("    jmp ." + true_target);
        emit(false_label + ":");
        emit_phi_moves(cur_block_label, false_target);
        emit("    jmp ." + false_target);
    }
}

// ===============================================================
//  PHI-разрешение
// ===============================================================

// ---------------------------------------------------------------
// build_phi_map — строит карту PHI-moves для функции
//
// Для каждой PHI-инструкции:
//   dest = PHI (val1, label1), (val2, label2), ...
//
// Создаём запись:
//   phi_moves_[this_block][label1] += {dest.name, val1}
//   phi_moves_[this_block][label2] += {dest.name, val2}
// ---------------------------------------------------------------
void X86Generator::build_phi_map(const IRFunction& func) {
    phi_moves_.clear();

    for (const auto& block : func.blocks) {
        for (const auto& instr : block.instructions) {
            if (instr.opcode != IROpcode::PHI) continue;

            // srcs: [val0, label0, val1, label1, ...]
            for (size_t i = 0; i + 1 < instr.srcs.size(); i += 2) {
                const Operand& val  = instr.srcs[i];
                const Operand& pred = instr.srcs[i + 1];

                // Пропускаем None-операнды (неинициализированные PHI-входы)
                if (val.is_none() || pred.is_none()) continue;

                PhiMove pm;
                pm.dest_name = instr.dest.name;
                pm.source    = val;

                phi_moves_[block.label][pred.name].push_back(pm);
            }
        }
    }
}

// ---------------------------------------------------------------
// emit_phi_moves — генерирует move-инструкции для PHI-разрешения
//
// Вызывается перед jump из from_block в to_block.
// Для каждого PHI в to_block, у которого предшественник = from_block:
//   mov eax, <phi_source>
//   mov <phi_dest_slot>, eax
// ---------------------------------------------------------------
void X86Generator::emit_phi_moves(const std::string& from_block,
                                  const std::string& to_block) {
    auto it1 = phi_moves_.find(to_block);
    if (it1 == phi_moves_.end()) return;

    auto it2 = it1->second.find(from_block);
    if (it2 == it1->second.end()) return;

    const auto& moves = it2->second;
    if (moves.empty()) return;

    emit("    ; PHI resolution: " + from_block + " -> " + to_block);
    for (const auto& pm : moves) {
        load_operand(pm.source, "eax", "rax");
        // Записываем в слот PHI-destination
        if (frame_.has_slot(pm.dest_name)) {
            emit("    mov " + frame_.slot_ref(pm.dest_name) + ", eax");
            regalloc_.stores++;
        }
    }
}

bool X86Generator::has_phi_moves(const std::string& from_block,
                                 const std::string& to_block) const {
    auto it1 = phi_moves_.find(to_block);
    if (it1 == phi_moves_.end()) return false;
    auto it2 = it1->second.find(from_block);
    if (it2 == it1->second.end()) return false;
    return !it2->second.empty();
}
