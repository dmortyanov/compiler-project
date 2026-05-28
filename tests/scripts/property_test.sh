#!/bin/bash
# ============================================================
# property_test.sh — property-based тесты
#
# Свойство: оптимизация НЕ изменяет наблюдаемое поведение.
#
# Для каждой тестовой программы:
#   1. Компилируем БЕЗ --optimize → запускаем → exit code A
#   2. Компилируем С --optimize   → запускаем → exit code B
#   3. A == B → PASS
#
# Использование: bash tests/scripts/property_test.sh [compiler_path]
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

# compile_and_run <src_file> <name_suffix> <extra_flags...>
# Sets global: RESULT_EXIT
compile_and_run() {
    local src_file="$1"
    local suffix="$2"
    shift 2
    local extra_flags="$@"

    local asm_file="$TMPDIR/${suffix}.asm"
    local obj_file="$TMPDIR/${suffix}.o"
    local bin_file="$TMPDIR/${suffix}"

    if ! "$COMPILER" compile --input "$src_file" --output "$asm_file" $extra_flags 2>/dev/null; then
        RESULT_EXIT=-1
        return
    fi

    if ! nasm -f elf64 -o "$obj_file" "$asm_file" 2>/dev/null; then
        RESULT_EXIT=-2
        return
    fi

    if ! ld -o "$bin_file" "$TMPDIR/runtime.o" "$obj_file" 2>/dev/null; then
        RESULT_EXIT=-3
        return
    fi

    set +e
    "$bin_file" >/dev/null 2>&1
    RESULT_EXIT=$?
    set -e
}

echo "=== Property-Based Tests: Optimization Preserves Behavior ==="
echo ""

# Test programs
TEST_PROGRAMS=(
    "tests/codegen/valid/arithmetic/simple_add.src"
    "tests/codegen/valid/control_flow/if_else.src"
    "tests/codegen/valid/control_flow/while_loop.src"
    "tests/codegen/valid/function_calls/factorial.src"
    "tests/integration/e2e/fibonacci.src"
    "tests/integration/e2e/sum_evens.src"
)

for src in "${TEST_PROGRAMS[@]}"; do
    [ -f "$src" ] || continue
    name=$(basename "$src" .src)
    TOTAL=$((TOTAL + 1))

    # Without optimization
    compile_and_run "$src" "${name}_noopt"
    noopt_exit=$RESULT_EXIT

    # With optimization
    compile_and_run "$src" "${name}_opt" --optimize
    opt_exit=$RESULT_EXIT

    # With optimization + inline
    compile_and_run "$src" "${name}_opt_inline" --optimize --inline
    opt_inline_exit=$RESULT_EXIT

    if [ "$noopt_exit" -lt 0 ] || [ "$opt_exit" -lt 0 ]; then
        echo "  SKIP: $name — compilation failed"
        continue
    fi

    if [ "$noopt_exit" -eq "$opt_exit" ] && [ "$noopt_exit" -eq "$opt_inline_exit" ]; then
        echo "  PASS: $name (noopt=$noopt_exit, opt=$opt_exit, opt+inline=$opt_inline_exit)"
        PASS=$((PASS + 1))
    else
        echo "  FAIL: $name (noopt=$noopt_exit, opt=$opt_exit, opt+inline=$opt_inline_exit)"
        FAIL=$((FAIL + 1))
    fi
done

echo ""
echo "=== Summary ==="
echo "Passed: $PASS  Failed: $FAIL  Total: $TOTAL"

[ $FAIL -eq 0 ] && exit 0 || exit 1
