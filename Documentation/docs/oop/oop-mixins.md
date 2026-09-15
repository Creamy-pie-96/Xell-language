# Mixins

> **Previous:** [Inheritance](oop-inheritance.md)  
> **Next:** [Interfaces](oop-interfaces.md)

---

Mixins are a powerful tool for **code reuse through composition**. They are classes that provide reusable methods and fields, but unlike regular classes, mixins **cannot be instantiated** and **cannot have `__init__`**. Instead, they are "mixed in" to other classes using the `with` keyword, allowing you to compose behavior from multiple sources without the rigidity of single inheritance.

Think of mixins as **bundles of functionality** that you can attach to any class that needs them – logging, serialization, validation, caching, and more.

---

## Quick Reference

| Keyword/Syntax | What it does | Minimal example |
|----------------|--------------|-----------------|
| `mixin Name : ... ;` | Defines a mixin | `mixin Loggable : fn log(self, msg): print(msg) ;` |
| `with Mixin1, Mixin2` | Composes mixins into a class | `class User with Loggable, Serializable` |
| Mixin methods | Used like regular methods on the composed class | `u->log("Hello")` |
| Mixin fields | Available as fields on the composed class | `self->log_level` |
| Mixin inheritance | Mixins can inherit from other mixins | `mixin TimestampedLoggable inherits Loggable` |
| `is` on mixins | Mixins are structs; `is` works with them | `u is Loggable` → `true` |

---

## Defining a Mixin

Use the `mixin` keyword, followed by the name, a colon, and a block of methods and fields.

```xell
mixin Loggable :
    log_level = "info"

    fn log(self, msg):
        print("[" + self->log_level + "] " + msg)
    ;

    fn error(self, msg):
        let old = self->log_level
        self->log_level = "error"
        self->log(msg)
        self->log_level = old
    ;
;
```

- Mixins can have **fields** (with default values).
- Mixins can have **methods** – these will be available on any class that uses the mixin.
- Mixins **cannot** have `__init__`.
- Mixins **cannot** be instantiated directly.

### Mixin Body Rules

- All fields must have default values (like structs and classes).
- Methods follow the same syntax as class methods (first parameter is `self`).
- The block ends with `;`.

---

## Using a Mixin: `with`

Apply one or more mixins to a class using the `with` keyword after the class name (and after `inherits` if present).

```xell
class User with Loggable :
    name = ""
    age = 0

    fn __init__(self, name, age):
        self->name = name
        self->age = age
    ;

    fn greet(self):
        self->log("Greeting from " + self->name)
        print("Hello, I'm " + self->name)
    ;
;

let u = User("Alice", 30)
u->log("User created")   # [info] User created
u->greet()               # [info] Greeting from Alice
                         # Hello, I'm Alice
```

- The `with` keyword accepts one or more mixin names separated by commas.
- Mixins are applied **in the order they are listed**.
- The composed class inherits all fields and methods from all mixins.

---

## Multiple Mixins

You can compose a class from multiple mixins.

```xell
mixin Serializable :
    fn to_json(self):
        let fields = {}
        for [key, val] in self:
            fields[key] = val
        ;
        give str(fields)
    ;
;

mixin Timestamped :
    created_at = none

    fn touch(self):
        self->created_at = now()
    ;
;

class Document with Serializable, Timestamped :
    title = ""
    content = ""

    fn __init__(self, title, content):
        self->title = title
        self->content = content
        self->touch()
    ;
;

let doc = Document("Hello", "World!")
print(doc->to_json())   # {"title": "Hello", "content": "World!", "created_at": 1234567890}
```

**Expected output:**
```
{"title": "Hello", "content": "World!", "created_at": 1234567890}
```

---

## Mixin Method Resolution Order

When a class uses multiple mixins, methods are resolved in this order:

1. **Class's own methods** – highest priority.
2. **Parents** (if any) – `inherits` parents come before mixins.
3. **Mixins** – in the order they appear in `with`.

