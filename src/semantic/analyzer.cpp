#include "semantic/analyzer.h"

#include <algorithm>
#include <cmath>
#include <sstream>

// ---------------------------------------------------------------
// Constructor
// ---------------------------------------------------------------
SemanticAnalyzer::SemanticAnalyzer() = default;

// ---------------------------------------------------------------
// Error helper
// ---------------------------------------------------------------
void SemanticAnalyzer::error(SemanticErrorKind kind, int line, int col,
                             const std::string& msg,
                             const std::string& expected,
                             const std::string& actual,
                             const std::string& suggestion) {
    SemanticError err;
    err.kind = kind;
    err.message = msg;
    err.filename = current_filename_;
    err.line = line;
    err.column = col;
    if (!current_function_.empty())
        err.context = "в функции '" + current_function_ + "'";
    err.expected_type = expected;
    err.actual_type = actual;
    err.suggestion = suggestion;
    errors_.push_back(err);
}

// ---------------------------------------------------------------
// Type resolution
// ---------------------------------------------------------------
Type* SemanticAnalyzer::resolve_type_name(const std::string& name,
                                          int line, int col) {
    Type* t = types_.resolve(name);
    if (!t) {
        error(SemanticErrorKind::UnknownType, line, col,
              "неизвестный тип '" + name + "'");
        return types_.type_error();
    }
    return t;
}

// ---------------------------------------------------------------
// Find similar name (Levenshtein-like, simple)
// ---------------------------------------------------------------
static int edit_distance(const std::string& a, const std::string& b) {
    int n = static_cast<int>(a.size());
    int m = static_cast<int>(b.size());
    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
    for (int i = 0; i <= n; ++i) dp[i][0] = i;
    for (int j = 0; j <= m; ++j) dp[0][j] = j;
    for (int i = 1; i <= n; ++i)
        for (int j = 1; j <= m; ++j) {
            int cost = (a[i-1] == b[j-1]) ? 0 : 1;
            dp[i][j] = std::min({dp[i-1][j] + 1, dp[i][j-1] + 1,
                                 dp[i-1][j-1] + cost});
        }
    return dp[n][m];
}

std::string SemanticAnalyzer::find_similar(const std::string& name) const {
    std::string best;
    int best_dist = 999;
    for (const auto& scope : sym_.scopes()) {
        for (const auto& [sname, _] : scope.symbols) {
            (void)_;
            int d = edit_distance(name, sname);
            if (d < best_dist && d <= 2 && sname != name) {
                best_dist = d;
                best = sname;
            }
        }
    }
    return best.empty() ? "" : "возможно, вы имели в виду '" + best + "'?";
}

// ---------------------------------------------------------------
// Pass 1 — collect top-level declarations (forward refs)
// ---------------------------------------------------------------
void SemanticAnalyzer::collect_declarations(ProgramNode& ast) {
    for (auto& decl : ast.declarations) {
        if (auto* fn = dynamic_cast<FunctionDeclNode*>(decl.get())) {
            // Resolve return type
            std::string ret_name = fn->return_type.empty() ? "void" : fn->return_type;

            // Build param list
            std::vector<FunctionParam> params;
            for (const auto& p : fn->parameters)
                params.push_back(FunctionParam{p.name, p.type_name});

            // Register function type
            Type* fn_type = types_.register_function(
                fn->name, params, ret_name);

            // Insert into symbol table
            Symbol sym;
            sym.name = fn->name;
            sym.type = fn_type;
            sym.kind = SymbolKind::Function;
            sym.decl_line = fn->line;
            sym.decl_column = fn->column;
            sym.params = params;
            sym.return_type_name = ret_name;

            if (!sym_.insert(sym)) {
                error(SemanticErrorKind::DuplicateDeclaration,
                      fn->line, fn->column,
                      "повторное объявление функции '" + fn->name + "'");
            }
        } else if (auto* st = dynamic_cast<StructDeclNode*>(decl.get())) {
            // Collect field info
            std::vector<StructField> fields;
            for (const auto& f : st->fields) {
                fields.push_back(StructField{f->name, f->type_name,
                                             f->line, f->column});
            }

            // Register struct type
            types_.register_struct(st->name, fields);

            // Insert into symbol table
            Symbol sym;
            sym.name = st->name;
            sym.type = types_.resolve(st->name);
            sym.kind = SymbolKind::Struct;
            sym.decl_line = st->line;
            sym.decl_column = st->column;
            sym.fields = fields;

            if (!sym_.insert(sym)) {
                error(SemanticErrorKind::DuplicateDeclaration,
                      st->line, st->column,
                      "повторное объявление структуры '" + st->name + "'");
            }
        }
    }
}

