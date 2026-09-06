#include "kite/interpreter/interpreter.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace kite {

namespace {

double numeric_value(const Value& value) {
    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        return static_cast<double>(*integer);
    }
    if (const auto* floating_point = std::get_if<double>(&value)) {
        return *floating_point;
    }
    return std::numeric_limits<double>::quiet_NaN();
}

} // namespace

Interpreter::Interpreter(std::ostream& output) : output_(output) {}

bool Interpreter::execute(const Program& program) {
    scopes_.clear();
    scopes_.emplace_back();
    functions_.clear();
    return_pending_ = false;
    for (const auto& statement : program.statements) {
        if (const auto* function = dynamic_cast<const FunctionStatement*>(statement.get())) {
            functions_[function->name] = function;
        }
    }
    return execute_block(program.statements);
}

bool Interpreter::execute_block(const std::vector<std::unique_ptr<Statement>>& statements) {
    for (const auto& statement : statements) {
        if (!execute_statement(*statement)) {
            return false;
        }
        if (return_pending_) {
            break;
        }
    }

    return true;
}

const std::vector<std::string>& Interpreter::errors() const {
    return errors_;
}

bool Interpreter::execute_statement(const Statement& statement) {
    if (const auto* let = dynamic_cast<const LetStatement*>(&statement)) {
        Value value;
        if (!evaluate(*let->initializer, value)) {
            return false;
        }

        scopes_.back()[let->name] = std::move(value);
        return true;
    }

    if (const auto* assignment = dynamic_cast<const AssignmentStatement*>(&statement)) {
        Value value;
        if (!evaluate(*assignment->value, value)) {
            return false;
        }
        Value* variable = find_variable(assignment->name);
        if (variable == nullptr) {
            report_error("unknown variable: " + assignment->name);
            return false;
        }
        *variable = std::move(value);
        return true;
    }

    if (const auto* expression = dynamic_cast<const ExpressionStatement*>(&statement)) {
        Value value;
        return evaluate(*expression->expression, value);
    }

    if (const auto* conditional = dynamic_cast<const IfStatement*>(&statement)) {
        Value condition;
        if (!evaluate(*conditional->condition, condition)) {
            return false;
        }
        const auto* boolean = std::get_if<bool>(&condition);
        if (boolean == nullptr) {
            report_error("if condition must be boolean");
            return false;
        }
        return *boolean
            ? execute_block(conditional->then_branch)
            : execute_block(conditional->else_branch);
    }

    if (const auto* loop = dynamic_cast<const WhileStatement*>(&statement)) {
        while (true) {
            Value condition;
            if (!evaluate(*loop->condition, condition)) {
                return false;
            }

            const auto* boolean = std::get_if<bool>(&condition);
            if (boolean == nullptr) {
                report_error("while condition must be boolean");
                return false;
            }
            if (!*boolean) {
                return true;
            }
            if (!execute_block(loop->body)) {
                return false;
            }
        }
    }

    if (const auto* function = dynamic_cast<const FunctionStatement*>(&statement)) {
        return true;
    }

    if (const auto* return_statement = dynamic_cast<const ReturnStatement*>(&statement)) {
        if (return_statement->value == nullptr) {
            report_error("return requires a value");
            return false;
        }
        if (!evaluate(*return_statement->value, return_value_)) {
            return false;
        }
        return_pending_ = true;
        return true;
    }

    report_error("unsupported statement");
    return false;
}

