#include "kite/semantic/semantic.hpp"

#include <algorithm>
#include <array>
#include <string_view>

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
    structs_.clear();
    in_function_ = false;
    loop_depth_ = 0;
    for (const auto& statement : program.statements) {
        if (statement->kind == NodeKind::Function) {
            const auto& function = static_cast<const FunctionStatement&>(*statement);
            functions_[function.name] =
                std::vector<SemanticType>(function.parameters.size(), SemanticType::Unknown);
        } else if (statement->kind == NodeKind::Struct) {
            const auto& structure = static_cast<const StructStatement&>(*statement);
            structs_[structure.name] = structure.fields;
        }
    }
    analyze_block(program.statements);
    return errors_.empty();
}

const std::vector<Diagnostic>& SemanticAnalyzer::errors() const { return errors_; }

void SemanticAnalyzer::report_error(const std::string& message) {
    errors_.push_back({message, error_line_, error_column_});
}

void SemanticAnalyzer::at(const Statement& node) {
    error_line_ = node.line;
    error_column_ = node.column;
}

void SemanticAnalyzer::at(const Expression& node) {
    error_line_ = node.line;
    error_column_ = node.column;
}

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

SemanticType* SemanticAnalyzer::lookup(const std::string& name) {
    for (auto scope = scopes_.rbegin(); scope != scopes_.rend(); ++scope) {
        const auto found = scope->find(name);
        if (found != scope->end()) return &found->second;
    }
    return nullptr;
}

void SemanticAnalyzer::analyze_statement(const Statement& statement) {
    at(statement);
    switch (statement.kind) {
    case NodeKind::Let: {
        const auto& let = static_cast<const LetStatement&>(statement);
        scopes_.back()[let.name] = analyze_expression(*let.initializer);
        return;
    }
    case NodeKind::Assignment: {
        const auto& assignment = static_cast<const AssignmentStatement&>(statement);
        SemanticType* variable = lookup(assignment.name);
        const SemanticType value_type = analyze_expression(*assignment.value);
        if (variable == nullptr) {
            report_error("assignment to unknown variable: " + assignment.name);
            return;
        }
        if (!is_assignable(*variable, value_type)) {
            report_error("assignment type mismatch for: " + assignment.name);
        }
        if (*variable == SemanticType::Unknown) *variable = value_type;
        return;
    }
    case NodeKind::IndexAssignment: {
        const auto& assignment = static_cast<const IndexAssignmentStatement&>(statement);
        const SemanticType target = analyze_expression(*assignment.target);
        const SemanticType key = analyze_expression(*assignment.index);
        analyze_expression(*assignment.value);
        if (target == SemanticType::Array && key != SemanticType::Integer && key != SemanticType::Unknown) {
            report_error("array index must be an integer");
        }
        if (target == SemanticType::Map && key != SemanticType::String && key != SemanticType::Unknown) {
            report_error("map key must be a string");
        }
        return;
    }
    case NodeKind::ExpressionStatement:
        analyze_expression(*static_cast<const ExpressionStatement&>(statement).expression);
        return;
    case NodeKind::If: {
        const auto& conditional = static_cast<const IfStatement&>(statement);
        if (!is_condition(analyze_expression(*conditional.condition))) {
            report_error("if condition must be boolean");
        }
        scopes_.emplace_back();
        analyze_block(conditional.then_branch);
        scopes_.pop_back();
        scopes_.emplace_back();
        analyze_block(conditional.else_branch);
        scopes_.pop_back();
        return;
    }
    case NodeKind::While: {
        const auto& loop = static_cast<const WhileStatement&>(statement);
        if (!is_condition(analyze_expression(*loop.condition))) {
            report_error("while condition must be boolean");
        }
        scopes_.emplace_back();
        ++loop_depth_;
        analyze_block(loop.body);
        --loop_depth_;
        scopes_.pop_back();
        return;
    }
    case NodeKind::For: {
        const auto& loop = static_cast<const ForStatement&>(statement);
        scopes_.emplace_back();
        if (loop.initializer != nullptr) analyze_statement(*loop.initializer);
        if (loop.condition != nullptr && !is_condition(analyze_expression(*loop.condition))) {
            report_error("for condition must be boolean");
        }
        ++loop_depth_;
        if (loop.step != nullptr) analyze_statement(*loop.step);
        analyze_block(loop.body);
        --loop_depth_;
        scopes_.pop_back();
        return;
    }
    case NodeKind::Break:
        if (loop_depth_ == 0) report_error("break outside loop");
        return;
    case NodeKind::Continue:
        if (loop_depth_ == 0) report_error("continue outside loop");
        return;
    case NodeKind::Try: {
        const auto& node = static_cast<const TryStatement&>(statement);
        scopes_.emplace_back();
        analyze_block(node.try_branch);
        scopes_.pop_back();
        scopes_.emplace_back();
        scopes_.back()[node.name] = SemanticType::String;
        analyze_block(node.catch_branch);
        scopes_.pop_back();
        return;
    }
    case NodeKind::Throw:
        analyze_expression(*static_cast<const ThrowStatement&>(statement).value);
        return;
    case NodeKind::Import:
        report_error("import is only allowed at the top level");
        return;
    case NodeKind::Struct:
        if (in_function_) report_error("struct is only allowed at the top level");
        return;
    case NodeKind::Function: {
        const auto& function = static_cast<const FunctionStatement&>(statement);
        const bool outer_in_function = in_function_;
        const int outer_loop_depth = loop_depth_;
        in_function_ = true;
        loop_depth_ = 0;
        scopes_.emplace_back();
        for (const auto& parameter : function.parameters) {
            scopes_.back()[parameter] = SemanticType::Unknown;
        }
        analyze_block(function.body);
        scopes_.pop_back();
        in_function_ = outer_in_function;
        loop_depth_ = outer_loop_depth;
        return;
    }
    case NodeKind::Return: {
        const auto& node = static_cast<const ReturnStatement&>(statement);
        if (!in_function_) report_error("return outside function");
        analyze_expression(*node.value);
        return;
    }
    default:
        return;
    }
}

