# Garbage Collection

> **Previous:** [Ordered Hash Table](ordered-hash-table.md)  
> **Next:** [Gotchas & Pitfalls](gotchas.md)

---

Xell uses a **hybrid memory management** system: **reference counting** with a **cycle‑collecting garbage collector** as a backup. This combination ensures deterministic memory reclamation for acyclic structures while automatically detecting and collecting cyclic garbage that would otherwise leak.

Think of it as **two layers of safety** – reference counting handles the common case quickly, and the cycle collector cleans up the rare cases where cycles form.

---

## Quick Reference

| Concept | Description |
|---------|-------------|
| **Reference counting** | Every `XObject` tracks how many references point to it. When the count reaches zero, the object is freed immediately. |
| **Container types** | Types that can hold references to other `XObject`s (list, tuple, set, map, function, enum, instance, struct_def, module). |
| **GC tracking** | Container objects are tracked on an intrusive doubly‑linked list. |
| **gc_refs** | Temporary reference count used during cycle detection (copied from refCount). |
| **Trial‑deletion** | The cycle‑detection algorithm that finds unreachable cyclic garbage. |
| **Threshold** | Auto‑collection triggers after 700 container allocations since the last collection. |
| `gc_collect()` | Manual collection trigger (if exposed; currently C++‑only). |

---

## Reference Counting

Every `XObject` is a handle to a heap‑allocated `XData` control block that contains an **atomic reference count** (`refCount`). When you copy an `XObject`, the ref count is incremented. When an `XObject` goes out of scope (or is overwritten), the ref count is decremented.

When the ref count reaches **zero**, the object is **immediately freed**:

1. The payload is destroyed (calling destructors of contained objects, which may decrement more ref counts).
2. The `XData` control block is deleted.
3. The allocation counter is updated (debug only).

This is deterministic and efficient for acyclic object graphs.

---

## Why a Cycle Collector?

Reference counting cannot handle cycles:

```xell
class Node :
    next = none
;

let a = Node()
let b = Node()
a->next = b
b->next = a   # cycle!
```

- `a` references `b`, and `b` references `a`.
- When `a` and `b` go out of scope, their ref counts are still 1 (each points to the other).
- Neither is freed – **memory leak**.

The **cycle collector** detects and breaks such cycles, freeing the objects.

---

## GC Tracking

Only **container types** are tracked by the GC. These are types that can hold references to other `XObject`s.

### Tracked Types

| Type | Reason |
|------|--------|
| `list` | Holds references to elements. |
| `tuple` | Holds references to elements. |
| `set` | Holds references to keys. |
| `frozen_set` | Holds references to keys. |
| `map` | Holds references to keys and values. |
| `function` | Holds references to closure variables (captured environment). |
| `enum` | Holds references to member values. |
| `instance` | Holds references to field values. |
| `struct_def` | Holds references to default values, methods, static fields, properties. |
| `module` | Holds references to exported values and owned environment variables. |

### Untracked Types

| Type | Reason |
|------|--------|
| `int`, `float`, `complex`, `bool`, `string`, `bytes`, `none` | Cannot hold references to other objects. |
| `generator` | Not tracked (contains state but not references that form cycles in the same way). |

When a container object is allocated, it is automatically added to the GC's **tracking list** (`GCHeap::track()`). When it is freed (ref count reaches zero), it is removed from the tracking list (`GCHeap::untrack()`).

---

## The Cycle‑Detection Algorithm (Trial Deletion)

The GC runs a **trial‑deletion** algorithm, similar to CPython's cycle collector. It runs periodically (after 700 container allocations) or can be triggered manually.

### Phase 1: Copy Reference Counts

For every tracked object, the `gc_refs` field is set to the current `refCount` (atomic load).

```cpp
p->gc_refs = p->refCount.load();
```

This is a snapshot of the reference counts at the start of collection.

### Phase 2: Subtract Internal References

For each tracked object, the GC visits all **children** (objects it references). For each child that is also tracked, it decrements the child's `gc_refs`.

After this pass, `gc_refs` represents **only external references** – references from outside the tracked set (e.g., from the stack, global variables, or non‑container objects).

### Phase 3: Find Roots

Objects with `gc_refs > 0` have external references – they are **roots**. They are added to a work list, and their `gc_refs` is set to `-1` (sentinel for "reachable").

### Phase 4: Transitively Mark Reachable Objects

The GC iteratively visits all children of each root, marking them as reachable (`gc_refs = -1`) and adding them to the work list. This process continues until no more objects are found.

### Phase 5: Identify Unreachable Objects

After marking, any tracked object with `gc_refs != -1` is **unreachable** – it is part of a cyclic garbage island. These objects are collected.

### Phase 6: Free Unreachable Objects

For each unreachable object:

1. It is removed from the tracking list (`untrack()`).
2. The `gc_collecting` flag is set (to prevent double‑freeing during payload destruction).
3. The payload is destroyed (which may decrement ref counts of children – but `gc_collecting` prevents re‑entry).
4. The `XData` control block is deleted.

