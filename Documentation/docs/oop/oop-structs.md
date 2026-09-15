# Structs

> **Previous:** [Built‑ins](../core/builtins.md)  
> **Next:** [Classes](oop-classes.md)

---

Structs are Xell's **value types** – lightweight data containers that group related fields together. Unlike classes, structs are **not** reference types and do **not** support inheritance. They are ideal for simple data aggregates where you want value semantics (copies are independent) and minimal overhead.

---

## Quick Reference

| Keyword/Syntax | What it does | Minimal example |
|----------------|--------------|-----------------|
| `struct Name : ... ;` | Defines a struct type | `struct Point : x = 0 y = 0 ;` |
| `field = expr` | Defines a field with a default value | `x = 0` |
| `fn method(...) : ... ;` | Defines a method on the struct | `fn distance(self) : give sqrt(self->x**2 + self->y**2) ;` |
| `Name(args...)` | Constructs an instance (positional) | `Point(10, 20)` |
| `Name({ field: value, ... })` | Constructs an instance (named) | `Point({ x: 10, y: 20 })` |
| `obj->field` | Accesses a field | `p->x` |
| `obj->method(args)` | Calls a method | `p->distance()` |

---

## Defining a Struct

Use the `struct` keyword, followed by the name, a colon, and a block of field definitions and methods.

```xell
struct Point :
    x = 0
    y = 0
;
```

- Fields are defined as `name = default_value`.
- Fields are **public** by default.
- The block ends with `;`.

---

## Fields

Fields are the data members of a struct. Each field must have a default value.

```xell
struct Rectangle :
    width = 1.0
    height = 1.0
    color = "black"
;
```

**Field access:**

```xell
let r = Rectangle()
print(r->width)     # 1.0
print(r->color)     # "black"
```

Fields can be accessed and modified:

```xell
r->width = 5.0
print(r->width)     # 5.0
```

---

## Methods

Methods are functions defined inside the struct body. They have access to the instance via the `self` parameter (by convention – the first parameter is always `self`).

```xell
struct Rectangle :
    width = 1.0
    height = 1.0

    fn area(self):
        give self->width * self->height
    ;

    fn scale(self, factor):
        self->width = self->width * factor
        self->height = self->height * factor
    ;
;
```

**Calling methods:**

```xell
let r = Rectangle()
r->scale(2.0)
print(r->area())   # 4.0
```

---

## Constructing Instances

### Positional Construction

Arguments are matched to fields in declaration order.

```xell
let p = Point(10, 20)
print(p->x)   # 10
print(p->y)   # 20
```

If you provide fewer arguments than fields, the remaining fields keep their default values:

```xell
let p = Point(5)      # y defaults to 0
print(p->x)   # 5
print(p->y)   # 0
```

### Named Construction

Use a map literal with field names and values. This is more explicit and order‑independent.

```xell
let p = Point({ x: 10, y: 20 })
let p2 = Point({ y: 30 })   # x defaults to 0
```

You can mix positional and named arguments (positional first, then named):

```xell
let p = Point(10, { y: 20 })   # x=10, y=20
```

> **Note:** Named construction uses a map literal, but the parser handles it specially – you don't need to call it as `Point({ x: 10 })`; the braces are optional inside the call parentheses. However, both forms work:
> ```xell
> let p = Point({ x: 10, y: 20 })   # explicit map
> let p = Point(x: 10, y: 20)       # shorthand
> ```

---

## Methods and `self`

The first parameter of any struct method is conventionally named `self`. It represents the instance on which the method is called.

```xell
fn area(self):
    give self->width * self->height
;
```

- `self` is automatically passed when you call `r->area()`.
- You can name the parameter anything, but `self` is idiomatic.
- `self` gives you access to the instance's fields via `->`.

---

## Methods with Additional Parameters

Methods can take additional parameters just like normal functions:

```xell
fn scale(self, factor):
    self->width = self->width * factor
    self->height = self->height * factor
;
```

```xell
let r = Rectangle(2.0, 3.0)
r->scale(2.0)
print(r->area())   # 24.0
```

---

## Complete Example

