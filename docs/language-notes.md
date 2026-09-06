# Language Notes

This document records Kite syntax and semantic decisions as the language develops.

## Lexer Milestone

The first source-level syntax supports:

- Identifiers containing letters, digits, and underscores, starting with a letter or underscore.
- The `let` keyword.
- Decimal integer literals.
- Decimal floating-point literals such as `3.14`.
- Double-quoted string literals without escape processing yet.
- The `=` operator.
- `(` and `)` punctuation.
- `,` between call arguments.
- `+`, `-`, `*`, and `/` arithmetic operators.
- `if`, `else`, `true`, and `false` keywords.
- `==`, `!=`, `<`, `<=`, `>`, and `>=` comparison operators.
- `!`, `&&`, and `||` boolean operators.
- `{` and `}` block delimiters.
- `[` and `]` array delimiters.
- Whitespace and newlines as separators rather than tokens.

Unknown characters and unterminated strings produce invalid tokens with source
line and column information. `print` is currently lexed as an identifier; it
will become a language feature when the parser and runtime are implemented.

The command-line tool reads a source file and passes it through the lexer,
parser, and interpreter.

## Parser Milestone

The parser currently accepts:

- `let name = expression` declarations.
- Assignment to an existing variable.
- Integer and string literals.
- Identifier expressions.
- Calls with zero or more comma-separated arguments, such as `print(value)`.
- Parenthesized expressions.
- Arithmetic expressions with `*` and `/` taking precedence over `+` and `-`.
- Multiple statements in one source file.

The parser produces an AST consumed by the interpreter. `print` remains an
ordinary identifier in the parser and is handled as a built-in at runtime.

## Interpreter Milestone

The interpreter currently supports:

- Integer and string values.
- Variables created by `let` declarations.
- Identifier lookup.
- The built-in `print` function, with zero or more arguments.

Unknown variables and functions produce runtime errors. Richer type checking
and nested scope rules are not implemented yet.

Arithmetic operands must be numeric. Division by zero is a runtime error.
Floating-point values can be combined with integers, and `/` produces a
floating-point result.

## Control Flow Milestone

The language currently supports conditional blocks:

```kite
if (score >= 60) {
	print("passed")
} else {
	print("try again")
}
```

Conditions must evaluate to booleans. `while` loops use the same condition
rules and execute a brace-delimited block:

```kite
while (count < 3) {
	print(count)
	let count = count + 1
}
```

## Functions

Functions use `fn`, parameters, a brace-delimited body, and `return`:

```kite
fn add(a, b) {
	return a + b
}

print(add(2, 3))
```

Function calls create a local scope. Functions may call themselves recursively.
Every function currently returns an empty value when execution reaches the end
without a `return` statement.

## Collections

Array literals and zero-based read-only indexing are supported:

```kite
let values = [10, 20, 30]
print(values[1])
```

Arrays can contain nested arrays. Maps, mutation, slicing, and collection
methods are planned for later runtime work. Existing variables can be updated
with assignment, and strings can be joined with `+`.

Maps use string keys and support read-only indexing. The built-ins `len`,
`upper`, and `lower` provide basic collection and string operations. The
`read_file` and `write_file` built-ins provide basic text file I/O.

The bytecode compiler currently covers literals, variables, assignment,
arithmetic, comparisons, boolean logic, unary operators, string concatenation,
and `print`. Other syntax continues to use the tree-walking interpreter.

## Math

The standard math functions currently available are `sqrt`, `pow`, `sin`,
`cos`, `tan`, `log`, `abs`, `floor`, and `ceil`. They use the C++ standard
library implementation and return numeric values.
