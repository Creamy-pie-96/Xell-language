# Inheritance

> **Previous:** [Classes](oop-classes.md)  
> **Next:** [Mixins](oop-mixins.md)

---

Inheritance is a fundamental pillar of object‑oriented programming in Xell. Classes can inherit from one or more parent classes, gaining their fields and methods while being able to override or extend them. Xell supports **multiple inheritance** with a well‑defined Method Resolution Order (MRO), along with the `parent` keyword for invoking parent implementations.

---

## Quick Reference

| Keyword/Syntax | What it does | Minimal example |
|----------------|--------------|-----------------|
| `class Child inherits Parent1, Parent2` | Declares inheritance from one or more parents | `class Dog inherits Animal, Pet` |
| `parent->method(args)` | Calls the parent's version of a method | `parent->__init__(self, name)` |
| `obj is ClassName` | Checks if `obj` is an instance of `ClassName` (including subclasses) | `d is Animal` → `true` |
| `belong ClassName` in `incase` | **Exact** type name match (not subtype‑aware) | `belong Animal` → `false` for a `Dog` instance |
| `fn method(self) ;` | Abstract method (no body) in parent | `fn area(self) ;` |
| Abstract method implementation | Subclass must implement all abstract methods | `fn area(self): give self->width * self->height ;` |

---

## Declaring Inheritance

Use the `inherits` keyword followed by one or more parent class names.

```xell
class Animal :
    name = ""

    fn __init__(self, name):
        self->name = name
    ;

    fn speak(self):
        print("?")
    ;
;

class Dog inherits Animal :
    fn speak(self):
        print("Woof!")
    ;
;

let d = Dog("Rex")
d->speak()   # Woof!
```

- Parent classes must be defined before they are used.
- The child class inherits all fields and methods from its parents.
- If a child defines a method with the same name as a parent, it **overrides** the parent's method.

---

## Multiple Inheritance

A class can inherit from multiple parents.

```xell
class Flyer :
    fn fly(self):
        print("Flying!")
    ;
;

class Swimmer :
    fn swim(self):
        print("Swimming!")
    ;
;

class Duck inherits Flyer, Swimmer :
    fn speak(self):
        print("Quack!")
    ;
;

let d = Duck()
d->fly()    # Flying!
d->swim()   # Swimming!
d->speak()  # Quack!
```

- Parents are listed in order of priority (left‑to‑right).
- If multiple parents define the same method, the **first** parent in the list takes precedence.

---

## Method Resolution Order (MRO)

Xell's MRO follows a **depth‑first, left‑to‑right** search:

1. **Child's own methods** – methods defined directly in the class.
2. **Parents** – in the order they are listed, each parent's own methods.
3. **Mixins** – after parents (lower priority).
4. **Interfaces** – only for validation, not for method dispatch.

```xell
class A :
    fn hello(self): print("A") ;
;

class B :
    fn hello(self): print("B") ;
;

class C inherits A, B :
    # No own hello() method
;

let c = C()
c->hello()   # A (first parent)
```

**Search order for `c->hello()`:**
1. `C` has no own `hello()`.
2. Check `A` – found → call `A::hello()`.
3. `B` is never checked because `A` provided the method.

---

## The `parent` Keyword

`parent` is a special binding available inside methods that refers to the **immediate parent of the class that defines the currently‑executing method**.

```xell
class Animal :
    fn __init__(self, name):
        self->name = name
    ;
;

class Dog inherits Animal :
    fn __init__(self, name, breed):
        parent->__init__(self, name)   # calls Animal::__init__
        self->breed = breed
    ;
;
```

### How `parent` is Resolved

`parent` is **not** a runtime lookup – it is bound at the method call site based on **the class that defines the method being called**. Specifically:

- Inside a method defined in class `Dog`, `parent` refers to `Dog`'s first parent (`Animal`).
- If `Dog` inherits from `Animal, Pet`, `parent` refers to `Animal` (the first parent).
- Inside a method defined in a parent class, `parent` refers to *that* class's parent.

