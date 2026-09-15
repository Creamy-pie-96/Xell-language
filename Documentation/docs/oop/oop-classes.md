# Classes

> **Previous:** [Structs](oop-structs.md)  
> **Next:** [Inheritance](oop-inheritance.md)

---

Classes are Xell's **reference types** – the foundation of object‑oriented programming. They support inheritance, mixins, interfaces, access control, properties, and a rich set of magic methods. Unlike structs (which are value types), classes are shared by reference and can model complex, stateful objects with identity.

---

## Quick Reference

| Keyword/Syntax | What it does | Minimal example |
|----------------|--------------|-----------------|
| `class Name : ... ;` | Defines a class | `class Person : name = "" age = 0 ;` |
| `abstract Name : ... ;` | Defines an abstract class | `abstract Shape : ... ;` |
| `mixin Name : ... ;` | Defines a mixin | `mixin Logger : fn log(self): ... ;` |
| `inherits Parent1, Parent2` | Multiple inheritance | `class Dog inherits Animal, Pet` |
| `with Mixin1, Mixin2` | Mixin composition | `class User with Loggable, Serializable` |
| `implements Interface1, Interface2` | Interface implementation | `class Circle implements Drawable` |
| `__init__(self, ...)` | Constructor | `fn __init__(self, name): self->name = name ;` |
| `__str__(self)` | String representation | `fn __str__(self): give self->name ;` |
| `__print__(self)` | Print representation | `fn __print__(self): print(self->name) ;` |
| `__len__(self)` | Length protocol | `fn __len__(self): give len(self->items) ;` |
| `__contains__(self, item)` | Membership protocol | `fn __contains__(self, item): give item in self->items ;` |
| `__iter__(self)` | Iteration protocol | `fn __iter__(self): give self->items ;` |
| `__next__(self)` | Iterator protocol | `fn __next__(self): give next(self->items) ;` |
| `__call__(self, ...)` | Callable object | `fn __call__(self, x): give x * 2 ;` |
| `__hash__(self)` | Hashing protocol | `fn __hash__(self): give hashme(self->id) ;` |
| `__add__(self, other)` | Operator overloading | `fn __add__(self, other): give self->value + other ;` |
| `public: / protected: / private:` | Access control labels | `private: let secret = "hidden" ;` |
| `static fn name(...)` | Static method | `static fn create(): give Class() ;` |
| `static field = value` | Static field | `static count = 0 ;` |
| `get name(self) : ... ;` | Property getter | `get area(self): give self->width * self->height ;` |
| `set name(self, val) : ... ;` | Property setter | `set area(self, val): ... ;` |
| `Name(args...)` | Instantiate (positional) | `Person("Alice", 30)` |
| `~Name(args...)` | Frozen instance (immutable) | `~Config({ url: "https://..." })` |
| `obj->field` / `obj->method()` | Member access | `p->name`, `p->greet()` |
| `parent->method()` | Call parent method | `parent->__init__(self)` |
| `@dataclass` | Data class marker | `@dataclass class Point : x = 0 y = 0 ;` |
| `@immutable` | Immutable class | `@immutable class Config : ... ;` |
| `@singleton` | Singleton class | `@singleton class App : ... ;` |

---

## Defining a Class

Use the `class` keyword, followed by the name, a colon, and a block containing fields, methods, and access control sections.

```xell
class Person :
    name = "unknown"
    age = 0

    fn __init__(self, name, age):
        self->name = name
        self->age = age
    ;

    fn greet(self):
        print("Hello, I'm " + self->name)
    ;
;
```

- The block ends with `;`.
- Fields must have default values (like structs).
- `__init__` is the constructor – it runs when an instance is created.

---

## Constructors: `__init__`

