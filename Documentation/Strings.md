## ⬅️ What's Previous?

Move back to the **[Arrays & Collections](./Arrays & Collections.md)** to review the foundational concepts.

---

# Strings

Strings in Xell are used to represent text. They are immutable and support powerful features like interpolation and a wide array of built-in manipulation functions.

## 📝 String Literals

Strings must be enclosed in **double quotes** `"`. Single quotes are not supported for string literals.

```xell
name = "Xell Language"
message = "Welcome to the world of automation!"
```

---

## 🪄 String Interpolation

Xell supports interpolation, allowing you to embed expressions directly inside a string using curly braces `{}`. The expression inside the braces is evaluated, and its result is converted to a string and inserted.

```xell
user = "Alice"
age = 25
greeting = "Hello, {user}! You are {age} years old."
print greeting # Output: Hello, Alice! You are 25 years old.
```

---

## ⌨️ Escape Sequences

To include special characters (like newlines or quotes) inside a string, use the backslash `\` escape character.

| Escape | Meaning | Example |
| :--- | :--- | :--- |
| `\\n` | Newline | `"Line one\\nLine two"` |
| `\\t` | Tab | `"Column1\\tColumn2"` |
| `\\\\` | Backslash | `"C:\\Windows"` |
| `\\\"` | Double Quote | `"He said, \"Hello!\""` |

---

## 🛠️ String Manipulation

Xell provides a comprehensive set of built-in functions for processing text.

### Transformation
| Function | Description | Example |
| :--- | :--- | :--- |
| `upper(str)` | Converts string to uppercase. | `upper("hello")` $\rightarrow$ `"HELLO"` |
| `lower(str)` | Converts string to lowercase. | `lower("HELLO")` $\rightarrow$ `"hello"` |
| `trim(str)` | Removes whitespace from both ends. | `trim("  hi  ")` $\rightarrow$ `"hi"` |
| `trim_start(str)`| Removes leading whitespace. | `trim_start("  hi ")` $\rightarrow$ `"hi "` |
| `trim_end(str)` | Removes trailing whitespace. | `trim_end("  hi ")` $\rightarrow$ `"  hi"` |
| `reverse(str)` | Reverses the string. | `reverse("abc")` $\rightarrow$ `"cba"` |

### Search & Replace
| Function | Description | Example |
| :--- | :--- | :--- |
| `replace(str, old, new)`| Replaces all occurrences of `old` with `new`. | `replace("aa", "a", "b")` $\rightarrow$ `"bb"` |
| `replace_first(str, o, n)`| Replaces only the first occurrence. | `replace_first("aa", "a", "b")` $\rightarrow$ `"ba"` |
| `contains(str, sub)` | Returns `true` if `sub` is in `str`. | `contains("hello", "ell")` $\rightarrow$ `true` |
| `index_of(str, sub)` | Returns index of first occurrence of `sub`. | `index_of("hello", "l")` $\rightarrow$ `2` |

### Splitting & Joining
| Function | Description | Example |
| :--- | :--- | :--- |
| `split(str, sep)` | Splits string into a list using `sep`. | `split("a,b,c", ",")` $\rightarrow$ `["a", "b", "c"]` |
| `join(list, sep)` | Joins a list into a string using `sep`. | `join(["a", "b"], "-")` $\rightarrow$ `"a-b"` |

### Utility
| Function | Description | Example |
| :--- | :--- | :--- |
| `len(str)` | Returns the length of the string. | `len("hello")` $\rightarrow$ `5` |
| `is_numeric(str)` | Returns `true` if string contains only numbers. | `is_numeric("123")` $\rightarrow$ `true` |
| `is_alpha(str)` | Returns `true` if string contains only letters. | `is_alpha("abc")` $\rightarrow$ `true` |
| `repeat(str, n)` | Repeats the string `n` times. | `repeat("!", 3)` $\rightarrow$ `"!!!"` |

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Classes & Objects](./Classes & Objects.md)** guide to further your understanding of Xell.
