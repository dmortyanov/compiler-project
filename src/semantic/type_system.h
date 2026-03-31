#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// ---------------------------------------------------------------
// TypeKind — each concrete type category
// ---------------------------------------------------------------
enum class TypeKind {
    Int,
    Float,
    Bool,
    Void,
    String,
    Struct,
    Function,
    Error   // poison type – prevents cascading errors
};

// ---------------------------------------------------------------
// Type — value-semantic type descriptor
// ---------------------------------------------------------------
struct StructField {
    std::string name;
    std::string type_name;
    int line = 0;
    int column = 0;
};

struct FunctionParam {
    std::string name;
    std::string type_name;
};

class Type {
public:
    TypeKind kind;
    std::string name;                           // e.g. "int", "Point"

    // Struct data (only when kind == Struct)
    std::vector<StructField> fields;

    // Function data (only when kind == Function)
    std::vector<FunctionParam> params;
    std::string return_type_name;               // e.g. "int", "void"

    int size_bytes = 0;

    Type() : kind(TypeKind::Error), name("<error>"), size_bytes(0) {}
    Type(TypeKind k, const std::string& n, int sz = 0)
        : kind(k), name(n), size_bytes(sz) {}

    bool operator==(const Type& o) const { return kind == o.kind && name == o.name; }
    bool operator!=(const Type& o) const { return !(*this == o); }

    bool is_error()   const { return kind == TypeKind::Error; }
    bool is_numeric() const { return kind == TypeKind::Int || kind == TypeKind::Float; }
    bool is_bool()    const { return kind == TypeKind::Bool; }
    bool is_void()    const { return kind == TypeKind::Void; }
};

// ---------------------------------------------------------------
// TypeRegistry — singleton-like store of canonical types
// ---------------------------------------------------------------
class TypeRegistry {
public:
    TypeRegistry();

    // Built-in types
    Type* type_int()    const { return int_; }
    Type* type_float()  const { return float_; }
    Type* type_bool()   const { return bool_; }
    Type* type_void()   const { return void_; }
    Type* type_string() const { return string_; }
    Type* type_error()  const { return error_; }

    // Resolve a type name: "int" → type_int(), struct name → struct type, etc.
    Type* resolve(const std::string& name) const;

    // Register a struct type
    Type* register_struct(const std::string& name,
                          const std::vector<StructField>& fields);

    // Register a function type
    Type* register_function(const std::string& name,
                            const std::vector<FunctionParam>& params,
                            const std::string& return_type);

    // Compatibility: can <from> be used where <to> is expected?
    bool is_compatible(const Type* from, const Type* to) const;

    // Common type of two operands (for binary operations)
    Type* common_numeric_type(const Type* a, const Type* b) const;

private:
    std::vector<std::unique_ptr<Type>> owned_;
    Type* int_;
    Type* float_;
    Type* bool_;
    Type* void_;
    Type* string_;
    Type* error_;

    std::unordered_map<std::string, Type*> named_types_;

    Type* make(TypeKind k, const std::string& n, int sz);
};
