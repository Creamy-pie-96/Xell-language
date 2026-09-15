# Data Types

> **Previous:** [Syntax Basics](syntax-basics.md)  
> **Next:** [Variables & Binding](variables-binding.md)

---

Xell is dynamically typed – variables can hold any type, and type checking is performed at runtime.  
This section covers all built‑in data types and their properties.

---

## Primitive Types

### Integer (`int`)

Integers are arbitrary‑precision (C++ `int64_t` under the hood, with overflow wrapping).

**Syntax:**

```xell
let a = 42
let b = -100
let c = 0x1F      # hexadecimal (31)
let d = 0o77      # octal (63)
let e = 0b1010    # binary (10)
let f = 1_000_000 # digit separator
```

**Operations:** `+`, `-`, `*`, `/` (returns float if not evenly divisible), `%` (modulo), `**` (exponentiation), bitwise (`|`, `&`, `^`, `<<`, `>>`, `~`).

**Truthiness:** `0` is `false`, everything else `true`.

---

### Float (`float`)

Double‑precision (IEEE 754).

**Syntax:**

```xell
let pi = 3.14159
let e = 2.71828
let sci = 1.2E10   # 1.2 × 10¹⁰
let neg = -0.001
```

**Operations:** same arithmetic as integers, but all results are `float`.

**Truthiness:** `0.0` is `false`, everything else `true`.

---

### Complex (`complex`)

Complex numbers with real and imaginary parts.

**Syntax:**

```xell
let z = 2 + 3i        # 2 + 3i
let w = 3.14i         # pure imaginary (0 + 3.14i)
let c = 0xAi          # imaginary from hex literal
```

**Operations:** `+`, `-`, `*`, `/`, `**`, `-` (unary). Comparison operators (`>`, `<`, etc.) are **not** supported for complex numbers.

**Truthiness:** `0 + 0i` is `false`, everything else `true`.

---

### Boolean (`bool`)

Exactly two values: `true` and `false`.

**Syntax:**

```xell
let ok = true
let fail = false
```

**Operations:** logical (`and`, `or`, `not`), equality (`==`, `!=`).

**Truthiness:** the value itself.

---

### String (`string`)

Immutable sequence of Unicode characters (UTF‑8 encoded).

**Syntax:**

- **Double‑quoted:** `"Hello"`
- **Single‑quoted:** `'World'`
- **Triple‑quoted:** `"""line1\nline2"""` (dedented automatically)
- **Raw:** `r"path\to\file"` (backslashes are literal)
- **F‑string:** `f"Hello, {name}!"` (interpolates expressions)

**Escape sequences:** `\n`, `\t`, `\r`, `\\`, `\"`, `\'`, `\xHH`, `\uXXXX`, `\UXXXXXXXX`.

**Operations:** concatenation (`+`), repetition (`*` with integer), indexing (`str[0]`), slicing (`str[1:3]`), membership (`"sub" in str`), comparison (`>`, `<`, `==`, `!=`).

**Truthiness:** empty string `""` is `false`, non‑empty is `true`.

---

### Bytes (`bytes`)

Immutable sequence of raw bytes (0–255).

**Syntax:**

```xell
let data = b"\x48\x65\x6c\x6c\x6f"   # bytes "Hello"
let empty = b""
```

**Operations:** indexing (`data[0]`), slicing, concatenation (`+`), repetition (`*`), membership, comparison.

**Truthiness:** empty `b""` is `false`, non‑empty is `true`.

---

### None (`none`)

Represents the absence of a value.

**Syntax:**

```xell
let x = none
```

**Operations:** equality (`==`, `!=`) with `none`.

**Truthiness:** always `false`.

---

## Compound Types

### List (`list`)

Mutable, ordered collection of arbitrary values.

**Syntax:** `[1, 2, "three", 4.0]`

**Operations:** indexing, slicing, append (`push`), pop, insert, remove, concatenation (`+`), repetition (`*`), membership (`in`), iteration.

**Truthiness:** empty list `[]` is `false`, non‑empty is `true`.

---

### Tuple (`tuple`)

Immutable, ordered collection. Often used for fixed‑size records.

**Syntax:** `(1, "two", 3.0)` or `(1,)` for single element.

**Operations:** indexing, slicing, concatenation (`+`), repetition (`*`), membership, iteration.

**Truthiness:** empty tuple `()` is `false`, non‑empty is `true`.

---

### Set (`set`)

Mutable, unordered collection of unique, **hashable** values.

**Syntax:** `{1, 2, 3}`

**Hashable types:** `int`, `float`, `complex`, `bool`, `string`, `bytes`, `tuple` (if all elements hashable), `frozen_set` (if all elements hashable), and frozen instances with `__hash__`.

**Operations:** `add`, `remove`, `has` (membership), `size`, union (`|`), intersection (`&`), difference (`-`), symmetric difference (`^`).

**Truthiness:** empty set `{}` (but see note below) is `false`, non‑empty is `true`.

> **Note:** `{}` in Xell creates an empty **map**, not an empty set. Use `set()` to create an empty set.

---

### Frozen Set (`frozen_set`)

Immutable version of a set. Once created, elements cannot be added or removed.

**Syntax:** `<1, 2, 3>` (angle brackets).

**Operations:** membership, iteration, size, and all set operations that return a new set.

**Truthiness:** empty `< >` is `false`, non‑empty is `true`.

---

### Map (`map`)

Mutable, ordered collection of key‑value pairs. Keys can be any **hashable** value; values can be any type. Iteration preserves insertion order.

**Syntax:** `{ "key": "value", 42: "answer", (1,2): "tuple key" }`

**Operations:** indexing (`map["key"]`), assignment (`map["key"] = new_value`), `has` (key membership), `remove`, `keys`, `values`, `entries`.

**Truthiness:** empty map `{}` is `false`, non‑empty is `true`.

---

## Type Checking

Use the `type()` built‑in to get the type name of any value:

```xell
print(type(42))        # "int"
print(type(3.14))      # "float"
print(type("hello"))   # "string"
print(type([]))        # "list"
```

For type‑aware logic, use `belong` in pattern matching or `is` with custom classes.

---

## Next Steps

Now that you understand Xell’s data types, learn how to work with them using **[Variables & Binding](variables-binding.md)**.

---

**Previous:** [Syntax Basics](syntax-basics.md)  
**Next:** [Variables & Binding](variables-binding.md)