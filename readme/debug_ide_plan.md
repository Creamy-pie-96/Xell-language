# Xell Debug System & IDE Enhancement Plan

## Overview

A **language-native** debugging and tracing system. Not bolted on — the interpreter itself speaks debug. Six decorator keywords (`@debug`, `@breakpoint`, `@watch`, `@checkpoint`, `@track`, `@notrack`) plus IDE integration for a complete debugging experience rivaling production debuggers.

**Core Philosophy:** Debugging is a first-class language feature, not an external tool.

```
╔══════════════════════════════════════════════════════╗
║              Xell Debug System                       ║
╠══════════════════════════════════════════════════════╣
║                                                      ║
║  @debug on / @debug off                              ║
║  → full trace of marked section                      ║
║  → all events, all variables, all lifecycles         ║
║                                                      ║
║  @debug sample <N>                                   ║
║  → traces first 50% and last 50% of N items          ║
║  → ideal for loops with 10000 iterations             ║
║                                                      ║
║  @breakpoint("name")                                 ║
║  → silent snapshot at that line                      ║
║  → state captured, execution continues               ║
║                                                      ║
║  @breakpoint pause                                   ║
║  → execution stops, step through                     ║
║  → press Tab to resume                               ║
║                                                      ║
║  @breakpoint pause <seconds>                         ║
║  → auto-resumes after N seconds                      ║
║                                                      ║
║  @breakpoint("name") when <condition>                ║
║  → conditional: only triggers when true              ║
║                                                      ║
║  @watch("expression")                                ║
║  → alerts when expression becomes true               ║
║  → tracked across entire execution                   ║
║                                                      ║
║  @checkpoint("name")                                 ║
║  → full state save for time-travel                   ║
║  → delta-compressed, disk-backed                     ║
║                                                      ║
║  @track <category>                                   ║
║  → selectively track ONLY specified things           ║
║  → everything else is silent                         ║
║                                                      ║
║  @notrack <category>                                 ║
║  → selectively EXCLUDE from tracking                 ║
║  → everything else still tracked                     ║
║                                                      ║
╠══════════════════════════════════════════════════════╣
║  IDE Controls:                                       ║
║  🔵 gutter click → snapshot breakpoint               ║
║  🟣 gutter click → pause breakpoint                  ║
║  (🔴 = error, 🟡 = warning — already taken)         ║
║  Ctrl+D  → smart debug run                           ║
║  F5      → continue                                  ║
║  F10     → step over                                 ║
║  F11     → step into                                 ║
║  F12     → step out                                  ║
╚══════════════════════════════════════════════════════╝
```

Six major systems:

1. **XellTrace** — Language-level trace/debug engine (interpreter)
2. **Decorator System** — `@debug`, `@breakpoint`, `@watch`, `@checkpoint`, `@track`, `@notrack`
3. **Time Travel** — Delta-snapshot engine for backward/forward stepping
4. **Enhanced IDE Panels** — Variables Tab, Dashboard Tab, Timeline, Call Stack
5. **Extended `--check-symbols` JSON** — Data source for all IDE panels at edit-time
6. **Cross-Module Debug** — Single tracer instance across all modules

---

## Part 1 — Decorator-Based Debug System

### 1.1 `@debug` — Section Tracing

Traces everything between `on` and `off`:

```
@debug on
x = 10
result = factorial(5)
show(result)
@debug off
```

On a function — traces all calls:

```
@debug
fn train_model() :
    load_data()
    for epoch in range(10) :
        loss = forward()
        backward(loss)
    ;
    save_model()
;
```

Sampling mode — for anything repetitive (loops, recursive functions, repeated calls):

```
@debug sample 100
for i in range(10000) :
    process(i)
;
```

Traces iterations 0-49 and 9950-9999 (first 50% + last 50% of sample size).

On recursive functions:

```
@debug sample 20
fn fibonacci(n) :
    if n <= 1 : give n ;
    give fibonacci(n-1) + fibonacci(n-2)
;
fibonacci(100)   # traces first 10 + last 10 calls
```

On repeatedly called functions:

```
@debug sample 50
fn process_batch(batch) :
    transform(batch)
;
for b in batches :
    process_batch(b)   # 1000 calls → traces first 25 + last 25
;
```

