#include <fstream>
#include <iostream>
#include <iterator>
#include <string>

#include "kite/interpreter/interpreter.hpp"
#include "kite/bytecode/bytecode.hpp"
#include "kite/parser/parser.hpp"

int main(int argc, char* argv[]) {
    bool use_bytecode = false;
    const char* path = nullptr;
    if (argc == 2) {
        path = argv[1];
    } else if (argc == 3 && std::string(argv[1]) == "--bytecode") {
        use_bytecode = true;
        path = argv[2];
    } else {
        std::cerr << "Usage: kite [--bytecode] <file>\n";
        return 1;
    }

    std::ifstream input(path);
    if (!input) {
        std::cerr << "Could not open file: " << path << '\n';
        return 1;
    }

    const std::string source((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
    kite::Parser parser{kite::Lexer(source)};
    const kite::Program program = parser.parse_program();
    if (!parser.errors().empty()) {
        for (const auto& error : parser.errors()) {
            std::cerr << error << '\n';
        }
        return 1;
    }

    if (use_bytecode) {
        kite::BytecodeCompiler compiler;
        const kite::Chunk chunk = compiler.compile(program);
        if (!compiler.errors().empty()) {
            for (const auto& error : compiler.errors()) std::cerr << error << '\n';
            return 1;
        }
        kite::BytecodeVm vm(std::cout);
        if (!vm.run(chunk)) {
            for (const auto& error : vm.errors()) std::cerr << error << '\n';
            return 1;
        }
        return 0;
    }

    kite::Interpreter interpreter(std::cout);
    if (!interpreter.execute(program)) {
        for (const auto& error : interpreter.errors()) {
            std::cerr << error << '\n';
        }
        return 1;
    }

    return 0;
}