// ---------------------------------------------------------------
// analyze() — main entry point
// ---------------------------------------------------------------
void SemanticAnalyzer::analyze(ProgramNode& ast) {
    errors_.clear();
    // Pass 1: collect forward declarations
    collect_declarations(ast);
    // Pass 2: full traversal
    ast.accept(*this);
}

// ---------------------------------------------------------------
// ProgramNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(ProgramNode& node) {
    for (auto& d : node.declarations) {
        d->accept(*this);
    }
}

// ---------------------------------------------------------------
// FunctionDeclNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(FunctionDeclNode& node) {
    std::string prev_fn = current_function_;
    current_function_ = node.name;

    std::string ret_name = node.return_type.empty() ? "void" : node.return_type;
    Type* ret_type = resolve_type_name(ret_name, node.line, node.column);
    Type* prev_ret = current_return_type_;
    current_return_type_ = ret_type;

    sym_.enter_scope("function:" + node.name);

    // Insert parameters
    for (const auto& p : node.parameters) {
        Type* pt = resolve_type_name(p.type_name, p.line, p.column);
        if (pt->is_void()) {
            error(SemanticErrorKind::TypeMismatch, p.line, p.column,
                  "параметр '" + p.name + "' не может иметь тип void");
        }
        Symbol sym;
        sym.name = p.name;
        sym.type = pt;
        sym.kind = SymbolKind::Parameter;
        sym.decl_line = p.line;
        sym.decl_column = p.column;
        sym.initialized = true;
        if (!sym_.insert(sym)) {
            error(SemanticErrorKind::DuplicateDeclaration,
                  p.line, p.column,
                  "повторное объявление параметра '" + p.name + "'");
        }
    }

    // Validate struct fields for duplicate names at declaration time
    // (this is done during collect_declarations for structs)

    // Visit body
    if (node.body) {
        // Visit body statements directly (don't push another scope for the body block)
        for (auto& s : node.body->statements) {
            s->accept(*this);
        }
    }

    sym_.exit_scope();
    current_return_type_ = prev_ret;
    current_function_ = prev_fn;
}

// ---------------------------------------------------------------
// StructDeclNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(StructDeclNode& node) {
    sym_.enter_scope("struct:" + node.name);

    // Check for duplicate field names and valid types
    std::unordered_map<std::string, int> field_names;
    for (const auto& f : node.fields) {
        Type* ft = resolve_type_name(f->type_name, f->line, f->column);
        if (ft->is_void()) {
            error(SemanticErrorKind::TypeMismatch, f->line, f->column,
                  "поле структуры '" + f->name + "' не может иметь тип void");
        }
        if (field_names.count(f->name)) {
            error(SemanticErrorKind::DuplicateDeclaration,
                  f->line, f->column,
                  "дублирующееся поле '" + f->name + "' в структуре '" + node.name + "'");
        } else {
            field_names[f->name] = f->line;
            Symbol sym;
            sym.name = f->name;
            sym.type = ft;
            sym.kind = SymbolKind::Variable;
            sym.decl_line = f->line;
            sym.decl_column = f->column;
            sym_.insert(sym);
        }
    }

    sym_.exit_scope();
}

// ---------------------------------------------------------------
// VarDeclStmtNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(VarDeclStmtNode& node) {
    Type* var_type = resolve_type_name(node.type_name, node.line, node.column);

    if (var_type->is_void()) {
        error(SemanticErrorKind::TypeMismatch, node.line, node.column,
              "переменная '" + node.name + "' не может иметь тип void");
    }

    // Check initializer
    if (node.initializer) {
        node.initializer->accept(*this);
        Type* init_type = last_expr_type_;

        if (init_type && !init_type->is_error() && !var_type->is_error()) {
            if (!types_.is_compatible(init_type, var_type)) {
                error(SemanticErrorKind::TypeMismatch,
                      node.line, node.column,
                      "невозможно инициализировать '" + node.name + "' типа '" +
                      var_type->name + "' значением типа '" +
                      init_type->name + "'",
                      var_type->name, init_type->name);
            }
        }
    }

    Symbol sym;
    sym.name = node.name;
    sym.type = var_type;
    sym.kind = SymbolKind::Variable;
    sym.decl_line = node.line;
    sym.decl_column = node.column;
    sym.initialized = (node.initializer != nullptr);

    if (!sym_.insert(sym)) {
        error(SemanticErrorKind::DuplicateDeclaration,
              node.line, node.column,
              "повторное объявление переменной '" + node.name + "' в текущей области видимости");
    }
}

