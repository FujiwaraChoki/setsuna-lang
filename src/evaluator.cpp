#include "evaluator.hpp"
#include "error.hpp"
#include <cmath>

namespace setsuna {

// Value implementation
std::string Value::toString() const {
    switch (type) {
        case ValueType::UNIT:
            return "()";
        case ValueType::INT:
            return std::to_string(asInt());
        case ValueType::FLOAT: {
            std::string s = std::to_string(asFloat());
            // Remove trailing zeros
            s.erase(s.find_last_not_of('0') + 1, std::string::npos);
            if (s.back() == '.') s += '0';
            return s;
        }
        case ValueType::BOOL:
            return asBool() ? "true" : "false";
        case ValueType::STRING:
            return "\"" + asString() + "\"";
        case ValueType::LIST: {
            std::string s = "[";
            const auto& list = asList();
            for (size_t i = 0; i < list.size(); i++) {
                if (i > 0) s += ", ";
                s += list[i]->toString();
            }
            return s + "]";
        }
        case ValueType::TUPLE: {
            std::string s = "(";
            const auto& tuple = asTuple();
            for (size_t i = 0; i < tuple.size(); i++) {
                if (i > 0) s += ", ";
                s += tuple[i]->toString();
            }
            return s + ")";
        }
        case ValueType::RECORD: {
            std::string s = "{ ";
            const auto& rec = asRecord();
            bool first = true;
            for (const auto& [k, v] : rec.fields) {
                if (!first) s += ", ";
                first = false;
                s += k + ": " + v->toString();
            }
            return s + " }";
        }
        case ValueType::CLOSURE:
            return "<fn>";
        case ValueType::BUILTIN:
            return "<builtin:" + asBuiltin().name + ">";
        case ValueType::ADT: {
            const auto& adt = asADT();
            std::string s = adt.ctorName;
            if (!adt.fields.empty()) {
                s += "(";
                for (size_t i = 0; i < adt.fields.size(); i++) {
                    if (i > 0) s += ", ";
                    s += adt.fields[i]->toString();
                }
                s += ")";
            }
            return s;
        }
        case ValueType::THUNK:
            return "<thunk>";
    }
    return "<unknown>";
}

bool Value::equals(const Value& other) const {
    if (type != other.type) return false;

    switch (type) {
        case ValueType::UNIT:
            return true;
        case ValueType::INT:
            return asInt() == other.asInt();
        case ValueType::FLOAT:
            return asFloat() == other.asFloat();
        case ValueType::BOOL:
            return asBool() == other.asBool();
        case ValueType::STRING:
            return asString() == other.asString();
        case ValueType::LIST: {
            const auto& a = asList();
            const auto& b = other.asList();
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); i++) {
                if (!a[i]->equals(*b[i])) return false;
            }
            return true;
        }
        case ValueType::TUPLE: {
            const auto& a = asTuple();
            const auto& b = other.asTuple();
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); i++) {
                if (!a[i]->equals(*b[i])) return false;
            }
            return true;
        }
        case ValueType::RECORD: {
            const auto& a = asRecord().fields;
            const auto& b = other.asRecord().fields;
            if (a.size() != b.size()) return false;
            for (const auto& [k, v] : a) {
                auto it = b.find(k);
                if (it == b.end() || !v->equals(*it->second)) return false;
            }
            return true;
        }
        case ValueType::ADT: {
            const auto& a = asADT();
            const auto& b = other.asADT();
            if (a.ctorName != b.ctorName) return false;
            if (a.fields.size() != b.fields.size()) return false;
            for (size_t i = 0; i < a.fields.size(); i++) {
                if (!a.fields[i]->equals(*b.fields[i])) return false;
            }
            return true;
        }
        default:
            return false;
    }
}

ValuePtr force(ValuePtr val) {
    while (val->isThunk()) {
        val = val->asThunk()->force();
    }
    return val;
}

ValuePtr Thunk::force() const {
    if (!evaluated) {
        // Will be implemented through evaluator
        // For now, return cached or throw
        throw std::runtime_error("Thunk evaluation not yet implemented");
    }
    return cached;
}

// Evaluator implementation
Evaluator::Evaluator(EnvPtr env) : env_(env) {}

