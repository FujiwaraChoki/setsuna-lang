#pragma once

#include <string>
#include <vector>
#include <variant>
#include "error.hpp"

namespace setsuna {

enum class TokenType {
    // Literals
    INT,
    FLOAT,
    STRING,
    FSTRING,  // Interpolated string f"..."
    IDENT,

    // Keywords
    LET,
    CONST,
    FN,
    IF,
    ELSE,
    MATCH,
    WHILE,
    FOR,
    IN,
    AS,
    TYPE,
    MODULE,
    IMPORT,
    TRUE,
    FALSE,

    // Operators
    PLUS,       // +
    MINUS,      // -
    STAR,       // *
    SLASH,      // /
    PERCENT,    // %
    EQ,         // ==
    NEQ,        // !=
    LT,         // <
    GT,         // >
    LTE,        // <=
    GTE,        // >=
    AND,        // &&
    OR,         // ||
    NOT,        // !
    ASSIGN,     // =
    ARROW,      // =>
    PIPE,       // |

    // Delimiters
    LPAREN,     // (
    RPAREN,     // )
    LBRACE,     // {
    RBRACE,     // }
    LBRACKET,   // [
    RBRACKET,   // ]
    MAP_START,  // %{
    COMMA,      // ,
    COLON,       // :
    DOUBLE_COLON, // ::
    SEMICOLON,  // ;
    DOT,        // .
    DOTDOTDOT,  // ...

    // Special
    NEWLINE,
    END_OF_FILE,
};

std::string tokenTypeToString(TokenType type);

struct Token {
    TokenType type;
    std::variant<std::monostate, int64_t, double, std::string> value;
    SourceLocation location;

    Token(TokenType t, SourceLocation loc) : type(t), location(loc) {}

    template<typename T>
    Token(TokenType t, T val, SourceLocation loc) : type(t), value(val), location(loc) {}

    std::string toString() const;

    int64_t asInt() const { return std::get<int64_t>(value); }
    double asFloat() const { return std::get<double>(value); }
    const std::string& asString() const { return std::get<std::string>(value); }
};

class Lexer {
public:
    Lexer(const std::string& source, const std::string& filename = "<stdin>");

    std::vector<Token> tokenize();
    Token nextToken();

private:
    std::string source_;
    std::string filename_;
    size_t pos_ = 0;
    int line_ = 1;
    int column_ = 1;

    char current() const;
    char peek(int offset = 1) const;
    void advance();
    void skipWhitespace();
    void skipComment();

    Token makeToken(TokenType type);
    Token readNumber();
    Token readString();
    Token readFString();
    Token readIdentOrKeyword();

    SourceLocation currentLocation() const;
};

} // namespace setsuna
