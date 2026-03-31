#include "semantic/symbol_table.h"

#include <sstream>

// ---------------------------------------------------------------
// SemanticSymbolTable
// ---------------------------------------------------------------

SemanticSymbolTable::SemanticSymbolTable() {
    scopes_.push_back(Scope{0, -1, 0, "global", {}, 0});
}

void SemanticSymbolTable::enter_scope(const std::string& label) {
    int id = static_cast<int>(scopes_.size());
    int depth = scopes_[current_].depth + 1;
    scopes_.push_back(Scope{id, current_, depth, label, {}, 0});
    current_ = id;
}

void SemanticSymbolTable::exit_scope() {
    if (current_ > 0)
        current_ = scopes_[current_].parent_id;
}

int SemanticSymbolTable::current_depth() const {
    return scopes_[current_].depth;
}

const std::string& SemanticSymbolTable::current_label() const {
    return scopes_[current_].label;
}

bool SemanticSymbolTable::insert(const Symbol& sym) {
    auto& table = scopes_[current_].symbols;
    if (table.find(sym.name) != table.end())
        return false;  // duplicate
    table[sym.name] = sym;

    // Track stack offset for variables/params
    if (sym.kind == SymbolKind::Variable || sym.kind == SymbolKind::Parameter) {
        auto& inserted = table[sym.name];
        inserted.stack_offset = scopes_[current_].next_offset;
        int sz = sym.type ? sym.type->size_bytes : 4;
        if (sz == 0) sz = 4;
        scopes_[current_].next_offset += sz;
    }
    return true;
}

Symbol* SemanticSymbolTable::lookup(const std::string& name) {
    int s = current_;
    while (s >= 0) {
        auto& table = scopes_[s].symbols;
        auto it = table.find(name);
        if (it != table.end()) return &it->second;
        s = scopes_[s].parent_id;
    }
    return nullptr;
}

Symbol* SemanticSymbolTable::lookup_local(const std::string& name) {
    auto& table = scopes_[current_].symbols;
    auto it = table.find(name);
    if (it != table.end()) return &it->second;
    return nullptr;
}

std::string SemanticSymbolTable::dump_text() const {
    std::ostringstream out;
    out << "Symbol Table (" << scopes_.size() << " scopes):\n";
    for (const auto& scope : scopes_) {
        out << "  Scope #" << scope.id
            << " [" << scope.label << "]"
            << " depth=" << scope.depth
            << " parent=" << scope.parent_id
            << "\n";
        for (const auto& [name, sym] : scope.symbols) {
            out << "    - " << name << ": "
                << symbol_kind_str(sym.kind);
            if (sym.type)
                out << " (" << sym.type->name << ")";
            out << " [line " << sym.decl_line << ":" << sym.decl_column << "]";
            if (sym.kind == SymbolKind::Function) {
                out << " params=(";
                for (size_t i = 0; i < sym.params.size(); ++i) {
                    if (i > 0) out << ", ";
                    out << sym.params[i].type_name << " " << sym.params[i].name;
                }
                out << ") -> " << sym.return_type_name;
            }
            if (sym.kind == SymbolKind::Variable || sym.kind == SymbolKind::Parameter) {
                out << " offset=" << sym.stack_offset;
            }
            out << "\n";
        }
    }
    return out.str();
}

std::string SemanticSymbolTable::dump_json() const {
    std::ostringstream out;
    out << "{\"scopes\":[";
    for (size_t si = 0; si < scopes_.size(); ++si) {
        if (si > 0) out << ",";
        const auto& scope = scopes_[si];
        out << "{\"id\":" << scope.id
            << ",\"label\":\"" << scope.label << "\""
            << ",\"depth\":" << scope.depth
            << ",\"parent_id\":" << scope.parent_id
            << ",\"symbols\":[";
        bool first = true;
        for (const auto& [name, sym] : scope.symbols) {
            if (!first) out << ",";
            first = false;
            out << "{\"name\":\"" << name << "\""
                << ",\"kind\":\"" << symbol_kind_str(sym.kind) << "\"";
            if (sym.type)
                out << ",\"type\":\"" << sym.type->name << "\"";
            out << ",\"line\":" << sym.decl_line
                << ",\"column\":" << sym.decl_column;
            if (sym.kind == SymbolKind::Function) {
                out << ",\"params\":[";
                for (size_t i = 0; i < sym.params.size(); ++i) {
                    if (i > 0) out << ",";
                    out << "{\"type\":\"" << sym.params[i].type_name
                        << "\",\"name\":\"" << sym.params[i].name << "\"}";
                }
                out << "],\"return_type\":\"" << sym.return_type_name << "\"";
            }
            out << "}";
        }
        out << "]}";
    }
    out << "]}";
    return out.str();
}
