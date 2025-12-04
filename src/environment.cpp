#include "environment.hpp"
#include "builtins.hpp"
#include "error.hpp"

namespace setsuna {

void Environment::define(const std::string& name, ValuePtr value, bool isConst) {
    if (!isConst && constNames_.count(name) > 0) {
        throw RuntimeError("Cannot redeclare const '" + name + "' with let");
    }
    bindings_[name] = value;
    if (isConst) {
        constNames_.insert(name);
    }
}

void Environment::set(const std::string& name, ValuePtr value, const SourceLocation& loc) {
    // Look for existing binding in scope chain
    Environment* env = this;
    while (env) {
        auto it = env->bindings_.find(name);
        if (it != env->bindings_.end()) {
            if (env->constNames_.count(name) > 0) {
                throw RuntimeError("Cannot reassign const variable '" + name + "'", loc);
            }
            it->second = value;
            return;
        }
        env = env->parent_.get();
    }
    throw RuntimeError("Undefined variable: " + name, loc);
}

std::optional<ValuePtr> Environment::get(const std::string& name) const {
    auto it = bindings_.find(name);
    if (it != bindings_.end()) {
        return it->second;
    }
    if (parent_) {
        return parent_->get(name);
    }
    return std::nullopt;
}

bool Environment::has(const std::string& name) const {
    if (bindings_.count(name) > 0) return true;
    if (parent_) return parent_->has(name);
    return false;
}

bool Environment::isConst(const std::string& name) const {
    if (constNames_.count(name) > 0) return true;
    if (parent_) return parent_->isConst(name);
    return false;
}

void Environment::defineType(const std::string& name, const TypeDef& def) {
    types_[name] = def;
}

std::optional<TypeDef> Environment::getType(const std::string& name) const {
    auto it = types_.find(name);
    if (it != types_.end()) {
        return it->second;
    }
    if (parent_) {
        return parent_->getType(name);
    }
    return std::nullopt;
}

void Environment::defineModule(const std::string& name, EnvPtr moduleEnv) {
    modules_[name] = moduleEnv;
}

std::optional<EnvPtr> Environment::getModule(const std::string& name) const {
    auto it = modules_.find(name);
    if (it != modules_.end()) {
        return it->second;
    }
    if (parent_) {
        return parent_->getModule(name);
    }
    return std::nullopt;
}

EnvPtr Environment::extend() {
    return std::make_shared<Environment>(shared_from_this());
}

EnvPtr makeGlobalEnv() {
    auto env = std::make_shared<Environment>();
    registerBuiltins(env);
    return env;
}

} // namespace setsuna
