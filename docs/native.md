# Native compilation

`kite native program.kite` compiles a typed subset of Kite to a standalone
executable. It generates C, then invokes the MSVC toolchain with
`/O2 /GL /LTCG`, so the output is within a small factor of hand-written C.

```
kite native program.kite            # -> program.exe
kite native program.kite -o app.exe
kite native program.kite --emit-c   # print the generated C and stop
```

On Windows it locates the Visual Studio C++ tools through `vswhere`; the
"Desktop development with C++" workload must be installed.

## The typed subset

Every function needs a type on each parameter and a return type:

```
fn gcd(a: int, b: int) -> int {
    while (b != 0) {
        let t = b
        b = a % b
        a = t
    }
    return a
}

print(gcd(1071, 462))
```

- Types: `int` (64-bit), `float` (double), `bool`, `string` (literals only).
- `let` types are inferred from the initializer, or written as `let x: int = 0`.
- Arithmetic, comparisons, `&& || !`, `if`/`else if`/`else`, `while`, `for`,
  and recursion all work.
- Top-level statements become `main`.

The checker runs before codegen and reports type mismatches (wrong argument
types, a non-`bool` condition, a `return` that disagrees with the signature,
reassigning a variable to a different type).

## Not yet supported

Arrays, maps, and the dynamic built-ins (`len`, `append`, `pop`, `type_of`,
file and math helpers), string operations beyond passing literals to `print`,
and any untyped function. Programs using those still run on the interpreter or
the bytecode VM.
