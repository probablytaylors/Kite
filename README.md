# Kite

A small scripting language I'm building from scratch in C++. It's dynamically
typed and runs `.kite` files through a tree-walking interpreter, with a bytecode
VM coming together alongside it.

```
fn fib(n) {
    if (n < 2) { return n }
    return fib(n - 1) + fib(n - 2)
}

for (let i = 0; i < 10; i += 1) {
    print(fib(i))
}
```

## Install

Windows only for now. Download `kite-setup.exe` from the
[releases page](https://github.com/probablytaylors/Kite/releases) and run it.
It puts `kite.exe` in `Program Files`, adds it to `PATH`, and associates
`.kite` files.

```
kite --version
kite run your-program.kite
```

`kite update` fetches the latest release and installs it in place; `kite update
--check` just reports whether a newer version is available.

## The language

Variables with `let`, functions with `fn`, C-style control flow:

```
let name = "Kite"
let scores = [82, 91, 74]
let total = 0

for (let i = 0; i < len(scores); i += 1) {
    total += scores[i]
}

if (total / len(scores) >= 80) {
    print(name + ": pass")
} else {
    print(name + ": retake")
}
```

Numbers are 64-bit integers or doubles; `/` always gives a double, `%` is
remainder. Strings take `+` and the `\n \t \r \0 \\ \"` escapes. There are
arrays, string-keyed maps, `if`/`else if`/`else`, `while` and `for`, recursion,
and short-circuiting `&&` / `||`.

Built-ins: `print`, `len`, `upper`, `lower`, `type_of`, `append`, `pop`,
`read_file`, `write_file`, and the usual math functions (`sqrt`, `pow`, trig,
`floor`, `min`, `max`, and so on). The `std/` directory has a few more helpers
written in Kite itself.

See [docs/language-reference.md](docs/language-reference.md) for the full
picture and [docs/architecture.md](docs/architecture.md) for how the
implementation fits together.

## Commands

```
kite run FILE               run a source file
kite run --bytecode FILE     run it on the bytecode VM instead
kite build FILE -o OUT.kbc   compile to a bytecode file
kite exec OUT.kbc            run a compiled bytecode file
```

The `.kbc` format is versioned. `kite exec` checks every constant reference and
jump target against the chunk before running, so a corrupt or hand-edited file
is rejected rather than executed.

The interpreter runs everything. The bytecode VM covers most of the language
except user-defined function calls, which still fall back to the interpreter.

## Building from source

You need CMake and Visual Studio 2022 (C++20).

```
cmake -S . -B build -A x64
cmake --build build
```

`build\Debug\kite.exe` is the result. Pass `-D KITE_WERROR=ON` to treat
warnings as errors. To rebuild the installer you also need
[Inno Setup](https://jrsoftware.org/isinfo.php):

```
cmake --build build --config Release
iscc installer\kite.iss
```

## Layout

```
src/  include/kite/   lexer, parser, semantic pass, interpreter, bytecode VM
std/                   standard library, written in Kite
installer/             Inno Setup script and icon
docs/                  architecture and language notes
```

## Roadmap

Done: the front end and semantic pass, the interpreter, the bytecode compiler
and VM, `.kbc` artifacts, and the Windows installer.

Next: function call frames in the VM, map mutation, an import system, and
further out, a static type system with native compilation.

## License

MIT. See [LICENSE](LICENSE).
