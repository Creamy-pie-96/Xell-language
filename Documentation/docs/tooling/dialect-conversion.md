# Dialect Conversion

> **Previous:** [Linting](linting.md)  
> **Next:** [Module Indexing](module-indexing.md)

---

Xell's dialect conversion system allows you to use **custom keywords** in your source code while still compiling to standard Xell. This is useful for creating domain‑specific languages, adapting to team‑preferred terminology, or writing code in a style that feels more natural for your use case.

Think of it as a **keyword translation layer** – you write code using your own words, and Xell translates it to canonical keywords before parsing and executing.

---

## Quick Reference

| Command / Syntax | What it does | Example |
|------------------|--------------|---------|
| `@convert "map.xesy"` | Declares dialect mapping at the top of a file | `@convert "dialect.xesy"` |
| `xell --convert <file> [map.xesy]` | Converts dialect file to canonical Xell **in‑place** | `xell --convert script.xel` |
| `xell --revert <file> [map.xesy]` | Restores dialect from canonical **in‑place** | `xell --revert script.xel` |
| `xell --gen_xesy [output.xesy]` | Generates a template `.xesy` mapping file | `xell --gen_xesy dialect.xesy` |
| `.xesy` file format | JSON mapping: canonical → custom | `{ "fn": "def", "give": "return" }` |
| `.xesy.revert` sidecar | Stores the mapping path used for `--convert` | `script.xel.xesy.revert` |

---

## The `@convert` Directive