// ---------------------------------------------------------------
// BlockStmtNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(BlockStmtNode& node) {
    sym_.enter_scope("block");
    for (auto& s : node.statements) {
        s->accept(*this);
    }
    sym_.exit_scope();
}

// ---------------------------------------------------------------
// ExprStmtNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(ExprStmtNode& node) {
    if (node.expression) {
        node.expression->accept(*this);
    }
}

// ---------------------------------------------------------------
// IfStmtNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(IfStmtNode& node) {
    if (node.condition) {
        node.condition->accept(*this);
        Type* cond_type = last_expr_type_;
        if (cond_type && !cond_type->is_error()) {
            // Condition must be bool or numeric (implicit to-bool)
            if (!cond_type->is_bool() && !cond_type->is_numeric()) {
                error(SemanticErrorKind::InvalidConditionType,
                      node.line, node.column,
                      "условие в 'if' должно иметь логический или числовой тип",
                      "bool", cond_type->name);
            }
        }
    }
    if (node.then_branch) node.then_branch->accept(*this);
    if (node.else_branch) node.else_branch->accept(*this);
}

// ---------------------------------------------------------------
// WhileStmtNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(WhileStmtNode& node) {
    if (node.condition) {
        node.condition->accept(*this);
        Type* cond_type = last_expr_type_;
        if (cond_type && !cond_type->is_error()) {
            if (!cond_type->is_bool() && !cond_type->is_numeric()) {
                error(SemanticErrorKind::InvalidConditionType,
                      node.line, node.column,
                      "условие в 'while' должно иметь логический или числовой тип",
                      "bool", cond_type->name);
            }
        }
    }
    loop_depth_++;
    if (node.body) node.body->accept(*this);
    loop_depth_--;
}

// ---------------------------------------------------------------
// ForStmtNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(ForStmtNode& node) {
    sym_.enter_scope("for");

    if (node.init) node.init->accept(*this);

    if (node.condition) {
        node.condition->accept(*this);
        Type* cond_type = last_expr_type_;
        if (cond_type && !cond_type->is_error()) {
            if (!cond_type->is_bool() && !cond_type->is_numeric()) {
                error(SemanticErrorKind::InvalidConditionType,
                      node.line, node.column,
                      "условие в 'for' должно иметь логический или числовой тип",
                      "bool", cond_type->name);
            }
        }
    }
    if (node.update) node.update->accept(*this);

    loop_depth_++;
    if (node.body) node.body->accept(*this);
    loop_depth_--;

    sym_.exit_scope();
}

// ---------------------------------------------------------------
// ReturnStmtNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(ReturnStmtNode& node) {
    if (node.value) {
        node.value->accept(*this);
        Type* val_type = last_expr_type_;

        if (current_return_type_ && current_return_type_->is_void()) {
            error(SemanticErrorKind::InvalidReturnType,
                  node.line, node.column,
                  "void-функция '" + current_function_ +
                  "' не должна возвращать значение",
                  "void", val_type ? val_type->name : "?");
        } else if (val_type && current_return_type_ &&
                   !val_type->is_error() && !current_return_type_->is_error()) {
            if (!types_.is_compatible(val_type, current_return_type_)) {
                error(SemanticErrorKind::InvalidReturnType,
                      node.line, node.column,
                      "несовпадение типа возврата в функции '" +
                      current_function_ + "'",
                      current_return_type_->name, val_type->name);
            }
        }
    } else {
        // Empty return — OK for void functions
        if (current_return_type_ && !current_return_type_->is_void() &&
            !current_return_type_->is_error()) {
            error(SemanticErrorKind::InvalidReturnType,
                  node.line, node.column,
                  "не-void функция '" + current_function_ +
                  "' должна возвращать значение",
                  current_return_type_->name, "void");
        }
    }
}

