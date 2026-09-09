# Language Notes

This document records Kite syntax and semantic decisions as the language develops.

## Lexer Milestone

The first source-level syntax supports:

- Identifiers containing letters, digits, and underscores, starting with a letter or underscore.
- The `let` keyword.
- Decimal integer literals.
- Decimal floating-point literals such as `3.14`.
- Double-quoted string literals with `\n`, `\t`, `\r`, `\0`, `\\`, and `\"` escapes.
- The `=` operator and the compound forms `+=`, `-=`, `*=`, `/=`, `%=`.
- `(` and `)` punctuation.
- `,` between call arguments and `;` between `for` clauses.
- `+`, `-`, `*`, `/`, and `%` arithmetic operators.
- `if`, `else`, `while`, `for`, `fn`, `return`, `true`, and `false` keywords.
- `==`, `!=`, `<`, `<=`, `>`, and `>=` comparison operators.
- `!`, `&&`, and `||` boolean operators.
- `{` and `}` block delimiters.
- `[` and `]` array delimiters.
- Whitespace and newlines as separators rather than tokens.

Unknown characters and unterminated strings produce invalid tokens with source
line and column information. `print` is lexed as an identifier and handled as a
built-in at runtime rather than as a keyword.

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

Unknown variables and functions produce runtime errors. Semantic analysis runs
before execution and checks concrete identifiers, conditions, assignments, and
operand types.

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

Arrays can contain nested arrays and can be mutated with `append` and `pop`.
Maps support read-only indexing. Existing variables can be updated with
assignment, and strings can be joined with `+`.

Maps use string keys and support read-only indexing. The built-ins `len`,
`upper`, and `lower` provide basic collection and string operations. The
`read_file` and `write_file` built-ins provide basic text file I/O. The
`type_of` built-in reports the runtime type of a value.

The bytecode compiler currently covers literals, variables, assignment,
arithmetic, comparisons, boolean logic, unary operators, string concatenation,
and `print`. Other syntax continues to use the tree-walking interpreter.

## Math

The standard math functions currently available are `sqrt`, `pow`, `sin`,
`cos`, `tan`, `log`, `abs`, `floor`, `ceil`, `exp`, `asin`, `acos`, `atan`,
`atan2`, `min`, and `max`. They use the C++ standard library implementation
and return numeric values.

## Operators And Loops Milestone

The `%` operator produces integer remainder for integer operands and `fmod`
otherwise. Assignment has the compound forms `+=`, `-=`, `*=`, `/=`, and `%=`,
which desugar to the plain binary operator plus assignment.

`if` accepts `else if` without nesting braces. `for (init; condition; step) {}`
is a C-style loop; each clause is optional. `return` inside a `while` or `for`
body stops the loop and the enclosing function.

String literals process the escapes `\n`, `\t`, `\r`, `\0`, `\\`, and `\"`.

## General Scripting Milestone

- `#` starts a line comment.
- Integer division: two integer operands give a truncated integer; a float
  operand gives a float. `%` was already integer-for-integers.
- `xs[i] = v` and `m[k] = v` assign into arrays and maps; a map grows a new key
  on assignment. `s[i]` reads a one-character string.
- `break` and `continue` work in `while` and `for` on every execution path and
  are compile-time errors outside a loop. `continue` still runs the `for` step.
- `for name in iterable` (arrays, strings, map keys). The parser desugars it to a
  C-style `for` over a hidden cursor `[__iter(iterable), 0]`, so every backend
  gets it for free. `range()` builds the integer array for counting loops.
- `try { } catch (e) { }` and `throw`. The interpreter reuses its `bool` error
  return; the VM gets a handler stack with `PushHandler`/`PopHandler`/`Throw`
  and unwinds the value and frame stacks to the handler on a fault. The caught
  value is always a string. `break`/`continue`/`return` emit the matching
  `PopHandler`s so they leave a `try` cleanly.
- The bytecode VM now covers the whole language and calls every built-in
  through a `CallNative` instruction; `&&` and `||` short-circuit.
- Optional `: type` / `-> type` annotations; only `kite native` enforces them.
