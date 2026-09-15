# Standard Library – Collections

> **Previous:** [Built‑in Modules](../modules/modules-builtins.md)  
> **Next:** [Higher‑Order Functions](stdlib-hof.md)

---

Xell provides a rich set of built‑in functions for working with collections – lists, sets, maps, and tuples. These functions are always available (Tier 1) and cover everything from basic manipulation (push, pop, shift) to searching, sorting, and set operations.

---

## Quick Reference

| Function | What it does | Example |
|----------|--------------|---------|
| `len(x)` | Returns length of collection | `len([1,2,3])` → `3` |
| `push(list, item)` | Appends to end | `push([1,2], 3)` → `[1,2,3]` |
| `pop(list)` | Removes and returns last element | `pop([1,2])` → `2` |
| `shift(list)` | Removes and returns first element | `shift([1,2])` → `1` |
| `unshift(list, item)` | Inserts at front | `unshift([1,2], 0)` → `[0,1,2]` |
| `insert(list, index, item)` | Inserts at position | `insert([1,3], 1, 2)` → `[1,2,3]` |
| `remove_val(list, val)` | Removes first occurrence | `remove_val([1,2,3], 2)` → `true` |
| `sort(list)` | Sorts ascending (in‑place) | `sort([3,1,2])` → `[1,2,3]` |
| `sort_desc(list)` | Sorts descending (in‑place) | `sort_desc([1,3,2])` → `[3,2,1]` |
| `reverse(str_or_list)` | Reverses in‑place | `reverse([1,2,3])` → `[3,2,1]` |
| `slice(list, start[, end])` | Returns a sub‑list | `slice([0,1,2,3], 1, 3)` → `[1,2]` |
| `flatten(list)` | Flattens one level | `flatten([1,[2,3],4])` → `[1,2,3,4]` |
| `unique(list)` | Removes duplicates (preserves order) | `unique([1,2,2,3])` → `[1,2,3]` |
| `first(list)` | Returns first element | `first([1,2,3])` → `1` |
| `last(list)` | Returns last element | `last([1,2,3])` → `3` |
| `zip(list1, list2)` | Zips two lists into pairs | `zip([1,2], ["a","b"])` → `[[1,"a"],[2,"b"]]` |
| `zip_longest(list1, list2[, fill])` | Zips with padding | `zip_longest([1,2], ["a"], "?")` → `[[1,"a"],[2,"?"]]` |
| `enumerate(list[, start])` | Returns `[index, value]` pairs | `enumerate(["a","b"], 1)` → `[[1,"a"],[2,"b"]]` |
| `sum(list)` | Sums numbers | `sum([1,2,3])` → `6` |
| `avg(list)` | Average (always float) | `avg([1,2,3])` → `2.0` |
| `min(list_or_a, b)` | Smallest value | `min([1,5,3])` → `1` |
| `max(list_or_a, b)` | Largest value | `max([1,5,3])` → `5` |
| `size(collection)` | Alias for `len` | `size([1,2,3])` → `3` |
| `join(list, sep)` | Joins strings with separator | `join(["a","b"], "-")` → `"a-b"` |
| `split(str, sep)` | Splits string into list | `split("a-b-c", "-")` → `["a","b","c"]` |
| `contains(container, elem)` | Checks membership | `contains([1,2,3], 2)` → `true` |
| `index_of(str_or_list, sub)` | Finds first index, or `-1` | `index_of([1,2,3], 2)` → `1` |
| `count(str_or_list, sub)` | Counts occurrences | `count([1,2,2,3], 2)` → `2` |
| `is_empty(str_or_list_or_map)` | Checks emptiness | `is_empty([])` → `true` |
| `range(start[, end[, step]])` | Returns a list of integers | `range(5)` → `[0,1,2,3,4]` |
| `keys(map)` | Returns list of keys | `keys({"a":1})` → `["a"]` |
| `values(map)` | Returns list of values | `values({"a":1})` → `[1]` |
| `entries(map)` | Returns `[key, value]` pairs | `entries({"a":1})` → `[["a",1]]` |
| `from_entries(list)` | Builds map from pairs | `from_entries([["a",1]])` → `{"a":1}` |
| `get(map, key[, default])` | Gets value or default | `get({"a":1}, "b", 0)` → `0` |
| `merge(map1, map2)` | Merges two maps | `merge({"a":1}, {"b":2})` → `{"a":1,"b":2}` |
| `delete_key(map, key)` | Removes a key, returns value | `delete_key({"a":1}, "a")` → `1` |
| `set(list_or_map, index_or_key, value)` | Sets by index (list) or key (map) | `set([1,2], 1, 99)` → `[1,99]` |
| `has(map_or_set, key)` | Checks if key exists | `has({"a":1}, "a")` → `true` |
| `add(set, elem)` | Adds element to set | `add({1,2}, 3)` → `{1,2,3}` |
| `remove(list_or_set_or_map, index_or_key)` | Removes from list, set, or map | `remove({1,2,3}, 2)` → `true` |
| `to_set(list_or_tuple_or_frozen)` | Converts to set | `to_set([1,2,2])` → `{1,2}` |
| `to_tuple(list_or_set_or_frozen)` | Converts to tuple | `to_tuple([1,2])` → `(1,2)` |
| `to_list(tuple_or_set_or_frozen)` | Converts to list | `to_list((1,2))` → `[1,2]` |
| `union_set(s1, s2)` | Set union | `union_set({1,2}, {2,3})` → `{1,2,3}` |
| `intersect(s1, s2)` | Set intersection | `intersect({1,2}, {2,3})` → `{2}` |
| `diff(s1, s2)` | Set difference | `diff({1,2}, {2,3})` → `{1}` |