void SemanticAnalyzer::analyze_struct_literal(const MapExpression& literal) {
    at(literal);
    const auto declared = structs_.find(literal.type_name);
    if (declared == structs_.end()) {
        report_error("unknown struct: " + literal.type_name);
        return;
    }
    std::vector<std::string> seen;
    for (const auto& entry : literal.entries) {
        if (entry.first->kind != NodeKind::String) continue;
        const std::string& field = static_cast<const StringExpression&>(*entry.first).value;
        if (std::find(seen.begin(), seen.end(), field) != seen.end()) {
            report_error("duplicate field '" + field + "' in " + literal.type_name);
        }
        seen.push_back(field);
        if (std::find(declared->second.begin(), declared->second.end(), field) == declared->second.end()) {
            report_error(literal.type_name + " has no field '" + field + "'");
        }
    }
    for (const auto& field : declared->second) {
        if (std::find(seen.begin(), seen.end(), field) == seen.end()) {
            report_error("missing field '" + field + "' in " + literal.type_name);
        }
    }
}

SemanticType SemanticAnalyzer::analyze_expression(const Expression& expression) {
    at(expression);
    switch (expression.kind) {
    case NodeKind::Boolean:
        return SemanticType::Boolean;
    case NodeKind::Integer:
        return SemanticType::Integer;
    case NodeKind::Float:
        return SemanticType::Float;
    case NodeKind::String:
        return SemanticType::String;
    case NodeKind::Identifier: {
        const auto& identifier = static_cast<const IdentifierExpression&>(expression);
        if (const SemanticType* type = lookup(identifier.name)) return *type;
        if (functions_.contains(identifier.name)) return SemanticType::Function;
        report_error("unknown identifier: " + identifier.name);
        return SemanticType::Unknown;
    }
    case NodeKind::Array: {
        const auto& array = static_cast<const ArrayExpression&>(expression);
        for (const auto& element : array.elements) analyze_expression(*element);
        return SemanticType::Array;
    }
    case NodeKind::Map: {
        const auto& map = static_cast<const MapExpression&>(expression);
        for (const auto& entry : map.entries) {
            if (analyze_expression(*entry.first) != SemanticType::String) {
                report_error("map keys must be strings");
            }
            analyze_expression(*entry.second);
        }
        if (!map.type_name.empty()) analyze_struct_literal(map);
        return SemanticType::Map;
    }
    case NodeKind::Index: {
        const auto& index = static_cast<const IndexExpression&>(expression);
        const SemanticType target = analyze_expression(*index.target);
        const SemanticType position = analyze_expression(*index.index);
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
    case NodeKind::Unary: {
        const auto& unary = static_cast<const UnaryExpression&>(expression);
        const SemanticType operand = analyze_expression(*unary.operand);
        if (unary.operator_type == UnaryOperator::Not) {
            if (operand != SemanticType::Boolean && operand != SemanticType::Unknown) {
                report_error("unary '!' requires boolean");
            }
            return SemanticType::Boolean;
        }
        if (!is_numeric(operand) && operand != SemanticType::Unknown) {
            report_error("unary '-' requires numeric value");
        }
        return operand;
    }
    case NodeKind::Binary:
        return analyze_binary(static_cast<const BinaryExpression&>(expression));
    case NodeKind::Call:
        return analyze_call(static_cast<const CallExpression&>(expression));
    default:
        return SemanticType::Unknown;
    }
}

SemanticType SemanticAnalyzer::analyze_binary(const BinaryExpression& binary) {
    const SemanticType left = analyze_expression(*binary.left);
    const SemanticType right = analyze_expression(*binary.right);
    at(binary);

    if (binary.operator_type == BinaryOperator::And || binary.operator_type == BinaryOperator::Or) {
        if (left != SemanticType::Boolean && left != SemanticType::Unknown) {
            report_error("logical operand must be boolean");
        }
        if (right != SemanticType::Boolean && right != SemanticType::Unknown) {
            report_error("logical operand must be boolean");
        }
        return SemanticType::Boolean;
    }
    if (binary.operator_type >= BinaryOperator::Equal) return SemanticType::Boolean;

    if (binary.operator_type == BinaryOperator::Add) {
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
    return left == SemanticType::Float || right == SemanticType::Float
        ? SemanticType::Float
        : SemanticType::Integer;
}

SemanticType SemanticAnalyzer::analyze_call(const CallExpression& call) {
    for (const auto& argument : call.arguments) analyze_expression(*argument);
    at(call);
    if (call.callee->kind != NodeKind::Identifier) return SemanticType::Unknown;

    const std::string& name = static_cast<const IdentifierExpression&>(*call.callee).name;
    if (name == "print" || name == "write_file") return SemanticType::Empty;
    if (name == "len" || name == "int") return SemanticType::Integer;
    if (name == "float") return SemanticType::Float;
    if (name == "str" || name == "upper" || name == "lower" || name == "trim" || name == "replace" ||
        name == "substring" || name == "join" || name == "read_file" || name == "type_of") {
        return SemanticType::String;
    }
    if (name == "append" || name == "split" || name == "keys" || name == "range") {
        return SemanticType::Array;
    }
    if (functions_.contains(name)) return SemanticType::Unknown;
    if (is_math_function(name)) return SemanticType::Float;
    return SemanticType::Unknown;
}

} // namespace kite
