#include "kite/builtins.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <string>
#include <unordered_map>

namespace kite {

namespace {

bool arity(BuiltinContext& context, const std::vector<Value>& arguments, std::size_t count,
    const char* name) {
    if (arguments.size() == count) return true;
    context.error = std::string("'") + name + "' expects " + std::to_string(count) + " argument(s)";
    return false;
}

bool want_string(BuiltinContext& context, const Value& value, const char* name) {
    if (value.is_string()) return true;
    context.error = std::string("'") + name + "' expects a string";
    return false;
}

double number_or_nan(const Value& value) {
    return value.is_number() ? value.as_number() : std::numeric_limits<double>::quiet_NaN();
}

bool math1(BuiltinContext& context, std::vector<Value>& arguments, Value& result,
    double (*fn)(double), const char* name, bool non_negative) {
    if (!arity(context, arguments, 1, name)) return false;
    const double x = number_or_nan(arguments[0]);
    if (std::isnan(x)) { context.error = std::string("'") + name + "' expects a number"; return false; }
    if (non_negative && x < 0.0) { context.error = std::string("'") + name + "' expects a non-negative number"; return false; }
    result = fn(x);
    return true;
}

bool bi_print(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    for (std::size_t index = 0; index < arguments.size(); ++index) {
        if (index > 0) context.out << ' ';
        context.out << to_string(arguments[index]);
    }
    context.out << '\n';
    result = Value::string("");
    return true;
}

bool bi_len(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 1, "len")) return false;
    const Value& value = arguments[0];
    if (value.is_string()) result = static_cast<std::int64_t>(value.as_string().size());
    else if (value.is_array()) result = static_cast<std::int64_t>(value.as_array().size());
    else if (value.is_map()) result = static_cast<std::int64_t>(value.as_map().size());
    else { context.error = "'len' expects a string, array, or map"; return false; }
    return true;
}

bool bi_case(BuiltinContext& context, std::vector<Value>& arguments, Value& result, bool upper,
    const char* name) {
    if (!arity(context, arguments, 1, name) || !want_string(context, arguments[0], name)) return false;
    std::string text = arguments[0].as_string();
    std::transform(text.begin(), text.end(), text.begin(), [upper](unsigned char character) {
        return static_cast<char>(upper ? std::toupper(character) : std::tolower(character));
    });
    result = Value::string(std::move(text));
    return true;
}

bool bi_type_of(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 1, "type_of")) return false;
    result = Value::string(type_name(arguments[0]));
    return true;
}

bool bi_str(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 1, "str")) return false;
    result = Value::string(to_string(arguments[0]));
    return true;
}

bool bi_int(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 1, "int")) return false;
    const Value& value = arguments[0];
    if (value.is_int()) { result = value; return true; }
    if (value.is_float()) { result = static_cast<std::int64_t>(value.as_float()); return true; }
    if (value.is_bool()) { result = static_cast<std::int64_t>(value.as_bool() ? 1 : 0); return true; }
    if (value.is_string()) {
        try {
            std::size_t consumed = 0;
            const long long parsed = std::stoll(value.as_string(), &consumed);
            if (consumed == value.as_string().size()) { result = static_cast<std::int64_t>(parsed); return true; }
        } catch (...) {
        }
        context.error = "'int' could not parse '" + value.as_string() + "'";
        return false;
    }
    context.error = "'int' cannot convert this value";
    return false;
}

bool bi_float(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 1, "float")) return false;
    const Value& value = arguments[0];
    if (value.is_number()) { result = value.as_number(); return true; }
    if (value.is_string()) {
        try {
            std::size_t consumed = 0;
            const double parsed = std::stod(value.as_string(), &consumed);
            if (consumed == value.as_string().size()) { result = parsed; return true; }
        } catch (...) {
        }
        context.error = "'float' could not parse '" + value.as_string() + "'";
        return false;
    }
    context.error = "'float' cannot convert this value";
    return false;
}

