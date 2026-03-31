#pragma once

#include <string>
#include <vector>

// ---------------------------------------------------------------
// SemanticErrorKind
// ---------------------------------------------------------------
enum class SemanticErrorKind {
    UndeclaredIdentifier,
    DuplicateDeclaration,
    TypeMismatch,
    ArgCountMismatch,
    ArgTypeMismatch,
    InvalidReturnType,
    InvalidConditionType,
    UseBeforeDeclaration,
    InvalidAssignmentTarget,
    UnknownType,
    InvalidOperator,
    MissingReturn
};

inline std::string error_kind_str(SemanticErrorKind k) {
    switch (k) {
        case SemanticErrorKind::UndeclaredIdentifier:   return "undeclared identifier";
        case SemanticErrorKind::DuplicateDeclaration:    return "duplicate declaration";
        case SemanticErrorKind::TypeMismatch:            return "type mismatch";
        case SemanticErrorKind::ArgCountMismatch:        return "argument count mismatch";
        case SemanticErrorKind::ArgTypeMismatch:         return "argument type mismatch";
        case SemanticErrorKind::InvalidReturnType:       return "invalid return type";
        case SemanticErrorKind::InvalidConditionType:    return "invalid condition type";
        case SemanticErrorKind::UseBeforeDeclaration:    return "use before declaration";
        case SemanticErrorKind::InvalidAssignmentTarget: return "invalid assignment target";
        case SemanticErrorKind::UnknownType:             return "unknown type";
        case SemanticErrorKind::InvalidOperator:         return "invalid operator";
        case SemanticErrorKind::MissingReturn:           return "missing return statement";
    }
    return "unknown error";
}

// ---------------------------------------------------------------
// SemanticError
// ---------------------------------------------------------------
struct SemanticError {
    SemanticErrorKind kind;
    std::string message;
    std::string filename;
    int line = 0;
    int column = 0;
    std::string context;             // e.g. "in function 'foo'"
    std::string expected_type;
    std::string actual_type;
    std::string suggestion;          // e.g. "did you mean 'y'?"
};

// ---------------------------------------------------------------
// Format a single error into a pretty string
// ---------------------------------------------------------------
std::string format_semantic_error(const SemanticError& err);

// ---------------------------------------------------------------
// Format all errors into a summary report
// ---------------------------------------------------------------
std::string format_error_report(const std::vector<SemanticError>& errors);
