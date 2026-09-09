#include "kite/bytecode/bytecode.hpp"

#include <cmath>
#include <fstream>
#include <limits>
#include <utility>
#include <vector>

#include "kite/builtins.hpp"

namespace kite {

Chunk BytecodeCompiler::compile(const Program& program) {
    chunk_ = {};
    function_indices_.clear();
    loops_.clear();
    handler_depth_ = 0;
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
        handler_depth_ = 0;
        for (const auto& child : function.body) compile_statement(*child);
        emit(OpCode::Constant, add_constant(Value::string("")));
        emit(OpCode::Return);
    }

    return chunk_;
}

const std::vector<std::string>& BytecodeCompiler::errors() const {
    return errors_;
}

std::size_t BytecodeCompiler::add_constant(Value value) {
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
            emit(OpCode::Store, add_constant(Value::string(let.name)));
        }
        return;
    }
    case NodeKind::Assignment: {
        const auto& assignment = static_cast<const AssignmentStatement&>(statement);
        if (assignment.slot >= 0 && assignment.value->kind == NodeKind::Binary) {
            const auto& binary = static_cast<const BinaryExpression&>(*assignment.value);
            const bool step_by_one = binary.left->kind == NodeKind::Identifier &&
                static_cast<const IdentifierExpression&>(*binary.left).slot == assignment.slot &&
                binary.right->kind == NodeKind::Integer &&
                static_cast<const IntegerExpression&>(*binary.right).value == 1;
            if (step_by_one && binary.operator_type == BinaryOperator::Add) {
                emit(OpCode::IncLocal, static_cast<std::size_t>(assignment.slot));
                return;
            }
            if (step_by_one && binary.operator_type == BinaryOperator::Subtract) {
                emit(OpCode::DecLocal, static_cast<std::size_t>(assignment.slot));
                return;
            }
        }
        compile_expression(*assignment.value);
        if (assignment.slot >= 0) {
            emit(OpCode::StoreLocal, static_cast<std::size_t>(assignment.slot));
        } else {
            emit(OpCode::Store, add_constant(Value::string(assignment.name)));
        }
        return;
    }
    case NodeKind::IndexAssignment: {
        const auto& assignment = static_cast<const IndexAssignmentStatement&>(statement);
        compile_expression(*assignment.target);
        compile_expression(*assignment.index);
        compile_expression(*assignment.value);
        emit(OpCode::SetIndex);
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
        for (const auto& child : conditional.then_branch) compile_statement(*child);
        const std::size_t end_jump = emit_jump(OpCode::Jump);
        patch_jump(false_jump, chunk_.code.size());
        for (const auto& child : conditional.else_branch) compile_statement(*child);
        patch_jump(end_jump, chunk_.code.size());
        return;
    }
    case NodeKind::While: {
        const auto& loop = static_cast<const WhileStatement&>(statement);
        const std::size_t start = chunk_.code.size();
        loops_.push_back({{}, {}, handler_depth_});
        compile_expression(*loop.condition);
        const std::size_t end_jump = emit_jump(OpCode::JumpIfFalse);
        for (const auto& child : loop.body) compile_statement(*child);
        emit(OpCode::Jump, start);
        patch_jump(end_jump, chunk_.code.size());
        for (const std::size_t jump : loops_.back().continue_jumps) patch_jump(jump, start);
        for (const std::size_t jump : loops_.back().break_jumps) patch_jump(jump, chunk_.code.size());
        loops_.pop_back();
        return;
    }
    case NodeKind::For: {
        const auto& loop = static_cast<const ForStatement&>(statement);
        if (loop.initializer != nullptr) compile_statement(*loop.initializer);
        const std::size_t start = chunk_.code.size();
        loops_.push_back({{}, {}, handler_depth_});
        std::size_t end_jump = 0;
        const bool has_condition = loop.condition != nullptr;
        if (has_condition) {
            compile_expression(*loop.condition);
            end_jump = emit_jump(OpCode::JumpIfFalse);
        }
        for (const auto& child : loop.body) compile_statement(*child);
        const std::size_t step = chunk_.code.size();
        if (loop.step != nullptr) compile_statement(*loop.step);
        emit(OpCode::Jump, start);
        if (has_condition) {
            patch_jump(end_jump, chunk_.code.size());
        }
        for (const std::size_t jump : loops_.back().continue_jumps) patch_jump(jump, step);
        for (const std::size_t jump : loops_.back().break_jumps) patch_jump(jump, chunk_.code.size());
        loops_.pop_back();
        return;
    }
    case NodeKind::Return: {
        const auto& return_statement = static_cast<const ReturnStatement&>(statement);
        const Expression& value = *return_statement.value;
        const bool return_local = value.kind == NodeKind::Identifier &&
            static_cast<const IdentifierExpression&>(value).slot >= 0;
        const bool return_const = value.kind == NodeKind::Integer;
        if (!return_local && !return_const) compile_expression(value);
        for (int depth = 0; depth < handler_depth_; ++depth) emit(OpCode::PopHandler);
        if (return_local) {
            emit(OpCode::ReturnLocal,
                static_cast<std::size_t>(static_cast<const IdentifierExpression&>(value).slot));
        } else if (return_const) {
            emit(OpCode::ReturnConst, add_constant(static_cast<const IntegerExpression&>(value).value));
        } else {
            emit(OpCode::Return);
        }
        return;
    }
    case NodeKind::Break: {
        if (loops_.empty()) {
            report_error("break outside loop");
            return;
        }
        for (int depth = loops_.back().handler_depth; depth < handler_depth_; ++depth) {
            emit(OpCode::PopHandler);
        }
        loops_.back().break_jumps.push_back(emit_jump(OpCode::Jump));
        return;
    }
    case NodeKind::Continue: {
        if (loops_.empty()) {
            report_error("continue outside loop");
            return;
        }
        for (int depth = loops_.back().handler_depth; depth < handler_depth_; ++depth) {
            emit(OpCode::PopHandler);
        }
        loops_.back().continue_jumps.push_back(emit_jump(OpCode::Jump));
        return;
    }
    case NodeKind::Try: {
        const auto& node = static_cast<const TryStatement&>(statement);
        const std::size_t handler_jump = emit_jump(OpCode::PushHandler);
        ++handler_depth_;
        for (const auto& child : node.try_branch) compile_statement(*child);
        --handler_depth_;
        emit(OpCode::PopHandler);
        const std::size_t end_jump = emit_jump(OpCode::Jump);
        patch_jump(handler_jump, chunk_.code.size());
        if (node.slot >= 0) {
            emit(OpCode::StoreLocal, static_cast<std::size_t>(node.slot));
        } else {
            emit(OpCode::Store, add_constant(Value::string(node.name)));
        }
        for (const auto& child : node.catch_branch) compile_statement(*child);
        patch_jump(end_jump, chunk_.code.size());
        return;
    }
    case NodeKind::Throw: {
        compile_expression(*static_cast<const ThrowStatement&>(statement).value);
        emit(OpCode::Throw);
        return;
    }
    case NodeKind::Function:
    case NodeKind::Struct:
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
        emit(OpCode::Constant, add_constant(Value::string(static_cast<const StringExpression&>(expression).value)));
        return;
    case NodeKind::Array: {
        const auto& array = static_cast<const ArrayExpression&>(expression);
        for (const auto& element : array.elements) compile_expression(*element);
        emit(OpCode::MakeArray, array.elements.size());
        return;
    }
    case NodeKind::Map: {
        const auto& map = static_cast<const MapExpression&>(expression);
        if (!map.type_name.empty()) {
            emit(OpCode::Constant, add_constant(Value::string(map.type_name)));
        }
        for (const auto& entry : map.entries) {
            compile_expression(*entry.first);
            compile_expression(*entry.second);
        }
        emit(map.type_name.empty() ? OpCode::MakeMap : OpCode::MakeStruct, map.entries.size());
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
            emit(OpCode::Load, add_constant(Value::string(identifier.name)));
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
        if (binary.operator_type == BinaryOperator::And || binary.operator_type == BinaryOperator::Or) {
            compile_expression(*binary.left);
            emit(OpCode::Dup);
            const std::size_t short_circuit = emit_jump(
                binary.operator_type == BinaryOperator::And ? OpCode::JumpIfFalse : OpCode::JumpIfTrue);
            emit(OpCode::Pop);
            compile_expression(*binary.right);
            patch_jump(short_circuit, chunk_.code.size());
            return;
        }
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
            emit(OpCode::Constant, add_constant(Value::string("")));
            return;
        }
        emit(OpCode::Constant, add_constant(Value::string(name)));
        for (const auto& argument : call.arguments) compile_expression(*argument);
        emit(OpCode::CallNative, call.arguments.size());
        return;
    }
    default:
        report_error("bytecode compiler does not support this expression yet");
        return;
    }
}

