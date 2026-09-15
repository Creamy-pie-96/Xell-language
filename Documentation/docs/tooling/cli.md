# CLI – Command‑Line Interface

> **Previous:** [REPL Usage](repl.md)  
> **Next:** [Linting](linting.md)

---

Xell provides a rich command‑line interface for running scripts, linting, debugging, converting dialects, generating module indexes, and more. This page covers every flag and option available from the terminal.

---

## Quick Reference

| Command | What it does | Example |
|---------|--------------|---------|
| `xell` | Starts the REPL | `$ xell` |
| `xell <file>` | Executes a script | `$ xell script.xel` |
| `-e "<code>"` | Executes code string | `$ xell -e "print(42)"` |
| `--check [file]` | Lints a file (or stdin) | `$ xell --check script.xel` |
| `--lint [file]` | Alias for `--check` | `$ xell --lint script.xel` |
| `--check-string "<code>"` | Lints raw source string | `$ xell --check-string "let x = 10"` |
| `--check-symbols [file]` | Lints + outputs symbol map JSON | `$ xell --check-symbols script.xel` |
| `--trace-vars <file>` | Runs with variable lifecycle tracing | `$ xell --trace-vars script.xel` |
| `--debug <file>` | Runs with debug IPC server | `$ xell --debug script.xel` |
| `--convert <file> [map.xesy]` | Converts dialect file to canonical | `$ xell --convert script.xel` |
| `--revert <file> [map.xesy]` | Restores dialect from canonical | `$ xell --revert script.xel` |
| `--gen_xesy [output.xesy]` | Generates template mapping file | `$ xell --gen_xesy dialect.xesy` |
| `--make_module [--update] <paths...>` | Builds `.xell_meta` and cache | `$ xell --make_module .` |
| `--terminal` | Launches the Xell Terminal GUI | `$ xell --terminal` |
| `--ide` | Alias for `--terminal` | `$ xell --ide` |
| `--customize` | Launches color customizer web app | `$ xell --customize` |
| `--kernel` | Runs as notebook kernel | `$ xell --kernel` |
| `--version`, `-v` | Prints version information | `$ xell --version` |
| `--help`, `-h` | Prints usage help | `$ xell --help` |
| `--source-dir <dir>` | Overrides module resolution directory | `$ xell script.xel --source-dir /project` |

---

## Running Scripts

### `xell <file.xel>`

Executes a Xell script file. Any arguments after the filename are passed to the script via the global `__args__` list.

```bash
$ xell script.xel
Hello, World!
```

**With arguments:**

```bash
$ xell script.xel --verbose --config config.xel
```

```xell
# Inside script.xel:
print(__args__)   # ["--verbose", "--config", "config.xel"]
```

**With `--source-dir` override:**

```bash
$ xell script.xel --source-dir /path/to/project
```

This tells the module resolver to treat the script as if it were located in `/path/to/project` for relative module resolution. Useful when the script is in a temporary directory but should resolve imports from a project directory (e.g., IDE usage).

---

### `xell -e "<code>"` or `xell --eval "<code>"`

Executes a Xell source string directly, without creating a file.

```bash
$ xell -e "print(2 + 2)"
4

$ xell -e "let x = 10; print(x * 2)"
20
```

Multi‑line code is supported with `\n` or by quoting multiple lines:

```bash
$ xell -e "fn add(a, b): give a + b ; print(add(5, 3))"
8
```

---

## Linting and Static Analysis

### `xell --check [file]` / `xell --lint [file]`

Performs a shallow linting pass: lex + parse (with error recovery) + static analysis. Does **not** execute the code. If no file is provided, reads from stdin.

```bash
$ xell --check script.xel
[XELL ERROR] Line 5 — TypeError: expected ':' after if condition
[XELL WARNING] Line 10 — unused variable 'x'
```

**With stdin:**

```bash
$ cat script.xel | xell --check
```

### `xell --check-string "<code>"`

