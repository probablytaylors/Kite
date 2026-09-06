# Architecture

Kite is organized as a sequence of replaceable language components:

1. Source text enters the lexer.
2. Tokens are consumed by the parser.
3. The parser produces an AST.
4. The interpreter evaluates the AST.
5. Semantic analysis, runtime, types, standard library, and modules extend it.

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
not yet represented in bytecode.