BytecodeVm::BytecodeVm(std::ostream& output) : output_(output) {}

bool BytecodeVm::run(const Chunk& chunk) {
    stack_.clear();
    stack_.reserve(8192);
    frames_.clear();
    handlers_.clear();
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
            if (base_ + instruction.operand >= stack_.size()) {
                report_error("local slot out of range"); goto fault;
            }
            Value local = stack_[base_ + instruction.operand];
            stack_.push_back(std::move(local));
            break;
        }
        case OpCode::StoreLocal:
            if (stack_.empty()) { report_error("stack underflow on store"); goto fault; }
            if (base_ + instruction.operand >= stack_.size()) {
                report_error("local slot out of range"); goto fault;
            }
            stack_[base_ + instruction.operand] = std::move(stack_.back());
            stack_.pop_back();
            break;
        case OpCode::IncLocal:
        case OpCode::DecLocal: {
            if (base_ + instruction.operand >= stack_.size()) {
                report_error("local slot out of range"); goto fault;
            }
            Value& slot = stack_[base_ + instruction.operand];
            const std::int64_t delta = instruction.opcode == OpCode::IncLocal ? 1 : -1;
            if (slot.is_int()) slot = slot.as_int() + delta;
            else if (slot.is_float()) slot = slot.as_float() + static_cast<double>(delta);
            else { report_error("operation requires numeric values"); goto fault; }
            break;
        }
        case OpCode::Call: {
            if (instruction.operand >= chunk.functions.size()) {
                report_error("invalid call target");
                goto fault;
            }
            if (frames_.size() >= 100000) { report_error("maximum call depth exceeded"); goto fault; }
            const FunctionInfo& function = chunk.functions[instruction.operand];
            if (stack_.size() < function.arity) { report_error("stack underflow on call"); goto fault; }
            const std::size_t new_base = stack_.size() - function.arity;
            stack_.resize(new_base + function.frame_size);
            frames_.push_back({instruction_pointer, base_});
            base_ = new_base;
            instruction_pointer = function.address - 1;
            break;
        }
        case OpCode::CallNative: {
            const std::size_t count = instruction.operand;
            if (count >= stack_.size()) { report_error("stack underflow on builtin call"); goto fault; }
            std::vector<Value> arguments;
            arguments.reserve(count);
            const std::size_t first = stack_.size() - count;
            for (std::size_t index = 0; index < count; ++index) {
                arguments.push_back(std::move(stack_[first + index]));
            }
            stack_.resize(first);
            Value name = std::move(stack_.back());
            stack_.pop_back();
            const BuiltinFn builtin = find_builtin(name.as_string());
            if (builtin == nullptr) {
                report_error("unknown function: " + name.as_string());
                goto fault;
            }
            BuiltinContext context{output_, {}};
            Value result;
            if (!builtin(context, arguments, result)) {
                report_error(context.error);
                goto fault;
            }
            stack_.push_back(std::move(result));
            break;
        }
        case OpCode::Return:
        case OpCode::ReturnLocal:
        case OpCode::ReturnConst: {
            if (frames_.empty()) { report_error("return outside function"); goto fault; }
            Value result;
            if (instruction.opcode == OpCode::ReturnLocal) {
                if (base_ + instruction.operand >= stack_.size()) {
                    report_error("local slot out of range"); goto fault;
                }
                result = stack_[base_ + instruction.operand];
            } else if (instruction.opcode == OpCode::ReturnConst) {
                result = chunk.constants[instruction.operand];
            } else {
                if (stack_.empty()) { report_error("stack underflow on return"); goto fault; }
                result = std::move(stack_.back());
            }
            stack_.resize(base_);
            stack_.push_back(std::move(result));
            instruction_pointer = frames_.back().return_ip;
            base_ = frames_.back().base;
            frames_.pop_back();
            break;
        }
        case OpCode::PushHandler:
            handlers_.push_back({instruction.operand, stack_.size(), frames_.size()});
            break;
        case OpCode::PopHandler:
            if (!handlers_.empty()) handlers_.pop_back();
            break;
        case OpCode::Throw: {
            if (stack_.empty()) { report_error("stack underflow on throw"); goto fault; }
            Value value = std::move(stack_.back());
            stack_.pop_back();
            report_error(value.is_string() ? value.as_string() : to_string(value));
            goto fault;
        }
        case OpCode::Load: {
            const auto& name = chunk.constants[instruction.operand].as_string();
            const auto variable = variables_.find(name);
            if (variable == variables_.end()) {
                report_error("unknown variable: " + name);
                goto fault;
            }
            stack_.push_back(variable->second);
            break;
        }
        case OpCode::Store: {
            if (stack_.empty()) { report_error("stack underflow on store"); goto fault; }
            variables_[chunk.constants[instruction.operand].as_string()] = stack_.back();
            stack_.pop_back();
            break;
        }
        case OpCode::Print:
            if (stack_.size() < instruction.operand) { report_error("stack underflow on print"); goto fault; }
            {
                const std::size_t first = stack_.size() - instruction.operand;
                for (std::size_t index = 0; index < instruction.operand; ++index) {
                    if (index > 0) output_ << ' ';
                    output_ << to_string(stack_[first + index]);
                }
                output_ << '\n';
                stack_.resize(first);
            }
            break;
        case OpCode::Pop:
            if (stack_.empty()) { report_error("stack underflow on pop"); goto fault; }
            stack_.pop_back();
            break;
        case OpCode::Dup: {
            if (stack_.empty()) { report_error("stack underflow on dup"); goto fault; }
            Value top = stack_.back();
            stack_.push_back(std::move(top));
            break;
        }
        case OpCode::Jump:
            instruction_pointer = instruction.operand - 1;
            break;
        case OpCode::JumpIfFalse: {
            if (stack_.empty()) { report_error("stack underflow on conditional jump"); goto fault; }
            const bool take_jump = !stack_.back().is_truthy();
            stack_.pop_back();
            if (take_jump) instruction_pointer = instruction.operand - 1;
            break;
        }
        case OpCode::JumpIfTrue: {
            if (stack_.empty()) { report_error("stack underflow on conditional jump"); goto fault; }
            const bool take_jump = stack_.back().is_truthy();
            stack_.pop_back();
            if (take_jump) instruction_pointer = instruction.operand - 1;
            break;
        }
        case OpCode::MakeArray: {
            if (stack_.size() < instruction.operand) { report_error("stack underflow on array construction"); goto fault; }
            Value array = Value::array();
            auto& elements = array.as_array();
            const std::size_t first = stack_.size() - instruction.operand;
            for (std::size_t index = 0; index < instruction.operand; ++index) {
                elements.push_back(std::move(stack_[first + index]));
            }
            stack_.resize(first);
            stack_.push_back(std::move(array));
            break;
        }
        case OpCode::MakeMap: {
            if (instruction.operand > stack_.size() / 2) { report_error("stack underflow on map construction"); goto fault; }
            Value map = Value::map();
            auto& entries = map.as_map();
            for (std::size_t index = 0; index < instruction.operand; ++index) {
                Value value = std::move(stack_.back()); stack_.pop_back();
                Value key = std::move(stack_.back()); stack_.pop_back();
                if (!key.is_string()) { report_error("map keys must be strings"); goto fault; }
                entries[key.as_string()] = std::move(value);
            }
            stack_.push_back(std::move(map));
            break;
        }
        case OpCode::MakeStruct: {
            if (stack_.empty() || instruction.operand > (stack_.size() - 1) / 2) {
                report_error("stack underflow on struct construction");
                goto fault;
            }
            Value structure = Value::map();
            auto& entries = structure.as_map();
            bool bad_key = false;
            for (std::size_t index = 0; index < instruction.operand; ++index) {
                Value value = std::move(stack_.back()); stack_.pop_back();
                Value key = std::move(stack_.back()); stack_.pop_back();
                if (!key.is_string()) { bad_key = true; break; }
                entries[key.as_string()] = std::move(value);
            }
            if (bad_key || !stack_.back().is_string()) {
                report_error("malformed struct instruction"); goto fault;
            }
            structure.map_type() = std::move(stack_.back().as_string());
            stack_.pop_back();
            stack_.push_back(std::move(structure));
            break;
        }
        case OpCode::Index: {
            if (stack_.size() < 2) { report_error("stack underflow on index"); goto fault; }
            Value index = std::move(stack_.back()); stack_.pop_back();
            Value target = std::move(stack_.back()); stack_.pop_back();
            if (target.is_array()) {
                const auto& elements = target.as_array();
                if (!index.is_int() || index.as_int() < 0 ||
                    static_cast<std::size_t>(index.as_int()) >= elements.size()) {
                    report_error("array index out of bounds");
                    goto fault;
                }
                stack_.push_back(elements[static_cast<std::size_t>(index.as_int())]);
            } else if (target.is_string()) {
                const std::string& text = target.as_string();
                if (!index.is_int() || index.as_int() < 0 ||
                    static_cast<std::size_t>(index.as_int()) >= text.size()) {
                    report_error("string index out of bounds");
                    goto fault;
                }
                stack_.push_back(Value::string(std::string(1, text[static_cast<std::size_t>(index.as_int())])));
            } else if (target.is_map()) {
                if (!index.is_string()) { report_error("map index requires a string key"); goto fault; }
                const auto& entries = target.as_map();
                const auto entry = entries.find(index.as_string());
                if (entry == entries.end()) {
                    report_error("map key not found: " + index.as_string());
                    goto fault;
                }
                stack_.push_back(entry->second);
            } else {
                report_error("index target must be an array or map");
                goto fault;
            }
            break;
        }
        case OpCode::SetIndex: {
            if (stack_.size() < 3) { report_error("stack underflow on index assignment"); goto fault; }
            Value value = std::move(stack_.back()); stack_.pop_back();
            Value index = std::move(stack_.back()); stack_.pop_back();
            Value target = std::move(stack_.back()); stack_.pop_back();
            if (target.is_array()) {
                if (!index.is_int()) { report_error("array index must be an integer"); goto fault; }
                auto& elements = target.as_array();
                const std::int64_t position = index.as_int();
                if (position < 0 || static_cast<std::size_t>(position) >= elements.size()) {
                    report_error("array index out of bounds");
                    goto fault;
                }
                elements[static_cast<std::size_t>(position)] = std::move(value);
            } else if (target.is_map()) {
                if (!index.is_string()) { report_error("map key must be a string"); goto fault; }
                target.as_map()[index.as_string()] = std::move(value);
            } else {
                report_error("cannot assign to an index of this value");
                goto fault;
            }
            break;
        }
        case OpCode::Negate:
        case OpCode::Not: {
            if (stack_.empty()) { report_error("stack underflow on unary operation"); goto fault; }
            Value value = std::move(stack_.back());
            stack_.pop_back();
            if (instruction.opcode == OpCode::Not) {
                if (!value.is_bool()) { report_error("unary '!' requires a boolean value"); goto fault; }
                stack_.push_back(!value.as_bool());
            } else if (value.is_int()) {
                stack_.push_back(-value.as_int());
            } else if (value.is_float()) {
                stack_.push_back(-value.as_float());
            } else {
                report_error("unary '-' requires a numeric value"); goto fault;
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
            if (!binary_operation(instruction.opcode)) goto fault;
            break;
        case OpCode::Halt:
            return true;
        }
        continue;

    fault:
        if (handlers_.empty()) {
            return false;
        }
        const Handler handler = handlers_.back();
        handlers_.pop_back();
        while (frames_.size() > handler.frame_depth) {
            base_ = frames_.back().base;
            frames_.pop_back();
        }
        if (stack_.size() > handler.stack_depth) {
            stack_.resize(handler.stack_depth);
        }
        std::string message = errors_.empty() ? std::string("error") : std::move(errors_.back());
        errors_.clear();
        stack_.push_back(Value::string(std::move(message)));
        instruction_pointer = handler.target - 1;
    }
    return true;
}