ValuePtr Evaluator::eval(const Program& program) {
    ValuePtr result = makeUnit();
    for (const auto& decl : program.declarations) {
        if (decl.is<ExprPtr>()) {
            result = eval(decl.as<ExprPtr>());
        } else {
            evalDecl(decl);
        }
    }
    return result;
}

ValuePtr Evaluator::eval(const ExprPtr& expr) {
    return eval(expr, env_);
}

ValuePtr Evaluator::eval(const ExprPtr& expr, EnvPtr env) {
    return std::visit([this, &env](const auto& e) -> ValuePtr {
        using T = std::decay_t<decltype(e)>;

        if constexpr (std::is_same_v<T, IntLiteral>) {
            return evalIntLiteral(e);
        } else if constexpr (std::is_same_v<T, FloatLiteral>) {
            return evalFloatLiteral(e);
        } else if constexpr (std::is_same_v<T, StringLiteral>) {
            return evalStringLiteral(e);
        } else if constexpr (std::is_same_v<T, BoolLiteral>) {
            return evalBoolLiteral(e);
        } else if constexpr (std::is_same_v<T, Identifier>) {
            return evalIdentifier(e, env);
        } else if constexpr (std::is_same_v<T, BinaryOp>) {
            return evalBinaryOp(e, env);
        } else if constexpr (std::is_same_v<T, UnaryOp>) {
            return evalUnaryOp(e, env);
        } else if constexpr (std::is_same_v<T, LetExpr>) {
            return evalLetExpr(e, env);
        } else if constexpr (std::is_same_v<T, FnDef>) {
            return evalFnDef(e, env);
        } else if constexpr (std::is_same_v<T, Lambda>) {
            return evalLambda(e, env);
        } else if constexpr (std::is_same_v<T, Call>) {
            return evalCall(e, env);
        } else if constexpr (std::is_same_v<T, IfExpr>) {
            return evalIfExpr(e, env);
        } else if constexpr (std::is_same_v<T, ListExpr>) {
            return evalListExpr(e, env);
        } else if constexpr (std::is_same_v<T, TupleExpr>) {
            return evalTupleExpr(e, env);
        } else if constexpr (std::is_same_v<T, RecordExpr>) {
            return evalRecordExpr(e, env);
        } else if constexpr (std::is_same_v<T, FieldAccess>) {
            return evalFieldAccess(e, env);
        } else if constexpr (std::is_same_v<T, MatchExpr>) {
            return evalMatchExpr(e, env);
        } else if constexpr (std::is_same_v<T, Block>) {
            return evalBlock(e, env);
        } else if constexpr (std::is_same_v<T, ModuleAccess>) {
            return evalModuleAccess(e, env);
        } else if constexpr (std::is_same_v<T, ConstructorCall>) {
            // Constructor call - create ADT value
            std::vector<ValuePtr> fields;
            for (const auto& arg : e.args) {
                fields.push_back(eval(arg, env));
            }
            return makeADT(e.typeName, e.ctorName, fields);
        } else {
            throw RuntimeError("Unknown expression type");
        }
    }, expr->data);
}

void Evaluator::evalDecl(const Decl& decl) {
    std::visit([this](const auto& d) {
        using T = std::decay_t<decltype(d)>;

        if constexpr (std::is_same_v<T, ExprPtr>) {
            eval(d);
        } else if constexpr (std::is_same_v<T, TypeDef>) {
            evalTypeDef(d);
        } else if constexpr (std::is_same_v<T, ModuleDef>) {
            evalModuleDef(d);
        } else if constexpr (std::is_same_v<T, ImportDecl>) {
            evalImportDecl(d);
        }
    }, decl.data);
}

// Literal evaluation
ValuePtr Evaluator::evalIntLiteral(const IntLiteral& lit) {
    return makeInt(lit.value);
}

ValuePtr Evaluator::evalFloatLiteral(const FloatLiteral& lit) {
    return makeFloat(lit.value);
}

ValuePtr Evaluator::evalStringLiteral(const StringLiteral& lit) {
    return makeString(lit.value);
}

ValuePtr Evaluator::evalBoolLiteral(const BoolLiteral& lit) {
    return makeBool(lit.value);
}

ValuePtr Evaluator::evalIdentifier(const Identifier& id, EnvPtr env) {
    auto val = env->get(id.name);
    if (!val) {
        throw RuntimeError("Undefined variable: " + id.name, id.loc);
    }
    return force(*val);
}

