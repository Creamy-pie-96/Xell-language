# Decorators

> **Previous:** [Shell](shell.md)  
> **Next:** [Built‑ins](builtins.md)

---

Decorators are a powerful metaprogramming feature that let you wrap or modify functions, classes, and even loops without changing their source code. Xell provides a rich set of built‑in decorators for debugging, tracing, profiling, and more – plus you can define your own.

---

## Quick Reference

| Keyword/Syntax | What it does | Minimal example |
|----------------|--------------|-----------------|
| `@<name>` | Applies a decorator to the following definition | `@debug fn foo(): ... ;` |
| `@debug` | Enables runtime tracing for a function | `@debug fn factorial(n): ... ;` |
| `@debug on / off` | Turns tracing on/off globally | `@debug on` |
| `@debug sample N` | Samples function calls (first and last N/2) | `@debug sample 10` |
| `@breakpoint` | Captures a snapshot of all variables | `@breakpoint("state")` |
| `@breakpoint pause` | Pauses execution | `@breakpoint pause` |
| `@breakpoint pause N` | Pauses for N seconds | `@breakpoint pause 5` |
| `@breakpoint("name") when EXPR` | Conditional snapshot | `@breakpoint("check") when x > 100` |
| `@watch("expr")` | Evaluates expression and triggers when true | `@watch("x > 100")` |
| `@track category(...)` | Selective variable/function tracking | `@track var(x, y) loop conditions` |
| `@notrack` | Excludes from tracking | `@notrack var(temp)` |
| `@profile [fn name]` | Measures execution time | `@profile fn heavy_work` |
| `@log "message"` | Prints a log message | `@log "starting process"` |
| `@log when COND "msg"` | Conditional logging | `@log when debug "state: {x}"` |
| `@checkpoint("name")` | Saves full state for time‑travel | `@checkpoint("before_update")` |
| `@safe_loop` | Ensures a loop contains a `break` | `@safe_loop loop : break ;` |
| `@eager` | Forces eager loading of a module | `@eager bring * from "mod"` |
| `@dataclass` | Marks a class as a data class | `@dataclass class Point : x = 0 y = 0 ;` |
| `@immutable` | Makes all instances immutable | `@immutable class Config : ... ;` |
| `@singleton` | Ensures only one instance exists | `@singleton class App : ... ;` |
| User‑defined decorator | A function that takes a function/class and returns a new one | `fn trace(f): fn wrapped(...): ... ;` |

---

## Decorator Syntax

Decorators are placed on the line immediately before the definition they modify:

```xell
@debug
fn factorial(n):
    if n <= 1: give 1
    give n * factorial(n - 1)
;
```

Multiple decorators stack (they are applied **bottom‑up**, i.e., the last decorator is the innermost):

```xell
@log
@debug
fn compute(x): give x * 2 ;
# `debug` is applied first, then `log` wraps the result
```

Decorators can be applied to:
- Functions (`fn` and `async fn`)
- Classes (`class`, `abstract`, `mixin`)
- Loops (`loop`)
- Import statements (`bring`)

---

## Built‑in Decorators

### `@debug` – Runtime Tracing

Enables tracing for the decorated function or scope.

```xell
@debug
fn factorial(n):
    if n <= 1: give 1
    give n * factorial(n - 1)
;

print(factorial(3))
```

The interpreter will emit trace events (variable changes, function calls, returns) that can be captured by the IDE or `--trace-vars` mode.

**Global toggles:**

```xell
@debug on   # enable tracing globally
@debug off  # disable
```

**Sampling mode (`@debug sample N`):**  
Traces the first `N/2` calls/iterations and the last `N/2` calls/iterations, giving a representative view without overwhelming the trace log.

```xell
@debug sample 10
fn heavy_loop(): ... ;
```

### `@breakpoint` – Snapshots and Pauses

Captures a snapshot of all visible variables at a specific point.

**`@breakpoint("name")`** – captures a named snapshot:

```xell
fn process(data):
    @breakpoint("before_processing")
    # ... data processing ...
;
```

**`@breakpoint pause`** – blocks execution until resumed by the IDE (or by pressing Enter in the terminal if no IDE is connected):

```xell
fn inspect():
    @breakpoint pause
    # ... paused, you can inspect variables ...
;
```

**`@breakpoint pause N`** – pauses for N seconds:

```xell
@breakpoint pause 5
print("This shows after 5 seconds pause")
```

**Conditional breakpoint:** `@breakpoint("name") when CONDITION`

```xell
@breakpoint("high_value") when x > 1000
```

### `@watch("expr")` – Expression Watching

Evaluates the expression after every statement and triggers an event when it becomes true.

```xell
let counter = 0
@watch("counter > 100")
for i in range(200):
    counter += 1
;
# When counter becomes 101, the watch triggers
```

- The expression is parsed and its dependencies tracked.
- It is evaluated only when a dependency changes (dirty‑flag optimisation).

### `@track` / `@notrack` – Selective Tracking

Controls which variables, functions, classes, or categories are traced.

**Syntax:**

```xell
@track var(x, y) loop conditions    # track variables x,y, loop events, condition branches
@notrack var(temp)                  # exclude `temp` from tracking
```

**Supported categories:**

