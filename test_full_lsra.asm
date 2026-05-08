; ============================================================
; MiniCompiler — x86-64 NASM output
; Target: Linux x86-64, System V AMD64 ABI
; ============================================================

section .text

global abs_val
global fib
global main

; ---- function abs_val ----
abs_val:
    push rbp
    mov rbp, rsp
    push rbx    ; save callee-saved
    push r12    ; save callee-saved
    sub rsp, 16
    mov ebx, edi    ; param x -> ebx
.entry:
    mov eax, ebx
    ; line 3
    mov eax, ebx
    xor ecx, ecx
    cmp eax, ecx
    setl al
    movzx eax, al
    mov r12d, eax
    test eax, eax
    jnz .L_then_0
    jmp .L_endif_1
.L_then_0:
    ; line 4
    mov eax, ebx
    neg eax
    mov r12d, eax
    mov rsp, rbp
    lea rsp, [rbp-16]
    pop r12
    pop rbx
    leave
    ret
.L_endif_1:
    mov eax, ebx
    mov rsp, rbp
    lea rsp, [rbp-16]
    pop r12
    pop rbx
    leave
    ret

; ---- function fib ----
fib:
    push rbp
    mov rbp, rsp
    push rbx    ; save callee-saved
    push r12    ; save callee-saved
    push r13    ; save callee-saved
    sub rsp, 40
    mov ebx, edi    ; param n -> ebx
.entry:
    mov eax, ebx
    ; line 10
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
    mov eax, ebx
    mov rsp, rbp
    lea rsp, [rbp-24]
    pop r13
    pop r12
    pop rbx
    leave
    ret
.L_endif_1:
    ; line 11
    mov eax, ebx
    mov ecx, 1
    sub eax, ecx
    mov r12d, eax
    ; line 11
    ; line 11
    mov edi, r12d
    call fib
    mov r12d, eax
    ; line 11
    mov eax, ebx
    mov ecx, 2
    sub eax, ecx
    mov r13d, eax
    ; line 11
    ; line 11
    mov edi, r13d
    call fib
    mov ebx, eax
    ; line 11
    mov eax, r12d
    mov ecx, ebx
    add eax, ecx
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
    push r13    ; save callee-saved
    sub rsp, 40
.entry:
    ; line 15
    mov eax, 5
    neg eax
    mov ebx, eax
    ; line 15
    ; line 15
    mov edi, ebx
    call abs_val
    mov ebx, eax
    ; line 15: int a
    mov r12d, eax
    ; line 16
    ; line 16
    mov edi, 7
    call fib
    mov ebx, eax
    ; line 16: int b
    mov r13d, eax
    ; line 17
    mov eax, r12d
    mov ecx, r13d
    add eax, ecx
    mov ebx, eax
    mov rsp, rbp
    lea rsp, [rbp-24]
    pop r13
    pop r12
    pop rbx
    leave
    ret

