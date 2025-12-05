#include "typechecker.hpp"
#include "error.hpp"

namespace setsuna {

// Type implementation
std::string Type::toString() const {
    return std::visit([](const auto& t) -> std::string {
        using T = std::decay_t<decltype(t)>;

        if constexpr (std::is_same_v<T, TypeVar>) {
            if (t.instance) return t.instance->toString();
            return "t" + std::to_string(t.id);
        } else if constexpr (std::is_same_v<T, IntType>) {
            return "Int";
        } else if constexpr (std::is_same_v<T, FloatType>) {
            return "Float";
        } else if constexpr (std::is_same_v<T, BoolType>) {
            return "Bool";
        } else if constexpr (std::is_same_v<T, StringType>) {
            return "String";
        } else if constexpr (std::is_same_v<T, UnitType>) {
            return "()";
        } else if constexpr (std::is_same_v<T, FunctionType>) {
            std::string s = "(";
            for (size_t i = 0; i < t.paramTypes.size(); i++) {
                if (i > 0) s += ", ";
                s += t.paramTypes[i]->toString();
            }
            s += ") -> " + t.returnType->toString();
            return s;
        } else if constexpr (std::is_same_v<T, ListTypeT>) {
            return "[" + t.elementType->toString() + "]";
        } else if constexpr (std::is_same_v<T, TupleTypeT>) {
            std::string s = "(";
            for (size_t i = 0; i < t.elementTypes.size(); i++) {
                if (i > 0) s += ", ";
                s += t.elementTypes[i]->toString();
            }
            return s + ")";
        } else if constexpr (std::is_same_v<T, RecordTypeT>) {
            std::string s = "{ ";
            bool first = true;
            for (const auto& [k, v] : t.fields) {
                if (!first) s += ", ";
                first = false;
                s += k + ": " + v->toString();
            }
            return s + " }";
        } else if constexpr (std::is_same_v<T, MapTypeT>) {
            return "Map<" + t.keyType->toString() + ", " + t.valueType->toString() + ">";
        } else if constexpr (std::is_same_v<T, ADTType>) {
            std::string s = t.name;
            if (!t.typeArgs.empty()) {
                s += "<";
                for (size_t i = 0; i < t.typeArgs.size(); i++) {
                    if (i > 0) s += ", ";
                    s += t.typeArgs[i]->toString();
                }
                s += ">";
            }
            return s;
        } else if constexpr (std::is_same_v<T, GenericType>) {
            return t.name;
        }
        return "unknown";
    }, data);
}

bool Type::equals(const Type& other) const {
    // Simple structural equality (not handling type variables properly)
    return toString() == other.toString();
}

// TypeEnv implementation
void TypeEnv::define(const std::string& name, TypePtr type) {
    bindings_[name] = TypeScheme{{}, type};
}

void TypeEnv::defineScheme(const std::string& name, TypeScheme scheme) {
    bindings_[name] = scheme;
}

std::optional<TypePtr> TypeEnv::get(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it != bindings_.end()) {
        return it->second.type;
    }
    if (parent_) {
        return parent_->get(name);
    }
    return std::nullopt;
}

std::optional<TypeScheme> TypeEnv::getScheme(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it != bindings_.end()) {
        return it->second;
    }
    if (parent_) {
        return parent_->getScheme(name);
    }
    return std::nullopt;
}

TypeEnv TypeEnv::extend() const {
    TypeEnv child;
    child.parent_ = std::make_shared<const TypeEnv>(*this);
    return child;
}