bool bi_append(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 2, "append")) return false;
    if (!arguments[0].is_array()) { context.error = "'append' expects an array"; return false; }
    arguments[0].as_array().push_back(std::move(arguments[1]));
    result = arguments[0];
    return true;
}

bool bi_pop(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 1, "pop")) return false;
    if (!arguments[0].is_array() || arguments[0].as_array().empty()) {
        context.error = "'pop' expects a non-empty array";
        return false;
    }
    result = std::move(arguments[0].as_array().back());
    arguments[0].as_array().pop_back();
    return true;
}

bool bi_keys(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 1, "keys")) return false;
    if (!arguments[0].is_map()) { context.error = "'keys' expects a map"; return false; }
    result = Value::array();
    for (const auto& entry : arguments[0].as_map()) result.as_array().push_back(Value::string(entry.first));
    std::sort(result.as_array().begin(), result.as_array().end(),
        [](const Value& a, const Value& b) { return a.as_string() < b.as_string(); });
    return true;
}

bool bi_has(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 2, "has")) return false;
    if (!arguments[0].is_map()) { context.error = "'has' expects a map"; return false; }
    if (!want_string(context, arguments[1], "has")) return false;
    result = arguments[0].as_map().count(arguments[1].as_string()) != 0;
    return true;
}

bool bi_remove(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 2, "remove")) return false;
    if (!arguments[0].is_map()) { context.error = "'remove' expects a map"; return false; }
    if (!want_string(context, arguments[1], "remove")) return false;
    result = arguments[0].as_map().erase(arguments[1].as_string()) != 0;
    return true;
}

bool bi_split(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 2, "split") || !want_string(context, arguments[0], "split") ||
        !want_string(context, arguments[1], "split")) {
        return false;
    }
    const std::string& text = arguments[0].as_string();
    const std::string& separator = arguments[1].as_string();
    result = Value::array();
    if (separator.empty()) {
        for (char character : text) result.as_array().push_back(Value::string(std::string(1, character)));
        return true;
    }
    std::size_t start = 0;
    while (true) {
        const std::size_t found = text.find(separator, start);
        if (found == std::string::npos) {
            result.as_array().push_back(Value::string(text.substr(start)));
            break;
        }
        result.as_array().push_back(Value::string(text.substr(start, found - start)));
        start = found + separator.size();
    }
    return true;
}

bool bi_join(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 2, "join")) return false;
    if (!arguments[0].is_array()) { context.error = "'join' expects an array"; return false; }
    if (!want_string(context, arguments[1], "join")) return false;
    std::string joined;
    const auto& elements = arguments[0].as_array();
    for (std::size_t index = 0; index < elements.size(); ++index) {
        if (index > 0) joined += arguments[1].as_string();
        joined += to_string(elements[index]);
    }
    result = Value::string(std::move(joined));
    return true;
}

bool bi_substring(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 3, "substring") || !want_string(context, arguments[0], "substring")) {
        return false;
    }
    if (!arguments[1].is_int() || !arguments[2].is_int()) {
        context.error = "'substring' expects integer bounds";
        return false;
    }
    const std::string& text = arguments[0].as_string();
    std::int64_t start = arguments[1].as_int();
    std::int64_t end = arguments[2].as_int();
    const std::int64_t size = static_cast<std::int64_t>(text.size());
    if (start < 0) start = 0;
    if (end > size) end = size;
    if (start >= end) { result = Value::string(""); return true; }
    result = Value::string(text.substr(static_cast<std::size_t>(start),
        static_cast<std::size_t>(end - start)));
    return true;
}

bool bi_contains(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 2, "contains")) return false;
    if (arguments[0].is_string()) {
        if (!want_string(context, arguments[1], "contains")) return false;
        result = arguments[0].as_string().find(arguments[1].as_string()) != std::string::npos;
        return true;
    }
    if (arguments[0].is_array()) {
        for (const auto& element : arguments[0].as_array()) {
            if (element.equals(arguments[1])) { result = true; return true; }
        }
        result = false;
        return true;
    }
    context.error = "'contains' expects a string or array";
    return false;
}

