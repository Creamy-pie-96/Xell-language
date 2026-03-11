# Xell Debug System — API Reference

> **Auto-generated documentation for Xell's built-in debug decorators.**
> These decorators provide inline debugging, tracing, profiling, and logging without external tools.

---

## Quick Reference

| Decorator             | Purpose                                                     | Requires `@debug on`? |
| --------------------- | ----------------------------------------------------------- | --------------------- |
| `@debug on/off`       | Enable/disable the debug trace system                       | No (it IS the toggle) |
| `@debug sample N`     | Only trace every Nth event (sampling mode)                  | No                    |
| `@breakpoint`         | Take a variable snapshot or pause execution                 | Yes (for snapshots)   |
| `@watch("expr")`      | Alert when an expression becomes true                       | Yes                   |
| `@checkpoint("name")` | Save full state for time-travel debugging                   | Yes                   |
| `@track`              | Selectively track variables, functions, classes, categories | Yes                   |
| `@notrack`            | Exclude specific items from tracking                        | Yes                   |
| `@profile fn name`    | Measure function execution time                             | Yes                   |
| `@log "message"`      | Print a log message (always, or conditionally)              | No (always prints)    |

---

## CLI Debug Mode

### `xell --debug <file>`

Launches the interpreter in **debug IPC mode**. The interpreter:

1. Creates a Unix domain socket at `/tmp/xell-debug-<pid>.sock`
2. Prints `XELL_DEBUG_SOCKET:<path>` to stderr (for the IDE to discover)
3. Sets `tracer.stepping = true` so execution pauses before the first statement
4. Waits for an IDE client to connect before proceeding
5. Sends state JSON after each pause, blocks for a command
6. On completion, sends `"finished"` state and outputs the full trace JSON to stdout

**Usage from terminal:**

```bash
xell --debug myscript.xel 2>/tmp/debug.log &
# Read socket path from /tmp/debug.log
```

The IDE terminal handles this automatically — press **F5** to launch.

---

## IDE Keybindings (Live Debugging)

| Key           | Action                                   |
| ------------- | ---------------------------------------- |
| **F5**        | Start debug session / Continue execution |
| **F9**        | Toggle breakpoint on current line        |
| **F10**       | Step Over (execute one statement)        |
| **F11**       | Step Into (descend into function call)   |
| **Shift+F11** | Step Out (return to caller)              |
| **F12**       | Stop debug session                       |

---

## Debug IPC Protocol (Phase 7)

The debug system uses **Unix domain sockets** with newline-delimited JSON messages.

### Socket Path

```
/tmp/xell-debug-<pid>.sock
```

### IDE → Interpreter Commands

```json
{"cmd":"continue"}
{"cmd":"step_over"}
{"cmd":"step_into"}
{"cmd":"step_out"}
{"cmd":"stop"}
{"cmd":"add_breakpoint","line":25,"type":"pause"}
{"cmd":"add_breakpoint","line":10,"type":"snapshot"}
{"cmd":"remove_breakpoint","line":25}
{"cmd":"add_watch","expr":"x > 100"}
{"cmd":"remove_watch","expr":"x > 100"}
{"cmd":"jump_to","sequence":42}
{"cmd":"eval","expr":"x + y"}
```

### Interpreter → IDE State

```json
{
  "state": "paused",
  "line": 25,
  "seq": 42,
  "depth": 3,
  "vars": { "x": "10", "y": "hello" },
  "callStack": ["main:1", "foo:5", "bar:25"]
}
```

| State      | Meaning                       |
| ---------- | ----------------------------- |
| `paused`   | Execution suspended at a line |
| `running`  | Execution in progress         |
| `finished` | Script completed              |

### Interpreter → IDE Events

```json
{
  "event": "breakpoint_hit",
  "name": "epoch_start",
  "line": 10,
  "seq": 42
}
```

### Message Classes

| Class          | Description                                                                                                                      |
| -------------- | -------------------------------------------------------------------------------------------------------------------------------- |
| `DebugIPC`     | Blocking client — connect to a running debug server                                                                              |
| `DebugServer`  | Non-blocking server — accept one IDE client, send/recv                                                                           |
| `DebugMessage` | Parsed command struct with `cmd`, `line`, `type`, `expr`, `sequence`                                                             |
| `DebugCmd`     | Enum: Continue, StepOver, StepInto, StepOut, Stop, AddBreakpoint, RemoveBreakpoint, AddWatch, RemoveWatch, JumpTo, Eval, Unknown |

---

## Detailed API

### `@debug on` / `@debug off`

Enables or disables the trace collector. All trace-dependent decorators (`@breakpoint`, `@watch`, `@checkpoint`, `@track`, `@profile`) only activate when debug is on.

**Syntax:**

```xell
@debug on       # enable tracing
@debug off      # disable tracing
```

**Example:**

```xell
@debug on
x = 10          # this assignment is traced
@debug off
y = 20          # this assignment is NOT traced
```

---

### `@debug sample N`

Sets sampling mode: only every Nth trace event is recorded. Useful for reducing noise in loops or recursive calls.

**Syntax:**

```xell
@debug sample N     # N is a positive integer
```

**Example:**

```xell
@debug on
@debug sample 10    # only record every 10th event
for i in range(1000):
    x = i * 2
;
```

---

### `@breakpoint`

Takes a snapshot of all visible variables at the current point, or pauses execution.

**Syntax:**

```xell
@breakpoint                         # anonymous snapshot (non-blocking)
@breakpoint("name")                 # named snapshot (non-blocking)
@breakpoint("name") when EXPR       # conditional snapshot (only if EXPR is truthy)
@breakpoint pause                   # pause execution until Enter is pressed
@breakpoint pause N                 # pause execution for N seconds, then resume
```

