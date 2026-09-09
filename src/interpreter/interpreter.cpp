#include "kite/interpreter/interpreter.hpp"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <iterator>
#include <limits>
#include <utility>

namespace kite {

namespace {

double numeric_value(const Value& value) {
    return value.is_number() ? value.as_number() : std::numeric_limits<double>::quiet_NaN();
}

} // namespace

Interpreter::Interpreter(std::ostream& output) : output_(output) {}

bool Interpreter::execute(const Program& program) {
    globals_.clear();
    stack_.clear();
    stack_.reserve(8192);
    frame_bases_.clear();
    frame_base_ = 0;
    functions_.clear();
    return_pending_ = false;
    for (const auto& statement : program.statements) {
        if (statement->kind == NodeKind::Function) {
            const auto* function = static_cast<const FunctionStatement*>(statement.get());
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
    switch (statement.kind) {
    case NodeKind::Let: {
        const auto& let = static_cast<const LetStatement&>(statement);
        Value value;
        if (!evaluate(*let.initializer, value)) {
            return false;
        }
        if (let.slot >= 0) {
            local(let.slot) = std::move(value);
        } else {
            globals_[let.name] = std::move(value);
        }
        return true;
    }

    case NodeKind::Assignment: {
        const auto& assignment = static_cast<const AssignmentStatement&>(statement);
        Value value;
        if (!evaluate(*assignment.value, value)) {
            return false;
        }
        if (assignment.slot >= 0) {
            local(assignment.slot) = std::move(value);
            return true;
        }
        Value* variable = find_global(assignment.name);
        if (variable == nullptr) {
            report_error("unknown variable: " + assignment.name);
            return false;
        }
        *variable = std::move(value);
        return true;
    }

    case NodeKind::ExpressionStatement: {
        Value value;
        return evaluate(*static_cast<const ExpressionStatement&>(statement).expression, value);
    }

    case NodeKind::If: {
        const auto& conditional = static_cast<const IfStatement&>(statement);
        Value condition;
        if (!evaluate(*conditional.condition, condition)) {
            return false;
        }
        if (!condition.is_bool()) {
            report_error("if condition must be boolean");
            return false;
        }
        return condition.as_bool()
            ? execute_block(conditional.then_branch)
            : execute_block(conditional.else_branch);
    }

    case NodeKind::While: {
        const auto& loop = static_cast<const WhileStatement&>(statement);
        while (true) {
            Value condition;
            if (!evaluate(*loop.condition, condition)) {
                return false;
            }
            if (!condition.is_bool()) {
                report_error("while condition must be boolean");
                return false;
            }
            if (!condition.as_bool()) {
                return true;
            }
            if (!execute_block(loop.body)) {
                return false;
            }
            if (return_pending_) {
                return true;
            }
        }
    }

    case NodeKind::For: {
        const auto& loop = static_cast<const ForStatement&>(statement);
        if (loop.initializer != nullptr && !execute_statement(*loop.initializer)) {
            return false;
        }
        while (true) {
            if (loop.condition != nullptr) {
                Value condition;
                if (!evaluate(*loop.condition, condition)) {
                    return false;
                }
                if (!condition.is_bool()) {
                    report_error("for condition must be boolean");
                    return false;
                }
                if (!condition.as_bool()) {
                    return true;
                }
            }
            if (!execute_block(loop.body)) {
                return false;
            }
            if (return_pending_) {
                return true;
            }
            if (loop.step != nullptr && !execute_statement(*loop.step)) {
                return false;
            }
        }
    }

    case NodeKind::Function:
        return true;

    case NodeKind::Return: {
        const auto& return_statement = static_cast<const ReturnStatement&>(statement);
        if (return_statement.value == nullptr) {
            report_error("return requires a value");
            return false;
        }
        if (!evaluate(*return_statement.value, return_value_)) {
            return false;
        }
        return_pending_ = true;
        return true;
    }

    default:
        report_error("unsupported statement");
        return false;
    }
}

bool Interpreter::evaluate(const Expression& expression, Value& value) {
    switch (expression.kind) {
    case NodeKind::Boolean:
        value = static_cast<const BooleanExpression&>(expression).value;
        return true;
    case NodeKind::Integer:
        value = static_cast<const IntegerExpression&>(expression).value;
        return true;
    case NodeKind::Float:
        value = static_cast<const FloatExpression&>(expression).value;
        return true;
    case NodeKind::String:
        value = Value::string(static_cast<const StringExpression&>(expression).value);
        return true;
    case NodeKind::Array:
        return evaluate_array(static_cast<const ArrayExpression&>(expression), value);
    case NodeKind::Map:
        return evaluate_map(static_cast<const MapExpression&>(expression), value);
    case NodeKind::Index:
        return evaluate_index(static_cast<const IndexExpression&>(expression), value);
    case NodeKind::Identifier:
        return evaluate_identifier(static_cast<const IdentifierExpression&>(expression), value);
    case NodeKind::Call:
        return evaluate_call(static_cast<const CallExpression&>(expression), value);
    case NodeKind::Binary:
        return evaluate_binary(static_cast<const BinaryExpression&>(expression), value);
    case NodeKind::Unary:
        return evaluate_unary(static_cast<const UnaryExpression&>(expression), value);
    default:
        report_error("unsupported expression");
        return false;
    }
}

bool Interpreter::evaluate_array(const ArrayExpression& array, Value& value) {
    Value result = Value::array();
    auto& elements = result.as_array();
    for (const auto& element : array.elements) {
        Value element_value;
        if (!evaluate(*element, element_value)) {
            return false;
        }
        elements.push_back(std::move(element_value));
    }
    value = std::move(result);
    return true;
}

bool Interpreter::evaluate_map(const MapExpression& map, Value& value) {
    Value result = Value::map();
    auto& entries = result.as_map();
    for (const auto& entry : map.entries) {
        Value key;
        Value entry_value;
        if (!evaluate(*entry.first, key) || !evaluate(*entry.second, entry_value)) {
            return false;
        }
        if (!key.is_string()) {
            report_error("map keys must be strings");
            return false;
        }
        entries[key.as_string()] = std::move(entry_value);
    }
    value = std::move(result);
    return true;
}

bool Interpreter::evaluate_binary(const BinaryExpression& binary, Value& value) {
    Value left;
    if (!evaluate(*binary.left, left)) {
        return false;
    }

    if (binary.operator_type == BinaryOperator::And || binary.operator_type == BinaryOperator::Or) {
        if (!left.is_bool()) {
            report_error("logical operators require boolean values");
            return false;
        }
        if (binary.operator_type == BinaryOperator::And && !left.as_bool()) {
            value = false;
            return true;
        }
        if (binary.operator_type == BinaryOperator::Or && left.as_bool()) {
            value = true;
            return true;
        }
        Value right;
        if (!evaluate(*binary.right, right)) {
            return false;
        }
        if (!right.is_bool()) {
            report_error("logical operators require boolean values");
            return false;
        }
        value = right.as_bool();
        return true;
    }

    Value right;
    if (!evaluate(*binary.right, right)) {
        return false;
    }

    const bool is_comparison = binary.operator_type == BinaryOperator::Less ||
        binary.operator_type == BinaryOperator::LessEqual ||
        binary.operator_type == BinaryOperator::Greater ||
        binary.operator_type == BinaryOperator::GreaterEqual;

    if (binary.operator_type == BinaryOperator::Add && left.is_string() && right.is_string()) {
        value = Value::string(left.as_string() + right.as_string());
        return true;
    }

    if (binary.operator_type == BinaryOperator::Equal || binary.operator_type == BinaryOperator::NotEqual) {
        const bool equal = left.equals(right);
        value = binary.operator_type == BinaryOperator::Equal ? equal : !equal;
        return true;
    }

    if (!left.is_number() || !right.is_number()) {
        report_error(is_comparison ? "comparison requires numeric values" : "arithmetic requires numeric values");
        return false;
    }

    const bool both_integer = left.is_int() && right.is_int();
    const double left_number = left.as_number();
    const double right_number = right.as_number();

    if (is_comparison) {
        switch (binary.operator_type) {
        case BinaryOperator::Less: value = left_number < right_number; break;
        case BinaryOperator::LessEqual: value = left_number <= right_number; break;
        case BinaryOperator::Greater: value = left_number > right_number; break;
        case BinaryOperator::GreaterEqual: value = left_number >= right_number; break;
        default: break;
        }
        return true;
    }

    if ((binary.operator_type == BinaryOperator::Divide ||
         binary.operator_type == BinaryOperator::Modulo) && right_number == 0.0) {
        report_error(binary.operator_type == BinaryOperator::Modulo
            ? "cannot take remainder by zero"
            : "cannot divide by zero");
        return false;
    }

    if (both_integer && binary.operator_type != BinaryOperator::Divide) {
        const std::int64_t a = left.as_int();
        const std::int64_t b = right.as_int();
        switch (binary.operator_type) {
        case BinaryOperator::Add: value = a + b; break;
        case BinaryOperator::Subtract: value = a - b; break;
        case BinaryOperator::Multiply: value = a * b; break;
        case BinaryOperator::Modulo: value = a % b; break;
        default: break;
        }
        return true;
    }

    switch (binary.operator_type) {
    case BinaryOperator::Add: value = left_number + right_number; break;
    case BinaryOperator::Subtract: value = left_number - right_number; break;
    case BinaryOperator::Multiply: value = left_number * right_number; break;
    case BinaryOperator::Divide: value = left_number / right_number; break;
    case BinaryOperator::Modulo: value = std::fmod(left_number, right_number); break;
    default: break;
    }
    return true;
}

bool Interpreter::evaluate_unary(const UnaryExpression& unary, Value& value) {
    Value operand;
    if (!evaluate(*unary.operand, operand)) {
        return false;
    }
    if (unary.operator_type == UnaryOperator::Not) {
        if (!operand.is_bool()) {
            report_error("unary '!' requires a boolean value");
            return false;
        }
        value = !operand.as_bool();
        return true;
    }
    if (operand.is_int()) {
        value = -operand.as_int();
        return true;
    }
    if (operand.is_float()) {
        value = -operand.as_float();
        return true;
    }
    report_error("unary '-' requires a numeric value");
    return false;
}

bool Interpreter::evaluate_call(const CallExpression& call, Value& value) {
    if (call.callee->kind != NodeKind::Identifier) {
        report_error("unknown function");
        return false;
    }
    const auto* callee = static_cast<const IdentifierExpression*>(call.callee.get());

    if (const auto function = functions_.find(callee->name); function != functions_.end()) {
        const std::size_t base = stack_.size();
        for (const auto& argument_expression : call.arguments) {
            Value argument;
            if (!evaluate(*argument_expression, argument)) {
                stack_.resize(base);
                return false;
            }
            stack_.push_back(std::move(argument));
        }
        return execute_function(*function->second, base, call.arguments.size(), value);
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
            output_ << to_string(argument);
        }
        output_ << '\n';
        value = Value::string("");
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
        if (argument.is_string()) {
            value = static_cast<std::int64_t>(argument.as_string().size());
            return true;
        }
        if (argument.is_array()) {
            value = static_cast<std::int64_t>(argument.as_array().size());
            return true;
        }
        if (argument.is_map()) {
            value = static_cast<std::int64_t>(argument.as_map().size());
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
        if (!argument.is_string()) {
            report_error("function '" + callee->name + "' requires a string");
            return false;
        }
        std::string result = argument.as_string();
        const bool upper = callee->name == "upper";
        std::transform(result.begin(), result.end(), result.begin(), [upper](unsigned char character) {
            return static_cast<char>(upper ? std::toupper(character) : std::tolower(character));
        });
        value = Value::string(std::move(result));
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
        if (!argument.is_string()) {
            report_error("function 'read_file' requires a string path");
            return false;
        }
        std::ifstream input(argument.as_string(), std::ios::binary);
        if (!input) {
            report_error("could not open file: " + argument.as_string());
            return false;
        }
        value = Value::string(std::string((std::istreambuf_iterator<char>(input)),
            std::istreambuf_iterator<char>()));
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
        if (!path_value.is_string() || !content_value.is_string()) {
            report_error("function 'write_file' requires string path and content");
            return false;
        }
        std::ofstream output(path_value.as_string(), std::ios::binary);
        if (!output) {
            report_error("could not write file: " + path_value.as_string());
            return false;
        }
        output << content_value.as_string();
        value = true;
        return true;
    }

    if (callee->name == "type_of") {
        if (call.arguments.size() != 1) {
            report_error("function 'type_of' expects one argument");
            return false;
        }
        Value argument;
        if (!evaluate(*call.arguments[0], argument)) return false;
        value = Value::string(type_name(argument));
        return true;
    }

    if (callee->name == "append") {
        if (call.arguments.size() != 2) {
            report_error("function 'append' expects two arguments");
            return false;
        }
        Value target;
        Value element;
        if (!evaluate(*call.arguments[0], target) || !evaluate(*call.arguments[1], element)) return false;
        if (!target.is_array()) {
            report_error("function 'append' requires an array");
            return false;
        }
        target.as_array().push_back(std::move(element));
        value = std::move(target);
        return true;
    }

    if (callee->name == "pop") {
        if (call.arguments.size() != 1) {
            report_error("function 'pop' expects one argument");
            return false;
        }
        Value target;
        if (!evaluate(*call.arguments[0], target)) return false;
        if (!target.is_array() || target.as_array().empty()) {
            report_error("function 'pop' requires a non-empty array");
            return false;
        }
        value = std::move(target.as_array().back());
        target.as_array().pop_back();
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

    if (callee->name == "sqrt") return math_function(static_cast<double (*)(double)>(std::sqrt), true);
    if (callee->name == "sin") return math_function(static_cast<double (*)(double)>(std::sin), false);
    if (callee->name == "cos") return math_function(static_cast<double (*)(double)>(std::cos), false);
    if (callee->name == "tan") return math_function(static_cast<double (*)(double)>(std::tan), false);
    if (callee->name == "log") return math_function(static_cast<double (*)(double)>(std::log), true);
    if (callee->name == "abs") return math_function(static_cast<double (*)(double)>(std::fabs), false);
    if (callee->name == "floor") return math_function(static_cast<double (*)(double)>(std::floor), false);
    if (callee->name == "ceil") return math_function(static_cast<double (*)(double)>(std::ceil), false);
    if (callee->name == "exp") return math_function(static_cast<double (*)(double)>(std::exp), false);
    if (callee->name == "asin") return math_function(static_cast<double (*)(double)>(std::asin), false);
    if (callee->name == "acos") return math_function(static_cast<double (*)(double)>(std::acos), false);
    if (callee->name == "atan") return math_function(static_cast<double (*)(double)>(std::atan), false);

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

    if (callee->name == "atan2" || callee->name == "min" || callee->name == "max") {
        if (call.arguments.size() != 2) {
            report_error("function '" + callee->name + "' expects two arguments");
            return false;
        }
        Value left;
        Value right;
        if (!evaluate(*call.arguments[0], left) || !evaluate(*call.arguments[1], right)) return false;
        if (!left.is_number() || !right.is_number()) {
            report_error("function '" + callee->name + "' requires numeric arguments");
            return false;
        }
        const double left_number = left.as_number();
        const double right_number = right.as_number();
        if (callee->name == "atan2") value = std::atan2(left_number, right_number);
        else if (callee->name == "min") value = std::min(left_number, right_number);
        else value = std::max(left_number, right_number);
        return true;
    }

    report_error("unknown function: " + callee->name);
    return false;
}

bool Interpreter::evaluate_index(const IndexExpression& index, Value& value) {
    Value target;
    Value position;
    if (!evaluate(*index.target, target) || !evaluate(*index.index, position)) {
        return false;
    }

    if (target.is_map()) {
        if (!position.is_string()) {
            report_error("map index requires a string key");
            return false;
        }
        const auto& entries = target.as_map();
        const auto entry = entries.find(position.as_string());
        if (entry == entries.end()) {
            report_error("map key not found: " + position.as_string());
            return false;
        }
        value = entry->second;
        return true;
    }
    if (!target.is_array() || !position.is_int()) {
        report_error("array index requires an array and integer index");
        return false;
    }
    const auto& elements = target.as_array();
    const std::int64_t position_index = position.as_int();
    if (position_index < 0 || static_cast<std::size_t>(position_index) >= elements.size()) {
        report_error("array index out of bounds");
        return false;
    }
    value = elements[static_cast<std::size_t>(position_index)];
    return true;
}

bool Interpreter::evaluate_identifier(const IdentifierExpression& identifier, Value& value) {
    if (identifier.slot >= 0) {
        value = local(identifier.slot);
        return true;
    }
    const auto global = globals_.find(identifier.name);
    if (global == globals_.end()) {
        report_error("unknown variable: " + identifier.name);
        return false;
    }
    value = global->second;
    return true;
}

bool Interpreter::execute_function(const FunctionStatement& function, std::size_t base, std::size_t argument_count, Value& value) {
    if (argument_count != function.parameters.size()) {
        report_error("function '" + function.name + "' expected " +
            std::to_string(function.parameters.size()) + " arguments");
        stack_.resize(base);
        return false;
    }

    constexpr std::size_t kMaxCallDepth = 1000;
    if (frame_bases_.size() >= kMaxCallDepth) {
        report_error("maximum call depth exceeded");
        stack_.resize(base);
        return false;
    }

    const bool previous_return_pending = return_pending_;
    Value previous_return_value = std::move(return_value_);
    return_pending_ = false;

    frame_bases_.push_back(frame_base_);
    frame_base_ = base;
    stack_.resize(base + static_cast<std::size_t>(function.frame_size));

    const bool succeeded = execute_block(function.body);
    Value result;
    if (succeeded && return_pending_) {
        result = std::move(return_value_);
    }
    stack_.resize(base);
    frame_base_ = frame_bases_.back();
    frame_bases_.pop_back();
    return_pending_ = previous_return_pending;
    return_value_ = std::move(previous_return_value);
    if (succeeded) {
        value = std::move(result);
    }
    return succeeded;
}

Value* Interpreter::find_global(const std::string& name) {
    const auto found = globals_.find(name);
    return found == globals_.end() ? nullptr : &found->second;
}

void Interpreter::report_error(const std::string& message) {
    errors_.push_back(message);
}

} // namespace kite
