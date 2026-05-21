#!/bin/bash
set -e
echo "=== Компиляция showcase ==="
./compiler compile --input demo/showcase.src --output demo/showcase.asm --optimize --stats --verbose --inline
echo "=== Ассемблирование ==="
nasm -f elf64 demo/showcase.asm -o demo/showcase.o
echo "=== Линковка ==="
gcc -no-pie -o demo/showcase demo/showcase.o
echo "=== Запуск ==="
./demo/showcase
echo ""
echo "=== Демонстрация системы ошибок ==="
echo "Запуск семантического анализатора на файле с ошибками..."
./compiler check --input demo/errors_showcase.src || true
echo "=== Готово ==="