---

## Lists

### `len(list)`

Returns the number of elements in a list.

```xell
let fruits = ["apple", "banana", "cherry"]
print(len(fruits))   # 3
```

**Output:**
```
3
```

### `push(list, item)`

Appends an item to the end of a list. Returns `none`.

```xell
let numbers = [1, 2, 3]
push(numbers, 4)
print(numbers)   # [1, 2, 3, 4]
```

**Output:**
```
[1, 2, 3, 4]
```

### `pop(list)`

Removes and returns the last element of a list. Throws `IndexError` if the list is empty.

```xell
let numbers = [1, 2, 3]
let last = pop(numbers)
print(last)      # 3
print(numbers)   # [1, 2]
```

**Output:**
```
3
[1, 2]
```

### `shift(list)`

Removes and returns the first element of a list. Throws `IndexError` if the list is empty.

```xell
let numbers = [1, 2, 3]
let first = shift(numbers)
print(first)     # 1
print(numbers)   # [2, 3]
```

**Output:**
```
1
[2, 3]
```

### `unshift(list, item)`

Inserts an item at the front of a list. Returns `none`.

```xell
let numbers = [2, 3]
unshift(numbers, 1)
print(numbers)   # [1, 2, 3]
```

**Output:**
```
[1, 2, 3]
```

### `insert(list, index, item)`

Inserts an item at a specific index. Negative indices count from the end. Returns `none`.

```xell
let numbers = [1, 2, 4]
insert(numbers, 2, 3)
print(numbers)   # [1, 2, 3, 4]

insert(numbers, 0, 0)
print(numbers)   # [0, 1, 2, 3, 4]
```

**Output:**
```
[1, 2, 3, 4]
[0, 1, 2, 3, 4]
```

### `remove_val(list, val)`

Removes the first occurrence of `val` from the list. Returns `true` if an element was removed, `false` otherwise.

```xell
let numbers = [1, 2, 3, 2]
let removed = remove_val(numbers, 2)
print(removed)   # true
print(numbers)   # [1, 3, 2]

removed = remove_val(numbers, 99)
print(removed)   # false
```

**Output:**
```
true
[1, 3, 2]
false
```

### `sort(list)`

Sorts the list in ascending order **in‑place**. Returns the list (for chaining). Works on numbers and strings.

```xell
let numbers = [3, 1, 4, 1, 5]
sort(numbers)
print(numbers)   # [1, 1, 3, 4, 5]

let words = ["banana", "apple", "cherry"]
sort(words)
print(words)     # ["apple", "banana", "cherry"]
```

**Output:**
```
[1, 1, 3, 4, 5]
["apple", "banana", "cherry"]
```

### `sort_desc(list)`

