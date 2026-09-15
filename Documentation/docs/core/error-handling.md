# Error Handling

> **Previous:** [Lambdas](lambdas.md)  
> **Next:** [Pattern Matching](pattern-matching.md)

---

Xell uses **exceptions** for error handling. The system is designed to be both simple and expressive, with a rich hierarchy of error types, structured error objects, and clean `try`/`catch`/`finally` semantics.

---

## Throwing Errors: `throw`

Use `throw` to raise an error. It accepts several forms:

### 1. String

```xell
throw "Something went wrong"
```

This raises a `RuntimeError` with the given message.

### 2. Map (structured error)

```xell
throw { "message": "Invalid input", "type": "ValueError" }
```

The map must contain a `"message"` string. The optional `"type"` key determines the error class (see [Error Types](#error-types) below). If `"type"` is omitted, it defaults to `"RuntimeError"`.

### 3. Instance (error object)

```xell
let err = ValueError("Invalid input")
throw err
```

The instance’s `typeName` and `message` field are used to determine the error class and message. This is the most flexible form and matches the error objects created by the interpreter.

### 4. Bare `throw` (re‑throw) – **not implemented**

```xell
throw    # ❌ currently raises RuntimeError("bare 'throw' with no value")
```

Bare throw is **not yet supported** – it is a placeholder for future re‑throw semantics.

---

## Catching Errors: `try` / `catch` / `finally`

### Basic `try`/`catch`

```xell
try :
    let x = 10 / 0
catch e :
    print("Caught: " + e->message)
;
```

- The `catch` clause captures the error in variable `e`.
- The error object has fields: `message`, `type`, `line`, and `traceback` (a list of stack frames).

### Type‑filtered `catch`

You can catch specific error types:

```xell
try :
    # code that might throw
catch e is TypeError :
    print("Type error: " + e->message)
catch e is ValueError or IndexError :
    print("Value or index error: " + e->message)
catch e :
    print("Other error: " + e->message)
;
```

- Multiple `catch` clauses are allowed.
- Each can match one or more error types using `is Type or Type`.
- The `catch` without a type filter catches **all** errors (including those not matching earlier clauses).
- Type matching uses the error’s `type` field. The type `"Error"` matches any error (the root of the hierarchy).

### `finally`

A `finally` block runs regardless of whether an error was thrown:

```xell
try :
    let file = open("data.txt")
    # process file
catch e :
    print("Error: " + e->message)
finally :
    close(file)   # always executed
;
```

- `finally` is optional.
- If both `catch` and `finally` are present, `catch` runs first, then `finally`.
- If an error occurs inside `catch`, `finally` still runs before the error propagates.

### Nested `try`/`catch`

```xell
try :
    try :
        throw "inner"
    catch e :
        print("inner catch")
        throw "rethrow from inner"   # propagates to outer
    ;
catch e :
    print("outer catch")
;
```

---

## Error Types

Xell defines a hierarchy of error classes, all inheriting from `Error`. The built‑in error types are:

| Category | Error Types |
|----------|-------------|
| **Core** | `Error` (base), `RuntimeError` |
| **Type** | `TypeError`, `ConversionError`, `HashError`, `IterationError` |
| **Value** | `ValueError`, `IndexError`, `KeyError` |
| **Arithmetic** | `DivisionByZero`, `OverflowError` |
| **Access** | `AttributeError`, `AccessError`, `ImmutabilityError` |
| **IO** | `IOError`, `FileNotFound` |
| **Process** | `ProcessError`, `CommandFailed` |
| **Environment** | `EnvError` |
| **Module** | `BringError`, `ModuleError`, `RequireError` |
| **Parse/Lex** | `ParseError`, `LexerError` |
| **Other** | `ArityError`, `RecursionError`, `AssertionError`, `NotImplementedError` |

These types are available as **global structs**, so you can use them for type checks and inheritance:

```xell
throw TypeError("Expected int")   # throws a TypeError instance
```

When you throw a map with `"type": "TypeError"`, Xell dispatches to the matching C++ exception class.

---

## Error Object Structure

Every caught error is an **instance** with the following fields:

- `message` – human‑readable error message (`string`)
- `type` – error type name (`string`)
- `line` – source line where the error occurred (`int`)
- `traceback` – list of stack frames, each a map with `name` and `line` (`list` of `map`)

```xell
try :
    throw "test"
catch e :
    print(e->type)          # "RuntimeError"
    print(e->line)          # line number
    for frame in e->traceback :
        print(frame["name"] + " at line " + str(frame["line"]))
    ;
;
```

---

## Common Patterns

### Input validation

```xell
fn parse_number(s):
    let n = to_int(s)
    if n < 0 :
        throw ValueError("Number must be non‑negative")
    give n
;
```

### Resource cleanup

```xell
try :
    let conn = connect_db()
    # work with connection
finally :
    if conn : conn->close() ;
;
```

### Propagating errors with context

```xell
try :
    process_file("data.txt")
catch e :
    throw { "message": "Processing failed: " + e->message, "type": e->type }
;
```

---

## Error Handling in Expression‑Mode

`try`/`catch`/`finally` are **statements**, not expressions. They cannot be used as part of an expression. For expression‑mode error handling, use `throw` inside an expression (e.g., in a ternary) and catch it at a higher level.

```xell
let result = if x > 0 : compute(x) else : throw "negative not allowed" ;
```

---

## Best Practices

1. **Be specific** – catch only what you can handle.
2. **Use structured errors** – maps with `message` and `type` provide more context than plain strings.
3. **Clean up in `finally`** – release resources (files, connections) there.
4. **Don't overuse** – errors are for exceptional conditions, not normal control flow.
5. **Document thrown errors** – mention which errors a function may throw in its docstring.

---

## Limitations

- **Bare `throw` is not implemented** – re‑throw is not available.
- **No custom error hierarchy** yet – user‑defined error types must be created as classes, but they won't automatically map to C++ exception types unless you use the built‑in type names.
- **Tracebacks** are captured only for errors that are caught; uncaught errors show the traceback at the top level.

---

## Next Steps

Now that you can handle errors, learn about **[Pattern Matching](pattern-matching.md)** – a powerful way to destructure and match values.

---

**Previous:** [Lambdas](lambdas.md)  
**Next:** [Pattern Matching](pattern-matching.md)