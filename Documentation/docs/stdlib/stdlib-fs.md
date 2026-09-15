# Standard Library – File System Operations

> **Previous:** [Higher‑Order Functions](stdlib-hof.md)  
> **Next:** [Path Utilities](stdlib-path.md)

---

Xell provides comprehensive filesystem operations for working with files and directories. The core functions are always available (Tier 1), while extended operations (symlinks, permissions, tree, find, etc.) are available in the `fs` module (Tier 2, requires `bring * from "fs"`).

---

## Quick Reference

| Function | Module | What it does | Example |
|----------|--------|--------------|---------|
| `mkdir(path)` | Tier 1 | Creates a directory | `mkdir("data")` |
| `rm(path)` | Tier 1 | Removes a file or directory | `rm("temp.txt")` |
| `cp(src, dst)` | Tier 1 | Copies a file or directory | `cp("a.txt", "b.txt")` |
| `mv(src, dst)` | Tier 1 | Moves a file or directory | `mv("a.txt", "backup/")` |
| `exists(path)` | Tier 1 | Checks if a path exists | `exists("data.txt")` → `true` |
| `is_file(path)` | Tier 1 | Checks if path is a file | `is_file("data.txt")` → `true` |
| `is_dir(path)` | Tier 1 | Checks if path is a directory | `is_dir("data/")` → `true` |
| `ls(path)` | Tier 1 | Lists directory contents | `ls(".")` → `["a.txt", "data/"]` |
| `read(path)` | Tier 1 | Reads file contents as string | `read("data.txt")` → `"hello"` |
| `write(path, content)` | Tier 1 | Writes string to file | `write("out.txt", "hello")` |
| `append(path, content)` | Tier 1 | Appends to file | `append("log.txt", "entry")` |
| `file_size(path)` | Tier 1 | Returns file size in bytes | `file_size("data.txt")` → `1024` |
| `ls_all(path)` | Tier 2 (fs) | Lists including hidden files | `ls_all(".")` → `[".hidden", "data.txt"]` |
| `touch(path)` | Tier 2 (fs) | Creates empty file or updates timestamp | `touch("newfile.txt")` |
| `cat(path)` | Tier 2 (fs) | File contents as string | `cat("data.txt")` |
| `read_lines(path)` | Tier 2 (fs) | Returns list of lines | `read_lines("data.txt")` → `["line1", "line2"]` |
| `write_lines(path, list)` | Tier 2 (fs) | Writes list of strings | `write_lines("out.txt", ["a","b"])` |
| `symlink(target, link_path)` | Tier 2 (fs) | Creates symbolic link | `symlink("data.txt", "link.txt")` |
| `hardlink(target, link_path)` | Tier 2 (fs) | Creates hard link | `hardlink("data.txt", "link.txt")` |
| `ln(target, link[, soft])` | Tier 2 (fs) | Creates link (soft by default) | `ln("data.txt", "link.txt")` |
| `readlink(path)` | Tier 2 (fs) | Resolves symlink to real path | `readlink("link.txt")` → `"data.txt"` |
| `chmod(path, mode)` | Tier 2 (fs) | Changes permissions | `chmod("script.sh", 755)` |
| `chown(path, user, group)` | Tier 2 (fs) | Changes owner (Unix only) | `chown("file.txt", "alice", "staff")` |
| `chgrp(path, group)` | Tier 2 (fs) | Changes group (Unix only) | `chgrp("file.txt", "staff")` |
| `stat(path)` | Tier 2 (fs) | Returns full metadata map | `stat("data.txt")` |
| `modified_time(path)` | Tier 2 (fs) | Last modification timestamp | `modified_time("data.txt")` → `1234567890` |
| `created_time(path)` | Tier 2 (fs) | Creation timestamp | `created_time("data.txt")` |
| `find(dir, pattern)` | Tier 2 (fs) | Recursively find files (glob) | `find(".", "*.txt")` |
| `find_regex(dir, pattern)` | Tier 2 (fs) | Recursively find files (regex) | `find_regex(".", "[a-z]+\.txt")` |
| `locate(name)` | Tier 2 (fs) | Finds file by exact name | `locate("config.xel")` |
| `glob(pattern)` | Tier 2 (fs) | Expands glob relative to CWD | `glob("*.xel")` |
| `file_diff(file1, file2)` | Tier 2 (fs) | Line‑by‑line diff | `file_diff("a.txt", "b.txt")` |
| `tree(path[, depth])` | Tier 2 (fs) | Directory tree as string | `tree(".")` |
| `extension(path)` | Tier 2 (fs) | File extension | `extension("data.txt")` → `".txt"` |
| `stem(path)` | Tier 2 (fs) | Filename without extension | `stem("data.txt")` → `"data"` |
| `disk_usage(path)` | Tier 2 (fs) | Total size in bytes | `disk_usage(".")` → `4096` |
| `disk_free(path)` | Tier 2 (fs) | Free disk space | `disk_free(".")` → `123456789` |
| `xxd(path)` | Tier 2 (fs) | Hex dump of file | `xxd("binary.dat")` |
| `strings(path[, min_len])` | Tier 2 (fs) | Extract printable strings | `strings("binary.dat", 4)` |

