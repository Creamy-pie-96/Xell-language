# Debugging

> **Previous:** [Module Indexing](module-indexing.md)  
> **Next:** [Notebook Kernel](kernel.md)

---

Xell provides a rich set of debugging and tracing tools that work both in scripts and interactively (REPL, IDE). From simple variable lifecycle tracing to full step‑through execution with breakpoints, you can inspect and control your program's behaviour at every level.

---

## Quick Reference

| Feature | Command / Syntax | What it does |
|---------|------------------|--------------|
| Variable tracing | `--trace-vars <file>` | Outputs JSON variable lifecycle events |
| Global debug toggle | `@debug on` / `@debug off` | Enables/disables tracing globally |
| Sampling | `@debug sample N` | Traces first N/2 and last N/2 iterations |
| Function debug | `@debug fn` | Enables tracing for a specific function |
| Breakpoint | `@breakpoint("name")` | Captures a snapshot of all variables |
| Pause breakpoint | `@breakpoint pause` | Pauses execution (IPC or stdin) |
| Timed pause | `@breakpoint pause N` | Pauses for N seconds |
| Conditional breakpoint | `@breakpoint("name") when EXPR` | Snapshot only if condition is true |
| Watch expression | `@watch("expr")` | Evaluates expression, triggers when true |
| Checkpoint | `@checkpoint("name")` | Full state save for time‑travel |
| Profile | `@profile [fn]` | Measures execution time of a function |
| Conditional logging | `@log "message"` or `@log when EXPR "msg"` | Prints interpolated log messages |
| Selective tracking | `@track var(x, y) loop conditions` | Tracks specific variables/categories |
| Exclude tracking | `@notrack var(temp)` | Excludes from tracking |
| Step‑through debugging | `--debug <file>` | Starts debug IPC server, IDE can step/continue |
| Debug socket | `XELL_DEBUG_SOCKET:<path>` | Printed to stderr for IDE connection |

---

## Overview

Xell's debugging system is built around a `TraceCollector` that records events (variable births, changes, function calls, returns, loop iterations, branch decisions, etc.) and can be queried for snapshots and watch evaluations.

There are three main usage modes:

1. **Passive tracing** – run the program with `--trace-vars` to get a JSON log of everything that happened.
2. **Instrumented debugging** – add `@breakpoint`, `@watch`, `@log` etc. in your code to inspect state at specific points.
3. **Interactive debugging** – use `--debug` to connect an IDE for step‑through control.

---

## Variable Lifecycle Tracing

### `--trace-vars <file>`

Runs the script with tracing enabled and outputs a JSON array of all trace events to stdout. This is ideal for the IDE's **LIFECYCLE** tab.

```bash
$ xell --trace-vars script.xel
[
  {"timestamp":1234567890,"event":"VAR_BORN","name":"x","type":"int","value":"10","line":1},
  {"timestamp":1234567891,"event":"FN_CALLED","name":"square","line":3},
  {"timestamp":1234567892,"event":"VAR_BORN","name":"y","type":"int","value":"100","line":4},
  {"timestamp":1234567893,"event":"FN_RETURNED","name":"square","value":"100","line":5}
]
```

**Events traced:** `VAR_BORN`, `VAR_CHANGED`, `VAR_ENTERED_SCOPE`, `VAR_EXITED_SCOPE`, `VAR_DIED`, `FN_CALLED`, `FN_RETURNED`, `FN_ERRORED`, `LOOP_STARTED`, `LOOP_ITERATION`, `LOOP_BROKE`, `LOOP_COMPLETED`, `BRANCH_IF`, `BRANCH_ELIF`, `BRANCH_ELSE`, `BRANCH_SKIPPED`, `SCOPE_ENTER`, `SCOPE_EXIT`, `MODULE_LOADED`, `MODULE_CACHED`, `ERROR_THROWN`, `ERROR_CAUGHT`, `ERROR_PROPAGATED`, `OBJ_CREATED`, `BREAKPOINT_HIT`, `WATCH_TRIGGERED`, `CHECKPOINT_SAVED`, `PROFILE_RESULT`, `LOG_MESSAGE`, `SAMPLE_GAP`.

### `@debug on` / `@debug off`

Globally enable/disable tracing at runtime.

```xell
@debug on
fn compute(): ... ;   # traced
@debug off
fn ignore(): ... ;    # not traced
```

### `@debug sample N`