bool Interpreter::evaluate(const Expression& expression, Value& value) {
    if (const auto* boolean = dynamic_cast<const BooleanExpression*>(&expression)) {
        value = boolean->value;
        return true;
    }

    if (const auto* integer = dynamic_cast<const IntegerExpression*>(&expression)) {
        value = integer->value;
        return true;
    }

    if (const auto* floating_point = dynamic_cast<const FloatExpression*>(&expression)) {
        value = floating_point->value;
        return true;
    }

    if (const auto* string = dynamic_cast<const StringExpression*>(&expression)) {
        value = string->value;
        return true;
    }

    if (const auto* array = dynamic_cast<const ArrayExpression*>(&expression)) {
        auto result = std::make_shared<ArrayValue>();
        for (const auto& element : array->elements) {
            Value value;
            if (!evaluate(*element, value)) {
                return false;
            }
            result->elements.push_back(std::move(value));
        }
        value = std::move(result);
        return true;
    }

    if (const auto* map = dynamic_cast<const MapExpression*>(&expression)) {
        auto result = std::make_shared<MapValue>();
        for (const auto& entry : map->entries) {
            Value key;
            Value value;
            if (!evaluate(*entry.first, key) || !evaluate(*entry.second, value)) {
                return false;
            }
            const auto* string_key = std::get_if<std::string>(&key);
            if (string_key == nullptr) {
                report_error("map keys must be strings");
                return false;
            }
            (*result).entries[*string_key] = std::move(value);
        }
        value = std::move(result);
        return true;
    }

    if (const auto* index = dynamic_cast<const IndexExpression*>(&expression)) {
        return evaluate_index(*index, value);
    }

    if (const auto* identifier = dynamic_cast<const IdentifierExpression*>(&expression)) {
        return evaluate_identifier(*identifier, value);
    }

    if (const auto* call = dynamic_cast<const CallExpression*>(&expression)) {
        return evaluate_call(*call, value);
    }

    if (const auto* binary = dynamic_cast<const BinaryExpression*>(&expression)) {
        Value left;
        if (!evaluate(*binary->left, left)) {
            return false;
        }

        if (binary->operator_type == BinaryOperator::And || binary->operator_type == BinaryOperator::Or) {
            const auto* left_boolean = std::get_if<bool>(&left);
            if (left_boolean == nullptr) {
                report_error("logical operators require boolean values");
                return false;
            }
            if (binary->operator_type == BinaryOperator::And && !*left_boolean) {
                value = false;
                return true;
            }
            if (binary->operator_type == BinaryOperator::Or && *left_boolean) {
                value = true;
                return true;
            }
            Value right;
            if (!evaluate(*binary->right, right)) {
                return false;
            }
            const auto* right_boolean = std::get_if<bool>(&right);
            if (right_boolean == nullptr) {
                report_error("logical operators require boolean values");
                return false;
            }
            value = *right_boolean;
            return true;
        }

        Value right;
        if (!evaluate(*binary->right, right)) {
            return false;
        }

        const bool is_comparison = binary->operator_type == BinaryOperator::Equal ||
            binary->operator_type == BinaryOperator::NotEqual ||
            binary->operator_type == BinaryOperator::Less ||
            binary->operator_type == BinaryOperator::LessEqual ||
            binary->operator_type == BinaryOperator::Greater ||
            binary->operator_type == BinaryOperator::GreaterEqual;
        const bool left_is_integer = std::holds_alternative<std::int64_t>(left);
        const bool right_is_integer = std::holds_alternative<std::int64_t>(right);
        const bool left_is_numeric = left_is_integer || std::holds_alternative<double>(left);
        const bool right_is_numeric = right_is_integer || std::holds_alternative<double>(right);

        if (binary->operator_type == BinaryOperator::Add &&
            std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right)) {
            value = std::get<std::string>(left) + std::get<std::string>(right);
            return true;
        }

        if (binary->operator_type == BinaryOperator::Equal || binary->operator_type == BinaryOperator::NotEqual) {
            const bool equal = left_is_numeric && right_is_numeric
                ? (left_is_integer ? static_cast<double>(std::get<std::int64_t>(left)) : std::get<double>(left)) ==
                    (right_is_integer ? static_cast<double>(std::get<std::int64_t>(right)) : std::get<double>(right))
                : left.index() == right.index() && left == right;
            value = binary->operator_type == BinaryOperator::Equal ? equal : !equal;
            return true;
        }

        if (!left_is_numeric || !right_is_numeric) {
            report_error(is_comparison ? "comparison requires numeric values" : "arithmetic requires numeric values");
            return false;
        }

        const double left_number = left_is_integer
            ? static_cast<double>(std::get<std::int64_t>(left))
            : std::get<double>(left);
        const double right_number = right_is_integer
            ? static_cast<double>(std::get<std::int64_t>(right))
            : std::get<double>(right);

        if (is_comparison) {
            switch (binary->operator_type) {
            case BinaryOperator::Less:
                value = left_number < right_number;
                break;
            case BinaryOperator::LessEqual:
                value = left_number <= right_number;
                break;
            case BinaryOperator::Greater:
                value = left_number > right_number;
                break;
            case BinaryOperator::GreaterEqual:
                value = left_number >= right_number;
                break;
            default:
                break;
            }
            return true;
        }

        if (binary->operator_type == BinaryOperator::Divide && right_number == 0.0) {
            report_error("cannot divide by zero");
            return false;
        }

        if (left_is_integer && right_is_integer && binary->operator_type != BinaryOperator::Divide) {
            const auto left_integer = std::get<std::int64_t>(left);
            const auto right_integer = std::get<std::int64_t>(right);
            switch (binary->operator_type) {
            case BinaryOperator::Add:
                value = left_integer + right_integer;
                break;
            case BinaryOperator::Subtract:
                value = left_integer - right_integer;
                break;
            case BinaryOperator::Multiply:
                value = left_integer * right_integer;
                break;
            case BinaryOperator::Divide:
                break;
            }
            return true;
        }

        switch (binary->operator_type) {
        case BinaryOperator::Add:
            value = left_number + right_number;
            break;
        case BinaryOperator::Subtract:
            value = left_number - right_number;
            break;
        case BinaryOperator::Multiply:
            value = left_number * right_number;
            break;
        case BinaryOperator::Divide:
            value = left_number / right_number;
            break;
        }
        return true;
    }

    if (const auto* unary = dynamic_cast<const UnaryExpression*>(&expression)) {
        Value operand;
        if (!evaluate(*unary->operand, operand)) {
            return false;
        }
        if (const auto* integer = std::get_if<std::int64_t>(&operand)) {
            value = -*integer;
            return true;
        }
        if (const auto* floating_point = std::get_if<double>(&operand)) {
            if (unary->operator_type == UnaryOperator::Not) {
                report_error("unary '!' requires a boolean value");
                return false;
            }
            value = -*floating_point;
            return true;
        }
        if (unary->operator_type == UnaryOperator::Not) {
            if (const auto* boolean = std::get_if<bool>(&operand)) {
                value = !*boolean;
                return true;
            }
            report_error("unary '!' requires a boolean value");
            return false;
        }
        report_error("unary '-' requires a numeric value");
        return false;
    }

    report_error("unsupported expression");
    return false;
}

