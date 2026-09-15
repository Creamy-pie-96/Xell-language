# Modules – Module Resolution

> **Previous:** [Importing Modules](modules-import.md)  
> **Next:** [Built‑in Modules](modules-builtins.md)

---

Module resolution is the process of locating a module when you write `bring Name` or `bring X of Module`. Xell uses a multi‑stage resolution algorithm that checks the current scope, the session cache, the filesystem (using `.xell_meta` for fast lookups), and finally built‑in modules. This system is designed to be fast, flexible, and transparent to the developer.

Think of module resolution as **Xell's way of finding your code** – it searches in a specific order, caches results, and handles circular imports gracefully.

---

## Quick Reference

| Concept | What it does | Example |
|---------|--------------|---------|
| **Session cache** | Stores loaded modules so they aren't reloaded | `bring Math` → cached after first load |
| **Search path** | Directories to search for modules | CWD → `XELL_PATH` → stdlib |
| `.xell_meta` | Fast index file mapping module names to files | `lib/` → `.xell_meta` → `"Math": "math.xel"` |
| `__xelcache__/` | Bytecode cache (future feature – currently placeholder) | `__xelcache__/math.xelc` |
| `XELL_PATH` | Environment variable for extra search directories | `XELL_PATH=/custom/lib:/usr/share/xell` |
| **Slow‑path scan** | Fallback search scanning all `.xel` files | Used when `.xell_meta` is missing |
| **Circular import detection** | Prevents infinite recursion | `a.xel` imports `b.xel`, which imports `a.xel` → error |
| **`from` directive** | Adds a specific directory to the search path for a `bring` | `from "vendor" bring JSON` |
| `--make_module` | Builds `.xell_meta` and `__xelcache__` | `xell --make_module .` |

---

## Resolution Order

When you write `bring X of Y` (or `bring Y`), Xell resolves `Y` in this exact order:

1. **Current scope** – is `Y` defined in the local environment?
   - If yes, use that value (must be a module).
2. **Session cache** – has `Y` been imported before?
   - If yes, return the cached module (no filesystem access).
3. **Filesystem** – search using the search path and `.xell_meta` / slow scan.
   - If found, execute the file, cache the module(s), return the result.
4. **Built‑in modules** – is `Y` a built‑in module?
   - If yes, return the built‑in module object.

For `bring X from "file.xel"`, the resolution is simpler:

1. Resolve the file path (relative to current file, respecting `from` if present).
2. Read, parse, and execute the file.
3. Import the specified items from the file's global environment.

---

## Search Path

The search path determines where Xell looks for modules on the filesystem.

### Default Search Path (in order)

1. **`from` directory** – if specified in the `bring` statement (e.g., `from "lib" bring Math`).
2. **Directory of the current source file** – the folder containing the file being executed.
3. **`XELL_PATH` environment variable** – colon‑separated on Unix, semicolon‑separated on Windows.
4. **Standard library directory** – installation‑dependent (e.g., `/usr/local/share/xell/stdlib`).

### Example

```xell
# File: /project/main.xel
from "lib" bring Math   # searches /project/lib/
bring Parser            # searches /project/ (current file dir)
```

If `XELL_PATH=/usr/local/xell/lib`, then after the current dir, it searches that.

---

## Fast Path: `.xell_meta`

`.xell_meta` is a JSON index file that maps module names to their source files, avoiding the need to scan directories or parse files to discover module names.

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
      "submodules": {
        "Geometry": {
          "file": "geometry.xel",
          "hash": "def456..."
        }
      }
    }
  }
}
```

- **`file`** – relative path to the source file.
- **`hash`** – SHA‑256 hash of the source file (for cache validation).
- **`exports`** – list of exported names (used for autocompletion).
- **`submodules`** – nested modules defined in the same file.

### How It's Used

When Xell searches for a module:

1. Check `.xell_meta` in each search path directory.
2. If found, look up the module name → get the file path.
3. Load that file directly (no need to scan all `.xel` files).
4. Validate the hash against `__xelcache__/` (if present).

### Generating `.xell_meta`

Use the `--make_module` command:

```bash
xell --make_module /path/to/lib
```

This scans all `.xel` files in the directory, extracts module definitions, and writes a `.xell_meta` file.

**Update mode:** `--make_module --update` preserves unchanged entries based on file hashes, making incremental builds fast.

```bash
xell --make_module --update /path/to/lib
```

---

## Cache Directory: `__xelcache__/`

The `__xelcache__/` directory stores bytecode and hash information for faster subsequent loads.

### Structure

```
__xelcache__/
├── math.xel.hash    # SHA‑256 hash of source file
└── math.xelc        # Bytecode cache (placeholder – future feature)
```

### How It's Used

1. After loading a file, Xell writes its SHA‑256 hash to `__xelcache__/file.xel.hash`.
2. On subsequent loads, it compares the current hash with the cached one.
3. If unchanged, it can skip parsing and (in the future) load the bytecode directly.

**Current behaviour:** The bytecode cache is **not yet implemented** – only the hash file is written. The `.xelc` file is currently a placeholder.

### Generating the Cache

`--make_module` generates both `.xell_meta` and `__xelcache__/`.

```bash
xell --make_module .
```

---

## Slow Path: Filesystem Scan

If `.xell_meta` is **not** present in a directory, Xell falls back to scanning all `.xel` files and parsing them to discover module names.

### When Slow Path Happens

- The directory does not contain a `.xell_meta` file.
- The `.xell_meta` file is missing the requested module.

### How Slow Path Works

1. Iterate over all `.xel` and `.xell` files in the directory.
2. For each file, parse just enough to find `module NAME :` declarations.
3. Build a mapping of module name → file path.
4. Use that mapping to resolve the requested module.

### Performance Impact

- Slow path can be **significantly slower** (O(n) files) than `.xell_meta` (O(1) lookup).
- Xell prints a warning when using slow path, suggesting you run `xell --make_module` to index the directory.

```xell
Warning: no .xell_meta found in /project/lib
         scanning all .xell files (slow). Run:
             xell --make_module /project/lib/
         to index this directory.
