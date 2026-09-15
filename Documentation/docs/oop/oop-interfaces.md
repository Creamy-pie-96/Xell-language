# Interfaces

> **Previous:** [Mixins](oop-mixins.md)  
> **Next:** [Access Control](oop-access-control.md)

---

Interfaces are **contracts** that define a set of methods that a class must implement. They allow you to write polymorphic code that works with any class that fulfills a specific contract, without requiring a common parent class. Interfaces in Xell are similar to those in Java or C# – they declare method signatures but provide no implementation.

Think of interfaces as **blueprints for behavior** – they tell you what methods a class must have, but not how they work.

---

## Quick Reference

| Keyword/Syntax | What it does | Minimal example |
|----------------|--------------|-----------------|
| `interface Name : ... ;` | Defines an interface | `interface Drawable : fn draw(self) ; fn get_bounds(self) ; ;` |
| `fn method(params) ;` | Method signature (no body) in an interface | `fn area(self) ;` |
| `implements Interface1, Interface2` | Class implements one or more interfaces | `class Circle implements Drawable` |
| Interface validation | Ensures all interface methods are implemented | Automatic at class definition time |
| `obj is Interface` | Checks if `obj` implements an interface | `c is Drawable` → `true` |
| `belong Interface` in `incase` | **Exact** type name match (not interface‑aware) | `belong Drawable` → `false` for a `Circle` |
| Multiple interfaces | A class can implement many interfaces | `class Shape implements Drawable, Serializable` |
| Interface inheritance | Interfaces can inherit from other interfaces | `interface Shape extends Drawable` |

---

## Defining an Interface

Use the `interface` keyword, followed by the name, a colon, and a block of method signatures. Method signatures are declared with `;` instead of a body.

```xell
interface Drawable :
    fn draw(self) ;
    fn get_bounds(self) ;
;

interface Serializable :
    fn to_json(self) ;
    fn from_json(self, data) ;
;
```

- Method signatures include the method name, parameters, and optional return type annotation.
- Bodies are **not** allowed – interfaces are pure contracts.
- The block ends with `;`.

### Interface Method Signatures

```xell
interface Calculator :
    fn add(self, a: int, b: int) -> int ;      # with type annotations
    fn subtract(self, a, b) ;                  # without annotations
    fn multiply(self, a, b) -> int ;           # with return type only
;
```

- Parameter names are required (they document the contract).
- Type annotations are optional but recommended for clarity.
- Return type annotations (using `-> Type`) are optional.

---

## Implementing an Interface

A class implements one or more interfaces using the `implements` keyword after the class name (and after `inherits` and `with` if present).

```xell
class Circle implements Drawable :
    radius = 0
    center_x = 0
    center_y = 0

    fn __init__(self, radius, x = 0, y = 0):
        self->radius = radius
        self->center_x = x
        self->center_y = y
    ;

    fn draw(self):
        print("Drawing circle at (" + str(self->center_x) + ", " + str(self->center_y) + ") with radius " + str(self->radius))
    ;

    fn get_bounds(self):
        give {
            "x": self->center_x - self->radius,
            "y": self->center_y - self->radius,
            "width": self->radius * 2,
            "height": self->radius * 2
        }
    ;
;

let c = Circle(5, 10, 10)
c->draw()
print(c->get_bounds())
```

**Expected output:**
```
Drawing circle at (10, 10) with radius 5
{"x": 5, "y": 5, "width": 10, "height": 10}
```

---

## Multiple Interfaces

A class can implement any number of interfaces.

```xell
interface Drawable :
    fn draw(self) ;
;

interface Resizable :
    fn resize(self, factor) ;
;

interface Serializable :
    fn to_json(self) ;
;

class Shape implements Drawable, Resizable, Serializable :
    width = 0
    height = 0

    fn draw(self):
        print("Drawing shape " + str(self->width) + "x" + str(self->height))
    ;

    fn resize(self, factor):
        self->width *= factor
        self->height *= factor
    ;

    fn to_json(self):
        give "{ \"width\": " + str(self->width) + ", \"height\": " + str(self->height) + " }"
    ;
;

let s = Shape({ width: 10, height: 20 })
s->draw()             # Drawing shape 10x20
s->resize(2)
print(s->to_json())   # { "width": 20, "height": 40 }
```

**Expected output:**
```
Drawing shape 10x20
{ "width": 20, "height": 40 }
```

---

## Interface Method Validation

When a class `implements` an interface, Xell verifies that **all** interface methods are implemented:

- The method must exist in the class (or its parents/mixins).
- The method must have the **same number of parameters** as the interface signature.
- Parameter types are **not** validated (the annotations are for documentation only).
- Return types are **not** validated.

```xell
interface Drawable :
    fn draw(self) ;
    fn get_bounds(self) ;
;

class BadCircle implements Drawable :
    radius = 0
    fn draw(self): print("draw") ;
    # ❌ missing get_bounds() – TypeError at definition time
;

# ❌ Error: class 'BadCircle' does not implement method 'get_bounds' required by interface 'Drawable'
```

### Parameter Count Mismatch

```xell
interface Calculator :
    fn add(self, a, b) ;
;

class BadCalc implements Calculator :
    fn add(self, a):   # ❌ only one parameter – should be two
        give a
    ;
;

# ❌ Error: method 'add' in class 'BadCalc' has 1 parameter(s), but interface 'Calculator' requires 2
```

---

## Interfaces and Inheritance

A class can both inherit from a parent class **and** implement interfaces.

```xell
class Animal :
    name = ""
    fn speak(self): print("?") ;
;

interface Pet :
    fn play(self) ;
;

class Dog inherits Animal implements Pet :
    fn speak(self): print("Woof!") ;
    fn play(self): print("Playing fetch!") ;
;

let d = Dog({ name: "Rex" })
d->speak()   # Woof! (overrides Animal)
d->play()    # Playing fetch!
```

**Order in class definition:**
```
class Name inherits Parent1, Parent2 with Mixin1, Mixin2 implements Interface1, Interface2
```

- `inherits` parents come first.
- `with` mixins come second.
- `implements` interfaces come last.

---

## Interfaces and Mixins

Mixins can provide implementations for interface methods, making it easy to reuse interface implementations across multiple classes.

```xell
interface Loggable :
    fn log(self, msg) ;
;

mixin ConsoleLogger :
    fn log(self, msg):
        print("[LOG] " + msg)
    ;
;

class App with ConsoleLogger implements Loggable :
    fn run(self):
        self->log("App started")
    ;
;

let app = App()
app->run()   # [LOG] App started
```

The class implements `Loggable` by inheriting the `log` method from the `ConsoleLogger` mixin – the validation passes.

---

## Interfaces and `is`

The `is` operator works with interfaces – an instance of a class that implements an interface is considered an instance of that interface type.

```xell
interface Drawable ;
class Circle implements Drawable ;

let c = Circle()
print(c is Drawable)   # true
print(c is Circle)     # true
```

### Polymorphic Dispatch with Interfaces

```xell
interface Renderable :
    fn render(self) ;
;

class Circle implements Renderable :
    fn render(self): print("Rendering circle") ;
;

class Square implements Renderable :
    fn render(self): print("Rendering square") ;
;

fn render_all(objects):
    for obj in objects:
        if obj is Renderable :
            obj->render()
        else :
            print("Warning: object is not renderable")
    ;
;

let shapes = [Circle(), Square()]
render_all(shapes)
```

**Expected output:**
```
Rendering circle
Rendering square
```

---

## Interface Inheritance

Interfaces can inherit from other interfaces using `inherits` (just like classes).

```xell
interface Drawable :
    fn draw(self) ;
;

interface AnimatedDrawable inherits Drawable :
    fn animate(self, time) ;
;

class Sprite implements AnimatedDrawable :
    fn draw(self):
        print("Drawing sprite")
    ;

    fn animate(self, time):
        print("Animating at time " + str(time))
    ;
;

let s = Sprite()
s->draw()      # Drawing sprite
s->animate(42) # Animating at time 42
```

- An interface that inherits from another interface **adds** methods to the contract.
- A class that implements the child interface must implement **all** methods from **both** interfaces.

---

## Interface as a Type

You can use interfaces as types in type annotations (though annotations are not enforced at runtime).

```xell
interface Drawable ;

fn process(obj: Drawable):   # annotates that obj should implement Drawable
    obj->draw()
;
```

The annotation is for documentation and static analysis only – it doesn't enforce the contract at runtime (Xell is dynamically typed).

---

## Interface vs. Abstract Class

| Aspect | Interface | Abstract Class |
|--------|-----------|----------------|
| Purpose | Defines a contract (what methods) | Provides a partial implementation |
| Methods | Only signatures, no bodies | Can have both abstract and concrete methods |
| Fields | ❌ Cannot have fields | ✅ Can have fields |
| `__init__` | ❌ No constructor | ✅ Can have `__init__` |
| Multiple | ✅ A class can implement many interfaces | ⚠️ A class can inherit from only one parent class (multiple inheritance allowed) |
| Instantiation | ❌ Cannot be instantiated | ❌ Cannot be instantiated (if abstract) |
| Use when | You need to define a capability | You need to share code and state |

