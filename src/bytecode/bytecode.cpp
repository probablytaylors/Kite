#include "kite/bytecode/bytecode.hpp"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <fstream>
#include <limits>
#include <ostream>
#include <sstream>
#include <unordered_map>
#include <utility>

namespace kite {

namespace {

bool is_numeric(const BytecodeValue& value) {
    return std::holds_alternative<std::int64_t>(value) || std::holds_alternative<double>(value);
}

bool is_boolean(const BytecodeValue& value) { return std::holds_alternative<bool>(value); }

double as_number(const BytecodeValue& value) {
    if (const auto* integer = std::get_if<std::int64_t>(&value)) {
        return static_cast<double>(*integer);
    }
    return std::get<double>(value);
}

}

Chunk BytecodeCompiler::compile(const Program& program) {
    chunk_ = {};
    function_indices_.clear();
    errors_.clear();

    for (const auto& statement : program.statements) {
        if (statement->kind != NodeKind::Function) continue;
        const auto& function = static_cast<const FunctionStatement&>(*statement);
        function_indices_[function.name] = chunk_.functions.size();
        chunk_.functions.push_back({0, static_cast<std::uint32_t>(function.parameters.size()),
            static_cast<std::uint32_t>(function.frame_size)});
    }

    for (const auto& statement : program.statements) {
        if (statement->kind != NodeKind::Function) compile_statement(*statement);
    }
    emit(OpCode::Halt);

    for (const auto& statement : program.statements) {
        if (statement->kind != NodeKind::Function) continue;
        const auto& function = static_cast<const FunctionStatement&>(*statement);
        chunk_.functions[function_indices_[function.name]].address = chunk_.code.size();
        for (const auto& child : function.body) compile_statement(*child);
        emit(OpCode::Constant, add_constant(std::string()));
        emit(OpCode::Return);
    }

    return chunk_;
}

const std::vector<std::string>& BytecodeCompiler::errors() const {
    return errors_;
}

std::size_t BytecodeCompiler::add_constant(BytecodeValue value) {
    chunk_.constants.push_back(std::move(value));
    return chunk_.constants.size() - 1;
}

void BytecodeCompiler::emit(OpCode opcode, std::size_t operand) {
    chunk_.code.push_back({opcode, operand});
}

std::size_t BytecodeCompiler::emit_jump(OpCode opcode) {
    const std::size_t index = chunk_.code.size();
    emit(opcode, 0);
    return index;
}

void BytecodeCompiler::patch_jump(std::size_t instruction, std::size_t target) {
    chunk_.code[instruction].operand = target;
}

void BytecodeCompiler::report_error(const std::string& message) {
    errors_.push_back(message);
}

void BytecodeCompiler::compile_statement(const Statement& statement) {
    switch (statement.kind) {
    case NodeKind::Let: {
        const auto& let = static_cast<const LetStatement&>(statement);
        compile_expression(*let.initializer);
        if (let.slot >= 0) {
            emit(OpCode::StoreLocal, static_cast<std::size_t>(let.slot));
        } else {
            emit(OpCode::Store, add_constant(let.name));
        }
        return;
    }
    case NodeKind::Assignment: {
        const auto& assignment = static_cast<const AssignmentStatement&>(statement);
        compile_expression(*assignment.value);
        if (assignment.slot >= 0) {
            emit(OpCode::StoreLocal, static_cast<std::size_t>(assignment.slot));
        } else {
            emit(OpCode::Store, add_constant(assignment.name));
        }
        return;
    }
    case NodeKind::ExpressionStatement:
        compile_expression(*static_cast<const ExpressionStatement&>(statement).expression);
        emit(OpCode::Pop);
        return;
    case NodeKind::If: {
        const auto& conditional = static_cast<const IfStatement&>(statement);
        compile_expression(*conditional.condition);
        const std::size_t false_jump = emit_jump(OpCode::JumpIfFalse);
        emit(OpCode::Pop);
        for (const auto& child : conditional.then_branch) compile_statement(*child);
        const std::size_t end_jump = emit_jump(OpCode::Jump);
        patch_jump(false_jump, chunk_.code.size());
        emit(OpCode::Pop);
        for (const auto& child : conditional.else_branch) compile_statement(*child);
        patch_jump(end_jump, chunk_.code.size());
        return;
    }
    case NodeKind::While: {
        const auto& loop = static_cast<const WhileStatement&>(statement);
        const std::size_t start = chunk_.code.size();
        compile_expression(*loop.condition);
        const std::size_t end_jump = emit_jump(OpCode::JumpIfFalse);
        emit(OpCode::Pop);
        for (const auto& child : loop.body) compile_statement(*child);
        emit(OpCode::Jump, start);
        patch_jump(end_jump, chunk_.code.size());
        emit(OpCode::Pop);
        return;
    }
    case NodeKind::For: {
        const auto& loop = static_cast<const ForStatement&>(statement);
        if (loop.initializer != nullptr) compile_statement(*loop.initializer);
        const std::size_t start = chunk_.code.size();
        std::size_t end_jump = 0;
        const bool has_condition = loop.condition != nullptr;
        if (has_condition) {
            compile_expression(*loop.condition);
            end_jump = emit_jump(OpCode::JumpIfFalse);
            emit(OpCode::Pop);
        }
        for (const auto& child : loop.body) compile_statement(*child);
        if (loop.step != nullptr) compile_statement(*loop.step);
        emit(OpCode::Jump, start);
        if (has_condition) {
            patch_jump(end_jump, chunk_.code.size());
            emit(OpCode::Pop);
        }
        return;
    }
    case NodeKind::Return: {
        const auto& return_statement = static_cast<const ReturnStatement&>(statement);
        compile_expression(*return_statement.value);
        emit(OpCode::Return);
        return;
    }
    case NodeKind::Function:
        return;
    default:
        report_error("bytecode compiler does not support this statement yet");
        return;
    }
}

void BytecodeCompiler::compile_expression(const Expression& expression) {
    switch (expression.kind) {
    case NodeKind::Boolean:
        emit(OpCode::Constant, add_constant(static_cast<const BooleanExpression&>(expression).value));
        return;
    case NodeKind::Integer:
        emit(OpCode::Constant, add_constant(static_cast<const IntegerExpression&>(expression).value));
        return;
    case NodeKind::Float:
        emit(OpCode::Constant, add_constant(static_cast<const FloatExpression&>(expression).value));
        return;
    case NodeKind::String:
        emit(OpCode::Constant, add_constant(static_cast<const StringExpression&>(expression).value));
        return;
    case NodeKind::Array: {
        const auto& array = static_cast<const ArrayExpression&>(expression);
        for (const auto& element : array.elements) compile_expression(*element);
        emit(OpCode::MakeArray, array.elements.size());
        return;
    }
    case NodeKind::Map: {
        const auto& map = static_cast<const MapExpression&>(expression);
        for (const auto& entry : map.entries) {
            compile_expression(*entry.first);
            compile_expression(*entry.second);
        }
        emit(OpCode::MakeMap, map.entries.size());
        return;
    }
    case NodeKind::Index: {
        const auto& index = static_cast<const IndexExpression&>(expression);
        compile_expression(*index.target);
        compile_expression(*index.index);
        emit(OpCode::Index);
        return;
    }
    case NodeKind::Identifier: {
        const auto& identifier = static_cast<const IdentifierExpression&>(expression);
        if (identifier.slot >= 0) {
            emit(OpCode::LoadLocal, static_cast<std::size_t>(identifier.slot));
        } else {
            emit(OpCode::Load, add_constant(identifier.name));
        }
        return;
    }
    case NodeKind::Unary: {
        const auto& unary = static_cast<const UnaryExpression&>(expression);
        compile_expression(*unary.operand);
        emit(unary.operator_type == UnaryOperator::Negate ? OpCode::Negate : OpCode::Not);
        return;
    }
    case NodeKind::Binary: {
        const auto& binary = static_cast<const BinaryExpression&>(expression);
        compile_expression(*binary.left);
        compile_expression(*binary.right);
        switch (binary.operator_type) {
        case BinaryOperator::Add: emit(OpCode::Add); break;
        case BinaryOperator::Subtract: emit(OpCode::Subtract); break;
        case BinaryOperator::Multiply: emit(OpCode::Multiply); break;
        case BinaryOperator::Divide: emit(OpCode::Divide); break;
        case BinaryOperator::Modulo: emit(OpCode::Modulo); break;
        case BinaryOperator::Equal: emit(OpCode::Equal); break;
        case BinaryOperator::NotEqual: emit(OpCode::NotEqual); break;
        case BinaryOperator::Less: emit(OpCode::Less); break;
        case BinaryOperator::LessEqual: emit(OpCode::LessEqual); break;
        case BinaryOperator::Greater: emit(OpCode::Greater); break;
        case BinaryOperator::GreaterEqual: emit(OpCode::GreaterEqual); break;
        case BinaryOperator::And: emit(OpCode::And); break;
        case BinaryOperator::Or: emit(OpCode::Or); break;
        }
        return;
    }
    case NodeKind::Call: {
        const auto& call = static_cast<const CallExpression&>(expression);
        if (call.callee->kind != NodeKind::Identifier) {
            report_error("bytecode compiler supports only named calls");
            return;
        }
        const auto& name = static_cast<const IdentifierExpression&>(*call.callee).name;
        if (const auto found = function_indices_.find(name); found != function_indices_.end()) {
            for (const auto& argument : call.arguments) compile_expression(*argument);
            emit(OpCode::Call, found->second);
            return;
        }
        if (name == "print") {
            for (const auto& argument : call.arguments) compile_expression(*argument);
            emit(OpCode::Print, call.arguments.size());
            emit(OpCode::Constant, add_constant(std::string()));
            return;
        }
        report_error("bytecode compiler does not support the call to '" + name + "' yet");
        return;
    }
    default:
        report_error("bytecode compiler does not support this expression yet");
        return;
    }
}

BytecodeVm::BytecodeVm(std::ostream& output) : output_(output) {}

bool BytecodeVm::is_truthy(const BytecodeValue& value) const {
    const auto* boolean = std::get_if<bool>(&value);
    return boolean != nullptr && *boolean;
}

bool BytecodeVm::run(const Chunk& chunk) {
    stack_.clear();
    stack_.reserve(8192);
    frames_.clear();
    base_ = 0;
    variables_.clear();
    errors_.clear();
    for (std::size_t instruction_pointer = 0; instruction_pointer < chunk.code.size(); ++instruction_pointer) {
        const Instruction instruction = chunk.code[instruction_pointer];
        switch (instruction.opcode) {
        case OpCode::Constant:
            stack_.push_back(chunk.constants[instruction.operand]);
            break;
        case OpCode::LoadLocal: {
            BytecodeValue local = stack_[base_ + instruction.operand];
            stack_.push_back(std::move(local));
            break;
        }
        case OpCode::StoreLocal:
            if (stack_.empty()) { report_error("stack underflow on store"); return false; }
            stack_[base_ + instruction.operand] = std::move(stack_.back());
            stack_.pop_back();
            break;
        case OpCode::Call: {
            if (instruction.operand >= chunk.functions.size()) {
                report_error("invalid call target");
                return false;
            }
            const FunctionInfo& function = chunk.functions[instruction.operand];
            if (stack_.size() < function.arity) { report_error("stack underflow on call"); return false; }
            const std::size_t new_base = stack_.size() - function.arity;
            stack_.resize(new_base + function.frame_size);
            frames_.push_back({instruction_pointer, base_});
            base_ = new_base;
            instruction_pointer = function.address - 1;
            break;
        }
        case OpCode::Return: {
            if (frames_.empty()) { report_error("return outside function"); return false; }
            if (stack_.empty()) { report_error("stack underflow on return"); return false; }
            BytecodeValue result = std::move(stack_.back());
            stack_.pop_back();
            stack_.resize(base_);
            stack_.push_back(std::move(result));
            instruction_pointer = frames_.back().return_ip;
            base_ = frames_.back().base;
            frames_.pop_back();
            break;
        }
        case OpCode::Load: {
            const auto& name = std::get<std::string>(chunk.constants[instruction.operand]);
            const auto variable = variables_.find(name);
            if (variable == variables_.end()) {
                report_error("unknown variable: " + name);
                return false;
            }
            stack_.push_back(variable->second);
            break;
        }
        case OpCode::Store: {
            if (stack_.empty()) { report_error("stack underflow on store"); return false; }
            const auto& name = std::get<std::string>(chunk.constants[instruction.operand]);
            variables_[name] = stack_.back();
            stack_.pop_back();
            break;
        }
        case OpCode::Print:
            if (stack_.size() < instruction.operand) { report_error("stack underflow on print"); return false; }
            {
                std::vector<std::string> values;
                for (std::size_t index = 0; index < instruction.operand; ++index) {
                    values.push_back(bytecode_value_to_string(stack_.back()));
                    stack_.pop_back();
                }
                for (std::size_t index = 0; index < values.size(); ++index) {
                    if (index > 0) output_ << ' ';
                    output_ << values[values.size() - index - 1];
                }
                output_ << '\n';
            }
            break;
        case OpCode::Pop:
            if (stack_.empty()) { report_error("stack underflow on pop"); return false; }
            stack_.pop_back();
            break;
        case OpCode::Jump:
            instruction_pointer = instruction.operand - 1;
            break;
        case OpCode::JumpIfFalse:
            if (stack_.empty()) { report_error("stack underflow on conditional jump"); return false; }
            if (!is_truthy(stack_.back())) { instruction_pointer = instruction.operand - 1; }
            break;
        case OpCode::MakeArray: {
            if (stack_.size() < instruction.operand) { report_error("stack underflow on array construction"); return false; }
            auto array = std::make_shared<BytecodeArray>();
            array->elements.resize(instruction.operand);
            for (std::size_t index = instruction.operand; index > 0; --index) {
                array->elements[index - 1] = stack_.back(); stack_.pop_back();
            }
            stack_.push_back(std::move(array));
            break;
        }
        case OpCode::MakeMap: {
            if (stack_.size() < instruction.operand * 2) { report_error("stack underflow on map construction"); return false; }
            auto map = std::make_shared<BytecodeMap>();
            for (std::size_t index = 0; index < instruction.operand; ++index) {
                BytecodeValue value = stack_.back(); stack_.pop_back();
                BytecodeValue key = stack_.back(); stack_.pop_back();
                const auto* string_key = std::get_if<std::string>(&key);
                if (string_key == nullptr) { report_error("map keys must be strings"); return false; }
                map->entries[*string_key] = std::move(value);
            }
            stack_.push_back(std::move(map));
            break;
        }
        case OpCode::Index: {
            if (stack_.size() < 2) { report_error("stack underflow on index"); return false; }
            BytecodeValue index = stack_.back(); stack_.pop_back();
            BytecodeValue target = stack_.back(); stack_.pop_back();
            if (const auto* array = std::get_if<std::shared_ptr<BytecodeArray>>(&target)) {
                const auto* integer = std::get_if<std::int64_t>(&index);
                if (!integer || *integer < 0 || static_cast<std::size_t>(*integer) >= (*array)->elements.size()) { report_error("array index out of bounds"); return false; }
                stack_.push_back((*array)->elements[static_cast<std::size_t>(*integer)]);
            } else if (const auto* map = std::get_if<std::shared_ptr<BytecodeMap>>(&target)) {
                const auto* key = std::get_if<std::string>(&index);
                if (!key || !(*map)->entries.contains(*key)) { report_error("map key not found"); return false; }
                stack_.push_back((*map)->entries.at(*key));
            } else { report_error("index target must be an array or map"); return false; }
            break;
        }
        case OpCode::Negate:
        case OpCode::Not: {
            if (stack_.empty()) { report_error("stack underflow on unary operation"); return false; }
            BytecodeValue value = stack_.back();
            stack_.pop_back();
            if (instruction.opcode == OpCode::Not) {
                const auto* boolean = std::get_if<bool>(&value);
                if (boolean == nullptr) { report_error("unary '!' requires a boolean value"); return false; }
                stack_.push_back(!*boolean);
            } else if (const auto* integer = std::get_if<std::int64_t>(&value)) {
                stack_.push_back(-*integer);
            } else if (const auto* floating_point = std::get_if<double>(&value)) {
                stack_.push_back(-*floating_point);
            } else {
                report_error("unary '-' requires a numeric value"); return false;
            }
            break;
        }
        case OpCode::Add:
        case OpCode::Subtract:
        case OpCode::Multiply:
        case OpCode::Divide:
        case OpCode::Modulo:
        case OpCode::Equal:
        case OpCode::NotEqual:
        case OpCode::Less:
        case OpCode::LessEqual:
        case OpCode::Greater:
        case OpCode::GreaterEqual:
        case OpCode::And:
        case OpCode::Or:
            if (!binary_operation(instruction.opcode)) return false;
            break;
        case OpCode::Halt:
            return true;
        }
    }
    return true;
}

bool BytecodeVm::binary_operation(OpCode opcode) {
    if (stack_.size() < 2) { report_error("stack underflow on binary operation"); return false; }
    BytecodeValue right = stack_.back(); stack_.pop_back();
    BytecodeValue left = stack_.back(); stack_.pop_back();
    if (opcode == OpCode::And || opcode == OpCode::Or) {
        const auto* left_boolean = std::get_if<bool>(&left);
        const auto* right_boolean = std::get_if<bool>(&right);
        if (!left_boolean || !right_boolean) { report_error("logical operators require boolean values"); return false; }
        stack_.push_back(opcode == OpCode::And ? *left_boolean && *right_boolean : *left_boolean || *right_boolean);
        return true;
    }
    if ((opcode == OpCode::Add) && std::holds_alternative<std::string>(left) && std::holds_alternative<std::string>(right)) {
        stack_.push_back(std::get<std::string>(left) + std::get<std::string>(right));
        return true;
    }
    if (opcode == OpCode::Equal || opcode == OpCode::NotEqual) {
        bool equal = false;
        if (is_numeric(left) && is_numeric(right)) equal = as_number(left) == as_number(right);
        else if (left.index() == right.index()) equal = left == right;
        stack_.push_back(opcode == OpCode::Equal ? equal : !equal);
        return true;
    }
    if (!is_numeric(left) || !is_numeric(right)) { report_error("operation requires numeric values"); return false; }
    const double left_number = as_number(left), right_number = as_number(right);
    if ((opcode == OpCode::Divide || opcode == OpCode::Modulo) && right_number == 0.0) {
        report_error(opcode == OpCode::Modulo ? "cannot take remainder by zero" : "cannot divide by zero");
        return false;
    }
    if (opcode == OpCode::Less || opcode == OpCode::LessEqual || opcode == OpCode::Greater || opcode == OpCode::GreaterEqual) {
        bool result = opcode == OpCode::Less ? left_number < right_number : opcode == OpCode::LessEqual ? left_number <= right_number : opcode == OpCode::Greater ? left_number > right_number : left_number >= right_number;
        stack_.push_back(result);
        return true;
    }
    if (std::holds_alternative<std::int64_t>(left) && std::holds_alternative<std::int64_t>(right) && opcode != OpCode::Divide) {
        const auto a = std::get<std::int64_t>(left), b = std::get<std::int64_t>(right);
        stack_.push_back(opcode == OpCode::Add ? a + b : opcode == OpCode::Subtract ? a - b :
            opcode == OpCode::Modulo ? a % b : a * b);
    } else {
        stack_.push_back(opcode == OpCode::Add ? left_number + right_number : opcode == OpCode::Subtract ? left_number - right_number :
            opcode == OpCode::Multiply ? left_number * right_number : opcode == OpCode::Modulo ? std::fmod(left_number, right_number) : left_number / right_number);
    }
    return true;
}

const std::vector<std::string>& BytecodeVm::errors() const { return errors_; }
void BytecodeVm::report_error(const std::string& message) { errors_.push_back(message); }

std::string bytecode_value_to_string(const BytecodeValue& value) {
    if (const auto* boolean = std::get_if<bool>(&value)) return *boolean ? "true" : "false";
    if (const auto* integer = std::get_if<std::int64_t>(&value)) return std::to_string(*integer);
    if (const auto* floating_point = std::get_if<double>(&value)) { std::ostringstream output; output << std::setprecision(15) << *floating_point; return output.str(); }
    if (const auto* array = std::get_if<std::shared_ptr<BytecodeArray>>(&value)) {
        std::ostringstream output; output << '[';
        for (std::size_t index = 0; index < (*array)->elements.size(); ++index) { if (index > 0) output << ", "; output << bytecode_value_to_string((*array)->elements[index]); }
        output << ']'; return output.str();
    }
    if (const auto* map = std::get_if<std::shared_ptr<BytecodeMap>>(&value)) {
        std::ostringstream output; output << '{'; std::vector<std::string> keys;
        for (const auto& entry : (*map)->entries) keys.push_back(entry.first);
        std::sort(keys.begin(), keys.end());
        for (std::size_t index = 0; index < keys.size(); ++index) { if (index > 0) output << ", "; output << '"' << keys[index] << "\": " << bytecode_value_to_string((*map)->entries.at(keys[index])); }
        output << '}'; return output.str();
    }
    return std::get<std::string>(value);
}

namespace {

constexpr std::uint64_t kMaxStringBytes = 1024ull * 1024ull * 64ull;
constexpr std::uint64_t kMaxConstants = 1'000'000ull;
constexpr std::uint64_t kMaxInstructions = 10'000'000ull;

template <typename T>
void write_binary(std::ostream& output, const T& value) { output.write(reinterpret_cast<const char*>(&value), sizeof(T)); }

template <typename T>
bool read_binary(std::istream& input, T& value) { return static_cast<bool>(input.read(reinterpret_cast<char*>(&value), sizeof(T))); }

void write_string(std::ostream& output, const std::string& value) {
    const std::uint64_t size = value.size();
    write_binary(output, size);
    output.write(value.data(), static_cast<std::streamsize>(size));
}

bool read_string(std::istream& input, std::string& value) {
    std::uint64_t size = 0;
    if (!read_binary(input, size) || size > kMaxStringBytes) return false;
    value.resize(static_cast<std::size_t>(size));
    return static_cast<bool>(input.read(value.data(), static_cast<std::streamsize>(size)));
}

bool write_value(std::ostream& output, const BytecodeValue& value) {
    const std::uint8_t type = static_cast<std::uint8_t>(value.index());
    write_binary(output, type);
    if (const auto* boolean = std::get_if<bool>(&value)) write_binary(output, *boolean);
    else if (const auto* integer = std::get_if<std::int64_t>(&value)) write_binary(output, *integer);
    else if (const auto* floating_point = std::get_if<double>(&value)) write_binary(output, *floating_point);
    else if (const auto* string = std::get_if<std::string>(&value)) write_string(output, *string);
    else return false;
    return static_cast<bool>(output);
}

bool read_value(std::istream& input, BytecodeValue& value) {
    std::uint8_t type = 0;
    if (!read_binary(input, type)) return false;
    if (type == 0) { bool item = false; if (!read_binary(input, item)) return false; value = item; }
    else if (type == 1) { std::int64_t item = 0; if (!read_binary(input, item)) return false; value = item; }
    else if (type == 2) { double item = 0.0; if (!read_binary(input, item)) return false; value = item; }
    else if (type == 3) { std::string item; if (!read_string(input, item)) return false; value = std::move(item); }
    else return false;
    return true;
}

bool operand_is_constant_index(OpCode opcode) {
    return opcode == OpCode::Constant || opcode == OpCode::Load || opcode == OpCode::Store;
}

bool operand_is_jump_target(OpCode opcode) {
    return opcode == OpCode::Jump || opcode == OpCode::JumpIfFalse;
}

bool operand_is_count(OpCode opcode) {
    return opcode == OpCode::Print || opcode == OpCode::MakeArray || opcode == OpCode::MakeMap;
}

bool operand_is_slot(OpCode opcode) {
    return opcode == OpCode::LoadLocal || opcode == OpCode::StoreLocal;
}

constexpr std::uint64_t kMaxFunctions = 100'000ull;
constexpr std::uint64_t kMaxFrameSlots = 100'000ull;

} // namespace

bool validate_chunk(const Chunk& chunk, std::string& error) {
    for (const auto& function : chunk.functions) {
        if (function.address >= chunk.code.size()) {
            error = "function address out of range";
            return false;
        }
        if (function.frame_size > kMaxFrameSlots || function.arity > function.frame_size) {
            error = "invalid function frame";
            return false;
        }
    }
    for (std::size_t index = 0; index < chunk.code.size(); ++index) {
        const Instruction instruction = chunk.code[index];
        if (static_cast<std::uint8_t>(instruction.opcode) > static_cast<std::uint8_t>(OpCode::Halt)) {
            error = "unknown opcode at instruction " + std::to_string(index);
            return false;
        }
        if (operand_is_constant_index(instruction.opcode)) {
            if (instruction.operand >= chunk.constants.size()) {
                error = "constant index out of range at instruction " + std::to_string(index);
                return false;
            }
            if ((instruction.opcode == OpCode::Load || instruction.opcode == OpCode::Store) &&
                !std::holds_alternative<std::string>(chunk.constants[instruction.operand])) {
                error = "variable name constant is not a string at instruction " + std::to_string(index);
                return false;
            }
        } else if (operand_is_jump_target(instruction.opcode)) {
            if (instruction.operand > chunk.code.size()) {
                error = "jump target out of range at instruction " + std::to_string(index);
                return false;
            }
        } else if (instruction.opcode == OpCode::Call) {
            if (instruction.operand >= chunk.functions.size()) {
                error = "call target out of range at instruction " + std::to_string(index);
                return false;
            }
        } else if (operand_is_slot(instruction.opcode)) {
            if (instruction.operand >= kMaxFrameSlots) {
                error = "frame slot out of range at instruction " + std::to_string(index);
                return false;
            }
        } else if (!operand_is_count(instruction.opcode) && instruction.operand != 0) {
            error = "unexpected non-zero operand at instruction " + std::to_string(index);
            return false;
        }
    }
    return true;
}

bool save_bytecode(const Chunk& chunk, const std::string& path, std::string& error) {
    std::string validation_error;
    if (!validate_chunk(chunk, validation_error)) {
        error = "refusing to serialize invalid chunk: " + validation_error;
        return false;
    }
    std::ofstream output(path, std::ios::binary);
    if (!output) { error = "could not create bytecode file: " + path; return false; }
    output.write(kBytecodeMagic, sizeof(kBytecodeMagic));
    write_binary(output, kBytecodeFormatVersion);
    const std::uint64_t constants = chunk.constants.size();
    const std::uint64_t instructions = chunk.code.size();
    write_binary(output, constants);
    for (const auto& value : chunk.constants) {
        if (!write_value(output, value)) {
            error = "could not write bytecode constants (unsupported constant type)";
            return false;
        }
    }
    write_binary(output, instructions);
    for (const auto& instruction : chunk.code) {
        const std::uint8_t opcode = static_cast<std::uint8_t>(instruction.opcode);
        const std::uint64_t operand = instruction.operand;
        write_binary(output, opcode);
        write_binary(output, operand);
    }

    const std::uint64_t functions = chunk.functions.size();
    write_binary(output, functions);
    for (const auto& function : chunk.functions) {
        write_binary(output, static_cast<std::uint64_t>(function.address));
        write_binary(output, function.arity);
        write_binary(output, function.frame_size);
    }

    if (!output) { error = "could not write bytecode file: " + path; return false; }
    return true;
}

bool load_bytecode(const std::string& path, Chunk& chunk, std::string& error) {
    std::ifstream input(path, std::ios::binary);
    if (!input) { error = "could not open bytecode file: " + path; return false; }

    char magic[sizeof(kBytecodeMagic)]{};
    if (!input.read(magic, sizeof(magic)) ||
        std::string(magic, sizeof(magic)) != std::string(kBytecodeMagic, sizeof(kBytecodeMagic))) {
        error = "not a Kite bytecode file: " + path;
        return false;
    }
    std::uint32_t version = 0;
    if (!read_binary(input, version)) { error = "truncated bytecode header"; return false; }
    if (version != kBytecodeFormatVersion) {
        error = "unsupported bytecode format version " + std::to_string(version) + " (expected " +
            std::to_string(kBytecodeFormatVersion) + ")";
        return false;
    }

    std::uint64_t constants = 0;
    if (!read_binary(input, constants) || constants > kMaxConstants) {
        error = "invalid bytecode constant table";
        return false;
    }
    chunk = {};
    chunk.constants.resize(static_cast<std::size_t>(constants));
    for (auto& value : chunk.constants) {
        if (!read_value(input, value)) { error = "invalid bytecode constant"; return false; }
    }

    std::uint64_t instructions = 0;
    if (!read_binary(input, instructions) || instructions > kMaxInstructions) {
        error = "invalid bytecode instruction stream";
        return false;
    }
    chunk.code.resize(static_cast<std::size_t>(instructions));
    for (auto& instruction : chunk.code) {
        std::uint8_t opcode = 0;
        std::uint64_t operand = 0;
        if (!read_binary(input, opcode) || !read_binary(input, operand)) {
            error = "truncated bytecode instruction";
            return false;
        }
        instruction = {static_cast<OpCode>(opcode), static_cast<std::size_t>(operand)};
    }

    std::uint64_t functions = 0;
    if (!read_binary(input, functions) || functions > kMaxFunctions) {
        error = "invalid bytecode function table";
        return false;
    }
    chunk.functions.resize(static_cast<std::size_t>(functions));
    for (auto& function : chunk.functions) {
        std::uint64_t address = 0;
        if (!read_binary(input, address) || !read_binary(input, function.arity) ||
            !read_binary(input, function.frame_size)) {
            error = "truncated bytecode function table";
            return false;
        }
        function.address = static_cast<std::size_t>(address);
    }

    input.peek();
    if (!input.eof()) { error = "trailing data after bytecode function table"; return false; }

    return validate_chunk(chunk, error);
}

} // namespace kite
