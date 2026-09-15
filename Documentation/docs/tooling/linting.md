# Linting – Static Analysis

> **Previous:** [CLI Commands](cli.md)  
> **Next:** [Dialect Conversion](dialect-conversion.md)

---

Xell includes a built‑in static analyzer and linter that catches common errors, unused variables, type mismatches, and other issues **without executing your code**. This is fast, safe, and ideal for editor integration (LSP), CI/CD pipelines, and pre‑commit hooks.

---

## Quick Reference

| Command | What it does | Example |
|---------|--------------|---------|
| `xell --check [file]` | Lints a file (or stdin) | `xell --check script.xel` |
| `xell --lint [file]` | Alias for `--check` | `xell --lint script.xel` |
| `xell --check-string "<code>"` | Lints raw source string | `xell --check-string "let x = 10"` |
| `xell --check-symbols [file]` | Lints + outputs symbol map JSON | `xell --check-symbols script.xel` |
| Error severity | `error` – must be fixed | `[XELL ERROR] Line 5 — ParseError` |
| Warning severity | `warning` – best practice | `[XELL WARNING] Line 10 — unused variable` |
| Hint severity | `hint` – informational | `[XELL HINT] Line 3 — consider using immutable` |

---

## Core Linting Commands

### `xell --check <file>` / `xell --lint <file>`

Performs a shallow linting pass:

1. **Lex** – tokenises the source.
2. **Parse with error recovery** – builds a partial AST even if there are syntax errors.
3. **Static analysis** – checks for unused variables, type inconsistencies, and common mistakes.
4. **Diagnostic output** – prints errors, warnings, and hints to stderr.

```bash
$ xell --check script.xel
[XELL ERROR] Line 5 — ParseError: Expected ':' after if condition
[XELL WARNING] Line 10 — unused variable 'x'
[XELL HINT] Line 3 — consider using 'immutable' for constant value
```

Exit code is `0` if no errors are found (warnings/hints do not affect exit code), and `1` if there is at least one error.

### `xell --check-string "<code>"`

Lints a raw source string without reading from a file. Ideal for IDE integrations (linting unsaved buffers).

```bash
$ xell --check-string "let x = 10; print(x +)"
[XELL ERROR] Line 1 — ParseError: Unexpected token: EOF
```

### `xell --check-symbols [file]`

Performs linting **and** outputs a JSON symbol map of all definitions and references on stdout. Used by the LSP for:

- Autocompletion
- Go‑to‑definition
- Find‑references
- Symbol outline

The JSON format is a list of symbol objects:

```json
[
  {
    "name": "main",
    "kind": "function",
    "line": 1,
    "detail": "fn main()",
    "parentScope": "global",
    "sourceFile": "/project/script.xel"
  },
  {
    "name": "x",
    "kind": "variable",
    "line": 3,
    "detail": "int",
    "parentScope": "main",
    "sourceFile": "/project/script.xel"
  }
]
```

**Symbol kinds:** `function`, `method`, `class`, `struct`, `interface`, `enum`, `module`, `variable`, `parameter`, `constant`.

---

## Diagnostic Severity Levels

| Severity | Description | Exit Code Impact |
|----------|-------------|------------------|
| `[XELL ERROR]` | Prevents the code from running correctly. Must be fixed. | `1` (failure) |
| `[XELL WARNING]` | Indicates a potential problem or bad practice. Should be reviewed. | `0` (success) |
| `[XELL HINT]` | Informational suggestion for improvement. | `0` (success) |

### Example Output

```
[XELL ERROR] Line 5 — ParseError: Expected ')' after function arguments
[XELL WARNING] Line 10 — unused variable 'temp' in function 'process'
[XELL HINT] Line 2 — consider using 'immutable' for constant value 'PI'
```

---

## Static Analyzer Checks

The Xell static analyzer currently performs the following checks:

### Syntax and Parse Errors

- Missing delimiters (`:`, `;`, `)`, `]`, `}`)
- Invalid syntax (e.g., `let x =` without a value)
- Unexpected tokens

### Unused Variables

- Detects variables that are defined but never used.
- Scoped correctly (ignores variables used in nested scopes).

