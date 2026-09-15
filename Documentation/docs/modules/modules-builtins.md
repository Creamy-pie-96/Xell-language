# Built‑in Modules

> **Previous:** [Module Resolution](modules-resolution.md)  
> **Next:** [Structs](../oop/oop-structs.md) *(or whatever follows in your reading order)*

---

Built‑in modules are pre‑compiled libraries that come with Xell. They provide functionality that is either too low‑level to implement in Xell itself, or that requires access to system resources (threads, network, filesystem, etc.). Some built‑in modules are always available (Tier 1), while others must be explicitly imported with `bring * from "module"` (Tier 2).

---

## Quick Reference

| Concept | What it does | Example |
|---------|--------------|---------|
| `bring * from "module"` | Loads a built‑in module’s functions into the current scope | `bring * from "threading"` |
| `Module->__exports__` | Lists all exported function names of a module | `threading->__exports__` |
| `Module->__name__` | The module’s name | `threading->__name__` |
| `Module->is_builtin` | Indicates whether a module is built‑in (read‑only) | `threading->is_builtin` → `true` |

---

## Tier 1 – Always Available

Tier 1 modules are **always loaded** – their functions are available in the global scope without any `bring`. They provide the core scripting primitives.

| Module | Description |
|--------|-------------|
| `io` | Input/output: `print`, `input`, `exit` |
| `math` | Mathematical constants and functions |
| `type` | Type introspection and conversion: `type`, `str`, `int`, `float`, `complex`, … |
| `collection` | Collection utilities: `len`, `push`, `pop`, `keys`, `values`, `range`, `set`, `has`, … |
| `util` | Utilities: `assert`, `format` |
| `os` | Filesystem, environment, and process: `mkdir`, `rm`, `read`, `write`, `env_get`, `run`, `pid`, … |
| `hash` | Hashing: `hash`, `hash_seed`, `is_hashable` |
| `string` | String operations: `trim`, `upper`, `lower`, `replace`, … |
| `list` | List operations: `shift`, `unshift`, `insert`, `sort`, `slice`, … |
| `map` | Map operations: `delete_key`, `get`, `merge`, `entries`, `from_entries` |
| `bytes` | Byte string operations: `bytes`, `encode`, `decode`, … |
| `generator` | Generator utilities: `next`, `is_exhausted`, `gen_collect` |
| `shell` | Shell‑style utilities: `error`, `clear`, `alias`, `which`, `history`, … |
| `casting` | Smart‑cast and container conversions: `Int`, `Float`, `List`, `~List`, … |

These functions are detailed in the [Built‑ins Reference](../core/builtins.md).

---

## Tier 2 – Require `bring`

Tier 2 modules must be loaded explicitly with `bring * from "module"`. They provide specialised functionality that may have external dependencies or be less frequently used.

| Module | Description |
|--------|-------------|
| `datetime` | Date, time, and sleep functions |
| `regex` | Regular expression matching and replacement |
| `fs` | Extended filesystem operations (symlinks, permissions, tree, find) |
| `textproc` | Text processing utilities (grep, sed, awk, head, tail, …) |
| `process` | Process management (ps, kill, pgrep, nohup, …) |
| `sysmon` | System monitoring (cpu_usage, memory info, lscpu, …) |
| `net` | Networking (HTTP, DNS, ping, traceroute, …) |
| `archive` | Archive and compression (zip, tar, gzip, …) |
| `json` | Data format parsing (JSON, CSV, TOML, YAML) |
| `threading` | Concurrency: threads and mutexes (see below) |

All functions in these modules are documented in the [Built‑ins Reference](../core/builtins.md) under their respective sections.

---

## The `threading` Module

The `threading` module provides concurrency primitives for parallel execution.

### Loading

```xell
bring * from "threading"
```

### Functions

| Function | Description |
|----------|-------------|
| `thread_spawn(fn, …args)` | Spawns a new thread executing `fn` with the given arguments. Returns a thread handle (integer). |
| `thread_join(handle)` | Waits for the thread to finish and returns its result. |
| `thread_done(handle)` | Returns `true` if the thread has finished (non‑blocking). |
| `thread_count()` | Returns the number of logical CPU cores. |
| `mutex_create()` | Creates a new mutex, returns a handle. |
| `mutex_lock(handle)` | Locks the mutex (blocks if already locked). |
| `mutex_unlock(handle)` | Unlocks the mutex. |
| `mutex_try_lock(handle)` | Tries to lock the mutex; returns `true` if successful, `false` otherwise. |

