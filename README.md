# Kite

Kite is a programming language built from scratch in C++.

## Current Status

The lexer, parser, AST, and a small interpreter are implemented. Kite can read
a source file, evaluate numeric and boolean values, perform basic arithmetic
and comparisons, store `let` bindings, and use `if`/`else` with `print`.

## Project Structure

```text
.
├── .github/workflows/       Continuous integration configuration
├── .vscode/                 VS Code workspace settings
├── docs/                    Architecture and language notes
├── examples/                Small future Kite programs
├── include/kite/            Public C++ headers
├── src/interpreter/         Interpreter implementation
├── src/lexer/               Lexer and token implementation
├── src/parser/              Parser implementation
├── std/                     Future standard library
├── tests/                   C++ tests and component test areas
├── tools/                   Future developer tools
├── CMakeLists.txt
├── CHANGELOG.md
└── LICENSE
```

## Build

From a PowerShell terminal at the repository root:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
```

The executable is written to `build/Debug/kite.exe`.

## Run

```powershell
.\build\Debug\kite.exe examples\hello.kite
```

The command reads, parses, and executes a Kite source file.

The initial bytecode compiler and stack VM can be selected with:

```powershell
.\build\Debug\kite.exe --bytecode examples\calculator.kite
```

## Tests

Build the project first, then run:

```powershell
ctest --test-dir build -C Debug --output-on-failure
```

CTest runs lexer, parser, interpreter, bytecode, and standard-library tests.

## CMake Targets

- `kite_core`: static library containing the lexer, parser, AST, interpreter, and bytecode VM.
- `kite`: command-line Kite interpreter.
- `kite --bytecode`: bytecode compiler and stack VM mode.
- `kite_lexer_tests`: lexer behavior tests.
- `kite_parser_tests`: parser and AST behavior tests.
- `kite_interpreter_tests`: interpreter behavior tests.
- `kite_bytecode_tests`: bytecode compiler and VM tests.
- `kite_stdlib_tests`: source-level standard-library tests.

## Development Roadmap

- [x] Initial lexer, token model, and command-line tool
- [x] Initial parser and AST
- [x] Initial interpreter with variables and `print`
- [x] Integer and floating-point arithmetic with operator precedence
- [x] Booleans, comparisons, and `if`/`else`
- [x] `while` loops
- [x] User-defined functions with parameters, returns, local scope, and recursion
- [x] Array literals and read-only indexing
- [x] Maps with string-key lookup
- [x] Basic string and collection utilities
- [x] Variable assignment and string concatenation
- [x] Boolean logic with short-circuit evaluation
- [x] Basic file read/write built-ins
- [x] Initial bytecode compiler and stack VM
- [x] Initial standard math functions
- [x] Initial source-level standard library modules
- [ ] Semantic analysis and richer type checking
- [ ] Runtime services and type system
- [ ] Expanded standard library and advanced mathematics
- [ ] Collections and modules
- [ ] Full bytecode support for control flow, functions, and collections
- [ ] Tooling such as a REPL, formatter, debugger, and package manager

See [docs/architecture.md](docs/architecture.md),
[docs/language-notes.md](docs/language-notes.md), and the
[language reference](docs/language-reference.md) for current documentation.
