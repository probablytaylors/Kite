# Changelog

## Unreleased

### Language

- String interpolation: `"total is ${a + b}"`. Each `${...}` is parsed as an
  expression and wrapped with `str`. `\${` is a literal `${`.
- Compound assignment on an index or field: `xs[i] += 1`, `counts.hits += 1`.
  The target and index must be plain (an identifier or literal).

### Errors

Errors now point at the source. The parser, semantic analyzer, and interpreter
report a line and column, and the offending line is printed with a caret:

```
script.kite:12:7: unknown identifier: total
 12 | print(total)
    |       ^
```

The bytecode VM carries a line table (`.kbc` format 14), so its runtime errors
show the line too (no column yet).

### Internal

- The semantic analyzer dispatches on `NodeKind` like the rest of the pipeline
  instead of `dynamic_cast` chains.
- `kite update`'s "what's new" reader is simpler and strips Markdown.

## 0.3.2 - 2026-09-09

Security fixes from a full audit. Update if you ever run `kite exec` on a `.kbc`
file you did not compile yourself.

### Security

- **The bytecode VM did not bounds-check local-slot access.** A hand-crafted
  `.kbc` file run through `kite exec` could make `LoadLocal` / `StoreLocal` /
  `IncLocal` / `DecLocal` / `ReturnLocal` read or write memory outside the
  value stack, which is exploitable for corruption. Every slot access is now
  checked.
- **`MakeStruct` trusted the stack contents.** A crafted `.kbc` could pass a
  non-string where a field name or the type name was expected, causing a type
  confusion. It now verifies them.
- Count operands on `MakeMap` / `MakeStruct` / `CallNative` could overflow
  `size_t` and slip past the stack-depth check; the bytecode verifier now
  bounds them and the runtime checks no longer multiply.
- `load_bytecode` sized its tables from the file's own claimed counts before
  reading; it now also checks them against the file size, so a tiny file can't
  force a large allocation.
- `INT64_MIN / -1` and `INT64_MIN % -1` are undefined in C++ and crashed the
  process (interpreter, VM, and native). They now return the wrapped result.
- The parser had no recursion limit, so deeply nested input
  (`((((...`, `----...`, nested blocks) overflowed the stack. It now stops at
  a fixed depth.
- `kite native` escaped only a few characters in string literals; it now
  octal-escapes everything outside plain ASCII (and `?`, to defeat trigraphs),
  guards non-finite float literals, and routes integer `/` and `%` through
  helpers that can't trigger `INT64_MIN / -1`.
- The module loader now caps import depth and file count.
- `kite update` validates release tags before using them in a command, aborts
  if it can't fetch the checksum (rather than proceeding unverified), and
  flags releases whose changelog mentions security fixes. Its "what's new"
  summary now strips Markdown and shows one short line per change.

Added `SECURITY.md` with a disclosure policy and the threat model.

## 0.3.1 - 2026-09-09

### Tooling

- `kite update <version>` installs a specific release, including an older one
  (a downgrade asks for confirmation and defaults to no).
- `kite update --list` shows every release and marks the installed one.
- `kite update` on the latest version now points at `--force` and
  `kite update <version>`.

## 0.3.0 - 2026-09-09

### Language

- `break` and `continue` in `while` and `for` loops, on all execution paths
  (interpreter, bytecode VM, native). Using either outside a loop is a
  compile-time error.
- `for name in iterable` over arrays, strings, and maps (keys, sorted), plus
  `range(n)` / `range(start, stop)` / `range(start, stop, step)`.
- `try { } catch (err) { }` and `throw value`. Any runtime error (a failed
  built-in, a bad index, division by zero, or a `throw`) is caught as a string.
  Works on the interpreter and the bytecode VM; native mode rejects it.
- `import "path"` merges another file's top-level declarations. Paths resolve
  against the importing file and then the directories around `kite.exe` (so
  `import "std/collections"` finds the bundled library). Imports are loaded
  once; mutual imports are fine.
- `map.field` as shorthand for `map["field"]`, reading and assigning.
- `struct Name { field, ... }` declares a record type; `Name { field: value, ... }`
  builds one. Fields are read and assigned with `.`, `type_of` reports the
  struct name, and the semantic pass checks that a literal names every field
  exactly once. Struct names must start with an uppercase letter. Interpreter
  and VM only (`.kbc` format 13, `MakeStruct`); native mode rejects them.
- `#` line comments.
- Integer division: `int / int` is now integer, matching C/Go/Rust; a float
  operand still gives a float.
- Index assignment: `xs[i] = v` and `m[k] = v`. Maps are built by assigning
  keys.
- String indexing: `s[i]` gives a one-character string.
- Optional `: type` annotations on `let` and parameters and `-> type` on
  functions. The interpreter and VM ignore them; the native compiler requires
  them.

### Runtime

- Added `kite native <file.kite>`: a typed subset of Kite compiles through C and
  `cl /O2 /GL` to a standalone executable. Functions take annotated parameter
  and return types (`fn f(n: int) -> int`), `let` types are inferred, and the
  checker rejects mismatches before codegen. `int`, `float`, `bool`, `string`,
  arithmetic, control flow, and recursion are supported; arrays, maps, and the
  dynamic built-ins are not yet. Native `fib(30)` runs within ~1.2x of
  hand-written C and ~12x faster than the bytecode VM.
- A shared builtin registry used by both the interpreter and the VM through a
  `CallNative` opcode, so builtins no longer force the VM back to the
  interpreter. New: `str`, `int`, `float`, `split`, `join`, `substring`,
  `contains`, `index_of`, `replace`, `trim`, `keys`, `has`, `remove`, `ord`,
  `chr`, `push`, `input`, `args`, `env`, `range`.
- The VM short-circuits `&&` / `||`.
- `.kbc` format 13: `PushHandler` / `PopHandler` / `Throw` opcodes for `try`,
  and `MakeStruct` for struct literals.
- A module loader (`src/module/`) resolves `import` before semantic analysis.

### Tooling

- `kite update` shows the installed and latest versions side by side, lists
  what changed from the release's changelog, asks before installing (skip with
  `--yes`), and downloads with a progress bar. `--check` reports without
  installing.

### Fixed

- `kite native` never invoked its build script when the shell disabled
  current-directory executable lookup (`NoDefaultCurrentDirectoryInExePath`);
  the script is now run by absolute path and pins its working directory.
- The semantic analyzer rejected `m[k]` when `k` had an unknown type.
- The VM's "map key not found" error now names the key and reports a
  non-string key separately, matching the interpreter.

## 0.2.0 - 2026-09-09

### Performance

- Dispatch AST nodes by a `NodeKind` tag instead of `dynamic_cast` chains.
- A resolver pass assigns parameters and function-locals call-frame slots; the
  interpreter and VM index a flat value stack instead of hashing names.
- User-defined functions now compile and run on the bytecode VM (call frames,
  a function table, `LoadLocal`/`StoreLocal`).
- `Value` is a 16-byte tagged union with intrusive refcounting, shared by the
  interpreter and VM, replacing the 40-byte `std::variant`.
- Fused opcodes: `IncLocal`/`DecLocal` for `i += 1`, `ReturnLocal`/`ReturnConst`,
  and `JumpIfFalse` now consumes its condition (`.kbc` format 8).
- Net: recursive `fib(30)` went from ~28x slower than CPython to ~2x on the
  interpreter; the VM now runs `fib`, a plain loop, and a primes sieve
  *faster* than CPython 3.14.

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
