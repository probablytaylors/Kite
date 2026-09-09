# Changelog

## Unreleased

- Added the lexer and token model.
- Added the `kite` command-line lexer tool.
- Added lexer coverage through CTest.
- Added the initial AST and parser for declarations, expressions, and calls.
- Added parser coverage through CTest.
- Added the initial interpreter for values, variables, and `print`.
- Added interpreter coverage through CTest.
- Added integer arithmetic with operator precedence and parenthesized expressions.
- Added floating-point literals and mixed numeric arithmetic.
- Added booleans, comparisons, and `if`/`else` blocks.
- Added `while` loops with boolean conditions.
- Added user-defined functions with parameters, returns, local scope, and recursion.
- Added array literals and zero-based read-only indexing.
- Added the current language reference at `docs/language-reference.md`.
- Added initial standard math functions: `sqrt`, `pow`, trigonometry, logarithms, and rounding.
- Added string-keyed maps, `len`, `upper`, and `lower` built-ins.
- Added assignment to existing variables and string concatenation.
- Added `!`, `&&`, and `||` with short-circuit evaluation.
- Added `read_file` and `write_file` text file built-ins.
- Added the initial bytecode compiler, constant pool, stack VM, and `--bytecode` CLI mode.
- Added initial source-level standard library modules under `std/`.
- Added CTest coverage for the standard-library modules.
- Added semantic analysis, runtime type inspection, mutable array operations, and expanded math functions.
- Added bytecode jumps, conditional/loop execution, array/map construction, and indexing.
- Added a versioned bytecode artifact format with `save_bytecode`/`load_bytecode` and `validate_chunk` operand checking.
- Added the `kite run`, `kite build`, and `kite exec` subcommands (`--bytecode` kept as an alias).
- Added the `KITE_WERROR` build option.
- Fixed a bytecode compiler bug where an `if` without an `else` left the condition value unbalanced on the stack.
- Removed a duplicate `while` keyword branch in the lexer.
