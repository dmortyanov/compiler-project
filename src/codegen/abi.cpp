#include "codegen/abi.h"

namespace x86abi {

// System V AMD64 ABI §3.2.3 — порядок регистров для аргументов
const char* const ARG_REGS_64[MAX_REG_ARGS] = {
    "rdi", "rsi", "rdx", "rcx", "r8", "r9"
};
const char* const ARG_REGS_32[MAX_REG_ARGS] = {
    "edi", "esi", "edx", "ecx", "r8d", "r9d"
};

// Caller-saved: вызывающая сторона должна считать эти регистры
// затёртыми после call
const char* const CALLER_SAVED[] = {
    "rax", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"
};

// Callee-saved: вызываемая функция обязана восстановить при возврате
const char* const CALLEE_SAVED[] = {
    "rbx", "r12", "r13", "r14", "r15"
};

const char* arg_reg_64(int index) {
    if (index < 0 || index >= MAX_REG_ARGS) return nullptr;
    return ARG_REGS_64[index];
}

const char* arg_reg_32(int index) {
    if (index < 0 || index >= MAX_REG_ARGS) return nullptr;
    return ARG_REGS_32[index];
}

int align_to(int size, int alignment) {
    // (size + alignment - 1) & ~(alignment - 1)
    // Пример: align_to(12, 16) = 16; align_to(32, 16) = 32
    return (size + alignment - 1) & ~(alignment - 1);
}

}  // namespace x86abi