Sorts the list in descending order **in‑place**. Returns the list (for chaining).

```xell
let numbers = [3, 1, 4, 1, 5]
sort_desc(numbers)
print(numbers)   # [5, 4, 3, 1, 1]
```

**Output:**
```
[5, 4, 3, 1, 1]
```

### `reverse(str_or_list)`

Reverses the order of a list or string **in‑place**. Returns the list or string.

```xell
let numbers = [1, 2, 3, 4]
reverse(numbers)
print(numbers)   # [4, 3, 2, 1]

let text = "hello"
reverse(text)
print(text)      # "olleh"
```

**Output:**
```
[4, 3, 2, 1]
olleh
```

### `slice(list, start[, end])`

Returns a sub‑list from `start` (inclusive) to `end` (exclusive). Negative indices count from the end.

```xell
let numbers = [0, 1, 2, 3, 4, 5]
let sub = slice(numbers, 1, 4)
print(sub)       # [1, 2, 3]

let tail = slice(numbers, 2)
print(tail)      # [2, 3, 4, 5]

let last_two = slice(numbers, -2)
print(last_two)  # [4, 5]
```

**Output:**
```
[1, 2, 3]
[2, 3, 4, 5]
[4, 5]
```

### `flatten(list)`

Flattens a nested list by one level. Returns a new list.

```xell
let nested = [1, [2, 3], 4, [5, 6, 7]]
let flat = flatten(nested)
print(flat)      # [1, 2, 3, 4, 5, 6, 7]
```

**Output:**
```
[1, 2, 3, 4, 5, 6, 7]
```

### `unique(list)`

Removes duplicate elements from a list while preserving order. Returns a new list.

```xell
let numbers = [1, 2, 2, 3, 3, 3, 4]
let unique_nums = unique(numbers)
print(unique_nums)   # [1, 2, 3, 4]
```

**Output:**
```
[1, 2, 3, 4]
```

### `first(list)`

Returns the first element of a list. Throws `IndexError` if the list is empty.

```xell
let numbers = [10, 20, 30]
print(first(numbers))   # 10
```

**Output:**
```
10
```

### `last(list)`

Returns the last element of a list. Throws `IndexError` if the list is empty.

```xell
let numbers = [10, 20, 30]
print(last(numbers))   # 30
```

**Output:**
```
30
```

### `zip(list1, list2)`

Zips two lists together, producing a list of `[a, b]` pairs. The result length is the minimum of the two input lengths.

```xell
let names = ["Alice", "Bob", "Charlie"]
let ages = [30, 25, 35, 40]
let pairs = zip(names, ages)
print(pairs)   # [["Alice", 30], ["Bob", 25], ["Charlie", 35]]
```

**Output:**
```
[["Alice", 30], ["Bob", 25], ["Charlie", 35]]
```

### `zip_longest(list1, list2[, fill])`

Zips two lists, filling the shorter list with a default value. Returns a list of `[a, b]` pairs.

```xell
let names = ["Alice", "Bob"]
let ages = [30, 25, 35]
let pairs = zip_longest(names, ages, "unknown")
print(pairs)   # [["Alice", 30], ["Bob", 25], ["unknown", 35]]
```

**Output:**
```
[["Alice", 30], ["Bob", 25], ["unknown", 35]]
```

### `enumerate(list[, start])`

Returns a list of `[index, value]` pairs, starting from `start` (default 0).

```xell
let fruits = ["apple", "banana", "cherry"]
let enumerated = enumerate(fruits, 1)
print(enumerated)   # [[1, "apple"], [2, "banana"], [3, "cherry"]]
```

**Output:**
```
[[1, "apple"], [2, "banana"], [3, "cherry"]]
```

### `sum(list)`

Returns the sum of all numbers in a list. Works with ints, floats, and complexes.

```xell
let numbers = [1, 2, 3, 4]
print(sum(numbers))   # 10

let floats = [1.5, 2.5, 3.0]
print(sum(floats))    # 7.0

let complexes = [2 + 3i, 1 - 2i]
print(sum(complexes)) # (3+1i)
```

**Output:**
```
10
7.0
(3+1i)
```

### `avg(list)`

Returns the average of all numbers in a list. Always returns a `float`.