```xell
mixin A :
    fn hello(self): print("A") ;
;

mixin B :
    fn hello(self): print("B") ;
;

class C with A, B :
    # No own hello()
;

class D with B, A :
    # Mixins in reverse order
;

let c = C()
c->hello()   # A (first mixin wins)

let d = D()
d->hello()   # B (first mixin wins)
```

### Mixin vs. Parent Priority

Parents **always** take precedence over mixins.

```xell
class Parent :
    fn hello(self): print("Parent") ;
;

mixin Mixin :
    fn hello(self): print("Mixin") ;
;

class Child inherits Parent with Mixin :
    # No own hello()
;

let c = Child()
c->hello()   # Parent (parents have higher priority than mixins)
```

**Full resolution order:**
1. Child's own methods
2. Parents (left‑to‑right)
3. Mixins (left‑to‑right)

---

## Mixins with Fields

Mixins can define fields that become part of the composed class.

```xell
mixin Counter :
    count = 0

    fn increment(self):
        self->count += 1
    ;

    fn decrement(self):
        self->count -= 1
    ;
;

class Widget with Counter :
    name = ""

    fn __init__(self, name):
        self->name = name
    ;
;

let w = Widget("button")
w->increment()
w->increment()
print(w->count)   # 2
```

Fields from mixins are **not** isolated – they become part of the class's field set.

### Field Name Conflicts

If multiple mixins define the same field name, the **last** mixin (rightmost) wins.

```xell
mixin A :
    value = 10
;

mixin B :
    value = 20
;

class C with A, B :
    # No own value
;

let c = C()
print(c->value)   # 20 (B wins)
```

**If the class itself defines the same field:**
```xell
class D with A, B :
    value = 30
;

let d = D()
print(d->value)   # 30 (class's own field wins)
```

---

## Mixins Cannot Have `__init__`

This is enforced by the parser:

```xell
mixin Logger :
    fn __init__(self):   # ❌ error – mixins cannot have __init__
        print("Logger init")
    ;
;
```

**Why?** Mixins are not meant to be instantiated on their own. Their fields are initialized by the composed class's `__init__`. If you need initialization logic in a mixin, provide an `init()` method that the class's `__init__` calls explicitly.

**Workaround:**

```xell
mixin Logger :
    log_level = "info"

    fn init_logger(self):
        # custom initialization logic
        self->log_level = "debug"
    ;
;

class App with Logger :
    fn __init__(self):
        self->init_logger()   # manually call mixin init
    ;
;
```

---

## Mixin Inheritance

Mixins can inherit from other mixins (or even classes), though this is less common.

```xell
mixin BaseLogger :
    fn log(self, msg):
        print("[LOG] " + msg)
    ;
;

mixin TimestampedLogger inherits BaseLogger :
    fn log(self, msg):
        let ts = now()
        parent->log(self, "[" + str(ts) + "] " + msg)
    ;
;

class App with TimestampedLogger :
    fn run(self):
        self->log("App started")
    ;
;

let app = App()
app->run()   # [LOG] [1234567890] App started
```

- Mixin inheritance follows the same rules as class inheritance.
- `parent` works inside mixin methods, referring to the parent mixin.
- Mixins that inherit from classes are allowed but unusual – the inherited class's methods become available as well.

---

## Mixins and `is`

The `is` operator works with mixins as well – an instance of a class that uses a mixin is considered an instance of that mixin type.

```xell
mixin Loggable ;
class User with Loggable ;

let u = User()
print(u is Loggable)   # true
print(u is User)       # true
```

This is useful for polymorphic checks:

```xell
fn process(obj):
    if obj is Loggable :
        obj->log("Processing")
    else :
        print("Not loggable")
    ;
;
```

---

## Complete Example: User Management with Mixins