ValuePtr Evaluator::evalBinaryOp(const BinaryOp& op, EnvPtr env) {
    // Short-circuit for && and ||
    if (op.op == BinaryOp::Op::AND) {
        auto left = force(eval(op.left, env));
        if (!left->asBool()) return makeBool(false);
        return eval(op.right, env);
    }
    if (op.op == BinaryOp::Op::OR) {
        auto left = force(eval(op.left, env));
        if (left->asBool()) return makeBool(true);
        return eval(op.right, env);
    }

    auto left = force(eval(op.left, env));
    auto right = force(eval(op.right, env));

    // String concatenation
    if (op.op == BinaryOp::Op::ADD && left->isString()) {
        return makeString(left->asString() + right->asString());
    }

    // Equality
    if (op.op == BinaryOp::Op::EQ) {
        return makeBool(left->equals(*right));
    }
    if (op.op == BinaryOp::Op::NEQ) {
        return makeBool(!left->equals(*right));
    }

    // Arithmetic
    bool useFloat = left->isFloat() || right->isFloat();
    double l = left->toNumber();
    double r = right->toNumber();

    switch (op.op) {
        case BinaryOp::Op::ADD:
            return useFloat ? makeFloat(l + r) : makeInt(static_cast<int64_t>(l + r));
        case BinaryOp::Op::SUB:
            return useFloat ? makeFloat(l - r) : makeInt(static_cast<int64_t>(l - r));
        case BinaryOp::Op::MUL:
            return useFloat ? makeFloat(l * r) : makeInt(static_cast<int64_t>(l * r));
        case BinaryOp::Op::DIV:
            if (r == 0) throw RuntimeError("Division by zero", op.loc);
            return useFloat ? makeFloat(l / r) : makeInt(static_cast<int64_t>(l / r));
        case BinaryOp::Op::MOD:
            return makeInt(static_cast<int64_t>(std::fmod(l, r)));
        case BinaryOp::Op::LT:
            return makeBool(l < r);
        case BinaryOp::Op::GT:
            return makeBool(l > r);
        case BinaryOp::Op::LTE:
            return makeBool(l <= r);
        case BinaryOp::Op::GTE:
            return makeBool(l >= r);
        default:
            throw RuntimeError("Unknown binary operator", op.loc);
    }
}

ValuePtr Evaluator::evalUnaryOp(const UnaryOp& op, EnvPtr env) {
    auto val = force(eval(op.operand, env));

    switch (op.op) {
        case UnaryOp::Op::NEG:
            if (val->isInt()) return makeInt(-val->asInt());
            if (val->isFloat()) return makeFloat(-val->asFloat());
            throw RuntimeError("Cannot negate non-number", op.loc);
        case UnaryOp::Op::NOT:
            return makeBool(!val->asBool());
    }
    throw RuntimeError("Unknown unary operator", op.loc);
}

ValuePtr Evaluator::evalLetExpr(const LetExpr& let, EnvPtr env) {
    auto val = eval(let.value, env);
    env->define(let.name, val);
    return val;
}

ValuePtr Evaluator::evalFnDef(const FnDef& fn, EnvPtr env) {
    std::vector<std::string> paramNames;
    for (const auto& [name, _] : fn.params) {
        paramNames.push_back(name);
    }

    auto closure = makeClosure(Closure{paramNames, fn.body, env});
    env->define(fn.name, closure);
    return closure;
}

ValuePtr Evaluator::evalLambda(const Lambda& lambda, EnvPtr env) {
    std::vector<std::string> paramNames;
    for (const auto& [name, _] : lambda.params) {
        paramNames.push_back(name);
    }

    return makeClosure(Closure{paramNames, lambda.body, env});
}

ValuePtr Evaluator::evalCall(const Call& call, EnvPtr env) {
    auto callee = force(eval(call.callee, env));

    std::vector<ValuePtr> args;
    for (const auto& arg : call.args) {
        args.push_back(eval(arg, env));
    }

    if (callee->isBuiltin()) {
        const auto& builtin = callee->asBuiltin();
        return builtin.fn(args);
    }

    if (callee->isClosure()) {
        const auto& closure = callee->asClosure();

        if (args.size() != closure.params.size()) {
            throw RuntimeError("Wrong number of arguments: expected " +
                std::to_string(closure.params.size()) + ", got " +
                std::to_string(args.size()), call.loc);
        }

        auto callEnv = closure.env->extend();
        for (size_t i = 0; i < args.size(); i++) {
            callEnv->define(closure.params[i], args[i]);
        }

        return eval(closure.body, callEnv);
    }

    throw RuntimeError("Cannot call non-function", call.loc);
}