```

---

## Circular Import Detection

Xell tracks which files are currently being imported to detect cycles.

```xell
# File: a.xel
bring B from "b.xel"   # ❌ circular import detected

# File: b.xel
bring A from "a.xel"   # ❌ circular import detected
```

### How It Works

- The module resolver maintains a set of files currently being imported (`importingFiles_`).
- When a `bring` starts, the file is added to the set.
- If a `bring` encounters a file already in the set, a `BringError` is thrown.
- The set is cleaned up when the import completes.

### Circular Import Example

```xell
# File: /project/lib/a.xel
module A :
    requires B
;

# File: /project/lib/b.xel
module B :
    requires A
;

# Error: Circular import detected: B -> A -> B
```

**Important:** Circular detection is per **file**, not per module. If two modules in the same file depend on each other, they don't cause a cycle because the file is imported as a whole.

---

## `from` Directory Resolution

The `from` directive modifies the search path for a `bring` statement.

```xell
from "vendor" bring JSON
```

### Resolution Order with `from`

1. **`from` directory** – explicitly specified.
2. (then the normal search path – but the `from` directory takes priority).

### Relative Paths

`from` paths are resolved relative to the **current source file's directory**.

```xell
# File: /project/main.xel
from "lib" bring Math   # looks for /project/lib/
from "../shared" bring Parser   # looks for /project/../shared/
```

### `from` with File Imports

```xell
from "vendor" bring square from "math.xel"
# Looks for /project/vendor/math.xel
```

---

## `XELL_PATH` Environment Variable

Set `XELL_PATH` to add extra directories to the search path.

```bash
export XELL_PATH=/custom/lib:/usr/local/xell/lib
xell my_script.xel
```

### Format

- **Unix** (Linux, macOS): colon‑separated (`:`)
- **Windows**: semicolon‑separated (`;`)

### Order

`XELL_PATH` directories are searched **after** the current directory and `from` directories, but **before** the standard library.

---

## Standard Library Directory

The standard library is installed alongside the Xell executable. Modules like `threading` are found here.

### Default Locations

- **Linux/macOS:** `/usr/local/share/xell/stdlib` or `/usr/share/xell/stdlib`
- **Windows:** `C:\Program Files\Xell\stdlib`

### Overriding

You can override the stdlib path by setting `XELL_PATH` to include your own stdlib directory.

---

## Complete Example: Module Resolution Walkthrough

```xell
# File: /project/app/main.xel

# 1. from directive
from "lib" bring Math   # searches /project/lib/

# 2. Current scope – Math is now defined
print(Math->square(5))   # 25

# 3. Another import from same module (uses cache)
from "lib" bring Math   # uses session cache – no filesystem access

# 4. Import from stdlib
bring * from "threading"   # resolves to built‑in module

# 5. Relative import
bring Helper from "./utils.xel"   # resolves to /project/app/utils.xel

