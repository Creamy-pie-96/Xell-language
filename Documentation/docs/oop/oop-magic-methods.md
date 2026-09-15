# Magic Methods

> **Previous:** [Properties](oop-properties.md)  
> **Next:** [Abstract Classes](oop-abstract.md)

---

Magic methods (also called **dunder methods**) are special methods that enable your classes to integrate with Xell's built‑in features and operators. They are identified by double underscores (`__`) at the start and end of their names. By implementing these methods, you can make your objects behave like built‑in types – supporting iteration, indexing, arithmetic, context management, and more.

Think of magic methods as **protocols** – they are Xell's way of saying "if your class has this method, it can do this thing."

---

## Quick Reference

| Magic Method | What it does | Minimal example |
|--------------|--------------|-----------------|
| `__init__(self, ...)` | Constructor | `fn __init__(self, name): self->name = name ;` |
| `__str__(self)` | String representation | `fn __str__(self): give self->name ;` |
| `__print__(self)` | Print representation | `fn __print__(self): print(self->name) ;` |
| `__len__(self)` | Length protocol | `fn __len__(self): give len(self->items) ;` |
| `__contains__(self, item)` | Membership protocol | `fn __contains__(self, item): give item in self->items ;` |
| `__iter__(self)` | Iteration protocol (returns iterator) | `fn __iter__(self): give self ;` |
| `__next__(self)` | Iterator protocol (returns next value) | `fn __next__(self): give next(self->items) ;` |
| `__call__(self, ...)` | Makes instance callable | `fn __call__(self, x): give x * 2 ;` |
| `__hash__(self)` | Hashing protocol | `fn __hash__(self): give hashme(self->id) ;` |
| `__enter__(self)` | Context manager entry | `fn __enter__(self): give self->handle ;` |
| `__exit__(self)` | Context manager exit | `fn __exit__(self): close(self->handle) ;` |
| `__add__(self, other)` | Addition (`+`) | `fn __add__(self, other): give Vector(self->x + other->x) ;` |
| `__sub__(self, other)` | Subtraction (`-`) | `fn __sub__(self, other): give Vector(self->x - other->x) ;` |
| `__mul__(self, other)` | Multiplication (`*`) | `fn __mul__(self, scalar): give Vector(self->x * scalar) ;` |
| `__div__(self, other)` | Division (`/`) | `fn __div__(self, other): give self->value / other ;` |
| `__mod__(self, other)` | Modulo (`%`) | `fn __mod__(self, other): give self->value % other ;` |
| `__pow__(self, other)` | Exponentiation (`**`) | `fn __pow__(self, exp): give self->value ** exp ;` |
| `__and__(self, other)` | Bitwise AND (`&`) | `fn __and__(self, other): give self->flags & other ;` |
| `__or__(self, other)` | Bitwise OR (`\|`) | `fn __or__(self, other): give self->flags \| other ;` |
| `__xor__(self, other)` | Bitwise XOR (`^`) | `fn __xor__(self, other): give self->flags ^ other ;` |
| `__lshift__(self, other)` | Left shift (`<<`) | `fn __lshift__(self, n): give self->value << n ;` |
| `__rshift__(self, other)` | Right shift (`>>`) | `fn __rshift__(self, n): give self->value >> n ;` |
| `__pipe__(self, other)` | Pipe (`\|>`) | `fn __pipe__(self, other): give self->value \|> other ;` |
| `__eq__(self, other)` | Equality (`==`) | `fn __eq__(self, other): give self->value == other ;` |
| `__ne__(self, other)` | Inequality (`!=`) | Derived from `__eq__` if not defined |
| `__lt__(self, other)` | Less than (`<`) | `fn __lt__(self, other): give self->value < other ;` |
| `__gt__(self, other)` | Greater than (`>`) | `fn __gt__(self, other): give self->value > other ;` |
| `__le__(self, other)` | Less than or equal (`<=`) | `fn __le__(self, other): give self->value <= other ;` |
| `__ge__(self, other)` | Greater than or equal (`>=`) | `fn __ge__(self, other): give self->value >= other ;` |
| `__neg__(self)` | Unary minus (`-x`) | `fn __neg__(self): give Vector(-self->x) ;` |
| `__invert__(self)` | Bitwise NOT (`~x`) | `fn __invert__(self): give ~self->value ;` |
| `__get__(self, key)` | Index get (`obj[key]`) | `fn __get__(self, key): give self->map[key] ;` |
| `__set__(self, key, val)` | Index set (`obj[key] = val`) | `fn __set__(self, key, val): self->map[key] = val ;` |

---

## Constructor: `__init__`

