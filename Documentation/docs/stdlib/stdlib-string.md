# Standard Library – String Functions

> **Previous:** [Math Functions](stdlib-math.md)  
> **Next:** [REPL Usage](../tooling/repl.md)

---

Xell provides a rich set of functions for working with strings. These are always available (Tier 1) and cover everything from basic case conversion to advanced searching, splitting, joining, and padding.

---

## Quick Reference

| Function | What it does | Example |
|----------|--------------|---------|
| `upper(str)` | Converts to uppercase | `upper("hello")` → `"HELLO"` |
| `lower(str)` | Converts to lowercase | `lower("HELLO")` → `"hello"` |
| `trim(str)` | Removes leading/trailing whitespace | `trim("  hi  ")` → `"hi"` |
| `trim_start(str)` | Removes leading whitespace | `trim_start("  hi  ")` → `"hi  "` |
| `trim_end(str)` | Removes trailing whitespace | `trim_end("  hi  ")` → `"  hi"` |
| `starts_with(str, prefix)` | Checks prefix | `starts_with("hello", "he")` → `true` |
| `ends_with(str, suffix)` | Checks suffix | `ends_with("hello", "lo")` → `true` |
| `replace(str, old, new)` | Replaces all occurrences | `replace("foo bar", "o", "a")` → `"faa bar"` |
| `replace_first(str, old, new)` | Replaces first occurrence | `replace_first("foo bar", "o", "a")` → `"fao bar"` |
| `substr(str, start, len)` | Extracts substring | `substr("hello", 1, 3)` → `"ell"` |
| `char_at(str, n)` | Gets character at index | `char_at("hello", 1)` → `"e"` |
| `count(str_or_list, sub)` | Counts occurrences | `count("hello", "l")` → `2` |
| `split(str, sep)` | Splits string into list | `split("a,b,c", ",")` → `["a","b","c"]` |
| `join(list, sep)` | Joins list into string | `join(["a","b"], "-")` → `"a-b"` |
| `index_of(str_or_list, sub)` | Finds first index | `index_of("hello", "l")` → `2` |
| `is_empty(str_or_list_or_map)` | Checks emptiness | `is_empty("")` → `true` |
| `is_numeric(str)` | Checks if numeric | `is_numeric("42")` → `true` |
| `is_alpha(str)` | Checks if all alphabetic | `is_alpha("abc")` → `true` |
| `lines(str)` | Splits into lines | `lines("a\nb\nc")` → `["a","b","c"]` |
| `to_chars(str)` | Splits into characters | `to_chars("abc")` → `["a","b","c"]` |
| `repeat(str, n)` | Repeats string | `repeat("ha", 3)` → `"hahaha"` |
| `pad_start(str, n, ch)` | Pads at start | `pad_start("42", 5, "0")` → `"00042"` |
| `pad_end(str, n, ch)` | Pads at end | `pad_end("42", 5, "0")` → `"42000"` |
| `center(str, width[, fill])` | Centers string | `center("abc", 7, "-")` → `"--abc--"` |
| `ljust(str, width[, fill])` | Left‑justifies | `ljust("abc", 5, " ")` → `"abc  "` |
| `rjust(str, width[, fill])` | Right‑justifies | `rjust("abc", 5, " ")` → `"  abc"` |
| `zfill(str, width)` | Zero‑pads on left | `zfill("42", 5)` → `"00042"` |
| `reverse(str_or_list)` | Reverses in‑place | `reverse("hello")` → `"olleh"` |
| `upper()` | Alias for `upper()` | – |
| `lower()` | Alias for `lower()` | – |
| `trim()` | Alias for `trim()` | – |
| `replace()` | Alias for `replace()` | – |

---

## Case Conversion

### `upper(str)`

Converts all alphabetic characters in the string to uppercase.

```xell
let text = "Hello, World!"
print(upper(text))   # "HELLO, WORLD!"
```

**Output:**
```
HELLO, WORLD!
```

### `lower(str)`

Converts all alphabetic characters in the string to lowercase.

```xell
let text = "Hello, World!"
print(lower(text))   # "hello, world!"
```

