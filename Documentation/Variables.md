## ⬅️ What's Previous?

Move back to the **[Grammar](./Grammar.md)** to review the foundational concepts.

---

# Variables

Variables in Xell are used to store data values. Xell uses a dynamic type system, meaning you don't need to explicitly declare the type of a variable.

## 📢 Declaration

### Dynamic Variables
Most variables in Xell are declared simply by assigning a value to a name. There is no need for keywords like `var` or `let`.

```xell
name = "Alice"
age = 30
is_ready = true
```

### Immutable Variables (Constants)
If you want to ensure a variable cannot be changed after it is first assigned, use the `immutable` keyword. This is useful for configuration values or constants.

```xell
immutable PI = 3.14159
immutable API_URL = "https://api.xell.lang"

# This would cause an error:
PI = 3.14 
```

---

## 🧬 Data Types

Xell supports several built-in types. All numbers are handled as 64-bit floating point values internally, so there is no distinction between integers and floats.

| Type | Description | Example |
| :--- | :--- | :--- |
| **`string`** | Text enclosed in double quotes. | `"Hello Xell"` |
| **`number`** | Numeric values (integer or decimal). | `42`, `-10.5` |
| **`bool`** | Boolean truth values. | `true`, `false` |
| **`list`** | An ordered collection of values. | `[1, "two", 3.0]` |
| **`map`** | A collection of key-value pairs. | `{ port: 8080, host: "localhost" }` |
| **`none`** | Represents the absence of a value. | `none` |

---

## 🔄 Mutability & Reassignment

Unless a variable is declared as `immutable`, its value can be changed at any time using the assignment operator `=`.

```xell
count = 10
print count # Output: 10

count = count + 1
print count # Output: 11
```

### Dynamic Typing
You can also change the type of a variable by assigning a value of a different type to it.

```xell
data = "Initial text"
data = 100 # Now it's a number
```

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Operators](./Operators.md)** guide to further your understanding of Xell.