**Example:**

```xell
@debug on

x = 10
@breakpoint("after x")             # snapshot: captures x=10

for i in range(5):
    x = x + i
    @breakpoint("loop") when x > 15    # only snapshot when x exceeds 15
;

@breakpoint pause                   # pause here — press Enter to continue
print("done")
```

**Notes:**

- Snapshots are recorded to the trace collector (visible in IDE LIFECYCLE tab).
- `pause` mode uses **IPC-based pause/resume** when running via `xell --debug` (F5 in IDE). Falls back to stdin in standalone mode.
- `pause N` sleeps for N seconds then auto-resumes.
- Conditional breakpoints evaluate the expression; if falsy, the breakpoint is skipped.
- **IDE breakpoints** (F9 / gutter click) are separate from `@breakpoint` decorators — they are sent to the interpreter over the IPC socket and stored in `TraceCollector::ideBreakpoints`.

---

### `@watch("expression")`

Registers a watch expression. The expression is evaluated after each traced statement; when it becomes truthy, a `WATCH_TRIGGERED` trace event is emitted.

**Syntax:**

```xell
@watch("expression")    # expression is a string containing valid Xell code
```

**Example:**

```xell
@debug on
@watch("x > 100")

x = 0
for i in range(50):
    x = x + i           # when x exceeds 100, a watch event fires
;
```

**Notes:**

- The expression string is parsed into an AST at parse time.
- Variable dependencies are extracted; the watch is only re-evaluated when a dependency changes.
- Watch events appear in the LIFECYCLE tab with the triggering value.

---

### `@checkpoint("name")`

Takes a full state snapshot (all variables, call stack) with a label. Used for time-travel debugging — you can compare checkpoints to see how state evolved.

**Syntax:**

```xell
@checkpoint("name")     # name is a string label
```

**Example:**

```xell
@debug on

data = [1, 2, 3]
@checkpoint("before sort")

data = sort(data)
@checkpoint("after sort")

# In the IDE, compare "before sort" vs "after sort" to see what changed
```

---

### `@track` / `@notrack`

Selectively include or exclude items from tracing. By default, `@debug on` traces everything. Use `@track` to focus on specific items, or `@notrack` to suppress noise.

**Syntax:**

```xell
# Track specific items
@track var(x, y, z)            # only trace these variables
@track fn(myFunc, helper)      # only trace these functions
@track class(MyClass)          # only trace this class
@track obj(myInstance)          # track a specific object instance

# Track categories (bare names, no parens)
@track loop                    # trace for/while loops
@track for                     # trace for loops only
@track while                   # trace while loops only
@track conditions              # trace if/elif/else branches
@track scope                   # trace scope enter/exit
@track imports                 # trace bring/import statements
@track returns                 # trace give (return) values
@track calls                   # trace function calls
@track mutations               # trace variable mutations
@track types                   # trace type changes
@track perf                    # trace performance metrics
@track recursion               # trace recursive calls

# Combine items and categories
@track var(x) fn(process) loop conditions

# Exclude specific items
@notrack var(temp, i, j)       # don't trace these variables
@notrack fn(helperUtil)        # don't trace this function
@notrack class(InternalClass)  # don't trace this class
```

**Example:**

```xell
@debug on
@track var(result, total) fn(calculate) loop

total = 0
for i in range(10):
    result = calculate(i)
    total = total + result
;
# Only 'result', 'total', calculate() calls, and loop iterations are traced
# Variables like 'i' and other functions are NOT traced
```

---

### `@profile fn name`

Registers a function for execution-time profiling. Each call to the function is timed, and results are emitted as trace events.

**Syntax:**

```xell
@profile fn functionName        # profile a specific function
@profile                        # profile the next statement (inline)
```

**Example:**

```xell
@debug on

fn fibonacci(n):
    if n <= 1:
        give n
    ;
    give fibonacci(n-1) + fibonacci(n-2)
;

@profile fn fibonacci

result = fibonacci(30)
# Profiling data shows: call count, total time, avg time per call
```

---

### `@log "message"`

Prints a log message to stdout. Supports `{variable}` interpolation in the message string. Can be conditional with `when`.

**Syntax:**

```xell
@log "message"                  # always print
@log "value is {x}"            # interpolate variable x into message
@log when EXPR "message"        # only print if EXPR is truthy
```

**Example:**

```xell
x = 42
@log "program started"
@log "x is currently {x}"              # prints: [LOG] x is currently 42

for i in range(10):
    x = x + i
    @log when x > 80 "x exceeded 80: {x}"   # only logs when condition met
;
```

**Notes:**

- `@log` always prints (does not require `@debug on`).
- Output format: `[LOG] message`
- If `@debug on` is active, a `LOG_MESSAGE` trace event is also emitted.
- `{varName}` placeholders are replaced with the variable's current value at runtime.
- If a variable in `{...}` is undefined, it shows `<undefined>`.

---

## Usage Patterns

### Basic Debugging

```xell
@debug on
# ... your code ...
@debug off
```

### Focused Debugging

```xell
@debug on
@track var(important_var) fn(critical_function)
@notrack var(i, j, temp)
# ... code with reduced noise ...
```

### Performance Profiling

```xell
@debug on
@profile fn slowFunction
@profile fn anotherFunction
# ... run your code, check timing in IDE ...
```

### Conditional Breakpoints

```xell
@debug on
for item in large_list:
    process(item)
    @breakpoint("found it") when item == target_value
;
```

### Log-Driven Debugging

```xell
# No @debug on needed for @log
@log "=== Starting processing ==="
for batch in batches:
    @log "Processing batch {batch}"
    @log when len(batch) > 1000 "⚠️ Large batch: {batch}"
;
@log "=== Done ==="
```