```xell
class Grandparent :
    fn greet(self): print("Grandparent") ;
;

class Parent inherits Grandparent :
    fn greet(self):
        print("Parent")
        parent->greet(self)   # calls Grandparent::greet()
    ;
;

class Child inherits Parent :
    fn greet(self):
        print("Child")
        parent->greet(self)   # calls Parent::greet()
    ;
;

let c = Child()
c->greet()
```

**Expected output:**
```
Child
Parent
Grandparent
```

### Important: `parent` is Static, Not Dynamic

`parent` is resolved based on the **static** class hierarchy of the **defining class**, not the **runtime** type of `self`.

```xell
class A :
    fn hello(self): print("A") ;
;

class B inherits A :
    fn hello(self):
        print("B")
        parent->hello(self)   # always calls A::hello()
    ;
;

class C inherits B :
    fn hello(self):
        print("C")
        parent->hello(self)   # always calls B::hello()
    ;
;

let c = C()
c->hello()
```

**Output:**
```
C
B
A
```

Even though `self` is a `C` instance, the `parent` calls are statically determined by each class's definition.

### Accessing `parent` Outside Methods

`parent` is **only** available inside class methods (i.e., when `self` is in scope). It cannot be used at the top level or in standalone functions.

```xell
parent->some_method()   # ❌ error – parent is not defined here
```

---

## Abstract Methods and Inheritance

A parent class can declare abstract methods (methods without a body) that children must implement.

```xell
abstract Shape :
    fn area(self) ;   # abstract – no body
;

class Circle inherits Shape :
    radius = 0

    fn __init__(self, radius):
        self->radius = radius
    ;

    fn area(self):
        give PI * self->radius ** 2
    ;
;

let c = Circle(5)
print(c->area())   # 78.5398...
```

- If a child class does **not** implement all abstract methods from its parents, the child remains abstract (cannot be instantiated).
- Abstract methods use `;` (semicolon) instead of a body.

### Abstract Method Inheritance Chain

```xell
abstract A :
    fn method(self) ;
;

abstract B inherits A :
    # Does NOT implement method() – still abstract
;

class C inherits B :
    fn method(self): print("implemented") ;   # now concrete
;

let c = C()
c->method()   # "implemented"
```

---

## `is` – Instance‑of Check

The `is` operator checks if an object is an instance of a class **or any of its subclasses**.

```xell
class Animal ;
class Dog inherits Animal ;
class Cat inherits Animal ;

let d = Dog()
print(d is Animal)   # true
print(d is Dog)      # true
print(d is Cat)      # false
```

This is a **subtype‑aware** check.

### `is not` – Negated Instance‑of Check

```xell
print(d is not Cat)   # true
```

### `is` vs `belong` in `incase`

**Critical distinction:**

- `is` (operator): **subtype‑aware** – matches instances of the class or any subclass.
- `belong` (in `incase`): **exact name match** – does **not** consider inheritance.

```xell
incase d :
    belong Animal : print("Animal")   # ❌ does NOT match a Dog instance
    belong Dog : print("Dog")         # ✅ matches
;

if d is Animal : print("Animal")      # ✅ matches (subtype‑aware)
```

**Use `is` for polymorphic type checks; use `belong` for exact type matching.**

---

## Field Inheritance

Child classes inherit all fields from their parents.

```xell
class Animal :
    name = ""
    age = 0
;

class Dog inherits Animal :
    breed = ""
;

let d = Dog({ name: "Rex", age: 3, breed: "Labrador" })
print(d->name)   # "Rex"
print(d->age)    # 3
print(d->breed)  # "Labrador"
```

### Field Overriding

A child class can override a parent's field default value.

```xell
class Animal :
    legs = 4
;

class Human inherits Animal :
    legs = 2   # overrides the default
;

let h = Human()
print(h->legs)   # 2
```

However, the parent's field definition remains unchanged – only the child's instances see the overridden default.

### Field Access in `__init__`

When overriding fields, be mindful of the order of initialization:

```xell
class Parent :
    value = 10
    fn __init__(self):
        print(self->value)   # prints 10
    ;
;

class Child inherits Parent :
    value = 20
;

let c = Child()   # prints 10 (Parent's __init__ runs before Child's defaults are applied)
```

**Why?** The parent's `__init__` runs before the child's fields are fully initialized. Use `parent->__init__()` calls carefully to avoid surprises.

---

## Complete Example: Shape Hierarchy

```xell
abstract Shape :
    fn area(self) ;
    fn perimeter(self) ;
;

class Rectangle inherits Shape :
    width = 0
    height = 0

    fn __init__(self, width, height):
        self->width = width
        self->height = height
    ;

    fn area(self):
        give self->width * self->height
    ;

    fn perimeter(self):
        give 2 * (self->width + self->height)
    ;

    fn is_square(self):
        give self->width == self->height
    ;
;

class Square inherits Rectangle :
    fn __init__(self, side):
        parent->__init__(self, side, side)
    ;

    fn is_square(self):
        give true
    ;
;

class Circle inherits Shape :
    radius = 0

    fn __init__(self, radius):
        self->radius = radius
    ;

    fn area(self):
        give PI * self->radius ** 2
    ;

    fn perimeter(self):
        give 2 * PI * self->radius
    ;
;

let shapes = [
    Rectangle(4, 5),
    Square(3),
    Circle(2)
]

for shape in shapes:
    print("Area: " + str(shape->area()))
    print("Perimeter: " + str(shape->perimeter()))
    if shape is Square :
        print("  -> Square special behavior")
    elif shape is Rectangle :
        print("  -> Rectangle special behavior")
    else :
        print("  -> Other shape")
;
```

**Expected output:**
```
Area: 20
Perimeter: 18
  -> Rectangle special behavior
Area: 9
Perimeter: 12
  -> Square special behavior
Area: 12.566...
Perimeter: 12.566...
  -> Other shape
```

---

## Edge Cases and Less Obvious Behavior

### 1. `parent` in Deep Hierarchies

`parent` always refers to the **immediate parent of the class that defines the current method**, not the next parent in the inheritance chain at runtime.

```xell
class A :
    fn greet(self): print("A") ;
;

class B inherits A :
    fn greet(self):
        print("B")
        parent->greet(self)   # always calls A
    ;
;

class C inherits B :
    # No own greet()
;

let c = C()
c->greet()   # B, A
```

### 2. Diamond Problem Resolution

Xell's MRO resolves diamond inheritance by searching **parents left‑to‑right, depth‑first**.

```xell
class Top :
    fn hello(self): print("Top") ;
;

class Left inherits Top :
    fn hello(self): print("Left") ;
;

class Right inherits Top :
    fn hello(self): print("Right") ;
;

class Bottom inherits Left, Right :
    # No own hello()
;

let b = Bottom()
b->hello()   # Left (first parent wins)
```

### 3. Abstract Methods and `belong`

```xell
abstract Animal :
    fn speak(self) ;
;

class Dog inherits Animal :
    fn speak(self): print("Woof") ;
;

let d = Dog()
d belong Animal   # ❌ false – belong does NOT check inheritance
d is Animal       # ✅ true  – is checks inheritance
```

This is a common source of confusion. Use `is` for polymorphic type checks.

### 4. Fields Are Not Polymorphic

Field access is **not** polymorphic – it always accesses the field in the class where it was defined.

```xell
class Parent :
    value = 10
;

class Child inherits Parent :
    value = 20
    fn get_parent_value(self):
        give parent->value   # 10
    ;
;

let c = Child()
print(c->value)              # 20 (own field)
print(c->get_parent_value()) # 10 (parent's field)
```

### 5. Constructors and Inheritance

`__init__` is **not** automatically called on parents. You must explicitly call `parent->__init__(self, ...)`.

```xell
class Parent :
    name = ""
    fn __init__(self, name):
        self->name = name
    ;
;

class Child inherits Parent :
    age = 0
    fn __init__(self, name, age):
        parent->__init__(self, name)   # MUST call parent constructor
        self->age = age
    ;
;

let c = Child("Alice", 30)
print(c->name)   # "Alice" (thanks to parent->__init__)
```

