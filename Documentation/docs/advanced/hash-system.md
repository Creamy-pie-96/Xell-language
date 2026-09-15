# Hash System

> **Previous:** [F‑Strings](f-strings.md)  
> **Next:** [Ordered Hash Table](ordered-hash-table.md)

---

Xell's hash system is the foundation of its unordered collections – sets (`set`), frozen sets (`frozen_set`), and maps (`map`). It provides a pluggable architecture with multiple hash algorithms, support for seeded hashing, and a built‑in cryptographic hash (SHA‑256) for cache invalidation.

---

## Quick Reference

| Concept | What it does | Example |
|---------|--------------|---------|
| `hash(value)` | Returns a hash of any hashable value (FNV‑1a by default) | `hash("hello")` → `123456789` |
| `hash(value, algo)` | Hash with a specific algorithm | `hash("hello", "murmur3")` |
| `hash_seed(value, seed)` | Seeded hashing (FNV‑1a) | `hash_seed("hello", 42)` |
| `hash_seed(value, seed, algo)` | Seeded hashing with algorithm | `hash_seed("hello", 42, "djb2")` |
| `is_hashable(value)` | Checks if a value can be hashed | `is_hashable([1,2,3])` → `false` |
| `fnv1a` | Default algorithm – fast, good distribution | `hash("hello")` |
| `djb2` | Classic string hash | `hash("hello", "djb2")` |
| `murmur3` | 64‑bit finalizer, excellent avalanche | `hash("hello", "murmur3")` |
| `siphash` | Keyed PRF, DoS‑resistant | `hash("hello", "siphash")` |
| SHA‑256 | Cryptographic hash for cache invalidation | Used by `--make_module` |
| `__hash__(self)` | Magic method for custom hashing | `fn __hash__(self): give hashme(self->id) ;` |

---

## Hashable Types

A value is **hashable** if it is immutable and can be used as a key in a map or an element in a set.

### Hashable Types

| Type | Notes |
|------|-------|
| `none` | Always hashable (canonical hash) |
| `int` | Hashable (integer value) |
| `float` | Hashable (normalises `-0.0` to `+0.0`) |
| `complex` | Hashable (real and imaginary parts) |
| `bool` | Hashable (`true` → `1`, `false` → `0`) |
| `string` | Hashable (UTF‑8 bytes) |
| `bytes` | Hashable (raw bytes) |
| `tuple` | Hashable if **all** elements are hashable |
| `frozen_set` | Hashable if **all** elements are hashable |
| `frozen instance` | Hashable if it is **frozen** (`frozen = true`) and implements `__hash__` |

### Non‑Hashable Types

| Type | Reason |
|------|--------|
| `list` | Mutable |
| `set` | Mutable |
| `map` | Mutable |
| `function` | Mutable (closure state) |
| `instance` (mutable) | Not frozen, no `__hash__` |

---

## Hash Algorithms

Xell provides four hash algorithms for general‑purpose hashing, plus SHA‑256 for cryptographic use.

### FNV‑1a (Default)

**FNV‑1a** (Fowler–Noll–Vo) is the default hash algorithm. It is fast, has excellent avalanche properties, and is widely used.

```xell
hash("hello")              # uses FNV‑1a
hash("hello", "fnv1a")     # explicit
```

**Characteristics:**
- 64‑bit output.
- Excellent distribution.
- Very fast for short inputs.
- No cryptographic security.

### DJB2

**DJB2** is a simple, fast string hash by Daniel J. Bernstein.

```xell
hash("hello", "djb2")
```

**Characteristics:**
- 64‑bit output (on 64‑bit systems).
- Simple and fast.
- Good for strings.

### MurmurHash3

**MurmurHash3** provides excellent avalanche properties and is used in many hash tables.

```xell
hash("hello", "murmur3")
```

**Characteristics:**
- 64‑bit finaliser.
- Excellent avalanche.
- Fast for all inputs.
- Non‑cryptographic.

### SipHash‑2‑4

**SipHash** is a keyed pseudorandom function, designed to be DoS‑resistant for hash tables.

```xell
hash("hello", "siphash")
```

**Characteristics:**
- Keyed PRF (deterministic key per process).
- DoS‑resistant.
- Slightly slower than FNV‑1a.

### SHA‑256

