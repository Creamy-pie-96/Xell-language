# Xell

> **A cross-platform scripting language to replace Bash, PowerShell, and Zsh.**

Xell (`.xel`) is a simple, readable scripting language designed for automation, file operations, and process management — the things you'd normally reach for a shell script to do, but without the platform-dependent quirks.

Write once, run on **Linux**, **macOS**, and **Windows**.

---

## Quick Start

```xell
# hello.xel
name = "world"
print "Hello, {name}!"
```

Build and run:

```bash
mkdir build && cd build
cmake .. && cmake --build .
./xell hello.xel
```

---

## Language Overview

### Variables

No `let`, `var`, or `const`. Just assign:

```xell
name    = "xell"
count   = 42
price   = 9.99
ready   = true
nothing = none
```

Reassignment uses the same syntax:

```xell
count = count + 1
```

### Types

| Type     | Example                 |
| -------- | ----------------------- |
| `string` | `"hello"`               |
| `number` | `42`, `3.14`            |
| `bool`   | `true`, `false`         |
| `list`   | `[1, 2, 3]`             |
| `map`    | `{ host: "localhost" }` |
| `none`   | `none`                  |

All numbers are 64-bit floating point internally. No int/float distinction.

---

## Strings

Double quotes only. Supports interpolation with `{expression}` and escape sequences:

```xell
name = "Alice"
greeting = "Hello, {name}!"
path = "{project}/src/{name}.xel"

escaped = "line one\nline two\ttabbed"
```

| Escape | Meaning      |
| ------ | ------------ |
| `\n`   | Newline      |
| `\t`   | Tab          |
| `\\`   | Backslash    |
| `\"`   | Double quote |

---

## Lists

Ordered, zero-indexed, mixed types allowed:

```xell
ports = [3000, 8080, 9000]
mixed = [1, "hello", true, none]
empty = []

first = ports[0]       # 3000
nested = [[1, 2], [3, 4]]
inner = nested[0][1]   # 2
```

---

## Maps

Key-value pairs. Keys are identifiers (no quotes needed):

```xell
config = {
    host:  "localhost",
    port:  3000,
    debug: true,
    tags:  ["web", "api"]
}

empty_map = {}
```

Access with `->`:

```xell
h = config->host      # "localhost"
p = config->port      # 3000
t = config->tags[0]   # "web"
```

---

## Operators

### Arithmetic

| Symbol | Meaning           | Example                         |
| ------ | ----------------- | ------------------------------- |
| `+`    | Add / concatenate | `x + 1`, `"hi" + " there"`      |
| `-`    | Subtract          | `x - 1`                         |
| `*`    | Multiply          | `x * 2`                         |
| `/`    | Divide            | `x / 2`                         |
| `++`   | Increment         | `++x` (prefix), `x++` (postfix) |
| `--`   | Decrement         | `--x` (prefix), `x--` (postfix) |

### Comparison

Both symbolic and keyword forms are supported — they are identical:

| Symbol | Keyword      | Meaning          |
| ------ | ------------ | ---------------- |
| `==`   | `is` or `eq` | Equal            |
| `!=`   | `ne`         | Not equal        |
| `>`    | `gt`         | Greater than     |
| `<`    | `lt`         | Less than        |
| `>=`   | `ge`         | Greater or equal |
| `<=`   | `le`         | Less or equal    |

```xell
if x == 10 :  ;
if x is 10 :  ;      # same thing
if count gt 5 :  ;
if score ge 90 :  ;
```

### Logical

| Keyword | Symbol | Meaning     |
| ------- | ------ | ----------- |
| `not`   | `!`    | Negate      |
| `and`   |        | Both true   |
| `or`    |        | Either true |

```xell
if not ready :
    print "waiting..."
;

if !ready :           # same as above
    print "waiting..."
;

if debug and host is "localhost" :
    print "local debug mode"
;
```

### Precedence (highest to lowest)

| Level | Operators                                    |
| ----- | -------------------------------------------- |
| 1     | `not`, `!`, unary `-`, prefix `++`/`--`      |
| 2     | `*`, `/`                                     |
| 3     | `+`, `-`                                     |
| 4     | `>`, `<`, `>=`, `<=`, `gt`, `lt`, `ge`, `le` |
| 5     | `==`, `!=`, `is`, `eq`, `ne`                 |
| 6     | `and`                                        |
| 7     | `or`                                         |

