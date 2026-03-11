# Xell Debug System — Manual Testing Guide

> Step-by-step instructions to manually verify every debug feature in the Xell Terminal IDE.
> Follow each section in order. Each section tests a specific feature.

---

## Prerequisites

1. Build the interpreter and terminal:
   ```bash
   make -C /home/DATA/CODE/code/Xell/build -j$(nproc)
   make -C /home/DATA/CODE/code/Xell/xell-terminal/build -j$(nproc)
   ```
2. Install:
   ```bash
   cd /home/DATA/CODE/code/Xell && ./install.sh --system --clean
   ```
3. Launch the IDE terminal:
   ```bash
   xell-terminal
   ```

---

## Section 1: F5 — Launch Debug Session

**Goal:** Verify F5 starts a debug session and pauses on the first line.

### Test File

Create and open `debug_test1.xel`:

```xell
x = 10
y = 20
z = x + y
print(z)
```

### Steps

1. **Open** the file `debug_test1.xel` in the editor
2. **Press F5**
3. **Verify:** Status bar shows `🐛 Debug session started`
4. **Verify:** The bottom panel switches to the TIMELINE tab
5. **Verify:** Line 1 (`x = 10`) is highlighted with a yellow background
6. **Verify:** A yellow `▶` arrow appears in the gutter next to line 1
7. **Verify:** The TIMELINE tab shows `[session] Debug started: ...`

**Expected:** Execution pauses on the very first line before running it.

---

## Section 2: F10 — Step Over (Simple Code)

**Goal:** Verify F10 advances one line at a time.

### Test File

Same file as Section 1 (`debug_test1.xel`).

### Steps (continue from Section 1, or start fresh with F5)

1. You should be paused at line 1
2. **Press F10** (Step Over)
3. **Verify:** Yellow arrow moves to line 2 (`y = 20`)
4. **Verify:** Status bar shows `⏸ Paused at line 2`
5. **Press F10** again
6. **Verify:** Arrow moves to line 3 (`z = x + y`)
7. **Press F10** again
8. **Verify:** Arrow moves to line 4 (`print(z)`)
9. **Press F10** one more time
10. **Verify:** Debug session ends, status bar shows `✓ Debug session finished`
11. **Verify:** Yellow arrow disappears from gutter

**Expected:** Each F10 press advances exactly one line. After the last line, the session finishes.

---

## Section 3: F10 — Step Over Does NOT Enter Functions

**Goal:** Verify F10 skips over function bodies.

### Test File

Create and open `debug_test2.xel`:

```xell
fn add(a, b) :
    result = a + b
    give result
;

x = add(3, 4)
print(x)
```

### Steps

1. **Press F5** to start debug
2. **Verify:** Paused on line 1 (`fn add(a, b) :`) — this is the function declaration
3. **Press F10** — moves to line 6 (`x = add(3, 4)`)
   - Note: function declarations are single statements; F10 skips the body
4. **Press F10** again
5. **Verify:** Arrow moves to line 7 (`print(x)`) — NOT into the function body (line 2)
6. **Press F10** to finish

**Expected:** F10 never enters line 2 or 3 (function body). It stays at the top-level.

---

## Section 4: F11 — Step Into Functions

**Goal:** Verify F11 enters function bodies.

### Test File

Same file as Section 3 (`debug_test2.xel`).

### Steps

1. **Press F5** to start debug
2. **Press F10** until paused on line 6 (`x = add(3, 4)`)
3. **Press F11** (Step Into)
4. **Verify:** Arrow jumps to line 2 (`result = a + b`) — inside the function!
5. **Verify:** CALLSTACK tab shows an entry for `add`
6. **Press F11** again
7. **Verify:** Arrow moves to line 3 (`give result`)
8. **Press F11** again
9. **Verify:** Arrow returns to line 7 (`print(x)`) — back at call site

**Expected:** F11 descends into the function body. The call stack grows by 1 entry.

---

## Section 5: Shift+F11 — Step Out

**Goal:** Verify Shift+F11 exits the current function.

### Test File

Create and open `debug_test3.xel`:

```xell
fn outer(n) :
    fn inner(x) :
        give x * 2
    ;
    result = inner(n)
    give result + 1
;

answer = outer(5)
print(answer)
```

### Steps

1. **Press F5** to start debug
2. **Press F10** until paused on line 9 (`answer = outer(5)`)
3. **Press F11** — enters `outer()`, paused at line 5 (`result = inner(n)`)
4. **Press F11** — enters `inner()`, paused at line 3 (`give x * 2`)
5. **Verify:** CALLSTACK tab shows entries for `outer` and `inner`
6. **Press Shift+F11** (Step Out)
7. **Verify:** Arrow jumps back to line 6 (`give result + 1`) — back in `outer`
8. **Verify:** CALLSTACK tab no longer shows `inner`
9. **Press Shift+F11** again
10. **Verify:** Arrow jumps back to line 10 (`print(answer)`) — back at top level