```xell
let numbers = [1, 2, 3, 4]
print(avg(numbers))   # 2.5

let ints = [10, 20, 30]
print(avg(ints))      # 20.0
```

**Output:**
```
2.5
20.0
```

### `min(list_or_a, b)` and `max(list_or_a, b)`

`min` returns the smallest value; `max` returns the largest. Works with a list or two arguments.

```xell
let numbers = [3, 1, 4, 1, 5]
print(min(numbers))   # 1
print(max(numbers))   # 5

print(min(10, 20))    # 10
print(max(10, 20))    # 20
```

**Output:**
```
1
5
10
20
```

### `size(collection)`

Alias for `len()`. Returns the length of a string, list, tuple, set, frozen set, or map.

```xell
let data = [1, 2, 3]
print(size(data))   # 3
```

**Output:**
```
3
```

---

## Strings (Collection‑Style)

### `join(list, sep)`

Joins a list of strings into a single string using a separator.

```xell
let words = ["apple", "banana", "cherry"]
let result = join(words, ", ")
print(result)   # "apple, banana, cherry"
```

**Output:**
```
apple, banana, cherry
```

### `split(str, sep)`

Splits a string into a list of substrings using a separator.

```xell
let text = "apple,banana,cherry"
let parts = split(text, ",")
print(parts)   # ["apple", "banana", "cherry"]
```

**Output:**
```
["apple", "banana", "cherry"]
```

### `contains(container, elem)`

Checks if an element is present in a string, list, tuple, set, or map (keys).

```xell
print(contains([1, 2, 3], 2))          # true
print(contains("hello", "lo"))         # true (substring)
print(contains({"a": 1}, "a"))         # true
```

**Output:**
```
true
true
true
```

### `index_of(str_or_list, sub_or_val)`

Returns the first index where `sub` occurs in a string or list. Returns `-1` if not found.

```xell
print(index_of([1, 2, 3, 2], 2))   # 1
print(index_of("hello", "l"))      # 2
print(index_of("hello", "x"))      # -1
```

**Output:**
```
1
2
-1
```

### `count(str_or_list, sub_or_val)`

Counts the number of occurrences of a substring in a string or value in a list.

```xell
print(count([1, 2, 2, 3, 2], 2))   # 3
print(count("hello", "l"))         # 2
print(count("hello", "lo"))        # 1
```

**Output:**
```
3
2
1
```

### `is_empty(str_or_list_or_map)`

Returns `true` if the collection is empty.

```xell
print(is_empty([]))        # true
print(is_empty([1, 2]))    # false
print(is_empty(""))        # true
print(is_empty({}))        # true
```

**Output:**
```
true
false
true
true
```

### `range([start], end, [step])`

Returns a list of integers from `start` (default 0) to `end` (exclusive), incrementing by `step` (default 1). Step can be negative for descending ranges.

```xell
print(range(5))            # [0, 1, 2, 3, 4]
print(range(1, 5))         # [1, 2, 3, 4]
print(range(0, 10, 2))     # [0, 2, 4, 6, 8]
print(range(5, 0, -1))     # [5, 4, 3, 2, 1]
```

**Output:**
```
[0, 1, 2, 3, 4]
[1, 2, 3, 4]
[0, 2, 4, 6, 8]
[5, 4, 3, 2, 1]
```

### `to_set(list_or_tuple_or_frozen)`

Converts a list, tuple, or frozen set to a mutable set. Deduplicates elements.

```xell
let list = [1, 2, 2, 3]
let s = to_set(list)
print(s)   # {1, 2, 3}
```

**Output:**
```
{1, 2, 3}
```

### `to_tuple(list_or_set_or_frozen)`

Converts a list, set, or frozen set to a tuple.

```xell
let list = [1, 2, 3]
let t = to_tuple(list)
print(t)   # (1, 2, 3)
```

**Output:**
```
(1, 2, 3)
```

### `to_list(tuple_or_set_or_frozen)`

Converts a tuple, set, or frozen set to a list.

```xell
let t = (1, 2, 3)
let list = to_list(t)
print(list)   # [1, 2, 3]
```

**Output:**
```
[1, 2, 3]
```

---

## Maps

### `keys(map)`

Returns a list of all keys in the map (insertion order preserved).

