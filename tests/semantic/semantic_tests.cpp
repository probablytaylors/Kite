#include <iostream>
#include <string>

#include "kite/parser/parser.hpp"
#include "kite/semantic/semantic.hpp"

namespace {

bool check(bool condition, const char* name) {
    std::cout << (condition ? "[PASS] " : "[FAIL] ") << name << '\n';
    return condition;
}

bool analyzes(const std::string& source) {
    kite::Parser parser{kite::Lexer(source)};
    const kite::Program program = parser.parse_program();
    kite::SemanticAnalyzer analyzer;
    return parser.errors().empty() && analyzer.analyze(program);
}

bool rejects(const std::string& source) { return !analyzes(source); }

}

int main() {
    std::cout << "## Kite semantic tests\n";
    bool all_passed = true;
    all_passed &= check(analyzes("let value = 1 + 2 print(value)"), "valid arithmetic");
    all_passed &= check(analyzes("if (true && !false) { print(1) }"), "valid condition");
    all_passed &= check(analyzes("let values = [1, 2] print(values[0])"), "valid array access");
    all_passed &= check(analyzes("let user = {\"name\": \"Kite\"} print(user[\"name\"] )"), "valid map access");
    all_passed &= check(analyzes("fn add(a, b) { return a + b } print(add(1, 2))"), "valid function");
    all_passed &= check(rejects("let value = true + 1"), "invalid arithmetic types");
    all_passed &= check(rejects("if (1) { print(1) }"), "invalid condition type");
    all_passed &= check(rejects("missing = 1"), "unknown assignment");
    all_passed &= check(rejects("return 1"), "return outside function");

    if (all_passed) {
        std::cout << "\nAll semantic tests passed.\n";
        return 0;
    }
    std::cout << "\nSemantic tests failed.\n";
    return 1;
}