// ---------------------------------------------------------------
// LiteralExprNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(LiteralExprNode& node) {
    switch (node.kind) {
        case LiteralExprNode::Kind::Integer:
            last_expr_type_ = types_.type_int();
            node.resolved_type = "int";
            break;
        case LiteralExprNode::Kind::Float:
            last_expr_type_ = types_.type_float();
            node.resolved_type = "float";
            break;
        case LiteralExprNode::Kind::Bool:
            last_expr_type_ = types_.type_bool();
            node.resolved_type = "bool";
            break;
        case LiteralExprNode::Kind::String:
            last_expr_type_ = types_.type_string();
            node.resolved_type = "string";
            break;
    }
}

// ---------------------------------------------------------------
// IdentifierExprNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(IdentifierExprNode& node) {
    Symbol* sym = sym_.lookup(node.name);
    if (!sym) {
        std::string suggestion = find_similar(node.name);
        error(SemanticErrorKind::UndeclaredIdentifier,
              node.line, node.column,
              "необъявленная переменная '" + node.name + "'",
              "", "", suggestion);
        last_expr_type_ = types_.type_error();
        node.resolved_type = "<error>";
        return;
    }

    // Check for use of function/struct name as variable
    if (sym->kind == SymbolKind::Function || sym->kind == SymbolKind::Struct) {
        // Allow function name lookup (for calls), set type
        last_expr_type_ = sym->type;
        node.resolved_type = sym->type ? sym->type->name : "<error>";
        return;
    }

    last_expr_type_ = sym->type;
    node.resolved_type = sym->type ? sym->type->name : "<error>";
}

// ---------------------------------------------------------------
// BinaryExprNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(BinaryExprNode& node) {
    node.left->accept(*this);
    Type* left_type = last_expr_type_;

    node.right->accept(*this);
    Type* right_type = last_expr_type_;

    if (!left_type || !right_type ||
        left_type->is_error() || right_type->is_error()) {
        last_expr_type_ = types_.type_error();
        node.resolved_type = "<error>";
        return;
    }

    const std::string& op = node.op;

    // Arithmetic: +, -, *, /, %
    if (op == "+" || op == "-" || op == "*" || op == "/" || op == "%") {
        if (!left_type->is_numeric() || !right_type->is_numeric()) {
            error(SemanticErrorKind::TypeMismatch,
                  node.line, node.column,
                  "бинарный оператор '" + op + "' требует числовых операндов",
                  "numeric", left_type->name + " and " + right_type->name);
            last_expr_type_ = types_.type_error();
            node.resolved_type = "<error>";
            return;
        }
        if (op == "%" && (left_type->kind == TypeKind::Float ||
                          right_type->kind == TypeKind::Float)) {
            error(SemanticErrorKind::TypeMismatch,
                  node.line, node.column,
                  "оператор '%' требует целочисленных операндов",
                  "int", left_type->name + " and " + right_type->name);
            last_expr_type_ = types_.type_error();
            node.resolved_type = "<error>";
            return;
        }
        Type* result = types_.common_numeric_type(left_type, right_type);
        last_expr_type_ = result;
        node.resolved_type = result->name;
        return;
    }

    // Comparison: ==, !=, <, <=, >, >=
    if (op == "==" || op == "!=" || op == "<" || op == "<=" ||
        op == ">"  || op == ">=") {
        if (!left_type->is_numeric() || !right_type->is_numeric()) {
            // Allow bool == bool, bool != bool
            if ((op == "==" || op == "!=") &&
                left_type->is_bool() && right_type->is_bool()) {
                last_expr_type_ = types_.type_bool();
                node.resolved_type = "bool";
                return;
            }
            error(SemanticErrorKind::TypeMismatch,
                  node.line, node.column,
                  "сравнение '" + op + "' требует числовых операндов",
                  "numeric", left_type->name + " and " + right_type->name);
            last_expr_type_ = types_.type_error();
            node.resolved_type = "<error>";
            return;
        }
        last_expr_type_ = types_.type_bool();
        node.resolved_type = "bool";
        return;
    }

    // Logical: &&, ||
    if (op == "&&" || op == "||") {
        if (!left_type->is_bool() || !right_type->is_bool()) {
            error(SemanticErrorKind::TypeMismatch,
                  node.line, node.column,
                  "логический оператор '" + op + "' требует логических операндов",
                  "bool", left_type->name + " and " + right_type->name);
            last_expr_type_ = types_.type_error();
            node.resolved_type = "<error>";
            return;
        }
        last_expr_type_ = types_.type_bool();
        node.resolved_type = "bool";
        return;
    }

    // Unknown operator
    error(SemanticErrorKind::InvalidOperator,
          node.line, node.column,
          "неизвестный бинарный оператор '" + op + "'");
    last_expr_type_ = types_.type_error();
    node.resolved_type = "<error>";
}

