# Architecture

Kite is organized as a sequence of replaceable language components:

1. Source text enters the lexer.
2. Tokens are consumed by the parser.
3. The parser produces an AST.
4. Semantic analysis validates the AST.
5. The interpreter or bytecode VM executes it.
6. Runtime, types, standard library, and modules extend it.

The lexer and token model are implemented today. The parser consumes those
tokens and builds a small AST for declarations, literals, identifiers, and
calls. The interpreter evaluates integer and string values, stores variables,
provides `print`, evaluates numeric arithmetic, and executes conditional
blocks and while loops. The remaining components are planned boundaries; they should be added
when each stage of the language needs them rather than as empty source files.
Function calls create local scopes and can propagate return values, including
through recursive calls.
Arrays are represented as runtime values and support literal construction and
read-only integer indexing. Mutation and richer collection types remain future
work.

The initial bytecode layer compiles a supported expression subset into a
constant pool and instruction stream. A stack VM executes those instructions;
the tree-walking interpreter remains the complete runtime path for features
not yet represented in bytecode. Control flow and collection construction are
now represented; user-defined function frames remain on the compiler roadmap.

A `Chunk` can be serialized to a versioned on-disk artifact (`kite build`) and
loaded back (`kite exec`). Loading runs `validate_chunk`, which checks that
every constant-pool index and jump target is in range, so a corrupt artifact
is rejected before the VM sees it. The tree-walking interpreter is the
reference semantics for any program the bytecode compiler also accepts.