Override with parentheses: `result = (x + y) * 2`

---

## Statement Termination

Statements end with a **newline** (implicit) or a **dot** `.` (explicit). The dot is always optional:

```xell
mkdir "src"            # newline ends it
mkdir "tests" .        # dot ends it — same thing
```

---

## Comments

```xell
# This is a single-line comment
x = 10   # inline comment

-->
    This is a
    multi-line comment
<--
```

---

## Scope Delimiters

`:` opens a scope. `;` closes it. Used with `if`, `elif`, `else`, `for`, `while`, and `fn`:

```xell
if ready :
    print "go!"
;
```

---

## Control Flow

### If / Elif / Else

```xell
if score ge 90 :
    print "A grade"
;
elif score ge 75 :
    print "B grade"
;
elif score ge 60 :
    print "C grade"
;
else :
    print "Failing"
;
```

### For Loop

Iterates over a list:

```xell
ports = [3000, 8080, 9000]

for port in ports :
    print "Starting on {port}"
;
```

### While Loop

```xell
count = 0

while count lt 5 :
    print "count is {count}"
    count = count + 1
;
```

Loops can be nested:

```xell
if ready :
    for file in files :
        print "processing {file}"
    ;
;
```

---

## Functions

### Define with `fn`

```xell
fn greet(name, greeting) :
    print "{greeting}, {name}!"
;

fn add(a, b) :
    give a + b
;
```

### Return with `give`

`give` is Xell's return keyword:

```xell
fn square(x) :
    give x * x
;

result = square(5)    # 25
```

`give` without a value returns `none`:

```xell
fn do_work() :
    mkdir "output"
    give
;
```

### Call with or without parentheses

```xell
# Both are equivalent:
greet("Alice", "Hello")
print "done"

# Paren-less style (shell-like):
greet "Alice" "Hello"
print "done"
```

Paren-less calls work when the function name is a bare identifier followed by arguments on the same line.

---

## Built-in Commands

### Output

| Command            | Description     |
| ------------------ | --------------- |
| `print(value)`     | Print to stdout |
| `print_err(value)` | Print to stderr |

### Filesystem

| Command                     | Description                    |
| --------------------------- | ------------------------------ |
| `mkdir(path)`               | Create directory (recursive)   |
| `copy(src, dest)`           | Copy file or directory         |
| `move(src, dest)`           | Move file or directory         |
| `delete(path)`              | Delete file or directory       |
| `exists(path)`              | Returns `true` / `false`       |
| `list_files(path)`          | Returns list of file paths     |
| `read_file(path)`           | Returns file content as string |
| `write_file(path, content)` | Write string to file           |

### Environment Variables

| Command                | Description      |
| ---------------------- | ---------------- |
| `env_get(name)`        | Get env variable |
| `env_set(name, value)` | Set env variable |

### Process Execution

| Command                | Description                           |
| ---------------------- | ------------------------------------- |
| `run(command)`         | Run external command (inherit output) |
| `run_capture(command)` | Run and return stdout as string       |

### Type Utilities

| Command            | Description              |
| ------------------ | ------------------------ |
| `type_of(value)`   | Returns type as string   |
| `to_string(value)` | Convert to string        |
| `to_number(value)` | Convert to number        |
| `length(value)`    | Length of string or list |

### Examples

```xell
mkdir "output/logs"
copy "template.xel" "new_script.xel"

run "git add ."
run "git commit -m 'initial'"
run "git push"

log = run_capture("git log --oneline")
print log

env_set "NODE_ENV" "production"
home = env_get("HOME")
```

---

## Bring (Imports)

`bring` imports names from another `.xel` file.

### Bring everything

```xell
bring * from "./helpers.xel"
```

### Bring specific names

```xell
bring setup from "./helpers.xel"
bring setup, deploy from "./helpers.xel"
```

### Bring with aliases

Aliases map positionally:

```xell
bring setup, deploy from "./helpers.xel" as s, d
# Now use s() instead of setup(), d() instead of deploy()
```