// ---------------------------------------------------------------
// UnaryExprNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(UnaryExprNode& node) {
    node.operand->accept(*this);
    Type* operand_type = last_expr_type_;

    if (!operand_type || operand_type->is_error()) {
        last_expr_type_ = types_.type_error();
        node.resolved_type = "<error>";
        return;
    }

    const std::string& op = node.op;

    if (op == "-") {
        if (!operand_type->is_numeric()) {
            error(SemanticErrorKind::TypeMismatch,
                  node.line, node.column,
                  "унарный '-' требует числового операнда",
                  "numeric", operand_type->name);
            last_expr_type_ = types_.type_error();
            node.resolved_type = "<error>";
            return;
        }
        last_expr_type_ = operand_type;
        node.resolved_type = operand_type->name;
        return;
    }

    if (op == "!") {
        if (!operand_type->is_bool()) {
            error(SemanticErrorKind::TypeMismatch,
                  node.line, node.column,
                  "унарный '!' требует логического операнда",
                  "bool", operand_type->name);
            last_expr_type_ = types_.type_error();
            node.resolved_type = "<error>";
            return;
        }
        last_expr_type_ = types_.type_bool();
        node.resolved_type = "bool";
        return;
    }

    // ++ / -- (prefix)
    if (op == "++" || op == "--") {
        if (!operand_type->is_numeric()) {
            error(SemanticErrorKind::TypeMismatch,
                  node.line, node.column,
                  "префиксный '" + op + "' требует числового операнда",
                  "numeric", operand_type->name);
            last_expr_type_ = types_.type_error();
            node.resolved_type = "<error>";
            return;
        }
        last_expr_type_ = operand_type;
        node.resolved_type = operand_type->name;
        return;
    }

    error(SemanticErrorKind::InvalidOperator,
          node.line, node.column,
          "неизвестный унарный оператор '" + op + "'");
    last_expr_type_ = types_.type_error();
    node.resolved_type = "<error>";
}

// ---------------------------------------------------------------
// PostfixExprNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(PostfixExprNode& node) {
    node.operand->accept(*this);
    Type* operand_type = last_expr_type_;

    if (!operand_type || operand_type->is_error()) {
        last_expr_type_ = types_.type_error();
        node.resolved_type = "<error>";
        return;
    }

    if (!operand_type->is_numeric()) {
        error(SemanticErrorKind::TypeMismatch,
              node.line, node.column,
              "постфиксный '" + node.op + "' требует числового операнда",
              "numeric", operand_type->name);
        last_expr_type_ = types_.type_error();
        node.resolved_type = "<error>";
        return;
    }

    last_expr_type_ = operand_type;
    node.resolved_type = operand_type->name;
}

