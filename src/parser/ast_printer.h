#pragma once

#include <sstream>
#include <string>

#include "parser/ast.h"

class ASTPrettyPrinter : public ASTVisitor {
public:
    std::string result() const { return out_.str(); }

    void visit(ProgramNode& node) override {
        out_ << "Program [line " << node.line << "]:\n";
        indent_++;
        for (auto& d : node.declarations) {
            d->accept(*this);
        }
        indent_--;
    }

    void visit(FunctionDeclNode& node) override {
        ind();
        out_ << "FunctionDecl: " << node.name << " -> " << node.return_type
             << " [line " << node.line << "]:\n";
        indent_++;
        ind();
        out_ << "Parameters: [";
        for (std::size_t i = 0; i < node.parameters.size(); ++i) {
            if (i > 0) out_ << ", ";
            out_ << node.parameters[i].type_name << " "
                 << node.parameters[i].name;
        }
        out_ << "]\n";
        ind();
        out_ << "Body [line " << node.body->line << "]:\n";
        indent_++;
        node.body->accept(*this);
        indent_--;
        indent_--;
    }

    void visit(StructDeclNode& node) override {
        ind();
        out_ << "StructDecl: " << node.name << " [line " << node.line
             << "]:\n";
        indent_++;
        for (auto& f : node.fields) {
            f->accept(*this);
        }
        indent_--;
    }

    void visit(VarDeclStmtNode& node) override {
        ind();
        out_ << "VarDecl: " << node.type_name << " " << node.name;
        if (node.initializer) {
            out_ << " = ";
            expr_inline_ = true;
            node.initializer->accept(*this);
            expr_inline_ = false;
        }
        out_ << " [line " << node.line << "]\n";
    }

    void visit(BlockStmtNode& node) override {
        ind();
        out_ << "Block:\n";
        indent_++;
        for (auto& s : node.statements) {
            s->accept(*this);
        }
        indent_--;
    }

    void visit(ExprStmtNode& node) override {
        if (node.expression) {
            ind();
            out_ << "ExprStmt [line " << node.line << "]:\n";
            indent_++;
            node.expression->accept(*this);
            indent_--;
        }
    }

    void visit(IfStmtNode& node) override {
        ind();
        out_ << "IfStmt [line " << node.line << "]:\n";
        indent_++;
        ind();
        out_ << "Condition:\n";
        indent_++;
        node.condition->accept(*this);
        indent_--;
        ind();
        out_ << "Then:\n";
        indent_++;
        node.then_branch->accept(*this);
        indent_--;
        if (node.else_branch) {
            ind();
            out_ << "Else:\n";
            indent_++;
            node.else_branch->accept(*this);
            indent_--;
        }
        indent_--;
    }

    void visit(WhileStmtNode& node) override {
        ind();
        out_ << "WhileStmt [line " << node.line << "]:\n";
        indent_++;
        ind();
        out_ << "Condition:\n";
        indent_++;
        node.condition->accept(*this);
        indent_--;
        ind();
        out_ << "Body:\n";
        indent_++;
        node.body->accept(*this);
        indent_--;
        indent_--;
    }

    void visit(ForStmtNode& node) override {
        ind();
        out_ << "ForStmt [line " << node.line << "]:\n";
        indent_++;
        if (node.init) {
            ind();
            out_ << "Init:\n";
            indent_++;
            node.init->accept(*this);
            indent_--;
        }
        if (node.condition) {
            ind();
            out_ << "Condition:\n";
            indent_++;
            node.condition->accept(*this);
            indent_--;
        }
        if (node.update) {
            ind();
            out_ << "Update:\n";
            indent_++;
            node.update->accept(*this);
            indent_--;
        }
        ind();
        out_ << "Body:\n";
        indent_++;
        node.body->accept(*this);
        indent_--;
        indent_--;
    }

    void visit(ReturnStmtNode& node) override {
        ind();
        out_ << "Return";
        if (node.value) {
            out_ << ": ";
            expr_inline_ = true;
            node.value->accept(*this);
            expr_inline_ = false;
        }
        out_ << " [line " << node.line << "]\n";
    }

    void visit(LiteralExprNode& node) override {
        if (expr_inline_) {
            out_ << node.raw;
            return;
        }
        ind();
        out_ << "Literal: " << node.raw << "\n";
    }

    void visit(IdentifierExprNode& node) override {
        if (expr_inline_) {
            out_ << node.name;
            return;
        }
        ind();
        out_ << "Identifier: " << node.name << "\n";
    }

