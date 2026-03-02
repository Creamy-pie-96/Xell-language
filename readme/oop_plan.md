# OOP Integration Plan for Xell

> **Status:** Confirmed Design — ready for implementation.
> **Date:** 2026
> **Note:** This is a full-featured, pragmatic OOP layer for Xell. It preserves
> Xell's crystal-clear syntax while supporting structs, classes, interfaces,
> abstract classes, mixins, decorators, access control, static members,
> properties, operator overloading, and function overloading.

---

## Table of Contents

1. [Design Philosophy](#1-design-philosophy)
2. [Phase 1: Structs](#2-phase-1-structs)
3. [Phase 2: Classes](#3-phase-2-classes)
4. [Phase 3: Inheritance & Interfaces](#4-phase-3-inheritance--interfaces)
5. [Phase 4: Access Control](#5-phase-4-access-control)
6. [Phase 5: Static Members](#6-phase-5-static-members)
7. [Phase 6: Properties (get / set)](#7-phase-6-properties-get--set)
8. [Phase 7: Interfaces](#8-phase-7-interfaces)
9. [Phase 8: Abstract Classes](#9-phase-8-abstract-classes)
10. [Phase 9: Mixins](#10-phase-9-mixins)
11. [Phase 10: Decorators](#11-phase-10-decorators)
12. [Phase 11: Magic Methods](#12-phase-11-magic-methods)
13. [Phase 12: Operator Overloading](#13-phase-12-operator-overloading)
14. [Phase 13: Function Overloading](#14-phase-13-function-overloading)
15. [Summary: Complete Syntax Reference](#15-summary-complete-syntax-reference)
16. [Implementation Roadmap](#16-implementation-roadmap)
17. [Appendix A: Method & Field Access (`->` and `of`)](#17-appendix-a-method--field-access---and-of)
18. [Appendix B: `__hash__` and Built-in Hash Functions](#18-appendix-b-__hash__-and-built-in-hash-functions)
19. [Appendix C: Lambda Syntax Reference](#19-appendix-c-lambda-syntax-reference)
20. [Appendix D: What Changed from Previous Plan](#20-appendix-d-what-changed-from-previous-plan)
21. [Appendix E: What We're NOT Adding](#21-appendix-e-what-were-not-adding)

---

## 1. Design Philosophy

Xell's OOP should be:

- **Explicit** — no hidden `this`. `self` is always the first parameter. No magic unless you define it.
- **Layered** — structs for data, classes for behavior, interfaces for contracts, mixins for reuse.
- **Consistent** — uses the same `:` / `;` block syntax as everything else in Xell.
- **`->` for access** — field and method access always uses `->`. The `of` keyword is an alias for natural reading.
- **`__dunder__` for magic** — all special behavior uses `__name__` style, clearly opt-in.
- **Type-aware overloading** — functions and methods can be overloaded by argument count and type.

**Guiding principle:** Structs are for named data. Classes are for encapsulated behavior. If you
only need fields + a few helpers, use a struct. If you need constructors, inheritance, or contracts,
use a class. If you need shared behavior across unrelated classes, use a mixin.

---

## 2. Phase 1: Structs

Structs are **lightweight named data containers** that also support methods. Think C structs or
Python's `@dataclass`, but with optional method definitions.

### Syntax

```xell
struct Point :
    x = 0
    y = 0

    fn distance(self, other) :
        give sqrt(pow(self->x - other->x, 2) + pow(self->y - other->y, 2))
    ;
;

# Create instances
p1 = Point()                   # defaults: x=0, y=0
p2 = Point(3, 7)               # positional: x=3, y=7
p3 = Point(x: 10, y: 20)      # named with colon syntax

# Access fields
print(p1->x)                   # 0
print(p2->y)                   # 7

# Mutate fields
p1->x = 42

# Methods work too
d = p2->distance(p3)

# Identity
typeof(p1)                     # "Point"
print(p1)                      # Point(x=42, y=0)
```

### Key Decisions

| Feature                 | Decision                       | Rationale                               |
| ----------------------- | ------------------------------ | --------------------------------------- |
| Field access            | `->` (arrow)                   | Already used for map key access in Xell |
| Named arg syntax        | `Point(x: 10, y: 20)`          | Colon separates key from value          |
| Default values          | Required for all fields        | No uninitialized fields ever            |
| Construction            | `Name(args)`                   | Same as function call syntax            |
| Positional + named args | Both supported                 | Flexible                                |
| Mutability              | Mutable by default             | Consistent with Xell variables          |
| Methods in structs      | ✅ Allowed                     | Structs are full-featured, not data-only |
| Printing                | `__print__` or auto-format     | `StructName(field1=val1, ...)`          |
| `typeof`                | Returns struct name            | `typeof(p)` → `"Point"`                 |
| Equality                | Field-by-field comparison      | `p1 == p2` if all fields match          |
| Constructor in struct   | ❌ — use `class` for that      | Keeps structs simple                    |
| Inheritance in struct   | ❌ — use `class` for that      | Keeps structs simple                    |

### Grammar Extension

```
STRUCT_DEF  := "struct" IDENTIFIER ':' { STRUCT_BODY } ';'
STRUCT_BODY := FIELD_DEF | FN_DEF
FIELD_DEF   := IDENTIFIER '=' EXPRESSION
NAMED_ARG   := IDENTIFIER ':' EXPRESSION
```

### Internal Representation

```
XObject
├── type_       = STRUCT
├── structName_ = "Point"
└── fields_     = {"x": XObject(42), "y": XObject(0)}
```

---

## 3. Phase 2: Classes

Classes are the full OOP unit: fields + constructor + methods + optional inheritance.

### Constructor — `__init__`

The constructor is always named `__init__`. `self` is the explicit first parameter.

```xell
class Animal :
    name = ""
    sound = ""
    legs = 4

    fn __init__(self, name, sound) :
        self->name = name
        self->sound = sound
    ;

    fn speak(self) :
        give "{self->name} says {self->sound}!"
    ;

    fn describe(self) :
        give "{self->name}: {self->legs} legs"
    ;
;

dog = Animal("Rex", "Woof")
print(dog->speak())            # Rex says Woof!
print(dog->legs)               # 4 (from default)
```

### class vs struct

| Feature                   | `struct`  | `class`   |
| ------------------------- | --------- | --------- |
| Fields                    | ✅        | ✅        |
| Default values            | Required  | Required  |
| Methods                   | ✅        | ✅        |
| Named arg construction    | ✅        | ✅        |
| Constructor (`__init__`)  | ❌        | ✅        |
| Inheritance (`inherits`)  | ❌        | ✅        |
| Implements (`implements`) | ❌        | ✅        |
| Access control            | ❌        | ✅        |
| Static members            | ❌        | ✅        |
| Properties (get/set)      | ❌        | ✅        |
| Decorators                | ✅        | ✅        |
| Operator overloading      | ✅        | ✅        |

**Rule:** No `__init__`, no inheritance, no access control needed? Use a struct. Otherwise, use a class.

---

## 4. Phase 3: Inheritance & Interfaces

### Inheritance — `inherits`, multiple allowed

```xell
class Dog inherits Animal :
    breed = ""

    fn __init__(self, name, breed) :
        parent.__init__(name, "Woof")
        self->breed = breed
    ;

    fn speak(self) :
        give "{self->name} the {self->breed} says Woof!"
    ;

    fn fetch(self) :
        give "{self->name} fetches the ball!"
    ;
;
```

### Multiple inheritance

```xell
class Circle inherits Shape, Geometry implements Drawable, Serializable :
    fn __init__(self, radius) :
        self->_radius = radius
    ;
;
```

### Parent calls — `parent` keyword

```xell
fn __init__(self, name) :
    parent.__init__(name, "Woof")    # calls Animal.__init__(self, name, "Woof")
;

fn speak(self) :
    parent.speak()                   # call parent's version first
    print "and also fetches!"
;
```

### Key Decisions

| Feature                  | Decision                           | Rationale                                            |
| ------------------------ | ---------------------------------- | ---------------------------------------------------- |
| Inheritance keyword      | `inherits`                         | Clear, unambiguous keyword                           |
| Multiple inheritance     | ✅ Allowed                          | `inherits A, B`                                      |
| Interface implementation | `implements`                       | Separate from inheritance — distinct concept         |
| Parent call keyword      | `parent`                           | Explicit — no magic method resolution                |
| Parent call syntax       | `parent.__init__(args)`            | Dot notation — `self` is implicit                    |
| `is` check               | Works with full inheritance chain  | `d is Animal` → `true`                               |
| Method override          | Implicit (just redefine)           | No `override` keyword needed                         |

### Grammar Extension

```
CLASS_DEF    := "class" IDENTIFIER
               [ "inherits" IDENTIFIER { "," IDENTIFIER } ]
               [ "implements" IDENTIFIER { "," IDENTIFIER } ]
               ':' { CLASS_BODY } ';'
CLASS_BODY   := FIELD_DEF | FN_DEF | ACCESS_BLOCK | STATIC_DEF | PROPERTY_DEF
```

### Method Resolution Order (MRO)

For multiple inheritance, left-to-right C3 linearization:

1. Look in the instance's own class
2. Left-most parent first, depth first
3. Continue up to the root

---

## 5. Phase 4: Access Control

Access control uses **C++ block-style** sections inside a class body.

```xell
class BankAccount :
    fn __init__(self, owner) :
        self->owner = owner
    ;

    private:
        balance = 0
        secret = "xyz"

    protected:
        internal_id = 42

    public:
        fn deposit(self, amount) :
            self->balance += amount
        ;

        fn get_balance(self) :
            give self->balance
        ;
;

acc = BankAccount("Alice")
acc->deposit(500)
print(acc->get_balance())      # 500
print(acc->balance)            # Error: 'balance' is private
print(acc->internal_id)        # Error: 'internal_id' is protected
```

### Key Decisions

| Modifier    | Accessible from                         |
| ----------- | --------------------------------------- |
| `public`    | Anywhere (default if no block declared) |
| `protected` | This class and subclasses               |
| `private`   | This class only                         |

- All fields/methods before any access block are **public** by default.
- Access blocks apply to everything listed under them until the next block or `;`.
- Applies to both fields and methods.
- Subclasses can access `protected` members but not `private` ones.

### Grammar Extension

```
ACCESS_BLOCK := ("private" | "protected" | "public") ':' { FIELD_DEF | FN_DEF }
```

---

## 6. Phase 5: Static Members

Static fields and methods belong to the **class**, not instances.

```xell
class MathHelper :
    static PI = 3.14159

    static fn circle_area(r) :
        give MathHelper.PI * r * r
    ;

    static fn square(x) :
        give x * x
    ;
;

print(MathHelper.PI)                  # 3.14159
print(MathHelper->circle_area(5))     # 78.53975
```

### Key Decisions

| Feature               | Decision                      | Rationale                              |
| --------------------- | ----------------------------- | -------------------------------------- |
| Keyword               | `static`                      | Universal, familiar                    |
| Static field access   | `ClassName.field`             | Dot notation — no instance needed      |
| Static method access  | `ClassName->method(args)`     | Arrow for method calls, dot for fields |
| `self` in static      | ❌ Not present                 | Static has no instance                 |
| Static in structs     | ❌ Not supported               | Use class for static members           |
| Inheritance of static | Inherited but not overridden  | Class-level constants                  |

---

## 7. Phase 6: Properties (get / set)

Properties intercept field reads and writes with custom logic.

```xell
class Circle :
    fn __init__(self, radius) :
        self->_radius = radius
    ;

    get radius(self) :
        give self->_radius
    ;

    set radius(self, val) :
        if val < 0 : error("negative radius!") ;
        self->_radius = val
    ;
;

c = Circle(5)
print(c->radius)               # 5         — calls get
c->radius = 10                 # calls set
c->radius = -1                 # Error: negative radius!
print(radius of c)             # 10        — same as c->radius
```

### Key Decisions

| Feature            | Decision                          | Rationale                            |
| ------------------ | --------------------------------- | ------------------------------------ |
| Getter syntax      | `get name(self) : ... ;`          | Reads naturally                      |
| Setter syntax      | `set name(self, val) : ... ;`     | Matches getter style                 |
| Access syntax      | `obj->name` / `obj->name = val`   | Transparent — same as field access   |
| Read-only property | Define `get` without `set`        | Attempting to write raises error     |
| Write-only         | Define `set` without `get`        | Attempting to read raises error      |
| Backing field      | Convention: `_name`               | No special syntax for backing fields |

---

## 8. Phase 7: Interfaces

Interfaces declare a **contract**: a set of methods a class must implement. All methods in an
interface are abstract (no body).

```xell
interface Drawable :
    fn draw(self) ;
    fn resize(self, factor) ;
;

interface Serializable :
    fn to_json(self) ;
    fn from_json(self, data) ;
;
```

A class implementing an interface **must** define all declared methods, or a compile/runtime error
is raised.

```xell
class Canvas implements Drawable :
    fn __init__(self, width, height) :
        self->width = width
        self->height = height
    ;

    fn draw(self) :
        print "drawing {self->width}x{self->height} canvas"
    ;

    fn resize(self, factor) :
        self->width  = self->width * factor
        self->height = self->height * factor
    ;
;
```

### Key Decisions

| Feature                  | Decision                              |
| ------------------------ | ------------------------------------- |
| Keyword                  | `interface`                           |
| Method bodies            | ❌ None — declaration only             |
| Implementation keyword   | `implements`                          |
| Multiple interfaces      | ✅ `implements A, B, C`               |
| Interface constants      | ❌ Not supported                       |
| Default implementations  | ❌ Use abstract class for that         |
| Interface inheritance    | ❌ Not planned                         |

---

## 9. Phase 8: Abstract Classes

Abstract classes are a **mix**: some methods have implementations (defaults), others are abstract
(no body, must be overridden).

```xell
abstract Shape :
    fn area(self) ;               # abstract — must implement
    fn perimeter(self) ;          # abstract — must implement

    fn color(self) :              # default — can override
        give "white"
    ;

    fn describe(self) :           # default — can override
        give "Shape: area={self->area()}, color={self->color()}"
    ;
;

class Rectangle inherits Shape :
    fn __init__(self, w, h) :
        self->w = w
        self->h = h
    ;

    fn area(self) :
        give self->w * self->h
    ;

    fn perimeter(self) :
        give 2 * (self->w + self->h)
    ;
;

r = Rectangle(4, 5)
print(r->area())               # 20
print(r->color())              # white    — inherited default
print(r->describe())           # Shape: area=20, color=white
```

### Key Decisions

| Feature                                      | Decision                                     |
| -------------------------------------------- | -------------------------------------------- |
| Keyword                                      | `abstract`                                   |
| Abstract method                              | `fn method(self) ;` — no body, ends with `;` |
| Default method                               | `fn method(self) : ... ;` — has a body       |
| Can be instantiated directly                 | ❌ Error if you try                           |
| Subclass must implement all abstract methods | ✅ Enforced                                   |
| Can `inherits` abstract                      | ✅                                            |
| Can `implements` interface                   | ✅                                            |

---

## 10. Phase 9: Mixins

Mixins provide **reusable method bundles** that can be mixed into any class using `with`.
Mixins have no constructor and cannot be instantiated.

```xell
mixin Serializable :
    fn to_json(self) :
        bring * from "json"
        give json_stringify(self)
    ;

    fn from_json(self, data) :
        bring * from "json"
        give json_parse(data)
    ;
;

mixin Loggable :
    fn log(self, msg) :
        print "[{typeof(self)}] {msg}"
    ;
;

class User with Serializable, Loggable :
    fn __init__(self, name) :
        self->name = name
    ;
;

u = User("Alice")
print(u->to_json())            # {"name": "Alice"}
u->log("created")              # [User] created
```

### Key Decisions

| Feature                  | Decision                               |
| ------------------------ | -------------------------------------- |
| Keyword                  | `mixin`                                |
| Usage keyword            | `with`                                 |
| Multiple mixins          | ✅ `with A, B, C`                      |
| Constructor in mixin     | ❌ Not allowed                          |
| Instantiation of mixin   | ❌ Not allowed                          |
| Combined with inherits   | ✅ `class Foo inherits Bar with Mixin`  |
| Conflict resolution      | Class definition wins over mixin       |

### Combined syntax

```xell
class Dog inherits Animal with Loggable, Serializable implements Drawable :
    fn __init__(self, name) :
        parent.__init__(name, "Woof")
        self->log("Dog created")
    ;
;
```

---

## 11. Phase 10: Decorators

Decorators modify class behavior at definition time using the `@decorator` syntax (already
supported for functions).

### `@dataclass` — auto-generates `__init__`, `__eq__`, `__print__` from fields

```xell
@dataclass
class Point :
    x = 0
    y = 0
;

p = Point(x: 3, y: 4)
print(p)                       # Point(x=3, y=4)
p1 = Point(1, 2)
p2 = Point(1, 2)
print(p1 == p2)                # true
```

### `@immutable` — all fields become read-only after `__init__`

```xell
@immutable
class Config :
    fn __init__(self, host, port) :
        self->host = host
        self->port = port
    ;
;

cfg = Config("localhost", 8080)
print(cfg->host)               # localhost
cfg->host = "other"            # Error: Config is immutable
```

### `@singleton` — only one instance ever created; repeated calls return the same object

```xell
@singleton
class Database :
    fn __init__(self) :
        self->connection = null
    ;

    fn connect(self, url) :
        self->connection = url
    ;
;

db1 = Database()
db2 = Database()
db1->connect("postgres://...")
print(db2->connection)         # postgres://...  — same instance
print(db1 == db2)              # true
```

### Built-in decorators

| Decorator    | Effect                                                       |
| ------------ | ------------------------------------------------------------ |
| `@dataclass` | Auto-generates `__init__`, `__eq__`, `__print__` from fields |
| `@immutable` | Makes all fields read-only after construction                |
| `@singleton` | Enforces single instance                                     |

Custom decorators (user-defined functions that accept and return a class) are also supported,
using the same mechanism as function decorators.

---

## 12. Phase 11: Magic Methods

All special behavior is defined using `__dunder__` naming. These are always opt-in — defining them
gives the class special capabilities.

```xell
fn __init__(self, ...)      # constructor — called when ClassName(...) is used
fn __del__(self)            # destructor  — called when object is garbage collected
fn __print__(self)          # print(obj)  — controls how obj is printed
fn __str__(self)            # "{obj}"     — controls string interpolation
fn __eq__(self, other)      # obj1 == obj2
fn __ne__(self, other)      # obj1 != obj2
fn __lt__(self, other)      # obj1 < obj2
fn __gt__(self, other)      # obj1 > obj2
fn __le__(self, other)      # obj1 <= obj2
fn __ge__(self, other)      # obj1 >= obj2
fn __add__(self, other)     # obj1 + obj2
fn __sub__(self, other)     # obj1 - obj2
fn __mul__(self, other)     # obj1 * obj2
fn __div__(self, other)     # obj1 / obj2
fn __mod__(self, other)     # obj1 % obj2
fn __pow__(self, other)     # obj1 ** obj2
fn __neg__(self)            # -obj
fn __len__(self)            # len(obj)
fn __get__(self, key)       # obj[key]
fn __set__(self, key, val)  # obj[key] = val
fn __call__(self, ...args)  # obj(...)
fn __iter__(self)           # for x in obj
fn __contains__(self, val)  # val in obj
fn __hash__(self)           # used as map key — see Appendix B
```

### Notes

- `__print__` controls what `print obj` outputs.
- `__str__` controls what `"{obj}"` produces in string interpolation.
- If only `__print__` is defined, interpolation falls back to it.
- If only `__str__` is defined, `print` uses it.
- `__hash__` must return a value produced by a built-in hash function (see Appendix B).

---

## 13. Phase 12: Operator Overloading

Operator overloading uses `__dunder__` magic methods (defined in Phase 11).

```xell
class Vector :
    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;

    fn __add__(self, other) :
        give Vector(self->x + other->x, self->y + other->y)
    ;

    fn __sub__(self, other) :
        give Vector(self->x - other->x, self->y - other->y)
    ;

    fn __mul__(self, scalar) :
        give Vector(self->x * scalar, self->y * scalar)
    ;

    fn __eq__(self, other) :
        give self->x == other->x and self->y == other->y
    ;

    fn __print__(self) :
        give "({self->x}, {self->y})"
    ;

    fn __lt__(self, other) :
        # Magnitude comparison
        give (self->x * self->x + self->y * self->y) lt
             (other->x * other->x + other->y * other->y)
    ;
;

a = Vector(1, 2)
b = Vector(3, 4)

c = a + b                      # Vector(4, 6)  — calls __add__
d = a * 3                      # Vector(3, 6)  — calls __mul__
print(c)                       # (4, 6)        — calls __print__
print(a == b)                  # false         — calls __eq__
```

### Supported Operator → Magic Method Mapping

| Operator   | Magic method                    | Notes                |
| ---------- | ------------------------------- | -------------------- |
| `+`        | `__add__(self, other)`          | Binary addition      |
| `-`        | `__sub__(self, other)`          | Binary subtraction   |
| `*`        | `__mul__(self, other)`          | Multiplication       |
| `/`        | `__div__(self, other)`          | Division             |
| `%`        | `__mod__(self, other)`          | Modulo               |
| `**`       | `__pow__(self, other)`          | Power                |
| `==`       | `__eq__(self, other)`           | Equality             |
| `!=`       | `__ne__(self, other)`           | Inequality           |
| `<`        | `__lt__(self, other)`           | Less than            |
| `>`        | `__gt__(self, other)`           | Greater than         |
| `<=`       | `__le__(self, other)`           | Less or equal        |
| `>=`       | `__ge__(self, other)`           | Greater or equal     |
| `[]`       | `__get__(self, key)`            | Index read           |
| `[]=`      | `__set__(self, key, val)`       | Index write          |
| `print()`  | `__print__(self)`               | Print output         |
| `"{obj}"`  | `__str__(self)`                 | String interpolation |
| `len()`    | `__len__(self)`                 | Length               |
| unary `-`  | `__neg__(self)`                 | Negation             |
| `obj(...)` | `__call__(self, ...args)`       | Call as function     |
| `in`       | `__contains__(self, val)`       | Membership test      |
| map key    | `__hash__(self)`                | Hashing              |

### Implementation Strategy

In `evalBinary()` / `evalUnary()`, before applying default operator logic:

1. Check if the left operand is a struct/class instance.
2. Check if its definition has the corresponding `__op__` method.
3. If yes → call that method.
4. If no → fall through to default behavior or raise `TypeError`.

---

## 14. Phase 13: Function Overloading

Functions and methods can be overloaded by **argument count** (always) and by **type annotation**
(when declared explicitly).

### Count-based — always supported, no special syntax

```xell
fn greet() :
    print "Hello stranger"
;

fn greet(name) :
    print "Hello {name}"
;

fn greet(name, title) :
    print "Hello {title} {name}"
;

greet()                        # → Hello stranger
greet("Prithu")                # → Hello Prithu
greet("Prithu", "Mr")          # → Hello Mr Prithu
```

### Type-based — only when you explicitly declare the type

```xell
fn process(str(name)) :        # ONLY accepts string
    print "Name: {name}"
;

fn process(int(age)) :         # ONLY accepts number
    print "Age: {age}"
;

fn process(bool(flag)) :       # ONLY accepts bool
    print "Flag: {flag}"
;

process("Prithu")              # → Name: Prithu
process(22)                    # → Age: 22
process(true)                  # → Flag: true
process([1, 2, 3])             # → TypeError: no matching overload for list
```

### Dynamic params — cannot be type-overloaded

```xell
fn greet(name) :               # dynamic — accepts anything
    print "{name}"
;

fn greet(int(name)) :          # ERROR at parse time:
                               # already have greet(1 param) as dynamic,
                               # cannot add type-specific version
;
```

### Mixed — count + type together

```xell
fn describe(str(name)) :
    print "Name: {name}"
;

fn describe(int(age)) :
    print "Age: {age}"
;

fn describe(str(name), int(age)) :
    print "{name} is {age}"
;

fn describe(name, age, city) : # dynamic — 3 params, no type restriction
    print "{name}, {age}, {city}"
;
```

### In classes too

```xell
class Calculator :
    fn add(int(a), int(b)) :
        give a + b              # integer addition
    ;

    fn add(str(a), str(b)) :
        give "{a}{b}"           # string concatenation
    ;

    fn add(a, b, c) :           # 3 args — count-based, dynamic
        give a + b + c
    ;
;
```

### Type Keywords for Overloading

| Type keyword | Matches                  |
| ------------ | ------------------------ |
| `str(p)`     | String values only       |
| `int(p)`     | Number values only       |
| `bool(p)`    | Boolean values only      |
| `list(p)`    | List values only         |
| `map(p)`     | Map values only          |
| `fn(p)`      | Function / lambda values |
| `set(p)`     | Set                      |
etc what ever ds or parameter types we can have(like float,tuple ....)

### Resolution Order

1. Match by exact argument count + type annotations first.
2. Fall back to count-only match (dynamic).
3. If no match → `TypeError: no matching overload`.

---

## 15. Summary: Complete Syntax Reference

```xell
# ── Struct (data container + methods) ───────────────

struct Point :
    x = 0
    y = 0

    fn distance(self, other) :
        give sqrt(pow(self->x - other->x, 2) + pow(self->y - other->y, 2))
    ;
;

p = Point(x: 3, y: 4)
print(p->x)


# ── Class with constructor ───────────────────────────

class Animal :
    name = ""
    sound = ""

    fn __init__(self, name, sound) :
        self->name = name
        self->sound = sound
    ;

    fn speak(self) :
        give "{self->name} says {self->sound}!"
    ;
;


# ── Inheritance (multiple) + implements ─────────────

class Dog inherits Animal implements Trainable :
    breed = ""

    fn __init__(self, name, breed) :
        parent.__init__(name, "Woof")
        self->breed = breed
    ;

    fn speak(self) :
        give "{self->name} the {self->breed} says Woof!"
    ;
;


# ── Access control ───────────────────────────────────

class BankAccount :
    fn __init__(self, owner) :
        self->owner = owner
    ;

    private:
        balance = 0

    public:
        fn deposit(self, amount) :
            self->balance += amount
        ;
;


# ── Static members ───────────────────────────────────

class MathHelper :
    static PI = 3.14159

    static fn circle_area(r) :
        give MathHelper.PI * r * r
    ;
;


# ── Properties ───────────────────────────────────────

class Circle inherits Shape implements Drawable :
    fn __init__(self, radius) :
        self->_radius = radius
    ;

    get radius(self) :
        give self->_radius
    ;

    set radius(self, val) :
        if val < 0 : error("negative radius") ;
        self->_radius = val
    ;

    fn area(self) :
        give 3.14 * self->_radius * self->_radius
    ;

    fn describe() :
        print "a circle"
    ;

    fn describe(str(label)) :
        print "{label}: r={self->_radius}"
    ;

    fn describe(int(precision)) :
        print "Circle r={format(":.{precision}f", self->_radius)}"
    ;

    fn draw(self) :
        print "drawing circle r={self->_radius}"
    ;

    fn __print__(self) :
        give "Circle(r={self->_radius})"
    ;

    fn __hash__(self) :
        give hash_int(self->_radius)
    ;

    fn __add__(self, other) :
        give Circle(self->_radius + other->_radius)
    ;
;

c = Circle(5)
print c                        # → Circle(r=5)     — calls __print__
print c->area()                # → 78.5
print radius of c              # → 5               — alias for c->radius
c->describe()                  # → a circle
c->describe("my circle")       # → my circle: r=5
c->describe(2)                 # → Circle r=5.00


# ── Interface ────────────────────────────────────────

interface Drawable :
    fn draw(self) ;
    fn resize(self, factor) ;
;


# ── Abstract class ───────────────────────────────────

abstract Shape :
    fn area(self) ;               # must implement
    fn color(self) :
        give "white"              # default — can override
    ;
;


# ── Mixin ────────────────────────────────────────────

mixin Serializable :
    fn to_json(self) :
        bring * from "json"
        give json_stringify(self)
    ;
;

class User with Serializable :
    fn __init__(self, name) :
        self->name = name
    ;
;


# ── Decorators ───────────────────────────────────────

@dataclass
class Point2 :
    x = 0
    y = 0
;

@immutable
class Config :
    fn __init__(self, host, port) :
        self->host = host
        self->port = port
    ;
;

@singleton
class Database :
    fn __init__(self) :
        self->connection = null
    ;
;


# ── Operator overloading ─────────────────────────────

class Vec :
    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;

    fn __add__(self, other) :
        give Vec(self->x + other->x, self->y + other->y)
    ;

    fn __print__(self) :
        give "({self->x}, {self->y})"
    ;
;

a = Vec(1, 2) + Vec(3, 4)      # Vec(4, 6)
print(a)                        # (4, 6)
```

---

## 16. Implementation Roadmap

### Phase 1: Structs with Methods (~2 days)

- [ ] Add `STRUCT` token to lexer
- [ ] Add `StructDef` AST node to parser (fields + methods)
- [ ] Add struct type to XObject (tag + field map)
- [ ] Implement struct construction in interpreter
- [ ] Wire `->` field/method access for struct instances
- [ ] Support named arg syntax `Point(x: 10, y: 20)`
- [ ] Add `typeof()` support for struct names
- [ ] Add `==` field-by-field default comparison
- [ ] Add default `__print__` format: `Name(field=val, ...)`
- [ ] Tests + documentation

### Phase 2: Classes with `__init__` (~2 days)

- [ ] Add `CLASS` token to lexer
- [ ] Add `ClassDef` AST node (constructor + methods + fields)
- [ ] Constructor is `__init__` (not `init`)
- [ ] Wire `ClassName(args)` → calls `__init__()`
- [ ] Tests

### Phase 3: Inheritance & Implements (~2 days)

- [ ] Parse `class Dog inherits Animal` syntax
- [ ] Parse `inherits A, B` (multiple)
- [ ] Parse `implements X, Y`
- [ ] Implement field + method inheritance (C3 MRO for multiple)
- [ ] Implement `parent.__init__(args)` resolution
- [ ] Implement `instance is ClassName` check up chain
- [ ] Method override resolution
- [ ] Tests

### Phase 4: Access Control (~1 day)

- [ ] Parse `private:` / `protected:` / `public:` blocks inside class body
- [ ] Tag each field/method with its access level
- [ ] Enforce access at runtime (field read/write, method call)
- [ ] Tests

### Phase 5: Static Members (~1 day)

- [ ] Parse `static field = val` and `static fn method()`
- [ ] Store static members on class definition, not instance
- [ ] Wire `ClassName.field` dot access
- [ ] Tests

### Phase 6: Properties (~1 day)

- [ ] Parse `get name(self) :` and `set name(self, val) :`
- [ ] On `->` field read: check if getter exists → call it
- [ ] On `->` field write: check if setter exists → call it
- [ ] Read-only / write-only enforcement
- [ ] Tests

### Phase 7: Interfaces (~1 day)

- [ ] Add `INTERFACE` token to lexer
- [ ] Parse `interface Foo : fn method(self) ; ;`
- [ ] Store interface contracts in symbol table
- [ ] On class definition: verify all `implements` contracts are satisfied
- [ ] Tests

### Phase 8: Abstract Classes (~1 day)

- [ ] Add `ABSTRACT` token to lexer
- [ ] Parse `abstract ClassName : ... ;`
- [ ] Mark abstract methods (no body) vs default methods (have body)
- [ ] Prevent direct instantiation
- [ ] Enforce abstract method implementation in subclasses
- [ ] Tests

### Phase 9: Mixins (~1 day)

- [ ] Add `MIXIN` and `WITH` tokens to lexer
- [ ] Parse `mixin Foo : ... ;` and `class Bar with Foo, Baz`
- [ ] Copy mixin methods into class at definition time
- [ ] Conflict resolution: class methods win over mixin methods
- [ ] Tests

### Phase 10: Decorators on Classes (~1 day)

- [ ] Extend decorator parsing to class definitions
- [ ] Implement `@dataclass`: auto-gen `__init__`, `__eq__`, `__print__`
- [ ] Implement `@immutable`: freeze fields post-`__init__`
- [ ] Implement `@singleton`: wrap constructor to return cached instance
- [ ] Tests

### Phase 11: Magic Methods & Operator Overloading (~1 day)

- [ ] Rename `op_*` → `__*__` in interpreter dispatch
- [ ] In `evalBinary()` / `evalUnary()`: check for `__op__` before default
- [ ] Wire all operators to their `__dunder__` methods (see Phase 12 table)
- [ ] `__print__` for `print obj`, `__str__` for `"{obj}"` interpolation
- [ ] `__hash__` for map key usage
- [ ] `__call__` for `obj()`
- [ ] `__iter__` / `__contains__` for `for` and `in`
- [ ] `__del__` destructor hook
- [ ] Tests

### Phase 12: Function Overloading (~2 days)

- [ ] Extend function/method definition to allow multiple signatures
- [ ] Parse `fn foo(str(x))` type annotations
- [ ] Store overload table per function name (count + type signature)
- [ ] Resolution: exact type-count match first, then count-only dynamic
- [ ] Parse-time error: dynamic + type-specific for same count
- [ ] Tests

**Total estimated: ~16 days**

---

## 17. Appendix A: Method & Field Access (`->` and `of`)

Since `.` is the statement terminator in Xell, all field and method access uses `->`.
The `of` keyword is an optional natural-language alias.

### `->` Syntax (primary)

```xell
# Field access
self->name
self->_radius
obj->balance

# Method call
obj->speak()
obj->deposit(100)
circle->area()

# Chaining
obj->get_address()->city
```

### `of` keyword (alias)

`name of obj` is exactly equivalent to `obj->name`.

```xell
print user->name
print name of user             # same thing

print radius of circle         # same as circle->radius
print area of shape            # same as shape->area (getter)
```

Both forms are interchangeable. `of` is for readability; `->` is for consistency and chaining.

---

## 18. Appendix B: `__hash__` and Built-in Hash Functions

`__hash__` lets a class be used as a **map key**. The user defines what to hash and which
algorithm to use, by calling a built-in hash function.

```xell
@immutable
class Point :
    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;

    fn __hash__(self) :
        # user decides what to hash and which algorithm
        give hash_sha256("{self->x},{self->y}")
        # or
        give hash_int(self->x * 31 + self->y)
    ;
;

locations = {}
p = Point(10, 20)
locations[p] = "home"
print(locations[p])            # home
```

### Built-in Hash Functions

| Function            | Description                |
| ------------------- | -------------------------- |
| `hash_int(val)`     | Fast integer hash          |
| `hash_str(val)`     | String hash                |
| `hash_sha256(val)`  | Cryptographic SHA-256 hash |
| `hash_md5(val)`     | MD5 hash                   |

The user picks whichever makes sense for their class. `@immutable` is recommended when
using `__hash__` since mutable objects as map keys cause undefined behavior.

---

## 19. Appendix C: Lambda Syntax Reference

Xell already supports full lambdas/closures. Here is the complete reference:

### Single-parameter inline

```xell
double = x => x * 2
double(5)                      # 10
```

### Multi-parameter inline

```xell
add = (a, b) => a + b
add(3, 4)                      # 7
```

### Zero-parameter inline

```xell
greet = () => "hello world"
greet()                        # "hello world"
```

### Multi-line body

```xell
process = (x, y) => :
    result = x * y + 1
    give result
;
process(3, 4)                  # 13
```

### As function arguments

```xell
nums = [1, 2, 3, 4, 5]

doubled = map(nums, x => x * 2)           # [2, 4, 6, 8, 10]
evens   = filter(nums, x => x % 2 == 0)   # [2, 4]
total   = reduce(nums, (a, b) => a + b, 0) # 15

any(nums, x => x gt 4)                    # true
all(nums, x => x gt 0)                    # true
```

### Closures

```xell
fn make_counter() :
    count = 0
    give () => :
        count = count + 1
        give count
    ;
;

c = make_counter()
print(c())                     # 1
print(c())                     # 2
print(c())                     # 3
```

### Storing in variables and passing around

```xell
fn apply(func, value) :
    give func(value)
;

result = apply(x => x * x, 7) # 49
```

---

## 20. Appendix D: What Changed from Previous Plan

Side-by-side comparison of every decision that changed between the original draft and this confirmed design.

| # | Topic | ❌ Old Design | ✅ New Design |
|---|-------|--------------|--------------|
| 1 | **Constructor name** | `fn init(self, ...)` | `fn __init__(self, ...)` |
| 2 | **Inheritance keyword** | `of` — single only: `class Dog of Animal` | `inherits` — multiple: `class Dog inherits Animal, Wolf` |
| 3 | **Multiple inheritance** | Explicitly banned ("Never — diamond problem") | ✅ Allowed via `inherits A, B` |
| 4 | **Interface keyword** | Not planned — "Use convention-based duck typing" | ✅ `interface` with `implements` keyword |
| 5 | **Abstract classes** | Not planned — "Duck typing is sufficient" | ✅ `abstract` keyword, mix of abstract + default methods |
| 6 | **Mixins** | "Maybe later — composition preferred" | ✅ `mixin` + `with` keyword, multiple allowed |
| 7 | **Access control** | Not planned — "All fields public like Python" | ✅ C++ block-style `private:` / `protected:` / `public:` |
| 8 | **Static members** | Not planned | ✅ `static field` and `static fn` accessed via `ClassName.field` |
| 9 | **Properties** | Not planned | ✅ `get name(self) :` / `set name(self, val) :` |
| 10 | **Decorators on classes** | "Maybe later" | ✅ `@dataclass`, `@immutable`, `@singleton` |
| 11 | **Magic method naming** | `op_add`, `op_sub`, `op_str` etc. | `__add__`, `__sub__`, `__print__` etc. (dunder style) |
| 12 | **Magic method count** | 18 operators (`op_*`) | 24 dunders (`__init__` through `__hash__`) |
| 13 | **`__str__` vs `__print__`** | Single `op_str` for both print and interpolation | Split: `__print__` for `print obj`, `__str__` for `"{obj}"` |
| 14 | **Destructor** | Not listed | ✅ `__del__(self)` added |
| 15 | **`__call__`** | Not listed | ✅ `__call__(self, ...args)` — makes object callable as `obj()` |
| 16 | **`__iter__`** | Not listed | ✅ `__iter__(self)` — enables `for x in obj` |
| 17 | **`__contains__`** | Not listed | ✅ `__contains__(self, val)` — enables `val in obj` |
| 18 | **`__pow__`** | Not listed | ✅ `__pow__(self, other)` — enables `obj ** other` |
| 19 | **Struct capabilities** | Data-only (Phase 1 was fields only, methods were Phase 2) | Methods fully supported inside struct body |
| 20 | **Named arg syntax** | `Point(x = 10, y = 20)` (equals sign) | `Point(x: 10, y: 20)` (colon) |
| 21 | **`of` keyword role** | Inheritance keyword — `class Dog of Animal` | Field-read alias — `name of user` = `user->name` |
| 22 | **Parent call keyword** | `super` | `parent` |
| 23 | **Parent call syntax** | `super->init(self, name)` — explicit `self`, arrow notation | `parent.__init__(name)` — `self` is implicit, dot notation |
| 24 | **Function overloading** | Not planned | ✅ Count-based (always) + type-based (`str(p)`, `int(p)`, etc.) |
| 25 | **`__hash__` detail** | Not detailed | User defines logic, calls built-in (`hash_int`, `hash_sha256`, etc.) |
| 26 | **MRO for multiple inheritance** | N/A (single only) | C3 linearization (left-to-right, depth-first) |
| 27 | **Implementation estimate** | ~8 days (5 phases) | ~16 days (12 phases) |
| 28 | **Design philosophy** | "Minimal — structs for 80% of use cases" | Layered full OOP: structs → classes → abstract → interfaces → mixins |

---

## 21. Appendix E: What We're NOT Adding

| Feature                      | Status         | Reason                                          |
| ---------------------------- | -------------- | ----------------------------------------------- |
| Metaclasses                  | ❌ Never       | Too complex for Xell's goals                    |
| Generics / templates         | ❌ Not planned | Dynamic typing handles this                     |
| Interface inheritance        | ❌ Not planned | Keep interface system flat and simple           |
| Virtual / override keywords  | ❌ Not planned | Method override is implicit (just redefine)     |
| Abstract properties          | ❌ Not planned | Use abstract methods + convention               |
| Friend classes               | ❌ Never       | Complexity, breaks encapsulation model          |
| Mixin inheritance chains     | ❌ Not planned | Mixins are flat — no `mixin A inherits B`       |
| Class variables (shared)     | ❌ Not planned | Use `static` fields instead                     |
| Reflection / introspection   | ⏳ Maybe later | `typeof()` is the start; deeper API later       |
| Custom decorators on classes | ✅ Supported   | Same mechanism as function decorators           |

---

_This document reflects the confirmed design. Implementation should follow the roadmap phases in order._
