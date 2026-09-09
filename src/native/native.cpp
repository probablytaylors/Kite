#include "kite/native/native.hpp"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace kite {

namespace {

enum class Type { Error, Void, Int, Float, Bool, String };

const char* c_type(Type type) {
    switch (type) {
    case Type::Int: return "int64_t";
    case Type::Float: return "double";
    case Type::Bool: return "int";
    case Type::String: return "const char*";
    case Type::Void: return "void";
    case Type::Error: return "void";
    }
    return "void";
}

Type type_from_name(const std::string& name) {
    if (name == "int") return Type::Int;
    if (name == "float") return Type::Float;
    if (name == "bool") return Type::Bool;
    if (name == "string") return Type::String;
    if (name.empty()) return Type::Void;
    return Type::Error;
}

struct Signature {
    std::vector<Type> parameters;
    Type result = Type::Void;
};

class Compiler {
public:
    bool compile(const Program& program, std::string& out, std::string& error) {
        collect_functions(program);
        for (const auto& statement : program.statements) {
            if (statement->kind == NodeKind::Function) {
                check_function(static_cast<const FunctionStatement&>(*statement));
            }
        }
        scope_.clear();
        current_result_ = Type::Void;
        for (const auto& statement : program.statements) {
            if (statement->kind != NodeKind::Function) check_statement(*statement, nullptr);
        }
        if (!errors_.empty()) {
            error = errors_.front();
            return false;
        }

        std::ostringstream source;
        source << "#include <stdint.h>\n#include <stdio.h>\n\n";
        for (const auto& statement : program.statements) {
            if (statement->kind != NodeKind::Function) continue;
            const auto& function = static_cast<const FunctionStatement&>(*statement);
            source << "static " << c_type(signatures_[function.name].result) << " kf_"
                   << function.name << "(";
            emit_parameters(source, function);
            source << ");\n";
        }
        source << '\n';
        for (const auto& statement : program.statements) {
            if (statement->kind != NodeKind::Function) continue;
            emit_function(source, static_cast<const FunctionStatement&>(*statement));
        }
        source << "int main(void) {\n";
        for (const auto& statement : program.statements) {
            if (statement->kind != NodeKind::Function) emit_statement(source, *statement, 1);
        }
        source << "    return 0;\n}\n";

        out = source.str();
        return true;
    }

private:
    std::unordered_map<std::string, Signature> signatures_;
    std::unordered_map<const Expression*, Type> types_;
    std::unordered_map<std::string, Type> scope_;
    std::vector<std::string> errors_;
    Type current_result_ = Type::Void;
    int loop_depth_ = 0;

    void fail(const std::string& message) { errors_.push_back(message); }

    void collect_functions(const Program& program) {
        for (const auto& statement : program.statements) {
            if (statement->kind != NodeKind::Function) continue;
            const auto& function = static_cast<const FunctionStatement&>(*statement);
            Signature signature;
            for (const auto& name : function.parameter_types) {
                const Type type = type_from_name(name);
                if (type == Type::Error || type == Type::Void) {
                    fail("function '" + function.name + "' needs a type on every parameter");
                }
                signature.parameters.push_back(type);
            }
            signature.result = type_from_name(function.return_type);
            if (signature.result == Type::Error) {
                fail("unknown return type '" + function.return_type + "' on '" + function.name + "'");
            }
            signatures_[function.name] = signature;
        }
    }

    void check_function(const FunctionStatement& function) {
        scope_.clear();
        loop_depth_ = 0;
        const Signature& signature = signatures_[function.name];
        for (std::size_t index = 0; index < function.parameters.size(); ++index) {
            scope_[function.parameters[index]] = signature.parameters[index];
        }
        current_result_ = signature.result;
        for (const auto& statement : function.body) check_statement(*statement, &function);
    }

