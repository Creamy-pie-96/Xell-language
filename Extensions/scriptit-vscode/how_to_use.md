# ScriptIt VS Code Extension — How to Use

## Installation

Run the all-in-one installer:

```bash
cd scriptit-vscode
python3 install.py
```

This automatically:

1. Installs missing dependencies (cmake, g++, Node.js 22, npm)
2. Builds the ScriptIt binary and installs it system-wide
3. Compiles the VS Code extension
4. Packages it into a `.vsix`
5. Installs the `.vsix` into VS Code

For partial installs, use:

```bash
python3 install.py --ext-only    # Only the VS Code extension
python3 install.py --build-only  # Only the ScriptIt binary
python3 install.py --check       # Just see what's installed
```

After install, **restart VS Code** to activate.

---

## File Types

| Extension | Type          | What it does                                                   |
| --------- | ------------- | -------------------------------------------------------------- |
| `.sit`    | Script file   | Full syntax highlighting, IntelliSense, diagnostics            |
| `.si`     | Script file   | Same as `.sit` (shorter alias)                                 |
| `.nsit`   | Notebook file | Opens in VS Code's native notebook editor with ScriptIt kernel |

Each file type has its own icon in the file explorer:

- **`.sit`** / **`.si`** — Blue "Si" code icon
- **`.nsit`** — Purple "NSi" notebook icon

---

## Syntax Highlighting

Every part of your code gets colored:

```text
fn add(a, b):           # fn = keyword (blue), add = function name (yellow)
    give a + b.         #   parameters (orange), give = keyword, . = terminator
;

var x = 10.             # var = keyword, x = variable (light blue)
var name, age = ...     # Both name and age highlighted as variables

let y be 42.            # let/be = keywords, y = variable
new items are [1, 2].   # new/are = keywords, items = variable

for i in range(10):     # for/in/range = keywords, i = loop variable
    print(i).           # print = builtin (green-ish)
;

sin(3.14).              # sin = math function (different color)
double("42").           # double = type conversion function
True, False, None       # Constants (specific color)
```

### What gets colored (scope → theme color):

| Element          | Example                   | Scope                              | Typical Color |
| ---------------- | ------------------------- | ---------------------------------- | ------------- |
| Keywords         | `fn`, `if`, `for`, `give` | `keyword.*`                        | Blue/Purple   |
| Function names   | `fn **add**(x):`          | `entity.name.function`             | Yellow        |
| Parameters       | `fn add(**x, y**):`       | `variable.parameter`               | Orange        |
| Variables        | `var **x** = ...`         | `variable.other.declaration`       | Light blue    |
| Loop variables   | `for **i** in ...`        | `variable.other.loop`              | Light blue    |
| Builtins         | `print()`, `len()`        | `support.function.builtin`         | Green         |
| Math functions   | `sin()`, `sqrt()`         | `support.function.math`            | Green         |
| Type conversions | `str()`, `double()`       | `support.function.type`            | Teal          |
| Strings          | `"hello"`                 | `string.quoted`                    | Orange/Brown  |
| Numbers          | `42`, `3.14`              | `constant.numeric`                 | Light green   |
| Comments         | `# note`                  | `comment.line`                     | Gray/Green    |
| Constants        | `True`, `False`, `None`   | `constant.language`                | Blue          |
| Operators        | `==`, `+=`, `&&`          | `keyword.operator`                 | Light gray    |
| @ references     | `@variable`               | `storage.modifier.reference`       | Red           |
| Method calls     | `.upper()`                | `entity.name.function.method`      | Yellow        |
| Block end        | `;` on its own line       | `punctuation.terminator.block`     | Gray          |
| Statement end    | `.` at end of line        | `punctuation.terminator.statement` | Gray          |

---

## IntelliSense (Auto-Completion)

Start typing and completions appear automatically:

- **Keywords**: `fn`, `var`, `if`, `for`, `while`, `give`, `pass`, `with`, `as`, ...
- **Builtins**: `print`, `len`, `type`, `isinstance`, `sorted`, `map`, ...
- **Math**: `sin`, `cos`, `sqrt`, `log`, `abs`, `ceil`, `floor`, ...
- **Types**: `str`, `int`, `double`, `bool`, `list`, `set`, `dict`, ...
- **Your functions**: Any `fn name(...)` you define
- **Your variables**: Any `var name = ...` you declare

### Method Completions

Type `.` after any variable to get method suggestions:

- String methods: `.upper()`, `.lower()`, `.split()`, `.replace()`, ...
- List methods: `.append()`, `.pop()`, `.sort()`, `.index()`, ...
- Set methods: `.add()`, `.union()`, `.intersection()`, ...
- File methods: `.read()`, `.write()`, `.close()`, ...

---

## Hover Information

Hover over any keyword or function name to see:

- **Signature**: `print(value1, value2, ...)`
- **Description**: "Print one or more values to stdout, separated by spaces."

Works for all builtins, math functions, keywords, and type conversions.

---

## Signature Help

Type a function name followed by `(` to see parameter info:

```text
print(  ←── popup shows: print(value1, value2, ...)
                         first value to print
```

As you type commas, the active parameter highlights change.

---

## Diagnostics (Error Detection)

### Static Analysis (instant, no binary needed):

- **Bracket matching**: Detects unclosed `(`, `[`, `{` or mismatched pairs
- **Unterminated strings**: Missing closing `"` or `'`
- **= vs == warning**: Warns if you use `=` inside `if`/`while` conditions
- **fn missing colon/dot**: Warns if `fn name()` has no `:` or `.`

### Subprocess Validation (requires `scriptit` binary):

- Runs your code through `scriptit --script`
- Parses error messages with line numbers
- Shows actual runtime errors (undefined variables, type errors, etc.)

Toggle diagnostics: Settings → `scriptit.enableLinting`

---

## Snippets

Type a prefix and press Tab:

| Prefix   | Expands to                    |
| -------- | ----------------------------- |
| `fn`     | Function definition with body |
| `fnr`    | Function with give (return)   |
| `fwd`    | Forward declaration           |
| `if`     | If block                      |
| `ife`    | If-else block                 |
| `ifee`   | If-elif-else block            |
| `forr`   | For range(N) loop             |
| `forft`  | For range(from A to B)        |
| `forfts` | For range(from A to B step S) |
| `forin`  | For item in collection        |
| `while`  | While loop                    |
| `var`    | Variable declaration          |
| `pr`     | print() call                  |
| `give`   | Return statement              |
| `with`   | Context manager               |
| `let`    | let X be Y.                   |
| `new`    | new X are [...].              |
| `of`     | method of object.             |

---

## Running Scripts

### Run Current File

- **Keyboard**: `Ctrl+Shift+R` (Mac: `Cmd+Shift+R`)
- **Command Palette**: "ScriptIt: Run Current File"
- **Editor title bar**: Click the ▶ Play button (on `.sit` files)

### Run Selection

- Select code → Command Palette → "ScriptIt: Run Selection"

---

## Notebook Support (.nsit files)

Opening any `.nsit` file in VS Code gives you a native notebook experience:

### Features:

- **Code cells**: Write ScriptIt code, execute with Shift+Enter
- **Markdown cells**: Rich text between code cells
- **Kernel**: Runs a real ScriptIt kernel subprocess
- **Persistent state**: Variables carry across cells
- **Output display**: stdout, stderr, and results shown below cells

### Keyboard Shortcuts (VS Code notebook defaults):

| Key           | Action                           |
| ------------- | -------------------------------- |
| `Shift+Enter` | Run cell and move to next        |
| `Ctrl+Enter`  | Run cell, stay in place          |
| `Escape`      | Enter command mode               |
| `Enter`       | Enter edit mode                  |
| `A`           | Insert cell above (command mode) |
| `B`           | Insert cell below (command mode) |
| `DD`          | Delete cell (command mode)       |
| `M`           | Change to markdown cell          |
| `Y`           | Change to code cell              |

### Opening the Web Notebook:

If you prefer the browser-based notebook:

- Command Palette → "ScriptIt: Open Notebook Server"
- This starts the Python notebook server and opens it

---

## Configuration

Open Settings (Ctrl+,) and search "scriptit":

| Setting                        | Default    | Description                                   |
| ------------------------------ | ---------- | --------------------------------------------- |
| `scriptit.scriptitPath`        | `scriptit` | Path to binary. Use full path if not in PATH. |
| `scriptit.maxNumberOfProblems` | `100`      | Max diagnostics shown                         |
| `scriptit.enableLinting`       | `true`     | Toggle error detection on/off                 |
| `scriptit.trace.server`        | `off`      | Debug LSP communication                       |

---

## File Icons

The extension provides custom file icons:

- **Enable**: Settings → "File Icon Theme" → Select "ScriptIt File Icons"
- Note: This only adds icons for `.sit`, `.si`, and `.nsit` files. Your other files will use your existing icon theme.

For best results, keep your current icon theme and the ScriptIt icons will override just the ScriptIt file types.

---

## Tips & Tricks

1. **Quick function**: Type `fn` + Tab → fills in a function template
2. **Check type**: Hover over `type` to see its signature
3. **Find errors fast**: Red squiggles appear as you type
4. **Notebook workflow**: Use `.nsit` for exploration, `.sit` for final scripts
5. **@ parameters**: The `@` symbol in function calls gets special red highlighting
6. **Block terminators**: `;` on its own line gets dimmed (it's a structural element, not code)
