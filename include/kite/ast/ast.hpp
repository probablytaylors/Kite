#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace kite {

struct Expression {
    virtual ~Expression() = default;
};

struct Statement {
    virtual ~Statement() = default;
};

struct Program {
    std::vector<std::unique_ptr<Statement>> statements;
};

struct IdentifierExpression final : Expression {
    explicit IdentifierExpression(std::string name) : name(std::move(name)) {}

    std::string name;
};

struct IntegerExpression final : Expression {
    explicit IntegerExpression(std::int64_t value) : value(value) {}

    std::int64_t value;
};

struct BooleanExpression final : Expression {
    explicit BooleanExpression(bool value) : value(value) {}

    bool value;
};

struct FloatExpression final : Expression {
    explicit FloatExpression(double value) : value(value) {}

    double value;
};

struct StringExpression final : Expression {
    explicit StringExpression(std::string value) : value(std::move(value)) {}

    std::string value;
};

struct ArrayExpression final : Expression {
    std::vector<std::unique_ptr<Expression>> elements;
};

struct MapExpression final : Expression {
    std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> entries;
};

struct IndexExpression final : Expression {
    std::unique_ptr<Expression> target;
    std::unique_ptr<Expression> index;
};

enum class BinaryOperator {
    Add,
    Subtract,
    Multiply,
    Divide,
    Modulo,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    And,
    Or
};

enum class UnaryOperator {
    Negate,
    Not
};

struct UnaryExpression final : Expression {
    UnaryOperator operator_type;
    std::unique_ptr<Expression> operand;
};

struct BinaryExpression final : Expression {
    BinaryOperator operator_type;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

struct CallExpression final : Expression {
    std::unique_ptr<Expression> callee;
    std::vector<std::unique_ptr<Expression>> arguments;
};

struct LetStatement final : Statement {
    std::string name;
    std::unique_ptr<Expression> initializer;
};

struct AssignmentStatement final : Statement {
    std::string name;
    std::unique_ptr<Expression> value;
};

struct ExpressionStatement final : Statement {
    std::unique_ptr<Expression> expression;
};

struct IfStatement final : Statement {
    std::unique_ptr<Expression> condition;
    std::vector<std::unique_ptr<Statement>> then_branch;
    std::vector<std::unique_ptr<Statement>> else_branch;
};

struct WhileStatement final : Statement {
    std::unique_ptr<Expression> condition;
    std::vector<std::unique_ptr<Statement>> body;
};

struct ForStatement final : Statement {
    std::unique_ptr<Statement> initializer;
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> step;
    std::vector<std::unique_ptr<Statement>> body;
};

struct ReturnStatement final : Statement {
    std::unique_ptr<Expression> value;
};

struct FunctionStatement final : Statement {
    std::string name;
    std::vector<std::string> parameters;
    std::vector<std::unique_ptr<Statement>> body;
};

} // namespace kite