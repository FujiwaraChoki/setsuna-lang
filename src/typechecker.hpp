#pragma once

#include "ast.hpp"
#include "types.hpp"
#include <unordered_set>

namespace setsuna {

class TypeChecker {
public:
    TypeChecker();

    // Type check a program
    void check(const Program& program);

    // Infer type of an expression
    TypePtr infer(const ExprPtr& expr);

private:
    TypeEnv env_;
    int nextTypeVar_ = 0;

    // Create fresh type variable
    TypePtr freshTypeVar();

    // Type inference
    TypePtr inferExpr(const ExprPtr& expr, TypeEnv& env);

    // Unification
    void unify(TypePtr t1, TypePtr t2);
    TypePtr find(TypePtr t);
    bool occursIn(int varId, TypePtr t);

    // Generalization and instantiation
    TypeScheme generalize(TypePtr t, const TypeEnv& env);
    TypePtr instantiate(const TypeScheme& scheme);

    // Get free type variables
    std::unordered_set<int> freeTypeVars(TypePtr t);
    std::unordered_set<int> freeTypeVars(const TypeEnv& env);

    // Apply substitution
    TypePtr apply(TypePtr t);
};

} // namespace setsuna
