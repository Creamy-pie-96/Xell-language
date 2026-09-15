# Modules – Defining Modules

> **Previous:** [Magic Methods](../oop/oop-magic-methods.md)  
> **Next:** [Importing Modules](modules-import.md)

---

Modules are Xell's **unit of code organisation and distribution**. They let you group related functions, classes, structs, and other definitions into reusable containers. Modules are **first‑class** – they can be passed around, stored in variables, and even nested inside each other.

Think of modules as **named namespaces** – they keep your code organised, avoid name collisions, and enable code reuse across files.

---

## Quick Reference

| Keyword/Syntax | What it does | Minimal example |
|----------------|--------------|-----------------|
| `module name : body ;` | Defines a module | `module Math : fn square(x): give x*x ; ;` |
| `export` | Exports a declaration from the module | `export fn square(x): give x*x ;` |
| `requires name` | Declares a dependency on another module | `requires JSON` |
| `requires X, Y of module->path` | Imports specific items from a module path | `requires parse, stringify of JSON` |
| `__name__` | Module's name (dunder) | `Math->__name__` → `"Math"` |
| `__file__` | Source file path | `Math->__file__` → `"/path/to/math.xel"` |
| `__version__` | User‑set version string | `Math->__version__` → `"1.2.0"` |
| `__exports__` | List of exported names | `Math->__exports__` → `["square", "cube"]` |
| `__submodules__` | List of nested submodule names | `Math->__submodules__` → `["Geometry"]` |
| `mod->exported_name` | Accesses an exported member | `Math->square(5)` |

---

## Defining a Module

Use the `module` keyword, followed by the name, a colon, and a block containing definitions.

```xell
module MathUtils :
    fn square(x):
        give x * x
    ;

    fn cube(x):
        give x * x * x
    ;

    fn factorial(n):
        if n <= 1: give 1
        give n * factorial(n - 1)
    ;
;
```

- The module body is a block (`:` … `;`).
- Inside the module, you can define functions, classes, structs, enums, and even nested modules.
- Variables defined inside the module are **local** to the module unless exported.
- The module name becomes a variable in the current scope – a `module` object.

### Accessing Module Members

By default, **no members are exported**. To access them, you must explicitly `export` them, or bring the module into scope and access via `->`.

```xell
# Using the module directly:
let m = MathUtils
# m->square(5)   # ❌ error – square is not exported

# You can still use it by bringing it into scope:
bring square, cube of MathUtils
print(square(5))   # 25
```

---

## Exporting Declarations

Use `export` inside the module body to make declarations visible outside.

```xell
module MathUtils :
    export fn square(x):
        give x * x
    ;

    export fn cube(x):
        give x * x * x
    ;

    # Not exported – only available inside the module
    fn helper(x):
        give x * 2
    ;
;

# Now accessible:
bring square, cube of MathUtils
print(square(5))   # 25
print(cube(3))     # 27
```

### What Can Be Exported

- Functions (`fn` and `async fn`)
- Classes (`class`)
- Structs (`struct`)
- Enums (`enum`)
- Modules (`module`)
- Variables (`let` and `immutable`)

```xell
module Data :
    export fn process(x): give x * 2 ;

    export class Parser :
        fn parse(self, s): give to_int(s) ;
    ;

    export struct Point :
        x = 0
        y = 0
    ;

    export immutable VERSION = "1.0.0"

    export module SubModule :
        export fn hello(): print("Hello") ;
    ;
;
```

### Exporting Variables

```xell
module Config :
    export let debug = false
    export immutable MAX_SIZE = 1024

    fn set_debug(val):
        debug = val   # ✅ modifies the exported variable
    ;
;
```

Variables exported from a module are **mutable** by default (unless `immutable`). Changes made inside the module are visible to all importers.

---

## `requires` – Module Dependencies

Modules can declare dependencies using `requires` at the top of their body.

### Simple Dependency

```xell
module MyApp :
    requires JSON
    requires Math

    export fn process(data):
        let parsed = JSON->parse(data)
        give Math->sqrt(parsed["value"])
    ;
;
```

- `requires JSON` brings the module `JSON` into scope **inside** the module.
- If the required module is not found, a `RequireError` is thrown when the module is defined.
- Dependencies are resolved using the same module resolution logic as `bring`.

### Item‑Specific Dependency

Import specific items from a module path:

```xell
module MyApp :
    requires parse, stringify of JSON
    requires sqrt, PI of Math

    export fn process(data):
        let parsed = parse(data)
        give parsed["value"] * PI
    ;
;
```

- `requires parse, stringify of JSON` imports only `parse` and `stringify` from the `JSON` module.
- This is equivalent to `bring parse, stringify of JSON` inside the module's environment.

### Module Path Resolution

Dependencies can use module paths with `->`:

```xell
module MyApp :
    requires parse of lib->JSON
    requires sqrt of Math->Core
;
```

---

## Module Metadata (Dunders)

Modules have several built‑in fields (dunders) that provide metadata.