// TypeChecker implementation
TypeChecker::TypeChecker() {
    // Register built-in types
    env_.define("print", makeFunctionType({makeGenericType("a")}, makeUnitType()));
    env_.define("println", makeFunctionType({makeGenericType("a")}, makeUnitType()));
    env_.define("str", makeFunctionType({makeGenericType("a")}, makeStringType()));
    env_.define("int", makeFunctionType({makeGenericType("a")}, makeIntType()));
    env_.define("float", makeFunctionType({makeGenericType("a")}, makeFloatType()));

    env_.define("head", makeFunctionType({makeListType(makeGenericType("a"))}, makeGenericType("a")));
    env_.define("tail", makeFunctionType({makeListType(makeGenericType("a"))}, makeListType(makeGenericType("a"))));
    env_.define("cons", makeFunctionType({makeGenericType("a"), makeListType(makeGenericType("a"))}, makeListType(makeGenericType("a"))));
    env_.define("len", makeFunctionType({makeListType(makeGenericType("a"))}, makeIntType()));
    env_.define("empty", makeFunctionType({makeListType(makeGenericType("a"))}, makeBoolType()));

    env_.define("abs", makeFunctionType({makeIntType()}, makeIntType()));
    env_.define("sqrt", makeFunctionType({makeFloatType()}, makeFloatType()));
    env_.define("pow", makeFunctionType({makeFloatType(), makeFloatType()}, makeFloatType()));
    env_.define("min", makeFunctionType({makeIntType(), makeIntType()}, makeIntType()));
    env_.define("max", makeFunctionType({makeIntType(), makeIntType()}, makeIntType()));

    env_.define("range", makeFunctionType({makeIntType(), makeIntType()}, makeListType(makeIntType())));
    env_.define("input", makeFunctionType({}, makeStringType()));
    env_.define("error", makeFunctionType({makeStringType()}, makeGenericType("a")));
}

void TypeChecker::check(const Program& program) {
    for (const auto& decl : program.declarations) {
        if (decl.is<ExprPtr>()) {
            infer(decl.as<ExprPtr>());
        }
    }
}

TypePtr TypeChecker::infer(const ExprPtr& expr) {
    return inferExpr(expr, env_);
}

TypePtr TypeChecker::freshTypeVar() {
    return makeTypeVar(nextTypeVar_++);
}

