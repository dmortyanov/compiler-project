#pragma once

#include <string>

#include "ir/basic_block.h"

// ---------------------------------------------------------------
// IR Output formats
// ---------------------------------------------------------------

/// Human-readable text dump of the IR (OUT-1)
std::string ir_to_text(const IRProgram& program);

/// Graphviz DOT format for CFG visualization (OUT-2)
std::string ir_to_dot(const IRProgram& program);

/// JSON representation of the IR (OUT-3)
std::string ir_to_json(const IRProgram& program);

/// IR statistics report (OUT-4)
std::string ir_statistics(const IRProgram& program);
