#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <unordered_map>
#include <optional>

namespace setsuna {

// Forward declarations
struct Type;
using TypePtr = std::shared_ptr<Type>;

// Type variable (for inference)
struct TypeVar {
    int id;
    mutable TypePtr instance;  // for union-find
};

// Primitive types
struct IntType {};
struct FloatType {};
struct BoolType {};
struct StringType {};
struct UnitType {};

// Function type
struct FunctionType {
    std::vector<TypePtr> paramTypes;
    TypePtr returnType;
};

// List type
struct ListTypeT {
    TypePtr elementType;
};

// Tuple type
struct TupleTypeT {
    std::vector<TypePtr> elementTypes;
};

// Record type
struct RecordTypeT {
    std::unordered_map<std::string, TypePtr> fields;
};

// Map type
struct MapTypeT {
    TypePtr keyType;
    TypePtr valueType;
};

// ADT type
struct ADTType {
    std::string name;
    std::vector<TypePtr> typeArgs;
};

// Generic/polymorphic type
struct GenericType {
    std::string name;  // e.g., "T", "U"
};

using TypeVariant = std::variant<
    TypeVar,
    IntType,
    FloatType,
    BoolType,
    StringType,
    UnitType,
    FunctionType,
    ListTypeT,
    TupleTypeT,
    RecordTypeT,
    MapTypeT,
    ADTType,
    GenericType
>;

struct Type {
    TypeVariant data;

    template<typename T>
    Type(T&& val) : data(std::forward<T>(val)) {}

    template<typename T>
    bool is() const { return std::holds_alternative<T>(data); }

    template<typename T>
    const T& as() const { return std::get<T>(data); }

    template<typename T>
    T& as() { return std::get<T>(data); }

    std::string toString() const;
    bool equals(const Type& other) const;
};

// Type constructors
inline TypePtr makeTypeVar(int id) {
    return std::make_shared<Type>(TypeVar{id, nullptr});
}

inline TypePtr makeIntType() {
    return std::make_shared<Type>(IntType{});
}

inline TypePtr makeFloatType() {
    return std::make_shared<Type>(FloatType{});
}

inline TypePtr makeBoolType() {
    return std::make_shared<Type>(BoolType{});
}

inline TypePtr makeStringType() {
    return std::make_shared<Type>(StringType{});
}

inline TypePtr makeUnitType() {
    return std::make_shared<Type>(UnitType{});
}

inline TypePtr makeFunctionType(std::vector<TypePtr> params, TypePtr ret) {
    return std::make_shared<Type>(FunctionType{std::move(params), ret});
}

inline TypePtr makeListType(TypePtr elem) {
    return std::make_shared<Type>(ListTypeT{elem});
}

inline TypePtr makeTupleType(std::vector<TypePtr> elems) {
    return std::make_shared<Type>(TupleTypeT{std::move(elems)});
}

inline TypePtr makeRecordType(std::unordered_map<std::string, TypePtr> fields) {
    return std::make_shared<Type>(RecordTypeT{std::move(fields)});
}

inline TypePtr makeMapType(TypePtr key, TypePtr value) {
    return std::make_shared<Type>(MapTypeT{key, value});
}

inline TypePtr makeADTType(const std::string& name, std::vector<TypePtr> args = {}) {
    return std::make_shared<Type>(ADTType{name, std::move(args)});
}

inline TypePtr makeGenericType(const std::string& name) {
    return std::make_shared<Type>(GenericType{name});
}

// Type scheme for polymorphic types (forall a. a -> a)
struct TypeScheme {
    std::vector<int> typeVars;  // Quantified type variables
    TypePtr type;
};

// Type environment
class TypeEnv {
public:
    void define(const std::string& name, TypePtr type);
    void defineScheme(const std::string& name, TypeScheme scheme);
    std::optional<TypePtr> get(const std::string& name) const;
    std::optional<TypeScheme> getScheme(const std::string& name) const;

    TypeEnv extend() const;

private:
    std::unordered_map<std::string, TypeScheme> bindings_;
    std::shared_ptr<const TypeEnv> parent_;
};

} // namespace setsuna
