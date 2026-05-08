#pragma once

// ---------------------------------------------------------------
// System V AMD64 ABI — определения для x86-64 Linux
//
// Ссылки:
//   Intel® 64 and IA-32 Architectures SDM, Vol. 2
//   System V ABI AMD64 §3.2.3 — Parameter Passing
// ---------------------------------------------------------------

namespace x86abi {

// Регистры для передачи целочисленных аргументов (в порядке ABI)
//   1-й аргумент → rdi, 2-й → rsi, ..., 6-й → r9
//   Начиная с 7-го — через стек (справа налево)
constexpr int MAX_REG_ARGS = 6;

extern const char* const ARG_REGS_64[MAX_REG_ARGS];   // rdi, rsi, ...
extern const char* const ARG_REGS_32[MAX_REG_ARGS];   // edi, esi, ...

// Caller-saved (volatile) — вызываемая функция может затереть
extern const char* const CALLER_SAVED[];
constexpr int NUM_CALLER_SAVED = 9;   // rax rcx rdx rsi rdi r8-r11

// Callee-saved (nonvolatile) — вызываемая функция обязана сохранить
extern const char* const CALLEE_SAVED[];
constexpr int NUM_CALLEE_SAVED = 5;   // rbx r12-r15

// Возвращаемое значение
constexpr const char* RET_REG_64 = "rax";
constexpr const char* RET_REG_32 = "eax";

// Указатели стека/фрейма
constexpr const char* STACK_PTR = "rsp";
constexpr const char* FRAME_PTR = "rbp";

// Scratch-регистры, используемые кодогенератором
//   A — основной (результат), B — вспомогательный, D — для div/mod
constexpr const char* SCRATCH_A_64 = "rax";
constexpr const char* SCRATCH_A_32 = "eax";
constexpr const char* SCRATCH_A_8  = "al";
constexpr const char* SCRATCH_B_64 = "rcx";
constexpr const char* SCRATCH_B_32 = "ecx";
constexpr const char* SCRATCH_D_64 = "rdx";
constexpr const char* SCRATCH_D_32 = "edx";

// Выравнивание и размеры
constexpr int STACK_ALIGNMENT = 16;
constexpr int QWORD_SIZE = 8;
constexpr int DWORD_SIZE = 4;

// Номера Linux-syscall (x86-64)
constexpr int SYS_READ  = 0;
constexpr int SYS_WRITE = 1;
constexpr int SYS_EXIT  = 60;

// Получить имя регистра-аргумента по индексу (0..5)
// Возвращает nullptr при index >= MAX_REG_ARGS
const char* arg_reg_64(int index);
const char* arg_reg_32(int index);

// Выровнять size вверх до кратного alignment
int align_to(int size, int alignment);

}  // namespace x86abi
