#pragma once

#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "kite/ast/ast.hpp"

namespace kite {

struct ArrayValue;
struct MapValue;
using Value = std::variant<bool, std::int64_t, double, std::string,
    std::shared_ptr<ArrayValue>, std::shared_ptr<MapValue>>;

struct ArrayValue {
    std::vector<Value> elements;
};

struct MapValue {
    std::unordered_map<std::string, Value> entries;
};

class Interpreter {
public:
    explicit Interpreter(std::ostream& output);

    bool execute(const Program& program);
    const std::vector<std::string>& errors() const;

private:
    bool execute_statement(const Statement& statement);
    bool execute_block(const std::vector<std::unique_ptr<Statement>>& statements);
    bool execute_function(const FunctionStatement& function, const std::vector<Value>& arguments, Value& value);
    bool evaluate(const Expression& expression, Value& value);
    bool evaluate_binary(const BinaryExpression& binary, Value& value);
    bool evaluate_unary(const UnaryExpression& unary, Value& value);
    bool evaluate_array(const ArrayExpression& array, Value& value);
    bool evaluate_map(const MapExpression& map, Value& value);
    bool evaluate_call(const CallExpression& call, Value& value);
    bool evaluate_index(const IndexExpression& index, Value& value);
    bool evaluate_identifier(const IdentifierExpression& identifier, Value& value);
    void report_error(const std::string& message);
    Value* find_global(const std::string& name);
    Value& local(int slot) { return stack_[frame_base_ + static_cast<std::size_t>(slot)]; }

    std::ostream& output_;
    std::vector<std::string> errors_;
    std::unordered_map<std::string, Value> globals_;
    std::vector<Value> stack_;
    std::vector<std::size_t> frame_bases_;
    std::size_t frame_base_ = 0;
    std::unordered_map<std::string, const FunctionStatement*> functions_;
    bool return_pending_ = false;
    Value return_value_ = std::string();
};

std::string value_to_string(const Value& value);

} // namespace kite