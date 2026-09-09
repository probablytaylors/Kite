#include "kite/value.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace kite {

bool Value::equals(const Value& other) const {
    if (is_number() && other.is_number()) {
        return as_number() == other.as_number();
    }
    if (type_ != other.type_) {
        return false;
    }
    switch (type_) {
    case ValueType::Nil: return true;
    case ValueType::Bool: return as_bool() == other.as_bool();
    case ValueType::Int: return int_ == other.int_;
    case ValueType::Float: return float_ == other.float_;
    case ValueType::String: return as_string() == other.as_string();
    case ValueType::Array:
    case ValueType::Map: return obj_ == other.obj_;
    }
    return false;
}

const char* type_name(const Value& value) {
    switch (value.type()) {
    case ValueType::Nil: return "nil";
    case ValueType::Bool: return "boolean";
    case ValueType::Int: return "integer";
    case ValueType::Float: return "float";
    case ValueType::String: return "string";
    case ValueType::Array: return "array";
    case ValueType::Map: return "map";
    }
    return "nil";
}

namespace {

std::string to_string(const Value& value, int depth) {
    switch (value.type()) {
    case ValueType::Nil:
        return "";
    case ValueType::Bool:
        return value.as_bool() ? "true" : "false";
    case ValueType::Int:
        return std::to_string(value.as_int());
    case ValueType::Float: {
        std::ostringstream output;
        output << std::setprecision(15) << value.as_float();
        return output.str();
    }
    case ValueType::String:
        return value.as_string();
    case ValueType::Array: {
        if (depth > 64) return "[...]";
        std::ostringstream output;
        output << '[';
        const auto& elements = value.as_array();
        for (std::size_t index = 0; index < elements.size(); ++index) {
            if (index > 0) output << ", ";
            output << to_string(elements[index], depth + 1);
        }
        output << ']';
        return output.str();
    }
    case ValueType::Map: {
        if (depth > 64) return "{...}";
        std::ostringstream output;
        output << '{';
        std::vector<std::string> keys;
        for (const auto& entry : value.as_map()) keys.push_back(entry.first);
        std::sort(keys.begin(), keys.end());
        for (std::size_t index = 0; index < keys.size(); ++index) {
            if (index > 0) output << ", ";
            output << '"' << keys[index] << "\": " << to_string(value.as_map().at(keys[index]), depth + 1);
        }
        output << '}';
        return output.str();
    }
    }
    return "";
}

} // namespace

std::string to_string(const Value& value) {
    return to_string(value, 0);
}

} // namespace kite
