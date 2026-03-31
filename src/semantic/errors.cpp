#include "semantic/errors.h"

#include <sstream>

std::string format_semantic_error(const SemanticError& err) {
    std::ostringstream out;
    out << "semantic error: " << error_kind_str(err.kind);
    if (!err.message.empty())
        out << ": " << err.message;
    out << "\n";

    // Location
    std::string file = err.filename.empty() ? "program.src" : err.filename;
    out << "  --> " << file << ":" << err.line << ":" << err.column << "\n";

    // Context
    if (!err.context.empty())
        out << "  | " << err.context << "\n";

    // Expected / actual types
    if (!err.expected_type.empty())
        out << "  = expected: " << err.expected_type << "\n";
    if (!err.actual_type.empty())
        out << "  = found: " << err.actual_type << "\n";

    // Suggestion
    if (!err.suggestion.empty())
        out << "  = note: " << err.suggestion << "\n";

    return out.str();
}

std::string format_error_report(const std::vector<SemanticError>& errors) {
    if (errors.empty())
        return "semantic analysis: no errors found\n";

    std::ostringstream out;
    for (const auto& err : errors) {
        out << format_semantic_error(err) << "\n";
    }
    out << "--- " << errors.size() << " semantic error"
        << (errors.size() != 1 ? "s" : "") << " found ---\n";
    return out.str();
}
