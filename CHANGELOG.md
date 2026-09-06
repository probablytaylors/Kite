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