    void visit(BinaryExprNode& node) override {
        if (expr_inline_) {
            node.left->accept(*this);
            out_ << " " << node.op << " ";
            node.right->accept(*this);
            return;
        }
        ind();
        out_ << "Binary: " << node.op << "\n";
        indent_++;
        node.left->accept(*this);
        node.right->accept(*this);
        indent_--;
    }

    void visit(UnaryExprNode& node) override {
        if (expr_inline_) {
            out_ << node.op;
            node.operand->accept(*this);
            return;
        }
        ind();
        out_ << "Unary: " << node.op << "\n";
        indent_++;
        node.operand->accept(*this);
        indent_--;
    }

    void visit(CallExprNode& node) override {
        if (expr_inline_) {
            out_ << node.callee << "(";
            for (std::size_t i = 0; i < node.arguments.size(); ++i) {
                if (i > 0) out_ << ", ";
                node.arguments[i]->accept(*this);
            }
            out_ << ")";
            return;
        }
        ind();
        out_ << "Call: " << node.callee << "\n";
        indent_++;
        for (auto& a : node.arguments) {
            a->accept(*this);
        }
        indent_--;
    }

    void visit(AssignmentExprNode& node) override {
        if (expr_inline_) {
            node.target->accept(*this);
            out_ << " " << node.op << " ";
            node.value->accept(*this);
            return;
        }
        ind();
        out_ << "Assignment: " << node.op << "\n";
        indent_++;
        node.target->accept(*this);
        node.value->accept(*this);
        indent_--;
    }

private:
    std::ostringstream out_;
    int indent_ = 0;
    bool expr_inline_ = false;

    void ind() {
        for (int i = 0; i < indent_; ++i) out_ << "  ";
    }
};

class ASTDotPrinter : public ASTVisitor {
public:
    std::string result() {
        return "digraph AST {\n  node [shape=box, fontname=\"Courier\"];\n" +
               out_.str() + "}\n";
    }

    void visit(ProgramNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"Program\", style=filled, fillcolor=\"#e0e0ff\"];\n";
        for (auto& d : node.declarations) {
            int child = peek_id();
            d->accept(*this);
            out_ << "  n" << id << " -> n" << child << ";\n";
        }
    }

    void visit(FunctionDeclNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"FunctionDecl: " << node.name
             << " -> " << node.return_type
             << "\", style=filled, fillcolor=\"#ffe0e0\"];\n";
        int body_id = peek_id();
        node.body->accept(*this);
        out_ << "  n" << id << " -> n" << body_id << ";\n";
    }

    void visit(StructDeclNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"StructDecl: " << node.name
             << "\", style=filled, fillcolor=\"#ffe0e0\"];\n";
        for (auto& f : node.fields) {
            int child = peek_id();
            f->accept(*this);
            out_ << "  n" << id << " -> n" << child << ";\n";
        }
    }

    void visit(VarDeclStmtNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"VarDecl: " << node.type_name << " "
             << node.name << "\", style=filled, fillcolor=\"#e0ffe0\"];\n";
        if (node.initializer) {
            int child = peek_id();
            node.initializer->accept(*this);
            out_ << "  n" << id << " -> n" << child << ";\n";
        }
    }

    void visit(BlockStmtNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"Block\", style=filled, fillcolor=\"#e0ffe0\"];\n";
        for (auto& s : node.statements) {
            int child = peek_id();
            s->accept(*this);
            out_ << "  n" << id << " -> n" << child << ";\n";
        }
    }

    void visit(ExprStmtNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"ExprStmt\", style=filled, fillcolor=\"#e0ffe0\"];\n";
        if (node.expression) {
            int child = peek_id();
            node.expression->accept(*this);
            out_ << "  n" << id << " -> n" << child << ";\n";
        }
    }

    void visit(IfStmtNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"IfStmt\", style=filled, fillcolor=\"#e0ffe0\"];\n";
        int cond = peek_id();
        node.condition->accept(*this);
        out_ << "  n" << id << " -> n" << cond << " [label=\"cond\"];\n";
        int then_id = peek_id();
        node.then_branch->accept(*this);
        out_ << "  n" << id << " -> n" << then_id << " [label=\"then\"];\n";
        if (node.else_branch) {
            int else_id = peek_id();
            node.else_branch->accept(*this);
            out_ << "  n" << id << " -> n" << else_id << " [label=\"else\"];\n";
        }
    }

