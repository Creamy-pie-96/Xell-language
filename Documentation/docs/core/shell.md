# Shell – External Commands & Pipelines

> **Previous:** [Async](async.md)  
> **Next:** [Decorators](decorators.md)

---

Xell integrates deeply with the operating system shell. You can execute external commands, capture their output, build pipelines, and control error handling – all with concise, readable syntax. This makes Xell an excellent choice for scripting, automation, and devops tasks.

---

## Quick Reference

| Keyword/Syntax | What it does | Minimal example |
|----------------|--------------|-----------------|
| `$<command>` | Executes a shell command and captures its output | `$ls -la` |
| `\|>` | Pipes two command strings together | `"ls" |> "grep xel"` |
| `&&` | Short‑circuiting shell AND (run right only if left succeeds) | `$ls && $echo "done"` |
| `\|\|` | Short‑circuiting shell OR (run right only if left fails) | `$ls /nonexistent \|\| $echo "fail"` |
| `set_e()` | Enables exit‑on‑error mode for external commands | `set_e()` |
| `last_exit_code` | Returns the exit code of the last executed command | `$false ; print(last_exit_code)` |
| `run(cmd)` | Executes a command and returns its exit code | `run("ls")` |
| `run_capture(cmd)` | Executes and returns `{stdout, stderr, exit_code}` | `let r = run_capture("ls")` |
| PATH fallback | Unknown function names are tried as external commands | `git status` (no `$` needed) |

---

## Shell Command Literal: `$cmd`

The simplest way to run a command is with the `$` prefix. It executes the command and captures its stdout.

```xell
$ls -la
```

- The command is passed to the shell (`/bin/sh` on Unix, `cmd.exe` on Windows).
- Output is captured and printed directly when used as a statement.
- When used in an expression, it returns a **list of lines** (stdout split by newline).

### Example: Listing files

```xell
# As a statement – prints output immediately
$ls

# As an expression – captures lines into a list
let files = $ls
print("Number of files: " + str(len(files)))
print("First file: " + files[0])
```

**Expected output** (varies by directory):
```
file1.txt
file2.xel
...
Number of files: 42
First file: file1.txt
```

---

## Shell Pipelines: `|>`

The `|>` operator concatenates two command strings with `" | "`, building a pipeline:

```xell
let cmd = "ls -la" |> "grep .xel"
$cmd   # executes: ls -la | grep .xel
```

- Both operands must be strings.
- The result is a new string containing the piped command.
- Useful for building complex pipelines dynamically.

### Example: Dynamic pipeline

```xell
let filter = "grep error"
let cmd = "tail -f log.txt" |> filter
$cmd   # tail -f log.txt | grep error
```

---

## Shell AND (`&&`) and OR (`||`)

These operators provide conditional execution based on command success/failure.

### `&&` – Run only if the left succeeds

```xell
$mkdir newdir && $echo "Directory created"
```

- The left side is evaluated.
- If its exit code is **0** (or the value is truthy), the right side runs.
- If the left fails (non‑zero exit code or falsy value), the right is skipped.

**Truthiness rules for `&&` / `||`:**
- Numeric `0` → success (truthy)
- Map with `"exit_code": 0` → success (truthy)
- Any other truthy value → success
- Non‑zero numeric, map with non‑zero exit_code, or falsy value → failure

### `||` – Run only if the left fails

```xell
$rm file.txt || $echo "File not found"
```

- If the left fails (non‑zero exit code or falsy), the right runs.
- If the left succeeds, the right is skipped.

### Example: Safe directory creation

```xell
$mkdir project || $echo "Directory already exists"
```

**Output:**
```
# If directory exists:
Directory already exists
# If directory didn't exist:
(no output from mkdir)
```

---

## `set_e()` – Exit‑on‑Error Mode

Enable strict mode where any command that exits with a non‑zero code throws a `CommandFailedError`:

```xell
set_e()
$ls /nonexistent   # throws CommandFailedError
```

- After `set_e()` is called, **all** external commands (`$`, `run()`, `run_capture()`) will throw on failure.
- This is similar to `set -e` in bash.
- Disable with `set_e(false)` (or `set_e(0)`).

### Example: Script with strict error handling

```xell
set_e()
try :
    $cp important.txt backup/
    $rm important.txt
catch e :
    print("Copy failed: " + e->message)
;
```

**Output:**
```
Copy failed: command exited with code 1: cp important.txt backup/
```

