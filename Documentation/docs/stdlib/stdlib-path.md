# Standard Library – Path Utilities

> **Previous:** [File System Operations](stdlib-fs.md)  
> **Next:** [Environment Variables](stdlib-env.md)

---

Xell provides a comprehensive set of path utilities for working with filesystem paths. Core functions are always available (Tier 1), while extended operations are available in the `fs` module (Tier 2, requires `bring * from "fs"`).

These functions help you manipulate and inspect paths in a cross‑platform way, handling differences between Windows, Linux, and macOS automatically.

---

## Quick Reference

| Function | Module | What it does | Example |
|----------|--------|--------------|---------|
| `cwd()` | Tier 1 | Returns current working directory | `cwd()` → `"/home/user/project"` |
| `pwd()` | Tier 1 | Alias for `cwd()` | `pwd()` → `"/home/user/project"` |
| `cd(path)` | Tier 1 | Changes working directory | `cd("src/")` |
| `abspath(path)` | Tier 1 | Returns absolute path | `abspath(".")` → `"/home/user/project"` |
| `basename(path)` | Tier 1 | Filename component | `basename("/path/to/file.txt")` → `"file.txt"` |
| `dirname(path)` | Tier 1 | Parent directory component | `dirname("/path/to/file.txt")` → `"/path/to"` |
| `ext(path)` | Tier 1 | File extension | `ext("file.txt")` → `".txt"` |
| `realpath(path)` | Tier 2 (fs) | Canonical absolute path (resolves symlinks) | `realpath("link.txt")` → `"/home/user/data.txt"` |
| `join_path(parts…)` | Tier 2 (fs) | Joins path components | `join_path("home", "user", "file.txt")` → `"home/user/file.txt"` |
| `normalize(path)` | Tier 2 (fs) | Resolves `.` and `..` | `normalize("a/../b/./c")` → `"b/c"` |
| `is_absolute(path)` | Tier 2 (fs) | Checks if absolute | `is_absolute("/tmp")` → `true` |
| `relative_path(path, base)` | Tier 2 (fs) | Path relative to base | `relative_path("/a/b/c", "/a")` → `"b/c"` |
| `home_dir()` | Tier 2 (fs) | User home directory | `home_dir()` → `"/home/alice"` |
| `temp_dir()` | Tier 2 (fs) | OS temporary directory | `temp_dir()` → `"/tmp"` |
| `extension(path)` | Tier 2 (fs) | Alias for `ext()` | `extension("file.txt")` → `".txt"` |
| `stem(path)` | Tier 2 (fs) | Filename without extension | `stem("file.txt")` → `"file"` |

---

## Tier 1 – Core Path Utilities

### `cwd()` / `pwd()`

Returns the current working directory as a string. `pwd()` is an alias.

```xell
let current = cwd()
print("Current directory: " + current)

let also_current = pwd()
print(also_current)   # same as cwd()
```

**Output (varies by system):**
```
Current directory: /home/user/project
/home/user/project
```

### `cd(path)`

Changes the current working directory to `path`. Throws `FileNotFoundError` if the directory doesn't exist.

```xell
cd("src/")
print(cwd())   # now inside src/

cd("..")
print(cwd())   # back to parent
```

**Throws:** `FileNotFoundError` if the path doesn't exist; `IOError` on other failures.

### `abspath(path)`

Returns the absolute (full) path of a given path, resolving relative components like `.` and `..`.

```xell
print(abspath("."))          # "/home/user/project"
print(abspath("src/../lib"))  # "/home/user/project/lib"
```

### `basename(path)`

Returns the last component of a path (the filename).

```xell
print(basename("/home/user/file.txt"))   # "file.txt"
print(basename("/home/user/"))           # "user"
print(basename("file.txt"))              # "file.txt"
```

### `dirname(path)`

Returns the parent directory component of a path (everything except the basename).

```xell
print(dirname("/home/user/file.txt"))   # "/home/user"
print(dirname("/home/user/"))           # "/home"
print(dirname("file.txt"))              # "."
```

### `ext(path)`

Returns the file extension, including the leading dot. Returns an empty string if there is no extension.

```xell
print(ext("file.txt"))     # ".txt"
print(ext("archive.tar.gz"))  # ".gz" (not ".tar.gz" – only the last extension)
print(ext("README"))       # ""
print(ext(".config"))      # ""
```

---

## Tier 2 – Extended Path Utilities (`bring * from "fs"`)

### `realpath(path)`

Returns the canonical absolute path, resolving all symbolic links and eliminating `.` and `..`. This is useful for getting the "real" path of a file.

```xell
bring * from "fs"

let real = realpath("link.txt")
print(real)   # "/home/user/actual/data.txt" (if link.txt points there)
```

**Throws:** `IOError` if the path doesn't exist or cannot be resolved.

### `join_path(parts…)`