    void visit(WhileStmtNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"WhileStmt\", style=filled, fillcolor=\"#e0ffe0\"];\n";
        int cond = peek_id();
        node.condition->accept(*this);
        out_ << "  n" << id << " -> n" << cond << " [label=\"cond\"];\n";
        int body = peek_id();
        node.body->accept(*this);
        out_ << "  n" << id << " -> n" << body << " [label=\"body\"];\n";
    }

    void visit(ForStmtNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"ForStmt\", style=filled, fillcolor=\"#e0ffe0\"];\n";
        if (node.init) {
            int child = peek_id();
            node.init->accept(*this);
            out_ << "  n" << id << " -> n" << child << " [label=\"init\"];\n";
        }
        if (node.condition) {
            int child = peek_id();
            node.condition->accept(*this);
            out_ << "  n" << id << " -> n" << child << " [label=\"cond\"];\n";
        }
        if (node.update) {
            int child = peek_id();
            node.update->accept(*this);
            out_ << "  n" << id << " -> n" << child << " [label=\"update\"];\n";
        }
        int body = peek_id();
        node.body->accept(*this);
        out_ << "  n" << id << " -> n" << body << " [label=\"body\"];\n";
    }

    void visit(ReturnStmtNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"Return\", style=filled, fillcolor=\"#e0ffe0\"];\n";
        if (node.value) {
            int child = peek_id();
            node.value->accept(*this);
            out_ << "  n" << id << " -> n" << child << ";\n";
        }
    }

    void visit(LiteralExprNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"Literal: " << node.raw
             << "\", style=filled, fillcolor=\"#ffffc0\"];\n";
    }

    void visit(IdentifierExprNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"Identifier: " << node.name
             << "\", style=filled, fillcolor=\"#ffffc0\"];\n";
    }

    void visit(BinaryExprNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"Binary: " << node.op
             << "\", style=filled, fillcolor=\"#ffffc0\"];\n";
        int left = peek_id();
        node.left->accept(*this);
        out_ << "  n" << id << " -> n" << left << " [label=\"left\"];\n";
        int right = peek_id();
        node.right->accept(*this);
        out_ << "  n" << id << " -> n" << right << " [label=\"right\"];\n";
    }

    void visit(UnaryExprNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"Unary: " << node.op
             << "\", style=filled, fillcolor=\"#ffffc0\"];\n";
        int child = peek_id();
        node.operand->accept(*this);
        out_ << "  n" << id << " -> n" << child << ";\n";
    }

    void visit(CallExprNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"Call: " << node.callee
             << "\", style=filled, fillcolor=\"#ffffc0\"];\n";
        for (auto& a : node.arguments) {
            int child = peek_id();
            a->accept(*this);
            out_ << "  n" << id << " -> n" << child << ";\n";
        }
    }

    void visit(AssignmentExprNode& node) override {
        int id = next_id();
        out_ << "  n" << id << " [label=\"Assignment: " << node.op
             << "\", style=filled, fillcolor=\"#ffffc0\"];\n";
        int target = peek_id();
        node.target->accept(*this);
        out_ << "  n" << id << " -> n" << target << " [label=\"target\"];\n";
        int value = peek_id();
        node.value->accept(*this);
        out_ << "  n" << id << " -> n" << value << " [label=\"value\"];\n";
    }

private:
    std::ostringstream out_;
    int id_counter_ = 0;
    int next_id() { return id_counter_++; }
    int peek_id() const { return id_counter_; }
};

class ASTJsonPrinter : public ASTVisitor {
public:
    std::string result() const { return out_.str(); }

    void visit(ProgramNode& node) override {
        out_ << "{\"type\":\"Program\",\"line\":" << node.line
             << ",\"declarations\":[";
        for (std::size_t i = 0; i < node.declarations.size(); ++i) {
            if (i > 0) out_ << ",";
            node.declarations[i]->accept(*this);
        }
        out_ << "]}";
    }

    void visit(FunctionDeclNode& node) override {
        out_ << "{\"type\":\"FunctionDecl\",\"name\":\"" << node.name
             << "\",\"return_type\":\"" << node.return_type
             << "\",\"line\":" << node.line << ",\"parameters\":[";
        for (std::size_t i = 0; i < node.parameters.size(); ++i) {
            if (i > 0) out_ << ",";
            out_ << "{\"type_name\":\"" << node.parameters[i].type_name
                 << "\",\"name\":\"" << node.parameters[i].name << "\"}";
        }
        out_ << "],\"body\":";
        node.body->accept(*this);
        out_ << "}";
    }