TypePtr TypeChecker::inferExpr(const ExprPtr& expr, TypeEnv& env) {
    return std::visit([this, &env](const auto& e) -> TypePtr {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, IntLiteral>) {
            return makeIntType();
        } else if constexpr (std::is_same_v<T, FloatLiteral>) {
            return makeFloatType();
        } else if constexpr (std::is_same_v<T, StringLiteral>) {
            return makeStringType();
        } else if constexpr (std::is_same_v<T, InterpolatedStringExpr>) {
            // Type-check all expressions inside the f-string
            for (const auto& part : e.parts) {
                if (part.isExpr) {
                    inferExpr(part.expr, env);
                }
            }
            return makeStringType();
        } else if constexpr (std::is_same_v<T, BoolLiteral>) {
            return makeBoolType();
        } else if constexpr (std::is_same_v<T, Identifier>) {
            auto scheme = env.getScheme(e.name);
            if (!scheme) {
                throw TypeError("Undefined variable: " + e.name, e.loc);
            }
            return instantiate(*scheme);
        } else if constexpr (std::is_same_v<T, BinaryOp>) {
            auto leftType = inferExpr(e.left, env);
            auto rightType = inferExpr(e.right, env);

            switch (e.op) {
                case BinaryOp::Op::ADD:
                case BinaryOp::Op::SUB:
                case BinaryOp::Op::MUL:
                case BinaryOp::Op::DIV:
                case BinaryOp::Op::MOD:
                    // Numeric operations
                    unify(leftType, rightType);
                    return leftType;

                case BinaryOp::Op::EQ:
                case BinaryOp::Op::NEQ:
                    unify(leftType, rightType);
                    return makeBoolType();

                case BinaryOp::Op::LT:
                case BinaryOp::Op::GT:
                case BinaryOp::Op::LTE:
                case BinaryOp::Op::GTE:
                    unify(leftType, rightType);
                    return makeBoolType();

                case BinaryOp::Op::AND:
                case BinaryOp::Op::OR:
                    unify(leftType, makeBoolType());
                    unify(rightType, makeBoolType());
                    return makeBoolType();
            }
            return freshTypeVar();
        } else if constexpr (std::is_same_v<T, UnaryOp>) {
            auto operandType = inferExpr(e.operand, env);

            switch (e.op) {
                case UnaryOp::Op::NEG:
                    // Could be Int or Float
                    return operandType;
                case UnaryOp::Op::NOT:
                    unify(operandType, makeBoolType());
                    return makeBoolType();
            }
            return freshTypeVar();
        } else if constexpr (std::is_same_v<T, LetExpr>) {
            auto valueType = inferExpr(e.value, env);
            auto scheme = generalize(valueType, env);
            env.defineScheme(e.name, scheme);
            return valueType;
        } else if constexpr (std::is_same_v<T, FnDef>) {
            auto fnEnv = env.extend();
            std::vector<TypePtr> paramTypes;

            for (const auto& [name, typeAnnotation] : e.params) {
                TypePtr paramType;
                if (typeAnnotation) {
                    // TODO: Convert type annotation to TypePtr
                    paramType = freshTypeVar();
                } else {
                    paramType = freshTypeVar();
                }
                paramTypes.push_back(paramType);
                fnEnv.define(name, paramType);
            }

            auto returnType = inferExpr(e.body, fnEnv);
            auto fnType = makeFunctionType(paramTypes, returnType);

            auto scheme = generalize(fnType, env);
            env.defineScheme(e.name, scheme);

            return fnType;
        } else if constexpr (std::is_same_v<T, Lambda>) {
            auto lambdaEnv = env.extend();
            std::vector<TypePtr> paramTypes;

            for (const auto& [name, typeAnnotation] : e.params) {
                TypePtr paramType;
                if (typeAnnotation) {
                    paramType = freshTypeVar();
                } else {
                    paramType = freshTypeVar();
                }
                paramTypes.push_back(paramType);
                lambdaEnv.define(name, paramType);
            }

            auto returnType = inferExpr(e.body, lambdaEnv);
            return makeFunctionType(paramTypes, returnType);
        } else if constexpr (std::is_same_v<T, Call>) {
            auto calleeType = find(inferExpr(e.callee, env));

            std::vector<TypePtr> argTypes;
            for (const auto& arg : e.args) {
                argTypes.push_back(inferExpr(arg, env));
            }

            auto returnType = freshTypeVar();
            auto expectedFnType = makeFunctionType(argTypes, returnType);

            unify(calleeType, expectedFnType);

            return returnType;
        } else if constexpr (std::is_same_v<T, IfExpr>) {
            auto condType = inferExpr(e.condition, env);
            unify(condType, makeBoolType());

            auto thenType = inferExpr(e.thenBranch, env);

            if (e.elseBranch) {
                auto elseType = inferExpr(e.elseBranch, env);
                unify(thenType, elseType);
            }

            return thenType;
        } else if constexpr (std::is_same_v<T, WhileExpr>) {
            auto condType = inferExpr(e.condition, env);
            unify(condType, makeBoolType());

            // Body can have any type, but loop returns the body type or unit
            auto bodyType = inferExpr(e.body, env);
            return bodyType;
        } else if constexpr (std::is_same_v<T, ForExpr>) {
            auto iterableType = inferExpr(e.iterable, env);

            // Get element type from list type
            auto elemType = freshTypeVar();
            unify(iterableType, makeListType(elemType));

            // Create new environment with loop variable
            auto loopEnv = env.extend();
            loopEnv.define(e.varName, elemType);

            auto bodyType = inferExpr(e.body, loopEnv);
            return bodyType;
        } else if constexpr (std::is_same_v<T, ListExpr>) {
            if (e.elements.empty()) {
                return makeListType(freshTypeVar());
            }

            auto elemType = inferExpr(e.elements[0], env);
            for (size_t i = 1; i < e.elements.size(); i++) {
                unify(elemType, inferExpr(e.elements[i], env));
            }

            return makeListType(elemType);
        } else if constexpr (std::is_same_v<T, TupleExpr>) {
            std::vector<TypePtr> elemTypes;
            for (const auto& elem : e.elements) {
                elemTypes.push_back(inferExpr(elem, env));
            }
            return makeTupleType(elemTypes);
        } else if constexpr (std::is_same_v<T, RecordExpr>) {
            std::unordered_map<std::string, TypePtr> fieldTypes;
            for (const auto& [name, expr] : e.fields) {
                fieldTypes[name] = inferExpr(expr, env);
            }
            return makeRecordType(fieldTypes);
        } else if constexpr (std::is_same_v<T, MapExpr>) {
            if (e.entries.empty()) {
                return makeMapType(freshTypeVar(), freshTypeVar());
            }

            auto keyType = inferExpr(e.entries[0].first, env);
            auto valueType = inferExpr(e.entries[0].second, env);

            for (size_t i = 1; i < e.entries.size(); i++) {
                unify(keyType, inferExpr(e.entries[i].first, env));
                unify(valueType, inferExpr(e.entries[i].second, env));
            }

            return makeMapType(keyType, valueType);
        } else if constexpr (std::is_same_v<T, FieldAccess>) {
            auto objType = find(inferExpr(e.object, env));

            if (objType->template is<RecordTypeT>()) {
                const auto& rec = objType->template as<RecordTypeT>();
                auto it = rec.fields.find(e.field);
                if (it == rec.fields.end()) {
                    throw TypeError("Unknown field: " + e.field, e.loc);
                }
                return it->second;
            }

            // For unknown types, return a fresh type variable
            return freshTypeVar();
        } else if constexpr (std::is_same_v<T, MatchExpr>) {
            auto scrutineeType = inferExpr(e.scrutinee, env);
            TypePtr resultType = nullptr;

            for (const auto& arm : e.arms) {
                auto armEnv = env.extend();
                // TODO: Properly type-check patterns
                auto bodyType = inferExpr(arm.body, armEnv);

                if (resultType) {
                    unify(resultType, bodyType);
                } else {
                    resultType = bodyType;
                }
            }

            return resultType ? resultType : freshTypeVar();
        } else if constexpr (std::is_same_v<T, Block>) {
            TypePtr lastType = makeUnitType();
            auto blockEnv = env.extend();

            for (const auto& expr : e.exprs) {
                lastType = inferExpr(expr, blockEnv);
            }

            return lastType;
        } else if constexpr (std::is_same_v<T, ModuleAccess>) {
            // Module access - return fresh type var for now
            return freshTypeVar();
        } else if constexpr (std::is_same_v<T, ConstructorCall>) {
            // ADT constructor - return the ADT type
            return makeADTType(e.typeName);
        } else {
            return freshTypeVar();
        }
    }, expr->data);
}

