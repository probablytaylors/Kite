# Standard Library

These files are the first Kite standard-library modules:

- `math.kite`: `square`, `cube`, `hypotenuse`, and `clamp`.
- `strings.kite`: `shout`, `quiet`, `join`, and `has_text`.
- `collections.kite`: `first`, `last`, and `is_empty`.
- `io.kite`: `read_text` and `write_text`.

They use the current language and existing built-ins. The module loader is not
implemented yet, so these files are standalone Kite source and are not imported
automatically by programs.

The planned import syntax is:

```kite
import math

print(math.square(5))
```

That syntax is documentation for the future module system, not an implemented
feature.