```xell
fn example():
    let x = 10      # ❌ unused variable warning
    let y = 20
    print(y)
;
```

### Unused Imports

- Detects `bring` statements for modules that are never used.
- Scoped correctly (ignores imports used in nested scopes).

```xell
bring Math    # ❌ unused import warning
bring JSON    # ✅ used
JSON->parse("{}")
```

### Type Inconsistencies (Basic)

- Detects obvious type mismatches (e.g., adding a string and a number).
- Detects calling a non‑function.

```xell
let x = "hello"
let y = x + 5    # ❌ type error – string + int

let z = 10
z()              # ❌ type error – int is not callable
```

### Unreachable Code

- Detects code after a `give` (return) statement.

```xell
fn example():
    give 42
    print("done")   # ❌ unreachable code warning
;
```

### Missing `else` in `if` Expressions

Expression‑mode `if` requires an `else` branch. The analyzer catches missing `else`.

```xell
let result = if x > 0 : "positive"    # ❌ error – missing else branch
```

---

## LSP Integration

The `--check-symbols` flag is designed for Language Server Protocol integration. The IDE can:

1. Run `xell --check-symbols <current_file>` on every keystroke.
2. Parse the JSON symbol map for autocomplete and navigation.
3. Parse the stderr diagnostics for inline error highlighting.

**Performance:** The checker is fast (sub‑millisecond for typical files) and safe (no code execution), making it suitable for real‑time editor integration.

---

## CI/CD Integration

You can use `xell --check` in CI pipelines to enforce code quality:

```bash
#!/bin/bash
# pre-commit hook or CI step
xell --check src/*.xel
if [ $? -ne 0 ]; then
    echo "Linting failed! Please fix errors before committing."
    exit 1
fi
```

**With git pre‑commit hook:**

```yaml
# .pre-commit-config.yaml
- repo: local
  hooks:
    - id: xell-lint
      name: Xell linter
      entry: xell --check
      language: system
      files: \.xel$
```

---

## Complete Example

```xell
# buggy.xel
bring Math

fn calculate(x):
    let unused = 42
    give x * 2
    print("done")
;

let result = calculate("10")
print(result)

let y = 100
```

**Running the linter:**

```bash
$ xell --check buggy.xel
[XELL WARNING] Line 1 — unused import 'Math'
[XELL WARNING] Line 4 — unused variable 'unused' in function 'calculate'
[XELL WARNING] Line 5 — unreachable code after 'give'
[XELL ERROR] Line 7 — TypeError: cannot multiply int by string
[XELL WARNING] Line 10 — unused variable 'y'
```

---

## What NOT to Do

### 1. Relying Solely on Linting for Type Safety

Xell is dynamically typed. The linter catches only obvious type mismatches. It cannot guarantee type correctness.

### 2. Forgetting That `--check` Does Not Execute the Code

`--check` only does static analysis. It does **not** detect runtime errors like:
- Index out of range
- Key not found
- Division by zero

Use `--trace-vars` or `--debug` for runtime debugging.

### 3. Ignoring Warnings and Hints

While they don't affect the exit code, warnings often indicate real problems or code smell.

### 4. Using `--check` on Incomplete Code (e.g., In the Middle of Editing)

The parser has error recovery, so incomplete code will produce errors, but it may also produce spurious warnings (e.g., unused variables that are about to be used). Use the IDE integration for real‑time feedback on unsaved buffers.

### 5. Expecting the Linter to Catch All Unused Variables Across Files

The linter is **file‑scoped**. Unused exports across modules are not detected.

### 6. Using `--check` Without a `.xell_meta` Index

Module resolution is file‑based. The linter can resolve imports only if the modules are available on the filesystem or in the `XELL_PATH`.

---

## See Also

- [CLI Commands](cli.md) – all command‑line flags
- [Dialect Conversion](dialect-conversion.md) – `--convert`, `--revert`
- [Debugging](debugging.md) – `--trace-vars`, `--debug`
- [Module Indexing](module-indexing.md) – `--make_module`

---

**Previous:** [CLI Commands](cli.md)  
**Next:** [Dialect Conversion](dialect-conversion.md)