```xell
mixin Validatable :
    fn validate(self):
        for [key, val] in self:
            if val == "" :
                throw ValueError(key + " cannot be empty")
        ;
        give true
    ;
;

mixin Persistable :
    fn save(self):
        print("Saving " + self->__type__)
        # simulated database save
    ;

    fn load(self, id):
        print("Loading " + self->__type__ + " with id " + str(id))
        # simulated database load
    ;
;

mixin Auditable :
    changes = []

    fn track_change(self, field, old_val, new_val):
        self->changes.append({
            "field": field,
            "old": old_val,
            "new": new_val,
            "time": now()
        })
    ;

    fn get_audit_log(self):
        give self->changes
    ;
;

class User with Validatable, Persistable, Auditable :
    id = 0
    name = ""
    email = ""

    fn __init__(self, id, name, email):
        self->id = id
        self->name = name
        self->email = email
        self->track_change("name", "", name)
        self->track_change("email", "", email)
    ;

    fn update_email(self, new_email):
        let old = self->email
        self->email = new_email
        self->track_change("email", old, new_email)
    ;

    fn save(self):
        self->validate()
        parent->save(self)   # calls Persistable::save
    ;
;

let u = User(1, "Alice", "alice@example.com")
u->update_email("alice@new.com")
u->save()

print("Audit log:")
for entry in u->get_audit_log():
    print("  " + entry["field"] + " changed from '" + entry["old"] + "' to '" + entry["new"] + "'")
;
```

**Expected output:**
```
Saving User
Audit log:
  name changed from '' to 'Alice'
  email changed from '' to 'alice@example.com'
  email changed from 'alice@example.com' to 'alice@new.com'
```

---

## Mixins vs. Inheritance vs. Composition

| Aspect | Inheritance | Mixins | Composition |
|--------|-------------|--------|-------------|
| Relationship | "is‑a" | "has‑a‑behavior" | "has‑a" |
| Code reuse | Inherits all parent methods | Injects methods into class | Delegates to contained objects |
| Flexibility | Tighter coupling | Moderate coupling | Loose coupling |
| Constructor | `__init__` can be overridden | No `__init__` allowed | Manual delegation |
| Multiple sources | Limited (multiple inheritance allowed) | Unlimited (many mixins) | Unlimited |
| Use when | You need polymorphic "is‑a" relationships | You need reusable behavior bundles | You need to delegate to specialized objects |

**When to use a mixin:**
- You have a reusable piece of functionality (logging, serialization, caching)
- The functionality doesn't require its own constructor state
- Multiple unrelated classes need the same behavior
- You want to avoid deep inheritance hierarchies

**When to use inheritance:**
- You have an "is‑a" relationship (e.g., `Dog` is an `Animal`)
- You need polymorphic behavior (methods that override parents)
- You need to share a common constructor (`__init__`)

**When to use composition:**
- You have a "has‑a" relationship (e.g., `Car` has an `Engine`)
- The contained object has its own lifecycle
- You need to delegate to different implementations at runtime

---

## Edge Cases and Less Obvious Behavior

### 1. Mixin Field Defaults Are Evaluated Once

Like structs and classes, mixin field defaults are evaluated at definition time, not per usage.

```xell
mixin Counter :
    count = 0           # same for all uses
;

class A with Counter ;
class B with Counter ;

let a = A()
let b = B()
a->count = 10
print(b->count)   # 0 (independent)
```

Each class gets its own copy of the field with the default value.

### 2. Mixin Methods Can Be Overridden

Classes can override mixin methods, just like they can override inherited methods.

```xell
mixin Logger :
    fn log(self, msg): print("LOG: " + msg) ;
;

class CustomLogger with Logger :
    fn log(self, msg): print("CUSTOM: " + msg) ;
;

let c = CustomLogger()
c->log("test")   # CUSTOM: test
```

### 3. Mixin Methods Can Call Other Mixin Methods

Mixins can call methods from other mixins that are composed together.

```xell
mixin Base :
    fn base_msg(self): give "base" ;
;

mixin Extended inherits Base :
    fn extended_msg(self): give self->base_msg() + " extended" ;
;

class C with Extended ;
```

This works because all mixin methods are available on the composed class.

### 4. `self` in Mixins Refers to the Composed Instance

When a mixin method uses `self`, it refers to the **full composed object**, not just the mixin part.

```xell
mixin Formatter :
    fn format(self):
        give "User: " + self->name   # accesses the class's name field
    ;
;

class User with Formatter :
    name = ""
;

let u = User({ name: "Alice" })
print(u->format())   # User: Alice
```

