# Kite Language Reference

This is the current language reference for the implemented Kite runtime. It is
intentionally short and describes behavior that exists in the interpreter.

## Running A Program

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug
.\build\Debug\kite.exe examples\hello.kite
```

The bytecode VM can be selected with:

```powershell
.\build\Debug\kite.exe run --bytecode examples\calculator.kite
```

A source file can also be compiled to a bytecode artifact and run separately:

```powershell
.\build\Debug\kite.exe build examples\calculator.kite -o calculator.kbc
.\build\Debug\kite.exe exec calculator.kbc
```

## Values

Kite currently supports:

- Integers: `42`
- Floating-point numbers: `3.14`
- Booleans: `true`, `false`
- Strings: `"Kite"`
- Arrays: `[1, 2, 3]`

## Variables

Use `let` to bind a value:

```kite
let name = "Kite"
let count = 3
```

Bindings are looked up by name. Function calls create a local scope. Existing
bindings can be updated with assignment:

```kite
let count = 0
count = count + 1
```

## Operators

Arithmetic operators:

```text
+  -  *  /
```

Unary minus is supported for numeric expressions. String values can be joined
with `+`. Multiplication and division bind more tightly than addition and subtraction.
Division always produces a floating-point result.

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
out-of-range element is a runtime error. Array mutation is not implemented yet.

## Maps

Maps use string keys and support read-only lookup:

```kite
let user = {"name": "Kite", "version": 1}
print(user["name"])
```

Map keys must be strings. Missing keys and invalid key types are runtime errors.

## Conditions And Loops

```kite
if (score >= 60) {
    print("passed")
} else {
    print("try again")
}

while (count < 3) {
    print(count)
    let count = count + 1
}
```

Conditions must evaluate to booleans.

Before execution, semantic analysis checks identifiers, assignment targets,
condition types, collection index types, return placement, and concrete
arithmetic operands.

## Functions

```kite
fn add(a, b) {
    return a + b
}

print(add(2, 3))
```

Functions support parameters, local scopes, return values, and recursion.

## Built-ins

The current built-in functions are:

- `print(...)`: writes values separated by spaces and ends with a newline.
- `len(value)`: returns the size of a string, array, or map.
- `upper(value)`: converts a string to uppercase.
- `lower(value)`: converts a string to lowercase.
- `read_file(path)`: reads a file as a string.
- `write_file(path, content)`: writes a string and returns `true`.

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

Modules and concurrency APIs are planned but not implemented yet.

Runtime helpers include:

- `type_of(value)`: returns `boolean`, `integer`, `float`, `string`, `array`, or `map`.
- `append(array, value)`: mutates an array and returns it.
- `pop(array)`: removes and returns the last array element.

## Standard Library

The `std/` directory contains the first source-level library modules:

- `std/math.kite`
- `std/strings.kite`
- `std/collections.kite`
- `std/io.kite`

They are valid standalone Kite source files and wrap the current built-ins.
Imports are not implemented yet, so programs must not assume these modules are
available automatically.

The bytecode compiler currently supports literals, variables, assignment,
arithmetic, comparisons, boolean logic, unary operators, string concatenation,
`print`, `if`/`else`, `while`, arrays, maps, and indexing. User-defined
functions and math/file built-ins still run through the tree-walking
interpreter until bytecode call frames and native-call instructions are added.

`kite build` produces a `.kbc` artifact holding the constant pool and
instruction stream; `kite exec` loads and runs one after validating that every
operand is in range. When a program uses only the supported subset, the
bytecode VM produces output identical to the interpreter.