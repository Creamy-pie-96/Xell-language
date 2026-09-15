# Variables & Binding

> **Previous:** [Data Types](data-types.md)  
> **Next:** [Operators](operators.md)

---

Xell provides two kinds of variable bindings: **mutable** (`let`) and **immutable** (`immutable`).  
Both are lexically scoped and support destructuring.

---

## `let` – Mutable Binding

Creates a variable that can be reassigned later.

```xell
let x = 10
x = 20          # reassignment is allowed
print(x)        # 20
```

Variables are **block‑scoped** – they exist only within the block where they are defined.

```xell
if true :
    let y = 42
;               # y goes out of scope here
# print(y)      # error – y is not defined
```

---

## `immutable` – Immutable Binding

Creates a variable that **cannot** be reassigned after its initial definition.

```xell
immutable PI = 3.14159
# PI = 3.14    # error – cannot reassign immutable variable
```

Immutable bindings are useful for constants and values that should never change.

```xell
immutable name = "Xell"
immutable answer = 42
```

> **Note:** Immutability applies to the **binding**, not the value. If the value is a mutable object (e.g., a list), you can still mutate the object itself:
> ```xell
> immutable list = [1, 2, 3]
> push(list, 4)   # allowed – mutates the list
> # list = []     # error – cannot reassign the binding
> ```

---

## Assignment

Assignment (`=`) updates an existing variable in the current or nearest enclosing scope. If the variable does not exist anywhere in the scope chain, it is created in the **current** scope.

```xell
let x = 10        # creates in current scope

fn outer():
    let y = 20
    fn inner():
        y = 30    # updates `y` in outer() scope (closure)
        z = 40    # creates `z` in inner() scope
    ;
;
```

### Augmented Assignment

Xell supports shorthand operators for modifying a variable in place:

| Operator | Meaning |
|----------|---------|
| `+=` | add and assign |
| `-=` | subtract and assign |
| `*=` | multiply and assign |
| `/=` | divide and assign |
| `%=` | modulo and assign |
| `**=` | exponentiate and assign |
| `&=` | bitwise AND and assign |
| `\|=` | bitwise OR and assign |
| `^=` | bitwise XOR and assign |
| `<<=` | left shift and assign |
| `>>=` | right shift and assign |

```xell
let count = 10
count += 5       # count = 15
count *= 2       # count = 30
count /= 3       # count = 10
```

---

## Destructuring Assignment

Xell supports unpacking values from lists, tuples, and maps into multiple variables.

### List / Tuple Destructuring

```xell
let [a, b, c] = [1, 2, 3]
print(a)    # 1
print(b)    # 2
print(c)    # 3

let (x, y) = (10, 20)
```

**Nested destructuring:**

```xell
let [first, [second, third]] = [1, [2, 3]]
print(second)   # 2
```

**Rest pattern (`...`):**

Captures remaining elements into a list.

```xell
let [head, ...tail] = [1, 2, 3, 4]
print(head)   # 1
print(tail)   # [2, 3, 4]
```

### Map / Object Destructuring

Unpacks fields from a map or instance into variables.

```xell
let point = { "x": 10, "y": 20 }
let { x, y } = point
print(x)    # 10
print(y)    # 20
```

**Renaming:**

```xell
let { x: xPos, y: yPos } = point
print(xPos)   # 10
```

**With instances:**

```xell
class Point :
    let x = 0
    let y = 0
;

let p = Point(10, 20)
let { x, y } = p
print(x)   # 10
```

**Discarding values:**

Use `_` to ignore a value.

```xell
let [a, _, c] = [1, 2, 3]
print(a)   # 1
print(c)   # 3
```

---

## Scope and Closures

Xell uses **lexical scoping**. Each block (`if`, `for`, `while`, `fn`, `module`, etc.) creates a new scope. Inner scopes can access variables from outer scopes (closures).

```xell
fn make_counter():
    let count = 0
    fn counter():
        count += 1
        give count
    ;
    give counter
;

let c = make_counter()
print(c())   # 1
print(c())   # 2
```

Variables are captured by reference; closures close over the variable itself, not its value.

---

## The `_` (Underscore) Variable

`_` is a special discard variable. It can be used in destructuring to ignore unwanted values and in `let … be` to discard a resource’s `__enter__` result.

```xell
let [_, value] = [1, 2]   # only `value` is bound
```

Unlike other variables, `_` can be reassigned freely and does not pollute the scope.

---

## Common Pitfalls

### Assignment vs. Declaration

```xell
let x = 10
x = 20      # assignment (updates existing)
y = 30      # creates `y` in current scope (if not exists)
```

If you intend to create a new variable, use `let` for clarity.

### Reassigning Immutable Variables

```xell
immutable NAME = "Xell"
NAME = "Other"   # error – cannot reassign immutable
```

### Scope Shadowing

```xell
let x = 10
fn shadow():
    let x = 20   # shadows outer x inside this function
    print(x)     # 20
;
print(x)         # 10 (outer x unchanged)
```

---

## Next Steps

Now that you understand variables and binding, explore **[Operators](operators.md)** to see how to manipulate values.

---

**Previous:** [Data Types](data-types.md)  
**Next:** [Operators](operators.md)