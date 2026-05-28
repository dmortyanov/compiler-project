#!/bin/bash
# ============================================================
# fuzz_test.sh — crash-test (fuzzing)
#
# Подаёт случайные и мутированные входные данные компилятору.
# Проверяет, что компилятор НЕ крашится (exit != 139/134/136),
# а корректно выводит ошибку (graceful handling).
#
# Использование: bash tests/scripts/fuzz_test.sh [compiler_path] [iterations]
# ============================================================
set -euo pipefail

COMPILER="${1:-./build/compiler}"
ITERATIONS="${2:-100}"
TMPDIR=$(mktemp -d)
PASS=0
FAIL=0
TOTAL=0

cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

# Crash exit codes: SIGSEGV=139, SIGABRT=134, SIGFPE=136
is_crash() {
    local code=$1
    [ "$code" -eq 139 ] || [ "$code" -eq 134 ] || [ "$code" -eq 136 ]
}

echo "=== Fuzzing Tests (crash-test) ==="
echo "Iterations: $ITERATIONS"
echo ""

# --- Strategy 1: Random bytes ---
echo "--- Strategy 1: Random bytes ---"
for i in $(seq 1 $((ITERATIONS / 4))); do
    TOTAL=$((TOTAL + 1))
    dd if=/dev/urandom bs=$((RANDOM % 200 + 1)) count=1 2>/dev/null > "$TMPDIR/fuzz.src"

    set +e
    "$COMPILER" lex --input "$TMPDIR/fuzz.src" >/dev/null 2>&1
    exit_code=$?
    set -e

    if is_crash $exit_code; then
        echo "  CRASH: random bytes iteration $i (exit=$exit_code)"
        FAIL=$((FAIL + 1))
    else
        PASS=$((PASS + 1))
    fi
done

# --- Strategy 2: Empty / whitespace ---
echo "--- Strategy 2: Edge cases ---"
for content in "" "   " $'\n\n\n' $'\t\t' "{{{{" "))))" ";;;;" "fn" "fn (" "fn main() ->" "int x ="; do
    TOTAL=$((TOTAL + 1))
    echo "$content" > "$TMPDIR/fuzz.src"

    set +e
    "$COMPILER" check --input "$TMPDIR/fuzz.src" >/dev/null 2>&1
    exit_code=$?
    set -e

    if is_crash $exit_code; then
        echo "  CRASH: edge case '$content' (exit=$exit_code)"
        FAIL=$((FAIL + 1))
    else
        PASS=$((PASS + 1))
    fi
done

# --- Strategy 3: Mutated valid programs ---
echo "--- Strategy 3: Mutated programs ---"
VALID_PROGRAM='fn main() -> int { int x = 42; if (x > 10) { return x; } return 0; }'
for i in $(seq 1 $((ITERATIONS / 4))); do
    TOTAL=$((TOTAL + 1))
    # Mutate: delete random chars
    len=${#VALID_PROGRAM}
    pos=$((RANDOM % len))
    cut_len=$((RANDOM % 10 + 1))
    mutated="${VALID_PROGRAM:0:$pos}${VALID_PROGRAM:$((pos + cut_len))}"
    echo "$mutated" > "$TMPDIR/fuzz.src"

    set +e
    "$COMPILER" compile --input "$TMPDIR/fuzz.src" --output "$TMPDIR/fuzz.asm" >/dev/null 2>&1
    exit_code=$?
    set -e

    if is_crash $exit_code; then
        echo "  CRASH: mutation iteration $i (exit=$exit_code)"
        echo "  input: $mutated"
        FAIL=$((FAIL + 1))
    else
        PASS=$((PASS + 1))
    fi
done

# --- Strategy 4: Insert garbage into valid programs ---
echo "--- Strategy 4: Injection ---"
for i in $(seq 1 $((ITERATIONS / 4))); do
    TOTAL=$((TOTAL + 1))
    len=${#VALID_PROGRAM}
    pos=$((RANDOM % len))
    garbage=$(head -c $((RANDOM % 10 + 1)) /dev/urandom | tr -dc 'a-zA-Z0-9@#$%^&*' || true)
    injected="${VALID_PROGRAM:0:$pos}${garbage}${VALID_PROGRAM:$pos}"
    echo "$injected" > "$TMPDIR/fuzz.src"

    set +e
    "$COMPILER" check --input "$TMPDIR/fuzz.src" >/dev/null 2>&1
    exit_code=$?
    set -e

    if is_crash $exit_code; then
        echo "  CRASH: injection iteration $i (exit=$exit_code)"
        FAIL=$((FAIL + 1))
    else
        PASS=$((PASS + 1))
    fi
done

echo ""
echo "=== Summary ==="
echo "Passed: $PASS  Crashed: $FAIL  Total: $TOTAL"

if [ $FAIL -gt 0 ]; then
    echo "WARNING: $FAIL crashes detected — compiler does not handle errors gracefully!"
    exit 1
fi
echo "All inputs handled gracefully."
exit 0