---

## Tier 1 – Core Filesystem Operations

### `mkdir(path)`

Creates a directory (including parent directories if needed).

```xell
mkdir("data")
mkdir("data/subdir")   # creates data/ then data/subdir/
```

**Throws:** `IOError` on failure (permission denied, invalid path, etc.).

### `rm(path)`

Removes a file or directory. For directories, removes recursively (like `rm -rf`).

```xell
rm("temp.txt")        # removes file
rm("data/")           # removes directory and all contents
```

**Throws:** `FileNotFoundError` if the path doesn't exist; `IOError` on other failures.

### `cp(src, dst)`

Copies a file or directory (recursively for directories).

```xell
cp("source.txt", "backup.txt")
cp("data/", "backup/")   # copies entire directory recursively
```

**Throws:** `FileNotFoundError` if `src` doesn't exist; `IOError` on other failures.

### `mv(src, dst)`

Moves a file or directory.

```xell
mv("old.txt", "new.txt")
mv("data/", "archive/")   # moves directory
```

**Throws:** `FileNotFoundError` if `src` doesn't exist; `IOError` on other failures.

### `exists(path)`

Returns `true` if the path exists, `false` otherwise.

```xell
if exists("config.xel"):
    print("Config found")
else:
    print("Config missing")
;
```

### `is_file(path)`

Returns `true` if the path exists and is a regular file.

```xell
is_file("data.txt")   # true (if it's a file)
is_file("data/")      # false (directory)
```

### `is_dir(path)`

Returns `true` if the path exists and is a directory.

```xell
is_dir("data/")   # true (if it's a directory)
is_dir("file")    # false (if it's a file)
```

### `ls(path)`

Returns a list of entry names in a directory (excluding `.` and `..`).

```xell
let entries = ls(".")
for entry in entries:
    print(entry)
;
```

**Expected output (varies):**
```
data.txt
README.xel
src/
```

### `read(path)`

Reads the entire contents of a file as a string.

```xell
let content = read("data.txt")
print(content)   # contents of data.txt
```

**Throws:** `FileNotFoundError` if the file doesn't exist; `IOError` on other failures.

### `write(path, content)`

Writes a string to a file (overwrites existing content).

```xell
write("output.txt", "Hello, world!")
```

**Throws:** `IOError` on failure (permission denied, invalid path, etc.).

### `append(path, content)`

