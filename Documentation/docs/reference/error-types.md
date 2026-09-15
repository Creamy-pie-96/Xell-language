# Error Types – Complete Reference

> **Previous:** [Keywords](keywords.md)  
> **Next:** [Built‑in Index](builtin-index.md)

---

This page lists every error type in Xell, organised by category. All errors inherit from `XellError` (which itself inherits from `std::runtime_error`), so a single `catch (e is XellError)` will catch any Xell‑specific error.

Each error type is available as a **global class/struct**, so you can use them in type checks and `belong` in `incase`.

---

## Quick Reference

| Category | Error Types |
|----------|-------------|
| **Lexer** | `LexerError` |
| **Parser** | `ParseError` |
| **Type** | `TypeError`, `ConversionError`, `HashError`, `IterationError` |
| **Value** | `ValueError`, `IndexError`, `KeyError` |
| **Name** | `UndefinedVariableError`, `UndefinedFunctionError` |
| **Arithmetic** | `DivisionByZeroError`, `OverflowError` |
| **Access** | `AttributeError`, `AccessError`, `ImmutabilityError` |
| **IO / Filesystem** | `IOError`, `FileNotFoundError` |
| **Process** | `ProcessError`, `CommandFailedError` |
| **Environment** | `EnvError` |
| **Module / Import** | `BringError`, `ModuleError`, `RequireError` |
| **Recursion** | `RecursionError` |
| **Assertion** | `AssertionError` |
| **Function** | `ArityError` |
| **Other** | `NotImplementedError`, `RuntimeError` |

---

## Base Error

### `XellError`

The base class for all Xell errors. All other error types inherit from this.

```xell
try :
    # code that might throw
catch e :
    print("Error: " + e->message)
    print("Type: " + e->type)
    print("Line: " + str(e->line))
    print("Traceback: " + str(e->traceback))
;
```

Every error object has these fields:

| Field | Type | Description |
|-------|------|-------------|
| `message` | `string` | Human‑readable error message. |
| `type` | `string` | Error type name (e.g., `"TypeError"`). |
| `line` | `int` | Source line where the error occurred. |
| `traceback` | `list` | List of stack frames (`{name, line}`). |

---

## Lexer Errors

### `LexerError`

Thrown when the lexer encounters invalid input – malformed numbers, unterminated strings, or unexpected characters.

```xell
# Unterminated string
let s = "hello   # ❌ LexerError

# Invalid number
let n = 0xG   # ❌ LexerError (invalid hex digit)
```

---

## Parse Errors

### `ParseError`

Thrown when the parser encounters invalid syntax – missing delimiters, unexpected tokens, or malformed statements.

```xell
# Missing semicolon
let x = 10   # ❌ ParseError – expected ';'

# Missing colon in if
if x > 0 print("positive") ;   # ❌ ParseError – expected ':'

# Unclosed block
fn foo() : print("hi")   # ❌ ParseError – expected ';' to close block
```

---

## Type Errors

### `TypeError`

Thrown when an operation is performed on a value of the wrong type.

```xell
"hello" + 5          # ❌ TypeError – string + int
[]()                 # ❌ TypeError – list is not callable
len(42)              # ❌ TypeError – len() expects a collection
```

### `ConversionError`

Thrown when a type conversion fails (e.g., parsing an invalid number).

```xell
to_int("hello")      # ❌ ConversionError – cannot convert 'hello' to int
to_float("abc")      # ❌ ConversionError – cannot convert 'abc' to float
```

### `HashError`

Thrown when trying to hash a non‑hashable value.

```xell
let s = set([1, 2])
add(s, [3, 4])       # ❌ HashError – list is not hashable

let m = {}
set(m, [1, 2], "val") # ❌ HashError – list is not hashable
```

### `IterationError`

Thrown when trying to iterate over a non‑iterable value.

```xell
for i in 42 :        # ❌ IterationError – int is not iterable
    print(i)
;
```

---

## Value Errors

### `ValueError`

Thrown when a value is of the right type but inappropriate for the operation.

```xell
mod(10, 0)           # ❌ DivisionByZeroError
range(5, 1, 0)       # ❌ ValueError – step cannot be zero
slice([1,2,3], 2, 1) # ❌ ValueError – start > end
```

### `IndexError`

Thrown when accessing an index out of range (list, tuple, string).

```xell
let l = [1, 2, 3]
l[10]                # ❌ IndexError – index out of range
"abc"[5]             # ❌ IndexError – string index out of range
```

### `KeyError`

Thrown when accessing a non‑existent key in a map.

```xell
let m = { "a": 1 }
m["b"]               # ❌ KeyError – 'b' not found
```

---

## Name Errors

### `UndefinedVariableError`

Thrown when a variable is referenced before being assigned.

```xell
print(x)             # ❌ UndefinedVariableError – 'x' is not defined
let y = z + 1        # ❌ UndefinedVariableError – 'z' is not defined
```

