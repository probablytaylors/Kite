#pragma once

#include <cstddef>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "kite/ast/ast.hpp"
#include "kite/value.hpp"

namespace kite {

class Interpreter {
public:
    explicit Interpreter(std::ostream& output);

    bool execute(const Program& program);
    const std::vector<std::string>& errors() const;

private:
    enum class LoopFlow { None, Break, Continue };

    bool execute_statement(const Statement& statement);
    bool execute_block(const std::vector<std::unique_ptr<Statement>>& statements);
    bool execute_function(const FunctionStatement& function, std::size_t base, std::size_t argument_count, Value& value);
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
    LoopFlow loop_flow_ = LoopFlow::None;
    Value return_value_;
};

} // namespace kite