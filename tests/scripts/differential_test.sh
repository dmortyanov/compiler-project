#!/bin/bash
# ============================================================
# differential_test.sh — сравнение с GCC
#
# Для каждой пары program.src / program.c:
#   1. Компилирует .src нашим компилятором → запускает → exit code A
#   2. Компилирует .c через gcc → запускает → exit code B
#   3. A == B → PASS
#
# Использование: bash tests/scripts/differential_test.sh [compiler_path]
# ============================================================
set -euo pipefail

COMPILER="${1:-./build/compiler}"
RUNTIME_SRC="./src/runtime/runtime.asm"
DIFF_DIR="./tests/differential"
TMPDIR=$(mktemp -d)
PASS=0
FAIL=0
TOTAL=0

cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

echo "=== Assembling runtime ==="
nasm -f elf64 -o "$TMPDIR/runtime.o" "$RUNTIME_SRC"
echo "OK"
echo ""

echo "=== Differential Tests (vs GCC) ==="
echo ""

for src_file in "$DIFF_DIR"/*.src; do
    [ -f "$src_file" ] || continue
    name=$(basename "$src_file" .src)
    c_file="$DIFF_DIR/${name}.c"

    if [ ! -f "$c_file" ]; then
        echo "  SKIP: $name — no matching .c file"
        continue
    fi

    TOTAL=$((TOTAL + 1))

    # --- Наш компилятор ---
    asm_file="$TMPDIR/${name}.asm"
    if ! "$COMPILER" compile --input "$src_file" --output "$asm_file" --optimize 2>/dev/null; then
        echo "  FAIL: $name — our compiler failed"
        FAIL=$((FAIL + 1))
        continue
    fi

    obj_file="$TMPDIR/${name}.o"
    if ! nasm -f elf64 -o "$obj_file" "$asm_file" 2>/dev/null; then
        echo "  FAIL: $name — NASM error"
        FAIL=$((FAIL + 1))
        continue
    fi

    our_bin="$TMPDIR/${name}_ours"
    if ! gcc -no-pie -nostartfiles -o "$our_bin" "$TMPDIR/runtime.o" "$obj_file" 2>/dev/null; then
        echo "  FAIL: $name — linker error"
        FAIL=$((FAIL + 1))
        continue
    fi

    set +e
    "$our_bin" > "$TMPDIR/our_stdout.txt" 2>/dev/null
    our_exit=$?
    set -e

    # --- GCC ---
    gcc_bin="$TMPDIR/${name}_gcc"
    if ! gcc -o "$gcc_bin" "$c_file" -lm 2>/dev/null; then
        echo "  FAIL: $name — GCC compilation failed"
        FAIL=$((FAIL + 1))
        continue
    fi

    set +e
    "$gcc_bin" > "$TMPDIR/gcc_stdout.txt" 2>/dev/null
    gcc_exit=$?
    set -e

    # --- Сравнение ---
    if [ "$our_exit" -eq "$gcc_exit" ]; then
        echo "  PASS: $name (exit: ours=$our_exit, gcc=$gcc_exit)"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $name (exit: ours=$our_exit, gcc=$gcc_exit)"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "=== Summary ==="
echo "Passed: $PASS  Failed: $FAIL  Total: $TOTAL"

[ $FAIL -eq 0 ] && exit 0 || exit 1