```xell
let m = { "x": 10, "y": 20, "z": 30 }
print(keys(m))   # ["x", "y", "z"]
```

**Output:**
```
["x", "y", "z"]
```

### `values(map)`

Returns a list of all values in the map (insertion order preserved).

```xell
let m = { "x": 10, "y": 20, "z": 30 }
print(values(m))   # [10, 20, 30]
```

**Output:**
```
[10, 20, 30]
```

### `entries(map)`

Returns a list of `[key, value]` pairs (insertion order preserved).

```xell
let m = { "x": 10, "y": 20 }
print(entries(m))   # [["x", 10], ["y", 20]]
```

**Output:**
```
[["x", 10], ["y", 20]]
```

### `from_entries(list)`

Builds a map from a list of `[key, value]` pairs.

```xell
let pairs = [["a", 1], ["b", 2], ["c", 3]]
let m = from_entries(pairs)
print(m)   # {"a": 1, "b": 2, "c": 3}
```

**Output:**
```
{"a": 1, "b": 2, "c": 3}
```

### `get(map, key[, default])`

Returns the value for `key`, or `default` (or `none`) if the key is not found.

```xell
let m = { "x": 10, "y": 20 }
print(get(m, "x"))       # 10
print(get(m, "z"))       # none
print(get(m, "z", 0))    # 0
```

**Output:**
```
10
none
0
```

### `merge(map1, map2)`

Returns a new map containing all keys from both maps. If the same key exists in both, `map2` wins.

```xell
let m1 = { "a": 1, "b": 2 }
let m2 = { "b": 3, "c": 4 }
let merged = merge(m1, m2)
print(merged)   # {"a": 1, "b": 3, "c": 4}
```

**Output:**
```
{"a": 1, "b": 3, "c": 4}
```

### `delete_key(map, key)`

Removes a key from the map and returns the value that was associated with it (or `none` if the key was not found).

```xell
let m = { "a": 1, "b": 2, "c": 3 }
let val = delete_key(m, "b")
print(val)   # 2
print(m)     # {"a": 1, "c": 3}
```

**Output:**
```
2
{"a": 1, "c": 3}
```

### `has(map_or_set, key)`

Checks if a key exists in a map or set.

```xell
let m = { "a": 1, "b": 2 }
print(has(m, "a"))   # true
print(has(m, "c"))   # false

let s = {1, 2, 3}
print(has(s, 2))     # true
```

**Output:**
```
true
false
true
```

### `set(list_or_map, index_or_key, value)`

- For lists: sets the element at `index_or_key` to `value`.
- For maps: sets the key `index_or_key` to `value`.

```xell
let l = [1, 2, 3]
set(l, 1, 99)
print(l)   # [1, 99, 3]

let m = { "a": 1 }
set(m, "b", 2)
print(m)   # {"a": 1, "b": 2}
```

**Output:**
```
[1, 99, 3]
{"a": 1, "b": 2}
```

### `remove(list_or_set_or_map, index_or_key)`

- For lists: removes the element at `index_or_key` and returns it.
- For sets: removes the element `index_or_key` and returns `true` if it was removed.
- For maps: removes the key `index_or_key` and returns `true` if it was removed.

```xell
let l = [1, 2, 3]
let val = remove(l, 1)
print(val)   # 2
print(l)     # [1, 3]

let s = {1, 2, 3}
let removed = remove(s, 2)
print(removed)   # true
print(s)         # {1, 3}

let m = { "a": 1, "b": 2 }
removed = remove(m, "a")
print(removed)   # true
print(m)         # {"b": 2}
```

**Output:**
```
2
[1, 3]
true
{1, 3}
true
{"b": 2}
```

---

## Sets

### `add(set, elem)`

Adds an element to a set. Returns `none`. Throws `HashError` if the element is not hashable.

```xell
let s = {1, 2, 3}
add(s, 4)
print(s)   # {1, 2, 3, 4}
```

**Output:**
```
{1, 2, 3, 4}
```

### `union_set(s1, s2)`

Returns a new set containing all elements from both sets.

```xell
let s1 = {1, 2}
let s2 = {2, 3}
let u = union_set(s1, s2)
print(u)   # {1, 2, 3}
```

