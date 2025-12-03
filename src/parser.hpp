#pragma once

#include <vector>
#include "lexer.hpp"
#include "ast.hpp"

namespace setsuna {

class Parser {
public:
    Parser(const std::vector<Token>& tokens);

    Program parse();

private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;

    // Token helpers
    const Token& current() const;
    const Token& peek(int offset = 1) const;
    bool check(TokenType type) const;
    bool match(TokenType type);
    Token expect(TokenType type, const std::string& msg);
    void advance();
    bool isAtEnd() const;
    void skipNewlines();

    // Declarations
    Decl parseDecl();
    TypeDef parseTypeDef();
    ModuleDef parseModuleDef();
    ImportDecl parseImportDecl();

    // Expressions
    ExprPtr parseExpr();
    ExprPtr parseLetExpr();
    ExprPtr parseFnDef();
    ExprPtr parseIfExpr();
    ExprPtr parseMatchExpr();
    ExprPtr parseBlock();

    // Binary expression with precedence
    ExprPtr parseOr();
    ExprPtr parseAnd();
    ExprPtr parseEquality();
    ExprPtr parseComparison();
    ExprPtr parseTerm();
    ExprPtr parseFactor();
    ExprPtr parseUnary();
    ExprPtr parseCall();
    ExprPtr parsePrimary();

    // Patterns
    PatternPtr parsePattern();

    // Types
    TypeExprPtr parseTypeExpr();

    // Helpers
    std::vector<std::pair<std::string, std::optional<TypeExprPtr>>> parseParams();
    std::vector<ExprPtr> parseArgs();
};

} // namespace setsuna