Lints a raw source string without reading from a file. Ideal for IDE integrations.

```bash
$ xell --check-string "let x = 10; print(x +)"
[XELL ERROR] Line 1 — ParseError: Unexpected token: EOF
```

### `xell --check-symbols [file]`

Performs linting and additionally outputs a JSON symbol map of all definitions and references on stdout. Used by the LSP for autocompletion.

```bash
$ xell --check-symbols script.xel
[
  {"name": "main", "kind": "function", "line": 1, ...},
  {"name": "x", "kind": "variable", "line": 5, ...}
]
```

---

## Debugging and Tracing

### `xell --trace-vars <file>`

Runs the script with variable lifecycle tracing enabled. Outputs a JSON array of trace events (variable births, changes, function calls, returns, etc.) to stdout.

```bash
$ xell --trace-vars script.xel
[
  {"timestamp":1234567890,"event":"VAR_BORN","name":"x","value":"10","line":1},
  {"timestamp":1234567891,"event":"FN_CALLED","name":"square","line":3},
  ...
]
```

This is used by the IDE's **LIFECYCLE** tab for lazy variable inspection.

### `xell --debug <file>`

Runs the script with an active debug server. The interpreter listens on a Unix socket (or named pipe on Windows) for IDE debug commands (step, continue, breakpoint add/remove, etc.).

```bash
$ xell --debug script.xel
XELL_DEBUG_SOCKET:/tmp/xell-debug-12345.sock
```

The IDE connects to the socket and can control execution step‑by‑step. The socket path is printed to stderr for the IDE to read.

**Optional `--source-dir`** works with `--debug` too:

```bash
$ xell --debug script.xel --source-dir /project
```

---

## Dialect Conversion

### `xell --convert <file> [map.xesy]`

Converts a dialect file to canonical Xell syntax **in‑place**. The mapping file (`.xesy`) specifies the custom‑to‑canonical word mappings. If no mapping file is given, Xell looks for a `.xesy` file in the same directory as the source file.

```bash
$ xell --convert script.xel
✓ Converted 'script.xel' to canonical Xell
  Mapping: dialect.xesy
  Revert info saved to: script.xel.xesy.revert
```

### `xell --revert <file> [map.xesy]`

Restores a canonical Xell file back to dialect syntax **in‑place**, re‑adding the `@convert` directive. Uses the same mapping file.

```bash
$ xell --revert script.xel
✓ Reverted 'script.xel' to dialect syntax
  Mapping: dialect.xesy
  @convert directive re-added
```

### `xell --gen_xesy [output.xesy]`

Generates a template `.xesy` mapping file with all canonical Xell keywords and built‑in function names.

```bash
$ xell --gen_xesy dialect.xesy
✓ Generated template mapping file: dialect.xesy
  Fill in the values with your dialect words, then use:
    @convert "dialect.xesy"
```

---

## Module Indexing

### `xell --make_module [--update] <paths...>`

Builds `.xell_meta` and `__xelcache__/` for the specified directories or files. This indexes modules for fast resolution (bypassing slow‑path scanning).

```bash
$ xell --make_module . /lib /project/src
  ✓ Math (from math.xel)
  ✓ JSON (from json.xel)
  ✓ Parser (from parser.xel)
  📝 Wrote .xell_meta
✅ Registered 3 module(s) from 3 file(s).
```

**With `--update`:**

```bash
$ xell --make_module --update .
  ⏭  math.xel (unchanged)
  ✓ json.xel (changed)
  ⏭  parser.xel (unchanged)
  📝 Wrote .xell_meta
```

Only files that have changed (based on SHA‑256 hash) are re‑indexed. Unchanged entries are preserved from the existing `.xell_meta`.

---

## Terminal GUI

### `xell --terminal` or `xell --ide`

Launches the Xell Terminal GUI (SDL2‑based) – a rich graphical interface for running scripts, editing code, and debugging.

```bash
$ xell --terminal
🖥️  Launching Xell Terminal...
```