Sampling applies to **any repeated execution** — loops, recursive calls, or functions called in a loop. The tracer counts invocations and applies the 50/50 split.

### 1.2 `@breakpoint` — Point-in-Time Inspection

**Snapshot mode** (non-blocking):

```
@breakpoint("epoch_start")
loss = forward()
@breakpoint("after_loss")
backward(loss)
```

Runs to completion. IDE shows all snapshots side-by-side after.

**Pause mode** (blocking):

```
@breakpoint pause           # pauses, Tab to resume
@breakpoint pause 5         # pauses 5 sec, auto-resumes
```

**Conditional**:

```
@breakpoint("big_loss") when loss > 0.5
```

Only triggers when condition is true.

**IDE gutter** (🔴/🟡 already taken for error/warning):

```
ln 23  🔵 │ for epoch in range(10) :       ← snapshot (click once)
ln 24     │     loss = forward()
ln 25  🟣 │     backward(loss)              ← pause (click twice)
```

Click once → 🔵 snapshot. Click again → 🟣 pause. Click again → removed.

### 1.3 `@watch` — Expression Monitoring

```
@watch("loss < 0.01")
@watch("len(buffer) > 1000")
```

Logs `WATCH_TRIGGERED` when expression becomes true.

**Optimization:** Uses a dependency-aware dirty flag. Each watch expression is analyzed for variable names it references. A watch is only re-evaluated when one of its dependency variables is written to (VAR_CHANGED/VAR_BORN). This turns O(statements × watch_complexity) into O(mutations × relevant_watches).

```cpp
struct WatchExpr {
    std::string expression;
    ExprPtr parsed;
    bool lastValue = false;
    std::unordered_set<std::string> dependsOn;  // {"loss", "buffer"}
};

// In Environment::set():
if (trace_->enabled) {
    trace_->markDirty(varName);  // O(1)
}

// In exec() after each statement:
if (trace_->enabled && trace_->hasDirtyWatches()) {
    trace_->evaluateDirtyWatches(env, interp, line);
}
```

### 1.4 `@checkpoint` — Explicit Time Travel Save

```
@checkpoint("before_training")
train()
@checkpoint("after_training")
```

Creates a full state save that the time travel engine can jump to instantly.

### 1.5 `@track` / `@notrack` — Selective Tracking

The power feature. Instead of tracing everything (expensive), selectively choose what to track.

**Variable tracking:**

```
@track var(x, y, z)           # only these 3 variables
@notrack var(temp, _internal) # everything except these
```

**Function tracking:**

```
@track fn(process, calculate)     # only these functions
@notrack fn(helper, _internal)    # everything except these
```

**Category tracking:**

```
@track conditions     # if/elif/else branch events
@track loop          # all loop events (for + while)
@track for           # only for-loop events
@track while         # only while-loop events
@track scope         # scope enter/exit events
@track imports       # module loading and resolution
@track returns       # function return values only
@track calls         # function calls only (not returns)
@track mutations     # only VAR_CHANGED (not VAR_BORN)
@track types         # only when TYPE changes (int→str)
@track perf          # timing/performance per operation
@track recursion     # recursion depth and call chain
```

**Object tracking:**

```
@track class(Dog)    # all instances of class Dog
@track obj(myDog)    # specific object instance
```

**Combinable on one line:**

```
@track var(loss, epoch) perf loop
@notrack fn(helper, _internal)
```

**Full example:**

```
@debug on
@track var(loss, epoch) perf loop
@notrack fn(helper, _internal)

for epoch in range(1000) :
    loss = forward()
    backward(loss)
;

@debug off
```

This traces only: the variables `loss` and `epoch`, performance timings, and loop events. Ignores function calls to `helper` and `_internal`. Result: a focused, low-overhead trace.

**Implementation:**

```cpp
struct TrackFilter {
    // Whitelist (if non-empty, ONLY these are tracked)
    std::unordered_set<std::string> trackVars;
    std::unordered_set<std::string> trackFns;
    std::unordered_set<std::string> trackClasses;
    std::unordered_set<std::string> trackObjs;

    // Category flags (default: all true when @debug on)
    bool trackConditions = true;
    bool trackLoopFor = true;
    bool trackLoopWhile = true;
    bool trackScope = true;
    bool trackImports = true;
    bool trackReturns = true;
    bool trackCalls = true;
    bool trackMutations = true;
    bool trackTypes = true;
    bool trackPerf = false;       // off by default (overhead)
    bool trackRecursion = false;  // off by default

    // Blacklist (always excluded)
    std::unordered_set<std::string> notrackVars;
    std::unordered_set<std::string> notrackFns;

    bool shouldTrackVar(const std::string& name) const;
    bool shouldTrackFn(const std::string& name) const;
    bool shouldTrackEvent(TraceEvent event) const;
};
```