**Expected:** Each Shift+F11 pops one level off the call stack.

---

## Section 6: F9 — Toggle Breakpoint (Gutter)

**Goal:** Verify F9 adds/removes a breakpoint at the cursor line.

### Test File

Same as Section 1 (`debug_test1.xel`).

### Steps (NO debug session running)

1. **Place cursor** on line 3 (`z = x + y`)
2. **Press F9**
3. **Verify:** A purple `●` dot appears in the gutter next to line 3
4. **Verify:** Status bar shows `Breakpoint toggled at line 3`
5. **Press F9** again (same line)
6. **Verify:** The purple dot disappears — breakpoint removed
7. **Press F9** again to re-add it

**Expected:** F9 toggles a purple circle in the gutter. Purple = "pause" type breakpoint.

---

## Section 7: F9 — Gutter Click Breakpoint

**Goal:** Verify clicking the gutter toggles breakpoints.

### Steps

1. **Click** in the gutter area (leftmost column) next to line 2
2. **Verify:** Purple `●` appears on line 2
3. **Click** the gutter next to line 2 again
4. **Verify:** Purple `●` disappears

**Expected:** Gutter click toggles breakpoints just like F9.

---

## Section 8: Continue (F5) — Runs to Breakpoint

**Goal:** Verify F5 (Continue) runs until the next breakpoint.

### Test File

```xell
x = 1
y = 2
z = x + y
total = z * 10
print(total)
```

### Steps

1. **Place cursor on line 4**, press **F9** to set breakpoint (purple dot on line 4)
2. **Press F5** to start debug session
3. **Verify:** Paused at line 1 (first statement)
4. **Press F5** (Continue)
5. **Verify:** Execution runs and pauses at line 4 (`total = z * 10`)
6. **Verify:** The yellow `▶` arrow is on line 4 (at the breakpoint)
7. **Press F5** again (Continue)
8. **Verify:** Execution runs to completion (no more breakpoints)
9. **Verify:** Status bar shows `✓ Debug session finished`

**Expected:** Continue (F5) skips straight to the breakpoint line.

---

## Section 9: Multiple Breakpoints

**Goal:** Verify multiple breakpoints are hit in order.

### Test File

Same 5-line file as Section 8.

### Steps

1. Set breakpoints on **line 2** and **line 4** using F9
2. **Press F5** to start → paused at line 1
3. **Press F5** (Continue) → paused at line 2
4. **Press F5** (Continue) → paused at line 4
5. **Press F5** (Continue) → session finished

**Expected:** Each breakpoint is hit exactly once, in line order.

---

## Section 10: Add Breakpoint While Paused

**Goal:** Verify you can add a breakpoint during a debug session.

### Test File

Same 5-line file.

### Steps

1. **Press F5** to start → paused at line 1
2. **Place cursor on line 4**, press **F9** to add breakpoint (while paused)
3. **Verify:** Purple dot appears on line 4
4. **Press F5** (Continue)
5. **Verify:** Paused at line 4 — the newly added breakpoint works!
6. **Press F5** → session finishes

**Expected:** Breakpoints added during a paused session are immediately effective.

---

## Section 11: Remove Breakpoint While Paused

**Goal:** Verify removing a breakpoint during a session prevents it from hitting.

### Steps

1. Set breakpoints on lines 2, 3, 4
2. **Press F5** → paused at line 1
3. **Press F9** on line 3 to remove its breakpoint (while paused)
4. **Press F5** → paused at line 2
5. **Press F5** → paused at line 4 (skipped line 3!)
6. **Press F5** → finished

**Expected:** Removed breakpoints are no longer hit.

---

## Section 12: F12 — Stop Debug Session

**Goal:** Verify F12 terminates the debug session.

### Test File

```xell
x = 1
y = 2
z = 3
w = 4
print(w)
```

### Steps

1. **Press F5** → paused at line 1
2. **Press F10** → paused at line 2
3. **Press F12** (Stop)
4. **Verify:** Status bar shows `⏹ Debug Stopped`
5. **Verify:** Yellow arrow disappears
6. **Verify:** The program does NOT continue executing

**Expected:** F12 immediately kills the debug session.

---

## Section 13: Variable Inspection

**Goal:** Verify the VARIABLES tab shows current variable values.

### Test File

