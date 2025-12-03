#include "lexer.hpp"
#include <unordered_map>
#include <cctype>

namespace setsuna {

static const std::unordered_map<std::string, TokenType> keywords = {
    {"let", TokenType::LET},
    {"fn", TokenType::FN},
    {"if", TokenType::IF},
    {"else", TokenType::ELSE},
    {"match", TokenType::MATCH},
    {"type", TokenType::TYPE},
    {"module", TokenType::MODULE},
    {"import", TokenType::IMPORT},
    {"true", TokenType::TRUE},
    {"false", TokenType::FALSE},
};

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::INT: return "INT";
        case TokenType::FLOAT: return "FLOAT";
        case TokenType::STRING: return "STRING";
        case TokenType::IDENT: return "IDENT";
        case TokenType::LET: return "LET";
        case TokenType::FN: return "FN";
        case TokenType::IF: return "IF";
        case TokenType::ELSE: return "ELSE";
        case TokenType::MATCH: return "MATCH";
        case TokenType::TYPE: return "TYPE";
        case TokenType::MODULE: return "MODULE";
        case TokenType::IMPORT: return "IMPORT";
        case TokenType::TRUE: return "TRUE";
        case TokenType::FALSE: return "FALSE";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::STAR: return "STAR";
        case TokenType::SLASH: return "SLASH";
        case TokenType::PERCENT: return "PERCENT";
        case TokenType::EQ: return "EQ";
        case TokenType::NEQ: return "NEQ";
        case TokenType::LT: return "LT";
        case TokenType::GT: return "GT";
        case TokenType::LTE: return "LTE";
        case TokenType::GTE: return "GTE";
        case TokenType::AND: return "AND";
        case TokenType::OR: return "OR";
        case TokenType::NOT: return "NOT";
        case TokenType::ASSIGN: return "ASSIGN";
        case TokenType::ARROW: return "ARROW";
        case TokenType::PIPE: return "PIPE";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::LBRACKET: return "LBRACKET";
        case TokenType::RBRACKET: return "RBRACKET";
        case TokenType::COMMA: return "COMMA";
        case TokenType::COLON: return "COLON";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::DOT: return "DOT";
        case TokenType::DOTDOTDOT: return "DOTDOTDOT";
        case TokenType::NEWLINE: return "NEWLINE";
        case TokenType::END_OF_FILE: return "EOF";
    }
    return "UNKNOWN";
}

std::string Token::toString() const {
    std::string result = tokenTypeToString(type);
    if (std::holds_alternative<int64_t>(value)) {
        result += "(" + std::to_string(std::get<int64_t>(value)) + ")";
    } else if (std::holds_alternative<double>(value)) {
        result += "(" + std::to_string(std::get<double>(value)) + ")";
    } else if (std::holds_alternative<std::string>(value)) {
        result += "(\"" + std::get<std::string>(value) + "\")";
    }
    return result;
}

Lexer::Lexer(const std::string& source, const std::string& filename)
    : source_(source), filename_(filename) {}

char Lexer::current() const {
    if (pos_ >= source_.size()) return '\0';
    return source_[pos_];
}

char Lexer::peek(int offset) const {
    size_t idx = pos_ + offset;
    if (idx >= source_.size()) return '\0';
    return source_[idx];
}

void Lexer::advance() {
    if (current() == '\n') {
        line_++;
        column_ = 1;
    } else {
        column_++;
    }
    pos_++;
}

SourceLocation Lexer::currentLocation() const {
    return {line_, column_, filename_};
}

void Lexer::skipWhitespace() {
    while (current() == ' ' || current() == '\t' || current() == '\r') {
        advance();
    }
}

void Lexer::skipComment() {
    if (current() == '/' && peek() == '/') {
        while (current() != '\n' && current() != '\0') {
            advance();
        }
    }
}

Token Lexer::makeToken(TokenType type) {
    return Token(type, currentLocation());
}

Token Lexer::readNumber() {
    SourceLocation loc = currentLocation();
    std::string num;
    bool isFloat = false;

    while (std::isdigit(current())) {
        num += current();
        advance();
    }

    if (current() == '.' && std::isdigit(peek())) {
        isFloat = true;
        num += current();
        advance();
        while (std::isdigit(current())) {
            num += current();
            advance();
        }
    }

    if (isFloat) {
        return Token(TokenType::FLOAT, std::stod(num), loc);
    }
    return Token(TokenType::INT, std::stoll(num), loc);
}