bool bi_index_of(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 2, "index_of") || !want_string(context, arguments[0], "index_of") ||
        !want_string(context, arguments[1], "index_of")) {
        return false;
    }
    const std::size_t found = arguments[0].as_string().find(arguments[1].as_string());
    result = found == std::string::npos ? static_cast<std::int64_t>(-1)
                                        : static_cast<std::int64_t>(found);
    return true;
}

bool bi_replace(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 3, "replace") || !want_string(context, arguments[0], "replace") ||
        !want_string(context, arguments[1], "replace") || !want_string(context, arguments[2], "replace")) {
        return false;
    }
    std::string text = arguments[0].as_string();
    const std::string& from = arguments[1].as_string();
    const std::string& to = arguments[2].as_string();
    if (!from.empty()) {
        std::size_t position = 0;
        while ((position = text.find(from, position)) != std::string::npos) {
            text.replace(position, from.size(), to);
            position += to.size();
        }
    }
    result = Value::string(std::move(text));
    return true;
}

bool bi_trim(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 1, "trim") || !want_string(context, arguments[0], "trim")) return false;
    const std::string& text = arguments[0].as_string();
    const auto begin = text.find_first_not_of(" \t\r\n");
    if (begin == std::string::npos) { result = Value::string(""); return true; }
    const auto end = text.find_last_not_of(" \t\r\n");
    result = Value::string(text.substr(begin, end - begin + 1));
    return true;
}

bool bi_ord(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 1, "ord") || !want_string(context, arguments[0], "ord")) return false;
    if (arguments[0].as_string().size() != 1) { context.error = "'ord' expects a one-character string"; return false; }
    result = static_cast<std::int64_t>(static_cast<unsigned char>(arguments[0].as_string()[0]));
    return true;
}

bool bi_chr(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 1, "chr")) return false;
    if (!arguments[0].is_int()) { context.error = "'chr' expects an integer"; return false; }
    result = Value::string(std::string(1, static_cast<char>(arguments[0].as_int() & 0xFF)));
    return true;
}

bool bi_read_file(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 1, "read_file") || !want_string(context, arguments[0], "read_file")) {
        return false;
    }
    std::ifstream input(arguments[0].as_string(), std::ios::binary);
    if (!input) { context.error = "could not open file: " + arguments[0].as_string(); return false; }
    result = Value::string(std::string((std::istreambuf_iterator<char>(input)),
        std::istreambuf_iterator<char>()));
    return true;
}

bool bi_write_file(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 2, "write_file") || !want_string(context, arguments[0], "write_file") ||
        !want_string(context, arguments[1], "write_file")) {
        return false;
    }
    std::ofstream output(arguments[0].as_string(), std::ios::binary);
    if (!output) { context.error = "could not write file: " + arguments[0].as_string(); return false; }
    output << arguments[1].as_string();
    result = true;
    return true;
}

bool bi_input(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arguments.empty()) context.out << to_string(arguments[0]);
    std::string line;
    if (!std::getline(std::cin, line)) { result = Value(); return true; }
    result = Value::string(std::move(line));
    return true;
}

std::vector<std::string> g_program_args;

bool bi_args(BuiltinContext&, std::vector<Value>&, Value& result) {
    result = Value::array();
    for (const auto& argument : g_program_args) result.as_array().push_back(Value::string(argument));
    return true;
}

bool bi_env(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 1, "env") || !want_string(context, arguments[0], "env")) return false;
#if defined(_WIN32)
    char* buffer = nullptr;
    std::size_t size = 0;
    if (_dupenv_s(&buffer, &size, arguments[0].as_string().c_str()) == 0 && buffer != nullptr) {
        result = Value::string(buffer);
        std::free(buffer);
    } else {
        result = Value();
    }