### 1.6 Decorator → Interpreter Mapping

| Decorator                    | AST Node                       | Interpreter Action                             |
| ---------------------------- | ------------------------------ | ---------------------------------------------- |
| `@debug on`                  | `DebugOnStmt`                  | `trace_.enabled = true`                        |
| `@debug off`                 | `DebugOffStmt`                 | `trace_.enabled = false`                       |
| `@debug sample N`            | `DebugSampleStmt(n)`           | `trace_.setSampling(n)`                        |
| `@debug` (on fn)             | `DebugDecorator` on FnDef      | `trace_.enableForFunction(name)`               |
| `@breakpoint("name")`        | `BreakpointStmt(name)`         | `trace_.snapshot(name, env)`                   |
| `@breakpoint pause`          | `BreakpointPauseStmt`          | `trace_.pause()`                               |
| `@breakpoint pause N`        | `BreakpointPauseStmt(N)`       | `trace_.pauseTimed(N)`                         |
| `@breakpoint("n") when cond` | `BreakpointCondStmt(n,cond)`   | eval cond, if true → snapshot                  |
| `@watch("expr")`             | `WatchStmt(expr)`              | `trace_.addWatch(expr)`                        |
| `@checkpoint("name")`        | `CheckpointStmt(name)`         | `timeTravel_.saveCheckpoint(name)`             |
| `@track var(x,y)`            | `TrackStmt(category, items)`   | `trace_.filter.trackVars = {x,y}`              |
| `@track perf loop`           | `TrackStmt(categories)`        | `filter.trackPerf=true; filter.trackLoop=true` |
| `@notrack fn(a,b)`           | `NotrackStmt(category, items)` | `trace_.filter.notrackFns = {a,b}`             |

---

## Part 2 — XellTrace Engine

### 2.1 Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Interpreter                               │
│                                                             │
│   exec(stmt) ──► TraceCollector.emit(event) ──► TraceLog   │
│                                                             │
│   Environment.set()  ──► VAR_BORN / VAR_CHANGED            │
│   execBlock()        ──► SCOPE_ENTER / SCOPE_EXIT          │
│   callUserFn()       ──► FN_CALLED / FN_RETURNED           │
│   execFor/While()    ──► LOOP events                       │
│   execIf()           ──► BRANCH events                     │
│   execBring()        ──► MODULE events                     │
│                                                             │
│   TrackFilter gates every emission ──► zero-cost if off    │
│   Module calls ──► same TraceCollector (cross-module)       │
│                                                             │
└──────────────────────┬──────────────────────────────────────┘
                       │
                       ▼
              TraceLog (vector<TraceEntry>)
                       │
                       ├──► JSON serialization ──► IDE
                       ├──► Timeline view
                       ├──► Variable lifecycle
                       ├──► Call stack view
                       └──► Time travel engine (delta snapshots)
```

### 2.2 Core Data Structures

**File: `src/interpreter/trace_collector.hpp`**

```cpp
enum class TraceEvent {
    // Variables
    VAR_BORN,           // First assignment
    VAR_CHANGED,        // Re-assignment
    VAR_ENTERED_SCOPE,  // Variable becomes visible
    VAR_EXITED_SCOPE,   // Variable goes out of scope
    VAR_DIED,           // Scope destroyed

    // Functions
    FN_CALLED,          // Function call begins
    FN_RETURNED,        // Function returns
    FN_ERRORED,         // Unhandled error in function

    // Loops
    LOOP_STARTED,       // Loop begins
    LOOP_ITERATION,     // Each iteration
    LOOP_BROKE,         // break hit
    LOOP_COMPLETED,     // Natural end

    // Branches
    BRANCH_IF,          // if evaluated true
    BRANCH_ELIF,        // elif evaluated true
    BRANCH_ELSE,        // else taken
    BRANCH_SKIPPED,     // Branch not taken (false)

