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
        case SemanticErrorKind::UndeclaredIdentifier:   return "необъявленный идентификатор";
        case SemanticErrorKind::DuplicateDeclaration:    return "повторное объявление";
        case SemanticErrorKind::TypeMismatch:            return "несовместимость типов";
        case SemanticErrorKind::ArgCountMismatch:        return "несовпадение количества аргументов";
        case SemanticErrorKind::ArgTypeMismatch:         return "несовпадение типа аргумента";
        case SemanticErrorKind::InvalidReturnType:       return "неверный тип возврата";
        case SemanticErrorKind::InvalidConditionType:    return "неверный тип условия";
        case SemanticErrorKind::UseBeforeDeclaration:    return "использование до объявления";
        case SemanticErrorKind::InvalidAssignmentTarget: return "недопустимая цель присваивания";
        case SemanticErrorKind::UnknownType:             return "неизвестный тип";
        case SemanticErrorKind::InvalidOperator:         return "недопустимый оператор";
        case SemanticErrorKind::MissingReturn:           return "отсутствует оператор return";
    }
    return "неизвестная ошибка";
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
    std::string context;             // например "в функции 'foo'"
    std::string expected_type;
    std::string actual_type;
    std::string suggestion;          // например "возможно, вы имели в виду 'y'?"
};

// ---------------------------------------------------------------
// Format a single error into a pretty string
// ---------------------------------------------------------------
std::string format_semantic_error(const SemanticError& err);

// ---------------------------------------------------------------
// Format all errors into a summary report
// ---------------------------------------------------------------
std::string format_error_report(const std::vector<SemanticError>& errors);