**Output:**
```
{1, 2, 3}
```

### `intersect(s1, s2)`

Returns a new set containing elements that are in both sets.

```xell
let s1 = {1, 2, 3}
let s2 = {2, 3, 4}
let i = intersect(s1, s2)
print(i)   # {2, 3}
```

**Output:**
```
{2, 3}
```

### `diff(s1, s2)`

Returns a new set containing elements that are in `s1` but not in `s2`.

```xell
let s1 = {1, 2, 3, 4}
let s2 = {3, 4, 5}
let d = diff(s1, s2)
print(d)   # {1, 2}
```

**Output:**
```
{1, 2}
```

---

## Complete Example: Data Processing Pipeline

```xell
# Data processing pipeline
let raw_data = [
    { "name": "Alice", "score": 85 },
    { "name": "Bob", "score": 92 },
    { "name": "Charlie", "score": 78 },
    { "name": "Alice", "score": 90 }   # duplicate name
]

# Extract all scores
let scores = []
for record in raw_data:
    push(scores, record["score"])
;

print("Scores:", scores)

# Sort scores
sort(scores)
print("Sorted:", scores)

# Average
let average = avg(scores)
print("Average:", average)

# Get unique names
let names = []
for record in raw_data:
    push(names, record["name"])
;
let unique_names = unique(names)
print("Unique names:", unique_names)

# Build a lookup map
let lookup = from_entries(zip(names, scores))
print("Lookup:", lookup)
```

**Expected output:**
```
Scores: [85, 92, 78, 90]
Sorted: [78, 85, 90, 92]
Average: 86.25
Unique names: ["Alice", "Bob", "Charlie"]
Lookup: {"Alice": 90, "Bob": 92, "Charlie": 78}
```

---

## What NOT to Do

### 1. Using `pop` on an Empty List

```xell
let empty = []
pop(empty)   # ❌ IndexError – pop from empty list
```

**Why it fails:** `pop` cannot remove from an empty list. Check `is_empty` first.

### 2. Using `sort` on a List with Mixed Types

```xell
let mixed = [1, "two", 3]
sort(mixed)   # ❌ TypeError – cannot compare int and string
```

**Why it fails:** `sort` compares elements; mixed types cannot be compared. Ensure lists are homogeneous for sorting.

### 3. Adding a Non‑Hashable Element to a Set

```xell
add({}, [1, 2])   # ❌ HashError – list is mutable and not hashable
```

**Why it fails:** Sets require hashable (immutable) elements. Use a tuple or frozen set instead.

### 4. Modifying a Frozen Set

```xell
let fs = iSet([1, 2, 3])
add(fs, 4)   # ❌ ImmutabilityError – frozen sets are immutable
```

**Why it fails:** Frozen sets cannot be modified. Use a regular set for mutable operations.

### 5. Expecting `zip` to Pad Missing Values

```xell
let a = [1, 2]
let b = ["a"]
zip(a, b)   # Returns [[1, "a"]] – 2 is dropped
```

**Why it happens:** `zip` truncates to the shortest list. Use `zip_longest` for padding.

### 6. Using `merge` with Non‑Map Arguments

```xell
merge([1, 2], { "a": 1 })   # ❌ TypeError – merge expects two maps
```

### 7. Forgetting that `reverse` Modifies In‑Place

```xell
let numbers = [1, 2, 3]
let reversed = reverse(numbers)   # reversed is the same list, not a copy
print(reversed is numbers)   # true
```

**Why it matters:** `reverse` modifies the original list. Use `slice` to create a reversed copy.

### 8. Using `remove` with an Out‑of‑Range Index on a List

```xell
let list = [1, 2, 3]
remove(list, 5)   # ❌ IndexError – index out of range
```

---

## See Also

- [Higher‑Order Functions](stdlib-hof.md) – `map`, `filter`, `reduce`, `any`, `all`
- [Data Types](../core/data-types.md) – collection types reference
- [Operators](../core/operators.md) – collection operators (`in`, `+`, `*`)
- [Built‑ins Reference](../core/builtins.md) – complete function list

---

**Previous:** [Built‑in Modules](../modules/modules-builtins.md)  
**Next:** [Higher‑Order Functions](stdlib-hof.md)