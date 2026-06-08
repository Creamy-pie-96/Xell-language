## ⬅️ What's Previous?

Move back to the **[Introduction](./Introduction.md)** to review the foundational concepts.

---

# Grammar & Syntax

This guide covers the fundamental rules of the Xell language, including how code is structured, how statements are terminated, and how to use comments.

## 🏗️ Code Structure

Xell is designed to be clean and readable. It uses a combination of newline-based termination and explicit scope delimiters.

### Statements and Termination
In Xell, a statement is a single unit of execution. There are two ways to terminate a statement:

1.  **Implicit Termination (Newline)**: By default, a newline ends a statement. This makes the code look clean and similar to Python or Bash.
    ```xell
    name = "Xell"
    print name
    ```

2.  **Explicit Termination (The Dot)**: You can use a dot `.` to explicitly terminate a statement. This is useful if you want to put multiple statements on a single line.
    ```xell
    mkdir "logs" . print "Logs created"
    ```

### Scopes and Blocks
Unlike languages that use curly braces `{ }` or strict indentation for blocks, Xell uses a specific pair of delimiters:
- **`:` (Colon)**: Opens a scope/block.
- **`;` (Semicolon)**: Closes a scope/block.

This is used with control flow statements (`if`, `for`, `while`) and function definitions (`fn`).

**Example:**
```xell
if true :
    print "This is inside the scope"
    print "Still inside"
;
print "Now we are outside the scope"
```

---

## ⚪ Whitespace

- **Indentation**: While Xell doesn't *require* indentation for logic (since it uses `;` to close blocks), it is highly recommended for readability.
- **Horizontal Space**: Spaces and tabs are generally ignored except when separating tokens (e.g., between a keyword and a variable name).
- **Newlines**: As mentioned, newlines are significant and act as the primary statement terminator.

---

## 📝 Comments

Comments are ignored by the interpreter and are used to document your code.

### Single-line Comments
Use the hash symbol `#` to start a single-line comment. Everything from the `#` to the end of the line is ignored.

```xell
# This is a full-line comment
print "Hello" # This is an inline comment
```

### Multi-line Comments
For longer descriptions or temporarily disabling blocks of code, use the `-->` and `<--` delimiters.

```xell
-->
This is a multi-line comment.
It can span as many lines
as you need.
<--
```

---

## 🔍 Summary Table

| Feature | Symbol | Note |
| :--- | :--- | :--- |
| **Statement End** | `\n` or `.` | Newline is default; dot is explicit. |
| **Open Scope** | `:` | Starts a block. |
| **Close Scope** | `;` | Ends a block. |
| **Single Comment** | `#` | Until end of line. |
| **Multi Comment** | `--> ... <--` | Spans multiple lines. |

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Variables](./Variables.md)** guide to further your understanding of Xell.