Token Lexer::readString() {
    SourceLocation loc = currentLocation();
    advance(); // skip opening quote

    std::string str;
    while (current() != '"' && current() != '\0') {
        if (current() == '\\') {
            advance();
            switch (current()) {
                case 'n': str += '\n'; break;
                case 't': str += '\t'; break;
                case 'r': str += '\r'; break;
                case '\\': str += '\\'; break;
                case '"': str += '"'; break;
                default: str += current();
            }
        } else {
            str += current();
        }
        advance();
    }

    if (current() == '\0') {
        throw LexerError("Unterminated string literal", loc);
    }
    advance(); // skip closing quote

    return Token(TokenType::STRING, str, loc);
}

Token Lexer::readIdentOrKeyword() {
    SourceLocation loc = currentLocation();
    std::string ident;

    while (std::isalnum(current()) || current() == '_') {
        ident += current();
        advance();
    }

    auto it = keywords.find(ident);
    if (it != keywords.end()) {
        return Token(it->second, loc);
    }
    return Token(TokenType::IDENT, ident, loc);
}

Token Lexer::nextToken() {
    skipWhitespace();
    skipComment();
    skipWhitespace();

    if (current() == '\0') {
        return makeToken(TokenType::END_OF_FILE);
    }

    SourceLocation loc = currentLocation();

    // Newlines (significant in some contexts)
    if (current() == '\n') {
        advance();
        return Token(TokenType::NEWLINE, loc);
    }

    // Numbers
    if (std::isdigit(current())) {
        return readNumber();
    }

    // Strings
    if (current() == '"') {
        return readString();
    }

    // Identifiers and keywords
    if (std::isalpha(current()) || current() == '_') {
        return readIdentOrKeyword();
    }

    // Multi-character operators
    if (current() == '=' && peek() == '>') {
        advance(); advance();
        return Token(TokenType::ARROW, loc);
    }
    if (current() == '=' && peek() == '=') {
        advance(); advance();
        return Token(TokenType::EQ, loc);
    }
    if (current() == '!' && peek() == '=') {
        advance(); advance();
        return Token(TokenType::NEQ, loc);
    }
    if (current() == '<' && peek() == '=') {
        advance(); advance();
        return Token(TokenType::LTE, loc);
    }
    if (current() == '>' && peek() == '=') {
        advance(); advance();
        return Token(TokenType::GTE, loc);
    }
    if (current() == '&' && peek() == '&') {
        advance(); advance();
        return Token(TokenType::AND, loc);
    }
    if (current() == '|' && peek() == '|') {
        advance(); advance();
        return Token(TokenType::OR, loc);
    }
    if (current() == '.' && peek() == '.' && peek(2) == '.') {
        advance(); advance(); advance();
        return Token(TokenType::DOTDOTDOT, loc);
    }

    // Single-character tokens
    char c = current();
    advance();

    switch (c) {
        case '+': return Token(TokenType::PLUS, loc);
        case '-': return Token(TokenType::MINUS, loc);
        case '*': return Token(TokenType::STAR, loc);
        case '/': return Token(TokenType::SLASH, loc);
        case '%': return Token(TokenType::PERCENT, loc);
        case '<': return Token(TokenType::LT, loc);
        case '>': return Token(TokenType::GT, loc);
        case '!': return Token(TokenType::NOT, loc);
        case '=': return Token(TokenType::ASSIGN, loc);
        case '|': return Token(TokenType::PIPE, loc);
        case '(': return Token(TokenType::LPAREN, loc);
        case ')': return Token(TokenType::RPAREN, loc);
        case '{': return Token(TokenType::LBRACE, loc);
        case '}': return Token(TokenType::RBRACE, loc);
        case '[': return Token(TokenType::LBRACKET, loc);
        case ']': return Token(TokenType::RBRACKET, loc);
        case ',': return Token(TokenType::COMMA, loc);
        case ':': return Token(TokenType::COLON, loc);
        case ';': return Token(TokenType::SEMICOLON, loc);
        case '.': return Token(TokenType::DOT, loc);
        default:
            throw LexerError(std::string("Unexpected character: '") + c + "'", loc);
    }
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (true) {
        Token tok = nextToken();
        tokens.push_back(tok);
        if (tok.type == TokenType::END_OF_FILE) break;
    }
    return tokens;
}

} // namespace setsuna
