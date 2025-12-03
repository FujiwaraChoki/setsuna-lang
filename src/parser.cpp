#include "parser.hpp"
#include <iostream>

namespace setsuna {

Parser::Parser(const std::vector<Token>& tokens) : tokens_(tokens) {}

const Token& Parser::current() const {
    if (pos_ >= tokens_.size()) return tokens_.back();
    return tokens_[pos_];
}

const Token& Parser::peek(int offset) const {
    size_t idx = pos_ + offset;
    if (idx >= tokens_.size()) return tokens_.back();
    return tokens_[idx];
}

bool Parser::check(TokenType type) const {
    return current().type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

Token Parser::expect(TokenType type, const std::string& msg) {
    if (!check(type)) {
        throw ParseError(msg + ", got " + tokenTypeToString(current().type), current().location);
    }
    Token tok = current();
    advance();
    return tok;
}

void Parser::advance() {
    if (!isAtEnd()) pos_++;
}

bool Parser::isAtEnd() const {
    return current().type == TokenType::END_OF_FILE;
}

void Parser::skipNewlines() {
    while (check(TokenType::NEWLINE)) advance();
}

// ============ Program ============

Program Parser::parse() {
    Program prog;
    skipNewlines();

    while (!isAtEnd()) {
        prog.declarations.push_back(parseDecl());
        skipNewlines();
    }

    return prog;
}

Decl Parser::parseDecl() {
    skipNewlines();

    if (check(TokenType::TYPE)) {
        return parseTypeDef();
    }
    if (check(TokenType::MODULE)) {
        return parseModuleDef();
    }
    if (check(TokenType::IMPORT)) {
        return parseImportDecl();
    }

    return Decl(parseExpr());
}

// ============ Type Definitions ============

TypeDef Parser::parseTypeDef() {
    SourceLocation loc = current().location;
    expect(TokenType::TYPE, "Expected 'type'");

    Token nameTok = expect(TokenType::IDENT, "Expected type name");
    std::string name = nameTok.asString();

    // Optional type parameters <T, U>
    std::vector<std::string> typeParams;
    if (match(TokenType::LT)) {
        do {
            Token param = expect(TokenType::IDENT, "Expected type parameter");
            typeParams.push_back(param.asString());
        } while (match(TokenType::COMMA));
        expect(TokenType::GT, "Expected '>'");
    }

    expect(TokenType::LBRACE, "Expected '{'");
    skipNewlines();

    std::vector<TypeConstructor> constructors;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        Token ctorName = expect(TokenType::IDENT, "Expected constructor name");
        TypeConstructor ctor;
        ctor.name = ctorName.asString();

        if (match(TokenType::LPAREN)) {
            if (!check(TokenType::RPAREN)) {
                do {
                    ctor.fields.push_back(parseTypeExpr());
                } while (match(TokenType::COMMA));
            }
            expect(TokenType::RPAREN, "Expected ')'");
        }

        constructors.push_back(ctor);

        if (!check(TokenType::RBRACE)) {
            if (!match(TokenType::COMMA)) {
                skipNewlines();
            }
        }
        skipNewlines();
    }

    expect(TokenType::RBRACE, "Expected '}'");

    return TypeDef{name, typeParams, constructors, loc};
}

// ============ Modules ============

ModuleDef Parser::parseModuleDef() {
    SourceLocation loc = current().location;
    expect(TokenType::MODULE, "Expected 'module'");

    Token nameTok = expect(TokenType::IDENT, "Expected module name");
    std::string name = nameTok.asString();

    expect(TokenType::LBRACE, "Expected '{'");
    skipNewlines();

    std::vector<ExprPtr> body;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        body.push_back(parseExpr());
        skipNewlines();
    }

    expect(TokenType::RBRACE, "Expected '}'");

    return ModuleDef{name, body, loc};
}

ImportDecl Parser::parseImportDecl() {
    SourceLocation loc = current().location;
    expect(TokenType::IMPORT, "Expected 'import'");

    Token nameTok = expect(TokenType::IDENT, "Expected module name");
    std::string name = nameTok.asString();

    std::optional<std::string> alias;
    // Could add 'as' keyword support here

    match(TokenType::SEMICOLON);

    return ImportDecl{name, alias, loc};
}

// ============ Expressions ============