bool BytecodeVm::binary_operation(OpCode opcode) {
    if (stack_.size() < 2) { report_error("stack underflow on binary operation"); return false; }
    Value right = std::move(stack_.back()); stack_.pop_back();
    Value left = std::move(stack_.back()); stack_.pop_back();

    if (opcode == OpCode::And || opcode == OpCode::Or) {
        if (!left.is_bool() || !right.is_bool()) {
            report_error("logical operators require boolean values");
            return false;
        }
        stack_.push_back(opcode == OpCode::And ? left.as_bool() && right.as_bool()
                                              : left.as_bool() || right.as_bool());
        return true;
    }
    if (opcode == OpCode::Add && left.is_string() && right.is_string()) {
        stack_.push_back(Value::string(left.as_string() + right.as_string()));
        return true;
    }
    if (opcode == OpCode::Equal || opcode == OpCode::NotEqual) {
        const bool equal = left.equals(right);
        stack_.push_back(opcode == OpCode::Equal ? equal : !equal);
        return true;
    }
    if (!left.is_number() || !right.is_number()) {
        report_error("operation requires numeric values");
        return false;
    }
    const double left_number = left.as_number();
    const double right_number = right.as_number();
    if ((opcode == OpCode::Divide || opcode == OpCode::Modulo) && right_number == 0.0) {
        report_error(opcode == OpCode::Modulo ? "cannot take remainder by zero" : "cannot divide by zero");
        return false;
    }
    switch (opcode) {
    case OpCode::Less: stack_.push_back(left_number < right_number); return true;
    case OpCode::LessEqual: stack_.push_back(left_number <= right_number); return true;
    case OpCode::Greater: stack_.push_back(left_number > right_number); return true;
    case OpCode::GreaterEqual: stack_.push_back(left_number >= right_number); return true;
    default: break;
    }
    if (left.is_int() && right.is_int()) {
        const std::int64_t a = left.as_int();
        const std::int64_t b = right.as_int();
        if ((opcode == OpCode::Divide || opcode == OpCode::Modulo) &&
            b == -1 && a == std::numeric_limits<std::int64_t>::min()) {
            stack_.push_back(opcode == OpCode::Modulo ? std::int64_t{0} : a);
            return true;
        }
        switch (opcode) {
        case OpCode::Add: stack_.push_back(a + b); break;
        case OpCode::Subtract: stack_.push_back(a - b); break;
        case OpCode::Multiply: stack_.push_back(a * b); break;
        case OpCode::Divide: stack_.push_back(a / b); break;
        case OpCode::Modulo: stack_.push_back(a % b); break;
        default: break;
        }
        return true;
    }
    switch (opcode) {
    case OpCode::Add: stack_.push_back(left_number + right_number); break;
    case OpCode::Subtract: stack_.push_back(left_number - right_number); break;
    case OpCode::Multiply: stack_.push_back(left_number * right_number); break;
    case OpCode::Divide: stack_.push_back(left_number / right_number); break;
    case OpCode::Modulo: stack_.push_back(std::fmod(left_number, right_number)); break;
    default: break;
    }
    return true;
}

