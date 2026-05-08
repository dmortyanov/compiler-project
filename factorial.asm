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
    sub rsp, 32
    mov dword [rbp-4], edi    ; param n
.entry:
    mov eax, dword [rbp-4]
    mov dword [rbp-8], eax
    ; line 2
    mov eax, dword [rbp-8]
    mov ecx, 1
    cmp eax, ecx
    setle al
    movzx eax, al
    mov dword [rbp-12], eax
    mov eax, dword [rbp-12]
    test eax, eax
    jnz .L_then_0
    jmp .L_endif_1
.L_then_0:
    mov eax, 1
    leave
    ret
.L_endif_1:
    ; line 3
    mov eax, dword [rbp-8]
    mov ecx, 1
    sub eax, ecx
    mov dword [rbp-16], eax
    ; line 3
    ; line 3
    mov edi, dword [rbp-16]
    call factorial
    mov dword [rbp-20], eax
    ; line 3
    mov eax, dword [rbp-8]
    mov ecx, dword [rbp-20]
    imul eax, ecx
    mov dword [rbp-24], eax
    mov eax, dword [rbp-24]
    leave
    ret

; ---- function main ----
main:
    push rbp
    mov rbp, rsp
    sub rsp, 16
.entry:
    ; line 7
    ; line 7
    mov edi, 5
    call factorial
    mov dword [rbp-4], eax
    ; line 7: int result
    mov eax, dword [rbp-4]
    mov dword [rbp-8], eax
    mov eax, dword [rbp-8]
    leave
    ret

