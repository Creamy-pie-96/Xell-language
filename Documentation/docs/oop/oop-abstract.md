# Abstract Classes

> **Previous:** [Magic Methods](oop-magic-methods.md)  
> **Next:** [Structs](oop-structs.md) *(or wherever it sits in your reading order)*

---

Abstract classes are **incomplete classes** – they define a common interface and partial implementation for a family of subclasses, but they cannot be instantiated on their own. They are used to model "is‑a" relationships where the parent class is too generic to have a concrete instance, and they often declare **abstract methods** that subclasses must implement.

Think of abstract classes as **blueprints for a family of classes** – they provide the shared structure and behavior, while leaving specific details to be filled in by concrete subclasses.

---

## Quick Reference

| Keyword/Syntax | What it does | Minimal example |
|----------------|--------------|-----------------|
| `abstract class Name : ... ;` | Defines an abstract class | `abstract Shape : fn area(self) ; ;` |
| `fn method(self) ;` | Abstract method (no body) | `fn area(self) ;` |
| `fn method(self): ... ;` | Concrete method (has body) | `fn describe(self): print("shape") ;` |
| `class Child inherits Parent` | Subclass must implement all abstract methods | `class Circle inherits Shape : fn area(self): ... ;` |
| Abstract method validation | Ensures all abstract methods are implemented | Automatic at class definition time |
| `obj is AbstractClass` | Checks if `obj` is an instance of a subclass | `c is Shape` → `true` |
| `belong AbstractClass` in `incase` | **Exact** name match (not abstract-aware) | `belong Shape` → `false` for a `Circle` |

---

## Defining an Abstract Class

Use the `abstract` keyword before `class`.

```xell
abstract Shape :
    fn area(self) ;       # abstract method – no body
    fn perimeter(self) ;  # abstract method – no body

    fn describe(self):    # concrete method – has a body
        print("This is a shape")
    ;
;
```

- Abstract classes are defined with `abstract class Name : ... ;`.
- They can have **abstract methods** (declared with `;` instead of a body).
- They can also have **concrete methods** (with full bodies).
- They can have **fields** (just like regular classes).
- They **cannot** be instantiated directly.

---

## Abstract Methods

Abstract methods are declared with just the signature and a semicolon – no body.

```xell
abstract Animal :
    fn speak(self) ;      # abstract – subclasses must implement
    fn move(self) ;       # abstract – subclasses must implement

    fn breathe(self):
        print("Breathing")   # concrete – inherited as‑is
    ;
;
```

- Abstract methods have **no implementation** in the abstract class.
- They must be implemented by **concrete subclasses**.
- If a subclass does not implement all abstract methods, it remains abstract (cannot be instantiated).
- Abstract methods can have parameters and return type annotations.

```xell
abstract Calculator :
    fn add(self, a: int, b: int) -> int ;   # abstract with types
    fn subtract(self, a, b) ;               # abstract without types
;
```

---

## Implementing Abstract Methods

A concrete subclass must implement **all** abstract methods from its parent(s).

```xell
abstract Shape :
    fn area(self) ;
    fn perimeter(self) ;
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

let c = Circle(5)
print(c->area())       # 78.5398...
print(c->perimeter())  # 31.4159...
```

**Expected output:**
```
78.53981633974483
31.41592653589793
```

---

## Abstract Methods in Inheritance Chains

If a subclass does **not** implement an abstract method, it remains abstract.

```xell
abstract A :
    fn method(self) ;
;

abstract B inherits A :
    # Does NOT implement method() – still abstract
;

class C inherits B :
    fn method(self):
        print("Implemented!")
    ;   # Now concrete
;

let c = C()
c->method()   # Implemented!
```

A class is concrete only if it implements **all** abstract methods from its entire inheritance chain.

```xell
# ❌ Error: class 'B' does not implement abstract method 'method' from 'A'
abstract B inherits A ;   # B is still abstract
```

---

## Abstract Classes Cannot Be Instantiated

```xell
abstract Shape :
    fn area(self) ;
;

# let s = Shape()   # ❌ TypeError – cannot instantiate abstract class
```

**Why:** Abstract classes are incomplete – they may have unimplemented methods. Instantiation is blocked to prevent calling undefined methods at runtime.

---

## Abstract Methods with Default Implementations