The `__init__` method is the constructor. It's called when you instantiate a class.

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
print(p->name)   # "Alice"
print(p->age)    # 30
```

- `__init__` is optional. If omitted, fields keep their default values.
- The first parameter is always `self` (the instance being constructed).
- You can have any number of additional parameters.
- If a field is not set in `__init__`, it keeps its default value.

### Overloading `__init__`

Xell supports overloaded constructors using type annotations:

```xell
class Person :
    name = ""
    age = 0

    fn __init__(self, name: string, age: int):
        self->name = name
        self->age = age
    ;

    fn __init__(self, name: string):
        self->name = name
        self->age = 0
    ;
;
```

The interpreter selects the best‑matching `__init__` based on argument types and count.

---

## Instantiation

### Positional Arguments

Arguments are matched to `__init__` parameters in order.

```xell
let p = Person("Alice", 30)
```

### Named Arguments

Use field names in a map literal (or shorthand) to specify values:

```xell
let p = Person({ name: "Alice", age: 30 })
let p2 = Person(name: "Bob")   # age defaults to 0
```

- Named arguments can appear in any order.
- They can be mixed with positional arguments (positional first).

### Default Construction

If `__init__` takes no parameters (or all parameters have defaults), you can instantiate with no arguments:

```xell
class DefaultPerson :
    name = "unknown"
    fn __init__(self): pass ;
;

let p = DefaultPerson()
```

---

## Frozen Instances: `~`

Use the `~` prefix to create an **immutable** (frozen) instance. Once frozen, fields cannot be modified.

```xell
@immutable
class Config :
    url = "https://api.example.com"
    timeout = 30

    fn __init__(self, url, timeout):
        self->url = url
        self->timeout = timeout
    ;
;

let c = ~Config("https://secure.api.com", 60)
c->url = "new"   # ❌ throws ImmutabilityError
```

- `~` works on **any** class, not just `@immutable` ones.
- If the class is `@immutable`, **all** instances are frozen after `__init__` completes (the `~` is redundant but harmless).
- Frozen instances are useful for configuration objects, constants, and immutable data transfer objects.

---

## Instance Methods

Methods are functions defined inside the class body. They always take `self` as the first parameter.

```xell
class Counter :
    value = 0

    fn increment(self):
        self->value += 1
    ;

    fn get(self):
        give self->value
    ;
;

let c = Counter()
c->increment()
print(c->get())   # 1
```

### Method Access

- Methods are accessed with `->` (like fields).
- The `self` parameter is automatically passed when you call `obj->method()`.

---

## Magic Methods (Protocols)

Magic methods enable classes to integrate with Xell's built‑in features.

### `__str__` – String Representation

Called by `str()` and `print()` for user‑friendly output.

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
print(p)        # Person(Alice, 30)  (print calls __str__)
```

### `__print__` – Print Representation

Called by `print()` if `__str__` is not defined, or for custom printing logic.

```xell
class Logger :
    name = ""

    fn __print__(self):
        print("LOGGER: " + self->name)
    ;
;
```

### `__len__` – Length Protocol

Called by `len()`.

```xell
class Collection :
    items = []

    fn __len__(self):
        give len(self->items)
    ;
;

let c = Collection()
print(len(c))   # 0
```

### `__contains__` – Membership Protocol

Called by the `in` operator.

```xell
class Bag :
    items = []

    fn __contains__(self, item):
        give contains(self->items, item)
    ;
;

let b = Bag()
print("apple" in b)   # false (or true if present)
```

### `__iter__` and `__next__` – Iteration Protocol

Called by `for` loops and iteration contexts.

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
    print(val)   # 0,1,2,3,4
;
```

### `__call__` – Callable Objects

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
```

### `__hash__` – Hashing Protocol

Called when an instance is used as a set element or map key. The instance must be frozen (`frozen = true`) for `__hash__` to be called.

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
```

### `__enter__` and `__exit__` – Context Manager Protocol

See [RAII](../core/raii.md) for details.

```xell
class File :
    path = ""
    handle = none

    fn __enter__(self):
        self->handle = open(self->path, "r")
        give self->handle
    ;

    fn __exit__(self):
        if self->handle : close(self->handle)
    ;
