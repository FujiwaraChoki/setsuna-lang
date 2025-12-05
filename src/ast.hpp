#pragma once

#include <string>
#include <vector>
#include <memory>
#include <variant>
#include <optional>
#include "error.hpp"

namespace setsuna {

// Forward declarations
struct Expr;
struct Pattern;
struct TypeExpr;

using ExprPtr = std::shared_ptr<Expr>;
using PatternPtr = std::shared_ptr<Pattern>;
using TypeExprPtr = std::shared_ptr<TypeExpr>;

// ============ Expressions ============

struct IntLiteral {
    int64_t value;
    SourceLocation loc;
};

struct FloatLiteral {
    double value;
    SourceLocation loc;
};

struct StringLiteral {
    std::string value;
    SourceLocation loc;
};

// Interpolated string part - either literal text or an expression
struct InterpolatedStringPart {
    bool isExpr;
    std::string text;     // If !isExpr
    ExprPtr expr;         // If isExpr
};

struct InterpolatedStringExpr {
    std::vector<InterpolatedStringPart> parts;
    SourceLocation loc;
};

struct BoolLiteral {
    bool value;
    SourceLocation loc;
};

struct Identifier {
    std::string name;
    SourceLocation loc;
};

struct BinaryOp {
    enum class Op {
        ADD, SUB, MUL, DIV, MOD,
        EQ, NEQ, LT, GT, LTE, GTE,
        AND, OR
    };
    Op op;
    ExprPtr left;
    ExprPtr right;
    SourceLocation loc;
};

struct UnaryOp {
    enum class Op { NEG, NOT };
    Op op;
    ExprPtr operand;
    SourceLocation loc;
};

struct LetExpr {
    std::string name;
    std::optional<TypeExprPtr> typeAnnotation;
    ExprPtr value;
    bool isConst;
    SourceLocation loc;
};

struct AssignExpr {
    std::string name;
    ExprPtr value;
    SourceLocation loc;
};

struct FnDef {
    std::string name;  // empty for lambda
    std::vector<std::pair<std::string, std::optional<TypeExprPtr>>> params;
    std::optional<TypeExprPtr> returnType;
    ExprPtr body;
    SourceLocation loc;
};

struct Lambda {
    std::vector<std::pair<std::string, std::optional<TypeExprPtr>>> params;
    ExprPtr body;
    SourceLocation loc;
};

struct Call {
    ExprPtr callee;
    std::vector<ExprPtr> args;
    SourceLocation loc;
};

struct IfExpr {
    ExprPtr condition;
    ExprPtr thenBranch;
    ExprPtr elseBranch;  // nullptr if no else
    SourceLocation loc;
};

struct WhileExpr {
    ExprPtr condition;
    ExprPtr body;
    SourceLocation loc;
};

struct ForExpr {
    std::string varName;
    ExprPtr iterable;
    ExprPtr body;
    SourceLocation loc;
};

struct ListExpr {
    std::vector<ExprPtr> elements;
    SourceLocation loc;
};

struct TupleExpr {
    std::vector<ExprPtr> elements;
    SourceLocation loc;
};

struct RecordExpr {
    std::vector<std::pair<std::string, ExprPtr>> fields;
    SourceLocation loc;
};

struct MapExpr {
    std::vector<std::pair<ExprPtr, ExprPtr>> entries;
    SourceLocation loc;
};

struct FieldAccess {
    ExprPtr object;
    std::string field;
    SourceLocation loc;
};

struct MatchArm {
    PatternPtr pattern;
    std::optional<ExprPtr> guard;
    ExprPtr body;
};

struct MatchExpr {
    ExprPtr scrutinee;
    std::vector<MatchArm> arms;
    SourceLocation loc;
};

struct Block {
    std::vector<ExprPtr> exprs;
    SourceLocation loc;
};

// ADT constructor call
struct ConstructorCall {
    std::string typeName;
    std::string ctorName;
    std::vector<ExprPtr> args;
    SourceLocation loc;
};

// Module member access (Math.add)
struct ModuleAccess {
    std::string moduleName;
    std::string memberName;
    SourceLocation loc;
};

using ExprVariant = std::variant<
    IntLiteral,
    FloatLiteral,
    StringLiteral,
    InterpolatedStringExpr,
    BoolLiteral,
    Identifier,
    BinaryOp,
    UnaryOp,
    LetExpr,
    AssignExpr,
    FnDef,
    Lambda,
    Call,
    IfExpr,
    WhileExpr,
    ForExpr,
    ListExpr,
    TupleExpr,
    RecordExpr,
    MapExpr,
    FieldAccess,
    MatchExpr,
    Block,
    ConstructorCall,
    ModuleAccess
>;

struct Expr {
    ExprVariant data;