# 6. Import with no from – uses current dir + XELL_PATH
bring JSON   # searches /project/app/, then XELL_PATH, then stdlib
```

### How Xell Resolves Each

| `bring` | Resolution Steps |
|---------|------------------|
| `from "lib" bring Math` | 1. Resolve `"lib"` → `/project/lib/`<br>2. Check `/project/lib/.xell_meta` for `"Math"`<br>3. If found, load `/project/lib/math.xel`<br>4. If not found, slow‑path scan `/project/lib/` |
| Second `from "lib" bring Math` | 1. Session cache hit → return cached module |
| `bring * from "threading"` | 1. Built‑in module found → return built‑in `threading` |
| `bring Helper from "./utils.xel"` | 1. Resolve path → `/project/app/utils.xel`<br>2. Read, parse, execute `utils.xel`<br>3. Import `Helper` from its global environment |
| `bring JSON` | 1. Current scope? No.<br>2. Session cache? No.<br>3. Filesystem: search `/project/app/` (no `.xell_meta`)<br>4. Slow‑path scan `/project/app/` (find? maybe no)<br>5. `XELL_PATH` directories<br>6. Standard library (maybe `stdlib/JSON.xel`) |

---

## Edge Cases and Less Obvious Behavior

### 1. Modules Are Cached by Name, Not by Path

If two different files define the same module name, the **first** one loaded is cached, and subsequent `bring`s return that cached module – even if a different file with the same name exists later in the search path.

```xell
# /project/lib/Math.xel – defines module Math
# /other/Math.xel – also defines module Math

# If XELL_PATH includes /project/lib first, that one is cached.
bring Math   # loads /project/lib/Math.xel (cached)
# Later, even if /other/Math.xel is found, it's ignored.
```

### 2. `bring * from "file"` Imports All Top‑Level Names

This includes non‑exported names, which can pollute your namespace.

```xell
# file.xel:
let secret = 42
fn helper(x): give x * 2
export fn process(x): give helper(x) + secret

bring * from "file.xel"
print(secret)   # 42 (imported even though not exported)
```

### 3. Circular Imports Are Detected at File Level

If two files import each other, the detection happens at the file level, not at the module level. This means if both modules are in the same file, there's no cycle because the file is loaded once.

```xell
# File: both.xel
module A : requires B ;
module B : requires A ;   # OK – same file, no cycle
```

### 4. `.xell_meta` Is Not Automatically Updated

If you add, remove, or rename modules, `.xell_meta` becomes stale. Run `xell --make_module` to regenerate it.

### 5. Slow‑Path Warnings Are Printed to stderr

Slow‑path warnings are captured in the `warnings` vector passed to `resolveFromFileSystem` and printed to stderr. They do **not** prevent execution – they just inform the user.

### 6. `__xelcache__` Is Created on Demand

The `__xelcache__` directory is created by `--make_module` but is **not** created automatically during normal execution (currently only the `.hash` file is written, but the `.xelc` placeholder is also written). Actually, in the current code, `--make_module` creates both `.hash` and `.xelc` placeholders.

---

## What NOT to Do

### 1. Relying on Slow‑Path in Production

```xell
bring Math   # If .xell_meta is missing, slow‑path scan will run
```

**Why it's bad:** Slow‑path scans can be slow in large directories. Always run `xell --make_module` to generate `.xell_meta`.

### 2. Expecting `.xell_meta` to Automatically Update

```xell
# After adding a new module:
touch new.xel
bring NewModule   # ❌ still missing if .xell_meta is stale
```

**Fix:** Run `xell --make_module --update .` to refresh the index.

### 3. Creating a Module with the Same Name as a Built‑in

```xell
# file: threading.xel
module threading : ... ;

bring threading   # ❌ shadows built‑in threading – confusion
```

### 4. Using Relative Paths Without a Base Directory

```xell
bring Math from "lib/math.xel"   # ✔ resolves relative to current file
bring Math from "/absolute/path"   # ✔ absolute path works
bring Math from "~/lib/math.xel"   # ❌ ~ is not expanded – use full path
```

### 5. Forgetting to Include `XELL_PATH` in Scripts

```bash
# Without XELL_PATH, modules in custom directories won't be found.
xell script.xel   # ❌ may fail to find modules
```

**Fix:** Set `XELL_PATH` in your environment or in a launcher script.

### 6. Deleting `.xell_meta` Without Regenerating

```bash
rm .xell_meta
# Slow‑path will be used – potentially slow.
```

### 7. Expecting `--make_module` to Handle Nested Modules Automatically

`--make_module` recursively scans directories, so nested modules are indexed. However, if you have a deeply nested structure, ensure the `.xell_meta` is placed at the root of your module search path.

---

## See Also

- [Importing Modules](modules-import.md) – how to use `bring`
- [Defining Modules](modules-definition.md) – how to create modules
- [Built‑in Modules](modules-builtins.md) – standard library modules
- [CLI – `--make_module`](../tooling/cli.md#--make_module) – command reference

---

**Previous:** [Importing Modules](modules-import.md)  
**Next:** [Built‑in Modules](modules-builtins.md)