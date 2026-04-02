#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "semantic/type_system.h"

// ---------------------------------------------------------------
// SymbolKind
// ---------------------------------------------------------------
enum class SymbolKind { Variable, Function, Parameter, Struct };

inline std::string symbol_kind_str(SymbolKind k) {
    switch (k) {
        case SymbolKind::Variable:  return "variable";
        case SymbolKind::Function:  return "function";
        case SymbolKind::Parameter: return "parameter";
        case SymbolKind::Struct:    return "struct";
    }
    return "unknown";
}

// ---------------------------------------------------------------
// Symbol — a single entry in the table
// ---------------------------------------------------------------
struct Symbol {
    std::string name;
    Type* type = nullptr;
    SymbolKind kind = SymbolKind::Variable;
    int decl_line = 0;
    int decl_column = 0;
    bool initialized = false;

    std::vector<FunctionParam> params;
    std::string return_type_name;

    std::vector<StructField> fields;

    int stack_offset = 0;
};

// ---------------------------------------------------------------
// Scope — a single scope level
// ---------------------------------------------------------------
struct Scope {
    int id = 0;
    int parent_id = -1;
    int depth = 0;
    std::string label;
    std::unordered_map<std::string, Symbol> symbols;
    int next_offset = 0;
};

// ---------------------------------------------------------------
// SemanticSymbolTable — hierarchical scope manager
// ---------------------------------------------------------------
class SemanticSymbolTable {
public:
    SemanticSymbolTable();

    void enter_scope(const std::string& label = "block");
    void exit_scope();
    int  current_depth() const;
    const std::string& current_label() const;
    bool insert(const Symbol& sym);
    Symbol* lookup(const std::string& name);
    Symbol* lookup_local(const std::string& name);

    const std::vector<Scope>& scopes() const { return scopes_; }
    int current_scope_id() const { return current_; }

    std::string dump_text() const;

    std::string dump_json() const;

private:
    std::vector<Scope> scopes_;
    int current_ = 0;
};
