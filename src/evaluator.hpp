#pragma once

#include "ast.hpp"
#include "value.hpp"
#include "environment.hpp"

namespace setsuna {

class Evaluator {
public:
    Evaluator(EnvPtr env);

    // Evaluate a program
    ValuePtr eval(const Program& program);

    // Evaluate a single expression
    ValuePtr eval(const ExprPtr& expr);
    ValuePtr eval(const ExprPtr& expr, EnvPtr env);

    // Evaluate a declaration
    void evalDecl(const Decl& decl);

    // Get environment
    EnvPtr env() const { return env_; }

private:
    EnvPtr env_;

    // Expression evaluation
    ValuePtr evalIntLiteral(const IntLiteral& lit);
    ValuePtr evalFloatLiteral(const FloatLiteral& lit);
    ValuePtr evalStringLiteral(const StringLiteral& lit);
    ValuePtr evalBoolLiteral(const BoolLiteral& lit);
    ValuePtr evalIdentifier(const Identifier& id, EnvPtr env);
    ValuePtr evalBinaryOp(const BinaryOp& op, EnvPtr env);
    ValuePtr evalUnaryOp(const UnaryOp& op, EnvPtr env);
    ValuePtr evalLetExpr(const LetExpr& let, EnvPtr env);
    ValuePtr evalAssignExpr(const AssignExpr& assign, EnvPtr env);
    ValuePtr evalFnDef(const FnDef& fn, EnvPtr env);
    ValuePtr evalLambda(const Lambda& lambda, EnvPtr env);
    ValuePtr evalCall(const Call& call, EnvPtr env);
    ValuePtr evalIfExpr(const IfExpr& ifExpr, EnvPtr env);
    ValuePtr evalListExpr(const ListExpr& list, EnvPtr env);
    ValuePtr evalTupleExpr(const TupleExpr& tuple, EnvPtr env);
    ValuePtr evalRecordExpr(const RecordExpr& record, EnvPtr env);
    ValuePtr evalFieldAccess(const FieldAccess& access, EnvPtr env);
    ValuePtr evalMatchExpr(const MatchExpr& match, EnvPtr env);
    ValuePtr evalBlock(const Block& block, EnvPtr env);
    ValuePtr evalModuleAccess(const ModuleAccess& access, EnvPtr env);

    // Pattern matching
    bool matchPattern(const PatternPtr& pattern, ValuePtr value, EnvPtr env);

    // Declaration evaluation
    void evalTypeDef(const TypeDef& def);
    void evalModuleDef(const ModuleDef& mod);
    void evalImportDecl(const ImportDecl& imp);
};

} // namespace setsuna
