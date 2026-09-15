# Standard Library – Higher‑Order Functions

> **Previous:** [Collections](stdlib-collections.md)  
> **Next:** [File System Operations](stdlib-fs.md)

---

Higher‑order functions are functions that take other functions as arguments or return them as results. Xell provides five core higher‑order functions for working with lists: `map`, `filter`, `reduce`, `any`, and `all`. These are always available (Tier 1) and are essential for functional‑style programming.

---

## Quick Reference

| Function | What it does | Example |
|----------|--------------|---------|
| `map(list, fn)` | Applies `fn` to each element, returns a new list | `map([1,2,3], x => x*2)` → `[2,4,6]` |
| `filter(list, fn)` | Returns elements where `fn` returns `true` | `filter([1,2,3,4], x => x%2==0)` → `[2,4]` |
| `reduce(list, fn, initial)` | Reduces to a single value | `reduce([1,2,3], (acc,x)=>acc+x, 0)` → `6` |
| `any(list, fn)` | `true` if any element matches | `any([1,2,3], x=>x>2)` → `true` |
| `all(list, fn)` | `true` if all elements match | `all([1,2,3], x=>x>0)` → `true` |

---

## `map(list, fn)`

Applies a function to every element of a list and returns a new list of the results.

### Syntax

```xell
map(list, fn)
```

- `list` – a list of values.
- `fn` – a function that takes one argument and returns a transformed value.

### Example: Doubling Numbers

```xell
let numbers = [1, 2, 3, 4, 5]
let doubled = map(numbers, x => x * 2)
print(doubled)   # [2, 4, 6, 8, 10]
```

**Output:**
```
[2, 4, 6, 8, 10]
```

### Example: Converting to Strings

```xell
let numbers = [10, 20, 30]
let strings = map(numbers, str)
print(strings)   # ["10", "20", "30"]
```

**Output:**
```
["10", "20", "30"]
```

### Example: Extracting Fields from a List of Maps

```xell
let people = [
    { "name": "Alice", "age": 30 },
    { "name": "Bob", "age": 25 },
    { "name": "Charlie", "age": 35 }
]

let names = map(people, p => p["name"])
print(names)   # ["Alice", "Bob", "Charlie"]
```

**Output:**
```
["Alice", "Bob", "Charlie"]
```

### Edge Cases

- If the input list is empty, `map` returns an empty list.
- If `fn` throws an error, `map` propagates it immediately.
- The returned list has the same length as the input.

```xell
map([], x => x * 2)   # []
```

---

## `filter(list, fn)`

Returns a new list containing only the elements for which `fn` returns `true`.

### Syntax

```xell
filter(list, fn)
```

- `list` – a list of values.
- `fn` – a function that takes one argument and returns a boolean (truthy/falsy).

### Example: Filtering Even Numbers

```xell
let numbers = [1, 2, 3, 4, 5, 6]
let evens = filter(numbers, x => x % 2 == 0)
print(evens)   # [2, 4, 6]
```

**Output:**
```
[2, 4, 6]
```

### Example: Filtering Strings by Length

```xell
let words = ["apple", "banana", "cat", "dog", "elephant"]
let long_words = filter(words, w => len(w) > 4)
print(long_words)   # ["apple", "banana", "elephant"]
```

**Output:**
```
["apple", "banana", "elephant"]
```

### Example: Filtering Maps by Condition

```xell
let people = [
    { "name": "Alice", "age": 30 },
    { "name": "Bob", "age": 25 },
    { "name": "Charlie", "age": 35 }
]

let over_30 = filter(people, p => p["age"] > 30)
print(over_30)   # [{"name": "Charlie", "age": 35}]
```

**Output:**
```
[{"name": "Charlie", "age": 35}]
```

### Edge Cases

- If the input list is empty, `filter` returns an empty list.
- Elements for which `fn` returns a falsy value (`false`, `none`, `0`, empty string, etc.) are excluded.
- The order of elements is preserved.

```xell
filter([1, 2, 3], x => false)   # []
filter([], x => true)           # []
```