    void visit(StructDeclNode& node) override {
        out_ << "{\"type\":\"StructDecl\",\"name\":\"" << node.name
             << "\",\"line\":" << node.line << ",\"fields\":[";
        for (std::size_t i = 0; i < node.fields.size(); ++i) {
            if (i > 0) out_ << ",";
            node.fields[i]->accept(*this);
        }
        out_ << "]}";
    }

    void visit(VarDeclStmtNode& node) override {
        out_ << "{\"type\":\"VarDecl\",\"type_name\":\"" << node.type_name
             << "\",\"name\":\"" << node.name << "\",\"line\":" << node.line;
        if (node.initializer) {
            out_ << ",\"initializer\":";
            node.initializer->accept(*this);
        }
        out_ << "}";
    }

    void visit(BlockStmtNode& node) override {
        out_ << "{\"type\":\"Block\",\"line\":" << node.line
             << ",\"statements\":[";
        for (std::size_t i = 0; i < node.statements.size(); ++i) {
            if (i > 0) out_ << ",";
            node.statements[i]->accept(*this);
        }
        out_ << "]}";
    }

    void visit(ExprStmtNode& node) override {
        out_ << "{\"type\":\"ExprStmt\",\"line\":" << node.line;
        if (node.expression) {
            out_ << ",\"expression\":";
            node.expression->accept(*this);
        }
        out_ << "}";
    }

    void visit(IfStmtNode& node) override {
        out_ << "{\"type\":\"IfStmt\",\"line\":" << node.line
             << ",\"condition\":";
        node.condition->accept(*this);
        out_ << ",\"then\":";
        node.then_branch->accept(*this);
        if (node.else_branch) {
            out_ << ",\"else\":";
            node.else_branch->accept(*this);
        }
        out_ << "}";
    }

    void visit(WhileStmtNode& node) override {
        out_ << "{\"type\":\"WhileStmt\",\"line\":" << node.line
             << ",\"condition\":";
        node.condition->accept(*this);
        out_ << ",\"body\":";
        node.body->accept(*this);
        out_ << "}";
    }

    void visit(ForStmtNode& node) override {
        out_ << "{\"type\":\"ForStmt\",\"line\":" << node.line;
        if (node.init) {
            out_ << ",\"init\":";
            node.init->accept(*this);
        }
        if (node.condition) {
            out_ << ",\"condition\":";
            node.condition->accept(*this);
        }
        if (node.update) {
            out_ << ",\"update\":";
            node.update->accept(*this);
        }
        out_ << ",\"body\":";
        node.body->accept(*this);
        out_ << "}";
    }

    void visit(ReturnStmtNode& node) override {
        out_ << "{\"type\":\"Return\",\"line\":" << node.line;
        if (node.value) {
            out_ << ",\"value\":";
            node.value->accept(*this);
        }
        out_ << "}";
    }

    void visit(LiteralExprNode& node) override {
        out_ << "{\"type\":\"Literal\",\"raw\":\"" << node.raw
             << "\",\"line\":" << node.line << "}";
    }

    void visit(IdentifierExprNode& node) override {
        out_ << "{\"type\":\"Identifier\",\"name\":\"" << node.name
             << "\",\"line\":" << node.line << "}";
    }

    void visit(BinaryExprNode& node) override {
        out_ << "{\"type\":\"Binary\",\"op\":\"" << node.op
             << "\",\"line\":" << node.line << ",\"left\":";
        node.left->accept(*this);
        out_ << ",\"right\":";
        node.right->accept(*this);
        out_ << "}";
    }

    void visit(UnaryExprNode& node) override {
        out_ << "{\"type\":\"Unary\",\"op\":\"" << node.op
             << "\",\"line\":" << node.line << ",\"operand\":";
        node.operand->accept(*this);
        out_ << "}";
    }

    void visit(CallExprNode& node) override {
        out_ << "{\"type\":\"Call\",\"callee\":\"" << node.callee
             << "\",\"line\":" << node.line << ",\"arguments\":[";
        for (std::size_t i = 0; i < node.arguments.size(); ++i) {
            if (i > 0) out_ << ",";
            node.arguments[i]->accept(*this);
        }
        out_ << "]}";
    }

    void visit(AssignmentExprNode& node) override {
        out_ << "{\"type\":\"Assignment\",\"op\":\"" << node.op
             << "\",\"line\":" << node.line << ",\"target\":";
        node.target->accept(*this);
        out_ << ",\"value\":";
        node.value->accept(*this);
        out_ << "}";
    }

private:
    std::ostringstream out_;
};
