# Architecture

A Kite program goes through these stages:

```
source -> lexer -> parser -> semantic analysis -> resolver -> interpreter | bytecode VM | native
```

The **lexer** (`src/lexer/`) turns source text into a flat token stream. The
**parser** (`src/parser/`) is recursive descent with precedence climbing for
expressions; it produces an AST of `Statement` and `Expression` nodes defined
in `include/kite/ast/ast.hpp`. Every node carries a `NodeKind` tag and the rest
of the pipeline dispatches on it with a `switch` and `static_cast`. `for x in`
is desugared here into a C-style `for` over a hidden cursor.

**Semantic analysis** (`src/semantic/`) walks the AST once and checks the things
that are cheap to catch early: undefined names, assignment to unknown variables,
non-boolean conditions, bad index types, `return`/`break`/`continue` placement,
and non-numeric arithmetic. The **resolver** (`src/resolver/`) then assigns every
parameter and function-local a call-frame slot, so the runtimes index a flat
value stack instead of hashing names; top-level bindings stay global-by-name.

Execution then happens one of three ways.

The **interpreter** (`src/interpreter/`) walks the AST directly. It is the
reference runtime: every language feature works here. Functions get their own
frame, recursion works, and arrays and maps are reference values.

The **bytecode compiler and VM** (`src/bytecode/`) lower the AST into a `Chunk`
-- a constant pool plus a flat instruction stream -- and run it on a stack
machine with call frames and an exception-handler stack for `try`. The VM covers
the whole dynamic language. Where the two overlap the interpreter is the
reference: the VM is expected to produce identical output.

The **native compiler** (`src/native/`) type-checks a typed subset (annotated
`fn` signatures, `int`/`float`/`bool`/`string`, no collections yet), emits C,
and builds it with `cl /O2 /GL`.

Built-ins live in one registry (`src/builtins.cpp`) shared by the interpreter
and the VM; the VM reaches them through a `CallNative` instruction.

A `Chunk` can be written to a versioned `.kbc` file with `kite build` and read
back with `kite exec`. `load_bytecode` runs `validate_chunk` first, which
confirms every constant-pool index and jump target is in range, so a truncated
or tampered file is rejected instead of crashing the VM.

Planned but not built: a module/import system, user-defined types, and widening
the native compiler to strings and collections.
