# Xell Slice Operations Guide

## Overview

Xell supports slice operations for **lists**, **strings**, and **tuples** using Python-like syntax:

```xell
list = [0, 1, 2, 3, 4, 5]
sub = list[1:4]        # [1, 2, 3]
sub = list[::2]        # [0, 2, 4] (every 2nd)
sub = list[1:5:2]      # [1, 3]
sub = list[-2:]        # [4, 5] (last 2)
str = "Hello, World!"
sub = str[0:5]         # "Hello"
sub = str[::-1]        # "!dlroW ,olleH" (reverse)
tup = (10, 20, 30, 40)
sub = tup[1:3]         # (20, 30)
```

## Syntax

- `[start:end]` — slice from `start` (inclusive) to `end` (exclusive)
- `[start:end:step]` — slice with a step
- `[:]` — copy entire sequence
- Negative indices count from the end
- Step can be negative (reverse)

## Built-in Functions

- `slice(list, start[, end])` — returns sublist
- `substr(str, start, length)` — substring

## Internal Interpretation

### Parsing

- Slice syntax is parsed as a `SliceExpr` AST node: `obj[start:end:step]`
- All three components are optional

### Evaluation

- The interpreter resolves start, end, and step, clamps indices, and handles negative values
- For lists/tuples: builds a new sequence by iterating with step
- For strings: builds a new string by concatenating characters
- Step zero throws an error

### Example (Interpreter Logic)

```cpp
// Pseudocode
if (obj.isList()) {
  for (int i = start; i < end; i += step) result.push_back(list[i]);
}
if (obj.isString()) {
  for (int i = start; i < end; i += step) result += str[i];
}
```

## Edge Cases

- Out-of-bounds indices are clamped
- Negative step reverses
- Step zero throws `ValueError`
- Empty slice returns empty sequence

## References

- See `LANGUAGE_FEATURES.md` and `src/interpreter/interpreter.cpp` for implementation details.
- Built-in: `slice`, `substr`, `char_at` for manual slicing.

---

**Supported:** List, String, Tuple slice syntax and builtins.

**Not Supported:** Map, Set, custom types (unless they implement slice magic).
