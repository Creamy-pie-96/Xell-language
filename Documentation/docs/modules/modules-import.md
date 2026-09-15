# Modules – Importing Modules

> **Previous:** [Defining Modules](modules-definition.md)  
> **Next:** [Module Resolution](modules-resolution.md)

---

Importing modules is how you bring code from other files and modules into your current scope. Xell's `bring` statement is flexible and powerful – it supports importing specific items, entire modules, aliasing, file‑based imports, module‑path imports, and even chaining multiple imports together.

Think of `bring` as your **gateway to code reuse** – it lets you pull in functionality from anywhere and give it a name that makes sense in your current context.

---

## Quick Reference

| Keyword/Syntax | What it does | Minimal example |
|----------------|--------------|-----------------|
| `bring Name` | Imports a module by name | `bring Math` |
| `bring X of Module` | Imports specific item `X` from a module | `bring square of Math` |
| `bring * of Module` | Imports all exported items from a module | `bring * of Math` |
| `bring X, Y of Module` | Imports multiple items | `bring square, cube of Math` |
| `bring Name as Alias` | Imports with a custom alias | `bring Math as M` |
| `bring X of Module as Alias` | Imports an item with an alias | `bring square of Math as sq` |
| `bring X from "file.xel"` | Imports from a file | `bring square from "math.xel"` |
| `bring X of Module->Path` | Imports from a nested module path | `bring parse of lib->JSON` |
| `bring X of A and Y of B` | Chains multiple imports | `bring square of A and cube of B` |
| `from "dir" bring ...` | Specifies a search directory | `from "lib" bring Math` |
| `@eager bring ...` | Forces eager loading | `@eager bring * from "heavy.xel"` |
| `bring * from "file"` | Imports all names from a file | `bring * from "utils.xel"` |

---

## Importing Modules by Name

The simplest form – bring a module into the current scope.

```xell
module Math :
    export fn square(x): give x * x ;
    export fn cube(x): give x * x * x ;
;

bring Math
print(Math->square(5))   # 25
```

- This imports the module itself, not its contents.
- Access exported items via `ModuleName->item`.
- The module must be defined or resolved from the filesystem.

---

## Importing Specific Items

Use `bring X of Module` to import specific exports directly into the current scope.

```xell
module Math :
    export fn square(x): give x * x ;
    export fn cube(x): give x * x * x ;
    fn helper(x): give x * 2 ;   # not exported
;

bring square, cube of Math
print(square(5))   # 25
print(cube(3))     # 27
# helper(5)        # ❌ error – helper is not exported
```

- You can list multiple items separated by commas.
- Each imported name becomes a variable in the current scope.
- Non‑exported items are **not** accessible.

---

## Importing All Exports

Use `bring * of Module` to import everything the module exports.

```xell
module Math :
    export fn square(x): give x * x ;
    export fn cube(x): give x * x * x ;
    export let PI = 3.14159 ;
;

bring * of Math
print(square(5))   # 25
print(PI)          # 3.14159
```

**Name collisions:** If an imported name already exists in the current scope, a `BringError` is thrown.

```xell
let square = 10
bring * of Math   # ❌ error – 'square' already exists in scope
```

Use aliases to avoid collisions:

```xell
bring * of Math as M
print(M->square(5))   # 25
```

---

## Importing with Aliases

Use `as` to rename an import.

### Module Alias

```xell
bring Math as M
print(M->square(5))   # 25
```

### Item Alias

```xell
bring square of Math as sq
print(sq(5))   # 25
```

### Multiple Aliases

```xell
bring square of Math as sq, cube of Math as cb
print(sq(5))   # 25
print(cb(3))   # 27
```

### Aliases for `*` Import

```xell
bring * of Math as M
print(M->square(5))   # 25
```

---

## Importing from Files

Use `bring X from "file.xel"` to import from a specific file.

```xell
# File: lib/math.xel
module Math :
    export fn square(x): give x * x ;
;

# Main file:
bring square from "lib/math.xel"
print(square(5))   # 25
```

- The file is parsed and executed once.
- All exported names from all modules in the file become available.
- If the file contains multiple modules, you can import from any of them.

### Importing All Names from a File

```xell
bring * from "utils.xel"
```

This imports all names from the file's global environment into the current scope. **This is different from `bring * of Module`** – it imports **everything** from the file (including non‑exported top‑level names), not just exports.

