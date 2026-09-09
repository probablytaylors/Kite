# Kite

Kite is a programming language built from scratch in C++.

## Current Status

The lexer, parser, AST, semantic analyzer, interpreter, and initial bytecode VM
are implemented. Kite can read source files, validate types, evaluate numeric
and boolean values, mutate arrays, perform arithmetic and comparisons, and use
control flow, functions, file I/O, and standard-library helpers.

## Project Structure

```text
.
├── .github/workflows/       Continuous integration configuration
├── docs/                    Architecture and language notes
├── examples/                Sample Kite programs
├── include/kite/            Public C++ headers
├── installer/               Windows installer script
├── src/                     Lexer, parser, semantic analyzer, interpreter, bytecode VM
├── std/                     Source-level standard library modules
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

### Bytecode VM

The bytecode compiler and stack VM can be selected with:

```powershell
.\build\Debug\kite.exe run --bytecode examples\calculator.kite
```

### Compiled artifacts

`kite build` compiles a source file to a `.kbc` bytecode artifact, and
`kite exec` runs one without touching the front end:

```powershell
.\build\Debug\kite.exe build examples\calculator.kite -o calculator.kbc
.\build\Debug\kite.exe exec calculator.kbc
```

The artifact format is versioned. `kite exec` validates every operand against
the constant pool and code bounds before running, so a truncated or corrupt
artifact is rejected rather than executed. `--bytecode` remains an alias for
`run --bytecode`.

Configure with `-D KITE_WERROR=ON` to treat compiler warnings as errors.

## Installer

`installer/kite.iss` builds a Windows installer with [Inno Setup](https://jrsoftware.org/isinfo.php).
It installs `kite.exe`, adds it to `PATH`, and registers the `.kite` extension.

```powershell
cmake --build build --config Release
iscc installer\kite.iss
```

The installer is written to `installer\output\kite-setup.exe`.

## CMake Targets

- `kite_core`: static library with the lexer, parser, AST, semantic analyzer, interpreter, and bytecode VM.
- `kite`: command-line Kite tool (`run`, `build`, `exec`).

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
- [x] Semantic analysis and richer type checking
- [x] Runtime type inspection and mutable array operations
- [x] Initial bytecode compiler and stack VM
- [x] Initial standard math functions
- [x] Expanded math functions
- [x] Initial source-level standard library modules
- [x] Bytecode control flow and collection literals/indexing
- [x] Bytecode artifacts: `kite build` / `kite exec` with a validated on-disk format
- [x] `%`, compound assignment, `else if`, `for` loops, string escapes
- [x] Windows installer (Inno Setup)
- [ ] Richer static inference and user-defined types
- [ ] Maps mutation and collection methods
- [ ] Modules and imports
- [ ] Bytecode function call frames and returns
- [ ] Standalone executables (bundle a `.kbc` artifact with the runtime)
- [ ] Tooling such as a REPL, formatter, debugger, and package manager

See [docs/architecture.md](docs/architecture.md),
[docs/language-notes.md](docs/language-notes.md), and the
[language reference](docs/language-reference.md) for current documentation.