// ---------------------------------------------------------------
// CallExprNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(CallExprNode& node) {
    Symbol* fn_sym = sym_.lookup(node.callee);
    if (!fn_sym) {
        std::string suggestion = find_similar(node.callee);
        error(SemanticErrorKind::UndeclaredIdentifier,
              node.line, node.column,
              "необъявленная функция '" + node.callee + "'",
              "", "", suggestion);
        // Still type-check arguments
        for (auto& a : node.arguments) a->accept(*this);
        last_expr_type_ = types_.type_error();
        node.resolved_type = "<error>";
        return;
    }

    if (fn_sym->kind != SymbolKind::Function) {
        error(SemanticErrorKind::TypeMismatch,
              node.line, node.column,
              "'" + node.callee + "' не является функцией",
              "function", symbol_kind_str(fn_sym->kind));
        for (auto& a : node.arguments) a->accept(*this);
        last_expr_type_ = types_.type_error();
        node.resolved_type = "<error>";
        return;
    }

    // Check argument count
    size_t expected = fn_sym->params.size();
    size_t actual = node.arguments.size();
    if (expected != actual) {
        std::string sig = node.callee + "(";
        for (size_t i = 0; i < fn_sym->params.size(); ++i) {
            if (i > 0) sig += ", ";
            sig += fn_sym->params[i].type_name;
        }
        sig += ") -> " + fn_sym->return_type_name;

        error(SemanticErrorKind::ArgCountMismatch,
              node.line, node.column,
              "функция '" + node.callee + "' ожидает " +
              std::to_string(expected) + " аргумент(ов), получено " +
              std::to_string(actual),
              std::to_string(expected) + " аргументов",
              std::to_string(actual) + " аргументов",
              "сигнатура функции: " + sig);
    }

    // Check argument types
    size_t check_count = std::min(expected, actual);
    for (size_t i = 0; i < node.arguments.size(); ++i) {
        node.arguments[i]->accept(*this);
        Type* arg_type = last_expr_type_;

        if (i < check_count && arg_type && !arg_type->is_error()) {
            Type* param_type = types_.resolve(fn_sym->params[i].type_name);
            if (param_type && !types_.is_compatible(arg_type, param_type)) {
                error(SemanticErrorKind::ArgTypeMismatch,
                      node.arguments[i]->line, node.arguments[i]->column,
                      "аргумент " + std::to_string(i + 1) + " функции '" +
                      node.callee + "': ожидалось '" + param_type->name +
                      "', получено '" + arg_type->name + "'",
                      param_type->name, arg_type->name);
            }
        }
    }

    // Result type = return type
    Type* ret = types_.resolve(fn_sym->return_type_name);
    last_expr_type_ = ret ? ret : types_.type_error();
    node.resolved_type = last_expr_type_->name;
}

// ---------------------------------------------------------------
// AssignmentExprNode
// ---------------------------------------------------------------
void SemanticAnalyzer::visit(AssignmentExprNode& node) {
    // Check target is a valid lvalue
    auto* ident = dynamic_cast<IdentifierExprNode*>(node.target.get());
    if (!ident) {
        error(SemanticErrorKind::InvalidAssignmentTarget,
              node.line, node.column,
              "левая часть присваивания не является переменной");
        // Still visit both sides
        node.target->accept(*this);
        node.value->accept(*this);
        last_expr_type_ = types_.type_error();
        node.resolved_type = "<error>";
        return;
    }

    node.target->accept(*this);
    Type* target_type = last_expr_type_;

    node.value->accept(*this);
    Type* value_type = last_expr_type_;

    if (target_type && value_type &&
        !target_type->is_error() && !value_type->is_error()) {
        // For compound assignments (+=, -=, *=, /=), check operators
        if (node.op == "+=" || node.op == "-=" ||
            node.op == "*=" || node.op == "/=") {
            if (!target_type->is_numeric() || !value_type->is_numeric()) {
                error(SemanticErrorKind::TypeMismatch,
                      node.line, node.column,
                      "составное присваивание '" + node.op +
                      "' требует числовых операндов",
                      target_type->name, value_type->name);
                last_expr_type_ = types_.type_error();
                node.resolved_type = "<error>";
                return;
            }
        } else {
            // Simple '='
            if (!types_.is_compatible(value_type, target_type)) {
                error(SemanticErrorKind::TypeMismatch,
                      node.line, node.column,
                      "невозможно присвоить значение типа '" + value_type->name +
                      "' переменной типа '" + target_type->name + "'",
                      target_type->name, value_type->name);
            }
        }
    }

    // Mark as initialized
    if (ident) {
        Symbol* sym = sym_.lookup(ident->name);
        if (sym) sym->initialized = true;
    }

    last_expr_type_ = target_type;
    node.resolved_type = target_type ? target_type->name : "<error>";
}

// ---------------------------------------------------------------
// Decorated AST dump (text)
// ---------------------------------------------------------------
class DecoratedASTPrinter : public ASTVisitor {
public:
    std::string result() const { return out_.str(); }
    DecoratedASTPrinter(SemanticSymbolTable& sym) : sym_(sym) {}