ExprPtr Parser::parseExpr() {
    skipNewlines();

    if (check(TokenType::LET)) return parseLetExpr();
    if (check(TokenType::FN)) return parseFnDef();
    if (check(TokenType::IF)) return parseIfExpr();
    if (check(TokenType::MATCH)) return parseMatchExpr();

    // Check for block vs record - record has { ident: ... }
    if (check(TokenType::LBRACE)) {
        int lookOffset = 1;
        while (peek(lookOffset).type == TokenType::NEWLINE) lookOffset++;
        bool isRecord = peek(lookOffset).type == TokenType::IDENT &&
                        peek(lookOffset + 1).type == TokenType::COLON;
        if (!isRecord) {
            return parseBlock();
        }
        // Fall through to parseOr -> parsePrimary for record
    }

    return parseOr();
}

ExprPtr Parser::parseLetExpr() {
    SourceLocation loc = current().location;
    expect(TokenType::LET, "Expected 'let'");

    Token nameTok = expect(TokenType::IDENT, "Expected identifier");
    std::string name = nameTok.asString();

    std::optional<TypeExprPtr> typeAnnotation;
    if (match(TokenType::COLON)) {
        typeAnnotation = parseTypeExpr();
    }

    expect(TokenType::ASSIGN, "Expected '='");
    ExprPtr value = parseExpr();
    match(TokenType::SEMICOLON);

    return makeExpr(LetExpr{name, typeAnnotation, value, loc});
}

ExprPtr Parser::parseFnDef() {
    SourceLocation loc = current().location;
    expect(TokenType::FN, "Expected 'fn'");

    Token nameTok = expect(TokenType::IDENT, "Expected function name");
    std::string name = nameTok.asString();

    auto params = parseParams();

    std::optional<TypeExprPtr> returnType;
    if (match(TokenType::COLON)) {
        returnType = parseTypeExpr();
    }

    ExprPtr body;
    if (match(TokenType::ARROW)) {
        body = parseExpr();
    } else {
        body = parseBlock();
    }

    match(TokenType::SEMICOLON);

    return makeExpr(FnDef{name, params, returnType, body, loc});
}

ExprPtr Parser::parseIfExpr() {
    SourceLocation loc = current().location;
    expect(TokenType::IF, "Expected 'if'");

    ExprPtr condition = parseExpr();
    ExprPtr thenBranch = parseBlock();

    ExprPtr elseBranch = nullptr;
    if (match(TokenType::ELSE)) {
        if (check(TokenType::IF)) {
            elseBranch = parseIfExpr();
        } else {
            elseBranch = parseBlock();
        }
    }

    return makeExpr(IfExpr{condition, thenBranch, elseBranch, loc});
}

ExprPtr Parser::parseMatchExpr() {
    SourceLocation loc = current().location;
    expect(TokenType::MATCH, "Expected 'match'");

    ExprPtr scrutinee = parseExpr();
    expect(TokenType::LBRACE, "Expected '{'");
    skipNewlines();

    std::vector<MatchArm> arms;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        PatternPtr pattern = parsePattern();

        std::optional<ExprPtr> guard;
        if (match(TokenType::IF)) {
            guard = parseExpr();
        }

        expect(TokenType::ARROW, "Expected '=>'");
        ExprPtr body = parseExpr();

        arms.push_back(MatchArm{pattern, guard, body});

        if (!check(TokenType::RBRACE)) {
            match(TokenType::COMMA);
        }
        skipNewlines();
    }

    expect(TokenType::RBRACE, "Expected '}'");

    return makeExpr(MatchExpr{scrutinee, arms, loc});
}

ExprPtr Parser::parseBlock() {
    SourceLocation loc = current().location;
    expect(TokenType::LBRACE, "Expected '{'");
    skipNewlines();

    std::vector<ExprPtr> exprs;
    while (!check(TokenType::RBRACE) && !isAtEnd()) {
        exprs.push_back(parseExpr());
        skipNewlines();
    }

    expect(TokenType::RBRACE, "Expected '}'");

    return makeExpr(Block{exprs, loc});
}

// ============ Binary Operators (Precedence Climbing) ============

ExprPtr Parser::parseOr() {
    ExprPtr left = parseAnd();

    while (check(TokenType::OR)) {
        SourceLocation loc = current().location;
        advance();
        ExprPtr right = parseAnd();
        left = makeExpr(BinaryOp{BinaryOp::Op::OR, left, right, loc});
    }

    return left;
}

ExprPtr Parser::parseAnd() {
    ExprPtr left = parseEquality();

    while (check(TokenType::AND)) {
        SourceLocation loc = current().location;
        advance();
        ExprPtr right = parseEquality();
        left = makeExpr(BinaryOp{BinaryOp::Op::AND, left, right, loc});
    }

    return left;
}