### `__name__`

The module's name.

```xell
module MathUtils :
    export fn get_name(self):   # not needed – accessible directly
        give MathUtils->__name__
    ;
;

print(MathUtils->__name__)   # "MathUtils"
```

### `__file__`

The absolute path to the source file.

```xell
module MathUtils :
    # Inside the module:
    print(MathUtils->__file__)   # "/home/user/project/math.xel"
;

print(MathUtils->__file__)   # "/home/user/project/math.xel"
```

### `__version__`

User‑set version string – set it like any other field.

```xell
module MathUtils :
    let __version__ = "1.2.0"
;

print(MathUtils->__version__)   # "1.2.0"
```

### `__exports__`

A list of all exported names.

```xell
module MathUtils :
    export fn square(x): give x*x ;
    export fn cube(x): give x*x*x ;
    let __version__ = "1.0"   # not exported
;

print(MathUtils->__exports__)   # ["square", "cube"]
```

### `__submodules__`

A list of nested submodule names.

```xell
module Parent :
    export module Child :
        export fn hello(): print("Hello") ;
    ;
;

print(Parent->__submodules__)   # ["Child"]
```

### `__module__`

The parent module name (for submodules).

```xell
module Parent :
    export module Child :
        print(Child->__module__)   # "Parent"
    ;
;
```

### `__cached__`

Path to the `.xelc` bytecode cache (future feature – currently returns `none`).

```xell
print(MathUtils->__cached__)   # none (not cached yet)
```

---

## First‑Class Modules

Modules are **first‑class values** – they can be stored, passed, and returned.

```xell
module Logger :
    export fn log(msg): print("[LOG] " + msg) ;
;

module App :
    let logger = Logger   # store module in a variable

    export fn run():
        logger->log("App starting")
    ;
;

App->run()   # [LOG] App starting
```

### Modules in Collections

```xell
module A ; module B ; module C ;

let modules = [A, B, C]
for mod in modules:
    print(mod->__name__)
;
```

**Expected output:**
```
A
B
C
```

---

## Nested Modules

Modules can be nested inside other modules.

```xell
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

# Access nested modules:
bring Point of Geometry
let p1 = Point({ x: 0, y: 0 })
let p2 = Point({ x: 3, y: 4 })
print(Point->distance(p1, p2))   # 5.0
```

- Nested modules are accessible via `Parent->Child`.
- They can be exported (like `Point` and `Circle` above).
- When exported, they are listed in `__submodules__`.

---

## Modules and Scoping

### Module Body Executes in Its Own Environment

The module body executes in a separate environment (child of the current scope). This means:

- Variables defined inside the module are **local** to the module.
- They do **not** leak into the outer scope unless exported.
- The module environment is **owned** by the module object and stays alive as long as the module does.

```xell
module Counter :
    let count = 0   # local to module

    export fn increment():
        count += 1   # ✅ modifies module's local count
    ;

    export fn get_count():
        give count
    ;
;

# count is NOT accessible here:
# print(count)   # ❌ error – count is not defined

Counter->increment()
Counter->increment()
print(Counter->get_count())   # 2
```

### Closure Capture

Functions inside a module capture the module's environment, allowing closures that access module‑local state.

```xell
module Factory :
    let next_id = 0

    export fn create(name):
        next_id += 1
        give { "id": next_id, "name": name }
    ;
;

let item1 = Factory->create("Widget")
let item2 = Factory->create("Gadget")
print(item1["id"])   # 1
print(item2["id"])   # 2
```

---

## Module Lifecycle

1. **Definition:** The `module` statement is encountered.
2. **Dependency Resolution:** `requires` are processed (modules are resolved and brought into scope).
3. **Body Execution:** The module body executes in a fresh environment.
4. **Export Collection:** All `export` declarations are collected into the module's export table.
5. **Binding:** The module object is bound to the module name in the current scope.
6. **Caching:** The module is cached by the module resolver for future `bring` lookups.

If a module is `bring`‑ed from a file, it is executed once and cached for the entire session.

---

## Complete Example: User Authentication Module

```xell
module Auth :
    requires hashme
    requires random_int

    private:
        users = {}
        sessions = {}

    export fn register(username, password):
        if username in users :
            throw ValueError("User already exists")
        let salt = random_int(1000)
        let hashed = hashme(password + str(salt))
        users[username] = { "password": hashed, "salt": salt }
        give true
    ;

    export fn login(username, password):
        if username not in users :
            throw KeyError("User not found")
        let user = users[username]
        let hashed = hashme(password + str(user["salt"]))
        if hashed != user["password"] :
            throw RuntimeError("Invalid password")
        let session_id = hashme(username + str(now()))
        sessions[session_id] = username
        give session_id
    ;

    export fn logout(session_id):
        if session_id not in sessions :
            throw KeyError("Invalid session")
        sessions.remove(session_id)
        give true
    ;

    export fn is_authenticated(session_id):
        give session_id in sessions
    ;

    export fn get_user(session_id):
        if session_id not in sessions :
            throw KeyError("Invalid session")
        give sessions[session_id]
    ;

    export immutable VERSION = "2.0.0"
;

# Usage:
bring register, login, logout, is_authenticated of Auth

register("alice", "secret123")
let session = login("alice", "secret123")
print(is_authenticated(session))   # true

logout(session)
print(is_authenticated(session))   # false

print(Auth->VERSION)   # "2.0.0"
```

