#include "ir/ir_printer.h"

#include <map>
#include <sstream>

// ---------------------------------------------------------------
// Helper: escape a string for DOT / JSON
// ---------------------------------------------------------------
static std::string escape_dot(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '<':  r += "&lt;";  break;
            case '>':  r += "&gt;";  break;
            case '&':  r += "&amp;"; break;
            case '\n': r += "\\l";   break;
            default:   r += c;
        }
    }
    return r;
}

static std::string escape_json(const std::string& s) {
    std::string r;
    for (char c : s) {
        switch (c) {
            case '"':  r += "\\\""; break;
            case '\\': r += "\\\\"; break;
            case '\n': r += "\\n";  break;
            case '\r': r += "\\r";  break;
            case '\t': r += "\\t";  break;
            default:   r += c;
        }
    }
    return r;
}

// ---------------------------------------------------------------
// ir_to_text — human-readable text dump (OUT-1)
// ---------------------------------------------------------------
std::string ir_to_text(const IRProgram& program) {
    std::ostringstream out;

    for (const auto& func : program.functions) {
        // Function header
        out << "function " << func.name << ": " << func.return_type << " (";
        for (size_t i = 0; i < func.params.size(); ++i) {
            if (i > 0) out << ", ";
            out << func.params[i].second << " " << func.params[i].first;
        }
        out << ")\n";

        // Basic blocks
        for (const auto& block : func.blocks) {
            out << "  " << block.label << ":\n";
            for (const auto& instr : block.instructions) {
                if (instr.opcode == IROpcode::LABEL) continue;
                out << instruction_to_string(instr) << "\n";
            }
            out << "\n";
        }
    }

    return out.str();
}

// ---------------------------------------------------------------
// ir_to_dot — Graphviz DOT for CFG visualization (OUT-2)
// ---------------------------------------------------------------
std::string ir_to_dot(const IRProgram& program) {
    std::ostringstream out;

    out << "digraph CFG {\n";
    out << "  rankdir=TB;\n";
    out << "  node [shape=record, fontname=\"Courier New\", fontsize=10];\n";
    out << "  edge [fontname=\"Courier New\", fontsize=9];\n\n";

    for (const auto& func : program.functions) {
        out << "  subgraph cluster_" << func.name << " {\n";
        out << "    label=\"function " << func.name << ": "
            << func.return_type << "\";\n";
        out << "    style=dashed;\n";
        out << "    color=\"#4A90D9\";\n";
        out << "    fontname=\"Courier New\";\n";
        out << "    fontsize=12;\n\n";

        for (const auto& block : func.blocks) {
            // Node Id: func_block
            std::string node_id = func.name + "_" + block.label;

            // Build label with instructions
            std::string label_str = block.label + ":\\l";
            for (const auto& instr : block.instructions) {
                if (instr.opcode == IROpcode::LABEL) continue;
                label_str += escape_dot(instruction_to_string(instr)) + "\\l";
            }

            // Color coding
            std::string fill_color = "#FFFFFF";
            if (block.label == "entry")
                fill_color = "#D5F5E3";  // green for entry
            else if (!block.instructions.empty() &&
                     block.instructions.back().opcode == IROpcode::RETURN)
                fill_color = "#FADBD8";  // red for exit/return
            else if (block.label.find("L_while") != std::string::npos ||
                     block.label.find("L_for") != std::string::npos)
                fill_color = "#D6EAF8";  // blue for loop headers

            out << "    " << node_id << " [label=\"{" << label_str << "}\""
                << ", style=filled, fillcolor=\"" << fill_color << "\"];\n";
        }

        // Edges
        for (const auto& block : func.blocks) {
            std::string from_id = func.name + "_" + block.label;
            for (const auto& succ : block.successors) {
                std::string to_id = func.name + "_" + succ;
                out << "    " << from_id << " -> " << to_id << ";\n";
            }
        }

        out << "  }\n\n";
    }

    out << "}\n";
    return out.str();
}