;
```

---

## Operator Overloading

Define magic methods to overload operators.

| Operator | Magic Method |
|----------|--------------|
| `+` | `__add__` |
| `-` | `__sub__` |
| `*` | `__mul__` |
| `/` | `__div__` |
| `%` | `__mod__` |
| `**` | `__pow__` |
| `&` | `__and__` |
| `|` | `__or__` |
| `^` | `__xor__` |
| `<<` | `__lshift__` |
| `>>` | `__rshift__` |
| `|>` | `__pipe__` |
| `==` | `__eq__` |
| `!=` | `__ne__` (fallback to `__eq__` negation) |
| `<` | `__lt__` |
| `>` | `__gt__` |
| `<=` | `__le__` |
| `>=` | `__ge__` |
| `-` (unary) | `__neg__` |
| `~` | `__invert__` |
| `[]` (get) | `__get__` |
| `[]` (set) | `__set__` |

**Example: Vector class**

```xell
class Vector :
    x = 0.0
    y = 0.0

    fn __add__(self, other):
        give Vector(self->x + other->x, self->y + other->y)
    ;

    fn __sub__(self, other):
        give Vector(self->x - other->x, self->y - other->y)
    ;

    fn __mul__(self, scalar):
        if scalar is float or scalar is int :
            give Vector(self->x * scalar, self->y * scalar)
        else :
            throw TypeError("scalar must be a number")
    ;

    fn __eq__(self, other):
        give self->x == other->x and self->y == other->y
    ;

    fn __str__(self):
        give "Vector(" + str(self->x) + ", " + str(self->y) + ")"
    ;
;

let v1 = Vector(1, 2)
let v2 = Vector(3, 4)
let v3 = v1 + v2   # Vector(4, 6)
let v4 = v2 - v1   # Vector(2, 2)
let v5 = v1 * 3    # Vector(3, 6)
print(v1 == v2)    # false
```

---

## Access Control: `public`, `protected`, `private`

Use access labels inside the class body to control visibility.

```xell
class Account :
    # Everything before the first label is public by default
    owner = ""

    private:
        balance = 0.0

    protected:
        account_id = ""

    public:  # back to public (redundant but allowed)
        fn deposit(self, amount):
            if amount > 0 :
                self->balance += amount
        ;

        fn get_balance(self):
            give self->balance
        ;
;
```

### Access Rules

| Access Level | Visibility |
|--------------|------------|
| `public` | Accessible from anywhere. This is the default. |
| `protected` | Accessible from the class itself and subclasses. |
| `private` | Accessible only from the class itself. |

```xell
let a = Account({ owner: "Alice" })
a->deposit(100)          # OK – public
a->get_balance()         # OK – public
print(a->balance)        # ❌ private – cannot access
print(a->account_id)     # ❌ protected – cannot access from outside
```

### Static Members and Access

Static members follow the same access rules as instance members.

---

## Static Members

### Static Fields

Shared across all instances of the class.

```xell
class Counter :
    static count = 0

    fn __init__(self):
        Counter->count += 1
    ;
;

let a = Counter()
let b = Counter()
print(Counter->count)   # 2
```

### Static Methods

Called on the class itself, not on instances. They do not receive `self`.

```xell
class Factory :
    static fn create(name):
        give Factory({ name: name })
    ;

    name = ""
;

let obj = Factory->create("item")
```

### Accessing Static Members

- Static fields: `ClassName->field`
- Static methods: `ClassName->method()`

---

## Properties: `get` / `set`

Properties allow you to define getter and setter methods that are accessed like fields.

```xell
class Rectangle :
    width = 0
    height = 0

    get area(self):
        give self->width * self->height
    ;

    set area(self, val):
        # Can't set area directly – throw or ignore
        throw AttributeError("area is read‑only")
    ;

    get perimeter(self):
        give 2 * (self->width + self->height)
    ;
