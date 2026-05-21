#pragma once

#include "ir/basic_block.h"

class FunctionInliner {
public:
    explicit FunctionInliner(IRProgram& program);
    void run();
    int get_functions_inlined() const { return functions_inlined_; }

private:
    IRProgram& program_;
    int functions_inlined_ = 0;
    int inline_counter_ = 0;

    bool should_inline(const IRFunction& func) const;
    void inline_calls(IRFunction& caller);
};