ExprPtr Parser::parseEquality() {
    ExprPtr left = parseComparison();

    while (check(TokenType::EQ) || check(TokenType::NEQ)) {
        SourceLocation loc = current().location;
        BinaryOp::Op op = check(TokenType::EQ) ? BinaryOp::Op::EQ : BinaryOp::Op::NEQ;
        advance();
        ExprPtr right = parseComparison();
        left = makeExpr(BinaryOp{op, left, right, loc});
    }

    return left;
}

ExprPtr Parser::parseComparison() {
    ExprPtr left = parseTerm();

    while (check(TokenType::LT) || check(TokenType::GT) ||
           check(TokenType::LTE) || check(TokenType::GTE)) {
        SourceLocation loc = current().location;
        BinaryOp::Op op;
        if (check(TokenType::LT)) op = BinaryOp::Op::LT;
        else if (check(TokenType::GT)) op = BinaryOp::Op::GT;
        else if (check(TokenType::LTE)) op = BinaryOp::Op::LTE;
        else op = BinaryOp::Op::GTE;
        advance();
        ExprPtr right = parseTerm();
        left = makeExpr(BinaryOp{op, left, right, loc});
    }

    return left;
}

ExprPtr Parser::parseTerm() {
    ExprPtr left = parseFactor();

    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        SourceLocation loc = current().location;
        BinaryOp::Op op = check(TokenType::PLUS) ? BinaryOp::Op::ADD : BinaryOp::Op::SUB;
        advance();
        ExprPtr right = parseFactor();
        left = makeExpr(BinaryOp{op, left, right, loc});
    }

    return left;
}

ExprPtr Parser::parseFactor() {
    ExprPtr left = parseUnary();

    while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT)) {
        SourceLocation loc = current().location;
        BinaryOp::Op op;
        if (check(TokenType::STAR)) op = BinaryOp::Op::MUL;
        else if (check(TokenType::SLASH)) op = BinaryOp::Op::DIV;
        else op = BinaryOp::Op::MOD;
        advance();
        ExprPtr right = parseUnary();
        left = makeExpr(BinaryOp{op, left, right, loc});
    }

    return left;
}

ExprPtr Parser::parseUnary() {
    if (check(TokenType::MINUS) || check(TokenType::NOT)) {
        SourceLocation loc = current().location;
        UnaryOp::Op op = check(TokenType::MINUS) ? UnaryOp::Op::NEG : UnaryOp::Op::NOT;
        advance();
        ExprPtr operand = parseUnary();
        return makeExpr(UnaryOp{op, operand, loc});
    }

    return parseCall();
}

ExprPtr Parser::parseCall() {
    ExprPtr expr = parsePrimary();

    while (true) {
        if (check(TokenType::LPAREN)) {
            SourceLocation loc = current().location;
            auto args = parseArgs();
            expr = makeExpr(Call{expr, args, loc});
        } else if (check(TokenType::DOT)) {
            SourceLocation loc = current().location;
            advance();
            Token field = expect(TokenType::IDENT, "Expected field name");
            expr = makeExpr(FieldAccess{expr, field.asString(), loc});
        } else {
            break;
        }
    }

    return expr;
}

