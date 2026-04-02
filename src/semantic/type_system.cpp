#include "semantic/type_system.h"

// ---------------------------------------------------------------
// TypeRegistry
// ---------------------------------------------------------------

TypeRegistry::TypeRegistry() {
    int_    = make(TypeKind::Int,    "int",    4);
    float_  = make(TypeKind::Float,  "float",  8);
    bool_   = make(TypeKind::Bool,   "bool",   1);
    void_   = make(TypeKind::Void,   "void",   0);
    string_ = make(TypeKind::String, "string", 8);
    error_  = make(TypeKind::Error,  "<error>", 0);
}

Type* TypeRegistry::make(TypeKind k, const std::string& n, int sz) {
    auto t = std::make_unique<Type>(k, n, sz);
    Type* ptr = t.get();
    owned_.push_back(std::move(t));
    named_types_[n] = ptr;
    return ptr;
}

Type* TypeRegistry::resolve(const std::string& name) const {
    if (name.empty()) return void_;
    auto it = named_types_.find(name);
    if (it != named_types_.end()) return it->second;
    return nullptr;
}

Type* TypeRegistry::register_struct(const std::string& name,
                                    const std::vector<StructField>& fields) {
    auto t = std::make_unique<Type>(TypeKind::Struct, name, 0);
    t->fields = fields;
    int sz = 0;
    for (const auto& f : fields) {
        Type* ft = resolve(f.type_name);
        if (ft) sz += ft->size_bytes;
        else    sz += 4;
    }
    t->size_bytes = sz;
    Type* ptr = t.get();
    owned_.push_back(std::move(t));
    named_types_[name] = ptr;
    return ptr;
}

Type* TypeRegistry::register_function(const std::string& name,
                                      const std::vector<FunctionParam>& params,
                                      const std::string& return_type) {
    auto t = std::make_unique<Type>(TypeKind::Function, name, 0);
    t->params = params;
    t->return_type_name = return_type;
    Type* ptr = t.get();
    owned_.push_back(std::move(t));
    return ptr;
}

bool TypeRegistry::is_compatible(const Type* from, const Type* to) const {
    if (!from || !to) return false;
    if (from->is_error() || to->is_error()) return true;
    if (from == to) return true;
    if (*from == *to) return true;

    if (from->kind == TypeKind::Int && to->kind == TypeKind::Float) return true;

    return false;
}

Type* TypeRegistry::common_numeric_type(const Type* a, const Type* b) const {
    if (!a || !b) return error_;
    if (a->is_error() || b->is_error()) return error_;
    if (a->kind == TypeKind::Float || b->kind == TypeKind::Float) return float_;
    if (a->kind == TypeKind::Int   && b->kind == TypeKind::Int)   return int_;
    return error_;
}