    void check_statement(const Statement& statement, const FunctionStatement* function) {
        switch (statement.kind) {
        case NodeKind::Let: {
            const auto& let = static_cast<const LetStatement&>(statement);
            const Type value = check_expression(*let.initializer);
            Type type = value;
            if (!let.declared_type.empty()) {
                type = type_from_name(let.declared_type);
                if (type == Type::Error) fail("unknown type '" + let.declared_type + "'");
                else if (value != type && !(type == Type::Float && value == Type::Int)) {
                    fail("'" + let.name + "' declared " + let.declared_type + " but assigned a different type");
                }
            }
            if (type == Type::Void || type == Type::Error) {
                fail("cannot infer a type for '" + let.name + "'");
                type = Type::Int;
            }
            scope_[let.name] = type;
            return;
        }
        case NodeKind::Assignment: {
            const auto& assignment = static_cast<const AssignmentStatement&>(statement);
            const Type value = check_expression(*assignment.value);
            const auto existing = scope_.find(assignment.name);
            if (existing == scope_.end()) {
                fail("assignment to undeclared variable '" + assignment.name + "'");
                return;
            }
            if (value != existing->second && !(existing->second == Type::Float && value == Type::Int)) {
                fail("assignment to '" + assignment.name + "' changes its type");
            }
            return;
        }
        case NodeKind::ExpressionStatement:
            check_expression(*static_cast<const ExpressionStatement&>(statement).expression);
            return;
        case NodeKind::If: {
            const auto& conditional = static_cast<const IfStatement&>(statement);
            if (check_expression(*conditional.condition) != Type::Bool) fail("if condition must be bool");
            for (const auto& child : conditional.then_branch) check_statement(*child, function);
            for (const auto& child : conditional.else_branch) check_statement(*child, function);
            return;
        }
        case NodeKind::While: {
            const auto& loop = static_cast<const WhileStatement&>(statement);
            if (check_expression(*loop.condition) != Type::Bool) fail("while condition must be bool");
            ++loop_depth_;
            for (const auto& child : loop.body) check_statement(*child, function);
            --loop_depth_;
            return;
        }
        case NodeKind::For: {
            const auto& loop = static_cast<const ForStatement&>(statement);
            if (loop.initializer != nullptr) check_statement(*loop.initializer, function);
            if (loop.condition != nullptr && check_expression(*loop.condition) != Type::Bool) {
                fail("for condition must be bool");
            }
            ++loop_depth_;
            if (loop.step != nullptr) check_statement(*loop.step, function);
            for (const auto& child : loop.body) check_statement(*child, function);
            --loop_depth_;
            return;
        }
        case NodeKind::Break:
            if (loop_depth_ == 0) fail("break outside loop");
            return;
        case NodeKind::Continue:
            if (loop_depth_ == 0) fail("continue outside loop");
            return;
        case NodeKind::Return: {
            const auto& return_statement = static_cast<const ReturnStatement&>(statement);
            const Type value = return_statement.value ? check_expression(*return_statement.value) : Type::Void;
            if (value != current_result_ && !(current_result_ == Type::Float && value == Type::Int)) {
                fail("return type does not match the function signature");
            }
            return;
        }
        case NodeKind::Function:
            return;
        default:
            fail("unsupported statement in native mode");
            return;
        }
    }

    Type binary_result(BinaryOperator op, Type left, Type right) {
        if (op == BinaryOperator::And || op == BinaryOperator::Or) {
            if (left != Type::Bool || right != Type::Bool) fail("logical operators need bool operands");
            return Type::Bool;
        }
        if (op >= BinaryOperator::Equal) {
            if (left != right && !((left == Type::Int && right == Type::Float) ||
                                   (left == Type::Float && right == Type::Int))) {
                fail("comparison needs matching operand types");
            }
            return Type::Bool;
        }
        if (left != Type::Int && left != Type::Float) { fail("arithmetic needs numeric operands"); return Type::Error; }
        if (right != Type::Int && right != Type::Float) { fail("arithmetic needs numeric operands"); return Type::Error; }
        return (left == Type::Float || right == Type::Float) ? Type::Float : Type::Int;
    }

