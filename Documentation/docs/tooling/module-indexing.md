# Module Indexing

> **Previous:** [Dialect Conversion](dialect-conversion.md)  
> **Next:** [Debugging](debugging.md)

---

Xell's module indexing system enables **fast module resolution** by pre‑building an index of all modules in a directory. Instead of scanning all `.xel` files every time a module is imported (slow‑path), Xell uses `.xell_meta` for O(1) lookups, and `__xelcache__/` for bytecode caching (future) and source hash validation.

Think of this as Xell's **module database** – it tells the interpreter exactly which file contains which module, without having to parse every file to find out.

---

## Quick Reference

| Command / Concept | What it does | Example |
|-------------------|--------------|---------|
| `xell --make_module <paths...>` | Builds `.xell_meta` and `__xelcache__/` for modules | `xell --make_module . /lib` |
| `xell --make_module --update <paths...>` | Incrementally updates changed modules | `xell --make_module --update .` |
| `.xell_meta` | JSON index file mapping module names to source files | `lib/.xell_meta` |
| `__xelcache__/` | Cache directory for source hashes and bytecode | `__xelcache__/math.xel.hash` |
| Source hash | SHA‑256 of source file for cache invalidation | Used by `--update` mode |
| Slow‑path scan | Fallback when `.xell_meta` is missing | Scans all `.xel` files to find modules |

---

## The Problem: Slow‑Path Scanning

When you write `bring Math`, Xell needs to find which file contains the `Math` module. Without an index, it must:

1. Scan every `.xel` file in the search path.
2. Parse each file enough to find `module Math :` declarations.
3. Build a mapping of module name → file path on the fly.

This is **O(n)** in the number of files – slow for large projects.

---

## The Solution: `.xell_meta`

`.xell_meta` is a JSON index file that maps module names to their source files in O(1) time.

### Format

```json
{
  "xell_meta_version": 1,
  "directory": "/project/lib",
  "modules": {
    "Math": {
      "file": "math.xel",
      "hash": "abc123...",
      "exports": ["square", "cube", "PI"],
      "convert": "dialect.xesy",
      "submodules": {
        "Geometry": {
          "file": "geometry.xel",
          "hash": "def456...",
          "exports": ["Point", "Circle"]
        }
      }
    }
  }
}
```

**Fields:**
- `xell_meta_version` – metadata version (currently `1`).
- `directory` – absolute path to the directory containing the index.
- `modules` – object mapping module names to entries.
  - `file` – relative path to the source file.
  - `hash` – SHA‑256 hash of the source file (for cache validation).
  - `exports` – list of exported names (for autocompletion).
  - `convert` – path to `.xesy` dialect mapping file (if applicable).
  - `submodules` – nested modules defined in the same file.

### How It's Used

When `bring Math` is executed:

1. Xell checks each directory in the search path for `.xell_meta`.
2. It looks up `"Math"` in the `modules` object.
3. If found, it loads `lib/math.xel` directly – no scanning needed.
4. If not found, it falls back to slow‑path scanning.

---

## The Cache Directory: `__xelcache__/`

`__xelcache__/` stores per‑file cache data for fast incremental builds and future bytecode loading.

### Structure

```
__xelcache__/
├── math.xel.hash   # SHA‑256 hash of source file
├── math.xelc       # Bytecode cache (placeholder – future feature)
├── json.xel.hash
├── json.xelc
└── parser.xel.hash
```

### `.hash` Files

Stores the SHA‑256 hash of the source file. Used by `--make_module --update` to detect changes:

1. Read the current source file.
2. Compute its SHA‑256 hash.
3. Compare with the stored hash.
4. If different, the file has changed and needs re‑indexing.

### `.xelc` Files (Future)

Placeholders for bytecode caching. When the bytecode VM is implemented, this will store compiled bytecode for fast loading (skipping parsing and execution).

**Current behaviour:** The `.xelc` file is written as a placeholder but not used.

---

## `xell --make_module`

### Full Build

```bash
xell --make_module /project/lib /project/src
```

**Process:**

1. Recursively scans the specified directories for `.xel` and `.xell` files.
2. For each file:
   a. Reads the source.
   b. Detects `@convert` (if present) and records the dialect mapping.
   c. Parses the file to find `module` definitions (with full hierarchy).
   d. Computes the SHA‑256 hash.
   e. Extracts exported names.
3. Writes `.xell_meta` in each directory.
4. Writes `__xelcache__/*.hash` and `__xelcache__/*.xelc` placeholders.

**Output:**

```
$ xell --make_module .
  ✓ Math (from math.xel)
  ✓ JSON (from json.xel)
  ✓ Parser (from parser.xel)
  ✓ Geometry->Point (from geometry.xel)
  📝 Wrote .xell_meta
  📝 Wrote __xelcache__/
✅ Registered 4 module(s) from 4 file(s).
```

### Incremental Update

```bash
xell --make_module --update .
```

**Process:**

1. Reads the existing `.xell_meta` (if it exists).
2. For each file:
   a. Computes the SHA‑256 hash.
   b. Compares with the cached hash in `__xelcache__/*.hash`.
   c. If unchanged, **skips** re‑indexing (preserves the existing entry).
   d. If changed or new, re‑indexes the file.
3. Writes a new `.xell_meta`, merging preserved entries with updated entries.

**Output:**