**SHA‑256** is a cryptographic hash function used for cache invalidation and integrity checking.

```xell
# Not exposed as a user‑facing builtin, but used internally by --make_module
```

**Characteristics:**
- 256‑bit output.
- Cryptographic strength.
- Used for `.xell_meta` cache validation.
- Not intended for general‑purpose hashing (much slower).

---

## The `hash()` Builtin

### `hash(value)`

Returns the hash of a hashable value using FNV‑1a (default).

```xell
print(hash(42))          # 123456789
print(hash("hello"))     # 987654321
print(hash((1, 2, 3)))   # combination of hashes
```

### `hash(value, algorithm)`

Hashes with a specific algorithm.

```xell
print(hash("hello", "djb2"))
print(hash("hello", "murmur3"))
print(hash("hello", "siphash"))
```

**Supported algorithm names:** `"fnv1a"`, `"djb2"`, `"murmur3"`, `"siphash"`.

### `hash_seed(value, seed)`

Seeded hashing – mixes a seed into the initial state.

```xell
print(hash_seed("hello", 42))      # seeded FNV‑1a
print(hash_seed("hello", 42, "djb2"))   # seeded DJB2
```

Seeded hashing is useful for:
- Per‑session hash randomisation (DoS protection).
- Deterministic hashing for testing.
- Customising hash behaviour.

### `is_hashable(value)`

Returns `true` if the value can be used as a key in a map or element in a set.

```xell
print(is_hashable(42))              # true
print(is_hashable("hello"))         # true
print(is_hashable([1, 2, 3]))       # false (list is mutable)
print(is_hashable((1, 2, 3)))       # true (tuple of hashable elements)
print(is_hashable({1, 2, 3}))       # false (set is mutable)
```

---

## Hashing Complex Types

### Tuples

A tuple is hashable only if all its elements are hashable. The hash is a combination of the hashes of each element.

```xell
let t1 = (1, 2, 3)
let t2 = (1, "hello", true)

print(hash(t1))   # combines hashes of 1, 2, 3
print(hash(t2))   # combines hashes of 1, "hello", true
```

### Frozen Sets

A `frozen_set` is hashable if all its elements are hashable. The hash is **order‑independent** – it uses XOR of element hashes, so the order of elements does not affect the hash.

```xell
let s1 = <1, 2, 3>
let s2 = <3, 2, 1>

print(hash(s1))   # same as hash(s2) – order-independent
```

### Custom Classes

For a class instance to be hashable:

1. The instance must be **frozen** (`frozen = true`). Use `~` prefix or `@immutable`.
2. The class must implement `__hash__`.

```xell
@immutable
class Point :
    x = 0
    y = 0

    fn __hash__(self):
        give hashme(self->x) ^ hashme(self->y)
    ;
;

let p = ~Point(10, 20)
print(hash(p))   # calls __hash__
```

If a class does not implement `__hash__` but is frozen, it is still non‑hashable (no default hash).

---

## How Hashing Works Internally

### Hash Function Pointer

Xell's hash system uses a function pointer (`HashFn`) that takes raw bytes and returns a `size_t`.

```cpp
using HashFn = size_t (*)(const void *, size_t);
```

### Hashing an `XObject`

The `hashXObject()` function dispatches based on the object's type:

| Type | Hashing Behaviour |
|------|-------------------|
| `none` | Returns a constant hash for `none`. |
| `int` | Uses `hash_int()` → `murmur3_fmix64` on the raw integer. |
| `float` | Normalises `-0.0` to `+0.0`, hashes the double. |
| `complex` | Hashes real and imaginary parts, combines them. |
| `bool` | Hashes `1` or `0`. |
| `string` | Hashes the raw UTF‑8 bytes. |
| `tuple` | Hashes a seed, then hashes each element, combining. |
| `frozen_set` | XORs the hashes of all elements (order‑independent). |
| `bytes` | Hashes the raw byte data. |
| `instance` | Calls `__hash__` via the interpreter callback (must be frozen). |

### Hash Combining

For composite types, Xell uses `hash_combine()`:

```cpp
size_t hash_combine(size_t h1, size_t h2) {
    h1 ^= h2 + 0x9e3779b97f4a7c15ULL + (h1 << 12) + (h1 >> 4);
    return h1;
}
```

