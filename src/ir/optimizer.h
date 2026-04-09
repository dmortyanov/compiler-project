#pragma once

#include <string>
#include <vector>

#include "ir/basic_block.h"

// ---------------------------------------------------------------
// OptimizationEntry — a single optimization performed
// ---------------------------------------------------------------
struct OptimizationEntry {
    std::string function;
    std::string block;
    std::string description;
    int instruction_index = 0;
};

// ---------------------------------------------------------------
// OptimizationMetrics — aggregate stats
// ---------------------------------------------------------------
struct OptimizationMetrics {
    int instructions_removed = 0;
    int instructions_modified = 0;
    int constants_folded = 0;
    int algebraic_simplifications = 0;
    int strength_reductions = 0;
    int dead_code_eliminated = 0;
    int jumps_chained = 0;
};

// ---------------------------------------------------------------
// PeepholeOptimizer — simple local optimizations on IR
// ---------------------------------------------------------------
class PeepholeOptimizer {
public:
    explicit PeepholeOptimizer(IRProgram& program);

    /// Apply all optimization passes.
    void optimize();

    /// Get human-readable report of changes.
    std::string get_optimization_report() const;

    /// Get raw metrics.
    const OptimizationMetrics& get_metrics() const { return metrics_; }

private:
    IRProgram& program_;
    std::vector<OptimizationEntry> log_;
    OptimizationMetrics metrics_;

    // Individual passes
    void fold_constants(IRFunction& func);
    void simplify_algebraic(IRFunction& func);
    void reduce_strength(IRFunction& func);
    void eliminate_dead_code(IRFunction& func);
    void chain_jumps(IRFunction& func);

    void add_entry(const std::string& func, const std::string& block,
                   int idx, const std::string& desc);
};