### Example: Parallel Computation

```xell
bring * from "threading"

fn square_worker(x):
    give x * x
;

let handles = []
for i in range(10):
    push(handles, thread_spawn(square_worker, i))
;

let results = []
for h in handles:
    push(results, thread_join(h))
;

print(results)   # [0, 1, 4, 9, 16, 25, 36, 49, 64, 81]
```

**Expected output:**
```
[0, 1, 4, 9, 16, 25, 36, 49, 64, 81]
```

### Example: Mutex‑Protected Counter

```xell
bring * from "threading"

let counter = 0
let m = mutex_create()

fn increment():
    mutex_lock(m)
    counter += 1
    mutex_unlock(m)
;

let handles = []
for i in range(100):
    push(handles, thread_spawn(increment))
;

for h in handles: thread_join(h) ;
print(counter)   # 100
```

### Important Notes

- Threads share the global environment; use mutexes to protect shared mutable state.
- `thread_join` blocks until the thread finishes; `thread_done` is non‑blocking.
- Threads are **OS threads** – they are scheduled by the operating system, not by Xell.
- The spawned thread runs in a **fresh interpreter instance** that shares the parent’s built‑ins but has its own global scope. Captured variables are copied by value (deep copy) at spawn time.

---

## Introspecting a Built‑in Module

Every module object (built‑in or user‑defined) exposes the following dunder fields:

- `__name__` – The module’s name.
- `__file__` – The file path (empty for built‑in modules).
- `__exports__` – A list of exported function names.
- `__submodules__` – A list of nested submodule names.
- `__version__` – A user‑set version string (empty for built‑ins).

```xell
bring * from "threading"
print(threading->__name__)      # "threading"
print(threading->__exports__)   # ["thread_spawn", "thread_join", "thread_done", ...]
```

### Finding a Function’s Module

If you have a function name and want to know which built‑in module provides it, you can use the `ModuleRegistry` (not exposed to user code) or simply refer to the [Built‑ins Reference](../core/builtins.md).

---

## Built‑in Modules vs. User Modules

| Aspect | Built‑in Module | User Module |
|--------|----------------|-------------|
| Source | Compiled into the interpreter | Source code (`.xel`/`.xell`) |
| Loading | Always available (Tier 1) or via `bring` (Tier 2) | Brought via `bring` from a file |
| `__file__` | `""` (empty string) | Absolute path to the source file |
| `__cached__` | `none` | Path to `.xelc` cache (future) |
| Modification | Cannot be changed at runtime | Can be changed by editing source and reloading (but caching applies) |
| Performance | Faster (no parsing/execution) | Slower on first load (parsing) |

---

## Common Pitfalls

### 1. Forgetting to `bring` a Tier 2 Module

```xell
thread_spawn(fn() -> print("hi"))   # ❌ error – thread_spawn is not defined
```

**Fix:** `bring * from "threading"` first.

### 2. Expecting Tier 1 Modules to Be Listed in `__exports__`

Tier 1 functions are injected directly into the global scope; they are not associated with a module object. The `__exports__` field only applies to actual module objects (user‑defined or Tier 2 built‑ins).

### 3. Mutating Shared State Without Synchronisation

```xell
let shared = 0
for i in range(100):
    thread_spawn(fn() -> shared += 1)
;
```

**Why it fails:** Data races – use mutexes.

### 4. Using `thread_join` on an Already Joined Thread

Calling `thread_join` again on a finished thread returns immediately (does not error), but it's good practice to check `thread_done` first.

### 5. Expecting `thread_spawn` to Copy by Value

Arguments are passed by **copy** (deep clone) to the new thread, not by reference. This avoids sharing mutable state inadvertently.

---

## See Also

- [Built‑ins Reference](../core/builtins.md) – full list of all built‑in functions
- [Defining Modules](modules-definition.md) – how to create your own modules
- [Importing Modules](modules-import.md) – `bring` syntax
- [Module Resolution](modules-resolution.md) – how modules are found

---

**Previous:** [Module Resolution](modules-resolution.md)  
**Next:** [Structs](../oop/oop-structs.md)