ExprPtr Parser::parsePrimary() {
    SourceLocation loc = current().location;

    // Literals
    if (check(TokenType::INT)) {
        int64_t val = current().asInt();
        advance();
        return makeExpr(IntLiteral{val, loc});
    }
    if (check(TokenType::FLOAT)) {
        double val = current().asFloat();
        advance();
        return makeExpr(FloatLiteral{val, loc});
    }
    if (check(TokenType::STRING)) {
        std::string val = current().asString();
        advance();
        return makeExpr(StringLiteral{val, loc});
    }
    if (check(TokenType::TRUE)) {
        advance();
        return makeExpr(BoolLiteral{true, loc});
    }
    if (check(TokenType::FALSE)) {
        advance();
        return makeExpr(BoolLiteral{false, loc});
    }

    // Identifier (field access and module access handled in parseCall)
    if (check(TokenType::IDENT)) {
        std::string name = current().asString();
        advance();
        return makeExpr(Identifier{name, loc});
    }

    // Parenthesized expr, tuple, or lambda
    if (check(TokenType::LPAREN)) {
        advance();

        // Empty parens - unit/empty tuple
        if (check(TokenType::RPAREN)) {
            advance();
            return makeExpr(TupleExpr{{}, loc});
        }

        // Check for lambda: (x) => ... or (x, y) => ...
        if (check(TokenType::IDENT)) {
            // Look ahead to see if this is a lambda
            size_t savedPos = pos_;
            std::vector<std::pair<std::string, std::optional<TypeExprPtr>>> params;

            // Try parsing as params
            bool isLambda = false;
            do {
                if (!check(TokenType::IDENT)) break;
                std::string paramName = current().asString();
                advance();

                std::optional<TypeExprPtr> paramType;
                if (match(TokenType::COLON)) {
                    paramType = parseTypeExpr();
                }

                params.push_back({paramName, paramType});
            } while (match(TokenType::COMMA));

            if (check(TokenType::RPAREN)) {
                advance();
                if (check(TokenType::ARROW)) {
                    isLambda = true;
                }
            }

            if (isLambda) {
                advance();  // skip =>
                ExprPtr body = parseExpr();
                return makeExpr(Lambda{params, body, loc});
            }

            // Not a lambda, restore and parse as expr
            pos_ = savedPos;
        }

        // Parse as expression or tuple
        ExprPtr first = parseExpr();

        if (check(TokenType::COMMA)) {
            // Tuple
            std::vector<ExprPtr> elements;
            elements.push_back(first);
            while (match(TokenType::COMMA)) {
                if (check(TokenType::RPAREN)) break;
                elements.push_back(parseExpr());
            }
            expect(TokenType::RPAREN, "Expected ')'");
            return makeExpr(TupleExpr{elements, loc});
        }

        expect(TokenType::RPAREN, "Expected ')'");
        return first;
    }

    // List
    if (check(TokenType::LBRACKET)) {
        advance();
        std::vector<ExprPtr> elements;

        if (!check(TokenType::RBRACKET)) {
            do {
                elements.push_back(parseExpr());
            } while (match(TokenType::COMMA));
        }

        expect(TokenType::RBRACKET, "Expected ']'");
        return makeExpr(ListExpr{elements, loc});
    }

    // Record
    if (check(TokenType::LBRACE)) {
        // Check if this is a record (has field: value) or a block
        // Look ahead for pattern like { name: ... }
        // Skip any newlines in the lookahead
        int lookOffset = 1;
        while (peek(lookOffset).type == TokenType::NEWLINE) lookOffset++;

        bool isRecord = peek(lookOffset).type == TokenType::IDENT &&
                        peek(lookOffset + 1).type == TokenType::COLON;

        if (isRecord) {
            advance();  // skip {
            std::vector<std::pair<std::string, ExprPtr>> fields;

            do {
                skipNewlines();
                Token fieldName = expect(TokenType::IDENT, "Expected field name");
                expect(TokenType::COLON, "Expected ':'");
                ExprPtr value = parseExpr();
                fields.push_back({fieldName.asString(), value});
            } while (match(TokenType::COMMA));

            skipNewlines();
            expect(TokenType::RBRACE, "Expected '}'");
            return makeExpr(RecordExpr{fields, loc});
        }

        return parseBlock();
    }

    throw ParseError("Unexpected token: " + tokenTypeToString(current().type), loc);
}

// ============ Patterns ============