    Type check_expression(const Expression& expression) {
        Type type = Type::Error;
        switch (expression.kind) {
        case NodeKind::Boolean: type = Type::Bool; break;
        case NodeKind::Integer: type = Type::Int; break;
        case NodeKind::Float: type = Type::Float; break;
        case NodeKind::String: type = Type::String; break;
        case NodeKind::Identifier: {
            const auto& identifier = static_cast<const IdentifierExpression&>(expression);
            const auto found = scope_.find(identifier.name);
            if (found == scope_.end()) { fail("unknown variable '" + identifier.name + "'"); break; }
            type = found->second;
            break;
        }
        case NodeKind::Unary: {
            const auto& unary = static_cast<const UnaryExpression&>(expression);
            const Type operand = check_expression(*unary.operand);
            if (unary.operator_type == UnaryOperator::Not) {
                if (operand != Type::Bool) fail("'!' needs a bool operand");
                type = Type::Bool;
            } else {
                if (operand != Type::Int && operand != Type::Float) fail("unary '-' needs a number");
                type = operand;
            }
            break;
        }
        case NodeKind::Binary: {
            const auto& binary = static_cast<const BinaryExpression&>(expression);
            type = binary_result(binary.operator_type,
                check_expression(*binary.left), check_expression(*binary.right));
            break;
        }
        case NodeKind::Call: {
            const auto& call = static_cast<const CallExpression&>(expression);
            if (call.callee->kind != NodeKind::Identifier) { fail("calls must name a function"); break; }
            const std::string& name = static_cast<const IdentifierExpression&>(*call.callee).name;
            if (name == "print") {
                for (const auto& argument : call.arguments) check_expression(*argument);
                type = Type::Void;
                break;
            }
            const auto signature = signatures_.find(name);
            if (signature == signatures_.end()) { fail("unknown function '" + name + "'"); break; }
            if (call.arguments.size() != signature->second.parameters.size()) {
                fail("wrong number of arguments to '" + name + "'");
            }
            for (std::size_t index = 0; index < call.arguments.size(); ++index) {
                const Type argument = check_expression(*call.arguments[index]);
                if (index < signature->second.parameters.size()) {
                    const Type expected = signature->second.parameters[index];
                    if (argument != expected && !(expected == Type::Float && argument == Type::Int)) {
                        fail("argument " + std::to_string(index + 1) + " to '" + name + "' has the wrong type");
                    }
                }
            }
            type = signature->second.result;
            break;
        }
        default:
            fail("unsupported expression in native mode");
            break;
        }
        types_[&expression] = type;
        return type;
    }

    void emit_parameters(std::ostream& out, const FunctionStatement& function) {
        if (function.parameters.empty()) {
            out << "void";
            return;
        }
        const Signature& signature = signatures_[function.name];
        for (std::size_t index = 0; index < function.parameters.size(); ++index) {
            if (index > 0) out << ", ";
            out << c_type(signature.parameters[index]) << ' ' << function.parameters[index];
        }
    }

    void emit_function(std::ostream& out, const FunctionStatement& function) {
        out << "static " << c_type(signatures_[function.name].result) << " kf_" << function.name << "(";
        emit_parameters(out, function);
        out << ") {\n";
        for (const auto& statement : function.body) emit_statement(out, *statement, 1);
        if (signatures_[function.name].result != Type::Void) out << "    return 0;\n";
        out << "}\n\n";
    }

    void indent(std::ostream& out, int depth) {
        for (int index = 0; index < depth; ++index) out << "    ";
    }

    void emit_block(std::ostream& out, const std::vector<std::unique_ptr<Statement>>& body, int depth) {
        out << "{\n";
        for (const auto& statement : body) emit_statement(out, *statement, depth + 1);
        indent(out, depth);
        out << "}";
    }

    void emit_for_clause(std::ostream& out, const Statement& statement) {
        switch (statement.kind) {
        case NodeKind::Let: {
            const auto& let = static_cast<const LetStatement&>(statement);
            out << c_type(scope_lookup_for_emit(let)) << ' ' << let.name << " = ";
            emit_expression(out, *let.initializer);
            return;
        }
        case NodeKind::Assignment: {
            const auto& assignment = static_cast<const AssignmentStatement&>(statement);
            out << assignment.name << " = ";
            emit_expression(out, *assignment.value);
            return;
        }
        case NodeKind::ExpressionStatement:
            emit_expression(out, *static_cast<const ExpressionStatement&>(statement).expression);
            return;
        default:
            return;
        }
    }

