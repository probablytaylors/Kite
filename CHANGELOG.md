# Changelog

## Unreleased

### Performance

- Dispatch AST nodes by a `NodeKind` tag instead of `dynamic_cast` chains.
- A resolver pass assigns parameters and function-locals call-frame slots; the
  interpreter and VM index a flat value stack instead of hashing names.
- User-defined functions now compile and run on the bytecode VM (call frames,
  `LoadLocal`/`StoreLocal`, a function table; `.kbc` format 5).
- `Value` is a 16-byte tagged union with intrusive refcounting, shared by the
  interpreter and VM, replacing the 40-byte `std::variant`.
- Net: recursive `fib(30)` went from ~28x slower than CPython to ~2x on the
  interpreter and ~1.15x on the VM.

### Fixed

- The semantic analyzer rejected valid code: a variable holding a function's
  result could not be reassigned, a parameter could not be an `if`/`while`/`for`
  condition, and `string + <call result>` was rejected.
- `return f(x)` clobbered its own result through an aliased output parameter.
- Deep recursion crashed the process; it now stops at depth 1000 with an error.
- Printing a self-referential array or map no longer overflows the stack.

### Added

- `kite update` — downloads the latest release, verifies its checksum, and runs
  the installer in place (Windows).
- `examples/stackvm.kite`, a stack machine with a call stack written in Kite.

## 0.1.0 - 2026-09-08

First tagged release. `kite-setup.exe` installs the interpreter, adds it to
`PATH`, and registers the `.kite` extension.

### Language

- `let` bindings, assignment, and `+= -= *= /= %=` compound assignment.
- Integer and floating-point arithmetic with precedence; `+ - * / %` and unary `-`.
- Booleans, comparisons, and `! && ||` with short-circuit evaluation.
- `if` / `else if` / `else`, `while`, and C-style `for` loops.
- Functions with parameters, returns, local scope, and recursion.
- Arrays, string-keyed maps, and indexing; `append` and `pop` mutate arrays.
- Double-quoted strings with `\n \t \r \0 \\ \"` escapes and `+` concatenation.
- Built-ins: `print`, `len`, `upper`, `lower`, `type_of`, `read_file`, `write_file`,
  and the math functions `sqrt`, `pow`, `sin`, `cos`, `tan`, `log`, `abs`,
  `floor`, `ceil`, `exp`, `asin`, `acos`, `atan`, `atan2`, `min`, `max`.

### Tooling

- `kite run` (tree-walking interpreter, or the stack VM with `--bytecode`).
- `kite build` / `kite exec` for versioned `.kbc` bytecode artifacts; loading
  validates every constant index and jump target.
- `kite --version`.
- Semantic analysis of identifiers, assignment targets, condition and index
  types, return placement, and arithmetic operands.

### Fixed

- Interpreter looped forever on `return` inside a `while` body.
- Bytecode compiler left an `if` without an `else` unbalanced on the stack.
