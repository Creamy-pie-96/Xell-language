# OOP Integration Plan for Xell

> **Status:** Draft — awaiting final review before implementation.
> **Date:** 2025
> **Note:** The original design philosophy was "no OOP by design." This plan
> proposes a **lightweight, pragmatic OOP layer** that fits Xell's crystal-clear
> syntax, without turning it into a full OO language.

---

## Table of Contents

1. [Design Philosophy](#1-design-philosophy)
2. [Phase 1: Structs (Data-Only Types)](#2-phase-1-structs)
3. [Phase 2: Methods on Structs](#3-phase-2-methods-on-structs)
4. [Phase 3: Classes (Struct + Constructor + Methods)](#4-phase-3-classes)
5. [Phase 4: Inheritance](#5-phase-4-inheritance)
6. [Phase 5: Operator Overloading](#6-phase-5-operator-overloading)
7. [Summary: Complete Syntax Reference](#7-summary-complete-syntax-reference)
8. [Implementation Roadmap](#8-implementation-roadmap)
9. [Appendix: Lambda Syntax Reference](#9-appendix-lambda-syntax-reference)
10. [Appendix: What We're NOT Adding](#10-appendix-what-were-not-adding)

---

## 1. Design Philosophy

Xell's OOP should be:

- **Minimal** — structs are enough for 80% of use cases. Classes only when behavior is needed.
- **Explicit** — no hidden `this`, no implicit constructors, no magic methods.
- **Composable** — structs + functions should feel natural with existing `->` method syntax.
- **Consistent** — uses the same `:` / `;` block syntax as everything else in Xell.
- **No class-based inheritance unless explicitly opted in** — prefer composition.

**Guiding principle:** If you can solve it with a map and functions, you should. Structs are for
when you need named fields with type clarity. Classes are for when you need encapsulated behavior.

---

## 2. Phase 1: Structs (Data-Only Types)

Structs are **named data containers** with fixed fields. Think C structs or Python's `@dataclass`.

### Syntax

```
# Define a struct
struct Point :
    x = 0
    y = 0
;

# Create instances
p1 = Point()                  # Uses defaults: x=0, y=0
p2 = Point(3, 7)              # Positional: x=3, y=7
p3 = Point(x = 10, y = 20)   # Named: x=10, y=20

# Access fields
print(p1->x)                  # 0
print(p2->y)                  # 7

# Mutate fields
p1->x = 42
print(p1->x)                  # 42

# Struct identity
typeof(p1)                    # "Point"
print(p1)                     # Point(x=42, y=0)
```

### Key Decisions

| Feature | Decision | Rationale |
|---------|----------|-----------|
| Field access | `->` (arrow) | Already used for map key access in Xell |
| Default values | Required for all fields | No uninitialized fields ever |
| Construction | `Name(args)` | Same as function call syntax |
| Positional + named args | Both supported | Flexible, Python-like |
| Mutability | Mutable by default | Consistent with Xell variables |
| Printing | Auto `toString()` | `StructName(field1=val1, field2=val2)` |
| typeof | Returns struct name | `typeof(p)` → `"Point"` |
| Equality | Field-by-field comparison | `p1 == p2` if all fields match |

### Grammar Extension

```
STRUCT_DEF := "struct" IDENTIFIER ':' { FIELD_DEF } ';'
FIELD_DEF  := IDENTIFIER '=' EXPRESSION
```

### Internal Representation

A struct instance is internally a **Map with a type tag**. The XObject gets a new variant
or a metadata field storing the struct name. This reuses the existing map infrastructure.

```
XObject
├── type_ = MAP (or new STRUCT type)
├── structName_ = "Point"
└── fields_ = {"x": XObject(42), "y": XObject(0)}
```

---

## 3. Phase 2: Methods on Structs

Methods are functions **bound to a struct**. The `self` keyword refers to the instance.

### Syntax

```
struct Circle :
    radius = 1.0

    fn area(self) :
        give 3.14159 * self->radius * self->radius
    ;

    fn scale(self, factor) :
        self->radius = self->radius * factor
    ;

    fn describe(self) :
        give "Circle(r={self->radius}, area={self->area()})"
    ;
;

c = Circle(5)
print(c->area())              # 78.53975
c->scale(2)
print(c->radius)              # 10
print(c->describe())          # Circle(r=10, area=314.159)
```

### Key Decisions

| Feature | Decision | Rationale |
|---------|----------|-----------|
| Self reference | Explicit `self` as first parameter | No hidden `this` — Xell is explicit |
| Method call | `instance->method()` | Reuses arrow syntax |
| Field access in methods | `self->field` | Consistent with external access |
| Static methods | `fn method()` (no `self`) | Called as `StructName->method()` |
| Method vs field | Methods are defined with `fn` | Clear distinction |

### Method Resolution

When `c->area()` is called:
1. Check if `area` is a field on the instance → no
2. Check if `area` is a method on the struct definition → yes
3. Call `area(c)` with `self` bound to the instance

This integrates cleanly with the existing `->` rewriting that already transforms
`obj->method(args)` into `method(obj, args)`.

---

## 4. Phase 3: Classes (Struct + Constructor + Methods)

Classes add a **constructor** (`init`) for complex initialization logic.

### Syntax

```
class Animal :
    # Fields with defaults
    name = ""
    sound = ""
    legs = 4

    # Constructor
    fn init(self, name, sound) :
        self->name = name
        self->sound = sound
    ;

    # Methods
    fn speak(self) :
        give "{self->name} says {self->sound}!"
    ;

    fn describe(self) :
        give "{self->name}: {self->legs} legs"
    ;
;

dog = Animal("Rex", "Woof")
print(dog->speak())           # Rex says Woof!
print(dog->legs)              # 4 (from default)
```

### class vs struct

| Feature | `struct` | `class` |
|---------|----------|---------|
| Fields | ✅ | ✅ |
| Default values | Required | Required |
| Methods | ✅ (Phase 2) | ✅ |
| Constructor (`init`) | ❌ | ✅ |
| Inheritance | ❌ | ✅ (Phase 4) |
| Operator overloading | ❌ | ✅ (Phase 5) |

**Rule:** If you don't need `init` or inheritance, use `struct`. If you do, use `class`.

---

## 5. Phase 4: Inheritance

Single inheritance with `of` keyword (already a Xell keyword).

### Syntax

```
class Animal :
    name = ""
    sound = ""

    fn init(self, name, sound) :
        self->name = name
        self->sound = sound
    ;

    fn speak(self) :
        give "{self->name} says {self->sound}!"
    ;
;

class Dog of Animal :
    breed = ""

    fn init(self, name, breed) :
        # Call parent constructor
        super->init(self, name, "Woof")
        self->breed = breed
    ;

    # Override parent method
    fn speak(self) :
        give "{self->name} the {self->breed} says Woof!"
    ;

    fn fetch(self) :
        give "{self->name} fetches the ball!"
    ;
;

d = Dog("Rex", "German Shepherd")
print(d->speak())             # Rex the German Shepherd says Woof!
print(d->fetch())             # Rex fetches the ball!
typeof(d)                     # "Dog"
d is Dog                      # true
d is Animal                   # true (inheritance check)
```

### Key Decisions

| Feature | Decision | Rationale |
|---------|----------|-----------|
| Inheritance keyword | `of` | Already a keyword, reads naturally: "Dog of Animal" |
| Multiple inheritance | **No** | Keep it simple — composition over inheritance |
| `super` keyword | `super->method()` | Explicit parent access |
| `is` check | Works with inheritance chain | `d is Animal` → true |
| Method override | Implicit (just redefine) | No `override` keyword needed |
| Abstract methods | **Not supported** | Use duck typing / convention |
| Mixins/traits | **Not supported initially** | Consider for future |

### Grammar Extension

```
CLASS_DEF := "class" IDENTIFIER [ "of" IDENTIFIER ] ':' { CLASS_BODY } ';'
CLASS_BODY := FIELD_DEF | FN_DEF
```

### Method Resolution Order (MRO)

Simple linear MRO since we only support single inheritance:
1. Look in the instance's class
2. Look in the parent class
3. Look in the grandparent class
4. ...up to the root

---

## 6. Phase 5: Operator Overloading

Allow classes to define custom behavior for operators.

### Syntax

```
class Vector :
    x = 0
    y = 0

    fn init(self, x, y) :
        self->x = x
        self->y = y
    ;

    # Operator overloads
    fn op_add(self, other) :
        give Vector(self->x + other->x, self->y + other->y)
    ;

    fn op_sub(self, other) :
        give Vector(self->x - other->x, self->y - other->y)
    ;

    fn op_mul(self, scalar) :
        give Vector(self->x * scalar, self->y * scalar)
    ;

    fn op_eq(self, other) :
        give self->x == other->x and self->y == other->y
    ;

    fn op_str(self) :
        give "({self->x}, {self->y})"
    ;

    fn op_lt(self, other) :
        # Magnitude comparison
        give (self->x * self->x + self->y * self->y) lt (other->x * other->x + other->y * other->y)
    ;
;

a = Vector(1, 2)
b = Vector(3, 4)

c = a + b                     # Vector(4, 6)  — calls op_add
d = a * 3                     # Vector(3, 6)  — calls op_mul
print(c)                      # (4, 6)        — calls op_str
print(a == b)                 # false          — calls op_eq
```

### Supported Operator Methods

| Operator | Method name | Signature |
|----------|-------------|-----------|
| `+` | `op_add(self, other)` | Binary addition |
| `-` | `op_sub(self, other)` | Binary subtraction |
| `*` | `op_mul(self, other)` | Multiplication |
| `/` | `op_div(self, other)` | Division |
| `%` | `op_mod(self, other)` | Modulo |
| `==` | `op_eq(self, other)` | Equality |
| `!=` | `op_ne(self, other)` | Inequality |
| `<` | `op_lt(self, other)` | Less than |
| `>` | `op_gt(self, other)` | Greater than |
| `<=` | `op_le(self, other)` | Less or equal |
| `>=` | `op_ge(self, other)` | Greater or equal |
| `[]` | `op_index(self, key)` | Index access |
| `[]=` | `op_index_set(self, key, val)` | Index assignment |
| `print()` | `op_str(self)` | String conversion |
| `len()` | `op_len(self)` | Length |
| unary `-` | `op_neg(self)` | Negation |
| `not` | `op_not(self)` | Logical not |

### Implementation Strategy

In `evalBinary()`, before applying the default operator logic:
1. Check if the left operand is a struct/class instance
2. Check if its class defines the corresponding `op_*` method
3. If yes, call that method instead
4. If no, fall through to default behavior (or throw TypeError)

---

## 7. Summary: Complete Syntax Reference

```
# ── Struct (data container) ──────────────────────

struct Point :
    x = 0
    y = 0
;

p = Point(3, 7)
print(p->x)

# ── Struct with methods ─────────────────────────

struct Circle :
    radius = 1.0

    fn area(self) :
        give 3.14159 * self->radius * self->radius
    ;
;

# ── Class (struct + constructor + inheritance) ──

class Animal :
    name = ""

    fn init(self, name) :
        self->name = name
    ;

    fn speak(self) :
        give "{self->name} makes a sound"
    ;
;

class Dog of Animal :
    breed = ""

    fn init(self, name, breed) :
        super->init(self, name)
        self->breed = breed
    ;

    fn speak(self) :
        give "{self->name} barks!"
    ;
;

# ── Operator overloading ────────────────────────

class Vec :
    x = 0
    y = 0

    fn init(self, x, y) :
        self->x = x
        self->y = y
    ;

    fn op_add(self, other) :
        give Vec(self->x + other->x, self->y + other->y)
    ;

    fn op_str(self) :
        give "({self->x}, {self->y})"
    ;
;

a = Vec(1, 2) + Vec(3, 4)    # Vec(4, 6)
print(a)                      # (4, 6)
```

---

## 8. Implementation Roadmap

### Phase 1: Structs (estimated: ~2 days)
- [ ] Add `STRUCT` token to lexer (`struct` keyword)
- [ ] Add `StructDef` AST node to parser
- [ ] Add struct type to XObject (tag + field map)
- [ ] Implement struct construction in interpreter
- [ ] Wire `->` field access for struct instances
- [ ] Add `typeof()` support for struct names
- [ ] Add `==` field-by-field comparison
- [ ] Add `toString()` formatting: `Name(field=val, ...)`
- [ ] Tests + documentation

### Phase 2: Methods (estimated: ~1 day)
- [ ] Allow `fn` definitions inside struct body
- [ ] Store methods in struct definition (separate from fields)
- [ ] Wire `instance->method()` to resolve struct methods
- [ ] Bind `self` automatically as first argument
- [ ] Tests

### Phase 3: Classes (estimated: ~2 days)
- [ ] Add `CLASS` token to lexer (`class` keyword)
- [ ] Add `ClassDef` AST node (extends StructDef with init + inheritance)
- [ ] Implement `init` constructor calling
- [ ] Wire `ClassName(args)` to call `init()`
- [ ] Tests

### Phase 4: Inheritance (estimated: ~2 days)
- [ ] Parse `class Dog of Animal` syntax
- [ ] Implement field + method inheritance
- [ ] Implement `super->method()` resolution
- [ ] Implement `instance is ClassName` check up the chain
- [ ] Method override resolution
- [ ] Tests

### Phase 5: Operator Overloading (estimated: ~1 day)
- [ ] In `evalBinary()`, check for `op_*` methods on struct/class instances
- [ ] Dispatch to `op_add`, `op_sub`, etc. when defined
- [ ] `op_str` for print/toString
- [ ] `op_index` / `op_index_set` for `[]`
- [ ] Tests

**Total estimated: ~8 days**

---

## 9. Appendix: Lambda Syntax Reference

Xell already supports full lambdas/closures. Here is the complete reference:

### Single-parameter inline
```
double = x => x * 2
double(5)                      # 10
```

### Multi-parameter inline
```
add = (a, b) => a + b
add(3, 4)                      # 7
```

### Zero-parameter inline
```
greet = () => "hello world"
greet()                        # "hello world"
```

### Multi-line body
```
process = (x, y) => :
    result = x * y + 1
    give result
;
process(3, 4)                  # 13
```

### As function arguments
```
nums = [1, 2, 3, 4, 5]

# map, filter, reduce all accept lambdas
doubled = map(nums, x => x * 2)         # [2, 4, 6, 8, 10]
evens = filter(nums, x => x % 2 == 0)   # [2, 4]
total = reduce(nums, (a, b) => a + b, 0) # 15

# any / all
any(nums, x => x gt 4)                   # true
all(nums, x => x gt 0)                   # true
```

### Closures
```
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
```
fn apply(func, value) :
    give func(value)
;

result = apply(x => x * x, 7) # 49
```

---

## 10. Appendix: What We're NOT Adding

To keep Xell clean and simple:

| Feature | Status | Reason |
|---------|--------|--------|
| Multiple inheritance | ❌ Never | Complexity, diamond problem |
| Abstract classes | ❌ Not planned | Duck typing is sufficient |
| Interfaces / protocols | ❌ Not planned | Use convention-based duck typing |
| Private/protected fields | ❌ Not planned | All fields are public (like Python) |
| Class variables (shared) | ❌ Not planned | Use module-level variables |
| Metaclasses | ❌ Never | Too complex for Xell's goals |
| Generics / templates | ❌ Not planned | Dynamic typing handles this |
| Decorators on classes | ⏳ Maybe later | Already have function decorators |
| Dataclass-like auto-gen | ⏳ Maybe later | Structs already auto-generate toString/eq |
| Mixins / traits | ⏳ Maybe later | Composition is preferred |

---

*This document is a proposal. Review and modify the syntax decisions before implementation begins.*