```xell
# File: utils.xel
fn helper(x): give x * 2
fn process(x): give helper(x) + 1

# Main file:
bring * from "utils.xel"
print(helper(5))   # 10  (helper is imported even though not exported!)
```

> **Warning:** `bring * from "file"` imports **all** top‑level names from the file, not just exported ones. Use this carefully to avoid polluting your namespace.

### File Path Resolution

Paths are resolved relative to the current source file's directory.

```xell
bring Math from "../lib/math.xel"
bring Parser from "./parser.xel"
```

You can also use the `from` directive to set a base directory (see below).

---

## Importing from Module Paths

Use `of` with `->` to traverse nested modules.

```xell
module lib :
    export module JSON :
        export fn parse(s): ... ;
        export fn stringify(obj): ... ;
    ;
;

bring parse of lib->JSON
print(parse('{"key": "value"}'))
```

- The path `lib->JSON` means: find module `lib`, then its submodule `JSON`.
- You can chain as many `->` as needed: `lib->JSON->Parser->parse`.

### Importing All from a Module Path

```xell
bring * of lib->JSON
print(parse('{}'))        # parse is now in scope
print(stringify({"a":1})) # stringify is now in scope
```

---

## The `from` Directive

`from "dir" bring ...` changes the search path for module resolution.

```xell
from "vendor" bring JSON
from "../shared" bring Math
```

- The directory is resolved relative to the current file.
- Modules are searched in this directory first, then the default search path.

### Combining `from` with `bring`

```xell
from "lib" bring JSON, Parser
from "lib/modules" bring Math as M
```

All items in a `from` clause are resolved in the specified directory.

### `from` with File Imports

```xell
from "vendor" bring square from "math.xel"
```

This looks for `vendor/math.xel`.

---

## Chaining Imports with `and`

Chain multiple imports in a single statement.

```xell
bring square of Math and cube of Math
# Equivalent to:
# bring square of Math
# bring cube of Math
```

**With different modules:**

```xell
bring square of Math and parse of JSON
```

**With aliases:**

```xell
bring square of Math as sq and cube of Math as cb
```

**With `from`:**

```xell
from "lib" bring Math and from "vendor" bring JSON
```

### `from` and `and` Chaining

```xell
from "lib" bring Math and Parser and from "utils" bring Helper
```

This is equivalent to:
```xell
from "lib" bring Math
from "lib" bring Parser
from "utils" bring Helper
```

---

## `@eager` – Eager Loading

By default, module resolution is **lazy** – modules are only loaded when needed. The `@eager` decorator forces immediate loading.

```xell
@eager bring * from "heavy.xel"
```

- This loads and executes the file immediately.
- Useful for modules with side effects that need to happen early.
- Also useful when you need to guarantee a module is cached before subsequent imports.

```xell
@eager bring Logger from "logger.xel"
# Logger is now loaded and cached
```

---

## Circular Import Detection

Xell detects circular imports and prevents infinite recursion.

```xell
# File: a.xel
bring B from "b.xel"   # ❌ error – circular import detected

# File: b.xel
bring A from "a.xel"   # ❌ error – circular import detected
```

If a circular import is detected, a `BringError` is thrown with a helpful message.

---

## Built‑in Modules

Some modules are built into the interpreter – they don't require files.

```xell
bring * from "threading"
thread_spawn(fn() -> print("hello"))
```

See [Built‑in Modules](modules-builtins.md) for the full list.

---

## Import Resolution Order

When you write `bring X of Y`, Xell resolves `Y` in this order:

1. **Current scope** – is `Y` defined in the local environment?
2. **Session cache** – has `Y` been imported before?
3. **Filesystem** – search for `Y` using the search path (see [Module Resolution](modules-resolution.md)).
4. **Built‑in modules** – is `Y` a built‑in module?

For `bring X from "file"`:

1. Resolve the file path (relative to current file).
2. Parse and execute the file.
3. Import the specified items from the file's environment.

---

## Complete Example: Library Import

```xell
# File: lib/geometry.xel
module Geometry :
    export module Point :
        export x = 0
        export y = 0

        export fn distance(p1, p2):
            give sqrt((p2->x - p1->x)**2 + (p2->y - p1->y)**2)
        ;
    ;

    export module Circle :
        radius = 0

        export fn area(self):
            give PI * self->radius ** 2
        ;
    ;
;

# File: main.xel
from "lib" bring Geometry as G

let p1 = G->Point({ x: 0, y: 0 })
let p2 = G->Point({ x: 3, y: 4 })
print(G->Point->distance(p1, p2))   # 5.0

# Or import specific items:
bring Point, Circle of G

let p = Point({ x: 10, y: 10 })
let c = Circle({ radius: 5 })
print(c->area())   # 78.5398...
```

