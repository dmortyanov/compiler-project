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
    sub rsp, 32
    mov dword [rbp-4], edi    ; param a
    mov dword [rbp-8], esi    ; param b
.entry:
    mov eax, dword [rbp-4]
    mov dword [rbp-12], eax
    mov eax, dword [rbp-8]
    mov dword [rbp-16], eax
    ; line 3
    mov eax, dword [rbp-12]
    mov ecx, dword [rbp-16]
    add eax, ecx
    mov dword [rbp-20], eax
    mov eax, dword [rbp-20]
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
    ; line 7
    mov edi, 2
    mov esi, 3
    call add
    mov dword [rbp-4], eax
    ; line 7: int result
    mov eax, dword [rbp-4]
    mov dword [rbp-8], eax
    mov eax, dword [rbp-8]
    leave
    ret

