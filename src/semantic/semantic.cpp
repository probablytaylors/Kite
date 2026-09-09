#include "kite/semantic/semantic.hpp"

#include <array>
#include <string_view>
#include <utility>

namespace kite {

namespace {

bool is_math_function(std::string_view name) {
    static constexpr std::array<std::string_view, 16> names = {
        "sqrt", "pow", "sin", "cos", "tan", "log", "abs", "floor", "ceil", "exp",
        "asin", "acos", "atan", "atan2", "min", "max",
    };
    for (const auto& candidate : names) {
        if (candidate == name) return true;
    }
    return false;
}

} // namespace

const char* semantic_type_name(SemanticType type) {
    switch (type) {
    case SemanticType::Unknown: return "unknown";
    case SemanticType::Boolean: return "boolean";
    case SemanticType::Integer: return "integer";
    case SemanticType::Float: return "float";
    case SemanticType::String: return "string";
    case SemanticType::Array: return "array";
    case SemanticType::Map: return "map";
    case SemanticType::Function: return "function";
    case SemanticType::Empty: return "empty";
    }
    return "unknown";
}

bool SemanticAnalyzer::analyze(const Program& program) {
    errors_.clear();
    scopes_.clear();
    scopes_.emplace_back();
    functions_.clear();
    in_function_ = false;
    for (const auto& statement : program.statements) {
        if (const auto* function = dynamic_cast<const FunctionStatement*>(statement.get())) {
            functions_[function->name] = std::vector<SemanticType>(function->parameters.size(), SemanticType::Unknown);
        }
    }
    analyze_block(program.statements);
    return errors_.empty();
}

const std::vector<std::string>& SemanticAnalyzer::errors() const { return errors_; }

void SemanticAnalyzer::report_error(const std::string& message) { errors_.push_back(message); }

bool SemanticAnalyzer::is_numeric(SemanticType type) const {
    return type == SemanticType::Integer || type == SemanticType::Float;
}

bool SemanticAnalyzer::is_condition(SemanticType type) const {
    return type == SemanticType::Boolean || type == SemanticType::Unknown;
}

bool SemanticAnalyzer::is_assignable(SemanticType expected, SemanticType actual) const {
    return expected == SemanticType::Unknown || actual == SemanticType::Unknown || expected == actual ||
        (expected == SemanticType::Float && actual == SemanticType::Integer) ||
        (expected == SemanticType::Integer && actual == SemanticType::Float);
}

void SemanticAnalyzer::analyze_block(const std::vector<std::unique_ptr<Statement>>& statements) {
    for (const auto& statement : statements) analyze_statement(*statement);
}

void SemanticAnalyzer::analyze_statement(const Statement& statement) {
    if (const auto* let = dynamic_cast<const LetStatement*>(&statement)) {
        const SemanticType value_type = analyze_expression(*let->initializer);
        scopes_.back()[let->name] = value_type;
        return;
    }
    if (const auto* assignment = dynamic_cast<const AssignmentStatement*>(&statement)) {
        SemanticType* variable = nullptr;
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            const auto found = scope->find(assignment->name);
            if (found != scope->end()) { variable = &found->second; break; }
        }
        const SemanticType value_type = analyze_expression(*assignment->value);
        if (variable == nullptr) {
            report_error("assignment to unknown variable: " + assignment->name);
            return;
        }
        if (!is_assignable(*variable, value_type)) {
            report_error("assignment type mismatch for: " + assignment->name);
        }
        if (*variable == SemanticType::Unknown) *variable = value_type;
        return;
    }
    if (const auto* index_assignment = dynamic_cast<const IndexAssignmentStatement*>(&statement)) {
        const SemanticType target = analyze_expression(*index_assignment->target);
        const SemanticType key = analyze_expression(*index_assignment->index);
        analyze_expression(*index_assignment->value);
        if (target == SemanticType::Array && key != SemanticType::Integer && key != SemanticType::Unknown) {
            report_error("array index must be an integer");
        }
        if (target == SemanticType::Map && key != SemanticType::String && key != SemanticType::Unknown) {
            report_error("map key must be a string");
        }
        return;
    }
    if (const auto* expression = dynamic_cast<const ExpressionStatement*>(&statement)) {
        analyze_expression(*expression->expression);
        return;
    }
    if (const auto* conditional = dynamic_cast<const IfStatement*>(&statement)) {
        if (!is_condition(analyze_expression(*conditional->condition))) report_error("if condition must be boolean");
        scopes_.emplace_back(); analyze_block(conditional->then_branch); scopes_.pop_back();
        scopes_.emplace_back(); analyze_block(conditional->else_branch); scopes_.pop_back();
        return;
    }
    if (const auto* loop = dynamic_cast<const WhileStatement*>(&statement)) {
        if (!is_condition(analyze_expression(*loop->condition))) report_error("while condition must be boolean");
        scopes_.emplace_back(); analyze_block(loop->body); scopes_.pop_back();
        return;
    }
    if (const auto* loop = dynamic_cast<const ForStatement*>(&statement)) {
        scopes_.emplace_back();
        if (loop->initializer != nullptr) analyze_statement(*loop->initializer);
        if (loop->condition != nullptr && !is_condition(analyze_expression(*loop->condition))) {
            report_error("for condition must be boolean");
        }
        if (loop->step != nullptr) analyze_statement(*loop->step);
        analyze_block(loop->body);
        scopes_.pop_back();
        return;
    }
    if (const auto* function = dynamic_cast<const FunctionStatement*>(&statement)) {
        const bool previous = in_function_;
        in_function_ = true;
        scopes_.emplace_back();
        for (const auto& parameter : function->parameters) scopes_.back()[parameter] = SemanticType::Unknown;
        analyze_block(function->body);
        scopes_.pop_back();
        in_function_ = previous;
        return;
    }
    if (const auto* return_statement = dynamic_cast<const ReturnStatement*>(&statement)) {
        if (!in_function_) report_error("return outside function");
        analyze_expression(*return_statement->value);
    }
}

