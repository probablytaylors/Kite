#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#include "kite/interpreter/interpreter.hpp"
#include "kite/parser/parser.hpp"

#ifndef KITE_SOURCE_DIR
#error KITE_SOURCE_DIR must be defined by CMake
#endif

namespace {

bool check(bool condition, const char* name) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
    return condition;
}

std::string read_file(const std::string& relative_path) {
    std::ifstream input(std::string(KITE_SOURCE_DIR) + "/" + relative_path);
    return {std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
}

bool run_module(const std::string& module, const std::string& usage, const std::string& expected) {
    const std::string source = read_file(module) + "\n" + usage;
    kite::Parser parser{kite::Lexer(source)};
    const kite::Program program = parser.parse_program();
    if (!parser.errors().empty()) return false;
    std::ostringstream output;
    kite::Interpreter interpreter(output);
    return interpreter.execute(program) && interpreter.errors().empty() && output.str() == expected;
}

} // namespace

int main() {
    std::cout << "## Kite standard library tests\n";
    bool all_passed = true;
    all_passed &= check(run_module("std/math.kite", "print(square(4)) print(hypotenuse(3, 4)) print(clamp(12, 0, 10))", "16\n5\n10\n"), "math module");
    all_passed &= check(run_module("std/strings.kite", "print(shout(\"kite\")) print(join(\"K\", \"ite\")) print(has_text(\"x\"))", "KITE\nKite\ntrue\n"), "strings module");
    all_passed &= check(run_module("std/collections.kite", "let values = [4, 5, 6] print(first(values)) print(last(values)) print(is_empty(values))", "4\n6\nfalse\n"), "collections module");

    if (all_passed) {
        std::cout << "\nAll standard library tests passed.\n";
        return 0;
    }
    std::cout << "\nStandard library tests failed.\n";
    return 1;
}
