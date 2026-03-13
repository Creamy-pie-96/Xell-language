# Modules and Imports

This section explains how to split code into modules and import it safely.

## Basic `bring`

```xell
bring "math_utils.xel"
```

## Selective import

```xell
from "math_utils.xel" import add, sub
print(add(2, 3))
```

## Alias import

```xell
from "math_utils.xel" import add as plus
print(plus(1, 2))
```

## Wildcard import

```xell
from "math_utils.xel" import * as M
print(M->add(5, 6))
```

## Module definition

```xell
module math_utils:
    fn add(a, b):
        give a + b
    ;
;
```

## Export declaration

```xell
export fn add(a, b):
    give a + b
;

export class Counter:
    value = 0
;
```

## `requires` declaration

```xell
module app:
    requires math, fs
;
```

You can also require paths/chains depending on your module shape.

```xell
module app:
    requires math -> algebra, geometry
;
```

## Eager loading with `@eager`

By default imports may be lazy. Use `@eager` to force immediate load.

```xell
@eager bring "config.xel"
@eager bring "db.xel"
```

## Module metadata

Module objects expose metadata such as:

- `__name__`
- `__exports__`
- `__submodules__`
- `__module__`
- `__version__`
- `__cached__`
- `__args__`

```xell
bring "my_mod.xel"
print(my_mod->__name__)
print(my_mod->__exports__)
```

## Practical pattern

```xell
# file: lib/math.xel
export fn sq(x):
    give x * x
;

# file: main.xel
from "lib/math.xel" import sq
print(sq(9))
```