Sometimes you want a method to be overridable but provide a default implementation.

```xell
abstract Animal :
    fn speak(self) ;   # abstract – subclasses must implement

    fn describe(self):
        print("I am an animal")   # concrete – can be overridden
    ;
;

class Dog inherits Animal :
    fn speak(self):
        print("Woof!")
    ;

    # describe() is inherited as‑is
;

let d = Dog()
d->speak()      # Woof!
d->describe()   # I am an animal
```

---

## Abstract Classes vs. Interfaces

| Aspect | Abstract Class | Interface |
|--------|----------------|-----------|
| Purpose | Provide partial implementation + contract | Define a contract only |
| Fields | ✅ Can have fields | ❌ Cannot have fields |
| Concrete methods | ✅ Can have concrete methods | ❌ No method bodies |
| Abstract methods | ✅ Can have abstract methods | ✅ All methods are abstract |
| `__init__` | ✅ Can have `__init__` | ❌ No constructor |
| Multiple | ❌ Single inheritance (but multiple inheritance allowed) | ✅ Multiple interfaces allowed |
| Instantiation | ❌ Cannot instantiate | ❌ Cannot instantiate |
| Use when | You need to share code and state | You need to define a capability |

**When to use an abstract class:**
- You have a clear "is‑a" hierarchy.
- You need to share state (fields) across subclasses.
- You have some common implementation that subclasses can reuse.
- You need a constructor (`__init__`) to initialize shared state.

**When to use an interface:**
- You want to define a capability that many unrelated classes can implement.
- You don't need to share state or implementation.
- You want a class to implement multiple contracts.

---

## Abstract Classes and Mixins

Abstract classes can be combined with mixins.

```xell
abstract Shape :
    fn area(self) ;
;

mixin Loggable :
    fn log(self, msg):
        print("[LOG] " + msg)
    ;
;

class Circle inherits Shape with Loggable :
    radius = 0

    fn __init__(self, radius):
        self->radius = radius
    ;

    fn area(self):
        give PI * self->radius ** 2
    ;

    fn describe(self):
        self->log("Circle with radius " + str(self->radius))
    ;
;

let c = Circle(5)
c->describe()   # [LOG] Circle with radius 5
```

---

## Abstract Classes and `is`

The `is` operator works with abstract classes – an instance of a concrete subclass is considered an instance of the abstract class.

```xell
abstract Shape ;
class Circle inherits Shape ;

let c = Circle()
print(c is Shape)   # true
print(c is Circle)  # true
```

### `belong` in `incase` Does NOT Work with Abstract Classes

```xell
abstract Shape ;
class Circle inherits Shape ;

let c = Circle()
incase c :
    belong Shape : print("shape")   # ❌ does NOT match
    belong Circle : print("circle") # ✅ matches
;
```

**Why:** `belong` uses exact name matching, not inheritance. Use `is` for polymorphic checks.

---

## Complete Example: Shape Hierarchy with Abstract Methods

```xell
abstract Shape :
    fn area(self) ;
    fn perimeter(self) ;
    fn describe(self):
        print("Area: " + str(self->area()))
        print("Perimeter: " + str(self->perimeter()))
    ;
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

class Triangle inherits Shape :
    a = 0
    b = 0
    c = 0

    fn __init__(self, a, b, c):
        self->a = a
        self->b = b
        self->c = c
    ;

    fn area(self):
        # Heron's formula
        let s = (self->a + self->b + self->c) / 2
        give sqrt(s * (s - self->a) * (s - self->b) * (s - self->c))
    ;

    fn perimeter(self):
        give self->a + self->b + self->c
    ;
;

let shapes = [
    Rectangle(4, 5),
    Circle(3),
    Triangle(3, 4, 5)
]

for shape in shapes:
    shape->describe()
    print("---")
;
```

**Expected output:**
```
Area: 20
Perimeter: 18
---
Area: 28.274333882308138
Perimeter: 18.84955592153876
---
Area: 6.0
Perimeter: 12
---
```

---

## Edge Cases and Less Obvious Behavior

### 1. Abstract Methods Must Be Implemented

If a concrete class misses an abstract method, a `TypeError` is thrown at **definition time**.

```xell
abstract Shape :
    fn area(self) ;
    fn perimeter(self) ;
;

class Bad inherits Shape :
    fn area(self): give 0 ;   # ❌ missing perimeter() – TypeError
;
```