Called when an instance is created. See [Classes](oop-classes.md) for details.

```xell
class Person :
    name = ""
    age = 0

    fn __init__(self, name, age):
        self->name = name
        self->age = age
    ;
;

let p = Person("Alice", 30)
```

---

## String and Print Representations

### `__str__`

Called by `str()` and by `print()` when a user‑friendly string representation is needed.

```xell
class Person :
    name = ""
    age = 0

    fn __str__(self):
        give "Person(" + self->name + ", " + str(self->age) + ")"
    ;
;

let p = Person("Alice", 30)
print(str(p))   # Person(Alice, 30)
print(p)        # Person(Alice, 30)
```

### `__print__`

Called by `print()` if `__str__` is not defined, or for custom printing behavior.

```xell
class Logger :
    name = ""

    fn __print__(self):
        print("[LOGGER] " + self->name)
    ;
;

let l = Logger({ name: "app" })
print(l)   # [LOGGER] app
```

---

## Collection Protocols

### `__len__`

Called by `len()` to get the length of the object.

```xell
class Collection :
    items = []

    fn __len__(self):
        give len(self->items)
    ;
;

let c = Collection()
print(len(c))   # 0
push(c->items, 1)
print(len(c))   # 1
```

### `__contains__`

Called by the `in` operator to test membership.

```xell
class Bag :
    items = []

    fn __contains__(self, item):
        give contains(self->items, item)
    ;
;

let b = Bag({ items: [1, 2, 3] })
print(2 in b)   # true
print(5 in b)   # false
```

---

## Iteration Protocol: `__iter__` and `__next__`

### `__iter__`

Returns an iterator object – usually `self` if the object itself is its own iterator.

### `__next__`

Called by `for` loops and `next()` to get the next value. Should throw `"StopIteration"` (string) when exhausted.

```xell
class Range :
    start = 0
    end = 0
    current = 0

    fn __iter__(self):
        self->current = self->start
        give self
    ;

    fn __next__(self):
        if self->current >= self->end :
            throw "StopIteration"   # signals exhaustion
        let val = self->current
        self->current += 1
        give val
    ;
;

let r = Range({ start: 0, end: 5 })
for val in r:
    print(val)   # 0, 1, 2, 3, 4
;
```

**Expected output:**
```
0
1
2
3
4
```

---

## Callable Protocol: `__call__`

Makes an instance callable like a function.

```xell
class Multiplier :
    factor = 1

    fn __call__(self, x):
        give x * self->factor
    ;
;

let double = Multiplier({ factor: 2 })
print(double(5))   # 10
print(double(10))  # 20
```

---

## Hashing Protocol: `__hash__`

Called when an instance is used as a key in a map or element in a set. The instance **must be frozen** for `__hash__` to be called.

```xell
@immutable
class Point :
    x = 0
    y = 0

    fn __hash__(self):
        give hashme(self->x) ^ hashme(self->y)
    ;
;

let p = ~Point(10, 20)
let s = set([p])   # works because p is frozen and has __hash__
print(contains(s, p))   # true
```

---

## Context Manager Protocol: `__enter__` and `__exit__`

See [RAII](../core/raii.md) for full details.

```xell
class File :
    path = ""
    handle = none

    fn __enter__(self):
        self->handle = open(self->path, "r")
        give self->handle
    ;

    fn __exit__(self):
        if self->handle :
            close(self->handle)
    ;
;

let f be file = File({ path: "data.txt" }) :
    let content = file->read()
    print(content)
;
# file is automatically closed
```

---

## Arithmetic Operator Overloading

Define arithmetic operators to make your objects behave like numeric types.

### `__add__` – Addition (`+`)

```xell
class Vector :
    x = 0.0
    y = 0.0

    fn __add__(self, other):
        give Vector(self->x + other->x, self->y + other->y)
    ;
;

let v1 = Vector(1, 2)
let v2 = Vector(3, 4)
let v3 = v1 + v2   # Vector(4, 6)
print(v3->x)       # 4
print(v3->y)       # 6
```

### `__sub__` – Subtraction (`-`)

```xell
fn __sub__(self, other):
    give Vector(self->x - other->x, self->y - other->y)
;
```

### `__mul__` – Multiplication (`*`)

```xell
fn __mul__(self, other):
    if other is int or other is float :
        give Vector(self->x * other, self->y * other)
    else :
        throw TypeError("Vector * requires scalar")
;
```

### `__div__` – Division (`/`)

```xell
fn __div__(self, other):
    if other == 0 :
        throw DivisionByZeroError(0)
    give Vector(self->x / other, self->y / other)
;
```

### `__mod__` – Modulo (`%`)