ValuePtr Evaluator::evalIfExpr(const IfExpr& ifExpr, EnvPtr env) {
    auto cond = force(eval(ifExpr.condition, env));

    if (cond->asBool()) {
        return eval(ifExpr.thenBranch, env);
    } else if (ifExpr.elseBranch) {
        return eval(ifExpr.elseBranch, env);
    }
    return makeUnit();
}

ValuePtr Evaluator::evalListExpr(const ListExpr& list, EnvPtr env) {
    std::vector<ValuePtr> elements;
    for (const auto& elem : list.elements) {
        elements.push_back(eval(elem, env));
    }
    return makeList(elements);
}

ValuePtr Evaluator::evalTupleExpr(const TupleExpr& tuple, EnvPtr env) {
    std::vector<ValuePtr> elements;
    for (const auto& elem : tuple.elements) {
        elements.push_back(eval(elem, env));
    }
    return makeTuple(elements);
}

ValuePtr Evaluator::evalRecordExpr(const RecordExpr& record, EnvPtr env) {
    RecordValue rec;
    for (const auto& [name, expr] : record.fields) {
        rec.fields[name] = eval(expr, env);
    }
    return makeRecord(rec);
}

ValuePtr Evaluator::evalFieldAccess(const FieldAccess& access, EnvPtr env) {
    // Check if this is a module access (object is identifier that names a module)
    if (access.object->is<Identifier>()) {
        const auto& id = access.object->as<Identifier>();
        auto mod = env->getModule(id.name);
        if (mod) {
            // It's a module access
            auto val = (*mod)->get(access.field);
            if (!val) {
                throw RuntimeError("Unknown member: " + access.field +
                    " in module " + id.name, access.loc);
            }
            return *val;
        }
    }

    auto obj = force(eval(access.object, env));

    if (obj->isRecord()) {
        const auto& rec = obj->asRecord();
        auto it = rec.fields.find(access.field);
        if (it == rec.fields.end()) {
            throw RuntimeError("Unknown field: " + access.field, access.loc);
        }
        return it->second;
    }

    if (obj->isTuple()) {
        // Allow tuple.0, tuple.1, etc.
        try {
            size_t idx = std::stoul(access.field);
            const auto& tuple = obj->asTuple();
            if (idx >= tuple.size()) {
                throw RuntimeError("Tuple index out of bounds", access.loc);
            }
            return tuple[idx];
        } catch (...) {
            throw RuntimeError("Invalid tuple index: " + access.field, access.loc);
        }
    }

    throw RuntimeError("Cannot access field on non-record/tuple", access.loc);
}

ValuePtr Evaluator::evalMatchExpr(const MatchExpr& match, EnvPtr env) {
    auto scrutinee = force(eval(match.scrutinee, env));

    for (const auto& arm : match.arms) {
        auto matchEnv = env->extend();

        if (matchPattern(arm.pattern, scrutinee, matchEnv)) {
            // Check guard
            if (arm.guard) {
                auto guardVal = force(eval(*arm.guard, matchEnv));
                if (!guardVal->asBool()) continue;
            }

            return eval(arm.body, matchEnv);
        }
    }

    throw RuntimeError("No matching pattern", match.loc);
}

ValuePtr Evaluator::evalBlock(const Block& block, EnvPtr env) {
    auto blockEnv = env->extend();
    ValuePtr result = makeUnit();

    for (const auto& expr : block.exprs) {
        result = eval(expr, blockEnv);
    }

    return result;
}

ValuePtr Evaluator::evalModuleAccess(const ModuleAccess& access, EnvPtr env) {
    auto mod = env->getModule(access.moduleName);
    if (!mod) {
        throw RuntimeError("Unknown module: " + access.moduleName, access.loc);
    }

    auto val = (*mod)->get(access.memberName);
    if (!val) {
        throw RuntimeError("Unknown member: " + access.memberName +
            " in module " + access.moduleName, access.loc);
    }

    return *val;
}