void TypeChecker::unify(TypePtr t1, TypePtr t2) {
    t1 = find(t1);
    t2 = find(t2);

    if (t1.get() == t2.get()) return;

    // If either is a type variable, bind it
    if (t1->is<TypeVar>()) {
        auto& var1 = t1->as<TypeVar>();
        if (occursIn(var1.id, t2)) {
            throw TypeError("Infinite type");
        }
        var1.instance = t2;
        return;
    }

    if (t2->is<TypeVar>()) {
        auto& var2 = t2->as<TypeVar>();
        if (occursIn(var2.id, t1)) {
            throw TypeError("Infinite type");
        }
        var2.instance = t1;
        return;
    }

    // Both are concrete types - must match structurally
    if (t1->is<IntType>() && t2->is<IntType>()) return;
    if (t1->is<FloatType>() && t2->is<FloatType>()) return;
    if (t1->is<BoolType>() && t2->is<BoolType>()) return;
    if (t1->is<StringType>() && t2->is<StringType>()) return;
    if (t1->is<UnitType>() && t2->is<UnitType>()) return;

    if (t1->is<FunctionType>() && t2->is<FunctionType>()) {
        const auto& fn1 = t1->as<FunctionType>();
        const auto& fn2 = t2->as<FunctionType>();

        if (fn1.paramTypes.size() != fn2.paramTypes.size()) {
            throw TypeError("Function arity mismatch");
        }

        for (size_t i = 0; i < fn1.paramTypes.size(); i++) {
            unify(fn1.paramTypes[i], fn2.paramTypes[i]);
        }
        unify(fn1.returnType, fn2.returnType);
        return;
    }

    if (t1->is<ListTypeT>() && t2->is<ListTypeT>()) {
        unify(t1->as<ListTypeT>().elementType, t2->as<ListTypeT>().elementType);
        return;
    }

    if (t1->is<TupleTypeT>() && t2->is<TupleTypeT>()) {
        const auto& tup1 = t1->as<TupleTypeT>();
        const auto& tup2 = t2->as<TupleTypeT>();

        if (tup1.elementTypes.size() != tup2.elementTypes.size()) {
            throw TypeError("Tuple size mismatch");
        }

        for (size_t i = 0; i < tup1.elementTypes.size(); i++) {
            unify(tup1.elementTypes[i], tup2.elementTypes[i]);
        }
        return;
    }

    // Generic types are flexible
    if (t1->is<GenericType>() || t2->is<GenericType>()) {
        return;
    }

    throw TypeError("Cannot unify " + t1->toString() + " with " + t2->toString());
}

