#include <iostream>
#include <memory>
#include <string>

#include "kite/ast/ast.hpp"
#include "kite/parser/parser.hpp"

namespace {

bool check(bool condition, const char* name) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
    return condition;
}

kite::Parser parser_for(const std::string& source) {
    return kite::Parser(kite::Lexer(source));
}

bool parses_without_errors(const std::string& source, std::size_t statement_count) {
    auto parser = parser_for(source);
    const kite::Program program = parser.parse_program();
    return parser.errors().empty() && program.statements.size() == statement_count;
}

} // namespace

int main() {
    std::cout << "## Kite parser tests\n";
    bool all_passed = true;

    all_passed &= check(parses_without_errors("", 0), "empty program");

    {
        auto parser = parser_for("let count = 42");
        const kite::Program program = parser.parse_program();
        const auto* statement = dynamic_cast<const kite::LetStatement*>(program.statements[0].get());
        const auto* value = statement == nullptr
            ? nullptr
            : dynamic_cast<const kite::IntegerExpression*>(statement->initializer.get());
        all_passed &= check(parser.errors().empty() && statement != nullptr && statement->name == "count" &&
            value != nullptr && value->value == 42, "integer let declaration");
    }

    {
        auto parser = parser_for("let message = \"Kite\"");
        const kite::Program program = parser.parse_program();
        const auto* statement = dynamic_cast<const kite::LetStatement*>(program.statements[0].get());
        const auto* value = statement == nullptr
            ? nullptr
            : dynamic_cast<const kite::StringExpression*>(statement->initializer.get());
        all_passed &= check(parser.errors().empty() && value != nullptr && value->value == "Kite",
            "string let declaration");
    }

    {
        auto parser = parser_for("name");
        const kite::Program program = parser.parse_program();
        const auto* statement = dynamic_cast<const kite::ExpressionStatement*>(program.statements[0].get());
        const auto* expression = statement == nullptr
            ? nullptr
            : dynamic_cast<const kite::IdentifierExpression*>(statement->expression.get());
        all_passed &= check(parser.errors().empty() && expression != nullptr && expression->name == "name",
            "identifier expression");
    }

    {
        auto parser = parser_for("print(message, count)");
        const kite::Program program = parser.parse_program();
        const auto* statement = dynamic_cast<const kite::ExpressionStatement*>(program.statements[0].get());
        const auto* call = statement == nullptr
            ? nullptr
            : dynamic_cast<const kite::CallExpression*>(statement->expression.get());
        all_passed &= check(parser.errors().empty() && call != nullptr && call->arguments.size() == 2,
            "function call with multiple arguments");
    }

    all_passed &= check(parses_without_errors("let x = 1\nlet y = 2\nprint(x)", 3), "multiple statements");
    all_passed &= check(parses_without_errors("print()", 1), "empty call");
    all_passed &= check(parses_without_errors("if (true) { print(1) } else { print(2) }", 1),
        "if and else statement");
    all_passed &= check(parses_without_errors("while (true) { print(1) }", 1),
        "while statement");
    all_passed &= check(parses_without_errors("fn add(a, b) { return a + b }", 1),
        "function declaration");
    all_passed &= check(parses_without_errors("let values = [1, 2, 3] print(values[1])", 2),
        "array literal and indexing");
    all_passed &= check(parses_without_errors("let value = 1 value = 2 print(value)", 3),
        "assignment statement");
    all_passed &= check(parses_without_errors("if (!false && true || false) { print(1) }", 1),
        "logical expression");

    {
        auto parser = parser_for("let = 42");
        parser.parse_program();
        all_passed &= check(!parser.errors().empty() && parser.errors()[0].find("identifier") != std::string::npos,
            "missing let identifier");
    }

    {
        auto parser = parser_for("let value 42");
        parser.parse_program();
        all_passed &= check(!parser.errors().empty() && parser.errors()[0].find("'='") != std::string::npos,
            "missing let equals sign");
    }

    {
        auto parser = parser_for("print(");
        parser.parse_program();
        all_passed &= check(!parser.errors().empty() && parser.errors()[0].find("close call") != std::string::npos,
            "unterminated call");
    }

    {
        auto parser = parser_for("let value = )");
        parser.parse_program();
        all_passed &= check(!parser.errors().empty(), "unexpected token");
    }

    {
        auto parser = parser_for("let value = 1");
        parser.parse_program();
        all_passed &= check(parser.errors().empty(), "EOF handling");
    }

    if (all_passed) {
        std::cout << "\nAll parser tests passed.\n";
        return 0;
    }

    std::cout << "\nParser tests failed.\n";
    return 1;
}