const std::vector<std::string>& BytecodeVm::errors() const { return errors_; }
void BytecodeVm::report_error(const std::string& message) { errors_.push_back(message); }

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

bool write_value(std::ostream& output, const Value& value) {
    const auto type = static_cast<std::uint8_t>(value.type());
    write_binary(output, type);
    switch (value.type()) {
    case ValueType::Bool: { const bool item = value.as_bool(); write_binary(output, item); break; }
    case ValueType::Int: { const std::int64_t item = value.as_int(); write_binary(output, item); break; }
    case ValueType::Float: { const double item = value.as_float(); write_binary(output, item); break; }
    case ValueType::String: write_string(output, value.as_string()); break;
    default: return false;
    }
    return static_cast<bool>(output);
}

bool read_value(std::istream& input, Value& value) {
    std::uint8_t type = 0;
    if (!read_binary(input, type)) return false;
    switch (static_cast<ValueType>(type)) {
    case ValueType::Bool: {
        bool item = false;
        if (!read_binary(input, item)) return false;
        value = item;
        return true;
    }
    case ValueType::Int: {
        std::int64_t item = 0;
        if (!read_binary(input, item)) return false;
        value = item;
        return true;
    }
    case ValueType::Float: {
        double item = 0.0;
        if (!read_binary(input, item)) return false;
        value = item;
        return true;
    }
    case ValueType::String: {
        std::string item;
        if (!read_string(input, item)) return false;
        value = Value::string(std::move(item));
        return true;
    }
    default:
        return false;
    }
}