    void emit_statement(std::ostream& out, const Statement& statement, int depth) {
        switch (statement.kind) {
        case NodeKind::Let: {
            const auto& let = static_cast<const LetStatement&>(statement);
            indent(out, depth);
            out << c_type(scope_lookup_for_emit(let)) << ' ' << let.name << " = ";
            emit_expression(out, *let.initializer);
            out << ";\n";
            return;
        }
        case NodeKind::Assignment: {
            const auto& assignment = static_cast<const AssignmentStatement&>(statement);
            indent(out, depth);
            out << assignment.name << " = ";
            emit_expression(out, *assignment.value);
            out << ";\n";
            return;
        }
        case NodeKind::ExpressionStatement: {
            indent(out, depth);
            emit_expression(out, *static_cast<const ExpressionStatement&>(statement).expression);
            out << ";\n";
            return;
        }
        case NodeKind::If: {
            const auto& conditional = static_cast<const IfStatement&>(statement);
            indent(out, depth);
            out << "if (";
            emit_expression(out, *conditional.condition);
            out << ") ";
            emit_block(out, conditional.then_branch, depth);
            if (!conditional.else_branch.empty()) {
                out << " else ";
                emit_block(out, conditional.else_branch, depth);
            }
            out << '\n';
            return;
        }
        case NodeKind::While: {
            const auto& loop = static_cast<const WhileStatement&>(statement);
            indent(out, depth);
            out << "while (";
            emit_expression(out, *loop.condition);
            out << ") ";
            emit_block(out, loop.body, depth);
            out << '\n';
            return;
        }
        case NodeKind::For: {
            const auto& loop = static_cast<const ForStatement&>(statement);
            indent(out, depth);
            out << "for (";
            if (loop.initializer != nullptr) emit_for_clause(out, *loop.initializer);
            out << "; ";
            if (loop.condition != nullptr) emit_expression(out, *loop.condition);
            out << "; ";
            if (loop.step != nullptr) emit_for_clause(out, *loop.step);
            out << ") ";
            emit_block(out, loop.body, depth);
            out << '\n';
            return;
        }
        case NodeKind::Break:
            indent(out, depth);
            out << "break;\n";
            return;
        case NodeKind::Continue:
            indent(out, depth);
            out << "continue;\n";
            return;
        case NodeKind::Return: {
            const auto& return_statement = static_cast<const ReturnStatement&>(statement);
            indent(out, depth);
            if (return_statement.value == nullptr) {
                out << "return;\n";
            } else {
                out << "return ";
                emit_expression(out, *return_statement.value);
                out << ";\n";
            }
            return;
        }
        case NodeKind::Function:
            return;
        default:
            return;
        }
    }

    Type scope_lookup_for_emit(const LetStatement& let) {
        Type type = types_.count(let.initializer.get()) ? types_[let.initializer.get()] : Type::Int;
        if (!let.declared_type.empty()) type = type_from_name(let.declared_type);
        scope_[let.name] = type;
        return type;
    }

    void emit_expression(std::ostream& out, const Expression& expression) {
        switch (expression.kind) {
        case NodeKind::Boolean:
            out << (static_cast<const BooleanExpression&>(expression).value ? "1" : "0");
            return;
        case NodeKind::Integer:
            out << "INT64_C(" << static_cast<const IntegerExpression&>(expression).value << ")";
            return;
        case NodeKind::Float: {
            std::ostringstream number;
            number.precision(17);
            number << static_cast<const FloatExpression&>(expression).value;
            std::string text = number.str();
            if (text.find('.') == std::string::npos && text.find('e') == std::string::npos) text += ".0";
            out << text;
            return;
        }
        case NodeKind::String:
            emit_string_literal(out, static_cast<const StringExpression&>(expression).value);
            return;
        case NodeKind::Identifier:
            out << static_cast<const IdentifierExpression&>(expression).name;
            return;
        case NodeKind::Unary: {
            const auto& unary = static_cast<const UnaryExpression&>(expression);
            out << '(' << (unary.operator_type == UnaryOperator::Not ? "!" : "-");
            emit_expression(out, *unary.operand);
            out << ')';
            return;
        }
        case NodeKind::Binary: {
            const auto& binary = static_cast<const BinaryExpression&>(expression);
            out << '(';
            emit_expression(out, *binary.left);
            out << ' ' << binary_operator(binary.operator_type) << ' ';
            emit_expression(out, *binary.right);
            out << ')';
            return;
        }
        case NodeKind::Call: {
            const auto& call = static_cast<const CallExpression&>(expression);
            const std::string& name = static_cast<const IdentifierExpression&>(*call.callee).name;
            if (name == "print") {
                emit_print(out, call);
                return;
            }
            out << "kf_" << name << '(';
            for (std::size_t index = 0; index < call.arguments.size(); ++index) {
                if (index > 0) out << ", ";
                emit_expression(out, *call.arguments[index]);
            }
            out << ')';
            return;
        }
        default:
            return;
        }
    }

    void emit_print(std::ostream& out, const CallExpression& call) {
        std::string format;
        std::ostringstream arguments;
        for (std::size_t index = 0; index < call.arguments.size(); ++index) {
            if (index > 0) format += ' ';
            const Type type = types_.count(call.arguments[index].get())
                ? types_[call.arguments[index].get()] : Type::Int;
            if (type == Type::Bool) {
                format += "%s";
                arguments << ", ((";
                emit_expression(arguments, *call.arguments[index]);
                arguments << ") ? \"true\" : \"false\")";
            } else if (type == Type::Float) {
                format += "%.15g";
                arguments << ", (double)(";
                emit_expression(arguments, *call.arguments[index]);
                arguments << ")";
            } else if (type == Type::String) {
                format += "%s";
                arguments << ", (";
                emit_expression(arguments, *call.arguments[index]);
                arguments << ")";
            } else {
                format += "%lld";
                arguments << ", (long long)(";
                emit_expression(arguments, *call.arguments[index]);
                arguments << ")";
            }
        }
        format += "\\n";
        out << "printf(\"" << format << "\"" << arguments.str() << ")";
    }

