; ============================================================
; MiniCompiler — x86-64 NASM output
; Target: Linux x86-64, System V AMD64 ABI
; ============================================================

section .text

global add
global main

; ---- function add ----
add:
    push rbp
    mov rbp, rsp
    push rbx    ; save callee-saved
    push r12    ; save callee-saved
    push r13    ; save callee-saved
    sub rsp, 40
    mov ebx, edi    ; param a -> ebx
    mov r12d, esi    ; param b -> r12d
.entry:
    mov eax, ebx
    mov ebx, eax
    mov eax, r12d
    mov r12d, eax
    ; line 3
    mov eax, ebx
    mov ecx, r12d
    add eax, ecx
    mov r13d, eax
    mov eax, r13d
    mov rsp, rbp
    lea rsp, [rbp-24]
    pop r13
    pop r12
    pop rbx
    leave
    ret

; ---- function main ----
main:
    push rbp
    mov rbp, rsp
    push rbx    ; save callee-saved
    push r12    ; save callee-saved
    sub rsp, 16
.entry:
    ; line 7
    ; line 7
    ; line 7
    mov edi, 2
    mov esi, 3
    call add
    mov ebx, eax
    ; line 7: int result
    mov eax, ebx
    mov r12d, eax
    mov eax, r12d
    mov rsp, rbp
    lea rsp, [rbp-16]
    pop r12
    pop rbx
    leave
    ret