### 5. Mixins and `parent`

If a mixin inherits from another mixin, `parent` works inside mixin methods.

```xell
mixin A :
    fn foo(self): print("A") ;
;

mixin B inherits A :
    fn foo(self):
        parent->foo(self)   # calls A::foo
        print("B")
    ;
;

class C with B ;
let c = C()
c->foo()   # A, B
```

---

## What NOT to Do

### 1. Instantiating a Mixin

```xell
mixin Logger : fn log(self): print("log") ;
let l = Logger()   # ❌ TypeError – cannot instantiate mixin
```

**Why it fails:** The interpreter checks `def.isMixin` in `evalCall` and throws an error. Mixins are not constructible.

### 2. Defining `__init__` in a Mixin

```xell
mixin Bad :
    fn __init__(self):   # ❌ error – mixins cannot have __init__
        print("init")
    ;
;
```

**Why it fails:** The parser explicitly checks for `__init__` in mixin bodies and rejects it.

### 3. Expecting Mixins to Work Like Inheritance

```xell
mixin Logger :
    fn log(self): print("log") ;
;

class User with Logger :
    # No own log() method
;

let u = User()
print(u is Logger)   # true
print(u->log)        # method exists
```

This works correctly, but you cannot use `Logger` as a type in `belong` in `incase` for subtype‑aware matching.

### 4. Using `with` Without a Class

```xell
with Loggable   # ❌ syntax error – `with` must follow a class definition
```

**Why it fails:** `with` is only valid in the context of a class definition (`class Name inherits Parents with Mixins`).

### 5. Creating a Mixin That Depends on a Field the Class Doesn't Have

```xell
mixin Bad :
    fn process(self):
        give self->missing_field   # ❌ class may not have this field
;
;

class User with Bad :
    name = ""
;

let u = User()
u->process()   # ❌ runtime error – missing_field not found
```

**Why it fails:** Mixins cannot enforce that a class has a particular field. Document the required fields for your mixin.

### 6. Relying on Mixin Order for Complex Overrides

```xell
mixin A :
    fn compute(self): give 10 ;
;

mixin B :
    fn compute(self): give 20 ;
;

class C with A, B :
    # No own compute()
;

print(C()->compute())   # 10 (first mixin wins)
```

If you change the order to `with B, A`, the result changes to `20`. This can be surprising. For complex overrides, define the method explicitly in the class.

### 7. Using `parent` in a Mixin That Doesn't Have a Parent

```xell
mixin Logger :
    fn log(self):
        parent->log(self)   # ❌ error – Logger has no parent
    ;
;
```

**Why it fails:** `parent` is only valid inside methods of a class (or mixin) that actually has a parent. Ensure your mixin inherits from a parent mixin before using `parent`.

### 8. Expecting Mixin Fields to Be Isolated

```xell
mixin Counter :
    count = 0
;

class A with Counter ;
class B with Counter ;

let a = A()
a->count = 10
print(B()->count)   # 0 (isolated – each class has its own copy)
```

**This actually works correctly.** The pitfall would be expecting them to share state. They do not – each class gets its own independent copy of the mixin's fields.

### 9. Using Mixins for State That Needs Construction Logic

```xell
mixin Cache :
    cache = {}   # empty – but what if you need to load initial state?
;
```

**Why it's a problem:** Mixins cannot have `__init__`, so you cannot run initialization logic. If your mixin needs complex initialization, provide an `init()` method and call it from the class's `__init__`.

---

## See Also

- [Classes](oop-classes.md) – base class syntax and features
- [Inheritance](oop-inheritance.md) – extending classes with `inherits`
- [Interfaces](oop-interfaces.md) – contracts for classes
- [Abstract Classes](oop-abstract.md) – abstract methods and classes
- [Access Control](oop-access-control.md) – `protected` and mixins
- [Magic Methods](oop-magic-methods.md) – operator overloading and protocols

---

**Previous:** [Inheritance](oop-inheritance.md)  
**Next:** [Interfaces](oop-interfaces.md)