### `xell --customize`

Launches the color customizer web app (Python server). This lets you preview and customize the terminal's color scheme.

```bash
$ xell --customize
🎨 Launching Xell Color Customizer...
   Server: /usr/local/share/xell/color_customizer/customizer_server.py
```

---

## Notebook Kernel

### `xell --kernel`

Runs Xell as a JSON‑over‑stdout notebook kernel. Used for integration with Jupyter‑like notebooks or custom frontends.

```bash
$ xell --kernel
{"status":"kernel_ready","version":"0.1.0"}
```

The kernel accepts JSON commands on stdin:

```json
{"action":"execute","cell_id":"cell_1","code":"print(42)"}
```

And responds with:

```json
{"cell_id":"cell_1","status":"ok","stdout":"42\n","stderr":"","execution_count":1}
```

---

## Version and Help

### `xell --version` or `xell -v`

Prints version information.

```bash
$ xell --version
Xell Language v0.1.0
```

### `xell --help` or `xell -h`

Prints usage help with all available commands.

```bash
$ xell --help
Usage:
  xell                  Start interactive REPL
  xell <file.xel>       Execute a Xell script
  xell -e "<code>"      Execute a Xell source string directly
  ...
```

---

## Environment Variables

### `XELL_PATH`

Specifies additional directories for module resolution (colon‑separated on Unix, semicolon‑separated on Windows).

```bash
export XELL_PATH=/custom/lib:/usr/local/xell/lib
xell script.xel
```

### `XELL_HOME`

Points to the Xell installation directory. Used to locate resources like terminal binaries, color customizer, and standard library.

```bash
export XELL_HOME=/opt/xell
xell script.xel
```

### `XELL_DEBUG_SOCKET`

When `--debug` is used, the socket path is printed to stderr. The IDE reads this to connect to the debug session.

---

## What NOT to Do

### 1. Using `--check` Without Saving the File

`--check` operates on the file as it exists on disk. For unsaved buffer linting, use `--check-string` (which accepts raw source).

### 2. Forgetting the Semicolon in `-e` Code

```bash
$ xell -e "let x = 10"   # ❌ prints "10" (expression mode)
```

**Fix:** Add a semicolon for statement mode:
```bash
$ xell -e "let x = 10 ;"
```

### 3. Running `--convert` Without a Backup

`--convert` modifies the file **in‑place**. Always version‑control your files or keep a backup.

### 4. Using `--make_module` on a Directory Without `.xel` Files

```bash
$ xell --make_module empty_dir
No .xel or .xell files found.
```

### 5. Expecting `--trace-vars` to Work Without `@debug on`

`--trace-vars` enables tracing by default. If you also have `@debug off` in the source, the trace will be empty.

### 6. Using `--kernel` in a Terminal Expecting Human Interaction

`--kernel` is for programmatic integration. It expects JSON on stdin and outputs JSON on stdout; it is not interactive.

### 7. Expecting `--customize` to Work Without Python3

The color customizer is a Python web app. Python3 must be installed and accessible.

### 8. Using `--source-dir` Without a Script File

`--source-dir` only works with `xell <file>` and `--debug <file>`.

### 9. Forgetting That `--terminal` Requires SDL2

The Xell Terminal GUI requires SDL2 and SDL2_ttf libraries to be installed.

---

## See Also

- [REPL Usage](repl.md) – interactive shell
- [Linting](linting.md) – static analysis flags
- [Dialect Conversion](dialect-conversion.md) – `--convert`, `--revert`, `--gen_xesy`
- [Module Indexing](module-indexing.md) – `--make_module`
- [Debugging](debugging.md) – `--trace-vars`, `--debug`
- [Kernel](kernel.md) – `--kernel` JSON protocol
- [Terminal GUI](terminal-gui.md) – `--terminal`, `--ide`, `--customize`

---

**Previous:** [REPL Usage](repl.md)  
**Next:** [Linting](linting.md)