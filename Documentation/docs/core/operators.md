# Operators

> **Previous:** [Variables & Binding](variables-binding.md)  
> **Next:** [Expressions](expressions.md)

---

Xell provides a rich set of operators for arithmetic, comparison, logic, bitwise operations, and shell integration. Operators follow standard precedence rules with a few Xell‑specific twists.

---

## Arithmetic Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `+` | Addition / string/list concatenation | `x + y` |
| `-` | Subtraction | `x - y` |
| `*` | Multiplication / repetition | `x * y` |
| `/` | Division (returns `float` if not evenly divisible) | `x / y` |
| `%` | Modulo (remainder) | `x % y` |
| `**` | Exponentiation (right‑associative) | `x ** y` |

**Numeric operations:**

```xell
let a = 10 + 5      # 15
let b = 10 - 5      # 5
let c = 10 * 5      # 50
let d = 10 / 3      # 3.333... (float)
let e = 10 % 3      # 1
let f = 2 ** 8      # 256
```

**Integer division:**  
If both operands are integers and the division is exact, the result is an `int`; otherwise it’s a `float`.

```xell
10 / 2   # 5 (int)
10 / 3   # 3.33333 (float)
```

**Complex numbers:** Arithmetic works with complex numbers too.

```xell
let z = (2 + 3i) + (1 - 2i)   # 3 + 1i
```

**String/list concatenation:**

```xell
"Hello, " + "world!"   # "Hello, world!"
[1, 2] + [3, 4]        # [1, 2, 3, 4]
```

**Repetition:**

```xell
"abc" * 3   # "abcabcabc"
[0] * 5     # [0, 0, 0, 0, 0]
```

---

## Comparison Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `==` | Equal | `x == y` |
| `!=` | Not equal | `x != y` |
| `>` | Greater than | `x > y` |
| `<` | Less than | `x < y` |
| `>=` | Greater than or equal | `x >= y` |
| `<=` | Less than or equal | `x <= y` |
| `is` | Instance check / identity (for objects) | `obj is ClassName` |
| `is not` | Negated instance check | `obj is not ClassName` |
| `in` | Membership | `x in container` |
| `not in` | Negated membership | `x not in container` |

**Numeric comparisons:**

```xell
10 > 5      # true
3.14 <= 3.14   # true
2 + 3i == 2 + 3i   # true
```

**Strings compare lexicographically by Unicode code point:**

```xell
"apple" < "banana"   # true
"Zebra" > "Apple"    # true (case‑sensitive)
```

**Chained comparisons:**

Xell supports chained comparisons with short‑circuiting.

```xell
let x = 5
x > 0 and x < 10   # true (explicit `and`)
1 < x < 10         # true (chained – equivalent to `1 < x and x < 10`)
```

Chained comparisons are evaluated left‑to‑right with short‑circuit semantics.

**`is` – instance check:**

```xell
class Animal ; class Dog inherits Animal ;

let d = Dog()
d is Dog      # true
d is Animal   # true (subclass matches parent)
```

For non‑instance operands, `is` falls back to `==`.

**`in` – membership:**

Works on strings, lists, tuples, sets, maps, and any type with `__contains__`.

```xell
"sub" in "substring"       # true
3 in [1, 2, 3]             # true
"key" in { "key": "val" }  # true
```

---

## Logical Operators

| Operator | Description | Short‑circuits? |
|----------|-------------|-----------------|
| `and` | Logical AND | Yes (returns first falsy value) |
| `or` | Logical OR | Yes (returns first truthy value) |
| `not` | Logical NOT | No |

```xell
true and false   # false
true or false    # true
not true         # false
```

**Short‑circuit behavior:**

```xell
let x = 10
x > 0 and print("positive")   # prints "positive"
x < 0 or print("zero or negative")   # prints "zero or negative"
```

**Truthiness rules:**

- `none` → `false`
- `0`, `0.0`, `0+0i` → `false`
- Empty containers (`[]`, `""`, `{}`, `< >`) → `false`
- Everything else → `true`

---

## Bitwise Operators

| Operator | Description | Example |
|----------|-------------|---------|
| `|` | Bitwise OR | `x | y` |
| `&` | Bitwise AND | `x & y` |
| `^` | Bitwise XOR | `x ^ y` |
| `<<` | Left shift | `x << n` |
| `>>` | Right shift | `x >> n` |
| `~` | Bitwise NOT | `~x` |

**Operands must be integers.**

```xell
let a = 0b1010   # 10
let b = 0b1100   # 12
a | b   # 0b1110 (14)
a & b   # 0b1000 (8)
a ^ b   # 0b0110 (6)
a << 2  # 0b101000 (40)
a >> 1  # 0b0101 (5)
~a      # -11 (two's complement)
```

---

## Shell Operators

These operators are designed for command‑line integration.

### Pipe (`|>`)

Concatenates two strings with `" | "` – useful for building shell pipelines.

```xell
let cmd = "ls" |> "grep .xel"
# cmd = "ls | grep .xel"
```

### Shell AND (`&&`)

Executes the right‑hand side **only if** the left‑hand side succeeds (exit code `0` for commands, or `truthy` for other values).

