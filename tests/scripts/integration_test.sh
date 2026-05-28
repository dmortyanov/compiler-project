#!/bin/bash
# ============================================================
# integration_test.sh — end-to-end чёрный ящик
#
# Компилирует .src → asm → binary, проверяет exit code и stdout.
# Использование: bash tests/scripts/integration_test.sh [compiler_path]
# ============================================================
set -euo pipefail

COMPILER="${1:-./build/compiler}"
RUNTIME_SRC="./src/runtime/runtime.asm"
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

# run_test <name> <source> <expected_exit> [expected_stdout_pattern]
run_test() {
    local name="$1"
    local src_file="$2"
    local expected_exit="$3"
    local expected_stdout="${4:-}"
    TOTAL=$((TOTAL + 1))

    # 1. Compile
    local asm_file="$TMPDIR/${name}.asm"
    if ! "$COMPILER" compile --input "$src_file" --output "$asm_file" --optimize 2>/dev/null; then
        echo "  FAIL: $name — compilation error"
        FAIL=$((FAIL + 1))
        return
    fi

    # 2. Assemble
    local obj_file="$TMPDIR/${name}.o"
    if ! nasm -f elf64 -o "$obj_file" "$asm_file" 2>"$TMPDIR/nasm_err.txt"; then
        echo "  FAIL: $name — NASM error:"
        head -5 "$TMPDIR/nasm_err.txt"
        FAIL=$((FAIL + 1))
        return
    fi

    # 3. Link
    local bin_file="$TMPDIR/${name}"
    if ! ld -o "$bin_file" "$TMPDIR/runtime.o" "$obj_file" 2>"$TMPDIR/ld_err.txt"; then
        echo "  FAIL: $name — linker error:"
        head -5 "$TMPDIR/ld_err.txt"
        FAIL=$((FAIL + 1))
        return
    fi

    # 4. Run
    set +e
    local actual_stdout
    actual_stdout=$("$bin_file" 2>/dev/null)
    local actual_exit=$?
    set -e

    # 5. Check exit code
    if [ "$actual_exit" -ne "$expected_exit" ]; then
        echo "  FAIL: $name (exit=$actual_exit, expected=$expected_exit)"
        FAIL=$((FAIL + 1))
        return
    fi

    # 6. Check stdout pattern (optional)
    if [ -n "$expected_stdout" ]; then
        if ! echo "$actual_stdout" | grep -q "$expected_stdout"; then
            echo "  FAIL: $name — stdout mismatch (expected pattern: '$expected_stdout')"
            echo "  actual: $actual_stdout"
            FAIL=$((FAIL + 1))
            return
        fi
    fi

    echo "  PASS: $name (exit=$actual_exit)"
    PASS=$((PASS + 1))
}

echo "=== Integration Tests (E2E) ==="
echo ""

# --- Тесты на файлах из tests/integration/e2e/ ---
for src_file in tests/integration/e2e/*.src; do
    [ -f "$src_file" ] || continue
    name=$(basename "$src_file" .src)
    expected_file="${src_file%.src}.expected"
    if [ -f "$expected_file" ]; then
        expected_exit=$(head -1 "$expected_file" | tr -d '[:space:]')
        expected_stdout=$(tail -n +2 "$expected_file" | head -1)
        run_test "$name" "$src_file" "$expected_exit" "$expected_stdout"
    fi
done

# --- Тесты на существующих файлах ---
run_test "factorial" "tests/codegen/valid/function_calls/factorial.src" 120
run_test "simple_add" "tests/codegen/valid/arithmetic/simple_add.src" 5

echo ""
echo "=== Summary ==="
echo "Passed: $PASS  Failed: $FAIL  Total: $TOTAL"

[ $FAIL -eq 0 ] && exit 0 || exit 1