// Pattern matching
bool Evaluator::matchPattern(const PatternPtr& pattern, ValuePtr value, EnvPtr env) {
    value = force(value);

    return std::visit([this, &value, &env](const auto& p) -> bool {
        using T = std::decay_t<decltype(p)>;

        if constexpr (std::is_same_v<T, WildcardPattern>) {
            return true;
        } else if constexpr (std::is_same_v<T, VarPattern>) {
            env->define(p.name, value);
            return true;
        } else if constexpr (std::is_same_v<T, LiteralPattern>) {
            return std::visit([&value](const auto& lit) -> bool {
                using LT = std::decay_t<decltype(lit)>;
                if constexpr (std::is_same_v<LT, int64_t>) {
                    return value->isInt() && value->asInt() == lit;
                } else if constexpr (std::is_same_v<LT, double>) {
                    return value->isFloat() && value->asFloat() == lit;
                } else if constexpr (std::is_same_v<LT, std::string>) {
                    return value->isString() && value->asString() == lit;
                } else if constexpr (std::is_same_v<LT, bool>) {
                    return value->isBool() && value->asBool() == lit;
                }
                return false;
            }, p.value);
        } else if constexpr (std::is_same_v<T, ListPattern>) {
            if (!value->isList()) return false;
            const auto& list = value->asList();

            if (p.rest) {
                // [head, ...tail] pattern
                if (list.size() < p.elements.size()) return false;

                for (size_t i = 0; i < p.elements.size(); i++) {
                    if (!matchPattern(p.elements[i], list[i], env)) return false;
                }

                std::vector<ValuePtr> rest(list.begin() + p.elements.size(), list.end());
                env->define(*p.rest, makeList(rest));
                return true;
            } else {
                // Exact match
                if (list.size() != p.elements.size()) return false;
                for (size_t i = 0; i < p.elements.size(); i++) {
                    if (!matchPattern(p.elements[i], list[i], env)) return false;
                }
                return true;
            }
        } else if constexpr (std::is_same_v<T, TuplePattern>) {
            if (!value->isTuple()) return false;
            const auto& tuple = value->asTuple();

            if (tuple.size() != p.elements.size()) return false;
            for (size_t i = 0; i < p.elements.size(); i++) {
                if (!matchPattern(p.elements[i], tuple[i], env)) return false;
            }
            return true;
        } else if constexpr (std::is_same_v<T, RecordPattern>) {
            if (!value->isRecord()) return false;
            const auto& rec = value->asRecord();

            for (const auto& [name, pat] : p.fields) {
                auto it = rec.fields.find(name);
                if (it == rec.fields.end()) return false;
                if (!matchPattern(pat, it->second, env)) return false;
            }
            return true;
        } else if constexpr (std::is_same_v<T, ConstructorPattern>) {
            if (!value->isADT()) return false;
            const auto& adt = value->asADT();

            if (adt.ctorName != p.ctorName) return false;
            if (adt.fields.size() != p.args.size()) return false;

            for (size_t i = 0; i < p.args.size(); i++) {
                if (!matchPattern(p.args[i], adt.fields[i], env)) return false;
            }
            return true;
        }
        return false;
    }, pattern->data);
}

// Declaration evaluation
void Evaluator::evalTypeDef(const TypeDef& def) {
    env_->defineType(def.name, def);

    // Create constructor functions
    for (const auto& ctor : def.constructors) {
        if (ctor.fields.empty()) {
            // Nullary constructor - just a value
            env_->define(ctor.name, makeADT(def.name, ctor.name, {}));
        } else {
            // Constructor with fields - create a function
            std::vector<std::string> params;
            for (size_t i = 0; i < ctor.fields.size(); i++) {
                params.push_back("_arg" + std::to_string(i));
            }

            std::string typeName = def.name;
            std::string ctorName = ctor.name;
            int arity = static_cast<int>(ctor.fields.size());

            env_->define(ctor.name, makeBuiltin(ctor.name, arity,
                [typeName, ctorName](const std::vector<ValuePtr>& args) {
                    return makeADT(typeName, ctorName, args);
                }));
        }
    }
}

void Evaluator::evalModuleDef(const ModuleDef& mod) {
    auto modEnv = env_->extend();

    for (const auto& expr : mod.body) {
        eval(expr, modEnv);
    }

    env_->defineModule(mod.name, modEnv);
}

void Evaluator::evalImportDecl(const ImportDecl& imp) {
    // For now, just register that we want this module
    // Full implementation would load from file
    (void)imp;
}

} // namespace setsuna