### Example

```xell
# helpers.xel
fn setup_dirs(project) :
    mkdir "{project}/src"
    mkdir "{project}/tests"
;

fn deploy(project, env) :
    run "npm run build"
    give "{project} deployed to {env}"
;
```

```xell
# main.xel
bring setup_dirs, deploy from "./helpers.xel" as setup, push

setup("my-app")
result = push("my-app", "production")
print result
```

---

## Scope Rules

Xell uses **dynamic scoping**. Functions can see variables from their caller's scope:

```xell
app_name = "xell"

fn print_name() :
    print app_name       # visible from caller scope
;

print_name()             # prints "xell"
```

Lookup order:

1. Current block scope
2. Enclosing block scopes
3. Caller's scope (for functions)
4. Global scope
5. Error: `UndefinedVariable`

---

## Error Messages

Runtime errors include the line number and error type:

```
[XELL ERROR] Line 14 — TypeError: cannot add number and list
[XELL ERROR] Line 7  — UndefinedVariable: 'projct' is not defined
[XELL ERROR] Line 22 — ArityError: 'setup' expects 2 args, got 1
```

| Error               | Trigger                                   |
| ------------------- | ----------------------------------------- |
| `UndefinedVariable` | Variable used before assigned             |
| `UndefinedFunction` | Function called that doesn't exist        |
| `TypeError`         | Wrong type for an operation               |
| `IndexError`        | List index out of range                   |
| `KeyError`          | Map key doesn't exist                     |
| `DivisionByZero`    | Division by zero                          |
| `FileNotFound`      | File operation on missing path            |
| `BringError`        | File not found, name missing, or circular |
| `ArityError`        | Wrong number of arguments                 |

---

## Full Example

```xell
# deploy.xel — A deployment automation script

bring setup_dirs from "./helpers.xel"

project = "my-app"
env = env_get("DEPLOY_ENV")

if env is none :
    env = "staging"
    print "No DEPLOY_ENV set, defaulting to {env}"
;

# Set up directory structure
setup_dirs(project)

# Build
print "Building {project} for {env}..."
run "npm run build"

# Deploy
config = {
    host:    "deploy.example.com",
    port:    22,
    project: project,
    env:     env
}

if env is "production" :
    print "Deploying to PRODUCTION on {config->host}:{config->port}"
;
else :
    print "Deploying to {env} on {config->host}"
;

# Process files
files = list_files("./dist")
count = 0

for file in files :
    print "  uploading {file}"
    count = count + 1
;

print "Done! Uploaded {count} files to {env}."
```

---

## Building from Source

```bash
git clone <repo-url> && cd Xell
mkdir build && cd build
cmake ..
cmake --build .
```

This produces the `xell` executable. Run a script:

```bash
./xell path/to/script.xel
```

Run tests:

```bash
./parser_test
```

---

## Project Structure

```
Xell/
├── CMakeLists.txt          # Build configuration
├── README.md               # This file
├── src/
│   ├── main.cpp            # CLI entry point
│   ├── lexer/
│   │   ├── token.hpp       # Token types & helpers
│   │   ├── lexer.hpp       # Lexer class declaration
│   │   └── lexer.cpp       # Tokenizer implementation
│   ├── parser/
│   │   ├── ast.hpp         # AST node definitions
│   │   ├── parser.hpp      # Parser class declaration
│   │   └── parser.cpp      # Recursive descent parser
│   ├── interpreter/        # (future) Tree-walking interpreter
│   ├── builtins/           # (future) Built-in command implementations
│   └── os/                 # (future) Platform-specific utilities
├── test/
│   └── parser_test.cpp     # Lexer & parser tests
├── stdlib/
│   └── std.xel/            # (future) Standard library
└── readme/
    ├── lang_plan.md         # Architecture & design decisions
    ├── xell_grammar.md      # Complete grammar specification
    ├── lexer_guide.md       # Beginner's guide to the lexer
    ├── ast_guide.md         # Beginner's guide to the AST
    └── parser_guide.md      # Beginner's guide to the parser
```

---

## License

See [LICENSE](LICENSE) for details.