bool operand_is_constant_index(OpCode opcode) {
    return opcode == OpCode::Constant || opcode == OpCode::Load || opcode == OpCode::Store ||
        opcode == OpCode::ReturnConst;
}

bool operand_is_jump_target(OpCode opcode) {
    return opcode == OpCode::Jump || opcode == OpCode::JumpIfFalse ||
        opcode == OpCode::JumpIfTrue || opcode == OpCode::PushHandler;
}

bool operand_is_count(OpCode opcode) {
    return opcode == OpCode::Print || opcode == OpCode::MakeArray || opcode == OpCode::MakeMap ||
        opcode == OpCode::MakeStruct || opcode == OpCode::CallNative;
}

bool operand_is_slot(OpCode opcode) {
    return opcode == OpCode::LoadLocal || opcode == OpCode::StoreLocal ||
        opcode == OpCode::IncLocal || opcode == OpCode::DecLocal ||
        opcode == OpCode::ReturnLocal;
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
                !chunk.constants[instruction.operand].is_string()) {
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
        } else if (operand_is_count(instruction.opcode)) {
            if (instruction.operand > chunk.code.size()) {
                error = "operand count out of range at instruction " + std::to_string(index);
                return false;
            }
        } else if (instruction.operand != 0) {
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
    std::ifstream input(path, std::ios::binary | std::ios::ate);
    if (!input) { error = "could not open bytecode file: " + path; return false; }
    const std::streamoff file_size = input.tellg();
    input.seekg(0, std::ios::beg);
    const std::uint64_t remaining = file_size > 0 ? static_cast<std::uint64_t>(file_size) : 0;

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
    if (!read_binary(input, constants) || constants > kMaxConstants || constants * 2 > remaining) {
        error = "invalid bytecode constant table";
        return false;
    }
    chunk = {};
    chunk.constants.resize(static_cast<std::size_t>(constants));
    for (auto& value : chunk.constants) {
        if (!read_value(input, value)) { error = "invalid bytecode constant"; return false; }
    }

    std::uint64_t instructions = 0;
    if (!read_binary(input, instructions) || instructions > kMaxInstructions ||
        instructions * 9 > remaining) {
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
    if (!read_binary(input, functions) || functions > kMaxFunctions || functions * 16 > remaining) {
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
