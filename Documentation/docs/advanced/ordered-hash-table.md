# Ordered Hash Table

> **Previous:** [Hash System](hash-system.md)  
> **Next:** [Garbage Collection](garbage-collection.md)

---

The `OrderedHashTable` is the core data structure underlying Xell's `set`, `frozen_set`, and `map` types. It combines a **hash table** (for O(1) average lookup) with a **doubly‑linked list** (for insertion‑order iteration), while avoiding tombstones entirely through a **free‑list recycling** mechanism.

This design provides the performance and memory efficiency of a modern hash table (like Python 3.7+ dictionaries) with the predictable iteration order that Xell users expect.

---

## Quick Reference

| Concept | Description |
|---------|-------------|
| **Insertion order** | Iteration preserves the order in which keys were inserted. |
| **No tombstones** | Deleted entries are fully unlinked and recycled, not just marked as dead. |
| **Separate chaining** | Collisions are resolved with a linked list per bucket (via indices). |
| **Free‑list recycling** | Deleted nodes are reused for future insertions, avoiding allocation overhead. |
| **Load factor** | 75% (grows when `size * 4 > buckets * 3`). |
| **Pool allocation** | All nodes are stored in a contiguous vector (arena). |
| **O(1) average** | Insert, lookup, delete all O(1) expected. |
| **Memory overhead** | Per node: key, value, hash, 3 × int32 (order_prev, order_next, chain_next). |

---

## Design Overview

### The Problem

Traditional hash tables (e.g., `std::unordered_map`) use tombstones for deletion, leading to wasted space and degraded performance over time. They also do not preserve insertion order, making iteration unpredictable.

### Xell's Solution

`OrderedHashTable` solves both problems:

1. **Insertion order** is maintained by a **doubly‑linked list** that links nodes in the order they were inserted.
2. **No tombstones** – when a node is deleted, it is **removed from the bucket chain** and **recycled** via a free list, making its memory available for future insertions.

This approach keeps the table compact and fast, even after many deletions.

---

## Memory Layout

### Pool (Node Storage)

All nodes are stored in a **contiguous vector** (`pool_`). Each node contains:

| Field | Type | Description |
|-------|------|-------------|
| `key` | `Key` | The key (copied on insertion). |
| `value` | `Value` | The value (copied on insertion). |
| `hash` | `size_t` | Cached hash value of the key. |
| `order_prev` | `int32_t` | Index of the previous node in insertion order (`-1` for head). |
| `order_next` | `int32_t` | Index of the next node in insertion order (`-1` for tail). |
| `chain_next` | `int32_t` | Index of the next node in the bucket chain (`-1` for end). |

### Buckets

The `buckets_` vector stores **bucket heads** – indices into the pool (`-1` for empty bucket). Each bucket is a singly‑linked list of nodes (collision chain).

### Order List

The `order_head_` and `order_tail_` indices point to the first and last nodes in insertion order. The doubly‑linked list (`order_prev`/`order_next`) allows O(1) insertion at the tail and O(1) removal from any position.

### Free List

Deleted nodes are added to a **free list** (`free_head_`). Their `chain_next` field is reused to link them in the free list. When a new node is needed, the allocator pops from the free list first, avoiding allocation overhead.

---

## Operations

### `set(key, value)`

1. Compute hash `h = hasher_(key)`.
2. Locate bucket `b = h % buckets_.size()`.
3. Traverse the bucket chain to check for an existing key:
   - If found, update the value and return `false` (update).
4. If load factor exceeds 75%, **rehash** (grow buckets) and recompute bucket.
5. Allocate a node (from free list or `push_back` to pool).
6. Populate the node: key, value, hash.
7. Insert into bucket chain (at the head).
8. Append to the insertion‑order list (at the tail).
9. Increment `live_count_` and return `true` (insert).

**Complexity:** O(1) average, O(n) worst‑case (rehash).

### `get(key)`

1. Compute `h` and locate bucket `b`.
2. Traverse the bucket chain, comparing `hash` and `equal_`.
3. If found, return a pointer to the value.
4. Otherwise, return `nullptr`.

**Complexity:** O(1) average.

### `has(key)`

Wrapper around `get()` – returns `true` if the key exists.

### `remove(key)`

1. Compute `h` and locate bucket `b`.
2. Traverse the bucket chain, tracking the previous node.
3. If found:
   - Unlink from the bucket chain (update `chain_next` of previous node or bucket head).
   - Unlink from the insertion‑order list (update `order_prev`/`order_next` of neighbours).
   - **Recycle** the node: reset key/value, add to free list (`chain_next` = `free_head_`, `free_head_` = index).
   - Decrement `live_count_`.
   - Return `true`.
4. Otherwise, return `false`.

**Complexity:** O(1) average.

### Iteration

The `Iterator` class walks the `order_next` pointers starting from `order_head_`. This yields entries in strict insertion order. (New keys are appended at the tail; updates to existing keys do not move the node.)

### Rehashing

When the load factor exceeds 75%, the table grows the bucket array (doubles in size) and re‑inserts all live nodes into the new buckets. The order list is **unchanged** – only the bucket chains are rebuilt.

---

## Load Factor and Growth

The load factor is **75%** (`LF_NUM = 3`, `LF_DEN = 4`). Growth is triggered when:

```
live_count_ + 1 > buckets_.size() * 3 / 4
```

When triggered, the bucket count doubles (`rehash(buckets_.size() * 2)`).

This keeps the average bucket chain length short, ensuring O(1) performance.

---

## Memory Efficiency

### No Tombstones

Because deleted nodes are fully recycled, the table does not accumulate "dead" space. The `pool_` vector only grows when the free list is empty and the load factor allows. This is more memory‑efficient than tombstone‑based tables.

### Free List Reuse

The free list reuses deleted nodes for future insertions, avoiding repeated allocation/deallocation and keeping the pool compact.

---

## Iteration Order Guarantees

- **Insertion order** is preserved: keys are iterated in the order they were first added.
- **Updates** to existing keys do **not** change the order.
- **Deletions** remove nodes from the order list; subsequent insertions append to the end (order is not re‑used).
- **Rehashing** does **not** affect the order list.

This matches the behaviour of Python dictionaries (Python 3.7+) and ensures predictable, user‑friendly iteration.

---

## Thread Safety

`OrderedHashTable` is **not** thread‑safe by default. Concurrent modifications require external synchronisation (e.g., mutexes). The Xell interpreter uses it in a single‑threaded context; the `threading` module provides mutexes for user‑level concurrency.

---

## Complete Example: Using OrderedHashTable in Xell

```xell
# Maps are OrderedHashTable under the hood
let m = {}

# Insertion order is preserved
set(m, "first", 1)
set(m, "second", 2)
set(m, "third", 3)
set(m, "fourth", 4)

# Iteration order matches insertion
for [key, val] in entries(m):
    print(key + " = " + str(val))
;
```

**Output:**
```
first = 1
second = 2
third = 3
fourth = 4
```

```xell
# Deleting and re‑inserting appends to the end
remove(m, "second")
set(m, "second", 22)   # re‑inserted at the end

for [key, val] in entries(m):
    print(key + " = " + str(val))
;
```

**Output:**
```
first = 1
third = 3
fourth = 4
second = 22
```

The order is now: first, third, fourth, second – the re‑inserted "second" appears at the end.

---

## Performance Characteristics

| Operation | Average | Worst‑case |
|-----------|---------|------------|
| `set` (insert) | O(1) | O(n) (rehash) |
| `set` (update) | O(1) | O(n) |
| `get` | O(1) | O(n) |
| `has` | O(1) | O(n) |
| `remove` | O(1) | O(n) |
| Iteration | O(n) | O(n) |

The worst‑case O(n) occurs when many keys hash to the same bucket (collision) or during rehashing. In practice, with good hash distribution, performance is effectively O(1).

---

## Edge Cases and Less Obvious Behaviour

### 1. Re‑insertion After Deletion

When a key is deleted and later re‑inserted, the new node is placed at the **end** of the insertion‑order list. The old position is not reused, preserving the "append‑only" nature of the order.

### 2. Node Reuse in the Free List

When a node is recycled, its `key` and `value` are reset (destructed). The free list uses `chain_next` to link nodes, so the order and bucket chains are cleared. This ensures no stale data remains.

### 3. Load Factor and Performance

The 75% load factor is a good balance between memory and performance. If you expect many insertions, the table will grow gradually, avoiding frequent rehashes.

### 4. Hash Collisions and Bucket Chains

Collisions are resolved by chaining. In the worst case (all keys hash to the same bucket), performance degrades to O(n). This is mitigated by the hash algorithm (FNV‑1a, Murmur3, etc.) which provides excellent distribution.

### 5. Memory Overhead of `order_prev` and `order_next`

The doubly‑linked list adds two `int32_t` fields per node (8 bytes). This is a trade‑off for O(1) deletion and iteration order. For large tables, the overhead is acceptable (similar to Python dicts).

---

## What NOT to Do

### 1. Relying on Iteration Order After Deletion

If you delete keys and then iterate, the remaining keys retain their original insertion order (skipping deleted positions). This is predictable and intentional.

### 2. Mutating a Table While Iterating

Modifying the table (insert/delete) while iterating over it can lead to undefined behaviour or incorrect iteration. If you need to mutate, collect keys or use a snapshot.

### 3. Using a Non‑Hashable Type as a Key

`OrderedHashTable` relies on the `Hash` functor. In Xell, this means keys must be hashable. Using a mutable type (list, set, map) will cause a `HashError` at runtime.

### 4. Assuming O(1) for Every Operation

While average‑case is O(1), worst‑case can be O(n) if many collisions occur. Avoid using poor hash functions or adversarial keys.

### 5. Forgetting That Rehashing Is Expensive

Rehashing is O(n) and can cause a brief pause. The table grows only when needed; frequent rehashes can be a performance concern. The 75% load factor keeps rehash frequency low.

### 6. Using `OrderedHashTable` Directly in User Code

`OrderedHashTable` is an internal implementation detail. Xell users should use `set`, `frozen_set`, and `map` – not the underlying C++ data structure.

---

## See Also

- [Hash System](hash-system.md) – hash algorithms and `__hash__`
- [Data Types](../core/data-types.md) – `set`, `frozen_set`, `map`
- [Garbage Collection](garbage-collection.md) – how the GC handles hash tables

---

**Previous:** [Hash System](hash-system.md)  
**Next:** [Garbage Collection](garbage-collection.md)