**When to use an interface:**
- You want to define a capability that many unrelated classes can implement.
- You don't need to share state or implementation.
- You want a class to implement multiple contracts.

**When to use an abstract class:**
- You need to share state (`fields`) across subclasses.
- You have some methods implemented but others left abstract.
- You have a clear "is‑a" hierarchy.

---

## Complete Example: Drawing Application

```xell
interface Drawable :
    fn draw(self) ;
;

interface Resizable :
    fn resize(self, factor) ;
    fn get_area(self) ;
;

interface Clickable :
    fn on_click(self, x, y) ;
;

abstract Shape :
    x = 0
    y = 0
    color = "black"

    fn set_color(self, color):
        self->color = color
    ;

    fn get_position(self):
        give { "x": self->x, "y": self->y }
    ;
;

class Circle inherits Shape implements Drawable, Resizable :
    radius = 0

    fn __init__(self, radius, x = 0, y = 0):
        self->radius = radius
        self->x = x
        self->y = y
    ;

    fn draw(self):
        print("Drawing circle at (" + str(self->x) + ", " + str(self->y) + ") radius " + str(self->radius) + " color " + self->color)
    ;

    fn resize(self, factor):
        self->radius *= factor
    ;

    fn get_area(self):
        give PI * self->radius ** 2
    ;
;

class Rectangle inherits Shape implements Drawable, Resizable, Clickable :
    width = 0
    height = 0

    fn __init__(self, width, height, x = 0, y = 0):
        self->width = width
        self->height = height
        self->x = x
        self->y = y
    ;

    fn draw(self):
        print("Drawing rectangle at (" + str(self->x) + ", " + str(self->y) + ") " + str(self->width) + "x" + str(self->height) + " color " + self->color)
    ;

    fn resize(self, factor):
        self->width *= factor
        self->height *= factor
    ;

    fn get_area(self):
        give self->width * self->height
    ;

    fn on_click(self, click_x, click_y):
        if click_x >= self->x and click_x <= self->x + self->width and click_y >= self->y and click_y <= self->y + self->height :
            print("Rectangle clicked!")
        else :
            print("Missed rectangle")
    ;
;

let shapes = [
    Circle(5, 10, 10),
    Rectangle(20, 10, 50, 50)
]

for shape in shapes:
    shape->draw()
    if shape is Resizable :
        shape->resize(2)
        print("New area: " + str(shape->get_area()))
    if shape is Clickable :
        shape->on_click(55, 55)
;
```

**Expected output:**
```
Drawing circle at (10, 10) radius 5 color black
New area: 314.1592653589793
Drawing rectangle at (50, 50) 20x10 color black
New area: 800
Rectangle clicked!
```

---

## Edge Cases and Less Obvious Behavior

### 1. Interfaces Cannot Have Fields

```xell
interface Bad :
    name = ""   # ❌ error – interfaces cannot have fields
    fn greet(self) ;
;
```

**Why:** Interfaces are pure contracts – they define what methods a class must have, not what data it stores. Use an abstract class if you need to share fields.

### 2. Interface Method Bodies Are Not Allowed

```xell
interface Bad :
    fn greet(self):
        print("Hello")   # ❌ error – interfaces cannot have bodies
    ;
;
```

**Why:** Interfaces only declare methods; they don't provide implementations. Use an abstract class or mixin for implementation.

### 3. Interfaces Cannot Be Instantiated

```xell
interface Drawable ;
let d = Drawable()   # ❌ TypeError – cannot instantiate interface
```

**Why:** Interfaces are not constructible – they only define contracts. Use a class that implements the interface.

### 4. `belong` in `incase` Does NOT Match Interfaces

```xell
interface Drawable ;
class Circle implements Drawable ;

let c = Circle()
incase c :
    belong Drawable : print("drawable")   # ❌ does NOT match
    belong Circle : print("circle")       # ✅ matches
;
```

**Why:** `belong` uses exact type name matching, not interface implementation checks. Use `is` for interface checks:

```xell
if c is Drawable : print("drawable")   # ✅ works
```

### 5. Interfaces and Multiple Inheritance

An interface can inherit from multiple interfaces (same syntax as classes).

```xell
interface A :
    fn method_a(self) ;
;

interface B :
    fn method_b(self) ;
;

interface C inherits A, B :
    fn method_c(self) ;
;

class Impl implements C :
    fn method_a(self): print("A") ;
    fn method_b(self): print("B") ;
    fn method_c(self): print("C") ;
;

let i = Impl()
i->method_a()   # A
i->method_b()   # B
i->method_c()   # C
```

