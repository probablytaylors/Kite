# Changelog

## Unreleased

- Fixed the semantic analyzer rejecting valid code: a variable holding a
  function's result could not be reassigned, and a parameter could not be used
  as an `if`/`while`/`for` condition.
- Added `examples/stackvm.kite`, a stack machine with a call stack written in Kite.

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
