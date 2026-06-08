## ⬅️ What's Previous?

Move back to the **[Loops](./Loops.md)** to review the foundational concepts.

---

# Arrays & Collections

Xell provides two primary collection types: **Lists** and **Maps**. Both are dynamic and can hold values of mixed types.

## 📜 Lists

A list is an ordered, zero-indexed collection of values.

### Creation & Access
```xell
# Create a list
fruits = ["apple", "banana", "cherry"]
mixed = [1, "hello", true, none]

# Access by index
first = fruits[0]   # "apple"
last = fruits[-1]   # "cherry" (negative indexing)
```

### List Manipulation
Xell provides a rich set of built-in functions to modify and analyze lists.

| Function | Description | Example |
| :--- | :--- | :--- |
| `push(list, val)` | Adds `val` to the end of the list. | `push(fruits, "date")` |
| `pop(list)` | Removes and returns the last item. | `item = pop(fruits)` |
| `shift(list)` | Removes and returns the first item. | `item = shift(fruits)` |
| `unshift(list, val)`| Inserts `val` at the beginning. | `unshift(fruits, "apricot")` |
| `insert(list, i, v)`| Inserts `v` at index `i`. | `insert(fruits, 1, "blueberry")` |
| `remove_val(list, v)`| Removes first occurrence of `v`. | `remove_val(fruits, "apple")` |
| `slice(list, s, e)` | Returns a sublist from `s` to `e`. | `slice(fruits, 0, 2)` |
| `sort(list)` | Sorts list in ascending order (in-place). | `sort(fruits)` |
| `sort_desc(list)` | Sorts list in descending order (in-place).| `sort_desc(fruits)` |
| `flatten(list)` | Flattens a nested list one level deep. | `flatten([[1, 2], 3])` $\rightarrow$ `[1, 2, 3]` |
| `unique(list)` | Returns a list with duplicates removed. | `unique([1, 1, 2])` $\rightarrow$ `[1, 2]` |
| `len(list)` | Returns the number of elements. | `len(fruits)` |
| `range(s, e, st)` | Generates a list of numbers. | `range(0, 10, 2)` $\rightarrow$ `[0, 2, 4, 6, 8]` |

---

## 🗺️ Maps

A map is a collection of key-value pairs. Keys are typically identifiers or strings.

### Creation & Access
```xell
# Create a map
config = {
    host: "localhost",
    port: 8080,
    debug: true
}

# Access using the -> operator
h = config->host      # "localhost"
p = config->port      # 8080
```

### Map Manipulation
Maps can be modified using built-in functions.

| Function | Description | Example |
| :--- | :--- | :--- |
| `set(map, k, v)` | Sets key `k` to value `v`. | `set(config, "timeout", 30)` |
| `get(map, k, def)` | Gets value for `k`, or `def` if missing. | `get(config, "host", "127.0.0.1")` |
| `remove(map, k)` | Removes key `k` from the map. | `remove(config, "debug")` |
| `delete_key(map, k)`| Removes key `k` and returns its value. | `val = delete_key(config, "port")` |
| `merge(m1, m2)` | Merges two maps (m2 overrides m1). | `new_cfg = merge(defaults, user_cfg)` |
| `keys(map)` | Returns a list of all keys. | `keys(config)` |
| `values(map)` | Returns a list of all values. | `values(config)` |
| `entries(map)` | Returns list of `[key, value]` pairs. | `entries(config)` |
| `from_entries(list)`| Creates a map from `[key, value]` pairs. | `from_entries([["a", 1], ["b", 2]])` |
| `len(map)` | Returns number of key-value pairs. | `len(config)` |

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Strings](./Strings.md)** guide to further your understanding of Xell.