SemanticType SemanticAnalyzer::analyze_expression(const Expression& expression) {
    if (dynamic_cast<const BooleanExpression*>(&expression)) return SemanticType::Boolean;
    if (dynamic_cast<const IntegerExpression*>(&expression)) return SemanticType::Integer;
    if (dynamic_cast<const FloatExpression*>(&expression)) return SemanticType::Float;
    if (dynamic_cast<const StringExpression*>(&expression)) return SemanticType::String;
    if (const auto* identifier = dynamic_cast<const IdentifierExpression*>(&expression)) {
        for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
            const auto found = scope->find(identifier->name);
            if (found != scope->end()) return found->second;
        }
        if (functions_.contains(identifier->name)) return SemanticType::Function;
        report_error("unknown identifier: " + identifier->name);
        return SemanticType::Unknown;
    }
    if (const auto* array = dynamic_cast<const ArrayExpression*>(&expression)) {
        for (const auto& element : array->elements) analyze_expression(*element);
        return SemanticType::Array;
    }
    if (const auto* map = dynamic_cast<const MapExpression*>(&expression)) {
        for (const auto& entry : map->entries) {
            if (analyze_expression(*entry.first) != SemanticType::String) report_error("map keys must be strings");
            analyze_expression(*entry.second);
        }
        return SemanticType::Map;
    }
    if (const auto* index = dynamic_cast<const IndexExpression*>(&expression)) {
        const SemanticType target = analyze_expression(*index->target);
        const SemanticType position = analyze_expression(*index->index);
        if (target == SemanticType::Array && position != SemanticType::Integer &&
            position != SemanticType::Unknown) {
            report_error("array index must be integer");
        }
        if (target == SemanticType::Map && position != SemanticType::String &&
            position != SemanticType::Unknown) {
            report_error("map index must be string");
        }
        return SemanticType::Unknown;
    }
    if (const auto* unary = dynamic_cast<const UnaryExpression*>(&expression)) {
        const SemanticType operand = analyze_expression(*unary->operand);
        if (unary->operator_type == UnaryOperator::Not) {
            if (operand != SemanticType::Boolean && operand != SemanticType::Unknown) report_error("unary '!' requires boolean");
            return SemanticType::Boolean;
        }
        if (!is_numeric(operand) && operand != SemanticType::Unknown) report_error("unary '-' requires numeric value");
        return operand;
    }
    if (const auto* binary = dynamic_cast<const BinaryExpression*>(&expression)) {
        const SemanticType left = analyze_expression(*binary->left);
        const SemanticType right = analyze_expression(*binary->right);
        if (binary->operator_type == BinaryOperator::And || binary->operator_type == BinaryOperator::Or) {
            if (left != SemanticType::Boolean && left != SemanticType::Unknown) report_error("logical operand must be boolean");
            if (right != SemanticType::Boolean && right != SemanticType::Unknown) report_error("logical operand must be boolean");
            return SemanticType::Boolean;
        }
        if (binary->operator_type >= BinaryOperator::Equal) return SemanticType::Boolean;
        if (binary->operator_type == BinaryOperator::Add) {
            const bool left_stringish = left == SemanticType::String || left == SemanticType::Unknown;
            const bool right_stringish = right == SemanticType::String || right == SemanticType::Unknown;
            if ((left == SemanticType::String && right_stringish) ||
                (right == SemanticType::String && left_stringish)) {
                return SemanticType::String;
            }
        }
        if ((!is_numeric(left) && left != SemanticType::Unknown) ||
            (!is_numeric(right) && right != SemanticType::Unknown)) {
            report_error("arithmetic operands must be numeric");
        }
        return left == SemanticType::Float || right == SemanticType::Float ? SemanticType::Float : SemanticType::Integer;
    }
    if (const auto* call = dynamic_cast<const CallExpression*>(&expression)) {
        const auto* callee = dynamic_cast<const IdentifierExpression*>(call->callee.get());
        for (const auto& argument : call->arguments) analyze_expression(*argument);
        if (callee == nullptr) return SemanticType::Unknown;
        if (callee->name == "print" || callee->name == "write_file") return SemanticType::Empty;
        if (callee->name == "len") return SemanticType::Integer;
        if (callee->name == "upper" || callee->name == "lower" || callee->name == "read_file" ||
            callee->name == "type_of") return SemanticType::String;
        if (callee->name == "append") return SemanticType::Array;
        if (functions_.contains(callee->name)) return SemanticType::Unknown;
        if (is_math_function(callee->name)) return SemanticType::Float;
        return SemanticType::Unknown;
    }
    return SemanticType::Unknown;
}

} // namespace kite