### 2. Abstract Classes Can Have Concrete Methods

Abstract classes are not limited to abstract methods – they can provide full implementations.

```xell
abstract Logger :
    fn log(self, msg):   # concrete
        print("[LOG] " + msg)
    ;

    fn error(self, msg):   # concrete
        print("[ERROR] " + msg)
    ;
;
```

### 3. Abstract Methods Can Have Default Implementations?

**No.** Abstract methods cannot have bodies. If you want a default implementation, make the method concrete (with a body). Subclasses can still override it.

### 4. Abstract Classes Can Have Fields

Abstract classes can define fields that subclasses inherit.

```xell
abstract Animal :
    name = ""
    age = 0

    fn speak(self) ;   # abstract
;

class Dog inherits Animal :
    fn speak(self):
        print(self->name + " says Woof!")
    ;
;

let d = Dog({ name: "Rex", age: 3 })
d->speak()   # Rex says Woof!
```

### 5. Abstract Classes and Multiple Inheritance

An abstract class can inherit from another abstract class (or concrete class).

```xell
abstract A :
    fn method_a(self) ;
;

abstract B inherits A :
    fn method_b(self) ;
;

class C inherits B :
    fn method_a(self): print("A") ;
    fn method_b(self): print("B") ;
;
```

### 6. Abstract Methods and Type Annotations

Abstract methods can have type annotations for documentation and overload resolution.

```xell
abstract Calculator :
    fn add(self, a: int, b: int) -> int ;
;
```

### 7. Abstract Classes and `__init__`

Abstract classes can have `__init__`. Subclasses should call `parent->__init__`.

```xell
abstract Base :
    id = 0
    fn __init__(self, id):
        self->id = id
    ;
;

class Child inherits Base :
    name = ""
    fn __init__(self, id, name):
        parent->__init__(self, id)
        self->name = name
    ;
;
```

---

## What NOT to Do

### 1. Instantiating an Abstract Class

```xell
abstract Shape : fn area(self) ; ;
let s = Shape()   # ❌ TypeError – cannot instantiate abstract class
```

### 2. Forgetting to Implement an Abstract Method

```xell
abstract Shape :
    fn area(self) ;
;

class Circle inherits Shape :
    # ❌ missing area() – TypeError
;
```

### 3. Implementing an Abstract Method Incorrectly (Wrong Parameter Count)

```xell
abstract Calculator :
    fn add(self, a, b) ;
;

class Bad implements Calculator :
    fn add(self, a):   # ❌ wrong parameter count
        give a
    ;
;
```

### 4. Using `belong` for Abstract Type Checks

```xell
abstract Shape ;
class Circle inherits Shape ;

let c = Circle()
incase c :
    belong Shape : print("shape")   # ❌ does NOT match
    else : print("other")
;
```

**Use `is` instead:**

```xell
if c is Shape : print("shape")   # ✅ works
```

### 5. Declaring an Abstract Method in a Concrete Class

```xell
class Shape :
    fn area(self) ;   # ❌ error – concrete classes cannot have abstract methods
;
```

**Fix:** Make the class `abstract` or implement the method.

### 6. Forgetting That Abstract Classes Cannot Be Instantiated Even If They Have No Abstract Methods

```xell
abstract Empty :
    fn hello(self): print("hello") ;   # no abstract methods
;

# let e = Empty()   # ❌ still cannot instantiate – abstract class
```

### 7. Using `abstract` with Structs

```xell
abstract struct Point :   # ❌ error – structs cannot be abstract
    x = 0
    y = 0
;
```

**Why:** Abstract is an OOP feature for classes. Structs are value types without inheritance.

### 8. Assuming Abstract Methods Are Optional

All abstract methods **must** be implemented by concrete subclasses. There is no way to ignore them.

---

## See Also

- [Classes](oop-classes.md) – base class syntax
- [Inheritance](oop-inheritance.md) – extending abstract classes
- [Interfaces](oop-interfaces.md) – alternative to abstract classes
- [Mixins](oop-mixins.md) – composing behavior
- [Access Control](oop-access-control.md) – `protected` in abstract classes

---

**Previous:** [Magic Methods](oop-magic-methods.md)  
**Next:** [Structs](oop-structs.md)