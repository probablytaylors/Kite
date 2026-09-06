#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

#include "kite/ast/ast.hpp"

namespace kite {

using BytecodeValue = std::variant<bool, std::int64_t, double, std::string>;

enum class OpCode {
    Constant,
    Load,
    Store,
    Add,
    Subtract,
    Multiply,
    Divide,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    And,
    Or,
    Negate,
    Not,
    Print,
    Pop,
    Halt
};

struct Instruction {
    OpCode opcode;
    std::size_t operand = 0;
};

struct Chunk {
    std::vector<BytecodeValue> constants;
    std::vector<Instruction> code;
};

class BytecodeCompiler {
public:
    Chunk compile(const Program& program);
    const std::vector<std::string>& errors() const;

private:
    std::size_t add_constant(BytecodeValue value);
    void emit(OpCode opcode, std::size_t operand = 0);
    void report_error(const std::string& message);
    void compile_statement(const Statement& statement);
    void compile_expression(const Expression& expression);

    Chunk chunk_;
    std::vector<std::string> errors_;
};

class BytecodeVm {
public:
    explicit BytecodeVm(std::ostream& output);

    bool run(const Chunk& chunk);
    const std::vector<std::string>& errors() const;

private:
    bool binary_operation(OpCode opcode);
    void report_error(const std::string& message);

    std::ostream& output_;
    std::vector<BytecodeValue> stack_;
    std::unordered_map<std::string, BytecodeValue> variables_;
    std::vector<std::string> errors_;
};

std::string bytecode_value_to_string(const BytecodeValue& value);

} // namespace kite
