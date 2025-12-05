#include "builtins.hpp"
#include "error.hpp"
#include <iostream>
#include <cmath>
#include <sstream>
#include <random>
#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <curl/curl.h>

namespace fs = std::filesystem;

namespace setsuna {

// Random number generator for random() and random_int()
static std::random_device rd;
static std::mt19937 gen(rd());

// ============ HTTP Helper Functions ============

// CURL write callback - collects response body
static size_t curlWriteCallback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t totalSize = size * nmemb;
    output->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}

// CURL header callback - collects response headers
static size_t curlHeaderCallback(char* buffer, size_t size, size_t nitems, std::vector<std::pair<std::string, std::string>>* headers) {
    size_t totalSize = size * nitems;
    std::string line(buffer, totalSize);

    // Remove trailing \r\n
    while (!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
        line.pop_back();
    }

    // Parse header line
    size_t colonPos = line.find(':');
    if (colonPos != std::string::npos) {
        std::string key = line.substr(0, colonPos);
        std::string value = line.substr(colonPos + 1);
        // Trim leading whitespace from value
        size_t start = value.find_first_not_of(" \t");
        if (start != std::string::npos) {
            value = value.substr(start);
        }
        headers->emplace_back(key, value);
    }

    return totalSize;
}

// JSON Parser for http responses
class JsonParser {
public:
    static ValuePtr parse(const std::string& json) {
        size_t pos = 0;
        return parseValue(json, pos);
    }

private:
    static void skipWhitespace(const std::string& json, size_t& pos) {
        while (pos < json.size() && std::isspace(json[pos])) {
            pos++;
        }
    }

    static ValuePtr parseValue(const std::string& json, size_t& pos) {
        skipWhitespace(json, pos);
        if (pos >= json.size()) {
            throw RuntimeError("json_parse: unexpected end of input");
        }

        char c = json[pos];
        if (c == '{') return parseObject(json, pos);
        if (c == '[') return parseArray(json, pos);
        if (c == '"') return parseString(json, pos);
        if (c == 't' || c == 'f') return parseBool(json, pos);
        if (c == 'n') return parseNull(json, pos);
        if (c == '-' || std::isdigit(c)) return parseNumber(json, pos);

        throw RuntimeError("json_parse: unexpected character '" + std::string(1, c) + "'");
    }

    static ValuePtr parseObject(const std::string& json, size_t& pos) {
        pos++; // Skip '{'
        skipWhitespace(json, pos);

        RecordValue record;

        if (pos < json.size() && json[pos] == '}') {
            pos++;
            return makeRecord(record);
        }

        while (true) {
            skipWhitespace(json, pos);

            // Parse key
            if (json[pos] != '"') {
                throw RuntimeError("json_parse: expected string key in object");
            }
            auto keyVal = parseString(json, pos);
            std::string key = keyVal->asString();

            skipWhitespace(json, pos);
            if (json[pos] != ':') {
                throw RuntimeError("json_parse: expected ':' after object key");
            }
            pos++; // Skip ':'

            // Parse value
            auto value = parseValue(json, pos);
            record.fields[key] = value;

            skipWhitespace(json, pos);
            if (json[pos] == '}') {
                pos++;
                break;
            }
            if (json[pos] != ',') {
                throw RuntimeError("json_parse: expected ',' or '}' in object");
            }
            pos++; // Skip ','
        }

        return makeRecord(record);
    }

    static ValuePtr parseArray(const std::string& json, size_t& pos) {
        pos++; // Skip '['
        skipWhitespace(json, pos);

        std::vector<ValuePtr> elements;

        if (pos < json.size() && json[pos] == ']') {
            pos++;
            return makeList(elements);
        }

        while (true) {
            auto value = parseValue(json, pos);
            elements.push_back(value);

            skipWhitespace(json, pos);
            if (json[pos] == ']') {
                pos++;
                break;
            }
            if (json[pos] != ',') {
                throw RuntimeError("json_parse: expected ',' or ']' in array");
            }
            pos++; // Skip ','
        }

        return makeList(elements);
    }

    static ValuePtr parseString(const std::string& json, size_t& pos) {
        pos++; // Skip opening '"'
        std::string result;

        while (pos < json.size() && json[pos] != '"') {
            if (json[pos] == '\\') {
                pos++;
                if (pos >= json.size()) {
                    throw RuntimeError("json_parse: unexpected end in string escape");
                }
                switch (json[pos]) {
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    case '/': result += '/'; break;
                    case 'b': result += '\b'; break;
                    case 'f': result += '\f'; break;
                    case 'n': result += '\n'; break;
                    case 'r': result += '\r'; break;
                    case 't': result += '\t'; break;
                    case 'u': {
                        // Unicode escape \uXXXX
                        if (pos + 4 >= json.size()) {
                            throw RuntimeError("json_parse: invalid unicode escape");
                        }
                        std::string hex = json.substr(pos + 1, 4);
                        unsigned int codepoint = std::stoul(hex, nullptr, 16);
                        // Simple UTF-8 encoding for BMP characters
                        if (codepoint < 0x80) {
                            result += static_cast<char>(codepoint);
                        } else if (codepoint < 0x800) {
                            result += static_cast<char>(0xC0 | (codepoint >> 6));
                            result += static_cast<char>(0x80 | (codepoint & 0x3F));
                        } else {
                            result += static_cast<char>(0xE0 | (codepoint >> 12));
                            result += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
                            result += static_cast<char>(0x80 | (codepoint & 0x3F));
                        }
                        pos += 4;
                        break;
                    }
                    default:
                        throw RuntimeError("json_parse: invalid escape sequence");
                }
            } else {
                result += json[pos];
            }
            pos++;
        }

        if (pos >= json.size()) {
            throw RuntimeError("json_parse: unterminated string");
        }
        pos++; // Skip closing '"'
        return makeString(result);
    }

    static ValuePtr parseNumber(const std::string& json, size_t& pos) {
        size_t start = pos;
        bool isFloat = false;

        if (json[pos] == '-') pos++;

        while (pos < json.size() && std::isdigit(json[pos])) pos++;

        if (pos < json.size() && json[pos] == '.') {
            isFloat = true;
            pos++;
            while (pos < json.size() && std::isdigit(json[pos])) pos++;
        }

        if (pos < json.size() && (json[pos] == 'e' || json[pos] == 'E')) {
            isFloat = true;
            pos++;
            if (pos < json.size() && (json[pos] == '+' || json[pos] == '-')) pos++;
            while (pos < json.size() && std::isdigit(json[pos])) pos++;
        }

        std::string numStr = json.substr(start, pos - start);
        if (isFloat) {
            return makeFloat(std::stod(numStr));
        } else {
            return makeInt(std::stoll(numStr));
        }
    }

