#!/bin/bash
# ============================================================
# benchmark.sh — Профайлинг и замеры производительности
#
# Три режима:
#   1. time (секундомер)
#   2. perf stat (IPC, cache misses, cycles)
#   3. perf record + perf report (горячие функции)
#
# Использование: bash tests/scripts/benchmark.sh [compiler_path] [mode]
#   mode: time | perf | profile | all  (default: all)
# ============================================================
set -euo pipefail

COMPILER="${1:-./build/compiler}"
MODE="${2:-all}"
INPUT="demo/showcase.src"
TMPDIR=$(mktemp -d)

cleanup() { rm -rf "$TMPDIR"; }
trap cleanup EXIT

echo "=== MiniCompiler Benchmark ==="
echo "Compiler: $COMPILER"
echo "Input: $INPUT"
echo ""

# --- 1. Замер по секундомеру (time) ---
if [ "$MODE" = "time" ] || [ "$MODE" = "all" ]; then
    echo "--- 1. Time (секундомер) ---"
    echo ""

    echo "Compile (no optimization):"
    time "$COMPILER" compile --input "$INPUT" --output "$TMPDIR/out.asm" 2>/dev/null
    echo ""

    echo "Compile (with optimization):"
    time "$COMPILER" compile --input "$INPUT" --output "$TMPDIR/out_opt.asm" --optimize 2>/dev/null
    echo ""

    echo "Compile (with optimization + inline):"
    time "$COMPILER" compile --input "$INPUT" --output "$TMPDIR/out_opt_inl.asm" --optimize --inline 2>/dev/null
    echo ""

    echo "Lex only:"
    time "$COMPILER" lex --input "$INPUT" --output "$TMPDIR/tokens.txt" 2>/dev/null
    echo ""

    echo "Parse only:"
    time "$COMPILER" parse --input "$INPUT" --output "$TMPDIR/ast.txt" 2>/dev/null
    echo ""

    echo "IR generation:"
    time "$COMPILER" ir --input "$INPUT" --output "$TMPDIR/ir.txt" 2>/dev/null
    echo ""
fi

# --- 2. perf stat (IPC, cache misses, cycles) ---
if [ "$MODE" = "perf" ] || [ "$MODE" = "all" ]; then
    echo "--- 2. perf stat (hardware counters) ---"
    echo ""

    if ! command -v perf &> /dev/null; then
        echo "WARNING: 'perf' not found. Install with: sudo apt install linux-tools-common linux-tools-$(uname -r)"
        echo "Skipping perf tests."
    else
        echo "Full compilation pipeline:"
        perf stat -e cycles,instructions,cache-references,cache-misses,branches,branch-misses \
            "$COMPILER" compile --input "$INPUT" --output "$TMPDIR/perf_out.asm" --optimize 2>&1 \
            || echo "(perf may require root or kernel.perf_event_paranoid=-1)"
        echo ""

        echo "IPC (Instructions Per Cycle):"
        perf stat -e instructions,cycles \
            "$COMPILER" compile --input "$INPUT" --output "$TMPDIR/perf_ipc.asm" --optimize --inline 2>&1 \
            || echo "(perf may require elevated permissions)"
        echo ""
    fi
fi

# --- 3. perf record + perf report (горячие функции) ---
if [ "$MODE" = "profile" ] || [ "$MODE" = "all" ]; then
    echo "--- 3. perf record (function-level profiling) ---"
    echo ""

    if ! command -v perf &> /dev/null; then
        echo "WARNING: 'perf' not found. Skipping."
    else
        echo "Recording profile..."
        perf record -g -o "$TMPDIR/perf.data" \
            "$COMPILER" compile --input "$INPUT" --output "$TMPDIR/prof_out.asm" --optimize --inline 2>/dev/null \
            || echo "(perf record may require elevated permissions)"

        if [ -f "$TMPDIR/perf.data" ]; then
            echo ""
            echo "Top functions (perf report --stdio):"
            perf report -i "$TMPDIR/perf.data" --stdio --no-children 2>/dev/null | head -40 \
                || echo "(perf report failed)"
        fi
        echo ""
    fi
fi

echo "=== Benchmark complete ==="