---

## `reduce(list, fn, initial)`

Reduces a list to a single value by repeatedly applying `fn` to an accumulator and each element.

### Syntax

```xell
reduce(list, fn, initial)
```

- `list` – a list of values.
- `fn` – a function that takes two arguments: `(accumulator, element)` and returns a new accumulator value.
- `initial` – the starting value for the accumulator.

### Example: Sum of a List

```xell
let numbers = [1, 2, 3, 4]
let sum = reduce(numbers, (acc, x) => acc + x, 0)
print(sum)   # 10
```

**Output:**
```
10
```

### Example: Product of a List

```xell
let numbers = [1, 2, 3, 4]
let product = reduce(numbers, (acc, x) => acc * x, 1)
print(product)   # 24
```

**Output:**
```
24
```

### Example: Maximum Value

```xell
let numbers = [3, 1, 4, 1, 5]
let max_val = reduce(numbers, (acc, x) => if x > acc : x else : acc, numbers[0])
print(max_val)   # 5
```

**Output:**
```
5
```

### Example: Building a Map from a List of Pairs

```xell
let pairs = [["a", 1], ["b", 2], ["c", 3]]
let m = reduce(pairs, (acc, pair) => { acc[pair[0]] = pair[1]; give acc }, {})
print(m)   # {"a": 1, "b": 2, "c": 3}
```

**Output:**
```
{"a": 1, "b": 2, "c": 3}
```

### Edge Cases

- If the list is empty, `reduce` returns the `initial` value without calling `fn`.
- The `fn` must handle the accumulator type correctly.

```xell
reduce([], (acc, x) => acc + x, 0)   # 0
reduce([1, 2, 3], (acc, x) => acc + x, 0)   # 6
```

---

## `any(list, fn)`

Returns `true` if at least one element in the list satisfies the predicate `fn`. Otherwise returns `false`. Short‑circuits: stops evaluating as soon as a `true` is found.

### Syntax

```xell
any(list, fn)
```

- `list` – a list of values.
- `fn` – a function that takes one argument and returns a boolean (truthy/falsy).

### Example: Checking for Even Numbers

```xell
let numbers = [1, 3, 5, 7, 8]
let has_even = any(numbers, x => x % 2 == 0)
print(has_even)   # true
```

**Output:**
```
true
```

### Example: Checking for Negative Numbers

```xell
let numbers = [1, 2, 3, 4]
let has_negative = any(numbers, x => x < 0)
print(has_negative)   # false
```

**Output:**
```
false
```

### Example: Checking for Strings Starting with a Letter

```xell
let words = ["apple", "banana", "cherry"]
let starts_with_b = any(words, w => starts_with(w, "b"))
print(starts_with_b)   # true
```

**Output:**
```
true
```

### Edge Cases

- If the list is empty, `any` returns `false` (there are no elements, so none can satisfy the predicate).
- Short‑circuits: stops at the first `true` – no further elements are processed.

```xell
any([], x => true)   # false
any([1, 2, 3], x => x > 2)   # true (stops at 3)
```

---

## `all(list, fn)`

Returns `true` if **all** elements in the list satisfy the predicate `fn`. Otherwise returns `false`. Short‑circuits: stops evaluating as soon as a `false` is found.

### Syntax

```xell
all(list, fn)
```

- `list` – a list of values.
- `fn` – a function that takes one argument and returns a boolean (truthy/falsy).

### Example: Checking if All Numbers Are Positive

```xell
let numbers = [1, 2, 3, 4, 5]
let all_positive = all(numbers, x => x > 0)
print(all_positive)   # true
```

**Output:**
```
true
```

### Example: Checking if All Numbers Are Even

```xell
let numbers = [2, 4, 6, 7]
let all_even = all(numbers, x => x % 2 == 0)
print(all_even)   # false
```

**Output:**
```
false
```

### Example: Checking if All Strings Are Non‑Empty

```xell
let words = ["apple", "banana", "", "cherry"]
let all_nonempty = all(words, w => len(w) > 0)
print(all_nonempty)   # false
```

