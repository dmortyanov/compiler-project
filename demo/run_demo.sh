#!/bin/bash
set -e

# Определяем путь к компилятору
if [ -f "./build/compiler" ]; then
    CC="./build/compiler"
elif [ -f "./compiler" ]; then
    CC="./compiler"
else
    echo "ERROR: compiler not found. Run 'make build' first."
    exit 1
fi

echo "============================================"
echo "  MiniCompiler — Полная демонстрация"
echo "  Спринты 1-8 + Отладка + Тесты"
echo "============================================"
echo ""

# --- Sprint 1: Лексический анализ ---
echo "=== Sprint 1: Лексический анализ ==="
echo "Токенизация demo/showcase.src..."
$CC lex --input demo/showcase.src | head -15
echo "  ..."
echo ""

# --- Sprint 2: Синтаксический анализ ---
echo "=== Sprint 2: Синтаксический анализ ==="
echo "Построение AST (текстовый формат)..."
$CC parse --input demo/showcase.src | head -20
echo "  ..."
echo ""

# --- Sprint 3: Семантический анализ ---
echo "=== Sprint 3: Семантический анализ ==="
echo "Проверка типов и областей видимости..."
$CC check --input demo/showcase.src --verbose 2>&1 | head -10
echo ""

echo "--- Демонстрация системы ошибок ---"
echo "Запуск семантического анализатора на файле с ошибками..."
$CC check --input demo/errors_showcase.src 2>&1 || true
echo ""

# --- Sprint 4: IR генерация ---
echo "=== Sprint 4: Генерация IR (промежуточное представление) ==="
$CC ir --input demo/showcase.src --stats 2>&1 | head -30
echo "  ..."
echo ""

# --- Sprint 5+6: Кодогенерация ---
echo "=== Sprint 5+6: Кодогенерация x86-64 ==="
$CC compile --input demo/showcase.src --output demo/showcase.asm --optimize --inline --regalloc lsra --x86-peephole --stats --verbose 2>&1
echo ""

# --- Sprint 7: Массивы + extern + оптимизации ---
echo "=== Sprint 7: Ассемблирование и линковка ==="
nasm -f elf64 demo/showcase.asm -o demo/showcase.o
gcc -no-pie -o demo/showcase demo/showcase.o
echo "Сборка успешна!"
echo ""

# --- Запуск итоговой программы ---
echo "=== Запуск скомпилированной программы ==="
echo "---"
./demo/showcase
echo "---"
echo ""

# --- Sprint 8: DWARF отладочная информация ---
echo "=== DWARF Debug Info ==="
echo "Компиляция с отладочной информацией (--dwarf)..."
$CC compile --input demo/showcase.src --output demo/showcase_dbg.s --dwarf --optimize 2>&1
echo ""
echo "Первые строки GAS-вывода с .file/.loc:"
head -25 demo/showcase_dbg.s
echo "  ..."
echo ""

# Проверяем наличие as (GNU assembler)
if command -v as &> /dev/null; then
    echo "Сборка с отладочной информацией (as + gcc -g)..."
    as -g -o demo/showcase_dbg.o demo/showcase_dbg.s 2>/dev/null && \
    gcc -no-pie -g -o demo/showcase_dbg demo/showcase_dbg.o 2>/dev/null && \
    echo "Проверка DWARF информации:" && \
    (objdump --dwarf=decodedline demo/showcase_dbg 2>/dev/null | head -15 || \
     readelf --debug-dump=line demo/showcase_dbg 2>/dev/null | head -15 || \
     echo "(objdump/readelf не доступен)") || \
    echo "(Сборка с GAS не удалась — возможно, нужна доработка GAS-вывода)"
    echo ""
fi

# Очистка временных файлов
rm -f demo/showcase_dbg.s demo/showcase_dbg.o demo/showcase_dbg 2>/dev/null || true

echo "============================================"
echo "  Все возможности продемонстрированы!"
echo "============================================"