```
$ xell --make_module --update .
  ⏭  math.xel (unchanged)
  ✓ json.xel (changed)
  ⏭  parser.xel (unchanged)
  📝 Wrote .xell_meta
```

This is much faster than a full rebuild when only a few files have changed.

---

## Nested Modules

Modules defined inside other modules are recorded as `submodules` in `.xell_meta`.

```xell
# geometry.xel
module Geometry :
    export module Point :
        export x = 0
        export y = 0
    ;
;
```

`.xell_meta` entry:

```json
"Geometry": {
  "file": "geometry.xel",
  "hash": "...",
  "exports": [],
  "submodules": {
    "Point": {
      "file": "geometry.xel",
      "hash": "...",
      "exports": ["x", "y"]
    }
  }
}
```

---

## Slow‑Path Fallback

If `.xell_meta` is missing, Xell falls back to scanning:

```xell
Warning: no .xell_meta found in /project/lib
         scanning all .xell files (slow). Run:
             xell --make_module /project/lib/
         to index this directory.
```

**Performance impact:** Slow‑path is **O(n)** and can be significantly slower for large directories. Always generate `.xell_meta` for production projects.

---

## Dialect Conversion and Indexing

When indexing a file with `@convert`, the dialect mapping is recorded in `.xell_meta`:

```json
"Math": {
  "file": "math.xel",
  "hash": "...",
  "convert": "dialect.xesy",
  "exports": ["square", "cube"]
}
```

This ensures that when the module is loaded, the dialect mapping is applied before parsing.

---

## Complete Example

### Project Structure

```
/project/
├── lib/
│   ├── math.xel
│   ├── json.xel
│   └── geometry.xel
├── src/
│   └── main.xel
└── .xell_meta        # generated by --make_module
```

### Step 1: Build the index

```bash
$ cd /project
$ xell --make_module lib/ src/
  ✓ Math (from lib/math.xel)
  ✓ JSON (from lib/json.xel)
  ✓ Geometry (from lib/geometry.xel)
  ✓ Geometry->Point (from lib/geometry.xel)
  ✓ Geometry->Circle (from lib/geometry.xel)
  📝 Wrote lib/.xell_meta
  📝 Wrote lib/__xelcache__/
  📝 Wrote src/.xell_meta
  📝 Wrote src/__xelcache__/
✅ Registered 5 module(s) from 3 file(s).
```

### Step 2: Use the index

```xell
# src/main.xel
bring Math from "lib"   # O(1) lookup via .xell_meta
bring JSON from "lib"   # O(1) lookup
bring Point of Geometry->Point   # O(1) lookup (submodule)

print(Math->square(5))   # 25
```

### Step 3: Update after changes

```bash
$ # Edit lib/math.xel
$ xell --make_module --update lib/
  ⏭  json.xel (unchanged)
  ✓ math.xel (changed)
  ⏭  geometry.xel (unchanged)
  📝 Wrote lib/.xell_meta
```

---

## Edge Cases and Less Obvious Behavior

### 1. `.xell_meta` Must Be Generated Per Directory

Each directory with modules should have its own `.xell_meta`. The search path checks each directory individually.

### 2. Slow‑Path Warnings Are Only Printed Once

If `.xell_meta` is missing, Xell prints a warning and then uses slow‑path scanning. The warning is printed once per directory.

### 3. `--make_module` Recursively Scans Directories

`--make_module` recursively traverses subdirectories and indexes modules in each directory.

### 4. `--make_module` on a File

You can also run `--make_module` on individual files:

```bash
xell --make_module lib/math.xel
```

This indexes only that file and writes `.xell_meta` in its directory.

### 5. Nested Module Exports

Exported nested modules appear as exports of the parent module **and** as submodules.

### 6. `@convert` Mapping Paths Are Relative

The `convert` field in `.xell_meta` stores the `.xesy` filename (relative to the module file's directory).

---

## What NOT to Do

### 1. Relying on Slow‑Path in Production

```bash
# ❌ Slow for large projects
bring Math
```

**Fix:** Generate `.xell_meta` with `--make_module`.

### 2. Forgetting to Regenerate `.xell_meta` After Adding a Module

```bash
# ❌ New module not found
touch new.xel
bring New   # fails (not indexed)
```

**Fix:** Run `--make_module --update` or full `--make_module`.

### 3. Deleting `.xell_meta` Without Regenerating

```bash
rm .xell_meta   # ❌ slow‑path fallback
```

### 4. Using `--make_module` on a Directory Without `.xel` Files

```bash
$ xell --make_module empty_dir
No .xel or .xell files found.
```

### 5. Expecting `--make_module` to Resolve Imports

`--make_module` only **indexes** modules. It does **not** validate that imports resolve correctly.

### 6. Forgetting That `.xell_meta` Is Not Version‑Controlled

`.xell_meta` is a generated file. Add it to `.gitignore` and regenerate it in CI/deployment.

---

## See Also

- [Module Resolution](../modules/modules-resolution.md) – how modules are found
- [CLI Commands](cli.md) – `--make_module` flag details
- [Dialect Conversion](dialect-conversion.md) – `@convert` and `.xesy` mapping
- [Debugging](debugging.md) – `--trace-vars`, `--debug`

---

**Previous:** [Dialect Conversion](dialect-conversion.md)  
**Next:** [Debugging](debugging.md)