// ---------------------------------------------------------------
// ir_to_json — JSON representation of the IR (OUT-3)
// ---------------------------------------------------------------
std::string ir_to_json(const IRProgram& program) {
    std::ostringstream out;

    out << "{\n";
    out << "  \"functions\": [\n";

    for (size_t fi = 0; fi < program.functions.size(); ++fi) {
        const auto& func = program.functions[fi];
        out << "    {\n";
        out << "      \"name\": \"" << escape_json(func.name) << "\",\n";
        out << "      \"return_type\": \"" << escape_json(func.return_type) << "\",\n";

        // Parameters
        out << "      \"params\": [";
        for (size_t pi = 0; pi < func.params.size(); ++pi) {
            if (pi > 0) out << ", ";
            out << "{\"name\": \"" << escape_json(func.params[pi].first)
                << "\", \"type\": \"" << escape_json(func.params[pi].second) << "\"}";
        }
        out << "],\n";

        // Blocks
        out << "      \"blocks\": [\n";
        for (size_t bi = 0; bi < func.blocks.size(); ++bi) {
            const auto& block = func.blocks[bi];
            out << "        {\n";
            out << "          \"label\": \"" << escape_json(block.label) << "\",\n";

            // Successors
            out << "          \"successors\": [";
            for (size_t si = 0; si < block.successors.size(); ++si) {
                if (si > 0) out << ", ";
                out << "\"" << escape_json(block.successors[si]) << "\"";
            }
            out << "],\n";

            // Predecessors
            out << "          \"predecessors\": [";
            for (size_t pi = 0; pi < block.predecessors.size(); ++pi) {
                if (pi > 0) out << ", ";
                out << "\"" << escape_json(block.predecessors[pi]) << "\"";
            }
            out << "],\n";

            // Instructions
            out << "          \"instructions\": [\n";
            for (size_t ii = 0; ii < block.instructions.size(); ++ii) {
                const auto& instr = block.instructions[ii];
                if (instr.opcode == IROpcode::LABEL) continue;
                out << "            {\n";
                out << "              \"opcode\": \"" << opcode_to_string(instr.opcode) << "\",\n";
                out << "              \"dest\": \"" << escape_json(operand_to_string(instr.dest)) << "\",\n";
                out << "              \"srcs\": [";
                for (size_t si = 0; si < instr.srcs.size(); ++si) {
                    if (si > 0) out << ", ";
                    out << "\"" << escape_json(operand_to_string(instr.srcs[si])) << "\"";
                }
                out << "],\n";
                out << "              \"line\": " << instr.source_line;
                if (!instr.comment.empty())
                    out << ",\n              \"comment\": \"" << escape_json(instr.comment) << "\"";
                out << "\n";
                out << "            }";
                if (ii + 1 < block.instructions.size())
                    out << ",";
                out << "\n";
            }
            out << "          ]\n";

            out << "        }";
            if (bi + 1 < func.blocks.size()) out << ",";
            out << "\n";
        }
        out << "      ]\n";

        out << "    }";
        if (fi + 1 < program.functions.size()) out << ",";
        out << "\n";
    }

    out << "  ]\n";
    out << "}\n";
    return out.str();
}

// ---------------------------------------------------------------
// ir_statistics — statistics report (OUT-4)
// ---------------------------------------------------------------
std::string ir_statistics(const IRProgram& program) {
    std::ostringstream out;

    int total_functions = static_cast<int>(program.functions.size());
    int total_blocks = 0;
    int total_instructions = 0;
    int total_temps = 0;
    std::map<std::string, int> opcode_counts;

    for (const auto& func : program.functions) {
        total_blocks += static_cast<int>(func.blocks.size());
        total_temps = std::max(total_temps, func.temp_counter);

        for (const auto& block : func.blocks) {
            for (const auto& instr : block.instructions) {
                if (instr.opcode == IROpcode::LABEL) continue;
                total_instructions++;
                opcode_counts[opcode_to_string(instr.opcode)]++;
            }
        }
    }

    out << "=== IR Statistics ===\n";
    out << "Functions:         " << total_functions << "\n";
    out << "Basic blocks:      " << total_blocks << "\n";
    out << "Total instructions:" << total_instructions << "\n";
    out << "Max temporaries:   " << total_temps << "\n";
    out << "\nInstructions by type:\n";

    for (const auto& [name, count] : opcode_counts) {
        out << "  " << name << ": " << count << "\n";
    }

    // Per-function breakdown
    out << "\nPer-function breakdown:\n";
    for (const auto& func : program.functions) {
        int fb = static_cast<int>(func.blocks.size());
        int fi = 0;
        for (const auto& block : func.blocks)
            for (const auto& instr : block.instructions)
                if (instr.opcode != IROpcode::LABEL)
                    fi++;
        out << "  " << func.name << ": " << fb << " blocks, "
            << fi << " instructions, " << func.temp_counter << " temps\n";
    }

    return out.str();
}