    void visit(ProgramNode& node) override {
        out_ << "Program [global scope]:\n";
        // Print symbol table summary
        out_ << "  Symbol Table:\n";
        for (const auto& scope : sym_.scopes()) {
            out_ << "    " << scope.label << " (scope #" << scope.id << "):\n";
            for (const auto& [name, s] : scope.symbols) {
                out_ << "      - " << name << ": "
                     << symbol_kind_str(s.kind);
                if (s.type) out_ << "(" << s.type->name << ")";
                out_ << " (line " << s.decl_line << ")\n";
                if (s.kind == SymbolKind::Struct) {
                    for (const auto& f : s.fields)
                        out_ << "        Field: " << f.name << ": " << f.type_name << "\n";
                }
            }
        }
        out_ << "\n";
        indent_++;
        for (auto& d : node.declarations) d->accept(*this);
        indent_--;
    }

    void visit(FunctionDeclNode& node) override {
        ind();
        std::string ret = node.return_type.empty() ? "void" : node.return_type;
        out_ << "FunctionDecl: " << node.name << " -> " << ret
             << " (line " << node.line << "):\n";
        indent_++;
        ind(); out_ << "Parameters:\n";
        indent_++;
        for (const auto& p : node.parameters) {
            ind(); out_ << "- " << p.name << ": " << p.type_name << "\n";
        }
        indent_--;
        ind(); out_ << "Body [type checked]:\n";
        indent_++;
        if (node.body) {
            for (auto& s : node.body->statements) s->accept(*this);
        }
        indent_--;
        indent_--;
    }

    void visit(StructDeclNode& node) override {
        ind();
        out_ << "StructDecl: " << node.name << " (line " << node.line << "):\n";
        indent_++;
        for (const auto& f : node.fields) {
            ind(); out_ << "- " << f->name << ": " << f->type_name << "\n";
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
            if (!node.initializer->resolved_type.empty())
                out_ << " [type: " << node.initializer->resolved_type << "]";
        }
        out_ << " (line " << node.line << ")\n";
    }

    void visit(BlockStmtNode& node) override {
        ind(); out_ << "Block:\n";
        indent_++;
        for (auto& s : node.statements) s->accept(*this);
        indent_--;
    }

    void visit(ExprStmtNode& node) override {
        if (node.expression) {
            ind(); out_ << "ExprStmt:\n";
            indent_++;
            node.expression->accept(*this);
            indent_--;
        }
    }

    void visit(IfStmtNode& node) override {
        ind(); out_ << "IfStmt (line " << node.line << "):\n";
        indent_++;
        ind(); out_ << "Condition:\n";
        indent_++; node.condition->accept(*this); indent_--;
        ind(); out_ << "Then:\n";
        indent_++; node.then_branch->accept(*this); indent_--;
        if (node.else_branch) {
            ind(); out_ << "Else:\n";
            indent_++; node.else_branch->accept(*this); indent_--;
        }
        indent_--;
    }

    void visit(WhileStmtNode& node) override {
        ind(); out_ << "WhileStmt (line " << node.line << "):\n";
        indent_++;
        ind(); out_ << "Condition:\n";
        indent_++; node.condition->accept(*this); indent_--;
        ind(); out_ << "Body:\n";
        indent_++; node.body->accept(*this); indent_--;
        indent_--;
    }

    void visit(ForStmtNode& node) override {
        ind(); out_ << "ForStmt (line " << node.line << "):\n";
        indent_++;
        if (node.init) { ind(); out_ << "Init:\n"; indent_++; node.init->accept(*this); indent_--; }
        if (node.condition) { ind(); out_ << "Condition:\n"; indent_++; node.condition->accept(*this); indent_--; }
        if (node.update) { ind(); out_ << "Update:\n"; indent_++; node.update->accept(*this); indent_--; }
        ind(); out_ << "Body:\n"; indent_++; node.body->accept(*this); indent_--;
        indent_--;
    }

    void visit(ReturnStmtNode& node) override {
        ind(); out_ << "Return";
        if (node.value) {
            out_ << ": ";
            expr_inline_ = true;
            node.value->accept(*this);
            expr_inline_ = false;
            if (!node.value->resolved_type.empty())
                out_ << " [type: " << node.value->resolved_type << "]";
        }
        out_ << " (line " << node.line << ")\n";
    }

    void visit(LiteralExprNode& node) override {
        if (expr_inline_) {
            out_ << node.raw;
            return;
        }
        ind();
        out_ << "Literal: " << node.raw;
        if (!node.resolved_type.empty())
            out_ << " [type: " << node.resolved_type << "]";
        out_ << "\n";
    }