```xell
fn __mod__(self, other):
    give self->value % other
;
```

### `__pow__` – Exponentiation (`**`)

```xell
fn __pow__(self, exp):
    give Vector(self->x ** exp, self->y ** exp)
;
```

---

## Bitwise Operator Overloading

### `__and__` – Bitwise AND (`&`)

```xell
class Flags :
    value = 0

    fn __and__(self, other):
        give Flags(self->value & other)
    ;
;
```

### `__or__` – Bitwise OR (`|`)

```xell
fn __or__(self, other):
    give Flags(self->value | other)
;
```

### `__xor__` – Bitwise XOR (`^`)

```xell
fn __xor__(self, other):
    give Flags(self->value ^ other)
;
```

### `__lshift__` – Left Shift (`<<`)

```xell
fn __lshift__(self, n):
    give Flags(self->value << n)
;
```

### `__rshift__` – Right Shift (`>>`)

```xell
fn __rshift__(self, n):
    give Flags(self->value >> n)
;
```

### `__invert__` – Bitwise NOT (`~`)

```xell
fn __invert__(self):
    give Flags(~self->value)
;
```

---

## Pipe Operator: `__pipe__`

Called by the `|>` operator.

```xell
class Command :
    value = ""

    fn __pipe__(self, other):
        give Command(self->value + " | " + other->value)
    ;
;

let cmd1 = Command({ value: "ls" })
let cmd2 = Command({ value: "grep xel" })
let cmd3 = cmd1 |> cmd2
print(cmd3->value)   # "ls | grep xel"
```

---

## Comparison Operator Overloading

### `__eq__` – Equality (`==`)

```xell
class Vector :
    x = 0.0
    y = 0.0

    fn __eq__(self, other):
        if not other is Vector :
            give false
        give self->x == other->x and self->y == other->y
    ;
;

let v1 = Vector(1, 2)
let v2 = Vector(1, 2)
print(v1 == v2)   # true
```

### `__ne__` – Inequality (`!=`)

If `__ne__` is not defined, Xell derives it by negating `__eq__`.

```xell
# __ne__ is automatically derived from __eq__
print(v1 != v2)   # false
```

### `__lt__` – Less Than (`<`)

```xell
fn __lt__(self, other):
    give self->value < other->value
;
```

### `__gt__` – Greater Than (`>`)

```xell
fn __gt__(self, other):
    give self->value > other->value
;
```

### `__le__` – Less Than or Equal (`<=`)

```xell
fn __le__(self, other):
    give self->value <= other->value
;
```

### `__ge__` – Greater Than or Equal (`>=`)

```xell
fn __ge__(self, other):
    give self->value >= other->value
;
```

---

## Unary Operator Overloading

### `__neg__` – Unary Minus (`-x`)

```xell
class Vector :
    x = 0.0
    y = 0.0

    fn __neg__(self):
        give Vector(-self->x, -self->y)
    ;
;

let v = Vector(1, 2)
let neg = -v
print(neg->x)   # -1
print(neg->y)   # -2
```

### `__invert__` – Bitwise NOT (`~x`)

```xell
fn __invert__(self):
    give ~self->value
;
```

---

## Index/Subscript Protocol: `__get__` and `__set__`

### `__get__` – Index Get (`obj[key]`)

```xell
class Dictionary :
    data = {}

    fn __get__(self, key):
        if key in self->data :
            give self->data[key]
        else :
            throw KeyError("Key not found: " + str(key))
    ;
;

let d = Dictionary({ data: { "a": 1 } })
print(d["a"])   # 1
# print(d["b"]) # ❌ throws KeyError
```

### `__set__` – Index Set (`obj[key] = val`)

```xell
fn __set__(self, key, val):
    self->data[key] = val
;

d["b"] = 2
print(d["b"])   # 2
```

---

## Complete Example: Custom Vector Class

```xell
class Vector :
    x = 0.0
    y = 0.0

    fn __init__(self, x, y):
        self->x = x
        self->y = y
    ;

    fn __str__(self):
        give "Vector(" + str(self->x) + ", " + str(self->y) + ")"
    ;

    fn __add__(self, other):
        if not other is Vector :
            throw TypeError("Vector + requires Vector")
        give Vector(self->x + other->x, self->y + other->y)
    ;

    fn __sub__(self, other):
        if not other is Vector :
            throw TypeError("Vector - requires Vector")
        give Vector(self->x - other->x, self->y - other->y)
    ;

    fn __mul__(self, scalar):
        if not (scalar is int or scalar is float) :
            throw TypeError("Vector * requires scalar")
        give Vector(self->x * scalar, self->y * scalar)
    ;

    fn __eq__(self, other):
        if not other is Vector :
            give false
        give self->x == other->x and self->y == other->y
    ;

    fn __neg__(self):
        give Vector(-self->x, -self->y)
    ;

    get magnitude(self):
        give sqrt(self->x**2 + self->y**2)
    ;
;

let v1 = Vector(1, 2)
let v2 = Vector(3, 4)

print(v1 + v2)   # Vector(4, 6)
print(v2 - v1)   # Vector(2, 2)
print(v1 * 3)    # Vector(3, 6)
print(v1 == Vector(1, 2))   # true
print(-v1)       # Vector(-1, -2)
print(v1->magnitude)   # 2.236...
```

