## ⬅️ What's Previous?

Move back to the **[Built-ins](./Built-ins.md)** to review the foundational concepts.

---

# Dialects

One of Xell's most unique features is its **Dialect System**. Xell allows you to change the keywords of the language to match another language's style or to create your own domain-specific language (DSL).

## 🌍 What is a Dialect?

A dialect is a mapping file (with the `.xesy` extension) that tells the Xell interpreter to replace certain "custom" keywords with "canonical" Xell keywords before executing the code.

For example, if you prefer `func` over `fn` or `return` over `give`, you can define a dialect that makes these changes automatically.

---

## 🛠️ Using a Dialect

To use a dialect in your script, add the `@convert` directive at the very top of your `.xel` file.

### Syntax
```xell
@convert "path/to/my_dialect.xesy"

# Now you can use keywords defined in my_dialect.xesy
func greet(name) :
    return "Hello, {name}!"
;
```

If you simply use `@convert` without a path, Xell will look for a default system dialect.

---

## 📝 Creating Your Own Dialect

A dialect file is a simple JSON object where the **keys** are canonical Xell keywords and the **values** are your custom replacements.

### Example `my_dialect.xesy`
```json
{
  "_meta": {
    "dialect_name": "My Friendly Dialect",
    "description": "Custom keyword mapping for Xell. Fill in values to map canonical keywords to your dialect."
  },

  "fn": "func",
  "give": "return",
  "if": "whenever",
  "while": "repeat_while",
  "bring": "import"
}
```

With this dialect, a standard Xell function:
```xell
fn add(a, b) :
    give a + b
;
```
...can be written as:
```xell
func add(a, b) :
    return a + b
;
```

---

## 🔄 Conversion Utilities

Xell provides CLI tools to help you manage dialects:

- **Convert to Dialect**: `xell --convert <file> [map.xesy]`
  Converts a canonical Xell file into a specific dialect in-place.
- **Revert to Canonical**: `xell --revert <file> [map.xesy]`
  Restores a dialect file back to standard canonical Xell.
- **Generate Template**: `xell --gen_xesy [output.xesy]`
  Generates a template `.xesy` file containing all available keywords for you to customize.

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Standard Library](./Standard Library.md)** guide to further your understanding of Xell.