bool Interpreter::evaluate_call(const CallExpression& call, Value& value) {
    const auto* callee = dynamic_cast<const IdentifierExpression*>(call.callee.get());
    if (callee == nullptr) {
        report_error("unknown function");
        return false;
    }

    if (callee->name == "print") {
        for (std::size_t index = 0; index < call.arguments.size(); ++index) {
            Value argument;
            if (!evaluate(*call.arguments[index], argument)) {
                return false;
            }

            if (index > 0) {
                output_ << ' ';
            }
            output_ << value_to_string(argument);
        }
        output_ << '\n';

        value = std::string();
        return true;
    }

    if (callee->name == "len") {
        if (call.arguments.size() != 1) {
            report_error("function 'len' expects one argument");
            return false;
        }
        Value argument;
        if (!evaluate(*call.arguments[0], argument)) {
            return false;
        }
        if (const auto* string = std::get_if<std::string>(&argument)) {
            value = static_cast<std::int64_t>(string->size());
            return true;
        }
        if (const auto* array = std::get_if<std::shared_ptr<ArrayValue>>(&argument)) {
            value = static_cast<std::int64_t>((*array)->elements.size());
            return true;
        }
        if (const auto* map = std::get_if<std::shared_ptr<MapValue>>(&argument)) {
            value = static_cast<std::int64_t>((*map)->entries.size());
            return true;
        }
        report_error("function 'len' requires a string, array, or map");
        return false;
    }

    if (callee->name == "upper" || callee->name == "lower") {
        if (call.arguments.size() != 1) {
            report_error("function '" + callee->name + "' expects one argument");
            return false;
        }
        Value argument;
        if (!evaluate(*call.arguments[0], argument)) {
            return false;
        }
        const auto* string = std::get_if<std::string>(&argument);
        if (string == nullptr) {
            report_error("function '" + callee->name + "' requires a string");
            return false;
        }
        std::string result = *string;
        std::transform(result.begin(), result.end(), result.begin(), [upper = callee->name == "upper"](unsigned char character) {
            return static_cast<char>(upper ? std::toupper(character) : std::tolower(character));
        });
        value = std::move(result);
        return true;
    }

    if (callee->name == "read_file") {
        if (call.arguments.size() != 1) {
            report_error("function 'read_file' expects one argument");
            return false;
        }
        Value argument;
        if (!evaluate(*call.arguments[0], argument)) {
            return false;
        }
        const auto* path = std::get_if<std::string>(&argument);
        if (path == nullptr) {
            report_error("function 'read_file' requires a string path");
            return false;
        }
        std::ifstream input(*path, std::ios::binary);
        if (!input) {
            report_error("could not open file: " + *path);
            return false;
        }
        value = std::string((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
        return true;
    }

    if (callee->name == "write_file") {
        if (call.arguments.size() != 2) {
            report_error("function 'write_file' expects two arguments");
            return false;
        }
        Value path_value;
        Value content_value;
        if (!evaluate(*call.arguments[0], path_value) || !evaluate(*call.arguments[1], content_value)) {
            return false;
        }
        const auto* path = std::get_if<std::string>(&path_value);
        const auto* content = std::get_if<std::string>(&content_value);
        if (path == nullptr || content == nullptr) {
            report_error("function 'write_file' requires string path and content");
            return false;
        }
        std::ofstream output(*path, std::ios::binary);
        if (!output) {
            report_error("could not write file: " + *path);
            return false;
        }
        output << *content;
        value = true;
        return true;
    }

    const auto math_function = [&](double (*function)(double), bool non_negative) {
        if (call.arguments.size() != 1) {
            report_error("math function '" + callee->name + "' expects one argument");
            return false;
        }
        Value argument;
        if (!evaluate(*call.arguments[0], argument)) {
            return false;
        }
        const double number = numeric_value(argument);
        if (std::isnan(number)) {
            report_error("math function '" + callee->name + "' requires a numeric argument");
            return false;
        }
        if (non_negative && number < 0.0) {
            report_error("math function '" + callee->name + "' requires a non-negative argument");
            return false;
        }
        value = function(number);
        return true;
    };

    if (callee->name == "sqrt") {
        return math_function(static_cast<double (*)(double)>(std::sqrt), true);
    }
    if (callee->name == "sin") {
        return math_function(static_cast<double (*)(double)>(std::sin), false);
    }
    if (callee->name == "cos") {
        return math_function(static_cast<double (*)(double)>(std::cos), false);
    }
    if (callee->name == "tan") {
        return math_function(static_cast<double (*)(double)>(std::tan), false);
    }
    if (callee->name == "log") {
        return math_function(static_cast<double (*)(double)>(std::log), true);
    }
    if (callee->name == "abs") {
        return math_function(static_cast<double (*)(double)>(std::fabs), false);
    }
    if (callee->name == "floor") {
        return math_function(static_cast<double (*)(double)>(std::floor), false);
    }
    if (callee->name == "ceil") {
        return math_function(static_cast<double (*)(double)>(std::ceil), false);
    }
    if (callee->name == "pow") {
        if (call.arguments.size() != 2) {
            report_error("math function 'pow' expects two arguments");
            return false;
        }
        Value base;
        Value exponent;
        if (!evaluate(*call.arguments[0], base) || !evaluate(*call.arguments[1], exponent)) {
            return false;
        }
        const double base_number = numeric_value(base);
        const double exponent_number = numeric_value(exponent);
        if (std::isnan(base_number) || std::isnan(exponent_number)) {
            report_error("math function 'pow' requires numeric arguments");
            return false;
        }
        value = std::pow(base_number, exponent_number);
        return true;
    }

    {
        const auto function = functions_.find(callee->name);
        if (function == functions_.end()) {
            report_error("unknown function: " + callee->name);
            return false;
        }

        std::vector<Value> arguments;
        for (const auto& argument_expression : call.arguments) {
            Value argument;
            if (!evaluate(*argument_expression, argument)) {
                return false;
            }
            arguments.push_back(std::move(argument));
        }
        return execute_function(*function->second, arguments, value);
    }
}

bool Interpreter::evaluate_index(const IndexExpression& index, Value& value) {
    Value target;
    Value position;
    if (!evaluate(*index.target, target) || !evaluate(*index.index, position)) {
        return false;
    }

    const auto* array = std::get_if<std::shared_ptr<ArrayValue>>(&target);
    const auto* map = std::get_if<std::shared_ptr<MapValue>>(&target);
    const auto* integer = std::get_if<std::int64_t>(&position);
    if (map != nullptr) {
        const auto* string_key = std::get_if<std::string>(&position);
        if (string_key == nullptr) {
            report_error("map index requires a string key");
            return false;
        }
        const auto entry = (*map)->entries.find(*string_key);
        if (entry == (*map)->entries.end()) {
            report_error("map key not found: " + *string_key);
            return false;
        }
        value = entry->second;
        return true;
    }
    if (array == nullptr || integer == nullptr) {
        report_error("array index requires an array and integer index");
        return false;
    }
    if (*integer < 0 || static_cast<std::size_t>(*integer) >= (*array)->elements.size()) {
        report_error("array index out of bounds");
        return false;
    }

    value = (*array)->elements[static_cast<std::size_t>(*integer)];
    return true;
}

bool Interpreter::evaluate_identifier(const IdentifierExpression& identifier, Value& value) {
    const Value* variable = find_variable(identifier.name);
    if (variable == nullptr) {
        report_error("unknown variable: " + identifier.name);
        return false;
    }

    value = *variable;
    return true;
}

bool Interpreter::execute_function(const FunctionStatement& function, const std::vector<Value>& arguments, Value& value) {
    if (arguments.size() != function.parameters.size()) {
        report_error("function '" + function.name + "' expected " +
            std::to_string(function.parameters.size()) + " arguments");
        return false;
    }

    const bool previous_return_pending = return_pending_;
    const Value previous_return_value = return_value_;
    return_pending_ = false;
    return_value_ = std::string();
    scopes_.emplace_back();
    for (std::size_t index = 0; index < function.parameters.size(); ++index) {
        scopes_.back()[function.parameters[index]] = arguments[index];
    }

    const bool succeeded = execute_block(function.body);
    if (succeeded) {
        value = return_pending_ ? return_value_ : Value(std::string());
    }
    scopes_.pop_back();
    return_pending_ = previous_return_pending;
    return_value_ = previous_return_value;
    return succeeded;
}

const Value* Interpreter::find_variable(const std::string& name) const {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto variable = scope->find(name);
        if (variable != scope->end()) {
            return &variable->second;
        }
    }
    return nullptr;
}

void Interpreter::report_error(const std::string& message) {
    errors_.push_back(message);
}

Value* Interpreter::find_variable(const std::string& name) {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto variable = scope->find(name);
        if (variable != scope->end()) {
            return &variable->second;
        }
    }
    return nullptr;
}