;

let r = Rectangle({ width: 10, height: 5 })
print(r->area)        # 50
print(r->perimeter)   # 30
r->area = 100         # ❌ read‑only
```

### Read‑only Properties

If no `set` is defined, the property is read‑only.

### Write‑only Properties

If no `get` is defined, the property is write‑only (rarely used).

```xell
class Secure :
    set password(self, val):
        self->hash = hashme(val)
    ;
;
```

### Properties vs. Fields

- **Fields** store data directly (`self->name = "Alice"`).
- **Properties** are methods that are invoked when you access `obj->property`.

---

## Abstract Classes

Abstract classes cannot be instantiated directly. They may contain abstract methods (declared with `;` instead of a body) that subclasses must implement.

```xell
abstract Shape :
    fn area(self) ;   # abstract method – no body

    fn describe(self):
        print("This is a shape")
    ;
;

class Circle inherits Shape :
    radius = 0

    fn area(self):
        give PI * self->radius ** 2
    ;
;

# let s = Shape()   # ❌ error – cannot instantiate abstract class
let c = Circle({ radius: 5 })
print(c->area())    # 78.54
```

- If a subclass does not implement all abstract methods, it remains abstract.
- Abstract methods use `;` instead of a body (`: ... ;`).

---

## Mixins

Mixins are classes that provide reusable methods but cannot be instantiated. They are composed into classes using `with`.

```xell
mixin Loggable :
    fn log(self, msg):
        print("[LOG] " + msg)
    ;
;

mixin Serializable :
    fn to_json(self):
        # convert self to JSON
        give "{}"
    ;
;

class User with Loggable, Serializable :
    name = ""
    age = 0
;

let u = User({ name: "Alice", age: 30 })
u->log("User created")   # [LOG] User created
```

- Mixins cannot have `__init__`.
- Mixins are searched **after** parents but **before** the class's own methods? Actually, own methods > parents > mixins (from source: `XStructDef::findMethod` loops own methods, then parents, then mixins).
- Mixins cannot be instantiated.

---

## Interfaces

Interfaces define contracts that implementing classes must satisfy.

```xell
interface Drawable :
    fn draw(self) ;
    fn get_bounds(self) ;
;

class Circle implements Drawable :
    radius = 0

    fn draw(self):
        print("Drawing circle")
    ;

    fn get_bounds(self):
        give [ -self->radius, -self->radius, self->radius, self->radius ]
    ;
;

class Square implements Drawable :
    side = 0

    fn draw(self):
        print("Drawing square")
    ;

    fn get_bounds(self):
        give [ 0, 0, self->side, self->side ]
    ;
;
```

- If a class `implements` an interface, it must provide **all** methods declared in the interface.
- Parameter counts and types are validated at definition time.
- Interfaces cannot be instantiated.

---

## Class Decorators

### `@dataclass`

Marks a class as a data class. Currently a placeholder – it documents intent but does not generate any code.

```xell
@dataclass
class Point :
    x = 0
    y = 0
;
```

### `@immutable`

Makes **all** instances of the class immutable after `__init__` completes.

```xell
@immutable
class Config :
    url = "https://api.example.com"
    timeout = 30

    fn __init__(self, url, timeout):
        self->url = url
        self->timeout = timeout
    ;
;

let c = Config("https://secure.api.com", 60)
c->url = "new"   # ❌ ImmutabilityError
```

### `@singleton`

Ensures only one instance of the class is ever created.

```xell
@singleton
class App :
    name = ""

    fn __init__(self, name):
        self->name = name
        print("App created")
    ;
;

