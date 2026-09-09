#include <fstream>
#include <iostream>
#include <iterator>
#include <string>
#include <vector>

#include "kite/builtins.hpp"
#include "kite/interpreter/interpreter.hpp"
#include "kite/bytecode/bytecode.hpp"
#include "kite/module/module.hpp"
#include "kite/native/native.hpp"
#include "kite/parser/parser.hpp"
#include "kite/resolver/resolver.hpp"
#include "kite/semantic/semantic.hpp"
#include "kite/update.hpp"
#include "kite/version.hpp"

namespace {

int usage() {
    std::cerr <<
        "Usage:\n"
        "  kite <file.kite>              Run a source file with the tree-walking interpreter\n"
        "  kite run [--bytecode] <file> Run a source file (optionally on the bytecode VM)\n"
        "  kite build <file.kite> [-o <out.kbc>]\n"
        "                               Compile a source file to a bytecode artifact\n"
        "  kite exec <file.kbc>         Run a compiled bytecode artifact\n"
        "  kite native <file.kite> [-o <out.exe>]\n"
        "                               Compile a typed program to a native executable\n"
        "  kite update [--check]        Install the latest release (Windows)\n"
        "  kite --bytecode <file>       Alias for: kite run --bytecode <file>\n"
        "  kite --version               Print the version and exit\n";
    return 2;
}

int update_command(const std::vector<std::string>& args) {
    bool check_only = false;
    bool force = false;
    for (const auto& arg : args) {
        if (arg == "--check") check_only = true;
        else if (arg == "--force") force = true;
        else return usage();
    }
    return kite::run_update(check_only, force);
}

int print_version() {
    std::cout << "kite " << kite::kVersion << '\n';
    return 0;
}

bool load_source(const std::string& path, kite::Program& program) {
    std::vector<std::string> errors;
    if (!kite::load_program(path, program, errors)) {
        for (const auto& error : errors) std::cerr << error << '\n';
        return false;
    }
    return true;
}

bool front_end(const std::string& path, kite::Program& program) {
    if (!load_source(path, program)) {
        return false;
    }
    kite::SemanticAnalyzer analyzer;
    if (!analyzer.analyze(program)) {
        for (const auto& error : analyzer.errors()) std::cerr << error << '\n';
        return false;
    }
    kite::resolve(program);
    return true;
}

bool compile_chunk(const kite::Program& program, kite::Chunk& chunk) {
    kite::BytecodeCompiler compiler;
    chunk = compiler.compile(program);
    if (!compiler.errors().empty()) {
        for (const auto& error : compiler.errors()) std::cerr << error << '\n';
        return false;
    }
    return true;
}

int run_chunk(const kite::Chunk& chunk) {
    kite::BytecodeVm vm(std::cout);
    if (!vm.run(chunk)) {
        for (const auto& error : vm.errors()) std::cerr << error << '\n';
        return 1;
    }
    return 0;
}

int run_source(const std::string& path, bool use_bytecode) {
    kite::Program program;
    if (!front_end(path, program)) return 1;

    if (use_bytecode) {
        kite::Chunk chunk;
        if (!compile_chunk(program, chunk)) return 1;
        return run_chunk(chunk);
    }

    kite::Interpreter interpreter(std::cout);
    if (!interpreter.execute(program)) {
        for (const auto& error : interpreter.errors()) std::cerr << error << '\n';
        return 1;
    }
    return 0;
}

std::string default_artifact_path(const std::string& source_path) {
    const std::string suffix = ".kite";
    if (source_path.size() > suffix.size() &&
        source_path.compare(source_path.size() - suffix.size(), suffix.size(), suffix) == 0) {
        return source_path.substr(0, source_path.size() - suffix.size()) + ".kbc";
    }
    return source_path + ".kbc";
}

int build_command(const std::vector<std::string>& args) {
    std::string source_path;
    std::string output_path;
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (args[index] == "-o" || args[index] == "--output") {
            if (index + 1 >= args.size()) return usage();
            output_path = args[++index];
        } else if (source_path.empty()) {
            source_path = args[index];
        } else {
            return usage();
        }
    }
    if (source_path.empty()) return usage();
    if (output_path.empty()) output_path = default_artifact_path(source_path);

    kite::Program program;
    if (!front_end(source_path, program)) return 1;

    kite::Chunk chunk;
    if (!compile_chunk(program, chunk)) return 1;

    std::string error;
    if (!kite::save_bytecode(chunk, output_path, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    std::cerr << "Wrote " << output_path << " (" << chunk.constants.size() << " constants, "
              << chunk.code.size() << " instructions)\n";
    return 0;
}

int exec_command(const std::vector<std::string>& args) {
    if (args.size() != 1) return usage();

    kite::Chunk chunk;
    std::string error;
    if (!kite::load_bytecode(args[0], chunk, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    return run_chunk(chunk);
}

int native_command(const std::vector<std::string>& args) {
    std::string source_path;
    std::string output_path;
    bool emit_only = false;
    for (std::size_t index = 0; index < args.size(); ++index) {
        if (args[index] == "-o" || args[index] == "--output") {
            if (index + 1 >= args.size()) return usage();
            output_path = args[++index];
        } else if (args[index] == "--emit-c") {
            emit_only = true;
        } else if (source_path.empty()) {
            source_path = args[index];
        } else {
            return usage();
        }
    }
    if (source_path.empty()) return usage();

    kite::Program program;
    if (!load_source(source_path, program)) return 1;

    std::string error;
    if (emit_only) {
        std::string c_source;
        if (!kite::emit_c(program, c_source, error)) {
            std::cerr << error << '\n';
            return 1;
        }
        std::cout << c_source;
        return 0;
    }

    if (output_path.empty()) {
        const std::string suffix = ".kite";
        output_path = (source_path.size() > suffix.size() &&
            source_path.compare(source_path.size() - suffix.size(), suffix.size(), suffix) == 0)
            ? source_path.substr(0, source_path.size() - suffix.size()) + ".exe"
            : source_path + ".exe";
    }

    if (!kite::compile_native(program, output_path, error)) {
        std::cerr << error << '\n';
        return 1;
    }
    std::cerr << "Wrote " << output_path << '\n';
    return 0;
}

int run_command(const std::vector<std::string>& args) {
    bool use_bytecode = false;
    std::string source_path;
    std::vector<std::string> program_args;
    for (const auto& arg : args) {
        if (source_path.empty() && arg == "--bytecode") {
            use_bytecode = true;
        } else if (source_path.empty()) {
            source_path = arg;
        } else {
            program_args.push_back(arg);
        }
    }
    if (source_path.empty()) return usage();
    kite::set_program_args(std::move(program_args));
    return run_source(source_path, use_bytecode);
}

} // namespace

int main(int argc, char* argv[]) {
    const std::vector<std::string> args(argv + 1, argv + argc);
    if (args.empty()) return usage();

    if (args[0] == "--version" || args[0] == "-v" || args[0] == "version") return print_version();
    if (args[0] == "run") return run_command({args.begin() + 1, args.end()});
    if (args[0] == "build") return build_command({args.begin() + 1, args.end()});
    if (args[0] == "exec") return exec_command({args.begin() + 1, args.end()});
    if (args[0] == "native") return native_command({args.begin() + 1, args.end()});
    if (args[0] == "update") return update_command({args.begin() + 1, args.end()});
    if (args[0] == "--bytecode") return run_command(args);
    if (!args[0].empty() && args[0][0] != '-') return run_command(args);
    return usage();
}
