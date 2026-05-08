; ============================================================
; MiniCompiler — x86-64 NASM output
; Target: Linux x86-64, System V AMD64 ABI
; ============================================================

section .text

global factorial
global main

; ---- function factorial ----
factorial:
    push rbp
    mov rbp, rsp
    push rbx    ; save callee-saved
    push r12    ; save callee-saved
    push r13    ; save callee-saved
    sub rsp, 40
    mov ebx, edi    ; param n -> ebx
.entry:
    mov eax, ebx
    ; line 2
    mov eax, ebx
    mov ecx, 1
    cmp eax, ecx
    setle al
    movzx eax, al
    mov r12d, eax
    test eax, eax
    jnz .L_then_0
    jmp .L_endif_1
.L_then_0:
    mov eax, 1
    mov rsp, rbp
    lea rsp, [rbp-24]
    pop r13
    pop r12
    pop rbx
    leave
    ret
.L_endif_1:
    ; line 3
    mov eax, ebx
    mov ecx, 1
    sub eax, ecx
    mov r12d, eax
    ; line 3
    ; line 3
    mov edi, r12d
    call factorial
    mov r12d, eax
    ; line 3
    mov eax, ebx
    mov ecx, r12d
    imul eax, ecx
    mov r13d, eax
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
    mov edi, 5
    call factorial
    mov ebx, eax
    ; line 7: int result
    mov r12d, eax
    mov rsp, rbp
    lea rsp, [rbp-16]
    pop r12
    pop rbx
    leave
    ret

