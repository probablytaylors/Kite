# Kite

A small language I'm building from scratch in C++. It runs `.kite` files three
ways: a tree-walking interpreter, a bytecode VM, and — for a typed subset — a
native compiler that goes through C and lands within a small factor of
hand-written C.

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

`kite update` shows the installed and latest versions, lists what changed, asks
before installing, and downloads with a progress bar. `kite update --check`
reports without installing, `kite update --yes` skips the prompt,
`kite update --list` shows every release, and `kite update <version>` installs a
specific one (including an older one).

## The language

Variables with `let`, functions with `fn`, records with `struct`, C-style
control flow:

```
struct Student { name, scores }

fn average(s) {
    let total = 0
    for (score in s.scores) {
        total += score
    }
    return total / len(s.scores)
}

let s = Student { name: "Kite", scores: [82, 91, 74] }
if (average(s) >= 80) {
    print(s.name + ": pass")
} else {
    print(s.name + ": retake")
}
```

Numbers are 64-bit integers or doubles; `int / int` truncates to an integer, a
float operand makes it a float, `%` is remainder. Strings take `+`, integer
indexing, and the `\n \t \r \0 \\ \"` escapes. There are arrays and string-keyed
maps (indexable with `m["k"]` or `m.k`, both assignable), `if`/`else if`/`else`,
C-style `for`, `for x in collection`, `while`, `break` / `continue`, `try`/`catch`
with `throw`, recursion, short-circuiting `&&` / `||`, and `#` comments.

Built-ins: `print`, `len`, `type_of`, `str`/`int`/`float`, `range`, `upper`,
`lower`, `trim`, `split`, `join`, `substring`, `replace`, `contains`,
`index_of`, `append`, `push`, `pop`, `keys`, `has`, `remove`, `read_file`,
`write_file`, `input`, `args`, `env`, and the usual math functions (`sqrt`,
`pow`, trig, `floor`, `min`, `max`, and so on). The `std/` directory has a few
more helpers written in Kite itself.

`import "std/collections"` (or your own file) merges another file's top-level
functions; the `std/` modules ship next to `kite.exe`.

See [docs/language-reference.md](docs/language-reference.md) for the full
picture, [docs/native.md](docs/native.md) for the native compiler, and
[docs/architecture.md](docs/architecture.md) for how the pieces fit together.

## Commands

```
kite run FILE               run a source file
kite run --bytecode FILE     run it on the bytecode VM instead
kite build FILE -o OUT.kbc   compile to a bytecode file
kite exec OUT.kbc            run a compiled bytecode file
kite native FILE -o OUT.exe  compile a typed program to a native executable
```

The `.kbc` format is versioned. `kite exec` checks every constant reference and
jump target against the chunk before running, so a corrupt or hand-edited file
is rejected rather than executed.

The interpreter and the bytecode VM both run the full dynamic language. `kite
native` handles a typed subset (annotated `fn` signatures, `int`/`float`/`bool`,
no collections yet) and produces an executable that runs at roughly C speed.

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
src/  include/kite/   lexer, parser, semantic pass, resolver, module loader,
                      interpreter, bytecode VM, native compiler
std/                   standard library, written in Kite
installer/             Inno Setup script and icon
docs/                  architecture and language notes
```

## Roadmap

Done: the front end, semantic pass and resolver, the interpreter, the bytecode
compiler and VM (the full dynamic language, including function call frames and
structs), `.kbc` artifacts, native compilation of a typed subset, an import
system, and the Windows installer.

Next: widening the native compiler to cover strings, collections, and structs.

## License

MIT. See [LICENSE](LICENSE).