    template<typename T>
    Expr(T&& val) : data(std::forward<T>(val)) {}

    template<typename T>
    bool is() const { return std::holds_alternative<T>(data); }

    template<typename T>
    const T& as() const { return std::get<T>(data); }

    template<typename T>
    T& as() { return std::get<T>(data); }

    SourceLocation location() const;
};

// ============ Patterns ============

struct WildcardPattern {
    SourceLocation loc;
};

struct VarPattern {
    std::string name;
    SourceLocation loc;
};

struct LiteralPattern {
    std::variant<int64_t, double, std::string, bool> value;
    SourceLocation loc;
};

struct ListPattern {
    std::vector<PatternPtr> elements;
    std::optional<std::string> rest;  // for [head, ...tail]
    SourceLocation loc;
};

struct TuplePattern {
    std::vector<PatternPtr> elements;
    SourceLocation loc;
};

struct RecordPattern {
    std::vector<std::pair<std::string, PatternPtr>> fields;
    SourceLocation loc;
};

struct ConstructorPattern {
    std::string ctorName;
    std::vector<PatternPtr> args;
    SourceLocation loc;
};

using PatternVariant = std::variant<
    WildcardPattern,
    VarPattern,
    LiteralPattern,
    ListPattern,
    TuplePattern,
    RecordPattern,
    ConstructorPattern
>;

struct Pattern {
    PatternVariant data;

    template<typename T>
    Pattern(T&& val) : data(std::forward<T>(val)) {}

    template<typename T>
    bool is() const { return std::holds_alternative<T>(data); }

    template<typename T>
    const T& as() const { return std::get<T>(data); }
};

// ============ Type Expressions ============

struct TypeName {
    std::string name;
    std::vector<TypeExprPtr> typeArgs;
    SourceLocation loc;
};

struct FnType {
    std::vector<TypeExprPtr> paramTypes;
    TypeExprPtr returnType;
    SourceLocation loc;
};

struct TupleType {
    std::vector<TypeExprPtr> elementTypes;
    SourceLocation loc;
};

struct RecordType {
    std::vector<std::pair<std::string, TypeExprPtr>> fields;
    SourceLocation loc;
};

struct ListType {
    TypeExprPtr elementType;
    SourceLocation loc;
};

using TypeExprVariant = std::variant<
    TypeName,
    FnType,
    TupleType,
    RecordType,
    ListType
>;

struct TypeExpr {
    TypeExprVariant data;

    template<typename T>
    TypeExpr(T&& val) : data(std::forward<T>(val)) {}

    template<typename T>
    bool is() const { return std::holds_alternative<T>(data); }

    template<typename T>
    const T& as() const { return std::get<T>(data); }
};

// ============ Top-Level Declarations ============

struct TypeConstructor {
    std::string name;
    std::vector<TypeExprPtr> fields;
};

struct TypeDef {
    std::string name;
    std::vector<std::string> typeParams;
    std::vector<TypeConstructor> constructors;
    SourceLocation loc;
};

struct ModuleDef {
    std::string name;
    std::vector<ExprPtr> body;
    SourceLocation loc;
};

struct ImportDecl {
    std::string moduleName;
    std::optional<std::string> alias;
    SourceLocation loc;
};

using DeclVariant = std::variant<ExprPtr, TypeDef, ModuleDef, ImportDecl>;

struct Decl {
    DeclVariant data;

    template<typename T>
    Decl(T&& val) : data(std::forward<T>(val)) {}

    template<typename T>
    bool is() const { return std::holds_alternative<T>(data); }

    template<typename T>
    const T& as() const { return std::get<T>(data); }
};

struct Program {
    std::vector<Decl> declarations;
};

// Helper functions
inline ExprPtr makeExpr(ExprVariant&& v) {
    return std::make_shared<Expr>(std::move(v));
}

inline PatternPtr makePattern(PatternVariant&& v) {
    return std::make_shared<Pattern>(std::move(v));
}

inline TypeExprPtr makeTypeExpr(TypeExprVariant&& v) {
    return std::make_shared<TypeExpr>(std::move(v));
}

} // namespace setsuna
