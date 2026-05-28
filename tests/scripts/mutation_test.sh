#!/bin/bash
# ============================================================
# mutation_test.sh — Mutation Testing
#
# Вносит мутации в исходный код компилятора, пересобирает,
# запускает тесты. Если тесты проходят — мутант "выжил" (плохо).
#
# Использование: bash tests/scripts/mutation_test.sh
# ============================================================
set -euo pipefail

TMPDIR=$(mktemp -d)
SURVIVED=0
KILLED=0
TOTAL=0

cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

echo "=== Mutation Testing ==="
echo ""
echo "ВАЖНО: Этот скрипт модифицирует исходники компилятора, пересобирает"
echo "и запускает тесты. Оригинальные файлы восстанавливаются автоматически."
echo ""

# Файлы для мутации и конкретные мутации: file, original, mutated, description
declare -a MUT_FILES
declare -a MUT_ORIG
declare -a MUT_REPL
declare -a MUT_DESC

# Мутация 1: IROpcode ADD → SUB в optimizer
MUT_FILES+=("src/ir/optimizer.cpp")
MUT_ORIG+=("IROpcode::ADD")
MUT_REPL+=("IROpcode::SUB")
MUT_DESC+=("Change ADD to SUB in optimizer")

# Мутация 2: CMP_LT → CMP_GT
MUT_FILES+=("src/ir/ir_generator.cpp")
MUT_ORIG+=("IROpcode::CMP_LT")
MUT_REPL+=("IROpcode::CMP_GT")
MUT_DESC+=("Change CMP_LT to CMP_GT in IR generator")

# Мутация 3: Инвертировать условие return
MUT_FILES+=("src/ir/ir_instructions.cpp")
MUT_ORIG+=("IROpcode::RETURN")
MUT_REPL+=("IROpcode::NOP")
MUT_DESC+=("Change RETURN to NOP in instruction builder")

echo "Total mutations: ${#MUT_FILES[@]}"
echo ""

for i in "${!MUT_FILES[@]}"; do
    file="${MUT_FILES[$i]}"
    orig="${MUT_ORIG[$i]}"
    repl="${MUT_REPL[$i]}"
    desc="${MUT_DESC[$i]}"
    TOTAL=$((TOTAL + 1))

    echo "--- Mutation $((i+1)): $desc ---"
    echo "  File: $file"
    echo "  $orig → $repl"

    # Backup
    cp "$file" "$TMPDIR/backup_$i"

    # Apply mutation (first occurrence only)
    sed -i "0,/$orig/{s/$orig/$repl/}" "$file"

    # Rebuild
    set +e
    (cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug > /dev/null 2>&1 && make compiler > /dev/null 2>&1)
    build_ok=$?
    set -e

    if [ $build_ok -ne 0 ]; then
        echo "  RESULT: Mutant killed (build failure) ✓"
        KILLED=$((KILLED + 1))
        cp "$TMPDIR/backup_$i" "$file"
        continue
    fi

    # Run tests
    set +e
    (cd build && ctest --output-on-failure -R "codegen_tests|ir_tests" > /dev/null 2>&1)
    test_result=$?
    set -e

    # Restore
    cp "$TMPDIR/backup_$i" "$file"

    if [ $test_result -ne 0 ]; then
        echo "  RESULT: Mutant killed (tests failed) ✓"
        KILLED=$((KILLED + 1))
    else
        echo "  RESULT: Mutant SURVIVED (tests still pass) ✗"
        SURVIVED=$((SURVIVED + 1))
    fi
    echo ""
done

# Rebuild original
echo "=== Rebuilding original ==="
(cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug > /dev/null 2>&1 && make compiler > /dev/null 2>&1)

echo ""
echo "=== Summary ==="
echo "Total mutations: $TOTAL"
echo "Killed:   $KILLED"
echo "Survived: $SURVIVED"
if [ $TOTAL -gt 0 ]; then
    score=$(( KILLED * 100 / TOTAL ))
    echo "Mutation Score: ${score}%"
fi
echo ""

if [ $SURVIVED -gt 0 ]; then
    echo "WARNING: $SURVIVED mutants survived — consider improving test coverage."
    exit 1
fi
echo "All mutants killed — good test coverage!"
exit 0