### `UndefinedFunctionError`

Thrown when calling an undefined function.

```xell
foo(42)              # ❌ UndefinedFunctionError – 'foo' is not defined
```

---

## Arithmetic Errors

### `DivisionByZeroError`

Thrown when dividing by zero (or modulo by zero).

```xell
10 / 0               # ❌ DivisionByZeroError
10 % 0               # ❌ DivisionByZeroError
```

### `OverflowError`

Thrown when a numeric operation overflows (future‑proofing; currently used sparingly).

```xell
# (Currently limited – 64‑bit integers wrap)
```

---

## Access Errors

### `AttributeError`

Thrown when accessing a non‑existent field or method on an instance.

```xell
class Point : x = 0 y = 0 ;
let p = Point()
p->z                 # ❌ AttributeError – 'Point' has no field 'z'
```

### `AccessError`

Thrown when accessing a `private` or `protected` member from outside its allowed scope.

```xell
class Secret :
    private:
        password = "hunter2"
;

let s = Secret()
s->password          # ❌ AccessError – 'password' is private
```

### `ImmutabilityError`

Thrown when trying to modify an immutable value or binding.

```xell
immutable x = 10
x = 20               # ❌ ImmutabilityError – cannot reassign immutable

@immutable class Config : url = "" ;
let c = Config()
c->url = "new"       # ❌ ImmutabilityError – instance is frozen

let l = [1, 2, 3]
let frozen = iSet(l) # frozen set
add(frozen, 4)       # ❌ ImmutabilityError – cannot modify frozen set
```

---

## IO / Filesystem Errors

### `IOError`

Thrown for general I/O failures – permission denied, disk full, etc.

```xell
write("/root/file.txt", "content")   # ❌ IOError – permission denied
read("/dev/null")                    # ❌ IOError – not a file
```

### `FileNotFoundError`

Thrown when a file or directory does not exist.

```xell
read("missing.txt")   # ❌ FileNotFoundError – 'missing.txt' does not exist
mkdir("/root/")       # ❌ IOError – permission denied
```

---

## Process Errors

### `ProcessError`

Thrown when an external command cannot be executed.

```xell
run("nonexistent_cmd")   # ❌ ProcessError – command not found
```

### `CommandFailedError`

Thrown when `set_e()` is enabled and an external command exits with a non‑zero code.

```xell
set_e()
run("false")   # ❌ CommandFailedError – command exited with code 1
```

---

## Environment Errors

### `EnvError`

Thrown when accessing a non‑existent environment variable.

```xell
env_get("NONEXISTENT")   # ❌ EnvError – environment variable 'NONEXISTENT' not found
```

---

## Module / Import Errors

### `BringError`

Thrown when an import fails – module not found, file not found, or circular import.

```xell
bring MissingModule     # ❌ BringError – module 'MissingModule' not found
bring square from "missing.xel"   # ❌ BringError – cannot open file
```

### `ModuleError`

Thrown for general module‑related errors.

```xell
# (Rare – used for module metadata issues)
```

### `RequireError`

Thrown when a `requires` dependency cannot be satisfied.

```xell
module App :
    requires MissingModule   # ❌ RequireError – required module 'MissingModule' not found
;
```

---

## Recursion Errors

### `RecursionError`

Thrown when the maximum call depth is exceeded.

```xell
fn infinite():
    infinite()
;
infinite()   # ❌ RecursionError – maximum recursion depth exceeded
```

---

## Assertion Errors

### `AssertionError`

Thrown when an `assert` condition is `false`.

```xell
assert(x > 0, "x must be positive")   # throws if x <= 0
```

---

## Function Errors

### `ArityError`

Thrown when a function is called with the wrong number of arguments.

```xell
fn add(a, b): give a + b ;
add(5)      # ❌ ArityError – expects 2 arg(s), got 1
add(1, 2, 3) # ❌ ArityError – expects 2 arg(s), got 3
```

---

## Other Errors

### `NotImplementedError`

Thrown when a feature is not yet implemented.

```xell
# (Internal – used for stubs)
```

### `RuntimeError`

A generic runtime error for situations that don't fit other categories.

```xell
throw "Something went wrong"   # throws RuntimeError
```

---

## Complete Example: Error Handling

```xell
try :
    let result = risky_operation()
catch e is TypeError :
    print("Type error: " + e->message)
catch e is IndexError or KeyError :
    print("Index/key error: " + e->message)
catch e is DivisionByZeroError :
    print("Division by zero at line " + str(e->line))
catch e :
    print("Other error: " + e->message)
finally :
    print("Cleanup")
;
```

---

## See Also

- [Error Handling](../core/error-handling.md) – using `try`/`catch`/`finally`
- [Keywords](keywords.md) – `throw`, `try`, `catch`, `finally`
- [Built‑in Index](builtin-index.md) – built‑in functions

---

**Previous:** [Keywords](keywords.md)  
**Next:** [Built‑in Index](builtin-index.md)