```xell
x = 10
y = "hello"
z = x + 5
print(z)
```

### Steps

1. **Press F5** → paused at line 1
2. Switch to **VARIABLES** tab in the bottom panel
3. **Press F10** → paused at line 2
4. **Verify:** VARIABLES shows `x = 10` (type: int)
5. **Press F10** → paused at line 3
6. **Verify:** VARIABLES shows `x = 10`, `y = "hello"` (type: string)
7. **Press F10** → paused at line 4
8. **Verify:** VARIABLES shows `x`, `y`, and `z = 15`

**Expected:** Variables appear as they are assigned, with correct types and values.

---

## Section 14: Call Stack Display

**Goal:** Verify the CALLSTACK tab shows the current call chain.

### Test File

```xell
fn inner(n) :
    give n * 2
;
fn outer(n) :
    val = inner(n)
    give val + 1
;
answer = outer(5)
print(answer)
```

### Steps

1. **Press F5** → paused at line 1
2. Switch to **CALLSTACK** tab
3. **Verify:** Call stack is empty (top level)
4. **Press F10** until at line 8 (`answer = outer(5)`)
5. **Press F11** (Step Into) → enters `outer`, paused at line 5
6. **Verify:** CALLSTACK shows `▶ outer`
7. **Press F11** → enters `inner`, paused at line 2
8. **Verify:** CALLSTACK shows `▶ outer` and `inner` (or `outer → inner`)
9. **Press Shift+F11** → step out of `inner`
10. **Verify:** CALLSTACK shows only `outer`
11. **Press Shift+F11** → step out of `outer`
12. **Verify:** CALLSTACK is empty again

**Expected:** Call stack grows on F11, shrinks on Shift+F11.

---

## Section 15: Timeline Events

**Goal:** Verify the TIMELINE tab logs debug events.

### Test File

Same as Section 14.

### Steps

1. **Press F5** → switch to **TIMELINE** tab
2. **Verify:** `[session] Debug started: ...` appears
3. **Press F10** a few times
4. **Verify:** Each step adds a `[line N] paused (seq:X depth:Y)` entry
5. **Press F5** (Continue) → session finishes
6. **Verify:** `[session] Debug session ended` appears

**Expected:** Timeline shows a log of every pause/step event.

---

## Section 16: @debug on — Decorator-Based Tracing

**Goal:** Verify `@debug on` enables the trace system.

### Test File

```xell
@debug on
x = 10
y = 20
z = x + y
@debug off
w = 100
print(w)
```

### Steps

1. **Select lines 1-7**, press **Ctrl+Shift+D** (Debug Run)
2. Switch to **LIFECYCLE** tab
3. **Verify:** Variables `x`, `y`, `z` have trace events (born, mutated)
4. **Verify:** Variable `w` does NOT appear in the lifecycle (it was assigned after `@debug off`)

**Expected:** Only code between `@debug on` and `@debug off` is traced.

---

## Section 17: @breakpoint — Snapshot

**Goal:** Verify `@breakpoint("name")` captures a variable snapshot.

### Test File

```xell
@debug on
x = 10
@breakpoint("check x")
x = 99
@breakpoint("check x again")
print(x)
```

### Steps

1. **Select all**, press **Ctrl+Shift+D** (Debug Run)
2. Switch to **LIFECYCLE** tab
3. **Verify:** You see snapshot events labeled "check x" and "check x again"
4. **Verify:** "check x" shows `x=10`, "check x again" shows `x=99`

**Expected:** Named snapshots capture the state at that point.

---

## Section 18: @watch — Conditional Alert

**Goal:** Verify `@watch("expr")` triggers when the condition becomes true.

### Test File

```xell
@debug on
@watch("x > 5")
x = 1
x = 3
x = 6
x = 10
```

### Steps

1. **Select all**, press **Ctrl+Shift+D**
2. Switch to **LIFECYCLE** tab
3. **Verify:** A `WATCH_TRIGGERED` event appears when `x` first exceeds 5 (at `x = 6`)
4. **Verify:** The event shows the triggering value

**Expected:** Watch fires at `x = 6` (first time `x > 5` is true).

---

## Section 19: @checkpoint — State Snapshots

**Goal:** Verify `@checkpoint("name")` saves full state.

### Test File

```xell
@debug on
data = [1, 2, 3]
@checkpoint("before")
data = [3, 2, 1]
@checkpoint("after")
print(data)
```

### Steps

1. **Select all**, press **Ctrl+Shift+D**
2. Switch to **LIFECYCLE** tab
3. **Verify:** You see checkpoint events "before" and "after"
4. **Verify:** "before" captures `data = [1, 2, 3]`
5. **Verify:** "after" captures `data = [3, 2, 1]`

