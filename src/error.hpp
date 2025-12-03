#pragma once

#include <string>
#include <stdexcept>

namespace setsuna {

struct SourceLocation {
    int line = 1;
    int column = 1;
    std::string filename = "<stdin>";
};

class SetsunaError : public std::runtime_error {
public:
    SourceLocation location;

    SetsunaError(const std::string& msg, SourceLocation loc = {})
        : std::runtime_error(msg), location(loc) {}

    std::string format() const {
        return location.filename + ":" + std::to_string(location.line) + ":" +
               std::to_string(location.column) + ": error: " + what();
    }
};

class LexerError : public SetsunaError {
public:
    using SetsunaError::SetsunaError;
};

class ParseError : public SetsunaError {
public:
    using SetsunaError::SetsunaError;
};

class TypeError : public SetsunaError {
public:
    using SetsunaError::SetsunaError;
};

class RuntimeError : public SetsunaError {
public:
    using SetsunaError::SetsunaError;
};

} // namespace setsuna