**Expected output:**
```
5.0
78.5398...
```

---

## Edge Cases and Less Obvious Behavior

### 1. `bring * from "file"` Imports Everything

```xell
# File: data.xel
let secret = 42
fn helper(x): give x * 2
export fn process(x): give helper(x) + secret

# Main file:
bring * from "data.xel"
print(secret)   # 42 (imported even though not exported)
print(helper(5)) # 10 (imported even though not exported)
```

**Why:** `bring * from "file"` imports all top‑level names from the file's global environment, not just exports. Use `bring * of Module` to import only exports.

### 2. Modules Are Cached Across Imports

```xell
bring Math from "math.xel"   # loads and caches
bring Math from "math.xel"   # uses cache – file not re‑executed
```

### 3. Aliases Don't Affect the Original Module

```xell
bring Math as M
print(M->square(5))   # 25
print(Math->square(5)) # 25 (Math is still available)
```

### 4. `from` Directory Is Resolved Relative to Current File

```xell
# File: /project/main.xel
from "lib" bring Math   # looks for /project/lib/math.xel

# File: /project/sub/helper.xel
from "../lib" bring Math   # looks for /project/lib/math.xel
```

### 5. Importing Non‑Existent Items Throws

```xell
bring nonexistent of Math   # ❌ error – 'nonexistent' not found
```

### 6. Importing from a Non‑Existent File Throws

```xell
bring square from "missing.xel"   # ❌ error – cannot open file
```

### 7. `and` Chaining Preserves Order

```xell
bring A of M1 and B of M2
# A is resolved from M1, B from M2
```

### 8. `from` Applies to the Entire Chain

```xell
from "lib" bring Math and Parser
# Both Math and Parser are resolved from "lib" directory
```

---

## What NOT to Do

### 1. Importing the Same Name Twice

```xell
bring square of Math
bring square of Geometry   # ❌ error – 'square' already exists
```

**Fix:** Use an alias.

```xell
bring square of Math as m_square
bring square of Geometry as g_square
```

### 2. Using `from` Without a Directory

```xell
from bring Math   # ❌ error – 'from' requires a directory string
```

### 3. Mixing `from` and `of` Incorrectly

```xell
bring Math from "lib"   # ❌ error – 'from' must precede 'bring'
```

**Correct:**
```xell
from "lib" bring Math
```

### 4. Using `as` Without a Name

```xell
bring Math as   # ❌ error – missing alias name
```

### 5. Importing All from a File with Side Effects

```xell
bring * from "setup.xel"   # ❌ may execute code with side effects unexpectedly
```

**Better:** Import only what you need, or use `bring * of Module` to import only exports.

### 6. Forgetting That `bring * from "file"` Imports Non‑Exports

```xell
# File: data.xel
let config = { "mode": "debug" }
fn helper(x): give x * 2
export fn process(x): give helper(x)

# Main file:
bring * from "data.xel"
print(config)   # ❌ config is imported even though not exported – may cause pollution
```

**Better:** Use `bring * of Module` or explicitly import only what you need.

### 7. Expecting `bring` to Update When Source Changes

```xell
bring Math from "math.xel"   # loads file
# Edit math.xel
bring Math from "math.xel"   # ❌ still uses cached version
```

**Fix:** Restart the interpreter or use a development mode.

### 8. Using `bring * from "file"` in a Module

```xell
module App :
    bring * from "utils.xel"   # ❌ imports into module scope – may cause unexpected side effects
;
```

**Better:** Use `bring * of Module` or explicit imports.

### 9. Aliasing a Module to a Keyword

```xell
bring Math as if   # ❌ error – 'if' is a reserved keyword
```

### 10. Importing a Module That Doesn't Exist

```xell
bring NonExistent   # ❌ error – module not found
```

---

## See Also

- [Defining Modules](modules-definition.md) – how to create modules
- [Module Resolution](modules-resolution.md) – how modules are found
- [Built‑in Modules](modules-builtins.md) – standard library modules
- [Bring Statement](#) – detailed `bring` syntax

---

**Previous:** [Defining Modules](modules-definition.md)  
**Next:** [Module Resolution](modules-resolution.md)