This is based on the `boost::hash_combine` algorithm, which provides good mixing.

---

## SHA‑256 for Cache Invalidation

Xell uses SHA‑256 to compute file hashes for module caching.

### Usage

- **`--make_module`** writes SHA‑256 hashes to `__xelcache__/*.hash`.
- **`--update`** compares the current hash with the stored hash to detect changes.

### Implementation

SHA‑256 is implemented in pure C++ (FIPS 180‑4) and returns a 64‑character lowercase hex string.

```cpp
std::string sha256(const void *data, size_t len);
std::string sha256_string(const std::string &s);
```

---

## Custom Hash Algorithms (User‑Defined)

You can define your own hash algorithm by implementing a function that takes `(const void *data, size_t len)` and returns a `size_t`.

```cpp
size_t my_hash(const void *data, size_t len) {
    // custom hashing logic
}
```

Then use it with `hash()` by name (if registered) or via `hashRaw()`.

---

## Complete Example: Using Hashable Types

```xell
# Using hashable types in maps and sets
let m = {}
set(m, "key", "value")
set(m, (1, 2, 3), "tuple key")
set(m, <1, 2, 3>, "frozen set key")

print(has(m, "key"))        # true
print(has(m, (1, 2, 3)))    # true
print(has(m, <1, 2, 3>))    # true

# Set of hashable elements
let s = {}
add(s, 42)
add(s, "hello")
add(s, (1, 2, 3))
add(s, <1, 2, 3>)

print(contains(s, 42))      # true
print(contains(s, [1,2,3])) # false (list not hashable)

# Using hash() directly
print(hash(42))
print(hash("hello", "murmur3"))
print(hash_seed("hello", 42, "djb2"))
```

**Output (hashes vary):**
```
true
true
true
true
true
false
123456789
987654321
876543210
```

---

## What NOT to Do

### 1. Using a Mutable Type as a Map Key

```xell
let m = {}
set(m, [1, 2, 3], "value")   # ❌ TypeError – list is not hashable
```

**Why it fails:** Lists are mutable – their hash would change if modified, breaking the map's invariants.

### 2. Using a Mutable Instance in a Set

```xell
class Point : x = 0 y = 0 ;
let p = Point(10, 20)
let s = {p}   # ❌ TypeError – instance is not hashable (not frozen)
```

**Fix:** Make the instance frozen (`~Point(10, 20)`) or add `@immutable`.

### 3. Forgetting to Implement `__hash__` for Frozen Classes

```xell
@immutable
class Bad :
    id = 0
    # ❌ no __hash__ – instances are NOT hashable
;

let b = ~Bad({ id: 42 })
hash(b)   # ❌ TypeError – no __hash__ method
```

**Fix:** Implement `__hash__` in the class.

### 4. Expecting Two Equivalent Frozen Sets to Have the Same Hash

```xell
let s1 = <1, 2, 3>
let s2 = <3, 2, 1>
print(hash(s1) == hash(s2))   # true
```

This works (order‑independent). However, be aware that XOR‑based hashing can have collisions for sets with different elements if the XORs cancel out.

### 5. Using `hash()` on a Very Large Tuple

```xell
hash((1, 2, 3, ..., 10000))   # ❌ potentially slow
```

**Why it fails:** Hashing a tuple hashes every element. Large tuples may be slow. Avoid using huge tuples as map keys.

### 6. Expecting `hash()` to Be Cryptographic

FNV‑1a, DJB2, MurmurHash3, and SipHash are **not** cryptographic. They are designed for hash tables, not security. Use SHA‑256 for cryptographic purposes.

### 7. Relying on Hash Stability Across Sessions

FNV‑1a and DJB2 are deterministic, so hashes are stable across sessions. However, SipHash uses a deterministic key (not random), but the key is constant across sessions in Xell.

---

## See Also

- [Ordered Hash Table](ordered-hash-table.md) – the data structure used for sets and maps
- [Data Types](../core/data-types.md) – `set`, `frozen_set`, `map`
- [Magic Methods](../oop/oop-magic-methods.md) – `__hash__`
- [Built‑ins Reference](../core/builtins.md) – `hash()`, `hash_seed()`, `is_hashable()`

---

**Previous:** [F‑Strings](f-strings.md)  
**Next:** [Ordered Hash Table](ordered-hash-table.md)