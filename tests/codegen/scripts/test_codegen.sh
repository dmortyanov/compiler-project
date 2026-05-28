#!/bin/bash
# ============================================================
# test_codegen.sh — end-to-end тестирование кодогенерации
#
# Использование:
#   ./tests/codegen/scripts/test_codegen.sh [compiler_path]
#
# Предполагает: nasm, ld установлены (Linux x86-64)
# Компилятор по умолчанию: ./build/compiler
# Runtime: ./src/runtime/runtime.asm
# ============================================================

set -e

COMPILER="${1:-./build/compiler}"
RUNTIME_SRC="./src/runtime/runtime.asm"
TMPDIR=$(mktemp -d)
PASS=0
FAIL=0
TOTAL=0

cleanup() {
    rm -rf "$TMPDIR"
}
trap cleanup EXIT

# Собираем runtime.o один раз
echo "=== Assembling runtime ==="
nasm -f elf64 -o "$TMPDIR/runtime.o" "$RUNTIME_SRC"
echo "OK"
echo ""

# Таблица тестов: source_file expected_exit_code
declare -A TESTS
TESTS["tests/codegen/valid/arithmetic/simple_add.src"]=5
TESTS["tests/codegen/valid/control_flow/if_else.src"]=12
TESTS["tests/codegen/valid/control_flow/while_loop.src"]=55
TESTS["tests/codegen/valid/function_calls/factorial.src"]=120
TESTS["tests/codegen/valid/integration/full_program.src"]=18

echo "=== End-to-End Codegen Tests ==="
echo ""

for src in "${!TESTS[@]}"; do
    expected="${TESTS[$src]}"
    name=$(basename "$src" .src)
    TOTAL=$((TOTAL + 1))

    # 1. Компилируем в asm
    asm_file="$TMPDIR/${name}.asm"
    if ! "$COMPILER" compile --input "$src" --output "$asm_file" 2>/dev/null; then
        echo "  FAIL: $name — compilation error"
        FAIL=$((FAIL + 1))
        continue
    fi

    # 2. Ассемблируем NASM
    obj_file="$TMPDIR/${name}.o"
    if ! nasm -f elf64 -o "$obj_file" "$asm_file" 2>"$TMPDIR/nasm_err.txt"; then
        echo "  FAIL: $name — NASM error:"
        cat "$TMPDIR/nasm_err.txt" | head -5
        FAIL=$((FAIL + 1))
        continue
    fi

    # 3. Линкуем с runtime
    bin_file="$TMPDIR/${name}"
    if ! gcc -no-pie -nostartfiles -o "$bin_file" "$TMPDIR/runtime.o" "$obj_file" 2>"$TMPDIR/ld_err.txt"; then
        echo "  FAIL: $name — linker error:"
        cat "$TMPDIR/ld_err.txt" | head -5
        FAIL=$((FAIL + 1))
        continue
    fi

    # 4. Запускаем и проверяем exit code
    set +e
    "$bin_file"
    actual=$?
    set -e

    if [ "$actual" -eq "$expected" ]; then
        echo "  PASS: $name (exit=$actual, expected=$expected)"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $name (exit=$actual, expected=$expected)"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "=== Summary ==="
echo "Passed: $PASS  Failed: $FAIL  Total: $TOTAL"

if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