---

## `last_exit_code`

The exit code of the last executed command is stored in `last_exit_code`:

```xell
$ls /tmp
print(last_exit_code)   # 0

$ls /nonexistent
print(last_exit_code)   # non‑zero (e.g., 2)
```

- Works for `$`, `run()`, and `run_capture()`.
- If no command has been run, `last_exit_code` is `0`.

---

## Built‑in Command Functions

### `run(command)` – Execute and return exit code

```xell
let code = run("ls -la")
if code == 0 :
    print("Success")
else :
    print("Failed with code: " + str(code))
;
```

### `run_capture(command)` – Execute and capture stdout/stderr

```xell
let result = run_capture("ls /nonexistent")
print("Exit code: " + str(result["exit_code"]))
print("Stdout: " + result["stdout"])
print("Stderr: " + result["stderr"])
```

**Result map structure:**
- `stdout` – captured stdout (`string`)
- `stderr` – captured stderr (`string`)
- `exit_code` – integer exit code

---

## PATH‑based External Command Fallback

When you call an identifier that is **not** a built‑in function and **not** defined in the current scope, Xell checks if it matches an executable on your `PATH`. If so, it executes it as an external command.

```xell
git status          # runs `git status` (no $ needed)
python --version    # runs `python --version`
```

- This works **only** when the identifier is not a known Xell keyword.
- The command is invoked with the same arguments you pass.
- Output is returned as a string (stdout); if the command fails, an error is raised (unless `set_e()` is off, in which case the error is returned as a string containing both stdout and stderr).

### Example: Running `git` commands

```xell
let output = git log --oneline
print(output)
```

**Output:**
```
a1b2c3d Fix bug in parser
e4f5g6h Add new feature
...
```

---

## Shell Variables and Interpolation

Use `f`‑strings to inject variables into commands:

```xell
let filename = "data.txt"
$cat {filename}   # runs: cat data.txt
```

For complex arguments, use string concatenation:

```xell
let dir = "/tmp"
let pattern = "*.xel"
let cmd = "find " + dir + " -name '" + pattern + "'"
$cmd
```

---

## Common Patterns

### 1. Error‑handling with `set_e()` and `try`/`catch`

```xell
set_e()
try :
    $build.sh
    $deploy.sh
catch e :
    print("Deployment failed: " + e->message)
    exit(1)
;
```

### 2. Collecting command output as a list

```xell
let lines = $ls -1
for line in lines:
    process(line)
;
```

### 3. Conditional execution with `&&`

```xell
$mkdir build && $cd build && $cmake .. && $make
```

### 4. Silent failure with `||`

```xell
$rm tempfile || true   # ignore failure
```

### 5. Building dynamic pipelines

```xell
let base = "cat log.txt"
let filter = if verbose : "| grep -v debug" else : ""
let full_cmd = base + filter
$full_cmd
```

---

## What NOT to Do

### 1. Expecting `$cmd` to return structured data

```xell
let data = $ls -la
print(data[0]["name"])   # ❌ data is a list of strings, not structured
```

**Why it fails:** `$cmd` returns a list of **lines** (strings), not parsed file info. Use `ls` with `run_capture` and parse yourself if needed.

### 2. Using `&&` and `||` with non‑command expressions

```xell
let x = 10
x > 5 && $echo "big"   # ❌ left side is not a command
```

**Why it fails:** `&&` and `||` are intended for command success/failure; they use `truthy` rules, but it's clearer to use `if` for logic.

### 3. Forgetting to escape shell metacharacters

```xell
let unsafe = "file with spaces.txt"
$ls {unsafe}   # ❌ fails due to spaces
```

**Fix:** Use quotes around the variable.

```xell
$ls "{unsafe}"
```

### 4. Assuming `set_e()` affects already‑running commands

```xell
set_e()
$long_running &   # background command – set_e() does not affect it
```

`set_e()` only affects commands run **after** it is enabled, not those already running.

---

## See Also

- [Operators](operators.md) – for `&&`, `||`, and `|>` details
- [Error Handling](error-handling.md) – for `CommandFailedError`
- [Built‑ins](../stdlib/stdlib-process.md) – for `run`, `run_capture`, `pid`, `sleep`, `exit`
- [CLI](../tooling/cli.md) – for `--source-dir` and other flags

---

**Previous:** [Async](async.md)  
**Next:** [Decorators](decorators.md)