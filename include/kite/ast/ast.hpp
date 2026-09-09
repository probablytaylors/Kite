#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace kite {

enum class NodeKind {
    Identifier,
    Integer,
    Boolean,
    Float,
    String,
    Array,
    Map,
    Index,
    Unary,
    Binary,
    Call,
    Let,
    Assignment,
    ExpressionStatement,
    If,
    While,
    For,
    Return,
    Function
};

struct Expression {
    explicit Expression(NodeKind kind) : kind(kind) {}
    virtual ~Expression() = default;

    NodeKind kind;
};

struct Statement {
    explicit Statement(NodeKind kind) : kind(kind) {}
    virtual ~Statement() = default;

    NodeKind kind;
};

struct Program {
    std::vector<std::unique_ptr<Statement>> statements;
};

// slot >= 0 selects a slot in the current call frame; slot < 0 means the name
// is a global and is looked up by name. Filled in by the resolver.
constexpr int kGlobalSlot = -1;

struct IdentifierExpression final : Expression {
    explicit IdentifierExpression(std::string name)
        : Expression(NodeKind::Identifier), name(std::move(name)) {}

    std::string name;
    int slot = kGlobalSlot;
};

struct IntegerExpression final : Expression {
    explicit IntegerExpression(std::int64_t value)
        : Expression(NodeKind::Integer), value(value) {}

    std::int64_t value;
};

struct BooleanExpression final : Expression {
    explicit BooleanExpression(bool value)
        : Expression(NodeKind::Boolean), value(value) {}

    bool value;
};

struct FloatExpression final : Expression {
    explicit FloatExpression(double value)
        : Expression(NodeKind::Float), value(value) {}

    double value;
};

struct StringExpression final : Expression {
    explicit StringExpression(std::string value)
        : Expression(NodeKind::String), value(std::move(value)) {}

    std::string value;
};

struct ArrayExpression final : Expression {
    ArrayExpression() : Expression(NodeKind::Array) {}

    std::vector<std::unique_ptr<Expression>> elements;
};

struct MapExpression final : Expression {
    MapExpression() : Expression(NodeKind::Map) {}

    std::vector<std::pair<std::unique_ptr<Expression>, std::unique_ptr<Expression>>> entries;
};

struct IndexExpression final : Expression {
    IndexExpression() : Expression(NodeKind::Index) {}

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
    UnaryExpression() : Expression(NodeKind::Unary) {}

    UnaryOperator operator_type;
    std::unique_ptr<Expression> operand;
};

struct BinaryExpression final : Expression {
    BinaryExpression() : Expression(NodeKind::Binary) {}

    BinaryOperator operator_type;
    std::unique_ptr<Expression> left;
    std::unique_ptr<Expression> right;
};

struct CallExpression final : Expression {
    CallExpression() : Expression(NodeKind::Call) {}

    std::unique_ptr<Expression> callee;
    std::vector<std::unique_ptr<Expression>> arguments;
};

struct LetStatement final : Statement {
    LetStatement() : Statement(NodeKind::Let) {}

    std::string name;
    std::string declared_type;
    std::unique_ptr<Expression> initializer;
    int slot = kGlobalSlot;
};

struct AssignmentStatement final : Statement {
    AssignmentStatement() : Statement(NodeKind::Assignment) {}

    std::string name;
    std::unique_ptr<Expression> value;
    int slot = kGlobalSlot;
};

struct ExpressionStatement final : Statement {
    ExpressionStatement() : Statement(NodeKind::ExpressionStatement) {}

    std::unique_ptr<Expression> expression;
};

struct IfStatement final : Statement {
    IfStatement() : Statement(NodeKind::If) {}

    std::unique_ptr<Expression> condition;
    std::vector<std::unique_ptr<Statement>> then_branch;
    std::vector<std::unique_ptr<Statement>> else_branch;
};

struct WhileStatement final : Statement {
    WhileStatement() : Statement(NodeKind::While) {}

    std::unique_ptr<Expression> condition;
    std::vector<std::unique_ptr<Statement>> body;
};

struct ForStatement final : Statement {
    ForStatement() : Statement(NodeKind::For) {}

    std::unique_ptr<Statement> initializer;
    std::unique_ptr<Expression> condition;
    std::unique_ptr<Statement> step;
    std::vector<std::unique_ptr<Statement>> body;
};

struct ReturnStatement final : Statement {
    ReturnStatement() : Statement(NodeKind::Return) {}

    std::unique_ptr<Expression> value;
};

struct FunctionStatement final : Statement {
    FunctionStatement() : Statement(NodeKind::Function) {}

    std::string name;
    std::vector<std::string> parameters;
    std::vector<std::string> parameter_types;
    std::string return_type;
    std::vector<std::unique_ptr<Statement>> body;
    int frame_size = 0;
};

} // namespace kite
