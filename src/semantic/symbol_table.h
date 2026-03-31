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
    Type* type = nullptr;           // resolved Type*
    SymbolKind kind = SymbolKind::Variable;
    int decl_line = 0;
    int decl_column = 0;
    bool initialized = false;       // has initializer / is param

    // For functions — cached for quick access
    std::vector<FunctionParam> params;
    std::string return_type_name;

    // For structs
    std::vector<StructField> fields;

    // Stack layout (for future code-gen)
    int stack_offset = 0;
};

// ---------------------------------------------------------------
// Scope — a single scope level
// ---------------------------------------------------------------
struct Scope {
    int id = 0;
    int parent_id = -1;
    int depth = 0;
    std::string label;              // "global", "function:foo", "block", "struct:Bar"
    std::unordered_map<std::string, Symbol> symbols;
    int next_offset = 0;            // next stack offset within scope
};

// ---------------------------------------------------------------
// SemanticSymbolTable — hierarchical scope manager
// ---------------------------------------------------------------
class SemanticSymbolTable {
public:
    SemanticSymbolTable();

    // Scope management
    void enter_scope(const std::string& label = "block");
    void exit_scope();
    int  current_depth() const;
    const std::string& current_label() const;

    // Symbol operations
    bool insert(const Symbol& sym);               // false if duplicate
    Symbol* lookup(const std::string& name);       // current → outer
    Symbol* lookup_local(const std::string& name); // current scope only

    // Access
    const std::vector<Scope>& scopes() const { return scopes_; }
    int current_scope_id() const { return current_; }

    // Dump (text)
    std::string dump_text() const;

    // Dump (json)
    std::string dump_json() const;

private:
    std::vector<Scope> scopes_;
    int current_ = 0;
};