    static ValuePtr parseBool(const std::string& json, size_t& pos) {
        if (json.substr(pos, 4) == "true") {
            pos += 4;
            return makeBool(true);
        }
        if (json.substr(pos, 5) == "false") {
            pos += 5;
            return makeBool(false);
        }
        throw RuntimeError("json_parse: invalid boolean");
    }

    static ValuePtr parseNull(const std::string& json, size_t& pos) {
        if (json.substr(pos, 4) == "null") {
            pos += 4;
            return makeUnit();
        }
        throw RuntimeError("json_parse: invalid null");
    }
};

// JSON Stringify helper
static std::string jsonStringify(ValuePtr val, int indent = 0, bool pretty = false) {
    val = force(val);

    auto indentStr = [&](int level) -> std::string {
        return pretty ? std::string(level * 2, ' ') : "";
    };
    auto newline = [&]() -> std::string {
        return pretty ? "\n" : "";
    };

    if (val->isUnit()) {
        return "null";
    }
    if (val->isBool()) {
        return val->asBool() ? "true" : "false";
    }
    if (val->isInt()) {
        return std::to_string(val->asInt());
    }
    if (val->isFloat()) {
        std::ostringstream oss;
        oss << val->asFloat();
        return oss.str();
    }
    if (val->isString()) {
        std::string result = "\"";
        for (char c : val->asString()) {
            switch (c) {
                case '"': result += "\\\""; break;
                case '\\': result += "\\\\"; break;
                case '\b': result += "\\b"; break;
                case '\f': result += "\\f"; break;
                case '\n': result += "\\n"; break;
                case '\r': result += "\\r"; break;
                case '\t': result += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20) {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                        result += buf;
                    } else {
                        result += c;
                    }
            }
        }
        result += "\"";
        return result;
    }
    if (val->isList()) {
        const auto& list = val->asList();
        if (list.empty()) return "[]";

        std::string result = "[" + newline();
        for (size_t i = 0; i < list.size(); i++) {
            result += indentStr(indent + 1) + jsonStringify(list[i], indent + 1, pretty);
            if (i < list.size() - 1) result += ",";
            result += newline();
        }
        result += indentStr(indent) + "]";
        return result;
    }
    if (val->isRecord()) {
        const auto& record = val->asRecord();
        if (record.fields.empty()) return "{}";

        std::string result = "{" + newline();
        size_t i = 0;
        for (const auto& [key, value] : record.fields) {
            result += indentStr(indent + 1) + "\"" + key + "\":" + (pretty ? " " : "") + jsonStringify(value, indent + 1, pretty);
            if (i < record.fields.size() - 1) result += ",";
            result += newline();
            i++;
        }
        result += indentStr(indent) + "}";
        return result;
    }
    if (val->isTuple()) {
        // Represent tuples as arrays
        const auto& tuple = val->asTuple();
        if (tuple.empty()) return "[]";

        std::string result = "[" + newline();
        for (size_t i = 0; i < tuple.size(); i++) {
            result += indentStr(indent + 1) + jsonStringify(tuple[i], indent + 1, pretty);
            if (i < tuple.size() - 1) result += ",";
            result += newline();
        }
        result += indentStr(indent) + "]";
        return result;
    }

    throw RuntimeError("json_stringify: cannot convert value to JSON");
}