If you omit the `parent->__init__` call, the parent's fields remain at their default values.

---

## What NOT to Do

### 1. Using `belong` for Polymorphic Type Checks

```xell
class Animal ; class Dog inherits Animal ;

let d = Dog()
incase d :
    belong Animal : print("animal")   # ❌ does NOT match
    belong Dog : print("dog")         # ✅ matches
;
```

**Why it fails:** `belong` uses exact string name matching, not inheritance. Use `is` instead.

### 2. Expecting `parent` to Work in Static Methods

```xell
class Parent :
    static fn hello(): print("Parent") ;
;

class Child inherits Parent :
    static fn hello():
        parent->hello()   # ❌ error – parent is not available in static context
    ;
;
```

**Why it fails:** `parent` is only bound inside instance methods (where `self` exists). Static methods have no `self`.

### 3. Creating Circular Inheritance

```xell
class A inherits B ;   # ❌ error – B is not defined yet
class B inherits A ;   # ❌ error – circular dependency
```

**Why it fails:** The parser checks that parent classes exist and are not the same as the child. Circular inheritance is detected and rejected.

### 4. Forgetting to Call `parent->__init__`

```xell
class Parent :
    name = ""
    fn __init__(self, name): self->name = name ;
;

class Child inherits Parent :
    age = 0
    fn __init__(self, name, age):
        # missing parent->__init__ call
        self->age = age
    ;
;

let c = Child("Alice", 30)
print(c->name)   # "" (name never set)
```

**Why it fails:** Parent constructors are not called automatically. Always call `parent->__init__` explicitly.

### 5. Overriding a Method with a Different Signature

```xell
class Parent :
    fn process(self, x): give x * 2 ;
;

class Child inherits Parent :
    fn process(self): give 10 ;   # ❌ different arity – overloading, not overriding
;
```

**Why it's a problem:** This is legal but creates a new method that shadows the parent's method. The parent's method is still accessible via `parent->process(self, x)`.

### 6. Expecting `is` to Work with Structs

```xell
struct Point ; let p = Point()
p is Point   # ❌ TypeError – is only works with classes, not structs
```

**Why it fails:** Structs are value types without an inheritance hierarchy. Use `type(p) == "Point"` or `belong Point` for structs.

### 7. Using Abstract Classes Without Implementing All Abstract Methods

```xell
abstract Shape :
    fn area(self) ;
    fn perimeter(self) ;
;

class Circle inherits Shape :
    radius = 0
    fn area(self): give PI * radius ** 2 ;   # missing perimeter()
;

let c = Circle(5)   # ❌ TypeError – cannot instantiate abstract class
```

**Why it fails:** `Circle` is still abstract because it doesn't implement `perimeter()`. Fix by implementing it.

---

## Inheritance vs. Composition

| Aspect | Inheritance | Composition |
|--------|-------------|-------------|
| Relationship | "is‑a" (e.g., `Dog` is an `Animal`) | "has‑a" (e.g., `Car` has an `Engine`) |
| Code reuse | Inherits all parent methods and fields | Delegates to contained objects |
| Flexibility | Tighter coupling | Looser coupling |
| Use when | You need polymorphic behavior | You need to combine independent behaviors |
| In Xell | Use `inherits` | Use fields (e.g., `engine = Engine()`) |

**Example of composition:**

```xell
class Engine :
    fn start(self): print("Engine started") ;
;

class Car :
    engine = Engine()
    fn start(self): self->engine->start() ;
;
```

---

## See Also

- [Classes](oop-classes.md) – base class syntax and features
- [Mixins](oop-mixins.md) – composition via `with`
- [Interfaces](oop-interfaces.md) – contracts for classes
- [Abstract Classes](oop-abstract.md) – abstract methods and classes
- [Access Control](oop-access-control.md) – `protected` inheritance rules
- [Magic Methods](oop-magic-methods.md) – operator overloading and protocols

---

**Previous:** [Classes](oop-classes.md)  
**Next:** [Mixins](oop-mixins.md)