let a = App("Xell")   # prints "App created"
let b = App("Other")  # returns same instance, no print
print(a is b)         # true
```

---

## Complete Example

```xell
@singleton
@immutable
class DatabaseConfig :
    host = "localhost"
    port = 5432
    user = ""
    password = ""

    fn __init__(self, host, port, user, password):
        self->host = host
        self->port = port
        self->user = user
        self->password = password
    ;

    get connection_string(self):
        give "postgresql://" + self->user + ":" + self->password + "@" + self->host + ":" + str(self->port)
    ;
;

let config = DatabaseConfig("db.example.com", 5432, "admin", "secret")
print(config->connection_string)   # postgresql://admin:secret@db.example.com:5432

# config->host = "new"   # ❌ ImmutabilityError (frozen)
```

---

## What NOT to Do

### 1. Defining `__init__` in a mixin

```xell
mixin Logger :
    fn __init__(self): ... ;   # ❌ error – mixins cannot have __init__
;
```

**Why it fails:** The parser explicitly checks and rejects `__init__` in mixins.

### 2. Instantiating an abstract class

```xell
abstract Shape :
    fn area(self) ;
;

let s = Shape()   # ❌ TypeError – cannot instantiate abstract class
```

### 3. Instantiating an interface

```xell
interface Drawable :
    fn draw(self) ;
;

let d = Drawable()   # ❌ TypeError – cannot instantiate interface
```

### 4. Instantiating a mixin

```xell
mixin Loggable :
    fn log(self): print("log") ;
;

let l = Loggable()   # ❌ TypeError – cannot instantiate mixin
```

### 5. Forgetting to implement all interface methods

```xell
interface Drawable :
    fn draw(self) ;
    fn get_bounds(self) ;
;

class Circle implements Drawable :
    radius = 0
    fn draw(self): print("draw") ;
    # ❌ missing get_bounds – TypeError at definition time
;
```

### 6. Using `parent` outside a method

```xell
class Child inherits Parent :
    fn method(self):
        parent->some_method()   # ✅ OK inside method
    ;
;

parent->some_method()   # ❌ error – parent is only available inside methods
```

### 7. Expecting `protected` to be accessible from subclasses in a different module

```xell
# In module A:
class Parent :
    protected:
        secret = 42
;

# In module B:
class Child inherits Parent :
    fn peek(self):
        give self->secret   # ✅ OK (same class hierarchy)
;

# Outside:
let c = Child()
print(c->secret)   # ❌ AccessError – protected not accessible from outside
```

### 8. Using `@immutable` and then trying to modify a field after `__init__`

```xell
@immutable
class Counter :
    value = 0

    fn increment(self):
        self->value += 1   # ❌ ImmutabilityError
    ;
;

let c = Counter()
c->increment()   # ❌ – cannot modify immutable instance
```

### 9. Creating cyclic references with `@singleton`

```xell
@singleton
class A :
    b = none
    fn __init__(self): A->b = B() ;
;

@singleton
class B :
    a = none
    fn __init__(self): B->a = A() ;
;

# ❌ Infinite recursion – each singleton tries to create the other
```

**Why it fails:** `@singleton` works correctly, but cyclic dependencies in constructors cause infinite recursion.

### 10. Using `static` fields without the class name

```xell
class Counter :
    static count = 0
;

print(count)      # ❌ error – count is not in global scope
print(Counter->count)   # ✅ OK
```

---

## See Also

- [Structs](oop-structs.md) – value types vs. reference types
- [Inheritance](oop-inheritance.md) – extending classes
- [Mixins](oop-mixins.md) – composing behavior
- [Interfaces](oop-interfaces.md) – defining contracts
- [Access Control](oop-access-control.md) – `public`/`protected`/`private`
- [Properties](oop-properties.md) – getters and setters
- [Magic Methods](oop-magic-methods.md) – operator overloading
- [Abstract Classes](oop-abstract.md) – abstract methods and classes

---

**Previous:** [Structs](oop-structs.md)  
**Next:** [Inheritance](oop-inheritance.md)