    // Scope
    SCOPE_ENTER,        // New scope created
    SCOPE_EXIT,         // Scope destroyed

    // Imports
    MODULE_LOADED,      // bring resolved
    MODULE_CACHED,      // bring hit cache

    // Errors
    ERROR_THROWN,        // Runtime error raised
    ERROR_CAUGHT,       // catch block activated
    ERROR_PROPAGATED,   // Error bubbled up

    // Objects
    OBJ_CREATED,        // Object constructed

    // Debug Decorators
    BREAKPOINT_HIT,     // @breakpoint snapshot taken
    WATCH_TRIGGERED,    // @watch expression became true
    CHECKPOINT_SAVED,   // @checkpoint saved state
};

struct TraceEntry {
    int64_t timestamp_ns;       // Nanosecond timestamp
    int sequence;               // Monotonic counter
    TraceEvent event;
    int line;                   // Source line
    std::string name;           // Variable/function/module name
    std::string type;           // XObject type string
    std::string value;          // toString() (truncated at 200 chars)
    std::string scope;          // "global", "fn:factorial", "class:Dog"
    std::string module;         // Module name (empty = main file)
    std::string detail;         // Extra context
    int depth;                  // Call/scope depth

    // Causation tracking — WHO caused this event
    std::string byWhom;         // "fn:factorial", "for:i", "user:assignment"
    int byWhomLine;             // Line of the causing statement
};
```

**`byWhom` / `byWhomLine` — Causation Tracking:**

Every trace event records WHO caused it:

- `VAR_BORN x` → `byWhom: "assignment"`, `byWhomLine: 3`
- `FN_CALLED factorial` → `byWhom: "call:main"`, `byWhomLine: 15`
- `VAR_CHANGED loss` → `byWhom: "fn:forward"`, `byWhomLine: 23` (the return that assigned)
- `LOOP_ITERATION i=3` → `byWhom: "for:i"`, `byWhomLine: 12`
- `ERROR_THROWN` → `byWhom: "fn:divide"`, `byWhomLine: 45`

This allows the IDE to show "x was changed at line 15 **by** the return value of forward()".

### 2.3 Snapshot — Value Copy, Not Pointer

**Critical design decision:** Snapshots store **value copies**, not `Environment*` pointers.

```cpp
struct Snapshot {
    std::string name;           // Breakpoint name
    int line;
    int sequence;
    std::string module;

    // Deep copy of all visible variables at snapshot time
    // scope_name → {var_name → (type_str, value_str)}
    std::map<std::string,
             std::map<std::string, std::pair<std::string,std::string>>
    > scopeVars;

    // Call stack at this moment (copy, not pointers)
    std::vector<std::string> callStack;
};
```

Why: The `Environment*` pointer becomes dangling the moment the scope exits. By the time the user views a snapshot in the IDE, those scopes are long gone. We serialize all variables to strings at snapshot time. The overhead is acceptable because snapshots are rare (only at `@breakpoint` lines).

### 2.4 Cross-Module Debugging

The **same `TraceCollector*`** is passed to all module interpreter instances:

```cpp
// In execBring() — when importing a module:
childInterpreter.setTraceCollector(trace_);    // share, don't copy
trace_->setCurrentModule(moduleName);          // tag events
// ... execute module ...
trace_->setCurrentModule(parentModuleName);    // restore
```

Timeline shows module boundaries:

```
│  5  │ FN_CALLED     │ physics->solve()    │ → module:physics │
│  12 │ VAR_BORN      │ force = 98.1        │ module:physics   │
│  15 │ FN_RETURNED   │ → 98.1              │ ← module:physics │
```

### 2.5 Interpreter Hook Points

All hooks zero-cost when disabled: `if (!trace_ || !trace_->enabled) return;`
Additionally gated by `TrackFilter`: `if (!filter_.shouldTrackVar(name)) return;`

| Hook                          | Event(s)             | Gate                                         |
| ----------------------------- | -------------------- | -------------------------------------------- |
| `Environment::set()` new      | `VAR_BORN`           | `shouldTrackVar(name)`                       |
| `Environment::set()` existing | `VAR_CHANGED`        | `shouldTrackVar(name)` + `trackMutations`    |
| `execBlock()` entry/exit      | `SCOPE_ENTER/EXIT`   | `trackScope`                                 |
| `callUserFn()` pre/post       | `FN_CALLED/RETURNED` | `shouldTrackFn(name)` + `trackCalls/Returns` |
| `execFor()`                   | `LOOP_*`             | `trackLoopFor`                               |
| `execWhile()`                 | `LOOP_*`             | `trackLoopWhile`                             |
| `execIf()`                    | `BRANCH_*`           | `trackConditions`                            |
| `execBring()`                 | `MODULE_*`           | `trackImports`                               |
| exec() each stmt              | dirty watches        | only if dirty flags set                      |

---

## Part 3 — Time Travel Debugging

### 3.1 Delta-Compressed Checkpoint Chain

```
checkpoint ── δ ── δ ── δ ── δ ── checkpoint ── δ ── δ ── δ ── checkpoint
   (full)    (diff)(diff)(diff)(diff)  (full)  (diff)(diff)(diff)  (full)