**Output:**
```
false
```

### Edge Cases

- If the list is empty, `all` returns `true` (vacuously true – there are no elements to violate the condition).
- Short‑circuits: stops at the first `false` – no further elements are processed.

```xell
all([], x => false)   # true
all([1, 2, 3], x => x > 1)   # false (stops at 1)
```

---

## Complete Example: Data Analysis Pipeline

```xell
let data = [
    { "name": "Alice", "score": 85, "grade": "B" },
    { "name": "Bob", "score": 92, "grade": "A" },
    { "name": "Charlie", "score": 78, "grade": "C" },
    { "name": "Diana", "score": 88, "grade": "B" },
    { "name": "Eve", "score": 65, "grade": "D" }
]

# 1. Extract all scores
let scores = map(data, p => p["score"])
print("Scores:", scores)

# 2. Filter for grades B or better (A or B)
let passed = filter(data, p => p["grade"] == "A" or p["grade"] == "B")
print("Passed:", passed)

# 3. Compute average score
let avg_score = reduce(scores, (acc, s) => acc + s, 0) / len(scores)
print("Average score:", avg_score)

# 4. Check if anyone scored 90 or above
let has_high_scorer = any(data, p => p["score"] >= 90)
print("Has 90+ scorer?", has_high_scorer)

# 5. Check if everyone passed (grade D or better – assume D is pass)
let all_passed = all(data, p => p["grade"] != "F")
print("All passed?", all_passed)
```

**Expected output:**
```
Scores: [85, 92, 78, 88, 65]
Passed: [{"name": "Alice", "score": 85, "grade": "B"}, {"name": "Bob", "score": 92, "grade": "A"}, {"name": "Diana", "score": 88, "grade": "B"}]
Average score: 81.6
Has 90+ scorer? true
All passed? true
```

---

## What NOT to Do

### 1. Passing a Non‑List to `map`, `filter`, `reduce`, `any`, or `all`

```xell
map(42, x => x * 2)   # ❌ TypeError – expects a list
```

**Why it fails:** These functions work only on lists. Convert other collections with `to_list` first.

### 2. Using `reduce` Without an Initial Value

Xell's `reduce` requires an `initial` argument; it does not have a no‑initial version (unlike Python's `functools.reduce`). Always provide an initial value.

```xell
reduce([1, 2, 3], (acc, x) => acc + x)   # ❌ ArityError – missing third argument
```

### 3. Assuming `any` or `all` Work on Non‑List Collections

```xell
any({1, 2, 3}, x => x > 0)   # ❌ TypeError – expects a list
```

**Fix:** Convert with `to_list`:

```xell
any(to_list({1, 2, 3}), x => x > 0)
```

### 4. Expecting `map` or `filter` to Modify the Original List

`map` and `filter` return new lists; they do not modify the input.

```xell
let numbers = [1, 2, 3]
map(numbers, x => x * 2)
print(numbers)   # [1, 2, 3] – unchanged
```

### 5. Passing a Predicate That Returns Non‑Boolean Values

`filter`, `any`, and `all` use truthiness, so a predicate that returns a number (e.g., `x`) works, but it may produce unexpected results:

```xell
filter([0, 1, 2], x => x)   # [1, 2] (since 0 is falsy)
```

This is valid but can be confusing; use explicit comparisons for clarity.

### 6. Using `any` or `all` on an Empty List Without Understanding the Result

```xell
let empty = []
print(any(empty, x => true))   # false (correct)
print(all(empty, x => false))  # true (vacuously true)
```

This is mathematically correct but may be surprising. Ensure you handle empty lists explicitly if needed.

---

## See Also

- [Collections](stdlib-collections.md) – list and collection utilities
- [Functions](../core/functions.md) – defining functions to pass to HOFs
- [Lambdas](../core/lambdas.md) – anonymous function syntax
- [Built‑ins Reference](../core/builtins.md) – complete function list

---

**Previous:** [Collections](stdlib-collections.md)  
**Next:** [File System Operations](stdlib-fs.md)