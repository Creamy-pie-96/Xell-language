## ⬅️ What's Previous?

Move back to the **[Dialects](./Dialects.md)** to review the foundational concepts.

---

# Standard Library

The Xell Standard Library consists of the core data types and the extensive set of built-in functions available to every script.

## 💎 Core Types

All values in Xell belong to one of the following types. Xell is dynamically typed, but these types are strictly enforced internally.

| Type | Description | Mutable? |
| :--- | :--- | :--- |
| **`number`** | 64-bit floating point values. | No |
| **`string`** | UTF-8 encoded text. | No |
| **`bool`** | `true` or `false`. | No |
| **`list`** | Ordered sequence of values. | Yes |
| **`map`** | Key-value pairs. | Yes |
| **`tuple`** | Fixed-size ordered sequence. | No |
| **`set`** | Unordered collection of unique values. | Yes |
| **`frozen_set`** | Immutable set. | No |
| **`none`** | Represents nothing/null. | No |

---

## 📚 Built-in Modules

While most functions are available globally, they are logically grouped into several categories. For a detailed reference of these functions, please see the **[Built-ins Guide](./Built-ins.md)**.

### Key Categories:
- **OS & FS**: File manipulation, directory traversal, and environment variables.
- **Networking**: HTTP requests, DNS lookups, and connectivity tests.
- **Math**: Trigonometry, logarithms, and random number generation.
- **Collections**: Advanced list and map operations.
- **Text Processing**: Regex, string splitting, and formatting.
- **System**: Process management and system monitoring.

---

## 🧩 Extending the Library

Xell allows you to extend its functionality by creating your own modules. By using the `module` keyword and the `export` directive, you can build your own libraries and share them across projects using the `bring` statement.

**Example of a custom library structure:**
```
/my_project
  main.xel
  /lib
    math_ext.xel
    string_utils.xel
```
In `main.xel`:
```xell
bring * from "lib/math_ext.xel"
```

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Mathy Block](./Mathy Block.md)** guide to further your understanding of Xell.