#else
    const char* value = std::getenv(arguments[0].as_string().c_str());
    result = value ? Value::string(value) : Value();
#endif
    return true;
}

bool bi_pow(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 2, "pow")) return false;
    const double base = number_or_nan(arguments[0]);
    const double exponent = number_or_nan(arguments[1]);
    if (std::isnan(base) || std::isnan(exponent)) { context.error = "'pow' expects numbers"; return false; }
    result = std::pow(base, exponent);
    return true;
}

bool bi_min_max(BuiltinContext& context, std::vector<Value>& arguments, Value& result, bool maximum,
    const char* name) {
    if (!arity(context, arguments, 2, name)) return false;
    if (!arguments[0].is_number() || !arguments[1].is_number()) {
        context.error = std::string("'") + name + "' expects numbers";
        return false;
    }
    const double a = arguments[0].as_number();
    const double b = arguments[1].as_number();
    result = maximum ? std::max(a, b) : std::min(a, b);
    return true;
}

bool bi_atan2(BuiltinContext& context, std::vector<Value>& arguments, Value& result) {
    if (!arity(context, arguments, 2, "atan2")) return false;
    if (!arguments[0].is_number() || !arguments[1].is_number()) {
        context.error = "'atan2' expects numbers";
        return false;
    }
    result = std::atan2(arguments[0].as_number(), arguments[1].as_number());
    return true;
}

} // namespace

void set_program_args(std::vector<std::string> args) { g_program_args = std::move(args); }

BuiltinFn find_builtin(const std::string& name) {
    static const std::unordered_map<std::string, BuiltinFn> table = {
        {"print", bi_print}, {"len", bi_len}, {"type_of", bi_type_of},
        {"upper", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return bi_case(c, a, r, true, "upper"); }},
        {"lower", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return bi_case(c, a, r, false, "lower"); }},
        {"str", bi_str}, {"int", bi_int}, {"float", bi_float},
        {"append", bi_append}, {"push", bi_append}, {"pop", bi_pop},
        {"keys", bi_keys}, {"has", bi_has}, {"remove", bi_remove},
        {"split", bi_split}, {"join", bi_join}, {"substring", bi_substring},
        {"contains", bi_contains}, {"index_of", bi_index_of}, {"replace", bi_replace},
        {"trim", bi_trim}, {"ord", bi_ord}, {"chr", bi_chr},
        {"read_file", bi_read_file}, {"write_file", bi_write_file},
        {"input", bi_input}, {"args", bi_args}, {"env", bi_env},
        {"pow", bi_pow}, {"atan2", bi_atan2},
        {"min", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return bi_min_max(c, a, r, false, "min"); }},
        {"max", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return bi_min_max(c, a, r, true, "max"); }},
        {"sqrt", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return math1(c, a, r, std::sqrt, "sqrt", true); }},
        {"log", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return math1(c, a, r, std::log, "log", true); }},
        {"sin", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return math1(c, a, r, std::sin, "sin", false); }},
        {"cos", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return math1(c, a, r, std::cos, "cos", false); }},
        {"tan", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return math1(c, a, r, std::tan, "tan", false); }},
        {"abs", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return math1(c, a, r, std::fabs, "abs", false); }},
        {"floor", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return math1(c, a, r, std::floor, "floor", false); }},
        {"ceil", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return math1(c, a, r, std::ceil, "ceil", false); }},
        {"exp", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return math1(c, a, r, std::exp, "exp", false); }},
        {"asin", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return math1(c, a, r, std::asin, "asin", false); }},
        {"acos", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return math1(c, a, r, std::acos, "acos", false); }},
        {"atan", [](BuiltinContext& c, std::vector<Value>& a, Value& r) { return math1(c, a, r, std::atan, "atan", false); }},
    };
    const auto found = table.find(name);
    return found == table.end() ? nullptr : found->second;
}

} // namespace kite
