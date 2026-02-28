# Xell Language — Complete Grammar Specification
> Version: 0.1 (Draft)  
> File extension: `.xel`  
> Last updated: 2026-02-28

---

## How to Read This Document

```
RULE_NAME   := definition
[x]         = x is optional
{x}         = x repeats zero or more times
(x | y)     = x or y
"word"      = literal keyword or symbol
```

---

## Table of Contents

1. [Source & File Structure](#1-source--file-structure)
2. [Comments](#2-comments)
3. [Tokens & Literals](#3-tokens--literals)
4. [Identifiers & Keywords](#4-identifiers--keywords)
5. [Operators](#5-operators)
6. [Types & Values](#6-types--values)
7. [Expressions](#7-expressions)
8. [Statements](#8-statements)
9. [Control Flow](#9-control-flow)
10. [Functions](#10-functions)
11. [Built-in Commands](#11-built-in-commands)
12. [Bring System](#12-bring-system)
13. [Scope Rules](#13-scope-rules)
14. [Full Grammar Summary](#14-full-grammar-summary)
15. [Error Handling Tokens](#15-error-handling-tokens)

---

## 1. Source & File Structure

A Xell program is a plain text `.xel` file encoded in UTF-8.

A program is a sequence of **statements** executed top to bottom.

```
PROGRAM := { STATEMENT }
```

**Statement Termination:**  
A statement ends with either:
- A `.` (dot) explicitly placed at the end
- Or a **newline** (implicit termination — dot is optional)

```xell
mkdir "src"          # implicit termination — newline ends it
mkdir "tests" .      # explicit termination — dot ends it
```

Both are identical. The dot is optional but allowed anywhere for clarity.

**Scope Delimiters:**
- `:` opens a new scope (after `if`, `for`, `while`, `fn`)
- `;` closes that scope

```xell
if x == 1 :
    print "yes"
;
```

---

## 2. Comments

### Single-line Comment
Begins with `#`, ends at the newline. Can appear on its own line or after a statement.

```xell
# This is a comment
x = 10   # inline comment
```

### Multi-line Comment
Begins with `-->` and ends with `<--`. Can span multiple lines. Can appear anywhere whitespace is allowed.

```xell
--> 
    This is a 
    multi-line comment
<--

x = --> inline block comment <-- 10
```

---

## 3. Tokens & Literals

### String Literal
Enclosed in double quotes. Supports `{expression}` interpolation inside.

```
STRING := '"' { CHAR | INTERPOLATION } '"'
INTERPOLATION := '{' EXPRESSION '}'
```

```xell
name = "world"
greeting = "Hello, {name}!"
path = "{project}/src/{name}.xel"
```

> **Note:** Single quotes are NOT supported. Only double quotes.

---

### Number Literal
All numbers are stored as 64-bit floating point (`double`) internally.  
No distinction between int and float at the language level.

```
NUMBER := DIGITS [ '.' DIGITS ]
DIGITS := [0-9]+
```

```xell
age    = 25
price  = 9.99
big    = 1000000
```

---

### Boolean Literal

```
BOOL := "true" | "false"
```

```xell
debug = true
ready = false
```

---

### none Literal

```
none := "none"
```

```xell
result = none
```

---

### List Literal
Ordered, zero-indexed, can hold mixed types.

```
LIST := '[' [ EXPRESSION { ',' EXPRESSION } ] ']'
```

```xell
ports   = [3000, 8080, 9000]
mixed   = [1, "hello", true, none]
empty   = []
nested  = [[1, 2], [3, 4]]
```

Access by index using `[n]`:
```xell
first = ports[0]       # 3000
last  = ports[2]       # 9000
```

---

### Map Literal
Key-value pairs. Keys are always identifiers (no quotes needed on keys).  
Values can be any expression.

```
MAP := '{' [ MAP_ENTRY { ',' MAP_ENTRY } ] '}'
MAP_ENTRY := IDENTIFIER ':' EXPRESSION
```

```xell
config = {
    host:    "localhost",
    port:    3000,
    debug:   true,
    tags:    ["web", "api"]
}

empty_map = {}
```

Access by key using `->`:
```xell
h = config->host     # "localhost"
p = config->port     # 3000
```

---

## 4. Identifiers & Keywords

### Identifiers
Variable names, function names, map keys.

```
IDENTIFIER := LETTER { LETTER | DIGIT | '_' }
LETTER     := [a-zA-Z]
DIGIT      := [0-9]
```

```xell
myVar
project_name
x
file2
```

> Identifiers are **case-sensitive**. `MyVar` and `myvar` are different.

---

### Reserved Keywords
These cannot be used as variable or function names:

| Keyword | Purpose |
|---------|---------|
| `fn` | Function definition |
| `give` | Give back a value from a function |
| `if` | Conditional |
| `else` | Else branch |
| `for` | For loop |
| `while` | While loop |
| `in` | Used in for loops |
| `bring` | Bring names from another file |
| `from` | Used with bring |
| `as` | Alias in bring |
| `true` | Boolean true |
| `false` | Boolean false |
| `none` | none value |
| `and` | Logical AND |
| `or` | Logical OR |
| `not` | Logical NOT |
| `is` | Equality (alias for `==`) |
| `eq` | Equal (alias for `==`) |
| `ne` | Not equal (alias for `!=`) |
| `gt` | Greater than (alias for `>`) |
| `lt` | Less than (alias for `<`) |
| `ge` | Greater or equal (alias for `>=`) |
| `le` | Less or equal (alias for `<=`) |
| `of`| To access utility methode of anything. like type() of name is same as name->type()|

---

## 5. Operators

### Arithmetic Operators

| Symbol | Meaning | Example |
|--------|---------|---------|
| `+` | Addition / String concat | `x + 1`, `"hello" + " world"` |
| `-` | Subtraction | `x - 1` |
| `*` | Multiplication | `x * 2` |
| `/` | Division | `x / 2` |

---

### Comparison Operators
Both symbolic and keyword forms are valid and identical:

| Symbol | Keyword | Meaning |
|--------|---------|---------|
| `==` | `is` or `eq` | Equal |
| `!=` | `ne` | Not equal |
| `>` | `gt` | Greater than |
| `<` | `lt` | Less than |
| `>=` | `ge` | Greater or equal |
| `<=` | `le` | Less or equal |

```xell
if x == 10 :   ;
if x is 10 :   ;    # same thing
if x eq 10 :   ;    # same thing

if name ne "admin" :   ;
if count gt 5 :        ;
if score ge 90 :       ;
```

---

### Logical Operators

| Keyword | Meaning | Example |
|---------|---------|---------|
| `and` | Both must be true | `x > 0 and x < 10` |
| `or` | Either must be true | `x == 0 or x == none` |
| `not` | Negate | `not ready` |

```xell
if debug is true and host is "localhost" :
    print "local debug mode"
;

if not ready :
    print "not ready yet"
;
```

---

### Assignment Operator

```
ASSIGN := IDENTIFIER '=' EXPRESSION
```

```xell
x = 10
name = "xell"
config = { host: "localhost" }
```

> There is no `let` keyword. Just write `name = value`.

---

### Member Access

| Syntax | Use case |
|--------|----------|
| `map->key` | Access map value by key |
| `list[n]` | Access list item by index |
| `map["key"]` | Access map with string key |

```xell
config->host        # map key access
ports[0]            # list index access
config["host"]      # alternative map access (same as ->)
```

> `->` is the preferred style for map access. Both forms work.

---

### Operator Precedence (High to Low)

| Level | Operators | Associativity |
|-------|-----------|---------------|
| 1 (highest) | `not` | Right |
| 2 | `*` `/` | Left |
| 3 | `+` `-` | Left |
| 4 | `>` `<` `>=` `<=` `gt` `lt` `ge` `le` | Left |
| 5 | `==` `!=` `is` `eq` `ne` | Left |
| 6 | `and` | Left |
| 7 (lowest) | `or` | Left |

Use parentheses `( )` to override precedence:
```xell
result = (x + y) * 2
check  = (a or b) and c
```

---

## 6. Types & Values

| Type | Literal | Internal C++ type |
|------|---------|-------------------|
| `string` | `"hello"` | `std::string` |
| `number` | `42`, `3.14` | `double` |
| `bool` | `true`, `false` | `bool` |
| `list` | `[1, 2, 3]` | `std::vector<Value>` |
| `map` | `{ key: val }` | `std::unordered_map<std::string, Value>` |
| `none` | `none` | `std::monostate` |

**Type coercion rules (keep it simple):**
- `number + string` → concatenate (number becomes string first)
- `bool` in arithmetic → `true = 1`, `false = 0`
- Everything else is a type error at runtime

---

## 7. Expressions

```
EXPRESSION    := LOGICAL_OR

LOGICAL_OR    := LOGICAL_AND { "or" LOGICAL_AND }
LOGICAL_AND   := EQUALITY    { "and" EQUALITY }
EQUALITY      := COMPARISON  { ("==" | "!=" | "is" | "eq" | "ne") COMPARISON }
COMPARISON    := ADDITION    { (">" | "<" | ">=" | "<=" | "gt" | "lt" | "ge" | "le") ADDITION }
ADDITION      := MULTIPLY    { ("+" | "-") MULTIPLY }
MULTIPLY      := UNARY       { ("*" | "/") UNARY }
UNARY         := "not" UNARY | PRIMARY

PRIMARY       := LITERAL
              | IDENTIFIER
              | CALL
              | INDEX_ACCESS
              | MEMBER_ACCESS
              | '(' EXPRESSION ')'

LITERAL       := STRING | NUMBER | BOOL | none | LIST | MAP

CALL          := IDENTIFIER '(' [ EXPRESSION { ',' EXPRESSION } ] ')'
INDEX_ACCESS  := PRIMARY '[' EXPRESSION ']'
MEMBER_ACCESS := PRIMARY "->" IDENTIFIER
```

---

## 8. Statements

```
STATEMENT := ASSIGNMENT
           | CALL_STMT
           | IF_STMT
           | FOR_STMT
           | WHILE_STMT
           | FN_DEF
           | RETURN_STMT
           | IMPORT_STMT
           | BUILTIN_CALL
           [ '.' ]   # optional dot terminator
```

### Assignment

```
ASSIGNMENT := IDENTIFIER '=' EXPRESSION
```

```xell
x = 10
name = "xell"
files = ["a.xel", "b.xel"]
count = count + 1
```

> Reassignment uses the same syntax as first assignment. No `let` or `var`.

---

## 9. Control Flow

### If / elif / Else

```
IF_STMT := "if" EXPRESSION ':' BLOCK ';'
           { "elif" EXPRESSION ':' BLOCK ';' }
           [ "else" ':' BLOCK ';' ]
```

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

---

### For Loop
Iterates over a list. `item` is a new variable scoped to the loop body.

```
FOR_STMT := "for" IDENTIFIER "in" EXPRESSION ':' BLOCK ';'
```

```xell
ports = [3000, 8080, 9000]

for port in ports :
    print "Starting server on {port}"
;

# Works on any list expression
for file in list_files("./src") :
    print "Found: {file}"
;
```

---

### While Loop
Repeats as long as condition is true.

```
WHILE_STMT := "while" EXPRESSION ':' BLOCK ';'
```

```xell
count = 0

while count lt 5 :
    print "count is {count}"
    count = count + 1
;
```

---

### Block
A block is a sequence of statements between `:` and `;`.

```
BLOCK := { STATEMENT }
```

Blocks can be nested:
```xell
if ready :
    for file in files :
        print "processing {file}"
    ;
;
```

---

## 10. Functions

### Function Definition

```
FN_DEF := "fn" IDENTIFIER '(' [ PARAM_LIST ] ')' ':' BLOCK ';'
PARAM_LIST := IDENTIFIER { ',' IDENTIFIER }
```

```xell
fn greet(name, greeting) :
    print "{greeting}, {name}!"
;

fn get_path(project, subdir) :
    return "{project}/{subdir}"
;
```

---

### Give Statement
`give` is Xell's return keyword. Exits the function and optionally hands back a value.

```
GIVE_STMT := "give" [ EXPRESSION ]
```

```xell
fn add(a, b) :
    give a + b
;

fn do_something() :
    mkdir "output"
    give          # give with no value → gives back none
;
```

---

### Function Call

```
CALL := IDENTIFIER '(' [ EXPRESSION { ',' EXPRESSION } ] ')'
```

```xell
greet("Alice", "Hello")

result = add(10, 20)

path = get_path("my-project", "src")
print path
```

---

### Scope of Functions
Xell uses **dynamic scoping** — a function can see variables from its caller's scope.  
This is simpler to implement and natural for scripting use cases.

```xell
project = "my-app"

fn setup() :
    mkdir "{project}/src"    # 'project' is visible from caller scope
;

setup()
```

---

## 11. Built-in Commands

Built-ins look like function calls but are handled by the interpreter's built-in registry before looking up user-defined functions.

### Output

| Command | Description | Example |
|---------|-------------|---------|
| `print(value)` | Print to stdout | `print "hello"` |
| `print_err(value)` | Print to stderr | `print_err "error!"` |

> **Note:** `print` can be called without parentheses as a statement: `print "hello"` and `print("hello")` are both valid.

---

### Filesystem

| Command | Description | Example |
|---------|-------------|---------|
| `mkdir(path)` | Create directory (recursive) | `mkdir "src/utils"` |
| `copy(src, dest)` | Copy file or directory | `copy "a.txt" "b.txt"` |
| `move(src, dest)` | Move file or directory | `move "old.txt" "new.txt"` |
| `delete(path)` | Delete file or directory | `delete "temp/"` |
| `exists(path)` | Returns bool | `if exists("config.xel") :` |
| `list_files(path)` | Returns list of file paths | `files = list_files("./src")` |
| `read_file(path)` | Returns file content as string | `content = read_file("data.txt")` |
| `write_file(path, content)` | Write string to file | `write_file "out.txt" data` |

---

### Environment Variables

| Command | Description | Example |
|---------|-------------|---------|
| `env_get(name)` | Get env variable value | `home = env_get("HOME")` |
| `env_set(name, value)` | Set env variable | `env_set "DEBUG" "true"` |

---

### Process Execution

| Command | Description | Example |
|---------|-------------|---------|
| `run(command)` | Run an external command, inherit stdout/stderr | `run "npm install"` |
| `run_capture(command)` | Run and return stdout as string | `out = run_capture("git log")` |

```xell
run "git add ."
run "git commit -m 'initial'"
run "git push"

log = run_capture("git log --oneline")
print log
```

---

### Type Utilities

| Command | Description | Example |
|---------|-------------|---------|
| `type_of(value)` | Returns type as string | `type_of(42)` → `"number"` |
| `to_string(value)` | Convert to string | `to_string(3.14)` → `"3.14"` |
| `to_number(value)` | Convert to number | `to_number("42")` → `42` |
| `length(value)` | Length of string or list | `length([1,2,3])` → `3` |

---

## 12. Bring System

`bring` is Xell's import keyword. More powerful than a plain import — you can selectively bring in specific names from a file, and alias them.

```
BRING_STMT  := "bring" BRING_LIST "from" STRING [ BRING_ALIASES ]
             | "bring" '*' "from" STRING

BRING_LIST   := IDENTIFIER { ',' IDENTIFIER }
BRING_ALIASES := "as" IDENTIFIER { ',' IDENTIFIER }
```

---

### Forms

**Bring everything from a file:**
```xell
bring * from "./helpers.xel"
```

**Bring one specific name:**
```xell
bring setup from "./helpers.xel"
```

**Bring one name with an alias:**
```xell
bring setup from "./helpers.xel" as s
```

**Bring multiple names:**
```xell
bring setup, deploy from "./helpers.xel"
```

**Bring multiple names with aliases:**  
Aliases map positionally — first alias goes to first name, second to second, etc.
```xell
bring setup, deploy from "./helpers.xel" as s, d
```

---

### Example

```xell
# helpers.xel
fn setup_dirs(project) :
    mkdir "{project}/src"
    mkdir "{project}/tests"
    mkdir "{project}/logs"
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

### Rules

- `bring *` brings all functions and variables from the file into the current scope
- Named bring only brings the listed names — nothing else leaks in
- Aliases are optional — without `as`, the original name is used
- If a name doesn't exist in the file, throws `BringError` at runtime
- Circular brings (A brings from B, B brings from A) throw `BringError`

---

### Error

| Error | Trigger |
|-------|---------|
| `BringError` | File not found, name not found in file, or circular bring |

```
[XELL ERROR] Line 3 — BringError: 'deploy' not found in "./helpers.xel"
[XELL ERROR] Line 1 — BringError: circular bring detected between "a.xel" and "b.xel"
```

---

## 13. Scope Rules

Xell uses **dynamic scoping**. When looking up a variable, the interpreter walks up the call stack until it finds the variable.

```
Lookup order:
  1. Current block scope
  2. Enclosing block scope (for nested blocks)
  3. Caller's scope (for functions)
  4. Global scope
  5. Error: undefined variable
```

```xell
app_name = "xell"

fn print_name() :
    print app_name       # sees caller's 'app_name'
;

print_name()             # prints "xell"
```

---

## 14. Full Grammar Summary

```
PROGRAM       := { STATEMENT }

STATEMENT     := ( ASSIGNMENT
               | IF_STMT
               | FOR_STMT
               | WHILE_STMT
               | FN_DEF
               | GIVE_STMT
               | BRING_STMT
               | EXPRESSION )
               [ '.' ]

ASSIGNMENT    := IDENTIFIER '=' EXPRESSION

IF_STMT       := "if" EXPRESSION ':' BLOCK ';'
                 { "elif" EXPRESSION ':' BLOCK ';' }
                 [ "else" ':' BLOCK ';' ]

FOR_STMT      := "for" IDENTIFIER "in" EXPRESSION ':' BLOCK ';'

WHILE_STMT    := "while" EXPRESSION ':' BLOCK ';'

FN_DEF        := "fn" IDENTIFIER '(' [ PARAM_LIST ] ')' ':' BLOCK ';'

PARAM_LIST    := IDENTIFIER { ',' IDENTIFIER }

GIVE_STMT     := "give" [ EXPRESSION ]

BRING_STMT    := "bring" BRING_LIST "from" STRING [ "as" IDENTIFIER { ',' IDENTIFIER } ]
               | "bring" '*' "from" STRING
BRING_LIST    := IDENTIFIER { ',' IDENTIFIER }

BLOCK         := { STATEMENT }

EXPRESSION    := LOGICAL_OR
LOGICAL_OR    := LOGICAL_AND { "or" LOGICAL_AND }
LOGICAL_AND   := EQUALITY { "and" EQUALITY }
EQUALITY      := COMPARISON { ("==" | "!=" | "is" | "eq" | "ne") COMPARISON }
COMPARISON    := ADDITION { (">" | "<" | ">=" | "<=" | "gt" | "lt" | "ge" | "le") ADDITION }
ADDITION      := MULTIPLY { ("+" | "-") MULTIPLY }
MULTIPLY      := UNARY { ("*" | "/") UNARY }
UNARY         := "not" UNARY | PRIMARY

PRIMARY       := LITERAL
              | IDENTIFIER
              | CALL
              | INDEX_ACCESS
              | MEMBER_ACCESS
              | '(' EXPRESSION ')'

CALL          := IDENTIFIER '(' [ ARG_LIST ] ')'
ARG_LIST      := EXPRESSION { ',' EXPRESSION }

INDEX_ACCESS  := PRIMARY '[' EXPRESSION ']'
MEMBER_ACCESS := PRIMARY "->" IDENTIFIER

LITERAL       := STRING | NUMBER | BOOL | none | LIST | MAP

LIST          := '[' [ EXPRESSION { ',' EXPRESSION } ] ']'
MAP           := '{' [ MAP_ENTRY { ',' MAP_ENTRY } ] '}'
MAP_ENTRY     := IDENTIFIER ':' EXPRESSION

STRING        := '"' { CHAR | '{' EXPRESSION '}' } '"'
NUMBER        := DIGIT+ [ '.' DIGIT+ ]
BOOL          := "true" | "false"
none          := "none"
IDENTIFIER    := LETTER { LETTER | DIGIT | '_' }
```

---

## 15. Error Handling Tokens

Runtime errors will report the following info:
- **Line number** where the error occurred
- **Error type** — one of:

| Error | Trigger |
|-------|---------|
| `UndefinedVariable` | Variable used before assigned |
| `UndefinedFunction` | Function called that doesn't exist |
| `TypeError` | Wrong type for an operation |
| `IndexError` | List index out of range |
| `KeyError` | Map key doesn't exist |
| `DivisionByZero` | Division by zero |
| `FileNotFound` | Built-in file op on missing path |
| `BringError` | Brought file not found, name missing, or circular bring |
| `ArityError` | Wrong number of arguments to a function |

```
[XELL ERROR] Line 14 — TypeError: cannot add number and list
[XELL ERROR] Line 7  — UndefinedVariable: 'projct' is not defined (did you mean 'project'?)
[XELL ERROR] Line 22 — ArityError: 'setup' expects 2 args, got 1
```

---

## Quick Reference — Xell Syntax Cheatsheet

```xell
# Variables
name = "xell"
count = 42
ready = true
data = none

# Lists & Maps
ports = [3000, 8080]
cfg   = { host: "localhost", port: 3000 }

# Access
first_port = ports[0]
host       = cfg->host

# String interpolation
msg = "Running on {cfg->host}:{first_port}"

# Comments
# single line
--> multi
    line <--

# If
if count gt 10 and ready is true :
    print "ready and high count"
;
else :
    print "not yet"
;

# For
for port in ports :
    print "port: {port}"
;

# While
while count gt 0 :
    count = count - 1
;

# Function
fn deploy(project, env) :
    mkdir "{project}/dist"
    run "npm run build"
    give "{project} deployed to {env}"
;

result = deploy("my-app", "production")
print result

# Built-ins
mkdir "output/logs"
copy "template.xel" "new_script.xel"
run "git push"
log = run_capture("git log --oneline")
env_set "NODE_ENV" "production"
... etc

# Bring
bring * from "./helpers.xel"
bring setup from "./helpers.xel"
bring setup, deploy from "./helpers.xel" as s, d
```
