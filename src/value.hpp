#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <unordered_map>
#include <functional>
#include "ast.hpp"

namespace setsuna {

// Forward declarations
struct Value;
class Environment;

using ValuePtr = std::shared_ptr<Value>;
using EnvPtr = std::shared_ptr<Environment>;

// Built-in function signature
using BuiltinFn = std::function<ValuePtr(const std::vector<ValuePtr>&)>;

// Thunk for lazy evaluation
struct Thunk {
    ExprPtr expr;
    EnvPtr env;
    mutable ValuePtr cached;
    mutable bool evaluated = false;

    ValuePtr force() const;
};

// Closure captures function + environment
struct Closure {
    std::vector<std::string> params;
    ExprPtr body;
    EnvPtr env;
};

// ADT Value
struct ADTValue {
    std::string typeName;
    std::string ctorName;
    std::vector<ValuePtr> fields;
};

// Builtin wrapper
struct Builtin {
    std::string name;
    int arity;
    BuiltinFn fn;
};

// Record value
struct RecordValue {
    std::unordered_map<std::string, ValuePtr> fields;
};

// Wrapper types for List and Tuple to make them distinct in variant
struct ListValue {
    std::vector<ValuePtr> elements;
};

struct TupleValue {
    std::vector<ValuePtr> elements;
};

using ValueVariant = std::variant<
    std::monostate,              // Unit/Nil
    int64_t,                      // Int
    double,                       // Float
    bool,                         // Bool
    std::string,                  // String
    ListValue,                    // List
    TupleValue,                   // Tuple
    RecordValue,                  // Record
    Closure,                      // Function closure
    Builtin,                      // Built-in function
    ADTValue,                     // Algebraic data type value
    std::shared_ptr<Thunk>        // Lazy thunk
>;

enum class ValueType {
    UNIT,
    INT,
    FLOAT,
    BOOL,
    STRING,
    LIST,
    TUPLE,
    RECORD,
    CLOSURE,
    BUILTIN,
    ADT,
    THUNK
};

struct Value {
    ValueVariant data;
    ValueType type;

    // Constructors
    Value() : data(std::monostate{}), type(ValueType::UNIT) {}
    Value(int64_t v) : data(v), type(ValueType::INT) {}
    Value(double v) : data(v), type(ValueType::FLOAT) {}
    Value(bool v) : data(v), type(ValueType::BOOL) {}
    Value(const std::string& v) : data(v), type(ValueType::STRING) {}
    Value(ListValue v) : data(std::move(v)), type(ValueType::LIST) {}
    Value(TupleValue v) : data(std::move(v)), type(ValueType::TUPLE) {}
    Value(RecordValue v) : data(std::move(v)), type(ValueType::RECORD) {}
    Value(Closure v) : data(std::move(v)), type(ValueType::CLOSURE) {}
    Value(Builtin v) : data(std::move(v)), type(ValueType::BUILTIN) {}
    Value(ADTValue v) : data(std::move(v)), type(ValueType::ADT) {}
    Value(std::shared_ptr<Thunk> v) : data(std::move(v)), type(ValueType::THUNK) {}

    // Accessors
    int64_t asInt() const { return std::get<int64_t>(data); }
    double asFloat() const { return std::get<double>(data); }
    bool asBool() const { return std::get<bool>(data); }
    const std::string& asString() const { return std::get<std::string>(data); }
    const std::vector<ValuePtr>& asList() const { return std::get<ListValue>(data).elements; }
    std::vector<ValuePtr>& asList() { return std::get<ListValue>(data).elements; }
    const std::vector<ValuePtr>& asTuple() const { return std::get<TupleValue>(data).elements; }
    std::vector<ValuePtr>& asTuple() { return std::get<TupleValue>(data).elements; }
    const RecordValue& asRecord() const { return std::get<RecordValue>(data); }
    const Closure& asClosure() const { return std::get<Closure>(data); }
    const Builtin& asBuiltin() const { return std::get<Builtin>(data); }
    const ADTValue& asADT() const { return std::get<ADTValue>(data); }
    const std::shared_ptr<Thunk>& asThunk() const { return std::get<std::shared_ptr<Thunk>>(data); }

    // Check type
    bool isUnit() const { return type == ValueType::UNIT; }
    bool isInt() const { return type == ValueType::INT; }
    bool isFloat() const { return type == ValueType::FLOAT; }
    bool isBool() const { return type == ValueType::BOOL; }
    bool isString() const { return type == ValueType::STRING; }
    bool isList() const { return type == ValueType::LIST; }
    bool isTuple() const { return type == ValueType::TUPLE; }
    bool isRecord() const { return type == ValueType::RECORD; }
    bool isClosure() const { return type == ValueType::CLOSURE; }
    bool isBuiltin() const { return type == ValueType::BUILTIN; }
    bool isADT() const { return type == ValueType::ADT; }
    bool isThunk() const { return type == ValueType::THUNK; }
    bool isCallable() const { return isClosure() || isBuiltin(); }

    // Convert to number (for arithmetic)
    double toNumber() const {
        if (isInt()) return static_cast<double>(asInt());
        if (isFloat()) return asFloat();
        throw std::runtime_error("Not a number");
    }

    // String representation
    std::string toString() const;

    // Equality
    bool equals(const Value& other) const;
};

// Helper functions
inline ValuePtr makeUnit() {
    return std::make_shared<Value>();
}

inline ValuePtr makeInt(int64_t v) {
    return std::make_shared<Value>(v);
}

inline ValuePtr makeFloat(double v) {
    return std::make_shared<Value>(v);
}

inline ValuePtr makeBool(bool v) {
    return std::make_shared<Value>(v);
}

inline ValuePtr makeString(const std::string& v) {
    return std::make_shared<Value>(v);
}

inline ValuePtr makeList(std::vector<ValuePtr> v) {
    return std::make_shared<Value>(ListValue{std::move(v)});
}

inline ValuePtr makeTuple(std::vector<ValuePtr> v) {
    return std::make_shared<Value>(TupleValue{std::move(v)});
}

inline ValuePtr makeRecord(RecordValue v) {
    return std::make_shared<Value>(std::move(v));
}

inline ValuePtr makeClosure(Closure c) {
    return std::make_shared<Value>(std::move(c));
}

inline ValuePtr makeBuiltin(const std::string& name, int arity, BuiltinFn fn) {
    return std::make_shared<Value>(Builtin{name, arity, std::move(fn)});
}

inline ValuePtr makeADT(const std::string& typeName, const std::string& ctorName,
                         std::vector<ValuePtr> fields) {
    return std::make_shared<Value>(ADTValue{typeName, ctorName, std::move(fields)});
}

inline ValuePtr makeThunk(ExprPtr expr, EnvPtr env) {
    auto thunk = std::make_shared<Thunk>();
    thunk->expr = expr;
    thunk->env = env;
    return std::make_shared<Value>(thunk);
}

// Force evaluation of a value (unwrap thunks)
ValuePtr force(ValuePtr val);

} // namespace setsuna