    void visit(IdentifierExprNode& node) override {
        if (expr_inline_) {
            out_ << node.name;
            return;
        }
        ind();
        out_ << "Identifier: " << node.name;
        if (!node.resolved_type.empty())
            out_ << " [type: " << node.resolved_type << "]";
        out_ << "\n";
    }

    void visit(BinaryExprNode& node) override {
        if (expr_inline_) {
            node.left->accept(*this);
            out_ << " " << node.op << " ";
            node.right->accept(*this);
            return;
        }
        ind();
        out_ << "Binary: " << node.op;
        if (!node.resolved_type.empty())
            out_ << " [type: " << node.resolved_type << "]";
        out_ << "\n";
        indent_++; node.left->accept(*this); node.right->accept(*this); indent_--;
    }

    void visit(UnaryExprNode& node) override {
        if (expr_inline_) {
            out_ << node.op;
            node.operand->accept(*this);
            return;
        }
        ind();
        out_ << "Unary: " << node.op;
        if (!node.resolved_type.empty())
            out_ << " [type: " << node.resolved_type << "]";
        out_ << "\n";
        indent_++; node.operand->accept(*this); indent_--;
    }

    void visit(CallExprNode& node) override {
        if (expr_inline_) {
            out_ << node.callee << "(";
            for (size_t i = 0; i < node.arguments.size(); ++i) {
                if (i > 0) out_ << ", ";
                node.arguments[i]->accept(*this);
            }
            out_ << ")";
            return;
        }
        ind();
        out_ << "Call: " << node.callee;
        if (!node.resolved_type.empty())
            out_ << " [type: " << node.resolved_type << "]";
        out_ << "\n";
        indent_++;
        for (auto& a : node.arguments) a->accept(*this);
        indent_--;
    }

    void visit(PostfixExprNode& node) override {
        if (expr_inline_) {
            node.operand->accept(*this);
            out_ << node.op;
            return;
        }
        ind();
        out_ << "Postfix: " << node.op;
        if (!node.resolved_type.empty())
            out_ << " [type: " << node.resolved_type << "]";
        out_ << "\n";
        indent_++; node.operand->accept(*this); indent_--;
    }

    void visit(AssignmentExprNode& node) override {
        if (expr_inline_) {
            node.target->accept(*this);
            out_ << " " << node.op << " ";
            node.value->accept(*this);
            return;
        }
        ind();
        out_ << "Assignment: " << node.op;
        if (!node.resolved_type.empty())
            out_ << " [type: " << node.resolved_type << "]";
        out_ << "\n";
        indent_++;
        node.target->accept(*this);
        node.value->accept(*this);
        indent_--;
    }

private:
    SemanticSymbolTable& sym_;
    std::ostringstream out_;
    int indent_ = 0;
    bool expr_inline_ = false;
    void ind() { for (int i = 0; i < indent_; ++i) out_ << "  "; }
};

std::string SemanticAnalyzer::dump_decorated_ast(ProgramNode& ast) {
    DecoratedASTPrinter printer(sym_);
    ast.accept(printer);
    return printer.result();
}

// ---------------------------------------------------------------
// Validation report
// ---------------------------------------------------------------
std::string SemanticAnalyzer::validation_report() const {
    std::ostringstream out;

    // Error/warning count
    out << "=== Validation Report ===\n";
    out << "Errors: " << errors_.size() << "\n\n";

    // Symbols by scope
    out << "Declared symbols by scope:\n";
    for (const auto& scope : sym_.scopes()) {
        out << "  [" << scope.label << "] (depth " << scope.depth << "):\n";
        for (const auto& [name, s] : scope.symbols) {
            out << "    " << name << " : "
                << symbol_kind_str(s.kind);
            if (s.type) out << " (" << s.type->name << ")";
            out << "\n";
        }
    }

    // Type info
    out << "\nType hierarchy:\n";
    out << "  int (4 bytes)\n";
    out << "  float (8 bytes)\n";
    out << "  bool (1 byte)\n";
    out << "  void (0 bytes)\n";
    out << "  string (8 bytes)\n";

    // Memory layout
    out << "\nMemory layout summary:\n";
    for (const auto& scope : sym_.scopes()) {
        if (scope.next_offset > 0) {
            out << "  [" << scope.label << "]: "
                << scope.next_offset << " bytes\n";
        }
    }

    return out.str();
}
