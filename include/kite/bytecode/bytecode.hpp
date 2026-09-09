#pragma once

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "kite/ast/ast.hpp"
#include "kite/value.hpp"

namespace kite {

enum class OpCode {
    Constant,
    Load,
    Store,
    LoadLocal,
    StoreLocal,
    IncLocal,
    DecLocal,
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
    Or,
    Negate,
    Not,
    Print,
    Pop,
    Jump,
    JumpIfFalse,
    MakeArray,
    MakeMap,
    Index,
    Call,
    Return,
    ReturnLocal,
    ReturnConst,
    Halt
};

struct Instruction {
    OpCode opcode;
    std::size_t operand = 0;
};

struct FunctionInfo {
    std::size_t address = 0;
    std::uint32_t arity = 0;
    std::uint32_t frame_size = 0;
};

struct Chunk {
    std::vector<Value> constants;
    std::vector<Instruction> code;
    std::vector<FunctionInfo> functions;
};

class BytecodeCompiler {
public:
    Chunk compile(const Program& program);
    const std::vector<std::string>& errors() const;

private:
    std::size_t add_constant(Value value);
    void emit(OpCode opcode, std::size_t operand = 0);
    void report_error(const std::string& message);
    void compile_statement(const Statement& statement);
    void compile_expression(const Expression& expression);
    std::size_t emit_jump(OpCode opcode);
    void patch_jump(std::size_t instruction, std::size_t target);

    Chunk chunk_;
    std::unordered_map<std::string, std::size_t> function_indices_;
    std::vector<std::string> errors_;
};

class BytecodeVm {
public:
    explicit BytecodeVm(std::ostream& output);

    bool run(const Chunk& chunk);
    const std::vector<std::string>& errors() const;

private:
    struct Frame {
        std::size_t return_ip;
        std::size_t base;
    };

    bool binary_operation(OpCode opcode);
    void report_error(const std::string& message);

    std::ostream& output_;
    std::vector<Value> stack_;
    std::vector<Frame> frames_;
    std::size_t base_ = 0;
    std::unordered_map<std::string, Value> variables_;
    std::vector<std::string> errors_;
};

inline constexpr char kBytecodeMagic[4] = {'K', 'I', 'T', 'E'};
inline constexpr std::uint32_t kBytecodeFormatVersion = 8;

bool validate_chunk(const Chunk& chunk, std::string& error);
bool save_bytecode(const Chunk& chunk, const std::string& path, std::string& error);
bool load_bytecode(const std::string& path, Chunk& chunk, std::string& error);

} // namespace kite