**Expected output:**
```
Vector(4, 6)
Vector(2, 2)
Vector(3, 6)
true
Vector(-1, -2)
2.23606797749979
```

---

## Edge Cases and Less Obvious Behavior

### 1. `__ne__` Derivation

If you define `__eq__` but not `__ne__`, `!=` is automatically derived as the negation of `==`.

```xell
class Point :
    x = 0
    y = 0
    fn __eq__(self, other): give self->x == other->x and self->y == other->y ;
;

let p1 = Point(1, 2)
let p2 = Point(1, 3)
print(p1 != p2)   # true (derived from __eq__)
```

### 2. `__hash__` Requires Frozen Instance

`__hash__` is only called when the instance is **frozen** (`frozen = true`). Use `~` prefix or `@immutable` to freeze.

### 3. `__iter__` Should Return an Iterator

If your object is its own iterator (returns `self`), it must have `__next__`.

```xell
class Counter :
    value = 0
    max = 10

    fn __iter__(self):
        self->value = 0
        give self
    ;

    fn __next__(self):
        if self->value >= self->max :
            throw "StopIteration"
        let v = self->value
        self->value += 1
        give v
    ;
;
```

### 4. `__enter__` Can Return Any Value

The value returned by `__enter__` is bound to the `let … be` variable.

```xell
class Resource :
    fn __enter__(self):
        give "resource handle"
    ;
    fn __exit__(self): pass ;
;

let r be res = Resource() :
    print(res)   # "resource handle"
;
```

### 5. `__exit__` Return Value Is Ignored

Xell does not inspect the return value of `__exit__`. It is called purely for side effects.

### 6. Magic Methods on Structs

Structs do **not** support magic methods (except methods in general). Use classes for magic method support.

```xell
struct Point :
    x = 0
    y = 0
    fn __add__(self, other): ...   # ❌ ignored – structs don't support magic methods
;
```

---

## What NOT to Do

### 1. Defining Magic Methods Outside a Class

```xell
fn __add__(x, y): give x + y   # ❌ magic methods only work inside classes
```

### 2. Returning the Wrong Type from Magic Methods

```xell
fn __add__(self, other):
    give "string"   # ❌ should return a Vector or appropriate type
;
```

### 3. Forgetting `self` in Magic Methods

```xell
fn __add__(x, y):   # ❌ missing self – should be (self, other)
    give x + y
;
```

### 4. Using `__hash__` Without Freezing

```xell
class Bad :
    fn __hash__(self): give 42 ;

let s = set([Bad()])   # ❌ error – __hash__ requires frozen instance
```

### 5. Creating Infinite Recursion in Magic Methods

```xell
fn __add__(self, other):
    give self + other   # ❌ infinite recursion
;
```

**Fix:** Use the underlying fields.

```xell
fn __add__(self, other):
    give Vector(self->x + other->x)
;
```

### 6. Implementing `__next__` Without `__iter__`

```xell
class Bad :
    fn __next__(self): ...   # ❌ __next__ alone doesn't make an iterator
;
```

**Fix:** Implement `__iter__` as well.

### 7. Throwing Anything Other Than `"StopIteration"` for Exhaustion

```xell
fn __next__(self):
    if exhausted :
        throw 42   # ❌ should throw "StopIteration"
;
```

### 8. Assuming Magic Methods Are Called for Built‑in Types

Magic methods are only called on instances of user‑defined classes. Built‑in types use their own internal implementations.

---

## See Also

- [Classes](oop-classes.md) – where magic methods live
- [RAII](../core/raii.md) – `__enter__`/`__exit__` in detail
- [Structs](oop-structs.md) – why structs don't have magic methods
- [Operators](../core/operators.md) – how magic methods integrate with operators
- [Generators](../core/generators.md) – `__iter__`/`__next__` with generators

---

**Previous:** [Properties](oop-properties.md)  
**Next:** [Abstract Classes](oop-abstract.md)