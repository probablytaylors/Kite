# Kite Language Reference

This is the current language reference for the implemented Kite runtime. It is
intentionally short and describes behavior that exists in the interpreter.

## Running A Program

```powershell
kite run program.kite
```

The bytecode VM can be selected with `kite run --bytecode program.kite`. A
source file can also be compiled to a bytecode artifact and run separately:

```powershell
kite build program.kite -o program.kbc
kite exec program.kbc
```

## Values

Kite currently supports:

- Integers: `42`
- Floating-point numbers: `3.14`
- Booleans: `true`, `false`
- Strings: `"Kite"`
- Arrays: `[1, 2, 3]`
- Maps: `{"name": "Kite"}`

## Variables

Use `let` to bind a value:

```kite
let name = "Kite"
let count = 3
```

Bindings are looked up by name. Function calls create a local scope. Existing
bindings can be updated with assignment, including the compound forms `+=`,
`-=`, `*=`, `/=`, and `%=`:

```kite
let count = 0
count = count + 1
count += 2
```

## Operators

Arithmetic operators:

```text
+  -  *  /  %
```

Unary minus is supported for numeric expressions. String values can be joined
with `+`. `*`, `/`, and `%` bind more tightly than `+` and `-`. Division of two
integers truncates toward zero and produces an integer; a float operand makes
the result a float. `%` is integer remainder when both operands are integers and
`fmod` otherwise; a zero right operand is a runtime error.

Comparison operators:

```text
==  !=  <  <=  >  >=
```

Comparisons produce booleans. Parentheses can group expressions.

Boolean operators are:

```text
!  &&  ||
```

`!` negates a boolean. `&&` and `||` short-circuit and require boolean
operands. `!` binds most tightly, followed by `&&`, then `||`.

## Arrays

Array elements are comma-separated and may contain expressions:

```kite
let values = [10, 20, 30]
print(values[1])
```

Indexes are zero-based and must be integers. Indexing an invalid value or an
out-of-range element is a runtime error. An element can be replaced in place:

```kite
let values = [10, 20, 30]
values[1] = 25
```

## Maps

```kite
let user = {"name": "Kite", "version": 1}
print(user["name"])
print(user.name)
user.version = 2
```

Map keys must be strings. `m.field` is shorthand for `m["field"]` for both
reading and assignment. Assigning a key that does not exist adds it. Reading a
missing key or using a non-string key is a runtime error.

## Strings

String literals are double-quoted and support the escapes `\n`, `\t`, `\r`,
`\0`, `\\`, and `\"`. Indexing a string with an integer returns a
one-character string.

## Comments

A `#` begins a comment that runs to the end of the line.

## Conditions And Loops

```kite
if (score >= 90) {
    print("A")
} else if (score >= 60) {
    print("pass")
} else {
    print("fail")
}

while (count < 3) {
    print(count)
    count += 1
}

for (let i = 0; i < 3; i += 1) {
    print(i)
}

for (item in [10, 20, 30]) {
    print(item)
}
```

The C-style `for` clauses are a statement, an expression, and a statement; any of
the three may be omitted. `for (name in iterable)` walks an array's elements, a
string's characters, or a map's keys (sorted); `range(n)`, `range(start, stop)`,
and `range(start, stop, step)` build integer arrays for counting loops.

Conditions must evaluate to booleans.

`break` leaves the nearest enclosing loop and `continue` skips to its next
iteration (running the `for` step). Both are compile-time errors outside a loop.

Before execution, semantic analysis checks identifiers, assignment targets,
condition types, collection index types, `return`/`break`/`continue` placement,
and concrete arithmetic operands.

## Errors

Any runtime error -- a failing built-in, an out-of-range index, division by
zero -- can be caught:

```kite
try {
    let text = read_file("config.txt")
    print(text)
} catch (err) {
    print("could not read config: " + err)
}
```

The caught value is always a string. `throw value` raises one deliberately;
a non-string value is converted with `str`. An uncaught error stops the program
and prints the message, as before. `break`, `continue`, and `return` still pass
through a `try` block to their target.

## Functions

```kite
fn add(a, b) {
    return a + b
}

print(add(2, 3))
```

Functions support parameters, local scopes, return values, and recursion.

Parameters and `let` bindings may carry a `: type` annotation and functions a
`-> type` return annotation (`int`, `float`, `bool`, `string`). The interpreter
and bytecode VM ignore them; `kite native` requires them.

## Built-ins

Strings and collections:

- `print(...)`: writes values separated by spaces and ends with a newline.
- `len(value)`: size of a string, array, or map.
- `type_of(value)`: `boolean`, `integer`, `float`, `string`, `array`, or `map`.
- `str(value)`, `int(value)`, `float(value)`: convert between scalar types.
- `upper(s)`, `lower(s)`, `trim(s)`: case and whitespace.
- `substring(s, start, end)`, `replace(s, from, to)`.
- `contains(s, part)`, `index_of(s, part)`: substring search.
- `split(s, separator)`, `join(array, separator)`.
- `ord(s)`, `chr(code)`: character/codepoint conversion.
- `append(array, value)`, `push(array, value)`, `pop(array)`.
- `keys(map)`, `has(map, key)`, `remove(map, key)`.
- `range(n)`, `range(start, stop)`, `range(start, stop, step)`: integer arrays.

Environment and I/O:

- `read_file(path)`, `write_file(path, content)`.
- `input(prompt)`: reads a line from standard input.
- `args()`: the program arguments after the script name.
- `env(name)`: an environment variable, or an empty string.

## Math Functions

The interpreter provides these numeric functions:

- `sqrt(value)`
- `pow(base, exponent)`
- `sin(value)`
- `cos(value)`
- `tan(value)`
- `log(value)`
- `abs(value)`
- `floor(value)`
- `ceil(value)`
- `exp(value)`
- `asin(value)`
- `acos(value)`
- `atan(value)`
- `atan2(y, x)`
- `min(left, right)`
- `max(left, right)`

Math functions accept numeric values. `sqrt` and `log` require non-negative
inputs. Invalid arguments and domain errors are reported at runtime.

Concurrency APIs are planned but not implemented yet.

## Imports

```kite
import "std/collections"
import "./helpers"

print(sum([1, 2, 3]))
```

`import "path"` parses another file and adds its top-level declarations to the
program before the rest of the current file runs. The `.kite` extension is
optional. A path is resolved against the importing file's directory first, then
against the directories around `kite.exe` -- so `import "std/collections"` finds
the library that ships next to the executable. Each file is loaded once no
matter how many times it is imported, and mutual imports are allowed. `import`
must appear at the top level, not inside a function or block.

## Standard Library

The `std/` directory ships alongside `kite.exe` and holds source-level modules:

- `std/math.kite` -- `square`, `cube`, `hypotenuse`, `clamp`
- `std/strings.kite` -- `shout`, `quiet`, `has_text`, `starts_with`,
  `ends_with`, `repeat`
- `std/collections.kite` -- `first`, `last`, `is_empty`, `sum`, `reversed`,
  `contains_value`
- `std/io.kite` -- `read_text`, `write_text`

Each is also a valid standalone program.

The bytecode compiler covers the whole language: literals, variables,
assignment, arithmetic, comparisons, short-circuit boolean logic, unary
operators, string operations, `print`, `if`/`else`, `while`, `for`,
`break`/`continue`, arrays, maps, indexing, index assignment, user-defined
functions, and every built-in (through a native-call instruction).

`kite build` produces a `.kbc` artifact holding the constant pool and
instruction stream; `kite exec` loads and runs one after validating that every
operand is in range. The bytecode VM produces output identical to the
interpreter.