**Output:**
```
hello, world!
```

---

## Trimming

### `trim(str)`

Removes leading and trailing whitespace (spaces, tabs, newlines, carriage returns).

```xell
let text = "  Hello, World!  "
print("'" + trim(text) + "'")   # "'Hello, World!'"
```

**Output:**
```
'Hello, World!'
```

### `trim_start(str)`

Removes only leading whitespace.

```xell
let text = "  Hello  "
print("'" + trim_start(text) + "'")   # "'Hello  '"
```

**Output:**
```
'Hello  '
```

### `trim_end(str)`

Removes only trailing whitespace.

```xell
let text = "  Hello  "
print("'" + trim_end(text) + "'")   # "'  Hello'"
```

**Output:**
```
'  Hello'
```

---

## Searching

### `starts_with(str, prefix)`

Returns `true` if `str` starts with `prefix`.

```xell
print(starts_with("hello", "he"))    # true
print(starts_with("hello", "lo"))    # false
```

**Output:**
```
true
false
```

### `ends_with(str, suffix)`

Returns `true` if `str` ends with `suffix`.

```xell
print(ends_with("hello", "lo"))    # true
print(ends_with("hello", "he"))    # false
```

**Output:**
```
true
false
```

### `index_of(str, sub)`

Returns the first index where `sub` occurs in `str`, or `-1` if not found.

```xell
print(index_of("hello", "l"))    # 2
print(index_of("hello", "lo"))   # 3
print(index_of("hello", "x"))    # -1
```

**Output:**
```
2
3
-1
```

### `count(str, sub)`

Counts the number of non‑overlapping occurrences of `sub` in `str`. If `sub` is empty, returns `0`.

```xell
print(count("hello", "l"))     # 2
print(count("hello", "lo"))    # 1
print(count("hello", "x"))     # 0
```

**Output:**
```
2
1
0
```

---

## Replacing

### `replace(str, old, new)`

Replaces **all** occurrences of `old` with `new` in `str`.

```xell
let text = "foo bar foo"
print(replace(text, "foo", "baz"))   # "baz bar baz"
```

**Output:**
```
baz bar baz
```

### `replace_first(str, old, new)`

Replaces only the **first** occurrence of `old` with `new`.

```xell
let text = "foo bar foo"
print(replace_first(text, "foo", "baz"))   # "baz bar foo"
```

**Output:**
```
baz bar foo
```

---

## Substrings

### `substr(str, start, len)`

Extracts a substring of length `len` starting at `start`. Negative indices count from the end. If `start` is out of bounds, returns an empty string. If `len` is negative, returns an empty string.

```xell
print(substr("hello", 1, 3))     # "ell"
print(substr("hello", -2, 2))    # "lo"
print(substr("hello", 10, 3))    # ""
```

**Output:**
```
ell
lo
```

### `char_at(str, n)`

Returns the character at index `n` as a single‑character string. Negative indices count from the end. Throws `IndexError` if out of range.

```xell
print(char_at("hello", 1))       # "e"
print(char_at("hello", -1))      # "o"
```

**Output:**
```
e
o
```

---

## Splitting and Joining

### `split(str, sep)`

Splits `str` into a list of substrings using `sep` as the delimiter. If `sep` is empty, splits into individual characters.

```xell
let parts = split("apple,banana,cherry", ",")
print(parts)   # ["apple", "banana", "cherry"]

let chars = split("abc", "")
print(chars)   # ["a", "b", "c"]
```

**Output:**
```
["apple", "banana", "cherry"]
["a", "b", "c"]
```

### `join(list, sep)`

Joins a list of strings into a single string with `sep` between each element.

```xell
let words = ["apple", "banana", "cherry"]
let result = join(words, ", ")
print(result)   # "apple, banana, cherry"
```

**Output:**
```
apple, banana, cherry
```

### `lines(str)`

Splits a string into a list of lines (handles `\n`, `\r\n`, `\r`).

```xell
let text = "line1\nline2\r\nline3\rline4"
let lines_list = lines(text)
print(lines_list)   # ["line1", "line2", "line3", "line4"]
```

