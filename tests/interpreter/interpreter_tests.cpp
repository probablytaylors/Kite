#include <iostream>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "kite/interpreter/interpreter.hpp"
#include "kite/parser/parser.hpp"

namespace {

bool check(bool condition, const char* name) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
    return condition;
}

bool run_source(const std::string& source, const std::string& expected_output) {
    kite::Parser parser{kite::Lexer(source)};
    const kite::Program program = parser.parse_program();
    if (!parser.errors().empty()) {
        return false;
    }

    std::ostringstream output;
    kite::Interpreter interpreter(output);
    return interpreter.execute(program) && interpreter.errors().empty() && output.str() == expected_output;
}

bool runs_successfully(const std::string& source) {
    kite::Parser parser{kite::Lexer(source)};
    const kite::Program program = parser.parse_program();
    std::ostringstream output;
    kite::Interpreter interpreter(output);
    return parser.errors().empty() && interpreter.execute(program) && interpreter.errors().empty();
}

} // namespace

int main() {
    std::cout << "## Kite interpreter tests\n";
    bool all_passed = true;

    all_passed &= check(run_source("let count = 42\nprint(count)", "42\n"), "integer variable and print");
    all_passed &= check(run_source("let message = \"Kite\"\nprint(message)", "Kite\n"),
        "string variable and print");
    all_passed &= check(run_source("print(1, \"two\")", "1 two\n"), "multiple print arguments");
    all_passed &= check(run_source("print(2 + 3 * 4)", "14\n"), "operator precedence");
    all_passed &= check(run_source("print((2 + 3) * 4)", "20\n"), "parenthesized arithmetic");
    all_passed &= check(run_source("print(5 / 2)", "2.5\n"), "floating-point division");
    all_passed &= check(run_source("print(2.5 * 4)", "10\n"), "mixed arithmetic");
    all_passed &= check(run_source("print(-4)", "-4\n"), "unary minus");
    all_passed &= check(run_source("if (true) { print(1) } else { print(2) }", "1\n"),
        "true conditional branch");
    all_passed &= check(run_source("if (false) { print(1) } else { print(2) }", "2\n"),
        "false conditional branch");
    all_passed &= check(run_source("if (2 < 3) { print(\"yes\") }", "yes\n"),
        "comparison conditional");
    all_passed &= check(run_source("if (2 == 2) { print(true) }", "true\n"),
        "boolean output");
    all_passed &= check(run_source("let count = 0\nwhile (count < 3) { print(count) let count = count + 1 }",
        "0\n1\n2\n"), "while loop");
    all_passed &= check(run_source("fn add(a, b) { return a + b } print(add(2, 3))", "5\n"),
        "function parameters and return");
    all_passed &= check(run_source("fn square(value) { return value * value } print(square(6))", "36\n"),
        "function call");
    all_passed &= check(run_source("fn fact(value) { if (value <= 1) { return 1 } return value * fact(value - 1) } print(fact(5))",
        "120\n"), "recursive function");
    all_passed &= check(run_source("let value = 10 fn read() { let value = 20 return value } print(read()) print(value)",
        "20\n10\n"), "function local scope");
    all_passed &= check(run_source("let values = [1, 2, 3] print(values) print(values[1])",
        "[1, 2, 3]\n2\n"), "array values and indexing");
    all_passed &= check(run_source("let nested = [[1], [2]] print(nested[1][0])", "2\n"),
        "nested array indexing");
    all_passed &= check(run_source("let value = 1 value = value + 2 print(value)", "3\n"),
        "variable assignment");
    all_passed &= check(run_source("print(\"Hello, \" + \"Kite\")", "Hello, Kite\n"),
        "string concatenation");
    all_passed &= check(run_source("let data = {\"name\": \"Kite\", \"version\": 1} print(data) print(data[\"name\"])",
        "{\"name\": Kite, \"version\": 1}\nKite\n"), "map values and lookup");
    all_passed &= check(run_source("print(len(\"Kite\")) print(len([1, 2])) print(len({\"a\": 1}))",
        "4\n2\n1\n"), "length function");
    all_passed &= check(run_source("print(upper(\"kite\")) print(lower(\"KITE\"))", "KITE\nkite\n"),
        "string case functions");
    all_passed &= check(run_source("print(!false) print(true && false) print(false || true)",
        "true\nfalse\ntrue\n"), "boolean logic");
    all_passed &= check(run_source("print(true || missing()) print(false && missing())", "true\nfalse\n"),
        "short-circuit logic");

    {
        kite::Parser parser{kite::Lexer("print(missing)")};
        const kite::Program program = parser.parse_program();
        std::ostringstream output;
        kite::Interpreter interpreter(output);
        all_passed &= check(!interpreter.execute(program) && !interpreter.errors().empty(), "unknown variable error");
    }

    {
        kite::Parser parser{kite::Lexer("other(1)")};
        const kite::Program program = parser.parse_program();
        std::ostringstream output;
        kite::Interpreter interpreter(output);
        all_passed &= check(!interpreter.execute(program) && !interpreter.errors().empty(), "unknown function error");
    }

    {
        kite::Parser parser{kite::Lexer("print(1 / 0)")};
        const kite::Program program = parser.parse_program();
        std::ostringstream output;
        kite::Interpreter interpreter(output);
        all_passed &= check(!interpreter.execute(program) && !interpreter.errors().empty(), "division by zero error");
    }

    {
        kite::Parser parser{kite::Lexer("print(true + 1)")};
        const kite::Program program = parser.parse_program();
        std::ostringstream output;
        kite::Interpreter interpreter(output);
        all_passed &= check(!interpreter.execute(program) && !interpreter.errors().empty(),
            "invalid arithmetic types");
    }

    {
        kite::Parser parser{kite::Lexer("if (1) { print(1) }")};
        const kite::Program program = parser.parse_program();
        std::ostringstream output;
        kite::Interpreter interpreter(output);
        all_passed &= check(!interpreter.execute(program) && !interpreter.errors().empty(),
            "non-boolean condition error");
    }

    {
        kite::Parser parser{kite::Lexer("while (1) { print(1) }")};
        const kite::Program program = parser.parse_program();
        std::ostringstream output;
        kite::Interpreter interpreter(output);
        all_passed &= check(!interpreter.execute(program) && !interpreter.errors().empty(),
            "non-boolean while condition error");
    }

    {
        kite::Parser parser{kite::Lexer("fn add(a) { return a } print(add())")};
        const kite::Program program = parser.parse_program();
        std::ostringstream output;
        kite::Interpreter interpreter(output);
        all_passed &= check(!interpreter.execute(program) && !interpreter.errors().empty(),
            "function arity error");
    }

    {
        kite::Parser parser{kite::Lexer("let values = [1] print(values[2])")};
        const kite::Program program = parser.parse_program();
        std::ostringstream output;
        kite::Interpreter interpreter(output);
        all_passed &= check(!interpreter.execute(program) && !interpreter.errors().empty(),
            "array bounds error");
    }

    all_passed &= check(run_source("print(sqrt(25)) print(pow(2, 8)) print(abs(-4))", "5\n256\n4\n"),
        "basic math functions");
    all_passed &= check(runs_successfully("print(sin(0)) print(cos(0)) print(tan(0))"),
        "trigonometric functions");
    all_passed &= check(run_source("print(floor(3.9)) print(ceil(3.1))", "3\n4\n"),
        "rounding functions");
    all_passed &= check(!runs_successfully("print(sqrt(\"nope\"))"), "math type error");
    all_passed &= check(!runs_successfully("print(sqrt(-1))"), "math domain error");
    all_passed &= check(!runs_successfully("missing = 1"), "assignment unknown variable error");
    all_passed &= check(!runs_successfully("print(1 && true)"), "logical type error");

    {
        const std::filesystem::path path = std::filesystem::temp_directory_path() / "kite_runtime_test.txt";
        const std::string source = "write_file(\"" + path.string() + "\", \"Kite file\") print(read_file(\"" +
            path.string() + "\"))";
        all_passed &= check(run_source(source, "Kite file\n"), "file read and write");
        std::filesystem::remove(path);
    }

    if (all_passed) {
        std::cout << "\nAll interpreter tests passed.\n";
        return 0;
    }

    std::cout << "\nInterpreter tests failed.\n";
    return 1;
}