section .text
    global _start
    extern main
    global print_int
    global print_string
    global read_int
    global exit_program

; ------------------------------------------------------------
; _start
; Точка входа программы. Вызывает main() и использует
; возвращаемое значение (eax) как код завершения.
; ------------------------------------------------------------
_start:
    call main
    mov edi, eax
    mov eax, 60          ; sys_exit
    syscall

; ------------------------------------------------------------
; exit_program
; Завершает процесс с кодом, переданным в rdi/edi.
; ------------------------------------------------------------
exit_program:
    mov eax, 60          ; sys_exit
    syscall

; ------------------------------------------------------------
; print_int
; Выводит 32-битное знаковое целое число (из edi) в stdout.
; ------------------------------------------------------------
print_int:
    push rbp
    mov rbp, rsp
    sub rsp, 32          ; 32 байта под локальный буфер строки

    mov eax, edi
    mov rsi, rsp
    add rsi, 20
    mov byte [rsi], 0    ; Нуль-терминатор

    test eax, eax
    jns .positive

    neg eax
    mov byte [rsp], '-'  ; Записываем минус в начало буфера
    inc rsp              ; Слегка смещаем начало буфера
    jmp .convert

.positive:
    cmp eax, 0
    jne .convert
    dec rsi
    mov byte [rsi], '0'
    jmp .write

.convert:
    dec rsi
    xor edx, edx
    mov ecx, 10
    div ecx
    add dl, '0'
    mov byte [rsi], dl
    test eax, eax
    jnz .convert

.write:
    ; Вычисление длины строки
    mov rdx, rsp
    add rdx, 20
    sub rdx, rsi         ; длина = (rsp+20) - rsi
    mov rcx, rdx

    mov eax, 1           ; sys_write
    mov edi, 1           ; STDOUT_FILENO
    mov rdx, rcx         ; длина
    syscall

    mov rsp, rbp         ; Восстановление указателя стека
    pop rbp
    ret

; ------------------------------------------------------------
; print_string
; Выводит нуль-терминированную строку (указатель в rdi) в stdout.
; ------------------------------------------------------------
print_string:
    push rbp
    mov rbp, rsp

    mov rsi, rdi
    xor rcx, rcx
    dec rcx

.strlen_loop:
    inc rcx
    cmp byte [rsi + rcx], 0
    jne .strlen_loop

    mov eax, 1           ; sys_write
    mov edi, 1           ; STDOUT_FILENO
    mov rdx, rcx         ; длина
    syscall

    pop rbp
    ret

; ------------------------------------------------------------
; read_int
; Читает 32-битное знаковое целое число из stdin.
; Возвращает результат в eax.
; ------------------------------------------------------------
read_int:
    push rbp
    mov rbp, rsp
    sub rsp, 32          ; Буфер на стеке

    ; Читаем до 31 байта из stdin
    xor eax, eax         ; sys_read
    xor edi, edi         ; STDIN_FILENO
    mov rsi, rsp         ; буфер
    mov edx, 31          ; максимальное количество байт
    syscall

    mov rsi, rsp
    xor eax, eax         ; Накапливаемый результат
    xor ecx, ecx         ; Флаг знака (0 = плюс, 1 = минус)

.skip_whitespace:
    cmp byte [rsi], ' '
    je .next_char
    cmp byte [rsi], 9    ; '\t'
    je .next_char
    cmp byte [rsi], 10   ; '\n'
    je .next_char
    cmp byte [rsi], 13   ; '\r'
    je .next_char
    jmp .check_sign

.next_char:
    inc rsi
    jmp .skip_whitespace

.check_sign:
    cmp byte [rsi], '-'
    jne .check_plus
    mov ecx, 1           ; Устанавливаем флаг отрицательного числа
    inc rsi
    jmp .parse_digits
.check_plus:
    cmp byte [rsi], '+'
    jne .parse_digits
    inc rsi

.parse_digits:
    movzx edx, byte [rsi]
    cmp dl, '0'
    jb .done             ; Выход, если < '0'
    cmp dl, '9'
    ja .done             ; Выход, если > '9'
    
    sub dl, '0'
    imul eax, eax, 10
    add eax, edx

    inc rsi
    jmp .parse_digits

.done:
    test ecx, ecx
    jz .return
    neg eax

.return:
    mov rsp, rbp
    pop rbp
    ret

; Помечаем стек как неисполняемый (для безопасности Linux)
section .note.GNU-stack noalloc noexec nowrite progbits