**Expected:** Each checkpoint saves a complete snapshot with a label.

---

## Section 20: @track / @notrack — Selective Tracing

**Goal:** Verify `@track` and `@notrack` filter what is traced.

### Test File

```xell
@debug on
@notrack var(i)
total = 0
for i in [1, 2, 3, 4, 5] :
    total = total + i
;
print(total)
```

### Steps

1. **Select all**, press **Ctrl+Shift+D**
2. Switch to **LIFECYCLE** tab
3. **Verify:** `total` appears in the lifecycle with mutations
4. **Verify:** `i` does NOT appear (it was excluded by `@notrack`)

**Expected:** `@notrack var(i)` prevents `i` from appearing in trace output.

---

## Section 21: @log — Inline Logging

**Goal:** Verify `@log` prints messages to output.

### Test File

```xell
x = 42
@log "program started"
@log "x is {x}"
@log when x > 100 "x is big"
@log when x < 100 "x is small"
print("done")
```

### Steps

1. **Select all**, press **Ctrl+Enter** (Run in REPL)
2. Switch to **OUTPUT** tab
3. **Verify:** Output includes `[LOG] program started`
4. **Verify:** Output includes `[LOG] x is 42`
5. **Verify:** Output includes `[LOG] x is small` (not "x is big")

**Expected:** `@log` prints messages; conditional `@log when` only prints if condition is true.

---

## Section 22: @profile fn — Function Profiling

**Goal:** Verify `@profile fn name` measures execution time.

### Test File

```xell
@debug on
fn slow(n) :
    total = 0
    for i in range(n) :
        total = total + i
    ;
    give total
;
@profile fn slow
result = slow(1000)
print(result)
```

### Steps

1. **Select all**, press **Ctrl+Shift+D**
2. Switch to **LIFECYCLE** tab
3. **Verify:** A profiling event for `slow` appears showing execution time

**Expected:** Profile data shows call count and timing.

---

## Section 23: Breakpoint Colors

**Goal:** Verify breakpoint dots use correct colors.

### Steps

1. Open any file
2. **Press F9** on a line → **purple** `●` (pause breakpoint)
3. Visually confirm the color is purple/violet, not red

**Expected:** Pause breakpoints are purple `{160, 80, 255}`, not red.

---

## Section 24: Debug Line Indicator

**Goal:** Verify the paused line has visual indicators.

### Steps (during any debug session)

1. Start a debug session (F5)
2. **Verify:** The paused line has:
   - A **yellow `▶`** arrow in the gutter (column 0)
   - A **yellow-tinted background** on the entire line
   - **Yellow line numbers** for the paused line

**Expected:** The current debug line is clearly visible with yellow highlights.

---

## Section 25: Mixed Decorators + F5 Debug

**Goal:** Verify decorators work alongside F5 live debugging.

### Test File

```xell
@debug on
@breakpoint("snap1")
x = 10
@breakpoint("snap2")
y = 20
z = x + y
print(z)
```

### Steps

1. **Press F5** to start live debug
2. **Press F5** (Continue) to run to completion
3. **Verify:** Session completes without crash
4. **Verify:** Snapshot breakpoints were recorded (visible in TIMELINE)

**Expected:** Decorator-based breakpoints and F5 stepping coexist.

---

## Section 26: HELP Tab

**Goal:** Verify the HELP tab shows debug keybindings.

### Steps

1. Switch to **HELP** tab in the bottom panel
2. **Verify:** You see entries for:
   - F5 — Start debug / Continue execution
   - F9 — Toggle breakpoint
   - F10 — Step Over
   - F11 — Step Into
   - Shift+F11 — Step Out
   - F12 — Stop debug session

**Expected:** All debug keybindings are documented in the HELP tab.

---

## Quick Reference: Key Bindings

| Key              | Action                 |
| ---------------- | ---------------------- |
| **F5**           | Start debug / Continue |
| **F9**           | Toggle breakpoint      |
| **F10**          | Step Over              |
| **F11**          | Step Into              |
| **Shift+F11**    | Step Out               |
| **F12**          | Stop debug             |
| **Ctrl+Shift+D** | Debug Run (selection)  |
| **Ctrl+Enter**   | Run selection in REPL  |

---

## Automated Test Results

Before manual testing, verify automated tests pass:

```bash
# Unit tests (163 tests)
/home/DATA/CODE/code/Xell/build/debug_test

# IPC integration tests (28 tests)
/home/DATA/CODE/code/Xell/build/debug_ipc_test
```

Both should show `0 failed`.