### 6. Interface Validation Happens at Class Definition Time

```xell
interface Logger :
    fn log(self, msg) ;
;

class App implements Logger :
    # Missing log() method
;

# ❌ Error is thrown immediately when the class is defined
```

This is a compile‑time (parser‑time) check, not a runtime check.

### 7. Mixin Methods Count Toward Interface Implementation

```xell
interface Loggable :
    fn log(self, msg) ;
;

mixin ConsoleLogger :
    fn log(self, msg): print(msg) ;
;

class App with ConsoleLogger implements Loggable :
    # log() comes from the mixin – validation passes
;

let a = App()
a->log("test")   # test
```

### 8. Parent Methods Count Toward Interface Implementation

```xell
interface Drawable :
    fn draw(self) ;
;

class Parent :
    fn draw(self): print("draw") ;
;

class Child inherits Parent implements Drawable :
    # draw() inherited from Parent – validation passes
;

let c = Child()
c->draw()   # draw
```

---

## What NOT to Do

### 1. Defining a Method with Different Parameter Count Than the Interface

```xell
interface Calculator :
    fn add(self, a, b) ;
;

class Bad implements Calculator :
    fn add(self, a):   # ❌ one parameter instead of two
        give a
    ;
;
```

**Why it fails:** The parser validates parameter counts and throws a `TypeError` with details about the mismatch.

### 2. Forgetting to Implement All Interface Methods

```xell
interface Full :
    fn method1(self) ;
    fn method2(self) ;
;

class Partial implements Full :
    fn method1(self): print("one") ;
    # ❌ missing method2() – TypeError
;
;
```

### 3. Using `belong` for Interface Checks

```xell
interface Drawable ;
class Circle implements Drawable ;

let c = Circle()
incase c :
    belong Drawable : print("drawable")   # ❌ false – use is instead
    else : print("other")
;
```

**Why it fails:** `belong` matches the exact type name, not interface implementation. Use the `is` operator:

```xell
if c is Drawable : print("drawable")
```

### 4. Implementing an Interface in a Struct

```xell
interface Drawable ;
struct Point implements Drawable :   # ❌ error – structs cannot implement interfaces
    x = 0
    y = 0
    fn draw(self): print("draw") ;
;
```

**Why it fails:** Interfaces are an OOP feature for classes. Structs are value types and cannot implement interfaces.

### 5. Trying to Instantiate an Interface

```xell
interface Drawable ;
let d = Drawable()   # ❌ TypeError
```

### 6. Adding Implementation Code in an Interface

```xell
interface Drawable :
    fn draw(self):
        print("draw")   # ❌ error – interface methods cannot have bodies
    ;
;
```

**Why it fails:** The parser expects `;` after the method signature, not a body.

### 7. Expecting Interface Inheritance to Inherit Implementations

```xell
interface A :
    fn method(self) ;
;

interface B inherits A :
    # Does NOT inherit A's methods – just adds to the contract
;

class C implements B :
    fn method(self): print("ok") ;   # Must implement method() here
;
```

**Why:** Interface inheritance is about **contract composition**, not implementation reuse. Each interface method must still be implemented by the class.

### 8. Assuming Interface Methods Are Automatically Added to the Class

```xell
interface Drawable ;
class Circle implements Drawable :
    # ❌ error – draw() is not automatically added
;
```

**Why it fails:** Interfaces only declare requirements – they don't add methods to the class. You must implement every method explicitly.

### 9. Implementing an Interface Without a Full `__init__`

Interfaces don't require constructors, but if your class uses `__init__` logic, ensure it's complete:

```xell
interface Configurable :
    fn set_config(self, config) ;
;

class App implements Configurable :
    config = {}

    fn set_config(self, config):
        self->config = config
    ;

    # __init__ is optional – interface doesn't require it
;

let a = App()
a->set_config({ "mode": "debug" })
print(a->config)   # {"mode": "debug"}
```

---

## See Also

- [Classes](oop-classes.md) – implementing interfaces in classes
- [Inheritance](oop-inheritance.md) – extending classes with `inherits`
- [Mixins](oop-mixins.md) – composing behavior with `with`
- [Abstract Classes](oop-abstract.md) – abstract methods and classes
- [Access Control](oop-access-control.md) – `protected` and interface methods
- [Magic Methods](oop-magic-methods.md) – operator overloading and protocols

---

**Previous:** [Mixins](oop-mixins.md)  
**Next:** [Access Control](oop-access-control.md)