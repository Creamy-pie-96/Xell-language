# Xell Debug System — Usage Guide

A complete reference for Xell's **language-native** debugging and tracing system.
Six decorator keywords let you control debugging directly from your code:

| Decorator     | Purpose                           |
| ------------- | --------------------------------- |
| `@debug`      | Enable/disable tracing            |
| `@breakpoint` | Take snapshots or pause execution |
| `@watch`      | Monitor expressions for changes   |
| `@checkpoint` | Save named state snapshots        |
| `@track`      | Whitelist what to trace           |
| `@notrack`    | Blacklist what to exclude         |

---

## Table of Contents

- [Quick Start](#quick-start)
- [@debug — Section & Function Tracing](#debug--section--function-tracing)
- [@breakpoint — Snapshots & Pausing](#breakpoint--snapshots--pausing)
- [@watch — Expression Monitoring](#watch--expression-monitoring)
- [@checkpoint — Named Snapshots](#checkpoint--named-snapshots)
- [@track / @notrack — Selective Tracing](#track--notrack--selective-tracing)
- [Combining Decorators](#combining-decorators)
- [Live Debugging (F5 Session)](#live-debugging-f5-session)
- [Trace Events Reference](#trace-events-reference)
- [Snapshot Details](#snapshot-details)
- [Zero-Cost Guarantee](#zero-cost-guarantee)
- [Limitations & Notes](#limitations--notes)

---

## IDE Run Modes

The Xell IDE has four execution modes. Debug decorators (`@debug`, `@breakpoint`, `@watch`, etc.)
are only active in **Debug Run** and **Live Debug** modes.

| Shortcut       | Mode       | What runs                     | Variables | Objects | Dashboard | Lifecycle | Debug decorators | Stepping |
| -------------- | ---------- | ----------------------------- | :-------: | :-----: | :-------: | :-------: | :--------------: | :------: |
| **Ctrl+Enter** | Quick Run  | Selection, or line 1 → cursor |    ✅     |   ✅    |    ✅     |    ❌     |        ❌        |    ❌    |
| **Ctrl+R**     | Normal Run | Entire current file           |    ✅     |   ✅    |    ✅     |    ❌     |        ❌        |    ❌    |
| **Ctrl+D**     | Debug Run  | Selection, or line 1 → cursor |    ✅     |   ✅    |    ✅     |    ✅     |        ✅        |    ❌    |
| **F5**         | Live Debug | Entire current file via IPC   |    ✅     |   ✅    |    ✅     |    ✅     |        ✅        |    ✅    |

**Quick Run / Normal Run** — Fast execution. Variables, Objects, and Dashboard tabs
are always populated via static analysis. No lifecycle tracking, no runtime debug overhead.

**Debug Run** — Full debugging. Same code selection as Ctrl+Enter, but also runs
`--trace-vars` to collect lifecycle events (VAR_BORN, VAR_CHANGED, FUNCTION_CALLED, etc.).
Debug decorators (`@breakpoint`, `@watch`, `@checkpoint`, `@track`, `@notrack`) are active.

**Live Debug (F5)** — Interactive step-through debugging. Launches `xell --debug <file>`
as a subprocess, connects via Unix domain socket IPC, and pauses before the first
statement. Use F10/F11/Shift+F11 to step, F5 to continue, F9 to toggle breakpoints,
F12 to stop. The editor gutter shows breakpoint indicators (●) and the current
debug line arrow (▶). The TIMELINE and CALLSTACK tabs update in real-time.

> **Tip:** Use Ctrl+Enter for rapid iteration, Ctrl+D for post-mortem trace analysis,
> and F5 when you need interactive step-through debugging.

---

## Quick Start

```xell
@debug on
x = 10
x = x + 5
@debug off
```

This traces only the two lines between `@debug on` and `@debug off`. The trace
records `VAR_BORN x = 10`, `VAR_CHANGED x = 15`, and scope events.

---

## @debug — Section & Function Tracing

### @debug on / @debug off

Toggle tracing for a section of code:

```xell
@debug on
a = 1
b = 2
c = a + b
@debug off

# This line is NOT traced:
d = 100
```

**Effect:** `trace_->enabled` is set to `true` / `false`. All hooks check this
flag before emitting events — zero overhead when off.

### @debug sample N

Enable **sampling mode** — trace only the first and last N iterations in loops:

```xell
@debug on
@debug sample 10

for i in range(10000) :
    process(i)
;

@debug off
```

With `sample 10`:

- Iterations 0–9 are fully traced
- Iterations 10–9989 are skipped (no events emitted)
- Iterations 9990–9999 are fully traced

This lets you debug loop behaviour without the overhead of tracing all 10,000
iterations.

### @debug on function

Decorate a function definition to trace **only that function's execution**:

```xell
@debug
fn calculate(x, y) :
    result = x * y + x
    give result
;

# Tracing is OFF here — global scope not traced
a = calculate(3, 4)   # <-- Tracing turns ON inside calculate(), OFF on return
b = calculate(5, 6)   # <-- Same: ON inside, OFF after
```

**How it works:**

1. At definition time, the function name is registered with `enableForFunction()`.
2. When `callUserFn()` is invoked, it checks `isFunctionDebugEnabled(fn.name)`.
3. If enabled, tracing is temporarily turned on for the function's execution.
4. When the function returns, the previous tracing state is restored.

This is ideal for debugging a specific function without drowning in global trace
output.

---

## @breakpoint — Snapshots & Pausing

### @breakpoint("name") — Snapshot (Non-Blocking)

Takes an instant snapshot of all visible variables without stopping execution:

```xell
@debug on
x = 10
y = 20
@breakpoint("before_calc")
z = x + y
@breakpoint("after_calc")
@debug off
```

Each `@breakpoint("name")` call:

1. Captures ALL visible variables (deep copy of values, serialized to strings)
2. Records the full call stack
3. Stores everything in the `snapshots_` vector
4. Continues execution immediately

Snapshots are safe to inspect after execution — they store value copies, never
raw pointers.

### @breakpoint pause — Execution Pause

Pauses execution. In **Live Debug mode (F5)**, this sends a "paused" state to the
IDE over IPC and waits for a resume command. In standalone mode, it reads from stdin:

```xell
@debug on
x = compute_something()
@breakpoint pause
# In F5 mode: IDE shows paused state, press F5 to continue
# In standalone: press Enter to continue
y = compute_more()
@debug off
```

### @breakpoint pause N — Timed Pause

Pauses for N seconds, then automatically resumes:

```xell
@debug on
@breakpoint pause 3
# Waits 3 seconds, then continues
print("resumed!")
@debug off
```

### @breakpoint("name") when EXPR — Conditional Breakpoint

Only takes a snapshot when the expression evaluates to truthy:

```xell
@debug on
for i in range(100) :
    @breakpoint("found_it") when i == 42
    process(i)
;
@debug off
```

The snapshot is only taken when `i == 42`. Other iterations skip the breakpoint
entirely.

You can use any valid Xell expression after `when`:

```xell
@breakpoint("large_list") when len(items) > 1000
@breakpoint("error_state") when status == "error"
@breakpoint("match") when x > 0 and y < 10
```

---

## @watch — Expression Monitoring

### @watch("expression")

Monitors an expression and emits a `WATCH_TRIGGERED` event when it transitions
from false to true:

```xell
@debug on
@watch("x > 10")

x = 0
x = 5
x = 15   # <-- WATCH_TRIGGERED fires here (false → true transition)
x = 20   # <-- No trigger (already true)
x = 3    # <-- Resets to false
x = 12   # <-- WATCH_TRIGGERED fires again (false → true)

@debug off
```

**Key behaviour:**

- Watches are **dependency-aware**. `@watch("x > 10")` only re-evaluates when
  `x` changes (detected from the AST), not after every statement.
- The trigger fires on the **false → true transition**, not every time the
  expression is true.
- After triggering, the watch resets: it won't fire again until the expression
  becomes false and then true again.

### How Dependencies Work

When you write `@watch("x + y > threshold")`, the system:

1. Parses the expression string into an AST
2. Walks the AST to extract all referenced variable names: `{x, y, threshold}`
3. Stores these as `dependsOn` in the watch
4. After any variable assignment, `markDirty(varName)` checks if any watch
   depends on that variable
5. Only dirty watches are re-evaluated — others are skipped entirely

This makes watches very efficient even in tight loops.

---

## @checkpoint — Named Snapshots

```xell
@debug on

# ... do setup ...
@checkpoint("initial_state")

# ... do processing ...
@checkpoint("after_processing")

# ... do cleanup ...
@checkpoint("final_state")

@debug off
```

Checkpoints are essentially named `@breakpoint` snapshots — they capture the
full state (all variables, call stack) at that point. The difference is semantic:
checkpoints are intended for Time Travel debugging (Phase 10), where you can
replay back to any checkpoint.

---

## @track / @notrack — Selective Tracing

### @track — Whitelist Specific Items

```xell
@debug on
@track var(loss, epoch) perf loop

for epoch in range(1000) :
    loss = forward()
    backward(loss)
;

@debug off
```

This traces **only**:

- Variables `loss` and `epoch` (other variables are ignored)
- Performance timing events
- Loop events

### @notrack — Blacklist Specific Items

```xell
@debug on
@notrack fn(helper, _internal)
@notrack var(temp, _cache)

# Everything is traced EXCEPT:
# - calls to helper() and _internal()
# - variables temp and _cache
compute()
@debug off
```

### Categories

Both `@track` and `@notrack` support **item categories** and **event categories**:

**Item categories** (with parenthesized lists):

| Category | Syntax                | Effect                                  |
| -------- | --------------------- | --------------------------------------- |
| `var`    | `var(x, y, z)`        | Track/exclude specific variables        |
| `fn`     | `fn(compute, helper)` | Track/exclude specific functions        |
| `class`  | `class(Dog, Cat)`     | Track/exclude specific classes          |
| `obj`    | `obj(myInst, other)`  | Track/exclude specific object instances |

**Event categories** (bare names):

| Category     | What it controls                     |
| ------------ | ------------------------------------ |
| `loop`       | All loop events (for + while)        |
| `for`        | Only for-loop events                 |
| `while`      | Only while-loop events               |
| `conditions` | Branch events (if/elif/else)         |
| `scope`      | Scope enter/exit events              |
| `imports`    | Module load events                   |
| `returns`    | Function return events               |
| `calls`      | Function call events                 |
| `mutations`  | Variable mutation events             |
| `types`      | Type-related events                  |
| `perf`       | Performance timing (off by default)  |
| `recursion`  | Recursion detection (off by default) |

### Combining @track categories

```xell
@debug on
@track var(x, y) fn(compute) loop conditions

# Only traces: variables x and y, calls to compute(), loop events, and branches
for i in range(100) :
    x = compute(i)
    if x > 50 :
        y = x * 2
    ;
;
@debug off
```

### @notrack after @track

```xell
@debug on
@track var(x, y, z, temp)
@notrack var(temp)

# Traces x, y, z but NOT temp
temp = setup()
x = process(temp)
@debug off
```

Blacklist takes priority over whitelist for the same item.

---

## Combining Decorators

Decorators can be combined to create sophisticated debugging setups:

```xell
# Trace only a specific function, with sampling and watches
@debug
fn train(data) :
    @debug sample 50
    @track var(loss, accuracy) perf
    @watch("loss < 0.01")

    for epoch in range(10000) :
        loss = forward(data)
        accuracy = evaluate()
        backward(loss)
        @breakpoint("epoch_end") when epoch % 100 == 0
    ;
;
```

This:

1. Only traces inside `train()` (not global scope)
2. Samples first/last 50 loop iterations
3. Only tracks `loss` and `accuracy` variables + perf timing
4. Fires a watch when `loss` drops below 0.01
5. Takes a snapshot every 100 epochs

---

## Live Debugging (F5 Session)

### Overview

Press **F5** in the Xell IDE terminal to start an interactive debug session.
The IDE launches `xell --debug <file>` as a subprocess, connects via a Unix
domain socket, and gives you full step-through control.

### How It Works

1. **F5** pressed → IDE launches `xell --debug myscript.xel`
2. Interpreter creates socket at `/tmp/xell-debug-<pid>.sock`
3. Interpreter prints `XELL_DEBUG_SOCKET:/tmp/xell-debug-<pid>.sock` to stderr
4. IDE reads stderr, connects the debug client to the socket
5. IDE syncs any existing gutter breakpoints to the interpreter
6. Interpreter pauses before the first statement, sends state JSON
7. IDE highlights the current line in the editor (▶ arrow in gutter, yellow background)
8. User steps through code with F10/F11, or continues with F5

### Keybindings

| Key           | Action                                             |
| ------------- | -------------------------------------------------- |
| **F5**        | Start debug session / Continue (resume execution)  |
| **F9**        | Toggle breakpoint on current editor line           |
| **F10**       | Step Over — execute one statement, skip into calls |
| **F11**       | Step Into — descend into the next function call    |
| **Shift+F11** | Step Out — run until the current function returns  |
| **F12**       | Stop — terminate the debug session                 |

### Editor Indicatorsmake -C /home/DATA/CODE/code/Xell/build -j$(nproc) 2>&1 | tail -10 && echo "---BUILD OK---" && cd /home/DATA/CODE/code/Xell/build && ./debug_test 2>&1 | tail -30

During a debug session, the editor gutter shows:

| Symbol | Color  | Meaning                |
| ------ | ------ | ---------------------- |
| ▶      | Yellow | Current execution line |
| ●      | Red    | Pause breakpoint       |
| ●      | Purple | Snapshot breakpoint    |

The current execution line also has a yellow-tinted background in the code area.

### IDE Breakpoints vs Decorator Breakpoints

There are two kinds of breakpoints:

1. **IDE breakpoints** — Set via F9 or clicking the editor gutter. These are
   sent to the interpreter over IPC as `add_breakpoint`/`remove_breakpoint`
   commands. They are stored in `TraceCollector::ideBreakpoints` and checked
   at the top of `exec()`.

2. **Decorator breakpoints** — Written in code as `@breakpoint("name")`. These
   take snapshots during trace-mode execution. They work in both Ctrl+D and F5 modes.

Both types are visible in the gutter. IDE breakpoints can be toggled at any time
during a debug session.

### Bottom Panel Tabs

| Tab       | Content                                                   |
| --------- | --------------------------------------------------------- |
| REPL      | Interactive Xell shell                                    |
| OUTPUT    | stdout/stderr from the current run                        |
| VARIABLES | Current scope variables (all run modes)                   |
| OBJECTS   | Object instances and their fields                         |
| DASHBOARD | Runtime statistics and metrics                            |
| LIFECYCLE | Trace events from @debug decorators (Ctrl+D and F5)       |
| TIMELINE  | Real-time event feed during F5 sessions (stepping events) |
| CALLSTACK | Live call stack during F5 sessions                        |
| HELP      | Keyboard shortcuts and usage reference                    |

The **TIMELINE** tab shows a scrollable list of debug events with color coding:

- 🟡 Yellow — paused (stepping)
- 🔵 Blue — step executed
- 🔴 Red — breakpoint hit

The **CALLSTACK** tab shows the current call stack with a ▶ indicator on the
active frame. Click a frame to navigate to that source location.

### IPC Protocol Reference

The debug IPC uses **newline-delimited JSON** over Unix domain sockets.

**IDE → Interpreter (commands):**

| Command             | Fields         | Description                   |
| ------------------- | -------------- | ----------------------------- |
| `continue`          | —              | Resume execution              |
| `step_over`         | —              | Execute one statement         |
| `step_into`         | —              | Descend into function         |
| `step_out`          | —              | Return to caller              |
| `stop`              | —              | Terminate execution           |
| `add_breakpoint`    | `line`, `type` | Set a breakpoint              |
| `remove_breakpoint` | `line`         | Remove a breakpoint           |
| `add_watch`         | `expr`         | Add a watch expression        |
| `remove_watch`      | `expr`         | Remove a watch expression     |
| `jump_to`           | `sequence`     | Jump to trace sequence number |
| `eval`              | `expr`         | Evaluate expression in scope  |

**Interpreter → IDE (state updates):**

```json
{
  "state": "paused",
  "line": 25,
  "seq": 42,
  "depth": 3,
  "vars": { "x": "10", "name": "hello" },
  "callStack": ["main:1", "process:15", "helper:25"]
}
```

### Cross-Module Debugging

When a debug session encounters a `bring` statement, the trace collector is
shared with the imported module's interpreter. This means:

- Breakpoints set in the main file apply when stepping into brought modules
- The call stack shows cross-module frames (e.g., `moduleA:10 → main:25`)
- Timeline events include a `module` field identifying the source module
- The trace collector's `setCurrentModule()` tracks module transitions

### Example Session

```xell
# myscript.xel
fn fibonacci(n) :
    if n <= 1 :
        give n
    ;
    give fibonacci(n-1) + fibonacci(n-2)
;

result = fibonacci(10)
print(result)
```

1. Press **F5** — debugger launches, pauses at line 1
2. Editor shows ▶ on line 1 (the `fn` definition)
3. Press **F10** three times — steps to `result = fibonacci(10)`
4. Press **F11** — steps INTO fibonacci, now at line 2
5. Press **F10** — steps over `if` check
6. Press **Shift+F11** — runs until fibonacci returns, back at line 9
7. Press **F5** — continues to end, session finishes

---

## Trace Events Reference

| Event               | When Emitted                          |
| ------------------- | ------------------------------------- |
| `VAR_BORN`          | First assignment to a new variable    |
| `VAR_CHANGED`       | Re-assignment of existing variable    |
| `VAR_ENTERED_SCOPE` | Variable becomes visible in scope     |
| `VAR_EXITED_SCOPE`  | Variable goes out of scope            |
| `VAR_DIED`          | Scope destroyed, variable unreachable |
| `FN_CALLED`         | Function call begins                  |
| `FN_RETURNED`       | Function returns                      |
| `FN_ERRORED`        | Unhandled error in function           |
| `LOOP_STARTED`      | Loop begins                           |
| `LOOP_ITERATION`    | Each loop iteration                   |
| `LOOP_BROKE`        | `break` hit                           |
| `LOOP_COMPLETED`    | Loop finishes naturally               |
| `BRANCH_IF`         | `if` condition evaluated true         |
| `BRANCH_ELIF`       | `elif` condition evaluated true       |
| `BRANCH_ELSE`       | `else` branch taken                   |
| `BRANCH_SKIPPED`    | Branch condition was false            |
| `SCOPE_ENTER`       | New scope created                     |
| `SCOPE_EXIT`        | Scope destroyed                       |
| `MODULE_LOADED`     | `bring` resolved a module             |
| `MODULE_CACHED`     | `bring` hit the module cache          |
| `ERROR_THROWN`      | Runtime error raised                  |
| `ERROR_CAUGHT`      | `catch` block activated               |
| `ERROR_PROPAGATED`  | Error bubbled up through scope        |
| `OBJ_CREATED`       | Object constructed                    |
| `BREAKPOINT_HIT`    | `@breakpoint` snapshot taken          |
| `WATCH_TRIGGERED`   | `@watch` expression became true       |
| `CHECKPOINT_SAVED`  | `@checkpoint` saved state             |

---

## Snapshot Details

Every snapshot captures:

| Field       | Description                                           |
| ----------- | ----------------------------------------------------- |
| `name`      | Breakpoint/checkpoint name                            |
| `line`      | Source line number                                    |
| `sequence`  | Monotonic event counter at snapshot time              |
| `module`    | Module name (empty = main file)                       |
| `scopeVars` | `scope_name → {var_name → (type, value)}` — deep copy |
| `callStack` | Copy of the call stack at snapshot time               |

Snapshots store **value copies** (serialized strings), never raw `Environment*`
pointers. This means they are safe to read long after execution ends — the
original scopes may have been destroyed.

---

## Zero-Cost Guarantee

All trace hooks in the interpreter follow this pattern:

```
if (!trace_ || !trace_->enabled) return;
if (!filter_.shouldTrackVar(name)) return;
```

When tracing is disabled (no `@debug on`), there is **zero overhead** — just a
null pointer check. When tracing is enabled but filtered, the TrackFilter gates
each event with O(1) hash lookups.

---

## Limitations & Notes

1. **`@breakpoint pause`** uses IPC-based pause/resume when running via F5 (Live Debug).
   In standalone mode (no IPC), it falls back to stdin.

2. **`@watch`** evaluates in the current environment. If the watch references
   variables not in scope, it will throw (caught and ignored by the trace system).

3. **Sampling** (`@debug sample N`) applies to loop iterations. Non-loop code is
   always traced when `@debug on`.

4. **`@track`/`@notrack` are additive** — each `@track` call adds to the
   existing whitelist, it doesn't replace it. Use `@debug off` + `@debug on`
   to reset filters.

5. **`@debug` on a function** is a decorator — it goes on the line before `fn`:

   ```xell
   @debug
   fn myFunc() :
       # traced
   ;
   ```

   Don't confuse with `@debug on` (standalone statement).

6. **Watch dependency tracking** extracts variable names from the parsed AST.
   Complex expressions like `obj->method()` will track `obj` but not the
   method return value — the watch re-evaluates when `obj` changes.

7. **Live Debug (F5)** requires the `xell` binary to be installed and accessible
   in `$PATH`. The IDE terminal launches it as a subprocess.

8. **IPC sockets** are Unix domain sockets — currently Linux/macOS only.
   Socket files are automatically cleaned up when the debug session ends.

9. **Cross-module stepping** shares the trace collector across `bring` calls.
   The module field in trace entries identifies which module generated each event.

---

## Phase Status

| Phase | Feature                 | Status |
| ----- | ----------------------- | ------ |
| 1     | Extended symbol JSON    | ✅     |
| 2     | Enhanced Variables Tab  | ✅     |
| 3     | Dashboard Panel         | ✅     |
| 4     | TraceCollector engine   | ✅     |
| 5     | @breakpoint + @watch    | ✅     |
| 6     | Cross-module debug      | ✅     |
| 7     | Debug socket IPC        | ✅     |
| 8     | Timeline Tab            | ✅     |
| 9     | Step-through (F10/F11)  | ✅     |
| 10    | Time Travel engine      | ⬜     |
| 11    | Variable Lifecycle view | ✅     |
| 12    | Call Stack live view    | ✅     |
| 13    | Polish                  | ✅     |