To use a dialect mapping in a source file, add `@convert` to one of the first five non‑blank lines (it's typically the very first line):

```xell
@convert "dialect.xesy"

def greet(name):
    print("Hello, " + name)
;
```

- The path in `@convert` is resolved **relative to the source file's directory**.
- If no path is given, Xell looks for a `.xesy` file in the same directory:

```xell
@convert   # looks for *.xesy in the same directory
```

- The `@convert` line is **stripped** from the source before conversion – it's only a directive.

### How It Works

1. Xell reads the source file.
2. It checks the first five non‑blank lines for `@convert`.
3. If found, it resolves the `.xesy` mapping file.
4. It loads the mapping (canonical → custom) and **inverts** it (custom → canonical).
5. It strips the `@convert` line.
6. It replaces every custom keyword with its canonical equivalent (whole‑word matching).
7. It parses and executes the converted code.

---

## `.xesy` Mapping File Format

The `.xesy` file is a JSON object mapping **canonical Xell keywords** to **custom dialect words**.

### Template

```json
{
  "_meta": {
    "dialect_name": "My Dialect",
    "author": "Your Name",
    "xell_version": "0.1.0",
    "description": "Custom keyword mapping for Xell"
  },
  "fn": "",
  "give": "",
  "if": "",
  "elif": "",
  "else": "",
  "for": "",
  "while": "",
  "in": "",
  "break": "",
  "continue": "",
  "try": "",
  "catch": "",
  "finally": "",
  "incase": "",
  "let": "",
  "be": "",
  "loop": "",
  "bring": "",
  "from": "",
  "as": "",
  "module": "",
  "export": "",
  "requires": "",
  "enum": "",
  "struct": "",
  "class": "",
  "inherits": "",
  "immutable": "",
  "private": "",
  "protected": "",
  "public": "",
  "static": "",
  "interface": "",
  "implements": "",
  "abstract": "",
  "mixin": "",
  "with": "",
  "yield": "",
  "async": "",
  "await": "",
  "true": "",
  "false": "",
  "none": "",
  "and": "",
  "or": "",
  "not": "",
  "is": "",
  "eq": "",
  "ne": "",
  "gt": "",
  "lt": "",
  "ge": "",
  "le": "",
  "of": "",
  "print": "",
  "input": "",
  "exit": "",
  "len": "",
  "type": "",
  "str": "",
  "num": "",
  "typeof": "",
  "to_int": "",
  "to_float": "",
  "to_str": "",
  "push": "",
  "pop": "",
  "keys": "",
  "values": "",
  "range": "",
  "set": "",
  "has": "",
  "assert": "",
  "sort": "",
  "reverse": "",
  "slice": "",
  "join": "",
  "split": "",
  "map": "",
  "filter": "",
  "reduce": "",
  "contains": "",
  "index_of": "",
  "mkdir": "",
  "rm": "",
  "cp": "",
  "mv": "",
  "exists": "",
  "is_file": "",
  "is_dir": "",
  "ls": "",
  "read": "",
  "write": "",
  "append": "",
  "file_size": "",
  "cwd": "",
  "cd": "",
  "abspath": "",
  "basename": "",
  "dirname": "",
  "ext": "",
  "env_get": "",
  "env_set": "",
  "env_unset": "",
  "env_has": "",
  "run": "",
  "run_capture": "",
  "pid": "",
  "sleep": "",
  "floor": "",
  "ceil": "",
  "round": "",
  "abs": "",
  "mod": "",
  "sqrt": "",
  "pow": "",
  "sin": "",
  "cos": "",
  "tan": "",
  "log": "",
  "log10": "",
  "max": "",
  "min": "",
  "random": "",
  "random_int": "",
  "upper": "",
  "lower": "",
  "trim": "",
  "starts_with": "",
  "ends_with": "",
  "replace": "",
  "substr": "",
  "char_at": ""
}
```

**Fill in the values** with your custom words:

```json
{
  "fn": "def",
  "give": "return",
  "if": "if",
  "elif": "elseif",
  "else": "otherwise",
  "true": "yes",
  "false": "no",
  "none": "null",
  "print": "say",
  "len": "size"
}
```

### Rules

- Only **top‑level keys** are mapped – keywords inside compound expressions (like `fn foo(): ...`) are **not** recursively translated.
- Mapping is **whole‑word** – `fn` will be replaced by `def`, but `fnord` will remain unchanged.
- Strings and comments are **not** translated – only code keywords.
- The mapping file is **inverted** for conversion (canonical → custom is stored, but `--convert` uses custom → canonical, and `--revert` uses canonical → custom).

---

## CLI Commands

### `xell --convert <file> [map.xesy]`

Converts a dialect file to canonical Xell syntax **in‑place**.

```bash
$ xell --convert script.xel
✓ Converted 'script.xel' to canonical Xell
  Mapping: dialect.xesy
  Revert info saved to: script.xel.xesy.revert
```

**Process:**
1. Reads the source file.
2. Extracts the `@convert` directive and mapping path.
3. Loads the `.xesy` file (canonical → custom).
4. **Inverts** the mapping (custom → canonical).
5. Strips the `@convert` line.
6. Replaces custom words with canonical equivalents.
7. Writes the converted file back.
8. Creates a `.xesy.revert` sidecar file storing the mapping path for future `--revert`.

### `xell --revert <file> [map.xesy]`

Restores a canonical file back to dialect syntax **in‑place**, re‑adding the `@convert` directive.

```bash
$ xell --revert script.xel
✓ Reverted 'script.xel' to dialect syntax
  Mapping: dialect.xesy
  @convert directive re-added
```

**Process:**
1. Reads the source file (canonical Xell).
2. Determines the mapping path:
   - Explicit `map.xesy` argument (if provided).
   - `.xesy.revert` sidecar (if it exists).
   - Auto‑detects a `.xesy` file in the same directory.
3. Loads the `.xesy` file (canonical → custom).
4. Replaces canonical words with custom equivalents.
5. Pre‑pends the `@convert` directive.
6. Writes the reverted file back.
7. Removes the `.xesy.revert` sidecar.

### `xell --gen_xesy [output.xesy]`

Generates a template `.xesy` mapping file with all canonical Xell keywords and built‑ins.

```bash
$ xell --gen_xesy dialect.xesy
✓ Generated template mapping file: dialect.xesy
  Fill in the values with your dialect words, then use:
    @convert "dialect.xesy"
```

**Output file:** `dialect.xesy` (or the specified name). The file contains all canonical keywords and built‑ins with empty values to fill in.

---

## Complete Example: Creating and Using a Dialect

### Step 1: Generate a template

```bash
$ xell --gen_xesy my_dialect.xesy
✓ Generated template mapping file: my_dialect.xesy
```

### Step 2: Edit the mapping

```json
{
  "fn": "def",
  "give": "return",
  "if": "if",
  "elif": "elif",
  "else": "otherwise",
  "print": "say",
  "len": "size",
  "true": "yes",
  "false": "no"
}
```

### Step 3: Write code in your dialect

```xell
@convert "my_dialect.xesy"

def greet(name):
    say "Hello, " + name
;

def factorial(n):
    if n <= 1:
        return 1
    return n * factorial(n - 1)
;

say factorial(5)
```

### Step 4: Run it directly

```bash
$ xell script.xel
Hello, Alice
120
```

Xell automatically detects `@convert` and translates the code before execution.

### Step 5: Convert to canonical (optional)

```bash
$ xell --convert script.xel
✓ Converted 'script.xel' to canonical Xell
```

Now `script.xel` becomes:

```xell
fn greet(name):
    print("Hello, " + name)
;

fn factorial(n):
    if n <= 1:
        give 1
    give n * factorial(n - 1)
;

print(factorial(5))
```

### Step 6: Revert back to dialect (if needed)

```bash
$ xell --revert script.xel
✓ Reverted 'script.xel' to dialect syntax
```

---

## Edge Cases and Less Obvious Behavior

### 1. Multiple `.xesy` Files in the Same Directory

If you have multiple `.xesy` files and the `@convert` directive has no path, Xell will **auto‑detect** one arbitrarily (the first file found). Always specify a path to avoid ambiguity.

```xell
@convert "my_dialect.xesy"   # ✅ explicit
```

### 2. `@convert` Must Be in the First Five Non‑Blank Lines

```xell
# This is a comment
@convert "dialect.xesy"   # ✅ still works (within first 5 lines)
```

### 3. Mapping is Whole‑Word Only

```xell
# Mapping: "fn" → "def"
def compute():   # ✅ "def" becomes "fn"
    fnord = 10   # ❌ "fnord" stays "fnord" (not a keyword)
;
```

### 4. Strings and Comments Are Not Translated

```xell
print("def fn give")   # ❌ string content is NOT translated
# def fn give          # ❌ comment content is NOT translated
```

### 5. The `@convert` Line Is Stripped During Conversion

```bash
$ xell --convert script.xel
```

After conversion, the `@convert` line is removed. The `.xesy.revert` sidecar stores the mapping path for `--revert`.

### 6. `--revert` Without a `.xesy.revert` Sidecar

If the sidecar is missing, `--revert` will look for a `.xesy` file in the same directory. If none exists, it will fail.

```bash
$ xell --revert script.xel
Error: Cannot find mapping file for revert.
Specify explicitly: xell --revert script.xel <map.xesy>
```

### 7. Mapping Files Are Case‑Sensitive

```json
{
  "fn": "def"   # ✅
  "Fn": "def"   # ❌ ignored (not a canonical keyword)
}
```

---

## What NOT to Do

### 1. Using `@convert` Without a Mapping File

```xell
@convert   # ❌ requires a .xesy file in the same directory
```

**Why it fails:** Xell cannot find a mapping file. Either create one or specify a path.

### 2. Putting `@convert` After the First Five Lines

```xell
# line 1
# line 2
# line 3
# line 4
# line 5
@convert "dialect.xesy"   # ❌ ignored – too far down
```

### 3. Mapping Keywords to Reserved Characters

```json
{
  "fn": "def()"   # ❌ invalid – custom words must be valid identifiers
}
```

### 4. Forgetting to Invert the Mapping for `--convert`

`--convert` uses **custom → canonical** mapping. The `.xesy` file stores **canonical → custom**. The inversion happens automatically, so you don't need to worry about it.

### 5. Using `--revert` Without a Backup

`--revert` modifies the file **in‑place**. Always version‑control your files or create a backup.

### 6. Expecting `@convert` to Work in the REPL

The `@convert` directive only works in **files**. The REPL does not support dialect conversion.

### 7. Using Dialect Words in the Mapping File Itself

The `.xesy` file is in JSON format and uses **canonical keywords** as keys. Dialect words are **values** only.

---

## See Also

- [CLI Commands](cli.md) – `--convert`, `--revert`, `--gen_xesy`
- [Linting](linting.md) – static analysis of dialect files
- [Module Indexing](module-indexing.md) – indexing dialect files with `--make_module`

---

**Previous:** [Linting](linting.md)  
**Next:** [Module Indexing](module-indexing.md)