## ⬅️ What's Previous?

Move back to the **[Variables](./Variables.md)** to review the foundational concepts.

---

# Operators

Operators are symbols or keywords used to perform operations on variables and values.

## 🧮 Arithmetic Operators

Xell provides standard operators for mathematical calculations.

| Symbol | Operation | Example | Description |
| :--- | :--- | :--- | :--- |
| `+` | Addition / Concatenation | `5 + 2` or `"a" + "b"` | Adds numbers or joins strings. |
| `-` | Subtraction | `10 - 3` | Subtracts the right value from the left. |
| `*` | Multiplication | `4 * 3` | Multiplies two numbers. |
| `/` | Division | `10 / 2` | Divides the left value by the right. |
| `++` | Increment | `++x` or `x++` | Increases value by 1. |
| `--` | Decrement | `--x` or `x--` | Decreases value by 1. |

---

## ⚖️ Comparison Operators

Comparison operators are used to compare two values. Xell is unique in that it supports both **symbolic** and **keyword** forms, which are functionally identical.

| Symbolic | Keyword | Meaning | Example |
| :--- | :--- | :--- | :--- |
| `==` | `is` or `eq` | Equal to | `x == 10` or `x is 10` |
| `!=` | `ne` | Not equal to | `x != 0` or `x ne 0` |
| `>` | `gt` | Greater than | `x > 5` or `x gt 5` |
| `<` | `lt` | Less than | `x < 20` or `x lt 20` |
| `>=` | `ge` | Greater or equal | `x >= 10` or `x ge 10` |
| `<=` | `le` | Less or equal | `x <= 10` or `x le 10` |

---

## 🧠 Logical Operators

Logical operators are used to combine multiple boolean expressions.

| Keyword | Symbol | Meaning | Example |
| :--- | :--- | :--- | :--- |
| `not` | `!` | Negation | `not ready` or `!ready` |
| `and` | — | Logical AND | `is_valid and is_ready` |
| `or` | — | Logical OR | `has_admin or has_owner` |

---

## 🔝 Precedence

When multiple operators appear in a single expression, Xell evaluates them in the following order (from highest to lowest precedence):

1.  **Unary**: `not`, `!`, unary `-`, prefix `++` / `--`
2.  **Multiplicative**: `*`, `/`
3.  **Additive**: `+`, `-`
4.  **Relational**: `>`, `<`, `>=`, `<=`, `gt`, `lt`, `ge`, `le`
5.  **Equality**: `==`, `!=`, `is`, `eq`, `ne`
6.  **Logical AND**: `and`
7.  **Logical OR**: `or`

**Tip**: You can always use parentheses `( )` to override precedence and make your calculations explicit.

```xell
result = (5 + 2) * 10  # Evaluates to 70, not 25
```

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Functions](./Functions.md)** guide to further your understanding of Xell.
