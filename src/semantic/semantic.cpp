#include "kite/semantic/semantic.hpp"

#include <utility>

namespace kite {

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

bool SemanticAnalyzer::is_assignable(SemanticType expected, SemanticType actual) const {
    return expected == SemanticType::Unknown || actual == SemanticType::Unknown || expected == actual ||
        (expected == SemanticType::Float && actual == SemanticType::Integer);
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
        const auto variable = [&]() -> SemanticType {
            for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
                const auto found = scope->find(assignment->name);
                if (found != scope->end()) return found->second;
            }
            return SemanticType::Unknown;
        }();
        if (variable == SemanticType::Unknown) report_error("assignment to unknown variable: " + assignment->name);
        const SemanticType value_type = analyze_expression(*assignment->value);
        if (!is_assignable(variable, value_type)) report_error("assignment type mismatch for: " + assignment->name);
        return;
    }
    if (const auto* expression = dynamic_cast<const ExpressionStatement*>(&statement)) {
        analyze_expression(*expression->expression);
        return;
    }
    if (const auto* conditional = dynamic_cast<const IfStatement*>(&statement)) {
        if (analyze_expression(*conditional->condition) != SemanticType::Boolean) report_error("if condition must be boolean");
        scopes_.emplace_back(); analyze_block(conditional->then_branch); scopes_.pop_back();
        scopes_.emplace_back(); analyze_block(conditional->else_branch); scopes_.pop_back();
        return;
    }
    if (const auto* loop = dynamic_cast<const WhileStatement*>(&statement)) {
        if (analyze_expression(*loop->condition) != SemanticType::Boolean) report_error("while condition must be boolean");
        scopes_.emplace_back(); analyze_block(loop->body); scopes_.pop_back();
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
        if (target == SemanticType::Array && position != SemanticType::Integer) report_error("array index must be integer");
        if (target == SemanticType::Map && position != SemanticType::String) report_error("map index must be string");
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
        if (binary->operator_type == BinaryOperator::Add && left == SemanticType::String && right == SemanticType::String) return SemanticType::String;
        if (binary->operator_type >= BinaryOperator::Equal) return SemanticType::Boolean;
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
        if (callee->name == "upper" || callee->name == "lower" || callee->name == "read_file") return SemanticType::String;
        if (functions_.contains(callee->name)) return SemanticType::Unknown;
        return SemanticType::Float;
    }
    return SemanticType::Unknown;
}

} // namespace kite