**Output:**
```
["line1", "line2", "line3", "line4"]
```

### `to_chars(str)`

Splits a string into a list of single‑character strings.

```xell
print(to_chars("abc"))   # ["a", "b", "c"]
```

**Output:**
```
["a", "b", "c"]
```

---

## Padding and Alignment

### `pad_start(str, n, ch)`

Pads `str` on the left with `ch` until it reaches length `n`. If `n` is less than or equal to the length of `str`, returns `str` unchanged. `ch` must be a string (typically a single character).

```xell
print(pad_start("42", 5, "0"))   # "00042"
print(pad_start("abc", 6, "-"))  # "---abc"
```

**Output:**
```
00042
---abc
```

### `pad_end(str, n, ch)`

Pads `str` on the right with `ch` until it reaches length `n`.

```xell
print(pad_end("42", 5, "0"))   # "42000"
print(pad_end("abc", 6, "-"))  # "abc---"
```

**Output:**
```
42000
abc---
```

### `center(str, width[, fill])`

Centers `str` within a string of length `width` by padding equally on both sides with `fill` (defaults to space). If the total padding is odd, the extra character goes on the right.

```xell
print(center("abc", 7, "-"))   # "--abc--"
print(center("abc", 8, "-"))   # "--abc---"
print(center("abc", 5))        # " abc "
```

**Output:**
```
--abc--
--abc---
 abc
```

### `ljust(str, width[, fill])`

Left‑justifies `str` by padding on the right with `fill` (defaults to space).

```xell
print(ljust("abc", 5, "-"))   # "abc--"
print(ljust("abc", 5))        # "abc  "
```

**Output:**
```
abc--
abc
```

### `rjust(str, width[, fill])`

Right‑justifies `str` by padding on the left with `fill` (defaults to space).

```xell
print(rjust("abc", 5, "-"))   # "--abc"
print(rjust("abc", 5))        # "  abc"
```

**Output:**
```
--abc
  abc
```

### `zfill(str, width)`

Zero‑pads `str` on the left to reach `width`. If `str` starts with `+` or `-`, the sign is preserved and the zeros are inserted after the sign.

```xell
print(zfill("42", 5))        # "00042"
print(zfill("-42", 5))       # "-0042"
print(zfill("+42", 5))       # "+0042"
```

**Output:**
```
00042
-0042
+0042
```

---

## Other String Utilities

### `repeat(str, n)`

Repeats `str` `n` times. If `n` is negative, returns an empty string.

```xell
print(repeat("ha", 3))   # "hahaha"
print(repeat("x", 0))    # ""
```

**Output:**
```
hahaha
```

### `reverse(str)`

Reverses a string **in‑place** (modifies the original string). Returns the reversed string.

```xell
let text = "hello"
reverse(text)
print(text)   # "olleh"
```

**Output:**
```
olleh
```

### `is_empty(str_or_list_or_map)`

Returns `true` if the string, list, or map is empty.

```xell
print(is_empty(""))       # true
print(is_empty("abc"))    # false
print(is_empty([]))       # true
print(is_empty({}))       # true
```

**Output:**
```
true
false
true
true
```

### `is_numeric(str)`

Returns `true` if the string can be parsed as a number (integer or floating‑point).

```xell
print(is_numeric("42"))       # true
print(is_numeric("3.14"))     # true
print(is_numeric("-5"))       # true
print(is_numeric("abc"))      # false
```

**Output:**
```
true
true
true
false
```

### `is_alpha(str)`

Returns `true` if every character in `str` is an alphabetic letter (A‑Z or a‑z). Empty string returns `false`.

```xell
print(is_alpha("abc"))        # true
print(is_alpha("ABC"))        # true
print(is_alpha("abc123"))     # false
print(is_alpha(""))           # false
```

**Output:**
```
true
true
false
false
```

---

## F‑String Interpolation (Overview)

Xell supports `f"..."` string literals for interpolation. Inside an f‑string, expressions inside `{...}` are evaluated and their string representations are inserted.

