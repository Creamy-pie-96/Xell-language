# F‑Strings – String Interpolation

> **Previous:** [IDE – Integrated Development Environment](../tooling/ide.md)  
> **Next:** [Hash System](hash-system.md)

---

F‑strings (formatted string literals) are Xell's built‑in string interpolation mechanism. They allow you to embed expressions directly inside string literals using `{...}` syntax. F‑strings are evaluated at runtime, making them both powerful and convenient for constructing dynamic strings.

Think of f‑strings as **strings with holes** – you write the template, and Xell fills in the holes with the values of your expressions.

---

## Quick Reference

| Syntax | What it does | Example |
|--------|--------------|---------|
| `f"..."` | An f‑string literal | `f"Hello, {name}!"` |
| `{expr}` | Interpolates `expr` into the string | `f"Answer: {42}"` → `"Answer: 42"` |
| `{expr:format}` | Applies a format specifier | `f"Pi = {PI:.2f}"` → `"Pi = 3.14"` |
| `{{` | Escaped opening brace (literal `{`) | `f"{{brace}}"` → `"{brace}"` |
| `}}` | Escaped closing brace (literal `}`) | `f"{{brace}}"` → `"{brace}"` |
| `{expr!r}` | Uses `repr()` / `__repr__` (future) | – |
| `{expr!s}` | Uses `str()` / `__str__` (default) | – |

---

## Basic Syntax

An f‑string is a string literal prefixed with `f`:

```xell
let name = "Xell"
let version = 0.1
let message = f"Welcome to {name} v{version}"
print(message)
```

**Output:**
```
Welcome to Xell v0.1
```

Inside the `{...}`, you can put **any expression** – not just variable names:

```xell
let x = 10
let y = 20
print(f"{x} + {y} = {x + y}")   # 10 + 20 = 30
print(f"sqrt(2) = {sqrt(2):.4f}")   # sqrt(2) = 1.4142
```

**Output:**
```
10 + 20 = 30
sqrt(2) = 1.4142
```

---

## Format Specifiers

Format specifiers control how values are displayed. They follow a colon (`:`) after the expression:

```xell
{value:format_spec}
```

### Alignment and Padding

| Specifier | Description | Example |
|-----------|-------------|---------|
| `{:<n}` | Left‑aligned in field width `n` | `{"abc":<5}` → `"abc  "` |
| `{:>n}` | Right‑aligned in field width `n` | `{"abc":>5}` → `"  abc"` |
| `{:^n}` | Centred in field width `n` | `{"abc":^5}` → `" abc "` |
| `{:=n}` | Sign‑aware padding (numbers) | `{-42:=6}` → `"-   42"` |
| `{:#n}` | Uses `0x`, `0o`, `0b` prefix | `{42:#x}` → `"0x2a"` |

```xell
print(f"|{'left':<10}|")   # |left      |
print(f"|{'right':>10}|")  # |     right|
print(f"|{'center':^10}|") # |  center  |
```

**Output:**
```
|left      |
|     right|
|  center  |
```

### Number Formatting

| Specifier | Description | Example |
|-----------|-------------|---------|
| `{:.nf}` | Float with `n` decimal places | `{PI:.2f}` → `"3.14"` |
| `{:.Ne}` | Scientific notation with `N` digits | `{1234.567:.2e}` → `"1.23e+03"` |
| `{:.Ng}` | General format (auto‑choose) | `{1234.567:.4g}` → `"1235"` |
| `{:d}` | Integer (decimal) | `{42:d}` → `"42"` |
| `{:x}` | Hexadecimal (lowercase) | `{255:x}` → `"ff"` |
| `{:X}` | Hexadecimal (uppercase) | `{255:X}` → `"FF"` |
| `{:o}` | Octal | `{42:o}` → `"52"` |
| `{:b}` | Binary | `{42:b}` → `"101010"` |
| `{:#x}` | Hex with `0x` prefix | `{255:#x}` → `"0xff"` |

```xell
let pi = 3.141592653589793
let n = 42

print(f"Pi: {pi:.2f}")           # Pi: 3.14
print(f"Pi: {pi:.5f}")           # Pi: 3.14159
print(f"Hex: {n:x}")             # Hex: 2a
print(f"Hex: {n:#x}")            # Hex: 0x2a
print(f"Binary: {n:b}")          # Binary: 101010
print(f"Scientific: {1234.567:.2e}")  # Scientific: 1.23e+03
```

**Output:**
```
Pi: 3.14
Pi: 3.14159
Hex: 2a
Hex: 0x2a
Binary: 101010
Scientific: 1.23e+03
```

### Sign Formatting (Numbers)

| Specifier | Description | Example |
|-----------|-------------|---------|
| `{:+}` | Show sign for both positive and negative | `{42:+}` → `"+42"` |
| `{:-}` | Show sign only for negative (default) | `{-42:-}` → `"-42"` |
| `{: }` | Space for positive sign | `{42: }` → `" 42"` |

```xell
print(f"{42:+}")    # +42
print(f"{-42:+}")   # -42
print(f"{42: }")    #  42
```

### Complex Numbers

Complex numbers show real and imaginary parts:

```xell
let z = 2 + 3i
print(f"{z}")           # (2+3i)
print(f"{z:.2f}")       # (2.00+3.00i)
```