```

- **Checkpoint** = full snapshot of all scopes (every N steps, default N=100)
- **Delta** = only variables that changed since last entry
- **Reconstruct** state at step X: find nearest checkpoint before X, apply deltas forward

**Memory:**

- 10K steps: ~1MB (vs 50MB naïve). 50x savings.
- Disk backing when >10MB: older sections written to temp file

### 3.2 `@debug sample` + Time Travel Interaction

**Rule:** When sampling is active, time travel records **lightweight marker deltas** for skipped iterations (just sequence + line, no variable data). The timeline shows these as greyed-out gaps. Time travel can reconstruct state up to the boundary of a gap but not inside it.

```
@debug sample 100
for i in range(10000) :
    process(i)
;

Timeline:
  iter 0-49:   [full deltas — can time-travel freely]
  iter 50-9949: [marker: "9900 iterations skipped"]    ← greyed out
  iter 9950-9999: [full deltas — can time-travel freely]
```

If the user needs to debug the middle, they change `sample 100` to `sample 10000` (or remove sampling).

### 3.3 Data Structures

**File: `src/interpreter/time_travel.hpp`**

```cpp
struct FullCheckpoint {
    int sequence;
    int line;
    // scope_name → {var_name → (type, value)}
    std::map<std::string,
             std::map<std::string, std::pair<std::string,std::string>>
    > scopes;
    std::vector<std::string> callStack;
};

struct DeltaEntry {
    int sequence;
    int line;
    bool isGap = false;         // true for skipped sample iterations
    int gapSize = 0;            // how many steps were skipped
    std::vector<std::tuple<std::string,std::string,std::string>> changes;
    std::vector<std::string> born;
    std::vector<std::string> died;
};

class TimeTravelEngine {
public:
    void setCheckpointInterval(int n);
    void setMemoryLimit(size_t bytes);
    void recordStep(int sequence, int line, Environment* env);
    void recordGap(int fromSeq, int toSeq, int line);  // for sampling
    void saveCheckpoint(const std::string& name, int seq, int line, Environment* env);

    struct StateView {
        int sequence, line;
        std::string scope;
        std::map<std::string, std::pair<std::string,std::string>> variables;
        std::vector<std::string> callStack;
        bool inGap = false;     // true if inside a sampled gap
    };

