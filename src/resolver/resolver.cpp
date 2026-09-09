#include "kite/resolver/resolver.hpp"

#include <unordered_map>

namespace kite {

namespace {

class Scope {
public:
    int declare(const std::string& name) {
        const auto existing = slots_.find(name);
        if (existing != slots_.end()) return existing->second;
        const int slot = next_++;
        slots_.emplace(name, slot);
        return slot;
    }

    int lookup(const std::string& name) const {
        const auto found = slots_.find(name);
        return found == slots_.end() ? kGlobalSlot : found->second;
    }

    int size() const { return next_; }

private:
    std::unordered_map<std::string, int> slots_;
    int next_ = 0;
};

void resolve_block(const std::vector<std::unique_ptr<Statement>>& body, Scope& scope);

void resolve_expression(Expression& expression, const Scope& scope) {
    switch (expression.kind) {
    case NodeKind::Identifier:
        static_cast<IdentifierExpression&>(expression).slot =
            scope.lookup(static_cast<IdentifierExpression&>(expression).name);
        return;
    case NodeKind::Array:
        for (auto& element : static_cast<ArrayExpression&>(expression).elements) {
            resolve_expression(*element, scope);
        }
        return;
    case NodeKind::Map:
        for (auto& entry : static_cast<MapExpression&>(expression).entries) {
            resolve_expression(*entry.first, scope);
            resolve_expression(*entry.second, scope);
        }
        return;
    case NodeKind::Index: {
        auto& index = static_cast<IndexExpression&>(expression);
        resolve_expression(*index.target, scope);
        resolve_expression(*index.index, scope);
        return;
    }
    case NodeKind::Unary:
        resolve_expression(*static_cast<UnaryExpression&>(expression).operand, scope);
        return;
    case NodeKind::Binary: {
        auto& binary = static_cast<BinaryExpression&>(expression);
        resolve_expression(*binary.left, scope);
        resolve_expression(*binary.right, scope);
        return;
    }
    case NodeKind::Call: {
        auto& call = static_cast<CallExpression&>(expression);
        for (auto& argument : call.arguments) resolve_expression(*argument, scope);
        return;
    }
    default:
        return;
    }
}

void resolve_function(FunctionStatement& function) {
    Scope scope;
    for (const auto& parameter : function.parameters) scope.declare(parameter);
    resolve_block(function.body, scope);
    function.frame_size = scope.size();
}

void resolve_statement(Statement& statement, Scope& scope) {
    switch (statement.kind) {
    case NodeKind::Let: {
        auto& let = static_cast<LetStatement&>(statement);
        resolve_expression(*let.initializer, scope);
        let.slot = scope.declare(let.name);
        return;
    }
    case NodeKind::Assignment: {
        auto& assignment = static_cast<AssignmentStatement&>(statement);
        resolve_expression(*assignment.value, scope);
        assignment.slot = scope.lookup(assignment.name);
        return;
    }
    case NodeKind::IndexAssignment: {
        auto& assignment = static_cast<IndexAssignmentStatement&>(statement);
        resolve_expression(*assignment.target, scope);
        resolve_expression(*assignment.index, scope);
        resolve_expression(*assignment.value, scope);
        return;
    }
    case NodeKind::ExpressionStatement:
        resolve_expression(*static_cast<ExpressionStatement&>(statement).expression, scope);
        return;
    case NodeKind::If: {
        auto& conditional = static_cast<IfStatement&>(statement);
        resolve_expression(*conditional.condition, scope);
        resolve_block(conditional.then_branch, scope);
        resolve_block(conditional.else_branch, scope);
        return;
    }
    case NodeKind::While: {
        auto& loop = static_cast<WhileStatement&>(statement);
        resolve_expression(*loop.condition, scope);
        resolve_block(loop.body, scope);
        return;
    }
    case NodeKind::For: {
        auto& loop = static_cast<ForStatement&>(statement);
        if (loop.initializer != nullptr) resolve_statement(*loop.initializer, scope);
        if (loop.condition != nullptr) resolve_expression(*loop.condition, scope);
        if (loop.step != nullptr) resolve_statement(*loop.step, scope);
        resolve_block(loop.body, scope);
        return;
    }
    case NodeKind::Return: {
        auto& return_statement = static_cast<ReturnStatement&>(statement);
        if (return_statement.value != nullptr) resolve_expression(*return_statement.value, scope);
        return;
    }
    case NodeKind::Try: {
        auto& node = static_cast<TryStatement&>(statement);
        resolve_block(node.try_branch, scope);
        node.slot = scope.declare(node.name);
        resolve_block(node.catch_branch, scope);
        return;
    }
    case NodeKind::Throw:
        resolve_expression(*static_cast<ThrowStatement&>(statement).value, scope);
        return;
    case NodeKind::Function:
        resolve_function(static_cast<FunctionStatement&>(statement));
        return;
    default:
        return;
    }
}

void resolve_block(const std::vector<std::unique_ptr<Statement>>& body, Scope& scope) {
    for (const auto& statement : body) resolve_statement(*statement, scope);
}

} // namespace

void resolve(Program& program) {
    for (const auto& statement : program.statements) {
        if (statement->kind == NodeKind::Function) {
            resolve_function(static_cast<FunctionStatement&>(*statement));
        }
    }
}

} // namespace kite