PatternPtr Parser::parsePattern() {
    SourceLocation loc = current().location;

    // Wildcard
    if (check(TokenType::IDENT) && current().asString() == "_") {
        advance();
        return makePattern(WildcardPattern{loc});
    }

    // Literals
    if (check(TokenType::INT)) {
        int64_t val = current().asInt();
        advance();
        return makePattern(LiteralPattern{val, loc});
    }
    if (check(TokenType::FLOAT)) {
        double val = current().asFloat();
        advance();
        return makePattern(LiteralPattern{val, loc});
    }
    if (check(TokenType::STRING)) {
        std::string val = current().asString();
        advance();
        return makePattern(LiteralPattern{val, loc});
    }
    if (check(TokenType::TRUE)) {
        advance();
        return makePattern(LiteralPattern{true, loc});
    }
    if (check(TokenType::FALSE)) {
        advance();
        return makePattern(LiteralPattern{false, loc});
    }

    // List pattern
    if (check(TokenType::LBRACKET)) {
        advance();
        std::vector<PatternPtr> elements;
        std::optional<std::string> rest;

        if (!check(TokenType::RBRACKET)) {
            do {
                // Check for rest pattern: ...name
                if (check(TokenType::DOTDOTDOT)) {
                    advance();
                    Token restName = expect(TokenType::IDENT, "Expected identifier after '...'");
                    rest = restName.asString();
                    break;
                }
                elements.push_back(parsePattern());
            } while (match(TokenType::COMMA));
        }

        expect(TokenType::RBRACKET, "Expected ']'");
        return makePattern(ListPattern{elements, rest, loc});
    }

    // Tuple pattern
    if (check(TokenType::LPAREN)) {
        advance();
        std::vector<PatternPtr> elements;

        if (!check(TokenType::RPAREN)) {
            do {
                elements.push_back(parsePattern());
            } while (match(TokenType::COMMA));
        }

        expect(TokenType::RPAREN, "Expected ')'");
        return makePattern(TuplePattern{elements, loc});
    }

    // Record pattern
    if (check(TokenType::LBRACE)) {
        advance();
        std::vector<std::pair<std::string, PatternPtr>> fields;

        if (!check(TokenType::RBRACE)) {
            do {
                skipNewlines();
                Token fieldName = expect(TokenType::IDENT, "Expected field name");
                expect(TokenType::COLON, "Expected ':'");
                PatternPtr pat = parsePattern();
                fields.push_back({fieldName.asString(), pat});
            } while (match(TokenType::COMMA));
        }

        skipNewlines();
        expect(TokenType::RBRACE, "Expected '}'");
        return makePattern(RecordPattern{fields, loc});
    }

    // Variable or constructor pattern
    if (check(TokenType::IDENT)) {
        std::string name = current().asString();
        advance();

        // Constructor pattern: Some(x)
        if (check(TokenType::LPAREN)) {
            advance();
            std::vector<PatternPtr> args;

            if (!check(TokenType::RPAREN)) {
                do {
                    args.push_back(parsePattern());
                } while (match(TokenType::COMMA));
            }

            expect(TokenType::RPAREN, "Expected ')'");
            return makePattern(ConstructorPattern{name, args, loc});
        }

        // Just a variable binding
        return makePattern(VarPattern{name, loc});
    }

    throw ParseError("Expected pattern", loc);
}

// ============ Types ============

TypeExprPtr Parser::parseTypeExpr() {
    SourceLocation loc = current().location;

    // Function type: (Int, Int) -> Int
    if (check(TokenType::LPAREN)) {
        advance();
        std::vector<TypeExprPtr> paramTypes;

        if (!check(TokenType::RPAREN)) {
            do {
                paramTypes.push_back(parseTypeExpr());
            } while (match(TokenType::COMMA));
        }

        expect(TokenType::RPAREN, "Expected ')'");

        if (match(TokenType::ARROW)) {
            TypeExprPtr returnType = parseTypeExpr();
            return makeTypeExpr(FnType{paramTypes, returnType, loc});
        }

        // Tuple type
        return makeTypeExpr(TupleType{paramTypes, loc});
    }

    // List type: [Int]
    if (check(TokenType::LBRACKET)) {
        advance();
        TypeExprPtr elemType = parseTypeExpr();
        expect(TokenType::RBRACKET, "Expected ']'");
        return makeTypeExpr(ListType{elemType, loc});
    }

    // Named type: Int, Option<T>, etc.
    if (check(TokenType::IDENT)) {
        std::string name = current().asString();
        advance();

        std::vector<TypeExprPtr> typeArgs;
        if (match(TokenType::LT)) {
            do {
                typeArgs.push_back(parseTypeExpr());
            } while (match(TokenType::COMMA));
            expect(TokenType::GT, "Expected '>'");
        }

        return makeTypeExpr(TypeName{name, typeArgs, loc});
    }

    throw ParseError("Expected type expression", loc);
}

// ============ Helpers ============

std::vector<std::pair<std::string, std::optional<TypeExprPtr>>> Parser::parseParams() {
    std::vector<std::pair<std::string, std::optional<TypeExprPtr>>> params;

    expect(TokenType::LPAREN, "Expected '('");

    if (!check(TokenType::RPAREN)) {
        do {
            Token name = expect(TokenType::IDENT, "Expected parameter name");

            std::optional<TypeExprPtr> type;
            if (match(TokenType::COLON)) {
                type = parseTypeExpr();
            }

            params.push_back({name.asString(), type});
        } while (match(TokenType::COMMA));
    }

    expect(TokenType::RPAREN, "Expected ')'");
    return params;
}

std::vector<ExprPtr> Parser::parseArgs() {
    std::vector<ExprPtr> args;

    expect(TokenType::LPAREN, "Expected '('");

    if (!check(TokenType::RPAREN)) {
        do {
            args.push_back(parseExpr());
        } while (match(TokenType::COMMA));
    }

    expect(TokenType::RPAREN, "Expected ')'");
    return args;
}

} // namespace setsuna