    StateView getStateAt(int sequence) const;
    StateView stepForward() const;
    StateView stepBackward() const;
    void jumpTo(int sequence);
    void flushToDisk();

private:
    int interval_ = 100;
    size_t memLimit_ = 10 * 1024 * 1024;
    std::vector<FullCheckpoint> checkpoints_;
    std::vector<DeltaEntry> deltas_;
    std::string tempFilePath_;
};
```

### 3.4 IDE Time Travel Controls

```
┌────────────────────────────────────────────────────────────────┐
│ ◀◀  ◀  [═══════▓▓▓▓▓▓▓▓▓▓▓▓▓▓════════●] ▶  ▶▶              │
│ step 42/42     ↑ greyed = sample gap    ↑ drag                │
```

- ◀◀ = start, ◀ = back one, ▶ = forward one, ▶▶ = end
- Slider drag to any point. Grey sections = sampling gaps (non-navigable)
- Variables panel updates live as you scrub
- Editor highlights current "replayed" line

---

## Part 4 — IPC for Debug Control (Cross-Platform)

### 4.1 The Problem with PTY Escape Sequences

Using `\x1B[XSTEP:OVER\x07` over PTY is fragile — if program output or the shell writes simultaneously, commands can get corrupted or interleaved.

### 4.2 Solution: Cross-Platform IPC Channel

A dedicated IPC channel — Unix domain socket on Linux/macOS, named pipe on Windows:

```
Linux/macOS:  /tmp/xell-debug-{pid}.sock     (Unix domain socket)
Windows:      \\.\pipe\xell-debug-{pid}      (Named pipe)
```

**Protocol (same on all platforms — newline-delimited JSON):**

```
IDE → Interpreter:
    {"cmd": "step_over"}
    {"cmd": "step_into"}
    {"cmd": "step_out"}
    {"cmd": "continue"}
    {"cmd": "stop"}
    {"cmd": "add_breakpoint", "line": 25, "type": "snapshot"}
    {"cmd": "add_watch", "expr": "x > 100"}
    {"cmd": "jump_to", "sequence": 42}

Interpreter → IDE:
    {"state": "paused", "line": 25, "seq": 42, "vars": {...}, "callStack": [...]}
    {"state": "running"}
    {"state": "finished", "trace": "file:///tmp/xell-trace-123.json"}
```

**Benefits:**

- No corruption from interleaved output
- JSON messages self-delimiting (newline-separated)
- Bidirectional — IDE pushes commands, interpreter pushes state
- Program stdout/stderr goes through PTY as normal (clean separation)
- Auto-cleaned on process exit
- Fully cross-platform (Unix sockets + Windows named pipes)

**Implementation:**

```cpp
class DebugIPC {
public:
    void listen(int pid);       // Interpreter side: create channel
    void connect(int pid);      // IDE side: connect
    void send(const std::string& json);
    std::string recv();         // Blocks until message
    bool poll(int timeoutMs);   // Non-blocking check
    ~DebugIPC();                // Cleans up

private:
#ifdef _WIN32
    HANDLE pipeHandle_ = INVALID_HANDLE_VALUE;
    std::string pipePath_;      // \\.\pipe\xell-debug-{pid}
#else
    int serverFd_ = -1;
    int clientFd_ = -1;
    std::string socketPath_;    // /tmp/xell-debug-{pid}.sock
#endif
};
```

The IDE connects when it detects `--debug` mode. The interpreter creates the channel at startup if debug is enabled.

---

## Part 5 — Call Stack Live View

During pause mode:

```
┌────────────────────────────────────────────────┐
│ CALL STACK                          depth: 4   │
├────────────────────────────────────────────────┤
│ ▸ backward(loss=0.23)        ln 25  main.xel  │
│ ▸ compute_gradients(x)       ln 48  nn.xel    │
│ ▸ matrix_multiply(a, b)      ln 12  math.xel  │
│ ▸ dot_product(row, col)      ln 33  math.xel  │ ← current
├────────────────────────────────────────────────┤
│ Click frame to see its local variables         │
└────────────────────────────────────────────────┘
```

Click a frame → Variables tab shows that frame's locals (from the value-copied snapshot).

```cpp
struct CallFrame {
    std::string functionName;
    std::string moduleName;
    int line;
    std::string args;
    // Locals snapshot (value copy, not pointer)
    std::map<std::string, std::pair<std::string,std::string>> locals;
};
```

---

## Part 6 — Step-Through Execution

### 6.1 Step Commands

| Key     | Command   | Behavior                                       |
| ------- | --------- | ---------------------------------------------- |
| **F5**  | Continue  | Run to next breakpoint or end                  |
| **F10** | Step Over | Execute one statement, skip function internals |
| **F11** | Step Into | Pause at first line of called function         |
| **F12** | Step Out  | Continue until current function returns        |
| **Tab** | Resume    | Same as F5 (quick key)                         |
| **Esc** | Stop      | Abort execution                                |

### 6.2 Implementation

```cpp
void Interpreter::exec(StmtPtr stmt) {
    if (trace_ && trace_->stepping) {
        // Send state to IDE via debug socket
        debugSocket_.send(currentStateJSON(stmt->line));
        // Block until IDE sends next command
        auto cmd = debugSocket_.recvCommand();
        switch (cmd) {
            case CONTINUE:  trace_->stepping = false; break;
            case STEP_OVER: break;
            case STEP_INTO: trace_->stepInto = true; break;
            case STEP_OUT:  trace_->stepOutDepth = callStack_.size()-1; break;
            case STOP:      throw StopDebugSignal(); break;
        }
    }
    // ... normal dispatch
}
```

---

## Part 7 — Extended `--check-symbols` JSON

### 7.1 New Fields

| Field          | Type   | Description                                                    |
| -------------- | ------ | -------------------------------------------------------------- |
| `line_end`     | int    | End line (computed from body)                                  |
| `scope_type`   | string | `"global"`, `"function"`, `"class"`, `"module"`, `"for"`, etc. |
| `children`     | array  | Nested symbols (methods, exports)                              |
| `type`         | string | Inferred: `"int"`, `"str"`, `"list"`, `"fn"`, etc.             |
| `is_exported`  | bool   | Exported from module                                           |
| `is_immutable` | bool   | Declared with `be`                                             |

### 7.2 Computing `line_end`

No `endLine` on AST nodes. Compute from body:

- FnDef/ClassDef/ModuleDef/For/While/Loop: last stmt line in body + 1
- IfStmt: max across all branches + 1
- Variables: line_end = line

### 7.3 Type Inference from RHS

- IntLiteral → `"int"`, NumberLiteral → `"float"`, StringLiteral → `"str"`
- ListLiteral → `"list"`, MapLiteral → `"map"`, TupleLiteral → `"tuple"`
- BoolLiteral → `"bool"`, NoneLiteral → `"none"`, NewExpr → class name
- CallExpr → `"?"`, FnDef → `"fn"`, ClassDef → `"class"`

---

## Part 8 — Enhanced IDE Panels

### 8.1 Variables Tab (5 columns)

```
┌───────────┬──────────┬────────────┬──────────────┬────────────┐
│ Name ↕    │ Type     │ Value      │ Scope        │ Lines      │
├───────────┼──────────┼────────────┼──────────────┼────────────┤
│ x         │ int      │ 10         │ global       │ 3          │
│ name      │ str      │ "Rex"      │ global       │ 8          │
│ count     │ int      │ 42         │ fn: greet()  │ 12-18      │
│ Dog       │ class    │ <class>    │ global       │ 30-50      │
│ 🔄 loss   │ float    │ 0.023      │ global       │ 5          │
```

- Click name → jump to definition
- Click 🔄 → lifecycle view (debug mode only)
- Color by type: int=cyan, str=green, fn=yellow, class=magenta

### 8.2 Dashboard Tab (Right Panel)

```
┌──────────┬────────────────────────────┬─────────────┐
│ FileTree │     Editor                  │  Dashboard  │
│ (left)   │                            │  (right)    │
│          │                            │ ƒ factorial │
│          │                            │   ln 5-12   │
│          │                            │ ◆ Dog       │
│          │                            │   ▼ methods │
│          │                            │   ƒ __init__│
│          │                            │   ƒ bark()  │
│          │                            │ ◇ physics   │
│          ├────────────────────────────┤ ↻ for loop  │
│          │  Terminal / Output          │   ln 40-48  │
└──────────┴────────────────────────────┴─────────────┘
```

Icons: ƒ=function, ◆=class, ◇=module, ↻=loop, ◊=if, •=variable.
Resizable, collapsible, Ctrl+Shift+D toggle.

### 8.3 Timeline Tab

After debug run — bottom panel with time travel slider and event table.

### 8.4 Variable Lifecycle Sub-View

```
│ Lifecycle: 'loss'                                    │
│  3  │ VAR_BORN    │ float │ 0.5   │ by: assignment  │
│ 15  │ VAR_CHANGED │ float │ 0.23  │ by: fn:forward  │
│ 25  │ VAR_CHANGED │ float │ 0.01  │ by: fn:forward  │
│ Summary: Born ln 3 │ Changed 2x │ Final: 0.01       │
```

---

## Part 9 — Addressed Concerns

### C1: PTY Fragility

**Solution:** Unix domain socket (`/tmp/xell-debug-{pid}.sock`) for all debug control. PTY used only for program I/O. See Part 4.

### C2: Raw Environment Pointer in Snapshots

**Solution:** All snapshots store value copies (serialized `map<string, pair<string,string>>`), never `Environment*` pointers. See Part 2.3.

### C3: Sampling + Time Travel Gap

**Solution:** Skipped iterations recorded as gap markers in the delta chain. Time travel shows gaps as grey/non-navigable. State reconstruction works up to gap boundaries. See Part 3.2.

### C4: Watch Expression Cost

**Solution:** Dependency-aware dirty flags. Watches only re-evaluate when a variable they depend on is mutated. See Part 1.3.

### C5: Missing byWhom/byWhomLine

**Solution:** Added to TraceEntry struct. Every event records causation. See Part 2.2.

---

## Part 10 — Implementation Phases

| #   | Phase                                        | Priority    | Effort  | Depends     |
| --- | -------------------------------------------- | ----------- | ------- | ----------- |
| 1   | Extended `--check-symbols` JSON              | 🔴 Critical | Medium  | —           |
| 2   | Enhanced Variables Tab (5 cols)              | 🔴 Critical | Medium  | Phase 1     |
| 3   | Dashboard Tab (right panel)                  | 🟡 High     | Medium  | Phase 1     |
| 4   | TraceCollector + @debug + @track/@notrack    | 🔴 Critical | Large   | —           |
| 5   | @breakpoint + @watch decorators              | 🔴 Critical | Medium  | Phase 4     |
| 6   | Cross-module debug                           | 🟡 High     | Small   | Phase 4     |
| 7   | Debug socket IPC                             | 🟡 High     | Medium  | Phase 4     |
| 8   | Timeline Tab                                 | 🟡 High     | Medium  | Phase 4     |
| 9   | Step-through (F10/F11/F12)                   | 🟠 Medium   | Large   | Phase 4,7   |
| 10  | Time Travel engine                           | 🟠 Medium   | Large   | Phase 4     |
| 11  | Variable Lifecycle view                      | 🟡 High     | Small   | Phase 4,8   |
| 12  | Call Stack live view                         | 🟠 Medium   | Medium  | Phase 4,7,9 |
| 13  | Polish (conditional bp, timed pause, sample) | 🟢 Nice     | Ongoing | All         |

---

## File Changes Summary

| File                                       | Status  | Changes                                             |
| ------------------------------------------ | ------- | --------------------------------------------------- |
| `src/interpreter/trace_collector.hpp`      | **NEW** | TraceEntry, TraceEvent, TraceCollector, TrackFilter |
| `src/interpreter/time_travel.hpp`          | **NEW** | TimeTravelEngine, Checkpoint, Delta                 |
| `src/interpreter/debug_ipc.hpp`            | **NEW** | Cross-platform IPC (Unix socket / Win named pipe)   |
| `src/interpreter/interpreter.hpp`          | MODIFY  | `TraceCollector* trace_`, `callStack_`, debug hooks |
| `src/interpreter/interpreter.cpp`          | MODIFY  | Hook all exec methods, @decorator dispatch          |
| `src/interpreter/environment.hpp`          | MODIFY  | Trace callbacks in `set()`, `define()`              |
| `src/parser/ast.hpp`                       | MODIFY  | Decorator AST nodes                                 |
| `src/parser/parser.cpp`                    | MODIFY  | Parse all 6 decorators                              |
| `src/lexer/lexer.cpp`                      | MODIFY  | Recognize decorator tokens                          |
| `src/analyzer/symbol_collector.hpp`        | MODIFY  | lineEnd, scopeType, inferredType, children          |
| `src/main.cpp`                             | MODIFY  | `--debug` flag                                      |
| `xell-terminal/src/ui/repl_panel.hpp`      | MODIFY  | VarTabEntry 5 cols, lifecycle, timeline             |
| `xell-terminal/src/ui/layout_manager.hpp`  | MODIFY  | Dashboard, Ctrl+D, step keys, socket client         |
| `xell-terminal/src/ui/dashboard_panel.hpp` | **NEW** | Right-side dashboard                                |
| `xell-terminal/src/ui/timeline_panel.hpp`  | **NEW** | Timeline + time travel controls                     |
| `xell-terminal/src/ui/callstack_panel.hpp` | **NEW** | Call stack view                                     |