**Output:**
```
(2+3i)
(2.00+3.00i)
```

---

## Nesting F‑Strings

You can nest f‑strings inside expressions:

```xell
let name = "Xell"
let greeting = f"Hello, {f"{name}"}!"   # nested
print(greeting)   # Hello, Xell!
```

**Output:**
```
Hello, Xell!
```

Nesting is useful when you need dynamic format specifiers:

```xell
let precision = 3
let value = 3.14159
print(f"Pi: {value:.{precision}f}")   # Pi: 3.142
```

**Output:**
```
Pi: 3.142
```

---

## Escaping Braces

To include literal `{` or `}` characters in an f‑string, double them:

```xell
let name = "Xell"
print(f"{{name}} is a variable")   # {name} is a variable
print(f"{{{name}}}")               # {Xell}
```

**Output:**
```
{name} is a variable
{Xell}
```

---

## Magic Methods in F‑Strings

When interpolating an object, Xell checks for magic methods to determine its string representation:

1. **`__str__(self)`** – called for user‑friendly representation.
2. **`__print__(self)`** – called by `print()` (fallback if `__str__` is not defined).

```xell
class Person :
    name = ""
    age = 0

    fn __str__(self):
        give "Person(" + self->name + ", " + str(self->age) + ")"
    ;
;

let p = Person({ name: "Alice", age: 30 })
print(f"User: {p}")   # User: Person(Alice, 30)
```

**Output:**
```
User: Person(Alice, 30)
```

If no `__str__` is defined, the default `toString()` representation is used (usually `<struct ClassName>` or `ClassName(field1=val1, ...)`).

---

## Complete Example: Report Generator

```xell
let name = "Project Xell"
let version = "1.2.0"
let author = "The Xell Team"
let lines_of_code = 12438
let test_coverage = 87.5
let is_public = true

let report = f"""
=== Project Report ===
Name:        {name}
Version:     {version}
Author:      {author}
Lines of code: {lines_of_code:,}
Test coverage: {test_coverage:.1f}%
Public:      {is_public}
Status:      {if is_public : "Open source" else : "Private"}
Last updated: {now()}->format_date("%Y-%m-%d")
"""

print(report)
```

**Expected output (varies by date):**
```
=== Project Report ===
Name:        Project Xell
Version:     1.2.0
Author:      The Xell Team
Lines of code: 12,438
Test coverage: 87.5%
Public:      true
Status:      Open source
Last updated: 2026-07-23
```

---

## F‑String vs String Concatenation

| Aspect | F‑String | Concatenation (`+`) |
|--------|----------|---------------------|
| Readability | ✅ Clear and concise | ❌ Hard to read with many variables |
| Performance | ✅ Faster (single pass) | ❌ Creates intermediate strings |
| Expressions | ✅ Any expression | ❌ Only strings (need manual conversion) |
| Formatting | ✅ Built‑in | ❌ Manual (e.g., `round()`, `str()`) |
| Multi‑line | ✅ Preserves formatting | ❌ Awkward with line continuations |

**Prefer f‑strings** for any non‑trivial string construction.

---

## What NOT to Do

### 1. Forgetting the `f` Prefix

```xell
let name = "Xell"
print("{name}")   # ❌ prints "{name}", not "Xell"
```

**Fix:** Add the `f` prefix: `f"{name}"`.

### 2. Using Backslashes Inside `{}`

```xell
print(f"{1\n2}")   # ❌ syntax error – newline inside expression
```

**Fix:** Use parentheses or avoid newlines inside `{}`.

### 3. Nested Braces Without Doubling

```xell
print(f"{{{name}}"}   # ❌ unbalanced braces
```

**Fix:** Use `{{` and `}}` for literal braces.

### 4. Using `%` Formatting (Python‑style) in F‑Strings

Xell f‑strings use `{expr:format}`, not `%s` or `%d`. The `%` operator is not supported for string formatting.

### 5. Assuming F‑Strings Support All Python Format Specifiers

Xell's format specifiers support the most common ones (alignment, padding, precision, number bases). Some advanced specifiers may not be implemented.

### 6. Using F‑Strings in the REPL Without Assigning Variables

```xell
> f"{x}"   # ❌ error – x is not defined
```

**Fix:** Define `x` first.

### 7. Expecting F‑Strings to Work in Raw Strings

```xell
let s = fr"hello {name}"   # ❌ f and r can't be combined
```

**Why it fails:** `f` and `r` prefixes are mutually exclusive. Use `f"..."` for interpolation and `r"..."` for raw strings.

### 8. Using `__str__` for Debugging

`__str__` is for user‑friendly output. For debugging, use the default representation or implement a separate method.

### 9. Interpolating Very Large Expressions

```xell
print(f"{complex_computation_that_takes_seconds()}")   # ❌ slow
```

**Why it fails:** The expression is evaluated when the f‑string is evaluated. Keep expressions simple or compute them beforehand.

---

## See Also

- [Strings](../core/data-types.md#string) – string type reference
- [Built‑ins Reference](../core/builtins.md) – `str()`, `print()`
- [Magic Methods](../oop/oop-magic-methods.md) – `__str__`, `__print__`

---

**Previous:** [IDE – Integrated Development Environment](../tooling/ide.md)  
**Next:** [Hash System](hash-system.md)