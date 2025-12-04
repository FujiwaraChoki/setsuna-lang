#pragma once

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <optional>
#include "value.hpp"
#include "ast.hpp"

namespace setsuna {

class Environment : public std::enable_shared_from_this<Environment> {
public:
    Environment() : parent_(nullptr) {}
    Environment(EnvPtr parent) : parent_(parent) {}

    // Variable operations
    void define(const std::string& name, ValuePtr value, bool isConst = false);
    void set(const std::string& name, ValuePtr value, const SourceLocation& loc);
    std::optional<ValuePtr> get(const std::string& name) const;
    bool has(const std::string& name) const;
    bool isConst(const std::string& name) const;

    // Type definitions
    void defineType(const std::string& name, const TypeDef& def);
    std::optional<TypeDef> getType(const std::string& name) const;

    // Module definitions
    void defineModule(const std::string& name, EnvPtr moduleEnv);
    std::optional<EnvPtr> getModule(const std::string& name) const;

    // Create child scope
    EnvPtr extend();

    // Get parent
    EnvPtr parent() const { return parent_; }

private:
    EnvPtr parent_;
    std::unordered_map<std::string, ValuePtr> bindings_;
    std::unordered_set<std::string> constNames_;
    std::unordered_map<std::string, TypeDef> types_;
    std::unordered_map<std::string, EnvPtr> modules_;
};

// Create a new global environment
EnvPtr makeGlobalEnv();

} // namespace setsuna
