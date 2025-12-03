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

namespace fs = std::filesystem;

namespace setsuna {

// Random number generator for random() and random_int()
static std::random_device rd;
static std::mt19937 gen(rd());

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
}

} // namespace setsuna