TypePtr TypeChecker::find(TypePtr t) {
    if (t->is<TypeVar>()) {
        const auto& var = t->as<TypeVar>();
        if (var.instance) {
            // Path compression
            var.instance = find(var.instance);
            return var.instance;
        }
    }
    return t;
}

bool TypeChecker::occursIn(int varId, TypePtr t) {
    t = find(t);

    if (t->is<TypeVar>()) {
        return t->as<TypeVar>().id == varId;
    }

    if (t->is<FunctionType>()) {
        const auto& fn = t->as<FunctionType>();
        for (const auto& param : fn.paramTypes) {
            if (occursIn(varId, param)) return true;
        }
        return occursIn(varId, fn.returnType);
    }

    if (t->is<ListTypeT>()) {
        return occursIn(varId, t->as<ListTypeT>().elementType);
    }

    if (t->is<TupleTypeT>()) {
        for (const auto& elem : t->as<TupleTypeT>().elementTypes) {
            if (occursIn(varId, elem)) return true;
        }
        return false;
    }

    return false;
}

TypeScheme TypeChecker::generalize(TypePtr t, const TypeEnv& env) {
    auto freeInType = freeTypeVars(t);
    auto freeInEnv = freeTypeVars(env);

    std::vector<int> quantified;
    for (int v : freeInType) {
        if (freeInEnv.find(v) == freeInEnv.end()) {
            quantified.push_back(v);
        }
    }

    return TypeScheme{quantified, t};
}

TypePtr TypeChecker::instantiate(const TypeScheme& scheme) {
    std::unordered_map<int, TypePtr> subst;
    for (int v : scheme.typeVars) {
        subst[v] = freshTypeVar();
    }

    std::function<TypePtr(TypePtr)> inst = [&](TypePtr t) -> TypePtr {
        t = find(t);

        if (t->is<TypeVar>()) {
            auto it = subst.find(t->as<TypeVar>().id);
            if (it != subst.end()) {
                return it->second;
            }
            return t;
        }

        if (t->is<FunctionType>()) {
            const auto& fn = t->as<FunctionType>();
            std::vector<TypePtr> params;
            for (const auto& p : fn.paramTypes) {
                params.push_back(inst(p));
            }
            return makeFunctionType(params, inst(fn.returnType));
        }

        if (t->is<ListTypeT>()) {
            return makeListType(inst(t->as<ListTypeT>().elementType));
        }

        if (t->is<TupleTypeT>()) {
            std::vector<TypePtr> elems;
            for (const auto& e : t->as<TupleTypeT>().elementTypes) {
                elems.push_back(inst(e));
            }
            return makeTupleType(elems);
        }

        return t;
    };

    return inst(scheme.type);
}

std::unordered_set<int> TypeChecker::freeTypeVars(TypePtr t) {
    std::unordered_set<int> result;
    t = find(t);

    if (t->is<TypeVar>()) {
        result.insert(t->as<TypeVar>().id);
        return result;
    }

    if (t->is<FunctionType>()) {
        const auto& fn = t->as<FunctionType>();
        for (const auto& p : fn.paramTypes) {
            auto vars = freeTypeVars(p);
            result.insert(vars.begin(), vars.end());
        }
        auto retVars = freeTypeVars(fn.returnType);
        result.insert(retVars.begin(), retVars.end());
    }

    if (t->is<ListTypeT>()) {
        return freeTypeVars(t->as<ListTypeT>().elementType);
    }

    if (t->is<TupleTypeT>()) {
        for (const auto& e : t->as<TupleTypeT>().elementTypes) {
            auto vars = freeTypeVars(e);
            result.insert(vars.begin(), vars.end());
        }
    }

    return result;
}

std::unordered_set<int> TypeChecker::freeTypeVars(const TypeEnv& env) {
    // Simplified - would need to iterate all bindings
    (void)env;
    return {};
}

TypePtr TypeChecker::apply(TypePtr t) {
    return find(t);
}

} // namespace setsuna
