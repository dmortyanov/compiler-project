#include "semantic/errors.h"

#include <sstream>

std::string format_semantic_error(const SemanticError& err) {
    std::ostringstream out;
    out << "семантическая ошибка: " << error_kind_str(err.kind);
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
        out << "  = ожидалось: " << err.expected_type << "\n";
    if (!err.actual_type.empty())
        out << "  = найдено: " << err.actual_type << "\n";

    // Suggestion
    if (!err.suggestion.empty())
        out << "  = примечание: " << err.suggestion << "\n";

    return out.str();
}

std::string format_error_report(const std::vector<SemanticError>& errors) {
    if (errors.empty())
        return "семантический анализ: ошибки не найдены\n";

    std::ostringstream out;
    for (const auto& err : errors) {
        out << format_semantic_error(err) << "\n";
    }
    
    std::string err_word = "ошибок";
    if (errors.size() % 10 == 1 && errors.size() % 100 != 11) {
        err_word = "ошибка";
    } else if (errors.size() % 10 >= 2 && errors.size() % 10 <= 4 && (errors.size() % 100 < 10 || errors.size() % 100 >= 20)) {
        err_word = "ошибки";
    }
    
    out << "--- найдено " << errors.size() << " семантическ" 
        << (err_word == "ошибка" ? "ая " : (err_word == "ошибки" ? "ие " : "их "))
        << err_word << " ---\n";
    return out.str();
}
