#include <iostream>
#include <sstream>
#include <string>

#include "kite/bytecode/bytecode.hpp"
#include "kite/parser/parser.hpp"

namespace {

bool check(bool condition, const char* name) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
    return condition;
}

bool run_source(const std::string& source, const std::string& expected) {
    kite::Parser parser{kite::Lexer(source)};
    const kite::Program program = parser.parse_program();
    if (!parser.errors().empty()) return false;
    kite::BytecodeCompiler compiler;
    const kite::Chunk chunk = compiler.compile(program);
    if (!compiler.errors().empty()) return false;
    std::ostringstream output;
    kite::BytecodeVm vm(output);
    return vm.run(chunk) && vm.errors().empty() && output.str() == expected;
}

} // namespace

int main() {
    std::cout << "## Kite bytecode tests\n";
    bool all_passed = true;
    all_passed &= check(run_source("let value = 2 + 3 * 4 print(value)", "14\n"), "arithmetic bytecode");
    all_passed &= check(run_source("let name = \"Kite\" print(\"Hello, \" + name)", "Hello, Kite\n"),
        "string bytecode");
    all_passed &= check(run_source("print(2 < 3, true && !false)", "true true\n"), "boolean bytecode");
    all_passed &= check(!run_source("print(missing)", ""), "bytecode runtime error");

    if (all_passed) {
        std::cout << "\nAll bytecode tests passed.\n";
        return 0;
    }
    std::cout << "\nBytecode tests failed.\n";
    return 1;
}