    static void emit_string_literal(std::ostream& out, const std::string& value) {
        out << '"';
        for (char character : value) {
            switch (character) {
            case '"': out << "\\\""; break;
            case '\\': out << "\\\\"; break;
            case '\n': out << "\\n"; break;
            case '\t': out << "\\t"; break;
            case '\r': out << "\\r"; break;
            default: out << character; break;
            }
        }
        out << '"';
    }

    static const char* binary_operator(BinaryOperator op) {
        switch (op) {
        case BinaryOperator::Add: return "+";
        case BinaryOperator::Subtract: return "-";
        case BinaryOperator::Multiply: return "*";
        case BinaryOperator::Divide: return "/";
        case BinaryOperator::Modulo: return "%";
        case BinaryOperator::Equal: return "==";
        case BinaryOperator::NotEqual: return "!=";
        case BinaryOperator::Less: return "<";
        case BinaryOperator::LessEqual: return "<=";
        case BinaryOperator::Greater: return ">";
        case BinaryOperator::GreaterEqual: return ">=";
        case BinaryOperator::And: return "&&";
        case BinaryOperator::Or: return "||";
        }
        return "+";
    }
};

} // namespace

bool emit_c(const Program& program, std::string& c_source, std::string& error) {
    Compiler compiler;
    return compiler.compile(program, c_source, error);
}

bool compile_native(const Program& program, const std::string& output_path, std::string& error) {
    std::string c_source;
    if (!emit_c(program, c_source, error)) {
        return false;
    }

    const std::string c_path = output_path + ".c";
    {
        std::ofstream file(c_path, std::ios::binary);
        if (!file) { error = "could not write " + c_path; return false; }
        file << c_source;
    }

#if defined(_WIN32)
    std::error_code path_error;
    const std::filesystem::path work_dir = std::filesystem::current_path(path_error);
    const std::string script_path = output_path + ".build.bat";
    {
        std::ofstream script(script_path, std::ios::binary);
        script << "@echo off\r\n";
        if (!path_error) script << "cd /d \"" << work_dir.string() << "\"\r\n";
        script << "set \"VSWHERE=%ProgramFiles(x86)%\\Microsoft Visual Studio\\Installer\\vswhere.exe\"\r\n"
               << "for /f \"usebackq tokens=*\" %%i in (`\"%VSWHERE%\" -latest -property installationPath`) "
                  "do set \"VSPATH=%%i\"\r\n"
               << "if not defined VSPATH ( echo missing-visual-studio & exit /b 2 )\r\n"
               << "call \"%VSPATH%\\VC\\Auxiliary\\Build\\vcvars64.bat\" >nul 2>&1\r\n"
               << "cl /nologo /O2 /GL /Fe:\"" << output_path << "\" \"" << c_path << "\" "
                  "/Fo:\"" << c_path << ".obj\" /link /LTCG >\"" << c_path << ".log\" 2>&1\r\n";
    }
    std::error_code script_error;
    const std::filesystem::path script_full = std::filesystem::absolute(script_path, script_error);
    const std::string script_command = script_error ? script_path : script_full.string();
    const int status = std::system(("\"" + script_command + "\"").c_str());
    std::remove(script_path.c_str());
    std::remove((c_path + ".obj").c_str());
    if (status == 2) {
        error = "Visual Studio C++ build tools not found (install \"Desktop development with C++\")";
        std::remove(c_path.c_str());
        std::remove((c_path + ".log").c_str());
        return false;
    }
    if (status != 0) {
        error = "the C compiler rejected the generated code; see " + c_path + " and " + c_path + ".log";
        return false;
    }
    std::remove((c_path + ".log").c_str());
#else
    if (std::system(("cc -O2 -o \"" + output_path + "\" \"" + c_path + "\" 2>\"" + c_path + ".log\"").c_str()) != 0) {
        error = "the C compiler rejected the generated code; see " + c_path;
        return false;
    }
    std::remove((c_path + ".log").c_str());
#endif
    std::remove(c_path.c_str());
    return true;
}

} // namespace kite