| Category | What it tracks |
|----------|----------------|
| `var(a,b)` | Specific variables by name |
| `fn(a,b)` | Specific functions |
| `class(a,b)` | Specific classes |
| `obj(a,b)` | Specific object instances |
| `loop` | All loop events (`for`, `while`, `loop`) |
| `for` | `for`‑loop events only |
| `while` | `while`‑loop events only |
| `conditions` | Branch conditions (`if`, `elif`, `else`) |
| `scope` | Scope entry/exit |
| `imports` | Module loads |
| `returns` | Function returns |
| `calls` | Function calls |
| `mutations` | Variable assignments |
| `types` | Type checks |
| `perf` | Performance events |
| `recursion` | Recursive calls |

**Example:**

```xell
@track var(result) loop
fn compute():
    let result = 0
    for i in range(100):
        result += i
    ;
    give result
;
```

Unknown categories are silently ignored – be careful with typos.

### `@profile` – Performance Measurement

Measures the execution time of a function.

```xell
@profile fn heavy_computation()
# When called, prints: [PROFILE] heavy_computation: 1.23ms
```

Can also be used standalone:

```xell
@profile
heavy_computation()   # profiles only this call
```

### `@log` – User‑Defined Logging

Prints a message with optional interpolation and conditional execution.

**Simple log:**

```xell
@log "Starting process"
```

**Conditional log (`when`):**

```xell
@log when debug_enabled "Processing item {item_id}"
```

Interpolation uses `{variable}` placeholders:

```xell
let name = "Xell"
@log "Hello, {name}"   # prints: Hello, Xell
```

### `@checkpoint` – State Savepoint

Saves the entire state (all visible variables) for time‑travel debugging. Used by the IDE’s time‑travel features.

```xell
@checkpoint("before_update")
# ... code ...
@checkpoint("after_update")
```

### `@safe_loop` – Loop Break Guarantee

Ensures that a loop contains a `break` statement; if not, a runtime error is thrown.

```xell
@safe_loop
loop :
    if condition : break
;
```

If the loop body does not contain a `break` (at the same nesting level), the interpreter will raise an error. This prevents infinite loops.

### `@eager` – Eager Module Loading

Forces a module to be loaded immediately rather than lazily (future optimisation). Applied to `bring` statements.

```xell
@eager bring * from "heavy_module"
```

### `@dataclass` – Data Class Marker

Marks a class as a data class. Currently a placeholder – it documents intent and may be used by future code generation.

```xell
@dataclass class Point :
    x = 0
    y = 0
;
```

### `@immutable` – Immutable Class

Makes **every instance** of the class immutable (all fields read‑only after construction).

```xell
@immutable class Config :
    url = "https://api.example.com"
    timeout = 30
;

let c = Config()
c->timeout = 60   # ❌ throws ImmutabilityError
```

### `@singleton` – Singleton Class

Ensures that only one instance of the class is ever created. Subsequent calls to the constructor return the same instance.

```xell
@singleton class App :
    fn __init__(self): print("Creating App") ;
;

let a = App()   # prints "Creating App"
let b = App()   # returns the same instance (no print)
```

---

## User‑Defined Decorators

Any function can be used as a decorator. It receives the decorated object (function or class) and returns a new object (usually a wrapper).

```xell
fn trace(fn):
    fn wrapped(*args):
        print("Calling " + fn->name)
        let result = fn(...args)
        print("Returned " + str(result))
        give result
    ;
    give wrapped
;

@trace
fn add(a, b): give a + b ;

add(2, 3)
```

**Expected output:**
```
Calling add
Returned 5
```

---

## What NOT to Do

### 1. Applying a decorator without a definition

```xell
@debug
let x = 10   # ❌ decorator can only be applied to fn/class/loop/bring
```

**Why it fails:** The parser only accepts decorators before `fn`, `class`, `abstract`, `mixin`, `loop`, or `bring` statements.

### 2. Using `@breakpoint` without `@debug on`

```xell
@breakpoint("start")   # ❌ tracing must be enabled globally
fn process(): ... ;
```

**Why it fails:** `@breakpoint` only works when the `TraceCollector` is enabled. Enable it with `@debug on` at the top of your file, or via the `--trace-vars` / `--debug` CLI flags.

### 3. Using `@safe_loop` on a loop with a `break` inside a nested loop only

```xell
@safe_loop
loop :
    for i in range(3):
        break   # ❌ this break only exits the inner for, not the outer loop
    ;
;
```

**Why it fails:** `bodyContainsBreak` ignores `break` statements that are inside nested loops. The `@safe_loop` check only counts `break`s that are at the same nesting level as the loop itself.

### 4. Relying on unknown `@track` categories

```xell
@track catagory   # ❌ typo – "catagory" is silently ignored
fn compute(): ... ;
```

**Why it fails:** Unknown category names are ignored without warning. Check the spelling against the supported list.

### 5. Forgetting that `@debug sample N` only samples the first and last N/2

```xell
@debug sample 10
for i in range(1000): ... ;   # only ~10 iterations are traced
```

**Why it's intentional:** This is a feature – it gives a spectrum of behaviour without flooding the log. Be aware of the sampling distribution.

---

## See Also

- [Debugging](../tooling/debugging.md) – for CLI flags and IDE integration
- [Functions](functions.md) – for user‑defined decorator patterns
- [Classes](../oop/oop-classes.md) – for `@dataclass`, `@immutable`, `@singleton`
- [Control Flow](control-flow.md) – for `@safe_loop`
- [Modules Import](../modules/modules-import.md) – for `@eager`

---

**Previous:** [Shell](shell.md)  
**Next:** [Built‑ins](builtins.md)