```xell
struct BankAccount :
    owner = "unknown"
    balance = 0.0

    fn deposit(self, amount):
        if amount <= 0 :
            throw ValueError("Deposit amount must be positive")
        self->balance += amount
    ;

    fn withdraw(self, amount):
        if amount <= 0 :
            throw ValueError("Withdrawal amount must be positive")
        if amount > self->balance :
            throw RuntimeError("Insufficient funds")
        self->balance -= amount
    ;

    fn get_balance(self):
        give self->balance
    ;
;

let acc = BankAccount({ owner: "Alice", balance: 100.0 })
acc->deposit(50.0)
print(acc->get_balance())   # 150.0

try :
    acc->withdraw(200.0)
catch e :
    print("Error: " + e->message)   # Error: Insufficient funds
;
```

**Expected output:**
```
150.0
Error: Insufficient funds
```

---

## Structs vs. Classes

| Feature | Struct | Class |
|---------|--------|-------|
| Type | Value type (copies are independent) | Reference type (shared) |
| Inheritance | ❌ Not supported | ✅ Supported |
| `__init__` | ❌ No constructor method | ✅ Supports `__init__` |
| `__str__`, `__print__` | ❌ Not supported | ✅ Supported |
| Access control | ❌ All fields public | ✅ `public`/`protected`/`private` |
| Methods | ✅ Supported | ✅ Supported |
| Default fields | ✅ Required | ✅ Optional (via `__init__`) |
| Use case | Data containers, value objects | Complex objects with behavior and identity |

**When to use structs:**
- Simple data aggregates (e.g., `Point`, `Rectangle`, `Config`)
- Where value semantics are desirable (copies don't affect each other)
- Where you don't need inheritance or polymorphism

**When to use classes:**
- When you need inheritance, mixins, or interfaces
- When you need reference semantics (shared state)
- When you need `__init__` logic or `__str__`/`__print__` customization

---

## Edge Cases and Less Obvious Behavior

### 1. Default Values Are Evaluated at Definition Time

Default values are evaluated **once** when the struct is defined, not per instance:

```xell
struct Counter :
    value = 0
    next = value + 1   # ❌ next = 1 for all instances
;
```

If you need per‑instance computed defaults, use a factory function or class with `__init__`.

### 2. Structs Are Not Clonable Automatically

When you assign a struct to a new variable, it is **copied**:

```xell
let a = Point(10, 20)
let b = a
b->x = 99
print(a->x)   # 10 (unchanged)
```

This is value semantics – each variable holds its own independent copy.

### 3. Methods Cannot Be Added After Definition

Unlike classes, structs do not support runtime method addition. All methods must be defined in the struct body.

### 4. Field Order Matters for Positional Construction

Positional arguments are matched in the order fields appear in the struct definition. Reordering fields changes the constructor's signature.

---

## What NOT to Do

### 1. Defining a struct without a default value for a field

```xell
struct Point :
    x   # ❌ no default value – parse error
;
```

**Why it fails:** Every field must have a default value. Use `x = 0` or `x = none` if needed.

### 2. Inheriting from a struct

```xell
struct ColoredPoint inherits Point :   # ❌ structs don't support inheritance
    color = "red"
;
```

**Why it fails:** Structs are value types and do not support inheritance. Use `class` for inheritance.

### 3. Using `__init__` in a struct

```xell
struct Point :
    x = 0
    y = 0
    fn __init__(self): ... ;   # ❌ no effect – __init__ is ignored
;
```

**Why it fails:** Structs do not have constructors. Use a factory function or `class` if you need initialization logic.

### 4. Calling a method without `self`

```xell
struct Math :
    fn add(a, b): give a + b ;   # ❌ missing `self` – a struct method must have self
;
```

**Why it fails:** All struct methods must have `self` as the first parameter. If you don't need `self`, define a standalone function instead.

### 5. Expecting `self` to be mutable in immutable bindings

```xell
immutable p = Point(10, 20)
p->x = 30   # ❌ p is immutable – cannot modify fields
```

**Why it fails:** `immutable` applies to the binding – you cannot modify the struct through an immutable binding. Use `let` if you need mutability.

---

## See Also

- [Classes](oop-classes.md) – for reference types with inheritance
- [Access Control](oop-access-control.md) – for `public`/`protected`/`private` (classes only)
- [Properties](oop-properties.md) – for getters/setters (classes only)
- [Inheritance](oop-inheritance.md) – for class inheritance
- [Magic Methods](oop-magic-methods.md) – for operator overloading (classes only)

---

**Previous:** [Built‑ins](../core/builtins.md)  
**Next:** [Classes](oop-classes.md)