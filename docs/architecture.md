# Architecture

A Kite program goes through five stages:

```
source -> lexer -> parser -> semantic analysis -> interpreter or bytecode VM
```

The **lexer** (`src/lexer/`) turns source text into a flat token stream. The
**parser** (`src/parser/`) is recursive descent with precedence climbing for
expressions; it produces an AST of `Statement` and `Expression` nodes defined
in `include/kite/ast/ast.hpp`. **Semantic analysis** (`src/semantic/`) walks the
AST once before execution and checks the things that are cheap to catch early:
undefined names, assignment to unknown variables, non-boolean conditions, bad
index types, `return` outside a function, and non-numeric arithmetic.

Execution then happens one of two ways.

The **interpreter** (`src/interpreter/`) walks the AST directly. It's the
complete runtime: every language feature works here. Functions get their own
scope, recursion works, arrays and maps are reference values, and the built-ins
live in `evaluate_call`.

The **bytecode compiler and VM** (`src/bytecode/`) are the second path. The
compiler lowers the AST into a `Chunk` -- a constant pool plus a flat
instruction stream -- and a stack machine runs it. The VM handles expressions,
control flow, and collections, but not user-defined function calls yet, so
programs that call their own functions still run on the interpreter. Where the
two overlap, the interpreter is the reference: the VM is expected to produce
identical output.

A `Chunk` can be written to a versioned `.kbc` file with `kite build` and read
back with `kite exec`. `load_bytecode` runs `validate_chunk` first, which
confirms every constant-pool index and jump target is in range, so a truncated
or tampered file is rejected instead of crashing the VM.

Planned but not built: function call frames in the VM, a module/import system,
user-defined types, and a static type system feeding a native backend.