The GC then updates the global allocation counter (`notifyGCFreed()`).

---

## Collection Threshold

The GC runs automatically after **700 container allocations** since the last collection. This threshold can be changed via `GCHeap::setThreshold()` (C++ API; not exposed to Xell).

```cpp
static constexpr size_t DEFAULT_THRESHOLD = 700;
```

When the threshold is reached, `track()` calls `collect()` automatically.

---

## Manual Collection

The GC can be triggered manually via the C++ API (`GCHeap::instance().collect()`). There is currently **no user‑facing builtin** for manual collection in Xell, but it may be exposed in future versions as `gc_collect()`.

For now, collection is entirely automatic and transparent to the user.

---

## How Cycles Form

Common sources of cycles in Xell:

- **Self‑referential closures:** A closure captures a variable that refers back to the closure itself.
- **Graph structures:** Nodes referencing each other (linked lists with cycles, trees with parent pointers).
- **Singleton patterns:** A class instance stores a reference to itself.
- **Circular data:** Maps or lists containing references to themselves (if allowed, though Xell prevents direct self‑referential containers via the interpreter? Actually, Xell allows cycles as long as you construct them carefully, e.g., `let l = []; push(l, l)` creates a list that references itself.)

```xell
let l = []
push(l, l)   # list referencing itself – cycle
```

The GC detects and breaks this cycle.

---

## GC and Performance

The GC adds some overhead:

- **Tracking overhead:** Each container object has three extra pointer fields (`gc_next`, `gc_prev`, `gc_refs`) and a bool (`gc_tracked`). This is about 32 bytes per container.
- **Collection overhead:** The collection process visits all tracked objects and their children. This is O(tracked + references) and can pause execution briefly.
- **Threshold tuning:** The default threshold (700) balances frequency and overhead.

In practice, the GC is efficient and rarely noticeable in typical workloads.

---

## Edge Cases and Less Obvious Behaviour

### 1. `gc_collecting` Flag

During collection, unreachable objects have `gc_collecting = true`. This prevents `release()` from trying to free them again when their payload destructors decrement child ref counts.

### 2. `gc_tracked` Flag

An object is tracked only once. After being untracked (on free), it cannot be re‑tracked (it's dead).

### 3. Cycles Involving Non‑Container Types

Non‑container types (int, string, etc.) cannot participate in cycles because they cannot reference other objects. They are ignored by the collector.

### 4. Cycles Across Modules

Modules can reference each other, forming cycles. The GC handles module‑level cycles correctly.

### 5. Singleton Instances

`@singleton` classes store a reference to their singleton instance in the `XStructDef`. This creates a self‑reference cycle: the struct definition references the instance, and the instance references the struct definition (via its `structDef` field). The GC detects and breaks this cycle if the instance becomes otherwise unreachable.

### 6. Generators and GC

Generators are not tracked by the GC because they have their own thread‑based lifecycle. They are freed when the generator object's ref count drops to zero (the thread is joined in the destructor).

### 7. Recursive Data Structures

Deeply recursive structures (e.g., linked lists of length 1 million) are handled by reference counting for the acyclic parts. If a cycle forms, the GC will detect it.

---

## What NOT to Do

### 1. Relying on the GC for Deterministic Cleanup

The GC runs **asynchronously** (on allocation threshold). If you need deterministic cleanup (e.g., file handles, database connections), use RAII (`let … be`) or manual `close()`.

### 2. Expecting the GC to Run Immediately After a Cycle Forms

The GC runs only when the allocation threshold is reached. A cycle may persist for a while before collection.

### 3. Using Non‑Hashable Types in Sets/Maps and Expecting GC to Handle Them

The GC does not help with non‑hashable types. They will cause runtime errors regardless.

### 4. Creating Very Large Cycles and Expecting Collection to Be Instant

Large cycles may take a noticeable amount of time to collect. Avoid creating gigantic cyclic structures in performance‑critical code.

### 5. Assuming the GC Handles All Memory Issues

The GC only handles reference cycles. It does not prevent memory leaks from:
- Unbounded growth of containers (e.g., appending to a list indefinitely).
- Global variables holding references.
- Undisposed resources (files, sockets, etc.).

### 6. Relying on `gc_collect()` in User Code (Not Exposed)

There is currently no user‑facing `gc_collect()` builtin. Do not write code that depends on manual GC invocation.

### 7. Forgetting That `frozen_set` Is Tracked

`frozen_set` is a container (holds references to elements) and is tracked. However, its contents are immutable, so cycles involving frozen sets are rare but possible.

### 8. Expecting the GC to Free Objects with External References

If an object is referenced from the stack, a global variable, or a non‑tracked object, it will be considered a root and will not be collected.

---

## See Also

- [RAII](../core/raii.md) – deterministic resource management
- [Data Types](../core/data-types.md) – container types
- [Ordered Hash Table](ordered-hash-table.md) – data structure used by sets and maps

---

**Previous:** [Ordered Hash Table](ordered-hash-table.md)  
**Next:** [Gotchas & Pitfalls](gotchas.md)