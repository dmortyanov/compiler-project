#include "parser/symbol_table.h"

#include <sstream>

namespace {

struct Builder : ASTVisitor {
    SymbolTableResult result;
    int current_scope = 0;

    Builder() {
        result.scopes.push_back(ScopeTable{0, -1, {}});
    }

    int push_scope() {
        int id = static_cast<int>(result.scopes.size());
        result.scopes.push_back(ScopeTable{id, current_scope, {}});
        current_scope = id;
        return id;
    }

    void pop_scope() {
        current_scope = result.scopes[current_scope].parent_id;
    }

    void add_symbol(const std::string& name, const SymbolInfo& info) {
        auto& table = result.scopes[current_scope].symbols;
        if (table.find(name) != table.end()) {
            std::ostringstream ss;
            ss << info.line << ":" << info.column << " duplicate symbol '" << name
               << "' in same scope";
            result.errors.push_back(ss.str());
            return;
        }
        table[name] = info;
    }

    bool resolve(const std::string& name) const {
        int s = current_scope;
        while (s >= 0) {
            const auto& table = result.scopes[s].symbols;
            if (table.find(name) != table.end()) return true;
            s = result.scopes[s].parent_id;
        }
        return false;
    }

    void visit(ProgramNode& node) override {
        for (auto& d : node.declarations) d->accept(*this);
    }

    void visit(FunctionDeclNode& node) override {
        add_symbol(node.name, SymbolInfo{"fn", node.return_type, node.line, node.column});
        int prev = current_scope;
        push_scope();
        for (const auto& p : node.parameters) {
            add_symbol(p.name, SymbolInfo{"param", p.type_name, p.line, p.column});
        }
        if (node.body) node.body->accept(*this);
        pop_scope();
        current_scope = prev;
    }

    void visit(StructDeclNode& node) override {
        add_symbol(node.name, SymbolInfo{"struct", node.name, node.line, node.column});
        int prev = current_scope;
        push_scope();
        for (auto& f : node.fields) f->accept(*this);
        pop_scope();
        current_scope = prev;
    }

    void visit(VarDeclStmtNode& node) override {
        add_symbol(node.name, SymbolInfo{"var", node.type_name, node.line, node.column});
        if (node.initializer) node.initializer->accept(*this);
    }

    void visit(BlockStmtNode& node) override {
        int prev = current_scope;
        push_scope();
        for (auto& s : node.statements) s->accept(*this);
        pop_scope();
        current_scope = prev;
    }

    void visit(ExprStmtNode& node) override {
        if (node.expression) node.expression->accept(*this);
    }

    void visit(IfStmtNode& node) override {
        if (node.condition) node.condition->accept(*this);
        if (node.then_branch) node.then_branch->accept(*this);
        if (node.else_branch) node.else_branch->accept(*this);
    }

    void visit(WhileStmtNode& node) override {
        if (node.condition) node.condition->accept(*this);
        if (node.body) node.body->accept(*this);
    }

    void visit(ForStmtNode& node) override {
        int prev = current_scope;
        push_scope();
        if (node.init) node.init->accept(*this);
        if (node.condition) node.condition->accept(*this);
        if (node.update) node.update->accept(*this);
        if (node.body) node.body->accept(*this);
        pop_scope();
        current_scope = prev;
    }

    void visit(ReturnStmtNode& node) override {
        if (node.value) node.value->accept(*this);
    }

    void visit(LiteralExprNode&) override {}

    void visit(IdentifierExprNode& node) override {
        if (!resolve(node.name)) {
            std::ostringstream ss;
            ss << node.line << ":" << node.column << " unknown identifier '"
               << node.name << "'";
            result.errors.push_back(ss.str());
        }
    }

    void visit(BinaryExprNode& node) override {
        if (node.left) node.left->accept(*this);
        if (node.right) node.right->accept(*this);
    }

    void visit(UnaryExprNode& node) override {
        if (node.operand) node.operand->accept(*this);
    }

    void visit(CallExprNode& node) override {
        if (!resolve(node.callee)) {
            std::ostringstream ss;
            ss << node.line << ":" << node.column << " unknown function '"
               << node.callee << "'";
            result.errors.push_back(ss.str());
        }
        for (auto& a : node.arguments) a->accept(*this);
    }

    void visit(PostfixExprNode& node) override {
        if (node.operand) node.operand->accept(*this);
    }

    void visit(AssignmentExprNode& node) override {
        if (node.target) node.target->accept(*this);
        if (node.value) node.value->accept(*this);
    }
};

} // namespace

SymbolTableResult build_symbol_tables(ProgramNode& program) {
    Builder b;
    program.accept(b);
    return b.result;
}