void registerBuiltins(EnvPtr env) {
    // print(value) - Print a value
    env->define("print", makeBuiltin("print", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        if (val->isString()) {
            std::cout << val->asString() << std::endl;
        } else {
            std::cout << val->toString() << std::endl;
        }
        return makeUnit();
    }));

    // println(value) - Print a value with newline
    env->define("println", makeBuiltin("println", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        if (val->isString()) {
            std::cout << val->asString() << std::endl;
        } else {
            std::cout << val->toString() << std::endl;
        }
        return makeUnit();
    }));

    // str(value) - Convert to string
    env->define("str", makeBuiltin("str", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        if (val->isString()) {
            return val;
        }
        return makeString(val->toString());
    }));

    // int(value) - Convert to int
    env->define("int", makeBuiltin("int", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        if (val->isInt()) return val;
        if (val->isFloat()) return makeInt(static_cast<int64_t>(val->asFloat()));
        if (val->isString()) return makeInt(std::stoll(val->asString()));
        throw RuntimeError("Cannot convert to int");
    }));

    // float(value) - Convert to float
    env->define("float", makeBuiltin("float", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        if (val->isFloat()) return val;
        if (val->isInt()) return makeFloat(static_cast<double>(val->asInt()));
        if (val->isString()) return makeFloat(std::stod(val->asString()));
        throw RuntimeError("Cannot convert to float");
    }));

    // ============ List operations ============

    // head(list) - Get first element
    env->define("head", makeBuiltin("head", 1, [](const std::vector<ValuePtr>& args) {
        auto list = force(args[0]);
        if (!list->isList()) throw RuntimeError("head: expected list");
        const auto& lst = list->asList();
        if (lst.empty()) throw RuntimeError("head: empty list");
        return lst[0];
    }));

    // tail(list) - Get all but first element
    env->define("tail", makeBuiltin("tail", 1, [](const std::vector<ValuePtr>& args) {
        auto list = force(args[0]);
        if (!list->isList()) throw RuntimeError("tail: expected list");
        const auto& lst = list->asList();
        if (lst.empty()) throw RuntimeError("tail: empty list");
        std::vector<ValuePtr> result(lst.begin() + 1, lst.end());
        return makeList(result);
    }));

    // cons(elem, list) - Prepend element to list
    env->define("cons", makeBuiltin("cons", 2, [](const std::vector<ValuePtr>& args) {
        auto elem = force(args[0]);
        auto list = force(args[1]);
        if (!list->isList()) throw RuntimeError("cons: expected list");
        std::vector<ValuePtr> result = {elem};
        const auto& lst = list->asList();
        result.insert(result.end(), lst.begin(), lst.end());
        return makeList(result);
    }));

    // len(list) - Get length
    env->define("len", makeBuiltin("len", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        if (val->isList()) {
            return makeInt(static_cast<int64_t>(val->asList().size()));
        }
        if (val->isString()) {
            return makeInt(static_cast<int64_t>(val->asString().size()));
        }
        if (val->isTuple()) {
            return makeInt(static_cast<int64_t>(val->asTuple().size()));
        }
        throw RuntimeError("len: expected list, string, or tuple");
    }));

    // empty(list) - Check if empty
    env->define("empty", makeBuiltin("empty", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        if (val->isList()) {
            return makeBool(val->asList().empty());
        }
        if (val->isString()) {
            return makeBool(val->asString().empty());
        }
        throw RuntimeError("empty: expected list or string");
    }));

    // append(list, elem) - Append element to list
    env->define("append", makeBuiltin("append", 2, [](const std::vector<ValuePtr>& args) {
        auto list = force(args[0]);
        auto elem = force(args[1]);
        if (!list->isList()) throw RuntimeError("append: expected list");
        std::vector<ValuePtr> result = list->asList();
        result.push_back(elem);
        return makeList(result);
    }));

    // concat(list1, list2) - Concatenate lists
    env->define("concat", makeBuiltin("concat", 2, [](const std::vector<ValuePtr>& args) {
        auto list1 = force(args[0]);
        auto list2 = force(args[1]);
        if (!list1->isList() || !list2->isList()) {
            throw RuntimeError("concat: expected lists");
        }
        std::vector<ValuePtr> result = list1->asList();
        const auto& lst2 = list2->asList();
        result.insert(result.end(), lst2.begin(), lst2.end());
        return makeList(result);
    }));

    // reverse(list) - Reverse a list
    env->define("reverse", makeBuiltin("reverse", 1, [](const std::vector<ValuePtr>& args) {
        auto list = force(args[0]);
        if (!list->isList()) throw RuntimeError("reverse: expected list");
        std::vector<ValuePtr> result = list->asList();
        std::reverse(result.begin(), result.end());
        return makeList(result);
    }));

    // nth(list, index) - Get element at index
    env->define("nth", makeBuiltin("nth", 2, [](const std::vector<ValuePtr>& args) {
        auto list = force(args[0]);
        auto idx = force(args[1]);
        if (!list->isList()) throw RuntimeError("nth: expected list");
        if (!idx->isInt()) throw RuntimeError("nth: expected int index");
        const auto& lst = list->asList();
        size_t i = static_cast<size_t>(idx->asInt());
        if (i >= lst.size()) throw RuntimeError("nth: index out of bounds");
        return lst[i];
    }));

    // ============ Math operations ============

    // abs(x)
    env->define("abs", makeBuiltin("abs", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        if (val->isInt()) return makeInt(std::abs(val->asInt()));
        if (val->isFloat()) return makeFloat(std::abs(val->asFloat()));
        throw RuntimeError("abs: expected number");
    }));

    // floor(x)
    env->define("floor", makeBuiltin("floor", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        return makeInt(static_cast<int64_t>(std::floor(val->toNumber())));
    }));

    // ceil(x)
    env->define("ceil", makeBuiltin("ceil", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        return makeInt(static_cast<int64_t>(std::ceil(val->toNumber())));
    }));

    // round(x)
    env->define("round", makeBuiltin("round", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        return makeInt(static_cast<int64_t>(std::round(val->toNumber())));
    }));

    // sqrt(x)
    env->define("sqrt", makeBuiltin("sqrt", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        return makeFloat(std::sqrt(val->toNumber()));
    }));

    // pow(base, exp)
    env->define("pow", makeBuiltin("pow", 2, [](const std::vector<ValuePtr>& args) {
        auto base = force(args[0]);
        auto exp = force(args[1]);
        return makeFloat(std::pow(base->toNumber(), exp->toNumber()));
    }));

    // min(a, b)
    env->define("min", makeBuiltin("min", 2, [](const std::vector<ValuePtr>& args) {
        auto a = force(args[0]);
        auto b = force(args[1]);
        double va = a->toNumber();
        double vb = b->toNumber();
        if (a->isInt() && b->isInt()) {
            return makeInt(std::min(a->asInt(), b->asInt()));
        }
        return makeFloat(std::min(va, vb));
    }));

    // max(a, b)
    env->define("max", makeBuiltin("max", 2, [](const std::vector<ValuePtr>& args) {
        auto a = force(args[0]);
        auto b = force(args[1]);
        double va = a->toNumber();
        double vb = b->toNumber();
        if (a->isInt() && b->isInt()) {
            return makeInt(std::max(a->asInt(), b->asInt()));
        }
        return makeFloat(std::max(va, vb));
    }));

    // ============ Extended Math operations ============

    // sin(x)
    env->define("sin", makeBuiltin("sin", 1, [](const std::vector<ValuePtr>& args) {
        return makeFloat(std::sin(force(args[0])->toNumber()));
    }));

    // cos(x)
    env->define("cos", makeBuiltin("cos", 1, [](const std::vector<ValuePtr>& args) {
        return makeFloat(std::cos(force(args[0])->toNumber()));
    }));

    // tan(x)
    env->define("tan", makeBuiltin("tan", 1, [](const std::vector<ValuePtr>& args) {
        return makeFloat(std::tan(force(args[0])->toNumber()));
    }));

    // asin(x)
    env->define("asin", makeBuiltin("asin", 1, [](const std::vector<ValuePtr>& args) {
        return makeFloat(std::asin(force(args[0])->toNumber()));
    }));

    // acos(x)
    env->define("acos", makeBuiltin("acos", 1, [](const std::vector<ValuePtr>& args) {
        return makeFloat(std::acos(force(args[0])->toNumber()));
    }));

    // atan(x)
    env->define("atan", makeBuiltin("atan", 1, [](const std::vector<ValuePtr>& args) {
        return makeFloat(std::atan(force(args[0])->toNumber()));
    }));

    // atan2(y, x)
    env->define("atan2", makeBuiltin("atan2", 2, [](const std::vector<ValuePtr>& args) {
        double y = force(args[0])->toNumber();
        double x = force(args[1])->toNumber();
        return makeFloat(std::atan2(y, x));
    }));

    // log(x) - Natural logarithm
    env->define("log", makeBuiltin("log", 1, [](const std::vector<ValuePtr>& args) {
        return makeFloat(std::log(force(args[0])->toNumber()));
    }));

    // log10(x) - Base-10 logarithm
    env->define("log10", makeBuiltin("log10", 1, [](const std::vector<ValuePtr>& args) {
        return makeFloat(std::log10(force(args[0])->toNumber()));
    }));

    // exp(x) - e^x
    env->define("exp", makeBuiltin("exp", 1, [](const std::vector<ValuePtr>& args) {
        return makeFloat(std::exp(force(args[0])->toNumber()));
    }));

    // random() - Random float between 0 and 1
    env->define("random", makeBuiltin("random", 0, [](const std::vector<ValuePtr>&) {
        std::uniform_real_distribution<double> dist(0.0, 1.0);
        return makeFloat(dist(gen));
    }));

    // random_int(min, max) - Random integer in range [min, max]
    env->define("random_int", makeBuiltin("random_int", 2, [](const std::vector<ValuePtr>& args) {
        int64_t minVal = force(args[0])->asInt();
        int64_t maxVal = force(args[1])->asInt();
        std::uniform_int_distribution<int64_t> dist(minVal, maxVal);
        return makeInt(dist(gen));
    }));

    // Mathematical constants
    env->define("pi", makeFloat(3.14159265358979323846));
    env->define("e", makeFloat(2.71828182845904523536));

    // ============ String operations ============

    // substr(str, start, len)
    env->define("substr", makeBuiltin("substr", 3, [](const std::vector<ValuePtr>& args) {
        auto str = force(args[0]);
        auto start = force(args[1]);
        auto len = force(args[2]);
        if (!str->isString()) throw RuntimeError("substr: expected string");
        return makeString(str->asString().substr(
            static_cast<size_t>(start->asInt()),
            static_cast<size_t>(len->asInt())
        ));
    }));

    // split(str, delim)
    env->define("split", makeBuiltin("split", 2, [](const std::vector<ValuePtr>& args) {
        auto str = force(args[0]);
        auto delim = force(args[1]);
        if (!str->isString() || !delim->isString()) {
            throw RuntimeError("split: expected strings");
        }
        std::vector<ValuePtr> result;
        std::string s = str->asString();
        std::string d = delim->asString();
        size_t pos = 0;
        while ((pos = s.find(d)) != std::string::npos) {
            result.push_back(makeString(s.substr(0, pos)));
            s.erase(0, pos + d.length());
        }
        result.push_back(makeString(s));
        return makeList(result);
    }));

    // join(list, delim)
    env->define("join", makeBuiltin("join", 2, [](const std::vector<ValuePtr>& args) {
        auto list = force(args[0]);
        auto delim = force(args[1]);
        if (!list->isList()) throw RuntimeError("join: expected list");
        if (!delim->isString()) throw RuntimeError("join: expected string delimiter");
        std::string result;
        const auto& lst = list->asList();
        for (size_t i = 0; i < lst.size(); i++) {
            if (i > 0) result += delim->asString();
            auto val = force(lst[i]);
            result += val->isString() ? val->asString() : val->toString();
        }
        return makeString(result);
    }));

    // ============ Extended String operations ============

    // uppercase(str)
    env->define("uppercase", makeBuiltin("uppercase", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        if (!val->isString()) throw RuntimeError("uppercase: expected string");
        std::string s = val->asString();
        std::transform(s.begin(), s.end(), s.begin(), ::toupper);
        return makeString(s);
    }));

    // lowercase(str)
    env->define("lowercase", makeBuiltin("lowercase", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        if (!val->isString()) throw RuntimeError("lowercase: expected string");
        std::string s = val->asString();
        std::transform(s.begin(), s.end(), s.begin(), ::tolower);
        return makeString(s);
    }));

    // trim(str)
    env->define("trim", makeBuiltin("trim", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        if (!val->isString()) throw RuntimeError("trim: expected string");
        std::string s = val->asString();
        size_t start = s.find_first_not_of(" \t\n\r\f\v");
        if (start == std::string::npos) return makeString("");
        size_t end = s.find_last_not_of(" \t\n\r\f\v");
        return makeString(s.substr(start, end - start + 1));
    }));

    // trim_start(str)
    env->define("trim_start", makeBuiltin("trim_start", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        if (!val->isString()) throw RuntimeError("trim_start: expected string");
        std::string s = val->asString();
        size_t start = s.find_first_not_of(" \t\n\r\f\v");
        if (start == std::string::npos) return makeString("");
        return makeString(s.substr(start));
    }));

    // trim_end(str)
    env->define("trim_end", makeBuiltin("trim_end", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        if (!val->isString()) throw RuntimeError("trim_end: expected string");
        std::string s = val->asString();
        size_t end = s.find_last_not_of(" \t\n\r\f\v");
        if (end == std::string::npos) return makeString("");
        return makeString(s.substr(0, end + 1));
    }));

    // contains(str, substr)
    env->define("contains", makeBuiltin("contains", 2, [](const std::vector<ValuePtr>& args) {
        auto str = force(args[0]);
        auto substr = force(args[1]);
        if (!str->isString() || !substr->isString()) {
            throw RuntimeError("contains: expected strings");
        }
        return makeBool(str->asString().find(substr->asString()) != std::string::npos);
    }));

    // starts_with(str, prefix)
    env->define("starts_with", makeBuiltin("starts_with", 2, [](const std::vector<ValuePtr>& args) {
        auto str = force(args[0]);
        auto prefix = force(args[1]);
        if (!str->isString() || !prefix->isString()) {
            throw RuntimeError("starts_with: expected strings");
        }
        const std::string& s = str->asString();
        const std::string& p = prefix->asString();
        if (p.size() > s.size()) return makeBool(false);
        return makeBool(s.compare(0, p.size(), p) == 0);
    }));

    // ends_with(str, suffix)
    env->define("ends_with", makeBuiltin("ends_with", 2, [](const std::vector<ValuePtr>& args) {
        auto str = force(args[0]);
        auto suffix = force(args[1]);
        if (!str->isString() || !suffix->isString()) {
            throw RuntimeError("ends_with: expected strings");
        }
        const std::string& s = str->asString();
        const std::string& suf = suffix->asString();
        if (suf.size() > s.size()) return makeBool(false);
        return makeBool(s.compare(s.size() - suf.size(), suf.size(), suf) == 0);
    }));

    // replace(str, old, new) - Replace first occurrence
    env->define("replace", makeBuiltin("replace", 3, [](const std::vector<ValuePtr>& args) {
        auto strVal = force(args[0]);
        auto oldVal = force(args[1]);
        auto newVal = force(args[2]);
        if (!strVal->isString() || !oldVal->isString() || !newVal->isString()) {
            throw RuntimeError("replace: expected strings");
        }
        std::string s = strVal->asString();
        const std::string& oldStr = oldVal->asString();
        const std::string& newStr = newVal->asString();
        size_t pos = s.find(oldStr);
        if (pos != std::string::npos) {
            s.replace(pos, oldStr.size(), newStr);
        }
        return makeString(s);
    }));

    // replace_all(str, old, new) - Replace all occurrences
    env->define("replace_all", makeBuiltin("replace_all", 3, [](const std::vector<ValuePtr>& args) {
        auto strVal = force(args[0]);
        auto oldVal = force(args[1]);
        auto newVal = force(args[2]);
        if (!strVal->isString() || !oldVal->isString() || !newVal->isString()) {
            throw RuntimeError("replace_all: expected strings");
        }
        std::string s = strVal->asString();
        const std::string& oldStr = oldVal->asString();
        const std::string& newStr = newVal->asString();
        if (oldStr.empty()) return makeString(s);
        size_t pos = 0;
        while ((pos = s.find(oldStr, pos)) != std::string::npos) {
            s.replace(pos, oldStr.size(), newStr);
            pos += newStr.size();
        }
        return makeString(s);
    }));

    // char_at(str, index)
    env->define("char_at", makeBuiltin("char_at", 2, [](const std::vector<ValuePtr>& args) {
        auto str = force(args[0]);
        auto idx = force(args[1]);
        if (!str->isString()) throw RuntimeError("char_at: expected string");
        if (!idx->isInt()) throw RuntimeError("char_at: expected int index");
        const std::string& s = str->asString();
        size_t i = static_cast<size_t>(idx->asInt());
        if (i >= s.size()) throw RuntimeError("char_at: index out of bounds");
        return makeString(std::string(1, s[i]));
    }));

    // chars(str) - String to list of single-character strings
    env->define("chars", makeBuiltin("chars", 1, [](const std::vector<ValuePtr>& args) {
        auto str = force(args[0]);
        if (!str->isString()) throw RuntimeError("chars: expected string");
        const std::string& s = str->asString();
        std::vector<ValuePtr> result;
        for (char c : s) {
            result.push_back(makeString(std::string(1, c)));
        }
        return makeList(result);
    }));

    // index_of(str, substr) - Returns index or -1 if not found
    env->define("index_of", makeBuiltin("index_of", 2, [](const std::vector<ValuePtr>& args) {
        auto str = force(args[0]);
        auto substr = force(args[1]);
        if (!str->isString() || !substr->isString()) {
            throw RuntimeError("index_of: expected strings");
        }
        size_t pos = str->asString().find(substr->asString());
        if (pos == std::string::npos) {
            return makeInt(-1);
        }
        return makeInt(static_cast<int64_t>(pos));
    }));

    // ============ Type checking ============

    env->define("is_int", makeBuiltin("is_int", 1, [](const std::vector<ValuePtr>& args) {
        return makeBool(force(args[0])->isInt());
    }));

    env->define("is_float", makeBuiltin("is_float", 1, [](const std::vector<ValuePtr>& args) {
        return makeBool(force(args[0])->isFloat());
    }));

    env->define("is_string", makeBuiltin("is_string", 1, [](const std::vector<ValuePtr>& args) {
        return makeBool(force(args[0])->isString());
    }));

    env->define("is_bool", makeBuiltin("is_bool", 1, [](const std::vector<ValuePtr>& args) {
        return makeBool(force(args[0])->isBool());
    }));

    env->define("is_list", makeBuiltin("is_list", 1, [](const std::vector<ValuePtr>& args) {
        return makeBool(force(args[0])->isList());
    }));

    env->define("is_tuple", makeBuiltin("is_tuple", 1, [](const std::vector<ValuePtr>& args) {
        return makeBool(force(args[0])->isTuple());
    }));

    env->define("is_record", makeBuiltin("is_record", 1, [](const std::vector<ValuePtr>& args) {
        return makeBool(force(args[0])->isRecord());
    }));

    env->define("is_fn", makeBuiltin("is_fn", 1, [](const std::vector<ValuePtr>& args) {
        return makeBool(force(args[0])->isCallable());
    }));

    // ============ Functional operations ============

    // range(start, end) - Generate a list from start to end-1
    env->define("range", makeBuiltin("range", 2, [](const std::vector<ValuePtr>& args) {
        auto start = force(args[0]);
        auto end = force(args[1]);
        if (!start->isInt() || !end->isInt()) {
            throw RuntimeError("range: expected int arguments");
        }
        std::vector<ValuePtr> result;
        for (int64_t i = start->asInt(); i < end->asInt(); i++) {
            result.push_back(makeInt(i));
        }
        return makeList(result);
    }));

    // ============ I/O ============

    // input([prompt]) - Read a line from stdin with optional prompt
    env->define("input", makeBuiltin("input", -1, [](const std::vector<ValuePtr>& args) {
        if (!args.empty()) {
            auto prompt = force(args[0]);
            std::cout << (prompt->isString() ? prompt->asString() : prompt->toString());
            std::cout.flush();
        }
        std::string line;
        std::getline(std::cin, line);
        return makeString(line);
    }));

    // input_prompt(prompt) - Print prompt and read a line
    env->define("input_prompt", makeBuiltin("input_prompt", 1, [](const std::vector<ValuePtr>& args) {
        auto prompt = force(args[0]);
        std::cout << (prompt->isString() ? prompt->asString() : prompt->toString());
        std::cout.flush();
        std::string line;
        std::getline(std::cin, line);
        return makeString(line);
    }));

    // ============ Error handling ============

    // error(msg) - Throw a runtime error
    env->define("error", makeBuiltin("error", 1, [](const std::vector<ValuePtr>& args) -> ValuePtr {
        auto msg = force(args[0]);
        throw RuntimeError(msg->isString() ? msg->asString() : msg->toString());
    }));

    // assert(cond, msg) - Assert condition
    env->define("assert", makeBuiltin("assert", 2, [](const std::vector<ValuePtr>& args) {
        auto cond = force(args[0]);
        auto msg = force(args[1]);
        if (!cond->asBool()) {
            throw RuntimeError("Assertion failed: " +
                (msg->isString() ? msg->asString() : msg->toString()));
        }
        return makeUnit();
    }));

    // ============ File I/O ============

    // file_read(path) - Read entire file as string
    env->define("file_read", makeBuiltin("file_read", 1, [](const std::vector<ValuePtr>& args) {
        auto pathVal = force(args[0]);
        if (!pathVal->isString()) throw RuntimeError("file_read: expected string path");

        std::ifstream file(pathVal->asString());
        if (!file) {
            throw RuntimeError("file_read: could not open file: " + pathVal->asString());
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        return makeString(buffer.str());
    }));

    // file_write(path, content) - Write string to file (overwrites)
    env->define("file_write", makeBuiltin("file_write", 2, [](const std::vector<ValuePtr>& args) {
        auto pathVal = force(args[0]);
        auto contentVal = force(args[1]);
        if (!pathVal->isString()) throw RuntimeError("file_write: expected string path");
        if (!contentVal->isString()) throw RuntimeError("file_write: expected string content");

        std::ofstream file(pathVal->asString());
        if (!file) {
            throw RuntimeError("file_write: could not open file for writing: " + pathVal->asString());
        }

        file << contentVal->asString();
        return makeUnit();
    }));

    // file_append(path, content) - Append to file
    env->define("file_append", makeBuiltin("file_append", 2, [](const std::vector<ValuePtr>& args) {
        auto pathVal = force(args[0]);
        auto contentVal = force(args[1]);
        if (!pathVal->isString()) throw RuntimeError("file_append: expected string path");
        if (!contentVal->isString()) throw RuntimeError("file_append: expected string content");

        std::ofstream file(pathVal->asString(), std::ios::app);
        if (!file) {
            throw RuntimeError("file_append: could not open file for appending: " + pathVal->asString());
        }

        file << contentVal->asString();
        return makeUnit();
    }));

    // file_exists(path) - Check if file exists
    env->define("file_exists", makeBuiltin("file_exists", 1, [](const std::vector<ValuePtr>& args) {
        auto pathVal = force(args[0]);
        if (!pathVal->isString()) throw RuntimeError("file_exists: expected string path");

        return makeBool(fs::exists(pathVal->asString()));
    }));

    // file_delete(path) - Delete a file
    env->define("file_delete", makeBuiltin("file_delete", 1, [](const std::vector<ValuePtr>& args) {
        auto pathVal = force(args[0]);
        if (!pathVal->isString()) throw RuntimeError("file_delete: expected string path");

        std::error_code ec;
        bool removed = fs::remove(pathVal->asString(), ec);
        if (ec) {
            throw RuntimeError("file_delete: " + ec.message());
        }
        return makeBool(removed);
    }));

    // file_lines(path) - Read file as list of lines
    env->define("file_lines", makeBuiltin("file_lines", 1, [](const std::vector<ValuePtr>& args) {
        auto pathVal = force(args[0]);
        if (!pathVal->isString()) throw RuntimeError("file_lines: expected string path");

        std::ifstream file(pathVal->asString());
        if (!file) {
            throw RuntimeError("file_lines: could not open file: " + pathVal->asString());
        }

        std::vector<ValuePtr> lines;
        std::string line;
        while (std::getline(file, line)) {
            lines.push_back(makeString(line));
        }
        return makeList(lines);
    }));

    // dir_list(path) - List directory contents
    env->define("dir_list", makeBuiltin("dir_list", 1, [](const std::vector<ValuePtr>& args) {
        auto pathVal = force(args[0]);
        if (!pathVal->isString()) throw RuntimeError("dir_list: expected string path");

        std::vector<ValuePtr> entries;
        try {
            for (const auto& entry : fs::directory_iterator(pathVal->asString())) {
                entries.push_back(makeString(entry.path().filename().string()));
            }
        } catch (const fs::filesystem_error& e) {
            throw RuntimeError("dir_list: " + std::string(e.what()));
        }
        return makeList(entries);
    }));

    // dir_exists(path) - Check if directory exists
    env->define("dir_exists", makeBuiltin("dir_exists", 1, [](const std::vector<ValuePtr>& args) {
        auto pathVal = force(args[0]);
        if (!pathVal->isString()) throw RuntimeError("dir_exists: expected string path");

        return makeBool(fs::is_directory(pathVal->asString()));
    }));

    // ============ Sort and Compare ============

    // sort(list) - Sort a list (numbers or strings)
    env->define("sort", makeBuiltin("sort", 1, [](const std::vector<ValuePtr>& args) {
        auto list = force(args[0]);
        if (!list->isList()) throw RuntimeError("sort: expected list");

        std::vector<ValuePtr> result = list->asList();

        if (result.empty()) return makeList(result);

        // Determine type from first element
        auto first = force(result[0]);

        if (first->isInt() || first->isFloat()) {
            std::sort(result.begin(), result.end(), [](const ValuePtr& a, const ValuePtr& b) {
                return force(a)->toNumber() < force(b)->toNumber();
            });
        } else if (first->isString()) {
            std::sort(result.begin(), result.end(), [](const ValuePtr& a, const ValuePtr& b) {
                return force(a)->asString() < force(b)->asString();
            });
        } else {
            throw RuntimeError("sort: can only sort lists of numbers or strings");
        }

        return makeList(result);
    }));

    // compare(a, b) - Returns -1, 0, or 1 for comparison
    env->define("compare", makeBuiltin("compare", 2, [](const std::vector<ValuePtr>& args) {
        auto a = force(args[0]);
        auto b = force(args[1]);

        if ((a->isInt() || a->isFloat()) && (b->isInt() || b->isFloat())) {
            double va = a->toNumber();
            double vb = b->toNumber();
            if (va < vb) return makeInt(-1);
            if (va > vb) return makeInt(1);
            return makeInt(0);
        }

        if (a->isString() && b->isString()) {
            int cmp = a->asString().compare(b->asString());
            if (cmp < 0) return makeInt(-1);
            if (cmp > 0) return makeInt(1);
            return makeInt(0);
        }

        throw RuntimeError("compare: can only compare numbers or strings");
    }));

    // ============ HTTP/S Module ============

    // http_get(url) - Simple HTTP GET request, returns response body as string
    env->define("http_get", makeBuiltin("http_get", 1, [](const std::vector<ValuePtr>& args) {
        auto urlVal = force(args[0]);
        if (!urlVal->isString()) throw RuntimeError("http_get: expected string URL");

        std::string url = urlVal->asString();
        std::string responseBody;

        CURL* curl = curl_easy_init();
        if (!curl) {
            throw RuntimeError("http_get: failed to initialize CURL");
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Setsuna/1.0");

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::string error = curl_easy_strerror(res);
            curl_easy_cleanup(curl);
            throw RuntimeError("http_get: " + error);
        }

        curl_easy_cleanup(curl);
        return makeString(responseBody);
    }));

    // http_post(url, body) - Simple HTTP POST request, returns response body
    env->define("http_post", makeBuiltin("http_post", 2, [](const std::vector<ValuePtr>& args) {
        auto urlVal = force(args[0]);
        auto bodyVal = force(args[1]);
        if (!urlVal->isString()) throw RuntimeError("http_post: expected string URL");
        if (!bodyVal->isString()) throw RuntimeError("http_post: expected string body");

        std::string url = urlVal->asString();
        std::string requestBody = bodyVal->asString();
        std::string responseBody;

        CURL* curl = curl_easy_init();
        if (!curl) {
            throw RuntimeError("http_post: failed to initialize CURL");
        }

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
        curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(requestBody.size()));
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 30L);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Setsuna/1.0");

        CURLcode res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            std::string error = curl_easy_strerror(res);
            curl_easy_cleanup(curl);
            throw RuntimeError("http_post: " + error);
        }

        curl_easy_cleanup(curl);
        return makeString(responseBody);
    }));

    // http_request(options) - Advanced HTTP request with full control
    // Options record: { url: string, method?: string, headers?: record, body?: string, timeout?: int }
    // Returns: { status: int, body: string, headers: record }
    env->define("http_request", makeBuiltin("http_request", 1, [](const std::vector<ValuePtr>& args) {
        auto optionsVal = force(args[0]);
        if (!optionsVal->isRecord()) throw RuntimeError("http_request: expected record options");

        const auto& options = optionsVal->asRecord();

        // Get URL (required)
        auto urlIt = options.fields.find("url");
        if (urlIt == options.fields.end()) {
            throw RuntimeError("http_request: missing required 'url' field");
        }
        auto urlVal = force(urlIt->second);
        if (!urlVal->isString()) {
            throw RuntimeError("http_request: 'url' must be a string");
        }
        std::string url = urlVal->asString();

        // Get method (default: GET)
        std::string method = "GET";
        auto methodIt = options.fields.find("method");
        if (methodIt != options.fields.end()) {
            auto methodVal = force(methodIt->second);
            if (!methodVal->isString()) {
                throw RuntimeError("http_request: 'method' must be a string");
            }
            method = methodVal->asString();
            // Convert to uppercase
            std::transform(method.begin(), method.end(), method.begin(), ::toupper);
        }

        // Get body (optional)
        std::string requestBody;
        auto bodyIt = options.fields.find("body");
        if (bodyIt != options.fields.end()) {
            auto bodyVal = force(bodyIt->second);
            if (!bodyVal->isString()) {
                throw RuntimeError("http_request: 'body' must be a string");
            }
            requestBody = bodyVal->asString();
        }

        // Get timeout (default: 30)
        long timeout = 30;
        auto timeoutIt = options.fields.find("timeout");
        if (timeoutIt != options.fields.end()) {
            auto timeoutVal = force(timeoutIt->second);
            if (!timeoutVal->isInt()) {
                throw RuntimeError("http_request: 'timeout' must be an integer");
            }
            timeout = static_cast<long>(timeoutVal->asInt());
        }

        // Initialize CURL
        CURL* curl = curl_easy_init();
        if (!curl) {
            throw RuntimeError("http_request: failed to initialize CURL");
        }

        std::string responseBody;
        std::vector<std::pair<std::string, std::string>> responseHeaders;

        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlWriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBody);
        curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curlHeaderCallback);
        curl_easy_setopt(curl, CURLOPT_HEADERDATA, &responseHeaders);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Setsuna/1.0");

        // Set custom request method
        curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method.c_str());

        // Set request body for methods that support it
        if (!requestBody.empty()) {
            curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
            curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, static_cast<long>(requestBody.size()));
        }

        // Set custom headers
        struct curl_slist* headerList = nullptr;
        auto headersIt = options.fields.find("headers");
        if (headersIt != options.fields.end()) {
            auto headersVal = force(headersIt->second);
            if (!headersVal->isRecord()) {
                throw RuntimeError("http_request: 'headers' must be a record");
            }
            for (const auto& [key, value] : headersVal->asRecord().fields) {
                auto valForced = force(value);
                if (!valForced->isString()) {
                    throw RuntimeError("http_request: header values must be strings");
                }
                std::string headerLine = key + ": " + valForced->asString();
                headerList = curl_slist_append(headerList, headerLine.c_str());
            }
            curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headerList);
        }

        // Perform request
        CURLcode res = curl_easy_perform(curl);

        // Get status code
        long statusCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &statusCode);

        // Clean up header list
        if (headerList) {
            curl_slist_free_all(headerList);
        }

        if (res != CURLE_OK) {
            std::string error = curl_easy_strerror(res);
            curl_easy_cleanup(curl);
            throw RuntimeError("http_request: " + error);
        }

        curl_easy_cleanup(curl);

        // Build response headers record
        RecordValue headersRecord;
        for (const auto& [key, value] : responseHeaders) {
            headersRecord.fields[key] = makeString(value);
        }

        // Build response record
        RecordValue response;
        response.fields["status"] = makeInt(statusCode);
        response.fields["body"] = makeString(responseBody);
        response.fields["headers"] = makeRecord(headersRecord);

        return makeRecord(response);
    }));

    // url_encode(str) - URL encode a string
    env->define("url_encode", makeBuiltin("url_encode", 1, [](const std::vector<ValuePtr>& args) {
        auto strVal = force(args[0]);
        if (!strVal->isString()) throw RuntimeError("url_encode: expected string");

        CURL* curl = curl_easy_init();
        if (!curl) {
            throw RuntimeError("url_encode: failed to initialize CURL");
        }

        char* encoded = curl_easy_escape(curl, strVal->asString().c_str(),
                                          static_cast<int>(strVal->asString().size()));
        if (!encoded) {
            curl_easy_cleanup(curl);
            throw RuntimeError("url_encode: encoding failed");
        }

        std::string result(encoded);
        curl_free(encoded);
        curl_easy_cleanup(curl);

        return makeString(result);
    }));

    // url_decode(str) - URL decode a string
    env->define("url_decode", makeBuiltin("url_decode", 1, [](const std::vector<ValuePtr>& args) {
        auto strVal = force(args[0]);
        if (!strVal->isString()) throw RuntimeError("url_decode: expected string");

        CURL* curl = curl_easy_init();
        if (!curl) {
            throw RuntimeError("url_decode: failed to initialize CURL");
        }

        int decodedLen = 0;
        char* decoded = curl_easy_unescape(curl, strVal->asString().c_str(),
                                            static_cast<int>(strVal->asString().size()), &decodedLen);
        if (!decoded) {
            curl_easy_cleanup(curl);
            throw RuntimeError("url_decode: decoding failed");
        }

        std::string result(decoded, decodedLen);
        curl_free(decoded);
        curl_easy_cleanup(curl);

        return makeString(result);
    }));

    // ============ JSON Operations ============

    // json_parse(str) - Parse JSON string to Setsuna values
    env->define("json_parse", makeBuiltin("json_parse", 1, [](const std::vector<ValuePtr>& args) {
        auto strVal = force(args[0]);
        if (!strVal->isString()) throw RuntimeError("json_parse: expected string");

        return JsonParser::parse(strVal->asString());
    }));

    // json_stringify(value) - Convert Setsuna value to JSON string
    env->define("json_stringify", makeBuiltin("json_stringify", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        return makeString(jsonStringify(val));
    }));

    // json_pretty(value) - Convert Setsuna value to formatted JSON string
    env->define("json_pretty", makeBuiltin("json_pretty", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        return makeString(jsonStringify(val, 0, true));
    }));

    // ============ Map Operations ============

    // map_new() - Create an empty map
    env->define("map_new", makeBuiltin("map_new", 0, [](const std::vector<ValuePtr>&) {
        return makeMap(std::vector<std::pair<ValuePtr, ValuePtr>>{});
    }));

    // map_get(map, key) - Get value by key, returns the value or throws error if not found
    env->define("map_get", makeBuiltin("map_get", 2, [](const std::vector<ValuePtr>& args) {
        auto mapVal = force(args[0]);
        auto key = force(args[1]);

        if (!mapVal->isMap()) throw RuntimeError("map_get: expected map as first argument");

        const MapValue& m = mapVal->asMap();
        const ValuePtr* val = m.find(key);
        if (!val) {
            throw RuntimeError("map_get: key not found");
        }
        return *val;
    }));

    // map_get_or(map, key, default) - Get value by key, returns default if not found
    env->define("map_get_or", makeBuiltin("map_get_or", 3, [](const std::vector<ValuePtr>& args) {
        auto mapVal = force(args[0]);
        auto key = force(args[1]);
        auto defaultVal = force(args[2]);

        if (!mapVal->isMap()) throw RuntimeError("map_get_or: expected map as first argument");

        const MapValue& m = mapVal->asMap();
        const ValuePtr* val = m.find(key);
        return val ? *val : defaultVal;
    }));

    // map_set(map, key, value) - Returns a new map with the key-value pair added/updated
    env->define("map_set", makeBuiltin("map_set", 3, [](const std::vector<ValuePtr>& args) {
        auto mapVal = force(args[0]);
        auto key = force(args[1]);
        auto value = force(args[2]);

        if (!mapVal->isMap()) throw RuntimeError("map_set: expected map as first argument");

        // Create a copy
        MapValue newMap = mapVal->asMap();
        newMap.set(key, value);
        return makeMap(std::move(newMap));
    }));

    // map_has(map, key) - Check if key exists
    env->define("map_has", makeBuiltin("map_has", 2, [](const std::vector<ValuePtr>& args) {
        auto mapVal = force(args[0]);
        auto key = force(args[1]);

        if (!mapVal->isMap()) throw RuntimeError("map_has: expected map as first argument");

        const MapValue& m = mapVal->asMap();
        return makeBool(m.find(key) != nullptr);
    }));

    // map_remove(map, key) - Returns a new map with the key removed
    env->define("map_remove", makeBuiltin("map_remove", 2, [](const std::vector<ValuePtr>& args) {
        auto mapVal = force(args[0]);
        auto key = force(args[1]);

        if (!mapVal->isMap()) throw RuntimeError("map_remove: expected map as first argument");

        // Create a copy
        MapValue newMap = mapVal->asMap();
        newMap.remove(key);
        return makeMap(std::move(newMap));
    }));

    // map_keys(map) - Get all keys as a list
    env->define("map_keys", makeBuiltin("map_keys", 1, [](const std::vector<ValuePtr>& args) {
        auto mapVal = force(args[0]);

        if (!mapVal->isMap()) throw RuntimeError("map_keys: expected map as argument");

        const MapValue& m = mapVal->asMap();
        std::vector<ValuePtr> keys;
        for (const auto& [k, v] : m.entries) {
            keys.push_back(k);
        }
        return makeList(keys);
    }));

    // map_values(map) - Get all values as a list
    env->define("map_values", makeBuiltin("map_values", 1, [](const std::vector<ValuePtr>& args) {
        auto mapVal = force(args[0]);

        if (!mapVal->isMap()) throw RuntimeError("map_values: expected map as argument");

        const MapValue& m = mapVal->asMap();
        std::vector<ValuePtr> values;
        for (const auto& [k, v] : m.entries) {
            values.push_back(v);
        }
        return makeList(values);
    }));

    // map_entries(map) - Get all entries as a list of (key, value) tuples
    env->define("map_entries", makeBuiltin("map_entries", 1, [](const std::vector<ValuePtr>& args) {
        auto mapVal = force(args[0]);

        if (!mapVal->isMap()) throw RuntimeError("map_entries: expected map as argument");

        const MapValue& m = mapVal->asMap();
        std::vector<ValuePtr> entries;
        for (const auto& [k, v] : m.entries) {
            entries.push_back(makeTuple({k, v}));
        }
        return makeList(entries);
    }));

    // map_size(map) - Get the number of entries
    env->define("map_size", makeBuiltin("map_size", 1, [](const std::vector<ValuePtr>& args) {
        auto mapVal = force(args[0]);

        if (!mapVal->isMap()) throw RuntimeError("map_size: expected map as argument");

        return makeInt(static_cast<int64_t>(mapVal->asMap().entries.size()));
    }));

    // map_empty(map) - Check if map is empty
    env->define("map_empty", makeBuiltin("map_empty", 1, [](const std::vector<ValuePtr>& args) {
        auto mapVal = force(args[0]);

        if (!mapVal->isMap()) throw RuntimeError("map_empty: expected map as argument");

        return makeBool(mapVal->asMap().entries.empty());
    }));

    // is_map(value) - Type check for map
    env->define("is_map", makeBuiltin("is_map", 1, [](const std::vector<ValuePtr>& args) {
        auto val = force(args[0]);
        return makeBool(val->isMap());
    }));

    // map_from_list(list) - Create map from list of (key, value) tuples
    env->define("map_from_list", makeBuiltin("map_from_list", 1, [](const std::vector<ValuePtr>& args) {
        auto listVal = force(args[0]);

        if (!listVal->isList()) throw RuntimeError("map_from_list: expected list of tuples");

        MapValue m;
        for (const auto& entry : listVal->asList()) {
            auto tuple = force(entry);
            if (!tuple->isTuple() || tuple->asTuple().size() != 2) {
                throw RuntimeError("map_from_list: expected list of (key, value) tuples");
            }
            m.set(tuple->asTuple()[0], tuple->asTuple()[1]);
        }
        return makeMap(std::move(m));
    }));

    // map_merge(map1, map2) - Merge two maps (map2 values override map1)
    env->define("map_merge", makeBuiltin("map_merge", 2, [](const std::vector<ValuePtr>& args) {
        auto map1 = force(args[0]);
        auto map2 = force(args[1]);

        if (!map1->isMap()) throw RuntimeError("map_merge: expected map as first argument");
        if (!map2->isMap()) throw RuntimeError("map_merge: expected map as second argument");

        // Start with a copy of map1
        MapValue result = map1->asMap();

        // Add all entries from map2 (overwriting duplicates)
        for (const auto& [k, v] : map2->asMap().entries) {
            result.set(k, v);
        }

        return makeMap(std::move(result));
    }));
}

} // namespace setsuna
