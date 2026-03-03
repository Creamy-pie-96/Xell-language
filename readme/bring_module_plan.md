# Bring / Module System Plan for Xell

> **Status:** Confirmed Design — ready for implementation.
> **Date:** 2026
> **Note:** Xell's module system is file-decoupled, metadata-driven, and
> smart-cached. Multiple modules per file, fully nested, explicit exports,
> and a Python-style object model — but faster due to metadata indexing.

---

## Table of Contents

1. [Design Philosophy](#1-design-philosophy)
2. [New Keywords](#2-new-keywords)
3. [Defining Modules](#3-defining-modules)
4. [Export Rules](#4-export-rules)
5. [Import Syntax — All Forms](#5-import-syntax--all-forms)
6. [The `and` Chain](#6-the-and-chain)
7. [Module Path Separator (`->`)](#7-module-path-separator--)
8. [Accessing After Import](#8-accessing-after-import)
9. [Smart Import & Caching](#9-smart-import--caching)
10. [`__xelcache__` Layout](#10-__xelcache__-layout)
11. [Module as First-Class Object](#11-module-as-first-class-object)
12. [Metadata System — `.xell_meta`](#12-metadata-system--xell_meta)
13. [`xell --make_module` Command](#13-xell---make_module-command)
14. [Runtime Search Algorithm](#14-runtime-search-algorithm)
15. [Cache Invalidation](#15-cache-invalidation)
16. [`bring *` — Dump All](#16-bring---dump-all)
17. [Conflict Resolution](#17-conflict-resolution)
18. [Grammar Reference](#18-grammar-reference)
19. [Full End-to-End Example](#19-full-end-to-end-example)
20. [Summary: All Files Introduced](#20-summary-all-files-introduced)
21. [Implementation Roadmap](#21-implementation-roadmap)
22. [`requires` Keyword](#22-requires-keyword)
23. [`@eager` Decorator](#23-eager-decorator)
24. [Module Dunder Variables (`__name__` etc.)](#24-module-dunder-variables)
25. [Appendix A: Comparison with Python and C++](#25-appendix-a-comparison-with-python-and-c)
26. [Appendix B: What We're NOT Doing](#26-appendix-b-what-were-not-doing)

---

## 1. Design Philosophy

- **File-decoupled** — a file can hold any number of modules. The filename is irrelevant to the
  module name. No forced `file = module` constraint like Python or C++.
- **Explicit exports** — nothing leaks out of a module unless you write `export`. Privacy by default.
- **Smart, not dumb** — NOT a text-include like C++. Modules are parsed, compiled, cached, and
  returned as live objects. Executed once per session.
- **Metadata-indexed** — blind file scanning is never needed. A `.xell_meta` index file per
  directory maps module names to files, making lookup instant even across large codebases.
- **First-class** — a brought module is a real Xell object. It can be stored, passed, inspected.
- **Self-healing cache** — source file changes are detected automatically via hash comparison.
  No manual cache clearing ever needed.

---

## 2. New Keywords

| Keyword    | Role                                                           |
| ---------- | -------------------------------------------------------------- |
| `module`   | Declares a named module block (top-level or nested)            |
| `export`   | Marks a declaration as publicly importable from outside        |
| `bring`    | Imports a module or parts of a module (replaces old `bring`)   |
| `from`     | Specifies a directory to search first before the default path  |
| `of`       | Separates what you're bringing from which module it comes from |
| `as`       | Gives an alias to brought module(s) or item(s)                 |
| `and`      | Chains multiple bring targets in a single statement            |
| `requires` | Declares a module dependency — auto-brought before the module  |

> `of`, `and` and `as` were already in Xell. `from`, `module`, `export`, `requires` are new additions.

---

## 3. Defining Modules

### Key rules

- Any `.xell` file can contain **any number** of top-level `module` blocks.
- Modules can be **nested** as deeply as needed — a module inside a module is just a `module`
  block inside another `module` block. No special keyword for sub-modules.
- The **filename does not matter** — module identity comes from the `module name :` declaration.
- Everything inside a module is **private by default**. Only `export` declarations are visible outside.

### Syntax

```xell
# file: anything.xell — filename is irrelevant

module lib :

    module math_lib :
        export fn add(a, b) : give a + b ;
        export fn sub(a, b) : give a - b ;

        fn _helper(x) : give x * 2 ;    # private — no export
    ;

    module string_lib :
        export fn shout(s) : give upper(s) ;
        export fn whisper(s) : give lower(s) ;
    ;

    export PI = 3.14159                  # exported variable
    export class Vector : ... ;          # exported class

    fn _internal() : ... ;              # private function, invisible outside
;

module another_lib :
    export fn greet(name) : print "hello {name}" ;
;
```

### Nesting diagram

```
anything.xell
├── module lib
│   ├── module math_lib
│   │   ├── export add       ← importable as lib->math_lib->add
│   │   ├── export sub       ← importable as lib->math_lib->sub
│   │   └── _helper          ← private, not importable
│   ├── module string_lib
│   │   ├── export shout
│   │   └── export whisper
│   ├── export PI
│   ├── export Vector
│   └── _internal            ← private
└── module another_lib
    └── export greet
```

---

## 4. Export Rules

| What you write             | What it means                                                         |
| -------------------------- | --------------------------------------------------------------------- |
| `export fn foo()`          | `foo` is importable from outside                                      |
| `export class Bar`         | `Bar` is importable from outside                                      |
| `export x = 10`            | Variable `x` is importable from outside                               |
| `export module math_lib :` | Entire submodule is importable, including all its own exports         |
| `fn foo()` (no export)     | Completely private — invisible to `bring`, not even in error messages |
| `module sub :` (no export) | Private submodule — cannot be brought in                              |

```xell
module lib :
    export module math_lib :      # whole submodule exposed
        fn add(a, b) : give a + b ;
        fn sub(a, b) : give a - b ;
    ;

    module _internals :           # no export — completely hidden
        fn helper() : ... ;
    ;

    export PI = 3.14159
    export class Vector : ... ;
;
```

---

## 5. Import Syntax — All Forms

### Simplest — bring a whole module

```xell
bring lib
lib->math_lib->add(1, 2)
```

### With alias

```xell
bring lib as l
l->math_lib->add(1, 2)
```

### Multiple modules with aliases

```xell
bring lib, json, regex as l, j, r
```

### From a specific search directory

```xell
from "./libs" bring lib as l -->the search dir will be reletive to the current code file(like the code file we wrote the bring in)
```

### Specific submodule aliased

```xell
bring math_lib of mylib->lib as math  (the module structure is like mylib-->lib-->math_lib)
math->add(1, 2)
```

### Specific functions / items

```xell
bring add, sub of lib->math_lib as a, s
a(1, 2)
s(5, 3)
```

### Dump everything — namespaced

```xell
bring * of lib->math_lib as math
math->add(1, 2)
```

### Dump everything — into global scope

```xell
bring * of lib->math_lib        # add, sub, PI land in global scope
```

> ⚠️ **Warning emitted in script mode.** Suppress with `@no_warn`. See [§16](#16-bring---dump-all).

### Full syntax — every keyword used

```xell
from "./libs" bring add, sub, Vector of lib->math_lib as a, s, V
```

---

## 6. The `and` Chain

When you need to bring from multiple modules in one statement, chain with `and`.
All aliases line up left to right across all `and` segments.

```xell
# Single from, multiple modules
from "./libs" bring add, sub, Vector of lib->math_lib
             and gravity, solar of physics->mechanics as a, s, V, g, so

# Different from per segment
from "./libs"   bring add of lib->math_lib as a
and from "./vendor" bring gravity of physics_lib as g

# Mix of specific and whole-module bring
bring json as j
and bring add, sub of lib->math_lib as a, s
```

### Alias alignment rule

The `as` list at the end maps left-to-right to every brought item across all `and` segments, in
order. If the count of aliases doesn't match the count of imported items, it's a parse-time error.

```xell
# Correct — 4 items, 4 aliases
from "./libs" bring add, sub of lib->math_lib
             and gravity, solar of physics as a, s, g, so

# Error — 4 items, 3 aliases
from "./libs" bring add, sub of lib->math_lib
             and gravity, solar of physics as a, s, g
# ParseError: 4 items brought but only 3 aliases given
```

---

## 7. Module Path Separator (`->`)

Module path navigation uses `->`, consistent with all other field/method access in Xell.

```xell
# After import
lib->math_lib->add(1, 2)          # access nested module then call function
my_lib->PI                         # access exported variable
my_lib->Vector(1, 2)               # instantiate exported class

# In bring statements — -> for nested path
bring add of lib->math_lib as a    # lib -> math_lib -> add
```

---

## 8. Accessing After Import

```xell
# Full path access — no alias
bring lib
lib->math_lib->add(1, 2)
lib->PI

# Aliased top-level
bring lib as l
l->math_lib->add(1, 2)

# Submodule brought directly
bring math_lib of lib as m
m->add(1, 2)
m->sub(5, 3)

# Specific functions brought in
bring add, sub of lib->math_lib as a, s
a(1, 2)
s(5, 3)

# Whole module dumped into namespace
bring * of lib->math_lib as math
math->add(1, 2)

# Whole module dumped into global scope (warns)
bring * of lib->math_lib
add(1, 2)    # available directly
```

---

## 9. Smart Import & Caching

Xell's module system is **smart import** — not a dumb text inclusion like C++.

### What happens when you `bring lib`

```
1. RESOLVE — build the search path:
   - If from "dir" given → prepend it
   - Add current directory
   - Add all dirs in XELL_PATH environment variable
   - Add stdlib built-in modules

2. LOCATE — walk each directory in search path:
   - Does .xell_meta exist here?
     YES → O(1) lookup: find module_name in index → get filename
     NO  → SLOW PATH: scan all .xell files in directory, parse headers
           emit warning: "no .xell_meta found, run xell --make_module ./dir/"

3. CACHE CHECK:
   - Does __xelcache__/<filename>.xelc exist?
     YES → compare stored hash in .xelc.hash vs current file SHA
           hash matches → load bytecode directly (instant)
           hash mismatch → recompile, update .xelc and .xelc.hash

4. EXECUTE — run the module file once:
   - Top-level module code executes
   - All export declarations registered
   - Module object constructed

5. SESSION CACHE:
   - Module object stored in session registry keyed by (path + module_name)
   - Subsequent brings of same module → return cached object, zero re-execution

6. RETURN module object to caller
```

### Key guarantee: a module file is **executed at most once per session** regardless of how many

times it is brought.

---

## 10. `__xelcache__` Layout

Cache lives next to the source file in a `__xelcache__/` subdirectory.

```
project/
├── main.xell
├── utils.xell
├── libs/
│   ├── science.xell
│   ├── math.xell
│   ├── .xell_meta                    ← directory module index
│   └── __xelcache__/
│       ├── science.xelc              ← compiled bytecode
│       ├── science.xelc.hash         ← SHA of source for invalidation
│       ├── math.xelc
│       └── math.xelc.hash
├── .xell_meta                        ← root directory index
└── __xelcache__/
    ├── utils.xelc
    └── utils.xelc.hash
```

### File types

| File          | Location        | Contents                                       |
| ------------- | --------------- | ---------------------------------------------- |
| `.xell_meta`  | Each directory  | JSON index: module names → files + export list |
| `*.xelc`      | `__xelcache__/` | Compiled Xell bytecode                         |
| `*.xelc.hash` | `__xelcache__/` | SHA-256 of source file, used for invalidation  |

### `.gitignore` rules

```gitignore
__xelcache__/       # never commit — auto-regenerated
```

```gitignore
# DO commit
.xell_meta          # like package-lock.json — commit it
```

---

## 11. Module as First-Class Object

A brought module is a **real Xell object** with inspectable properties.

```xell
bring lib as l

# Inspect
print type(l)              # "module"
print l->__name__          # "lib"
print l->__path__          # "/path/to/anything.xell"
print l->__exports__       # ["add", "sub", "PI", "Vector", "math_lib"]

# Pass around
fn use_module(m) :
    give m->add(1, 2)
;
use_module(l->math_lib)

# Store in map
modules = {
    math: l->math_lib,
    strings: l->string_lib
}
modules->math->add(1, 2)

# Check if module has something
"add" in l->math_lib->__exports__    # true
```

### Module object fields

| Field            | Type   | Meaning                                                  |
| ---------------- | ------ | -------------------------------------------------------- |
| `__name__`       | string | `"lib->math_lib"` normally; `"__main__"` if run directly |
| `__path__`       | string | Absolute path to source file                             |
| `__file__`       | string | Alias for `__path__` — same value                        |
| `__exports__`    | list   | Names of all exported items                              |
| `__submodules__` | list   | Names of exported submodules                             |
| `__version__`    | string | Optional — set by module author; `none` if not declared  |
| `__module__`     | string | Parent module name; `none` for top-level modules         |
| `__cached__`     | string | Absolute path to `.xelc` bytecode cache file             |
| `__args__`       | list   | CLI argv when run directly; `none` when brought          |

---

## 12. Metadata System — `.xell_meta`

One `.xell_meta` file per directory. It is a JSON index that maps module names to source files
and records their export lists. This makes runtime lookup O(1) — no file scanning needed.

### Format

```json
{
  "xell_meta_version": 1,
  "generated": "2026-03-03T10:00:00",
  "directory": "./libs",
  "modules": {
    "lib": {
      "file": "math.xell",
      "hash": "a3f9c2d1e7b4f0...",
      "exports": ["add", "sub", "PI", "Vector", "math_lib", "string_lib"],
      "submodules": {
        "math_lib": {
          "exports": ["add", "sub"]
        },
        "string_lib": {
          "exports": ["shout", "whisper"]
        }
      }
    },
    "another_lib": {
      "file": "math.xell",
      "hash": "a3f9c2d1e7b4f0...",
      "exports": ["greet"],
      "submodules": {}
    },
    "physics": {
      "file": "science.xell",
      "hash": "b7d1e4a9c2f3...",
      "exports": ["gravity", "solar"],
      "submodules": {
        "mechanics": {
          "exports": ["force", "mass"]
        }
      }
    }
  }
}
```

### Notes

- Multiple modules from the same file appear as separate top-level entries — no code duplication.
- The `hash` in `.xell_meta` mirrors the `.xelc.hash` — used to detect whether a rebuild is needed
  without even opening the source file.
- `.xell_meta` is human-readable JSON — easy to inspect, diff, and commit to version control.

---

## 13. `xell --make_module` Command

This is the tool that registers modules and builds all metadata and bytecode cache.

### Usage

```bash
# Register specific files
xell --make_module mylib.xell physics.xell

# Register all .xell files in current directory
xell --make_module *.xell

# Register entire directory (recursive)
xell --make_module ./libs/

# Incremental update — only reprocess changed files
xell --make_module --update ./libs/
```

### What it does — step by step

```
For each .xell file given:
    1. Lex + parse the file
    2. Find all top-level module declarations
    3. Recursively find all nested module blocks
    4. Collect all export declarations at each level
    5. Compute SHA-256 hash of file contents
    6. Write / update entry in .xell_meta in the same directory
    7. Compile AST to bytecode → __xelcache__/<file>.xelc
    8. Write hash → __xelcache__/<file>.xelc.hash
```

### `--update` mode

```
For each .xell file in target:
    - Compare current SHA vs stored hash in .xell_meta
    - If changed → reprocess that file only
    - If unchanged → skip entirely
```

Fast incremental rebuild — suitable for CI/CD and build scripts.

### On first `bring` without metadata

If `.xell_meta` is absent, the runtime performs a **slow path scan** and emits a warning:

```
Warning: no .xell_meta found in ./libs/
         scanning all .xell files (slow). Run:
             xell --make_module ./libs/
         to index this directory.
```

The bring still works — just slower. This is the graceful degradation path.

---

## 14. Runtime Search Algorithm

```
SEARCH(module_name, from_dirs=[], session_cache):

Step 1 — session cache hit?
    If (file_path, module_name) in session_cache:
        → return cached module object immediately (zero I/O)

Step 2 — build search path:
    path = from_dirs + [current_dir] + XELL_PATH_dirs + [stdlib_dir]

Step 3 — walk search path:
    For each dir in path:
        a. Does dir/.xell_meta exist?
              YES → look up module_name in JSON index
                    found → note file = meta["modules"][module_name]["file"]
                             go to Step 4
                    not found → continue to next dir
              NO  → SLOW PATH: scan *.xell files in dir
                    parse each for module declarations
                    emit warning (see §13)
                    found → go to Step 4
                    not found → continue

Step 4 — cache check:
    xelc_path = dir/__xelcache__/<file>.xelc
    hash_path  = dir/__xelcache__/<file>.xelc.hash
    current_hash = SHA256(source_file)

    If xelc_path exists AND stored_hash == current_hash:
        → load bytecode from .xelc  (fast path)
    Else:
        → recompile source file
        → write new .xelc and .xelc.hash
        → update .xell_meta entry for this file

Step 5 — execute:
    Execute module bytecode once
    Build module object with export registry

Step 6 — store in session cache:
    session_cache[(file_path, module_name)] = module_object

Step 7 — return module object

NOT FOUND → ImportError: module 'X' not found
    Hint: "Did you run xell --make_module?"
```

---

## 15. Cache Invalidation

Cache is **automatic and self-healing**. You never manually clear it.

```
Source file edited
        ↓
Next bring → runtime reads .xelc.hash
        ↓
Hash mismatch detected
        ↓
Recompile source file
        ↓
Write new .xelc + .xelc.hash
        ↓
Update .xell_meta entry
        ↓
Other files in same directory → unaffected
```

Only the changed file is recompiled. Everything else loads from cache as before.

---

## 16. `bring *` — Dump All

`bring *` works like a controlled include — dumps all exported names from a module.

### Namespaced dump (recommended)

```xell
bring * of lib->math_lib as math     # safe — everything under math->
math->add(1, 2)
math->sub(5, 3)
```

### Global scope dump (use carefully)

```xell
bring * of lib->math_lib             # add, sub, PI go into global scope
add(1, 2)                            # callable directly
```

### Warning behavior

| Context             | Behavior                                   |
| ------------------- | ------------------------------------------ |
| REPL (interactive)  | Silent — no warning                        |
| `.xell` script file | Emits warning about global scope pollution |
| `@no_warn` present  | Suppresses warning                         |

```xell
@no_warn
bring * of lib->math_lib             # explicit opt-out of warning
```

---

## 17. Conflict Resolution

### Two files define the same module name

Use `from` + `as` — explicit beats all:

```xell
from "./local_libs" bring lib as local_lib
from "./vendor"     bring lib as vendor_lib

local_lib->add(1, 2)
vendor_lib->add(1, 2)
```

### Name collision when dumping

```xell
bring * of lib->math_lib    # exports: add, sub
bring * of lib->string_lib  # exports: add, shout  ← 'add' conflict!
# Error: name 'add' already in scope from lib->math_lib
# Use aliased dump instead:
bring * of lib->math_lib   as math
bring * of lib->string_lib as str
math->add(...)
str->add(...)
```

Unaliased `bring *` with a name collision is a **runtime error** unless the user explicitly
handles it with `as`.

---

## 18. Grammar Reference

```
BRING_STMT   := BRING_CLAUSE ["as" ALIAS_LIST]

BRING_CLAUSE := ["from" STRING] "bring" BRING_CHAIN

BRING_CHAIN  := BRING_PART { "and" ["from" STRING] BRING_PART }

BRING_PART   := BRING_ITEMS "of" MODULE_PATH
              | "*" "of" MODULE_PATH
              | MODULE_PATH

BRING_ITEMS  := IDENTIFIER { "," IDENTIFIER }

MODULE_PATH  := IDENTIFIER { "->" IDENTIFIER }

ALIAS_LIST   := IDENTIFIER { "," IDENTIFIER }

MODULE_DEF   := "module" IDENTIFIER ":" { MODULE_BODY } ";"

MODULE_BODY  := EXPORT_DECL | FN_DEF | CLASS_DEF | STRUCT_DEF | ASSIGN | MODULE_DEF | REQUIRES_STMT

EXPORT_DECL   := "export" ( FN_DEF | CLASS_DEF | STRUCT_DEF | ASSIGN | MODULE_DEF )

REQUIRES_STMT := "requires" ( IDENTIFIER | BRING_ITEMS "of" MODULE_PATH )
```

### Alias count rule

The total number of identifiers in `ALIAS_LIST` must equal the total number of items brought
across all `BRING_PART` segments. Mismatch → parse-time error.

---

## 19. Full End-to-End Example

### File structure

```
project/
├── main.xell
└── libs/
    ├── science.xell     ← contains: module physics, module chemistry
    ├── math.xell        ← contains: module lib (with math_lib submodule)
    └── .xell_meta       ← generated by xell --make_module
```

### `libs/math.xell`

```xell
module lib :

    module math_lib :
        export fn add(a, b) : give a + b ;
        export fn sub(a, b) : give a - b ;
        fn _internal(x) : give x * 2 ;
    ;

    export PI = 3.14159

    export class Vector :
        fn __init__(self, x, y) :
            self->x = x
            self->y = y
        ;
        fn __print__(self) :
            give "Vec({self->x}, {self->y})"
        ;
    ;
;
```

### `libs/science.xell`

```xell
module physics :
    module mechanics :
        export fn gravity(mass) : give mass * 9.8 ;
        export fn force(m, a) : give m * a ;
    ;
    export fn solar(dist) : give 1361 / (dist * dist) ;
;

module chemistry :
    export fn molar_mass(compound) : ... ;
;
```

### Step 1 — register modules

```bash
xell --make_module ./libs/
# Creates: libs/.xell_meta
#          libs/__xelcache__/math.xelc
#          libs/__xelcache__/math.xelc.hash
#          libs/__xelcache__/science.xelc
#          libs/__xelcache__/science.xelc.hash
```

### `main.xell`

```xell
# Bring specific items from multiple modules — chained
from "./libs" bring add, sub of lib->math_lib
             and gravity, solar of physics->mechanics as a, s, g, so

print a(1, 2)           # 3
print s(5, 3)           # 2
print g(10)             # 98.0
print so(1)             # 1361.0

# Bring whole submodule
from "./libs" bring math_lib of lib as math
print math->add(10, 5)  # 15

# Bring with global dump (warns in script mode)
@no_warn
from "./libs" bring * of lib->math_lib
print add(3, 4)         # 7

# Module as object
from "./libs" bring lib as l
print l->__name__       # lib
print l->__exports__    # ["PI", "Vector", "math_lib"]
fn compute(mod) :
    give mod->add(100, 200)
;
print compute(l->math_lib)   # 300
```

---

## 20. Summary: All Files Introduced

| File          | Where           | Purpose                                   | Commit? |
| ------------- | --------------- | ----------------------------------------- | ------- |
| `.xell_meta`  | Each directory  | Module name → file mapping + export index | ✅ Yes  |
| `*.xelc`      | `__xelcache__/` | Compiled Xell bytecode                    | ❌ No   |
| `*.xelc.hash` | `__xelcache__/` | SHA-256 of source for cache invalidation  | ❌ No   |

`.gitignore` addition:

```gitignore
__xelcache__/
```

---

## 21. Implementation Roadmap

### Phase 1: Keywords & Lexer (~0.5 days)

- [ ] Add `module`, `export`, `from`, `and` tokens to lexer
- [ ] `bring`, `of`, `as` tokens (verify they exist or add)
- [ ] Tests: all new keywords tokenize correctly

### Phase 2: Module Definition Parsing (~2 days)

- [ ] Add `ModuleDef` AST node (`module name : body ;`)
- [ ] Allow `module` blocks nested inside `module` blocks to any depth
- [ ] Add `ExportDecl` AST node wrapping any declaration
- [ ] Parse `export fn`, `export class`, `export struct`, `export var`, `export module`
- [ ] Multiple top-level `module` blocks in one file
- [ ] Tests: parse nested modules, export declarations

### Phase 3: Bring Statement Parsing (~1 day)

- [ ] Parse `bring MODULE_PATH`
- [ ] Parse `bring X as alias`
- [ ] Parse `bring X, Y as a, b`
- [ ] Parse `from "dir" bring ...`
- [ ] Parse `bring ITEMS of MODULE_PATH as aliases`
- [ ] Parse `bring * of MODULE_PATH`
- [ ] Parse `and` chaining: `bring X of A and Y of B as a, b`
- [ ] Parse-time alias count validation
- [ ] Tests: all bring forms parse correctly

### Phase 4: Module Object & Interpreter (~2 days)

- [ ] Add `MODULE` type to `XObject`
- [ ] Module stores: `__name__`, `__path__`, `__exports__`, export registry
- [ ] `->` access on module object dispatches to exported members
- [ ] Nested module access: `lib->math_lib->add`
- [ ] Session-level module registry (execute once per session)
- [ ] Tests: module objects, field access, nesting

### Phase 5: Import Execution & Search (~2 days)

- [ ] Build search path from `from` dir + cwd + `XELL_PATH` + stdlib
- [ ] Implement fast path: read `.xell_meta`, lookup module name → filename
- [ ] Implement slow path: scan `.xell` files if no `.xell_meta`, emit warning
- [ ] Execute module file once, register exports
- [ ] Implement `bring ITEMS of PATH as aliases` — selective import
- [ ] Implement `bring * of PATH` — full dump, namespaced and global
- [ ] `@no_warn` suppression for global dump
- [ ] `ImportError` with helpful hint message
- [ ] Tests: search path, selective import, global dump

### Phase 6: Cache System (~2 days)

- [ ] Define `.xelc` bytecode format
- [ ] Write `.xelc` and `.xelc.hash` on first compile
- [ ] On bring: compare stored hash vs current file SHA-256
- [ ] Fast path: load from `.xelc` if hash matches
- [ ] Slow path: recompile + update `.xelc`, `.xelc.hash`, `.xell_meta`
- [ ] `__xelcache__/` directory auto-created next to source
- [ ] Tests: cache hit, cache miss, hash mismatch triggers recompile

### Phase 7: `xell --make_module` CLI (~1 day)

- [ ] Add `--make_module` flag to CLI argument parser
- [ ] Parse target: single file, glob, or directory
- [ ] For each file: lex → parse → collect modules + exports + hash
- [ ] Write `.xell_meta` (create or merge-update)
- [ ] Write `.xelc` and `.xelc.hash` to `__xelcache__/`
- [ ] `--update` mode: only reprocess files whose hash changed
- [ ] Tests: metadata generation, incremental update

### Phase 8: Metadata `.xell_meta` Management (~1 day)

- [ ] Define `.xell_meta` JSON schema (version, generated timestamp, modules map)
- [ ] Reader: load and validate schema, report version mismatch
- [ ] Writer: create or update entries without clobbering unrelated entries
- [ ] Multiple modules from same file → separate index entries pointing to same file
- [ ] Tests: create, update, multi-module-per-file

### Phase 9: Conflict Resolution & Edge Cases (~0.5 days)

- [ ] Global dump collision → runtime error with suggestion to use `as`
- [ ] Two directories both have module of same name → require `from` or `as` to disambiguate
- [ ] `bring *` in REPL → silent; in script → warning
- [ ] Module self-import → detect cycle and error
- [ ] Circular dependency between modules → detect and error with chain shown
- [ ] Tests: all conflict and edge cases

### Phase 10: `requires` Keyword (~0.5 days)

- [ ] Add `REQUIRES` token to lexer
- [ ] Parse `requires IDENTIFIER` and `requires ITEMS of MODULE_PATH` inside module body
- [ ] Add `requires` list to `ModuleDef` AST node
- [ ] On `bring X`: read `requires` list, auto-bring each dependency first (in declaration order)
- [ ] `RequireError` if a required dependency cannot be found
- [ ] Error message: `"RequireError: module 'X' requires 'Y' but 'Y' could not be found"`
- [ ] Tests: auto-dependency resolution, missing requirement error, chained requires

### Phase 11: `@eager` Decorator on `bring` (~0.5 days)

- [ ] Extend decorator parsing to `bring` statements
- [ ] `@eager bring X` → load module immediately at point of statement
- [ ] Default (no decorator) → lazy: load module on first member access
- [ ] `requires` inside a module always resolve eagerly regardless of outer bring mode
- [ ] Tests: eager vs lazy loading, startup-time error reporting

### Phase 12: Module Dunder Variables (~1 day)

- [ ] Inject `__name__` into module scope at load time:
  - `"__main__"` when file is executed directly via `xell file.xell`
  - `"module_name"` when brought; `"parent->child"` for nested modules
- [ ] Inject `__file__` / `__path__` — absolute path to source file
- [ ] Inject `__module__` — parent module name; `none` for top-level
- [ ] Inject `__exports__` — list of exported names, auto-populated
- [ ] Inject `__cached__` — path to `.xelc` bytecode file
- [ ] Inject `__args__` — argv list when run directly; `none` when brought
- [ ] Support user-defined `__version__` field inside module body
- [ ] `if __name__ == "__main__" :` pattern: block skipped when module is brought
- [ ] Tests: all dunders correct values, `__main__` guard, nested `"lib->math_lib"` name

**Total estimated: ~14 days**

---

## 22. `requires` Keyword

A module can declare its own dependencies upfront using `requires`. This documents what the module needs and ensures those dependencies are resolved automatically when the module is brought.

### Syntax

```xell
module my_plugin :
    requires json
    requires math of stdlib
    requires Logger, Database of my_app->core

    export fn run() :
        data = json->parse("{}")
        Logger->log("starting")
    ;
;
```

### What it does

- **Documents dependencies** — anyone reading the module file sees exactly what it needs.
- **Auto-brings dependencies** — when you `bring my_plugin`, Xell automatically brings all
  required modules first, in declaration order, before loading `my_plugin`.
- **Fails early** — if a requirement is missing, `RequireError` is raised at bring time, not
  buried deep in a runtime crash when a specific line tries to use something undefined.

### Auto-bring chain

```
bring my_plugin
    → sees: requires json
    → auto-brings json first
    → sees: requires math of stdlib
    → auto-brings math from stdlib
    → sees: requires Logger, Database of my_app->core
    → auto-brings those
    → then loads my_plugin itself
    → all dependencies guaranteed before any code runs
```

### `requires` syntax forms

```xell
# Bring a whole module
requires json

# Bring specific items from a module
requires Logger, Database of my_app->core

# Bring from stdlib explicitly
requires math of stdlib

# Bring a nested module
requires mechanics of physics->classical
```

### Error when a requirement is missing

```
RequireError: module 'my_plugin' requires 'Logger' of 'my_app->core'
              but 'my_app->core' could not be found.
hint: did you run xell --make_module ./your_app/?
```

The error appears at bring time — not buried in runtime when a specific line executes.

### `requires` vs `bring` inside function body

| Approach                 | When it runs                 | Visibility                  | Error timing  |
| ------------------------ | ---------------------------- | --------------------------- | ------------- |
| `requires` at module top | Before module loads          | Upfront — self-documenting  | At bring time |
| `bring` inside `fn`      | When that function is called | Hidden inside function body | At runtime    |

`requires` is for real dependencies the module always needs. `bring` inside a function body is for optional or conditional imports.

---

## 23. `@eager` Decorator

By default all `bring` statements are **lazy** — the module is loaded on first member access, not at the point of the `bring` statement itself. The `@eager` decorator forces immediate loading.

### Syntax

```xell
bring json              # lazy — default: loads on first use
bring math              # lazy — default
@eager bring db         # loads immediately — connection established at startup
@eager bring config     # loads immediately — config validated at startup
print "ready"           # db and config are guaranteed fully loaded by here
```

### Lazy vs eager

| Mode           | Load time              | Error timing                     | Use when                           |
| -------------- | ---------------------- | -------------------------------- | ---------------------------------- |
| Lazy (default) | On first member access | When you first use it            | Most imports — no startup cost     |
| `@eager`       | At `bring` statement   | At startup, before anything runs | Connections, config, critical deps |

### Typical pattern

```xell
# Critical resources — must be ready before handling any request
@eager bring db_pool
@eager bring config

# Regular utilities — lazy is fine
bring json
bring regex
bring math

fn handle_request(req) :
    # db_pool and config guaranteed loaded here
    # json, regex, math load on first use
    data = json->parse(req->body)
    give db_pool->query("SELECT ...")
;
```

### `@eager` + `requires` interaction

If a module uses `requires`, its requirements are always resolved eagerly (before the module loads), regardless of whether the outer `bring` is lazy or `@eager`.

```xell
@eager bring my_plugin     # loads my_plugin immediately
                           # my_plugin's requires are resolved first, also immediately
```

---

## 24. Module Dunder Variables

Every module scope has a set of automatically injected `__dunder__` variables that provide self-awareness at runtime.

### Full set

| Variable      | Meaning                                                                         |
| ------------- | ------------------------------------------------------------------------------- |
| `__name__`    | `"__main__"` if run directly; full module path (`"lib->math_lib"`) when brought |
| `__module__`  | Parent module name — `"lib"` for a submodule; `none` for top-level              |
| `__file__`    | Absolute path to source file (same as `__path__`)                               |
| `__exports__` | List of exported names: `["add", "sub", "PI"]`                                  |
| `__version__` | Optional — set by module author; `none` if not declared                         |
| `__cached__`  | Absolute path to `.xelc` bytecode cache file                                    |
| `__args__`    | CLI argv list when run directly; `none` when brought as module                  |

### `__name__ == "__main__"` — entry point detection

The primary use case: write testable, importable modules that also work as runnable scripts.

```xell
# mathlib.xell
module math_lib :
    __version__ = "1.0.0"

    export fn add(a, b) : give a + b ;
    export fn sub(a, b) : give a - b ;
    export fn mul(a, b) : give a * b ;

    # unit tests — only runs when this file is executed directly
    if __name__ == "__main__" :
        print "testing math_lib {__version__}"
        assert add(1, 2) == 3
        assert sub(5, 3) == 2
        assert mul(4, 3) == 12
        print "all tests passed"
    ;
;
```

```bash
xell mathlib.xell
# → testing math_lib 1.0.0
# → all tests passed
```

```xell
# from another file
bring math_lib
math_lib->add(1, 2)    # test block never runs — __name__ is "math_lib", not "__main__"
```

### `__name__` in nested modules

Each module level has a `__name__` that includes its full path:

```xell
module lib :
    module math_lib :
        export fn debug() :
            print __name__      # → "lib->math_lib"
            print __module__    # → "lib"
            print __file__      # → "/path/to/file.xell"
        ;
    ;
;
```

### `__version__` — set by module author

```xell
module my_lib :
    __version__ = "2.3.1"

    export fn add(a, b) : give a + b ;
;

# user side
bring my_lib
print my_lib->__version__    # "2.3.1"
```

### `__args__` — CLI arguments when run directly

```xell
# deploy.xell — importable module and runnable script in one
module deploy_tools :
    export fn deploy(project) :
        print "deploying {project}"
    ;
;

if __name__ == "__main__" :
    args = __args__
    if len(args) < 1 :
        error("usage: xell deploy.xell <project>")
    ;
    deploy_tools->deploy(args[0])
;
```

```bash
xell deploy.xell my_project    # → deploying my_project
```

```xell
bring deploy_tools             # script block skipped, __args__ is none
deploy_tools->deploy("app")
```

### Summary table

| Variable      | When run directly (`xell file.xell`) | When brought (`bring mod`) |
| ------------- | ------------------------------------ | -------------------------- |
| `__name__`    | `"__main__"`                         | `"mod"` or `"parent->mod"` |
| `__module__`  | `none`                               | parent module name         |
| `__file__`    | absolute path                        | absolute path              |
| `__exports__` | list of exports                      | list of exports            |
| `__version__` | user defined or `none`               | user defined or `none`     |
| `__cached__`  | `.xelc` path                         | `.xelc` path               |
| `__args__`    | argv list                            | `none`                     |

---

## 25. Appendix A: Comparison with Python and C++

| Feature                   | C++                  | Python                  | Xell                       |
| ------------------------- | -------------------- | ----------------------- | -------------------------- |
| File = module name        | YES forced           | YES forced              | **NO — free**              |
| Multiple modules per file | YES (namespaces)     | NO                      | **YES**                    |
| Nested modules            | YES                  | YES (packages only)     | **YES — inline**           |
| Smart caching             | NO                   | YES `__pycache__`       | **YES `__xelcache__`**     |
| Metadata index            | NO                   | NO                      | **YES `.xell_meta`**       |
| Module as object          | NO                   | YES                     | **YES**                    |
| Explicit exports          | NO (public/private)  | NO (`__all__` optional) | **YES (`export` keyword)** |
| Privacy by default        | Partial              | NO                      | **YES**                    |
| Dump all                  | YES `#include`       | YES `import *`          | **YES `bring *`**          |
| Named import              | NO                   | YES `as`                | **YES `as`**               |
| Selective import          | Partial              | YES `from x import y`   | **YES `bring y of x`**     |
| Path override             | NO                   | YES `sys.path`          | **YES `from "dir" bring`** |
| Multi-source chain        | NO                   | NO                      | **YES `and` keyword**      |
| Self-healing cache        | NO                   | Partial                 | **YES — automatic**        |
| Executed once             | YES (include guards) | YES                     | **YES**                    |

---

## 26. Appendix B: What We're NOT Doing

| Feature                           | Status         | Reason                                                    |
| --------------------------------- | -------------- | --------------------------------------------------------- |
| Filename = module name            | ❌ Never       | Explicitly against this design                            |
| C-style `#include` text dump      | ❌ Never       | Smart import only                                         |
| Circular imports                  | ❌ Error       | Detected at runtime with full cycle shown                 |
| Private submodule visibility      | ❌ Never       | Private means invisible — not even in errors              |
| Module versioning in bring syntax | ❌ Not planned | Use `from "dir"` to point to specific version dir         |
| Dynamic bring (runtime strings)   | ⏳ Maybe later | `bring(module_name_variable)` — defer to later            |
| Package registry / installer      | ⏳ Maybe later | `xell install` command — out of scope for now             |
| Interface-style module contracts  | ❌ Not planned/we already have interface style for classes | `requires` is for dependencies, not caller contracts      |
| Lazy loading                      | ✅ Built-in    | Default mode — use `@eager bring` to force immediate load |
| Wildcard file glob in bring       | ❌ Not planned | Use `--make_module` to pre-register                       |

---

_This document reflects the confirmed module system design. Implementation follows the roadmap phases in order._