```xell
# With commands:
$ls && $echo "files listed"

# With exit codes:
let code = run("ls")
code == 0 && $echo "success"
```

**Truthiness for shell AND:**  
- Numeric `0` → success (`true`)  
- Map with `"exit_code": 0` → success  
- Any `truthy` value → success  

### Shell OR (`||`)

Executes the right‑hand side **only if** the left‑hand side fails (non‑zero exit code, or `falsy` value).

```xell
$ls /nonexistent || $echo "failed"
```

---

## Assignment Operators

| Operator | Meaning | Example |
|----------|---------|---------|
| `=` | Simple assignment | `x = 10` |
| `+=` | Add and assign | `x += 5` |
| `-=` | Subtract and assign | `x -= 3` |
| `*=` | Multiply and assign | `x *= 2` |
| `/=` | Divide and assign | `x /= 4` |
| `%=` | Modulo and assign | `x %= 2` |
| `**=` | Exponentiate and assign | `x **= 3` |
| `&=` | Bitwise AND and assign | `x &= 0xF` |
| `\|=` | Bitwise OR and assign | `x \|= 0x0F` |
| `^=` | Bitwise XOR and assign | `x ^= 0xAA` |
| `<<=` | Left shift and assign | `x <<= 1` |
| `>>=` | Right shift and assign | `x >>= 1` |

```xell
let x = 10
x += 5     # x = 15
x **= 2    # x = 225
```

---

## Increment / Decrement

| Operator | Description | Example |
|----------|-------------|---------|
| `++` | Increment by 1 | `x++` (postfix), `++x` (prefix) |
| `--` | Decrement by 1 | `x--` (postfix), `--x` (prefix) |

**Prefix vs. Postfix:**

- **Prefix** (`++x`, `--x`): returns the new value after modification.
- **Postfix** (`x++`, `x--`): returns the old value before modification.

```xell
let x = 5
let y = ++x   # x becomes 6, y = 6
let z = x--   # z = 6 (old value), x becomes 5
```

Operands must be numeric variables (not literals or expressions).

---

## Operator Precedence

From highest to lowest precedence:

| Precedence | Operators | Associativity |
|------------|-----------|---------------|
| 1 (highest) | `()` (grouping), `[]` (index), `->` (member), `++`/`--` (postfix) | Left‑to‑right |
| 2 | `**` (exponentiation) | Right‑to‑left |
| 3 | `+x`, `-x`, `++x`, `--x`, `~x`, `not x` (unary) | Right‑to‑left |
| 4 | `*`, `/`, `%` | Left‑to‑right |
| 5 | `+`, `-` (binary) | Left‑to‑right |
| 6 | `<<`, `>>` | Left‑to‑right |
| 7 | `&` (bitwise AND) | Left‑to‑right |
| 8 | `^` (bitwise XOR) | Left‑to‑right |
| 9 | `|` (bitwise OR) | Left‑to‑right |
| 10 | `==`, `!=`, `>`, `<`, `>=`, `<=`, `is`, `is not`, `in`, `not in` | Left‑to‑right |
| 11 | `and` | Left‑to‑right |
| 12 | `or` | Left‑to‑right |
| 13 | `|>` (pipe) | Left‑to‑right |
| 14 | `&&` (shell AND) | Left‑to‑right |
| 15 (lowest) | `||` (shell OR) | Left‑to‑right |

Use parentheses to override precedence:

```xell
2 + 3 * 4     # 14 (3*4 first)
(2 + 3) * 4   # 20
```

---

## Operator Overloading

Custom classes can define **magic methods** to overload operators:

| Operator | Magic Method |
|----------|--------------|
| `+` | `__add__` |
| `-` | `__sub__` |
| `*` | `__mul__` |
| `/` | `__div__` |
| `%` | `__mod__` |
| `**` | `__pow__` |
| `&` | `__and__` |
| `|` | `__or__` |
| `^` | `__xor__` |
| `<<` | `__lshift__` |
| `>>` | `__rshift__` |
| `\|>` | `__pipe__` |
| `==` | `__eq__` |
| `!=` | `__ne__` |
| `<` | `__lt__` |
| `>` | `__gt__` |
| `<=` | `__le__` |
| `>=` | `__ge__` |
| `-` (unary) | `__neg__` |
| `~` | `__invert__` |
| `[]` (get) | `__get__` |
| `[]` (set) | `__set__` |

See [Magic Methods](../oop/oop-magic-methods.md) for details.

---

## Common Pitfalls

### `=` vs `==`

```xell
if x = 10 :  # ❌ error – assignment not allowed in condition
    print("x is 10")
;
```

Use `==` for comparison, `=` only for assignment.

### Short‑circuit Precedence

```xell
true or false and false   # true (and binds tighter than or)
(true or false) and false # false
```

### Complex Number Comparisons

`>`, `<`, `>=`, `<=` do **not** work on complex numbers:

```xell
(2 + 3i) > (1 + 2i)   # TypeError
```

Use magnitude comparison manually if needed.

---

## Next Steps

Now that you know the operators, see how they combine into **[Expressions](expressions.md)**.

---

**Previous:** [Variables & Binding](variables-binding.md)  
**Next:** [Expressions](expressions.md)