Sampling mode: traces only the first `N/2` iterations and the last `N/2` iterations of loops or function calls. This gives a representative view without overwhelming the trace log.

```xell
@debug sample 10
for i in range(1000):
    # Only ~10 iterations are traced (first 5 and last 5)
;
```

### `@debug fn`

Enables tracing for a specific function (useful for focusing on a single function without flooding the trace).

```xell
@debug fn factorial
fn factorial(n):
    if n <= 1: give 1
    give n * factorial(n - 1)
;
```

---

## Selective Tracking: `@track` / `@notrack`

Control exactly what gets traced.

### `@track var(x, y) loop conditions`

Enables tracking of specific variables, functions, classes, or event categories.

```xell
@track var(result) loop for
@track fn(process) class(Data)
```

**Supported categories:**

| Category | What it tracks |
|----------|----------------|
| `var(a, b, ...)` | Specific variables by name |
| `fn(a, b, ...)` | Specific functions |
| `class(a, b, ...)` | Specific classes |
| `obj(a, b, ...)` | Specific object instances |
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

### `@notrack var(temp)`

Excludes from tracking (blacklist).

```xell
@notrack var(temp) loop while
```

---

## Breakpoints and Snapshots

### `@breakpoint("name")`

Captures a **snapshot** of all visible variables at that point. The snapshot is stored for later inspection (e.g., in the IDE's time‑travel timeline).

```xell
fn process(data):
    @breakpoint("before_transform")
    let transformed = transform(data)
    @breakpoint("after_transform")
;
```

### `@breakpoint pause`

**Pauses** execution. How it resumes depends on the context:

- **If a debug IPC session is active** (`--debug`): the interpreter enters step‑through mode and waits for the IDE to send `continue` or `step`.
- **Else if `pause N` is specified**: pauses for `N` seconds (timed).
- **Otherwise**: waits for Enter on stdin (fallback).

```xell
@breakpoint pause           # waits for IDE or Enter
@breakpoint pause 5         # sleeps for 5 seconds
```

### Conditional Breakpoint

```xell
@breakpoint("high_value") when x > 1000
```

The snapshot is only taken if the condition evaluates to `true`.

### `@checkpoint("name")`

Similar to a breakpoint, but also saves the **full state** for time‑travel debugging (supported by the IDE).

```xell
@checkpoint("before_update")
# ... code ...
@checkpoint("after_update")
```

---

## Watching Expressions

### `@watch("expr")`

Evaluates `expr` after every statement. When the expression becomes `true` (i.e., transitions from `false` to `true`), a `WATCH_TRIGGERED` event is emitted.

```xell
let counter = 0
@watch("counter > 100")
for i in range(200):
    counter += 1
;
# When counter reaches 101, the watch triggers
```

Dependencies are extracted from the expression, so evaluation is **lazy** – it's only checked when a dependency changes.

---

## Performance Profiling

### `@profile [fn]`

Measures execution time of the next statement or a specific function.

```xell
@profile fn heavy_computation
heavy_computation()   # prints: [PROFILE] heavy_computation: 1.23ms
```

**Standalone usage:**

```xell
@profile
expensive_call()   # profiles only this call
```

---

## Conditional Logging

### `@log "message"`

Prints the message with `{variable}` interpolation.

```xell
let name = "Alice"
@log "Processing item {name}"
# prints: [LOG] Processing item Alice
```

### `@log when EXPR "message"`

Conditional logging – only prints if `EXPR` evaluates to truthy.

```xell
let debug_mode = true
@log when debug_mode "Debug: {x} > {y}"
```

---

## Step‑Through Debugging (`--debug`)

### Starting a Debug Session

```bash
$ xell --debug script.xel
XELL_DEBUG_SOCKET:/tmp/xell-debug-12345.sock
```

The interpreter starts a debug server (Unix socket on Linux/macOS, named pipe on Windows) and waits for an IDE to connect. The socket path is printed to stderr.

### IDE Commands

Once connected, the IDE can send JSON commands:

| Command | JSON | Effect |
|---------|------|--------|
| Continue | `{"cmd":"continue"}` | Resume execution until next breakpoint |
| Step Over | `{"cmd":"step_over"}` | Execute one statement, skip function internals |
| Step Into | `{"cmd":"step_into"}` | Pause at first line of called function |
| Step Out | `{"cmd":"step_out"}` | Continue until current function returns |
| Stop | `{"cmd":"stop"}` | Abort execution |
| Add Breakpoint | `{"cmd":"add_breakpoint","line":25,"type":"snapshot"}` | Set breakpoint at line |
| Remove Breakpoint | `{"cmd":"remove_breakpoint","line":25}` | Remove breakpoint |
| Add Watch | `{"cmd":"add_watch","expr":"x > 100"}` | Add watch expression |
| Remove Watch | `{"cmd":"remove_watch","expr":"x > 100"}` | Remove watch expression |

### Interpreter → IDE Messages

| Message | Description |
|---------|-------------|
| `{"state":"paused","line":25,"seq":42,"vars":{...},"callStack":[...]}` | Execution paused |
| `{"state":"running"}` | Execution resumed |
| `{"state":"finished","exitCode":0}` | Program finished |
| `{"event":"breakpoint_hit","name":"epoch_start","line":10,"seq":42}` | Breakpoint hit |
| `{"event":"watch_triggered","expr":"x > 100","line":15,"seq":55}` | Watch triggered |
| `{"event":"error","message":"...","line":10}` | Runtime error occurred |

### `--source-dir` with `--debug`

```bash
$ xell --debug script.xel --source-dir /project
```

Overrides module resolution to `/project` – useful when the script is in a temporary directory but should resolve imports from a project directory.

---

## Complete Example: Debugging a Function

```xell
# debug_example.xel
@debug on
@track var(n, result) loop

fn factorial(n):
    @breakpoint("factorial_start") when n > 5
    if n <= 1:
        @log "Base case: {n}"
        give 1
    let result = n * factorial(n - 1)
    @log "n={n}, result={result}"
    give result
;

@profile
print(factorial(3))
```

**Running with `--trace-vars`:**

```bash
$ xell --trace-vars debug_example.xel
[
  {"event":"VAR_BORN","name":"n","value":"3","line":1},
  {"event":"FN_CALLED","name":"factorial","line":5},
  {"event":"VAR_BORN","name":"result","value":"6","line":9},
  {"event":"FN_RETURNED","name":"factorial","value":"6","line":10},
  ...
]
```

**Running with `--debug`:**

```bash
$ xell --debug debug_example.xel
XELL_DEBUG_SOCKET:/tmp/xell-debug-12345.sock
[IDE connects, sends "continue"]
[breakpoint hit at line 5]
[IDE inspects variables, sends "step_over"]
...
```

---

## What NOT to Do

### 1. Using `@debug on` Without Enabling the Trace Collector

The trace collector is enabled by default in the REPL and with `--trace-vars` / `--debug`. In normal execution, `@debug on` will have no effect unless tracing is globally enabled via CLI flags.

### 2. Using `@breakpoint` Without `@debug on` or `--trace-vars`

Breakpoints only capture snapshots when tracing is enabled.

### 3. Expecting `@breakpoint pause` to Work in a Script Without IPC

If no IDE is connected and no timeout is specified, `@breakpoint pause` will block waiting for stdin input. This may cause scripts to hang in headless environments.

### 4. Using `@watch` with a Very Expensive Expression

Watch expressions are evaluated after every statement. Avoid expensive computations in watches.

### 5. Forgetting That `@track` and `@notrack` Are Case‑Sensitive

```xell
@track var(X)   # tracks variable "X" – not "x"
```

### 6. Using `@log` Without Interpolation Placeholders

If you don't use `{variable}` in the message, it's still printed, but without substitution.

### 7. Expecting `--trace-vars` Output to Be Human‑Readable

It outputs raw JSON for machine processing. Use `jq` or the IDE to view it.

### 8. Using `@profile` on a Function That Throws

Profiling a function that throws an error will not produce a profile result.

### 9. Assuming `@debug sample N` Works for Non‑Loop Events

Sampling applies to loop iterations and function calls, not to every event. The exact sampling logic is implementation‑specific and may change.

### 10. Using `--debug` Without an IDE Listening

The interpreter will wait indefinitely for a connection if the IDE does not connect. Use `--debug` only when you intend to attach a debugger.

---

## See Also

- [CLI Commands](cli.md) – `--trace-vars`, `--debug`, `--source-dir`
- [REPL Usage](repl.md) – debugging in the interactive shell
- [Decorators](../core/decorators.md) – `@debug`, `@breakpoint`, `@watch`, `@track`, `@profile`, `@log`
- [Error Handling](../core/error-handling.md) – runtime error tracing

---

**Previous:** [Module Indexing](module-indexing.md)  
**Next:** [Notebook Kernel](kernel.md)