std::string value_to_string(const Value& value) {
    if (const auto* boolean = std::get_if<bool>(&value)) {
        return *boolean ? "true" : "false";
    }

    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        return std::to_string(*integer);
    }

    if (const auto* floating_point = std::get_if<double>(&value)) {
        std::ostringstream output;
        output << std::setprecision(15) << *floating_point;
        return output.str();
    }

    if (const auto* array = std::get_if<std::shared_ptr<ArrayValue>>(&value)) {
        std::ostringstream output;
        output << '[';
        for (std::size_t index = 0; index < (*array)->elements.size(); ++index) {
            if (index > 0) {
                output << ", ";
            }
            output << value_to_string((*array)->elements[index]);
        }
        output << ']';
        return output.str();
    }

    if (const auto* map = std::get_if<std::shared_ptr<MapValue>>(&value)) {
        std::ostringstream output;
        output << '{';
        std::vector<std::string> keys;
        for (const auto& entry : (*map)->entries) {
            keys.push_back(entry.first);
        }
        std::sort(keys.begin(), keys.end());
        for (std::size_t index = 0; index < keys.size(); ++index) {
            if (index > 0) {
                output << ", ";
            }
            output << '"' << keys[index] << "\": " << value_to_string((*map)->entries.at(keys[index]));
        }
        output << '}';
        return output.str();
    }

    return std::get<std::string>(value);
}

} // namespace kite