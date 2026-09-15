# Syntax Basics

> **Previous:** [Overview](overview.md)  
> **Next:** [Data Types](data-types.md)

---

## Lexical Structure

Xell’s syntax is designed to be clean and consistent. It borrows the best from Python’s readability, C‑style operators, and Ruby’s block‑like feel.

### Comments

Xell supports two comment styles:

#### Single‑line comments

Start with `#` and go to the end of the line:

```xell
# This is a comment
let x = 10  # also a comment after code
```

#### Multi‑line comments

Use `-->` to start and `<--` to end. They can span multiple lines:

```xell
--> This is a
    multi‑line comment
    that can be nested?
<--   # yes, it can
```

Multi‑line comments can be placed anywhere whitespace is allowed.

---

### Statement Termination

Xell uses **semicolons** (`;`) to end statements. This is explicit and unambiguous.

```xell
let x = 10 ;          # statement ends here
print(x) ;            # another statement
if x > 0 : print("positive") ;  # semicolon after if body
```

A semicolon is required after every block body (`:` … `;`). It also separates top‑level statements.

---

### Significant Newlines and Indentation

Unlike Python, **newlines are not significant** for block structure — Xell uses explicit `:` and `;` delimiters.  
However, newlines are still meaningful in the following ways:

- They separate statements when a semicolon is omitted at the end of a line (the parser inserts an implicit semicolon at newline *if* the line is a complete statement).  
- They are used as statement separators in the REPL and when reading from stdin.

**Recommendation:** Always use explicit `;` to avoid ambiguity.

#### Block syntax

A block starts with `:` and ends with `;`. It can contain multiple statements, each terminated by `;`.

```xell
if x > 0 :
    print("positive") ;
    let y = x * 2 ;
    print(y) ;
;   # end of if block
```

Indentation is not required but is strongly encouraged for readability.

---

### Identifiers

Identifiers name variables, functions, modules, etc.

- Must start with a letter (`a‑z`, `A‑Z`) or underscore (`_`).
- Subsequent characters can be letters, digits (`0‑9`), or underscores.
- Case‑sensitive: `foo` and `Foo` are different.

```xell
let myVar = 10
let _private = 20
let camelCase = 30
let snake_case = 40
```

Keywords (see [Keywords Reference](../reference/keywords.md)) cannot be used as identifiers.

---

### Literals

Xell has several literal forms:

#### Numbers

- **Integers**: `42`, `-100`, `0`, `0x1F` (hex), `0o77` (octal), `0b1010` (binary).  
  Underscores can be used as digit separators: `1_000_000`.

- **Floats**: `3.14`, `-0.001`, `1.2E10` (scientific notation).  
  Only uppercase `E` for exponent (lowercase `e` is Euler’s constant).

- **Imaginaries** (for complex numbers): `2i`, `3.14i`, `0xAi`.

#### Strings

- **Double‑quoted**: `"Hello"` – supports escape sequences (`\n`, `\t`, `\xHH`, `\uXXXX`, `\UXXXXXXXX`).
- **Single‑quoted**: `'Hello'` – same escapes.
- **Triple‑quoted**: `"""..."""` or `'''...'''` – preserve line breaks and indentation (dedented automatically).
- **Raw strings**: `r"..."` – backslashes are literal.
- **Byte strings**: `b"..."` – raw bytes, supports `\xHH` escapes.
- **F‑strings**: `f"..."` – interpolation with `{expr}` (see [F‑Strings](../advanced/f-strings.md)).

#### Booleans

- `true` and `false`

#### None

- `none` – null value.

#### Lists, Tuples, Sets, Maps

- **List**: `[1, 2, 3]`
- **Tuple**: `(1, 2, 3)` or `(1,)` for single element.
- **Set**: `{1, 2, 3}` (mutable)
- **Frozen set**: `<1, 2, 3>` (immutable)
- **Map**: `{ "key": "value", "answer": 42 }`

---

### Whitespace and Operator Parsing

Whitespace is generally ignored except as token separators. The parser follows the longest‑match rule for operators (e.g., `>>` is parsed as a single token, not two `>`).

Operators can be adjacent to identifiers and literals without spaces: `x+1` is parsed as `x + 1`.

---

## Next Steps

Now that you know the basic building blocks, proceed to **[Data Types](data-types.md)** for a detailed look at each type.

---

**Previous:** [Overview](overview.md)  
**Next:** [Data Types](data-types.md)