```xell
let name = "Xell"
let version = 0.1
print(f"Welcome to {name} v{version}")
```

**Output:**
```
Welcome to Xell v0.1
```

**Format specifiers:** You can use format specifiers inside `{...}` after a colon:

- `{value:.2f}` – float with 2 decimal places
- `{value:>10}` – right‑aligned in a 10‑character field
- `{value:^10}` – centered in a 10‑character field
- `{value:d}` – integer format

For a full guide, see [F‑Strings](../advanced/f-strings.md).

---

## Complete Example: Text Processor

```xell
# text_processor.xel
let text = "  Hello,  Xell User!  "

# Trim whitespace
let trimmed = trim(text)
print("Trimmed: '" + trimmed + "'")

# Convert to lowercase
let lowercased = lower(trimmed)
print("Lowercased: " + lowercased)

# Replace text
let replaced = replace(lowercased, "xell", "xell language")
print("Replaced: " + replaced)

# Split into words
let words = split(replaced, " ")
print("Words: " + join(words, ", "))

# Check if it starts with "hello"
print("Starts with 'hello'? " + str(starts_with(replaced, "hello")))

# Pad for alignment
print("Padded: " + pad_end(replaced, 30, "."))

# Count vowels
let vowels = ["a", "e", "i", "o", "u"]
let vowel_count = 0
for ch in to_chars(lowercased):
    if contains(vowels, ch):
        vowel_count += 1
;
print("Vowels: " + str(vowel_count))
```

**Expected output:**
```
Trimmed: 'Hello,  Xell User!'
Lowercased: hello,  xell user!
Replaced: hello,  xell language user!
Words: hello,, xell, language, user!
Starts with 'hello'? true
Padded: hello,  xell language user!....
Vowels: 9
```

---

## What NOT to Do

### 1. Using `split()` with an Empty Separator for Large Strings

```xell
split("abc", "")   # ["a", "b", "c"] – valid but may be inefficient for very large strings
```

This is allowed but creates a list of individual characters. Use `to_chars()` if that's the intent.

### 2. Using `pad_start()` or `pad_end()` with a Multi‑Character Fill String

```xell
pad_start("abc", 10, "XY")   # ❌ throws TypeError – fill must be a single character
```

**Why it fails:** The functions expect a single character for padding. Use `center()` with multiple characters if needed.

### 3. Expecting `is_numeric()` to Handle Leading/Trailing Whitespace

```xell
is_numeric(" 42 ")   # false (contains spaces)
```

**Fix:** Trim the string first: `is_numeric(trim(" 42 "))` → `true`.

### 4. Using `replace()` with an Empty `old` String

```xell
replace("hello", "", "x")   # ❌ infinite loop (would never terminate)
```

**Why it fails:** Replacing an empty string is not allowed; it would cause infinite recursion.

### 5. Expecting `reverse()` to Return a New String

`reverse()` modifies the string **in‑place**. It does not return a new string (it returns the same string for chaining, but the original is mutated).

```xell
let s = "abc"
let rev = reverse(s)   # rev is the same string object
print(rev is s)        # true
```

### 6. Using `substr()` with a Negative Length

```xell
substr("hello", 1, -3)   # returns "" (empty string)
```

### 7. Using `char_at()` with an Out‑of‑Range Index

```xell
char_at("hello", 10)   # ❌ IndexError
```

### 8. Using `join()` with a Non‑List Argument

```xell
join("a,b,c", ",")   # ❌ TypeError – expects a list
```

**Fix:** Use `split()` or convert to a list first.

### 9. Expecting `center()` to Handle Odd Width Correctly

```xell
center("abc", 8, "-")   # "--abc---" (extra pad on the right)
```

This is the intended behaviour (right‑heavy).

---

## See Also

- [Data Types](../core/data-types.md) – string type reference
- [F‑Strings](../advanced/f-strings.md) – interpolation and formatting
- [Collections](stdlib-collections.md) – `split`, `join`, `contains`, `index_of`, `count` (polymorphic)

---

**Previous:** [Math Functions](stdlib-math.md)  
**Next:** [REPL Usage](../tooling/repl.md)