Appends a string to the end of a file (creates the file if it doesn't exist).

```xell
append("log.txt", "New entry\n")
```

**Throws:** `IOError` on failure.

### `file_size(path)`

Returns the size of a file in bytes.

```xell
let size = file_size("data.txt")
print("File size: " + str(size) + " bytes")
```

**Throws:** `FileNotFoundError` if the file doesn't exist; `IOError` on other failures.

---

## Tier 2 – Extended Filesystem Operations (`bring * from "fs"`)

### Listing Files

#### `ls_all(path)`

Lists directory contents including hidden files (those starting with `.` on Unix, or all files on Windows).

```xell
bring * from "fs"

let all = ls_all(".")
print(all)   # [".config", "data.txt", ".hidden"]
```

#### `tree(path[, depth])`

Prints a directory tree as a string. Optionally limits depth.

```xell
bring * from "fs"

let tree_str = tree(".")
print(tree_str)
```

**Expected output:**
```
.
├── src/
│   ├── main.xel
│   └── lib/
│       └── util.xel
├── data.txt
└── README.xel
```

### Creating and Updating Files

#### `touch(path)`

Creates an empty file or updates its modification timestamp.

```xell
bring * from "fs"

touch("newfile.txt")   # creates empty file
touch("existing.txt")  # updates timestamp
```

#### `read_lines(path)`

Returns the file content as a list of lines (without newline characters).

```xell
bring * from "fs"

let lines = read_lines("data.txt")
for line in lines:
    print(line)
;
```

#### `write_lines(path, list)`

Writes a list of strings to a file, each on a new line.

```xell
bring * from "fs"

write_lines("out.txt", ["line1", "line2", "line3"])
```

#### `cat(path)`

Alias for `read()` – returns file contents as a string. Named for shell users.

```xell
bring * from "fs"

let content = cat("data.txt")
```

### Symlinks and Hard Links

#### `symlink(target, link_path)`

Creates a symbolic link (symlink). On Windows, requires admin privileges or developer mode.

```xell
bring * from "fs"

symlink("/usr/bin/python", "python")
```

#### `hardlink(target, link_path)`

Creates a hard link (same inode, multiple names).

```xell
bring * from "fs"

hardlink("data.txt", "backup.txt")
```

#### `ln(target, link[, soft])`

Creates a link – soft (symlink) by default, or hard if `soft = false`.

```xell
bring * from "fs"

ln("data.txt", "link.txt")        # soft link
ln("data.txt", "hard.txt", false) # hard link
```

#### `readlink(path)`

Resolves a symbolic link to its target path.

```xell
bring * from "fs"

let target = readlink("link.txt")
print(target)   # "data.txt"
```

### Permissions and Ownership

#### `chmod(path, mode)`

Changes file permissions. `mode` is an integer (e.g., `755` for `rwxr-xr-x`).

```xell
bring * from "fs"

chmod("script.sh", 755)
chmod("config.xel", 644)
```

**Windows note:** Limited support – sets read‑only flag based on write permission.

#### `chown(path, user, group)`

Changes file owner and group (Unix only). On Windows, this is a no‑op.

```xell
bring * from "fs"

chown("data.txt", "alice", "staff")
```

#### `chgrp(path, group)`

Changes file group (Unix only). On Windows, this is a no‑op.

```xell
bring * from "fs"

chgrp("data.txt", "staff")
```

### Metadata and Information

#### `stat(path)`

Returns a map with full file metadata:

| Field | Type | Description |
|-------|------|-------------|
| `path` | string | Absolute path |
| `is_file` | bool | True if regular file |
| `is_dir` | bool | True if directory |
| `is_symlink` | bool | True if symbolic link |
| `size` | int | File size in bytes |
| `modified_time` | int | Last modification timestamp (Unix seconds) |
| `permissions` | int | Permission bits |
| `type` | string | `"file"`, `"directory"`, `"symlink"`, etc. |

```xell
bring * from "fs"

let info = stat("data.txt")
print("Type: " + info["type"])
print("Size: " + str(info["size"]))
print("Modified: " + str(info["modified_time"]))
```

#### `modified_time(path)`

Returns the last modification timestamp as a Unix timestamp (seconds since epoch).

```xell
bring * from "fs"

let mtime = modified_time("data.txt")
print("Last modified: " + str(mtime))
```

#### `created_time(path)`

Returns the creation timestamp as a Unix timestamp (seconds since epoch). On some systems, this may fall back to `modified_time` if creation time is not available.

```xell
bring * from "fs"

let ctime = created_time("data.txt")
print("Created: " + str(ctime))
```

### Finding and Searching

#### `find(dir, pattern)`

Recursively finds files whose names match a glob pattern.

```xell
bring * from "fs"

let xel_files = find(".", "*.xel")
for f in xel_files:
    print(f)
;
```

#### `find_regex(dir, pattern)`

Recursively finds files whose names match a regex pattern.

```xell
bring * from "fs"

let files = find_regex(".", "[a-z]+_test\.xel")
print(files)
```

#### `locate(name)`

Finds files by exact name, searching recursively from the current working directory.

```xell
bring * from "fs"

let results = locate("config.xel")
if not is_empty(results):
    print("Found at: " + results[0])
else:
    print("Not found")
;
```

#### `glob(pattern)`

Expands a glob pattern relative to the current working directory. Returns a list of matching paths.

```xell
bring * from "fs"

let files = glob("*.txt")
print(files)   # ["a.txt", "b.txt"]
```

### Comparing and Patching

#### `file_diff(file1, file2)`

Returns a list of diff entries: lines that differ between two files. Each entry is in the format:
- `"+N: text"` – added line in file2
- `"-N: text"` – removed line from file1
- `"~N: -old +new"` – changed line

```xell
bring * from "fs"

let diff = file_diff("old.txt", "new.txt")
for entry in diff:
    print(entry)
;
```

### Disk Usage

#### `disk_usage(path)`

Returns the total size of a file or directory in bytes.

```xell
bring * from "fs"

let total = disk_usage(".")
print("Total size: " + str(total) + " bytes")
```

#### `disk_free(path)`

Returns the free disk space on the volume containing `path`.

```xell
bring * from "fs"

let free = disk_free(".")
print("Free space: " + str(free) + " bytes")
```

### Hex Dump and Strings

#### `xxd(path)`

Returns a hex dump of a file as a string.

```xell
bring * from "fs"

let hexdump = xxd("binary.dat")
print(hexdump)
```

**Expected output:**
```
00000000: 48 65 6c 6c 6f 20 57 6f 72 6c 64 21 0a        Hello World!.
```

#### `strings(path[, min_len])`

Extracts printable ASCII strings from a binary file. Returns a list of strings. The `min_len` parameter defaults to `4`.

```xell
bring * from "fs"

let strings_found = strings("binary.dat", 4)
for s in strings_found:
    print(s)
;
```

---

## Complete Example: Directory Cleanup

```xell
# Clean up old log files and backup old data
bring * from "fs"

# Check if temp directory exists
let temp_dir = "temp"

if not exists(temp_dir):
    mkdir(temp_dir)
    print("Created temp directory")
;

# Write some logs
append("temp/log.txt", "Started process\n")
append("temp/log.txt", "Processing data...\n")

# List all files
let files = ls(temp_dir)
print("Files in " + temp_dir + ": " + join(files, ", "))

# Get file size of log
if exists("temp/log.txt"):
    let size = file_size("temp/log.txt")
    print("Log size: " + str(size) + " bytes")
;

# Clean up old temp files
for f in files:
    if f != "log.txt":
        rm(temp_dir + "/" + f)
        print("Removed: " + f)
    ;
;
```

**Expected output:**
```
Created temp directory
Files in temp: log.txt
Log size: 42 bytes
```

---

## What NOT to Do

### 1. Using `rm` Without Checking if the Path Exists

```xell
rm("nonexistent.txt")   # ❌ FileNotFoundError
```

**Fix:** Use `exists()` to check first, or wrap in `try`/`catch`.

### 2. Using `read` on a Directory

```xell
read("data/")   # ❌ IOError – cannot read a directory as a file
```

### 3. Using `write` Without a Parent Directory

```xell
write("newdir/file.txt", "content")   # ❌ IOError if newdir doesn't exist
```

**Fix:** Ensure parent directories exist first with `mkdir`.

### 4. Assuming `ls` Returns Full Paths

`ls` returns only filenames, not full paths. Use `join_path` (from `stdlib-path.md`) to build full paths.

```xell
ls(".")   # ["a.txt", "b.txt"] – not ["./a.txt", "./b.txt"]
```

### 5. Forgetting to `bring` the `fs` Module for Extended Operations

```xell
stat("data.txt")   # ❌ error – stat is not defined
```

**Fix:** `bring * from "fs"` first.

### 6. Using `chmod` with Decimal Instead of Octal

```xell
chmod("script.sh", 755)   # Actually decimal 755, not octal 0755
```

In Xell, `755` is parsed as decimal. The `chmod` function handles conversion internally, so `755` works as expected.

### 7. Expecting `symlink` to Work Without Admin Privileges on Windows

On Windows, creating symbolic links requires Developer Mode or admin privileges. The call will fail if not available.

### 8. Using `tree` on Very Large Directories

`tree` traverses the entire directory structure – use `depth` to limit recursion for large directories.

---

## See Also

- [Path Utilities](stdlib-path.md) – path manipulation functions
- [Process Management](stdlib-process.md) – `run`, `run_capture`, `pid`
- [Environment Variables](stdlib-env.md) – `env_get`, `env_set`
- [Error Handling](../core/error-handling.md) – handling filesystem errors

---

**Previous:** [Higher‑Order Functions](stdlib-hof.md)  
**Next:** [Path Utilities](stdlib-path.md)