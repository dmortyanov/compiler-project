#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include "parser/ast.h"

struct SymbolInfo {
    std::string kind;      // "var", "fn", "struct", "param"
    std::string type_name; // for vars/params/fns
    int line = 0;
    int column = 0;
};

struct ScopeTable {
    int id = 0;
    int parent_id = -1;
    std::unordered_map<std::string, SymbolInfo> symbols;
};

struct SymbolTableResult {
    std::vector<ScopeTable> scopes;
    std::vector<std::string> errors;
};

// Builds scope tables (very small semantic pass).
SymbolTableResult build_symbol_tables(ProgramNode& program);