**Expected output:**
```
true
false
2.0.0
```

---

## Edge Cases and Less Obvious Behavior

### 1. Modules Are Cached Once Per Session

```xell
# In file math.xel:
module Math :
    export fn square(x): give x*x ;
    print("Math module loaded")
;

# In main file:
bring Math from "math.xel"   # prints "Math module loaded"
bring Math from "math.xel"   # uses cache – does NOT print again
```

### 2. Exporting Nothing Exports Everything

If a module has **no** `export` declarations, **all** top‑level definitions in the module are exported.

```xell
module Everything :
    fn square(x): give x*x ;      # exported implicitly
    fn cube(x): give x*x*x ;      # exported implicitly
    let VERSION = "1.0"           # exported implicitly
;

bring * of Everything
print(square(5))   # 25
```

### 3. Exporting a Module Exports It as a Submodule

```xell
module Parent :
    export module Child :
        export fn hello(): print("Hello") ;
    ;
;

bring Child of Parent
Child->hello()   # Hello
```

### 4. Module Names Are Case‑Sensitive

```xell
module Math ;
module math ;   # different module
```

### 5. Circular Module Dependencies

Xell detects circular dependencies and prevents infinite recursion.

```xell
# File a.xel:
module A :
    requires B   # ❌ error – circular dependency
;

# File b.xel:
module B :
    requires A   # ❌ error – circular dependency
;
```

### 6. Modules Can Have No Body

```xell
module Empty ;   # valid – empty module
```

### 7. `__version__` Must Be Set Explicitly

Xell does not automatically provide a version – you must define `__version__` in the module body.

```xell
module Math :
    let __version__ = "1.2.0"   # sets the version
;
```

---

## What NOT to Do

### 1. Forgetting to `export` Definitions

```xell
module Math :
    fn square(x): give x*x ;   # ❌ not exported – inaccessible outside
;

# Math->square(5)   # ❌ error – square is not exported
```

**Fix:** Add `export` before the definition.

### 2. Exporting a Variable and Then Reassigning It

```xell
module Config :
    export let debug = false
;

Config->debug = true   # ✅ allowed – exported variables are mutable
```

If you want immutability, use `immutable`.

### 3. Using a Module Before It's Defined

```xell
bring square of Math   # ❌ error – Math not defined yet
module Math :
    export fn square(x): give x*x ;
;
```

**Fix:** Define the module before using it.

### 4. Expecting `requires` to Bring Things into Global Scope

```xell
module App :
    requires JSON
;

# JSON is NOT available in global scope – only inside the module
# print(JSON)   # ❌ error – JSON not defined in global scope
```

**Fix:** Use `bring` in global scope, or access via the module that required it.

### 5. Creating a Module with the Same Name as a Built‑in

```xell
module Math :   # ❌ shadows the built‑in Math module
    export fn square(x): give x*x ;
;
```

**Why it's bad:** It overrides the built‑in Math module, losing access to its constants and functions.

### 6. Assuming Module State Is Isolated Per Importer

```xell
module Counter :
    let count = 0
    export fn increment(): count += 1 ;
    export fn get_count(): give count ;
;

# Both importers share the SAME module instance
bring increment, get_count of Counter
increment()
print(get_count())   # 1

bring Counter as C
C->increment()
print(C->get_count())   # 2 (same counter!)
```

**Why:** Modules are cached globally – all importers share the same module object.

### 7. Using `export` on a Type That Hasn't Been Defined

```xell
module Data :
    export fn process(self):   # ❌ error – Point not defined yet
        give Point(self->x, self->y)
    ;

    export struct Point :      # Defined after use – fails
        x = 0
        y = 0
    ;
;
```

**Fix:** Define types before using them, or use forward declarations.

### 8. Exporting Without a Name

```xell
module Bad :
    export   # ❌ error – nothing to export
;
```

### 9. Assuming Modules Are Automatically Reloaded on Change

```xell
bring Math from "math.xel"   # loads and caches
# Edit math.xel
bring Math from "math.xel"   # ❌ still uses cached version
```

**Fix:** Restart the interpreter to reload modules, or use a development mode that invalidates the cache.

---

## See Also

- [Importing Modules](modules-import.md) – how to use modules in other files
- [Module Resolution](modules-resolution.md) – how modules are found
- [Built‑in Modules](modules-builtins.md) – standard library modules
- [Bring Statement](modules-import.md#bring-syntax) – detailed `bring` syntax

---

**Previous:** [Magic Methods](../oop/oop-magic-methods.md)  
**Next:** [Importing Modules](modules-import.md)