Joins multiple path components into a single path, using the platform‑specific separator (`/` on Unix, `\` on Windows).

```xell
bring * from "fs"

let path = join_path("home", "user", "project", "src")
print(path)   # "home/user/project/src" (Unix)

let windows_path = join_path("C:", "Users", "Alice", "Documents")
print(windows_path)   # "C:\Users\Alice\Documents" (Windows)
```

### `normalize(path)`

Resolves `.` and `..` components in a path and removes redundant separators.

```xell
bring * from "fs"

let path = normalize("/home/user/../alice/./data.txt")
print(path)   # "/home/alice/data.txt"

let path2 = normalize("a//b/./c/..")
print(path2)  # "a/b"
```

### `is_absolute(path)`

Returns `true` if the path is absolute, `false` otherwise.

```xell
bring * from "fs"

print(is_absolute("/home/user"))   # true (Unix)
print(is_absolute("C:\\Users"))    # true (Windows)
print(is_absolute("relative"))     # false
```

### `relative_path(path, base)`

Returns `path` relative to `base`. This is the inverse of `join_path`.

```xell
bring * from "fs"

let rel = relative_path("/home/user/project/src/main.xel", "/home/user")
print(rel)   # "project/src/main.xel"

let rel2 = relative_path("/tmp/data.txt", "/usr")
print(rel2)  # "../tmp/data.txt"
```

### `home_dir()`

Returns the user's home directory.

```xell
bring * from "fs"

let home = home_dir()
print(home)   # "/home/alice" (Unix) or "C:\Users\Alice" (Windows)
```

### `temp_dir()`

Returns the system's temporary directory.

```xell
bring * from "fs"

let temp = temp_dir()
print(temp)   # "/tmp" (Unix) or "C:\Users\Alice\AppData\Local\Temp" (Windows)
```

### `extension(path)`

Alias for `ext()`. Returns the file extension including the leading dot.

```xell
bring * from "fs"

print(extension("data.txt"))   # ".txt"
```

### `stem(path)`

Returns the filename without the extension.

```xell
bring * from "fs"

print(stem("file.txt"))        # "file"
print(stem("archive.tar.gz"))  # "archive.tar" (only the last extension is removed)
```

---

## Complete Example: Project Setup Script

```xell
# Project setup script
bring * from "fs"

# Get project root (where this script is located)
let root = cwd()
print("Project root: " + root)

# Define paths relative to root
let src_dir = join_path(root, "src")
let build_dir = join_path(root, "build")
let config_path = join_path(root, "config.xel")

# Create directories if they don't exist
if not exists(src_dir):
    mkdir(src_dir)
    print("Created: " + src_dir)
;

if not exists(build_dir):
    mkdir(build_dir)
    print("Created: " + build_dir)
;

# Check if config exists
if exists(config_path):
    let config = read(config_path)
    print("Config found at: " + config_path)
    print("Content: " + config)
else:
    # Create default config
    write(config_path, "# Default configuration\nDEBUG = true\n")
    print("Created default config: " + config_path)
;

# Show directory structure
print("\nProject structure:")
print(tree(".", 2))

# Get absolute path of build directory
let build_abs = abspath(build_dir)
print("\nBuild directory absolute path: " + build_abs)
```

**Expected output (varies):**
```
Project root: /home/user/project
Created: /home/user/project/src
Created: /home/user/project/build
Config found at: /home/user/project/config.xel
Content: # Default configuration
DEBUG = true

Project structure:
.
├── src/
├── build/
└── config.xel

Build directory absolute path: /home/user/project/build
```

---

## What NOT to Do

### 1. Assuming `cd()` Changes the Working Directory for the Entire Process

```xell
cd("subdir/")   # ✅ changes CWD
cd("..")        # ✅ back to original

# But if you're running in a script, the CWD changes persist
```

### 2. Using `basename()` and `dirname()` with Trailing Slashes

```xell
print(basename("/home/user/"))   # "user"
print(dirname("/home/user/"))    # "/home"
```

These are the expected behaviours (like Python's `os.path.basename`). Be aware that trailing slashes affect the result.

### 3. Forgetting to `bring` the `fs` Module for Extended Functions

```xell
realpath("link.txt")   # ❌ error – realpath is not defined
```

**Fix:** `bring * from "fs"` first.

### 4. Assuming `ext()` Returns Multiple Extensions

```xell
ext("archive.tar.gz")   # ".gz" – only the last extension
```

If you need the full extension (e.g., `".tar.gz"`), you'll need to handle it manually.

### 5. Using `home_dir()` or `temp_dir()` Without `bring * from "fs"`

```xell
home_dir()   # ❌ error – home_dir is not defined
```

**Fix:** `bring * from "fs"` first.

### 6. Expecting `abspath()` to Resolve Symlinks

`abspath()` resolves `.` and `..` but does **not** resolve symlinks. Use `realpath()` for that.

```xell
let abs = abspath("link.txt")     # may still point to the symlink
let real = realpath("link.txt")   # resolves to the actual target
```

### 7. Using `join_path()` Without `bring * from "fs"`

```xell
join_path("a", "b")   # ❌ error – join_path is not defined
```

**Fix:** `bring * from "fs"` first.

---

## See Also

- [File System Operations](stdlib-fs.md) – file and directory operations
- [Environment Variables](stdlib-env.md) – environment variable functions
- [Process Management](stdlib-process.md) – `run`, `run_capture`, `pid`
- [Error Handling](../core/error-handling.md) – handling filesystem errors

---

**Previous:** [File System Operations](stdlib-fs.md)  
**Next:** [Environment Variables](stdlib-env.md)