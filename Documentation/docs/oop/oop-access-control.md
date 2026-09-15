# Access Control

> **Previous:** [Interfaces](oop-interfaces.md)  
> **Next:** [Properties](oop-properties.md)

---

Access control determines which parts of your code can access which members of a class. Xell provides three access levels – `public`, `protected`, and `private` – along with `static` members that belong to the class itself rather than instances. These features help you encapsulate implementation details, enforce invariants, and design clean APIs.

Think of access control as **who is allowed to touch what** – it lets you hide internal state and expose only what's necessary.

---

## Quick Reference

| Keyword/Syntax | What it does | Minimal example |
|----------------|--------------|-----------------|
| `public:` | Marks the following members as publicly accessible (default) | `public: let name = "" ;` |
| `protected:` | Marks the following members as accessible only within the class and subclasses | `protected: let secret = 42 ;` |
| `private:` | Marks the following members as accessible only within the class itself | `private: let password = "" ;` |
| `static` | Declares a member that belongs to the class, not instances | `static count = 0` |
| `static fn method(...)` | Declares a static method (no `self` parameter) | `static fn create(): give Class() ;` |
| `ClassName->field` | Accesses a static field | `Counter->count` |
| `ClassName->method()` | Calls a static method | `Factory->create()` |

---

## Access Levels Overview

| Access Level | Visibility | Can be accessed from |
|--------------|------------|---------------------|
| `public` | Everywhere | Any code, anywhere |
| `protected` | Class and subclasses | Inside the class itself and any subclass methods |
| `private` | Class only | Only inside the class itself (not even subclasses) |
| `static` | Class-level | Through the class name (`ClassName->member`) |

**Default:** If no access label is present, members are `public`.

---

## `public` – Accessible Everywhere

`public` members can be accessed from anywhere – inside the class, from subclasses, and from external code.

```xell
class Person :
    # Everything before the first label is public by default
    name = ""

    public:
        age = 0

        fn greet(self):
            print("Hello, I'm " + self->name)
        ;
;

let p = Person({ name: "Alice", age: 30 })
p->name = "Bob"        # ✅ public – accessible
p->age = 31            # ✅ public – accessible
p->greet()             # ✅ public – accessible
```

**Default behavior:** All members are `public` until an access label changes it.

---

## `protected` – Accessible Within Class and Subclasses

`protected` members are accessible from inside the class itself and from any subclass, but **not** from external code.

```xell
class Animal :
    protected:
        internal_id = 0

    public:
        fn set_id(self, id):
            self->internal_id = id   # ✅ accessible inside class
        ;
;

class Dog inherits Animal :
    fn get_id(self):
        give self->internal_id       # ✅ accessible inside subclass
    ;
;

let a = Animal()
# a->internal_id   # ❌ protected – not accessible from outside

let d = Dog()
d->set_id(42)
print(d->get_id())   # 42
```

**When to use `protected`:**
- For internal state that subclasses need to access.
- For helper methods that subclasses might want to override.
- For invariants that should be preserved across the class hierarchy.

---

## `private` – Accessible Only Within the Class

`private` members are accessible **only** from inside the class itself. Not even subclasses can access them.

```xell
class BankAccount :
    private:
        balance = 0.0
        password = ""

    public:
        fn __init__(self, initial_balance, password):
            self->balance = initial_balance
            self->password = password
        ;

        fn deposit(self, amount):
            if amount > 0 :
                self->balance += amount   # ✅ accessible inside class
        ;

        fn withdraw(self, amount, password):
            if password != self->password :
                throw AccessError("Wrong password")
            if amount > self->balance :
                throw RuntimeError("Insufficient funds")
            self->balance -= amount
        ;

        fn get_balance(self):
            give self->balance            # ✅ accessible inside class
        ;
;

class Fraudster inherits BankAccount :
    fn hack(self):
        # self->balance   # ❌ private – not accessible even in subclass
        # self->password  # ❌ private – not accessible
        print("Can't hack!")
    ;
;

let acc = BankAccount(100.0, "secret")
acc->deposit(50.0)
print(acc->get_balance())   # 150.0

# acc->balance   # ❌ private – not accessible from outside
# acc->password  # ❌ private – not accessible from outside
```

**When to use `private`:**
- For implementation details that should never change.
- For sensitive data (passwords, encryption keys).
- For internal state that subclasses should not depend on.

---

## Access Labels Affect Everything After Them

Access labels apply to **all members that follow** until the next label.

```xell
class Example :
    # No label – default is public
    a = 1

    private:
        b = 2          # private
        c = 3          # private

    protected:
        d = 4          # protected
        e = 5          # protected

    public:
        f = 6          # public (back to public)
        g = 7          # public
;
```

**Best practice:** Put `public` members first (the API), followed by `protected` (for subclasses), then `private` (implementation details).

---

## Static Members

`static` members belong to the **class itself**, not to individual instances. They are shared across all instances.

### Static Fields

```xell
class Counter :
    static count = 0

    fn __init__(self):
        Counter->count += 1   # increment class counter
    ;
;

let a = Counter()
let b = Counter()
let c = Counter()
print(Counter->count)   # 3
```

- Static fields are accessed via `ClassName->field`.
- They exist even if no instances have been created.
- They are useful for shared state, configuration, or tracking.

### Static Methods

Static methods are called on the class itself and do **not** receive a `self` parameter.

```xell
class MathUtils :
    static fn square(x):
        give x * x
    ;

    static fn cube(x):
        give x * x * x
    ;
;

print(MathUtils->square(5))   # 25
print(MathUtils->cube(3))     # 27
```

- Static methods cannot access instance fields (since there's no `self`).
- They can access other static members.
- They are useful for utility functions that don't need instance state.

### Access Control and Static Members

Static members follow the same access rules as instance members:

```xell
class Secure :
    private:
        static secret = "hidden"

    public:
        static fn get_secret():
            give Secure->secret   # ✅ accessible inside class
        ;
;

print(Secure->get_secret())   # "hidden"
# Secure->secret   # ❌ private – not accessible outside
```

---

## Access Control and Inheritance

### `protected` in Subclasses

A subclass can access `protected` members defined in its parent:

```xell
class Parent :
    protected:
        data = 42
;

class Child inherits Parent :
    fn peek(self):
        give self->data   # ✅ accessible – same hierarchy
;

let c = Child()
print(c->peek())   # 42
```

### `private` is NOT Inherited

`private` members are **not** accessible in subclasses:

```xell
class Parent :
    private:
        secret = "hidden"
;

class Child inherits Parent :
    fn peek(self):
        # give self->secret   # ❌ private – not accessible
        give "can't see secret"
    ;
;
```

### `protected` is Inherited

`protected` members **are** accessible in subclasses:

```xell
class Parent :
    protected:
        value = 10
;

class Child inherits Parent :
    fn get_value(self):
        give self->value   # ✅ protected – accessible in subclass
    ;
;

let c = Child()
print(c->get_value())   # 10
```

---

## Access Control Enforcement

Xell enforces access control at **runtime**. If you try to access a `private` or `protected` member from outside its allowed scope, an `AccessError` is thrown.

```xell
class Secret :
    private:
        password = "hunter2"
    public:
        fn check(self, pw):
            give pw == self->password   # OK – inside class
    ;
;

let s = Secret()
# s->password   # ❌ AccessError: 'password' is private
print(s->check("hunter2"))   # true
```

### How Access Control Works Internally

The interpreter tracks the **currently executing class context** (`executingMethodClass_`). When you access a member:

1. The interpreter looks up the member's access level.
2. If it's `public`, access is always allowed.
3. If it's `protected`, access is allowed if the current context is the same class or a subclass (via `isOrInherits`).
4. If it's `private`, access is allowed **only** if the current context is the **exact same class** (name match).

```xell
# Inside a method of class Parent:
# - Can access Parent's protected members ✅
# - Can access Parent's private members ✅
# - Can access Child's protected members? ❌ (not a subclass relationship from Parent's perspective)
```

---

## `static` Access in Subclasses

Static members are inherited by subclasses (they exist in the subclass's own scope, but they are distinct copies unless explicitly shared via the parent class name).

```xell
class Parent :
    static value = 10
;

class Child inherits Parent :
    # Child->value initially references Parent->value
;

print(Child->value)   # 10
Child->value = 20
print(Parent->value)  # 10 (still unchanged)
```

**Important:** Static members are **not shared** between parent and child unless you access them through the parent class name. They exist independently in each class's scope.

---

## Access Control Best Practices

### 1. Start with `public` API

```xell
class API :
    public:
        fn do_work(self):   # users call this
            self->step1()
            self->step2()
        ;

    private:
        fn step1(self): ... ;   # internal
        fn step2(self): ... ;   # internal
    ;
;
```

### 2. Use `protected` for Extensibility

```xell
class Framework :
    protected:
        fn hook(self):   # subclasses can override
            print("default hook")
        ;

    public:
        fn run(self):
            self->hook()   # call the hook
        ;
;
```

### 3. Keep `private` Members Small

```xell
class DataStore :
    private:
        # Minimal private state
        connection = none
        cache = {}

    public:
        fn query(self, sql):
            # use private state
        ;
;
```

### 4. Use `static` for Utility Functions

```xell
class MathUtils :
    static fn square(x): give x * x ;
    static fn cube(x): give x * x * x ;
    static fn sqrt(x): give pow(x, 0.5) ;
;
```

---

## Complete Example: Class with Full Access Control

```xell
class UserManager :
    private:
        users = {}           # stored as map of id → user data
        next_id = 1

    protected:
        fn generate_id(self):
            let id = self->next_id
            self->next_id += 1
            give id
        ;

        fn validate_user(self, user_data):
            if user_data["name"] == "" :
                throw ValueError("Name cannot be empty")
            give true
        ;

    public:
        static count = 0

        fn add_user(self, name, email):
            self->validate_user({ "name": name, "email": email })
            let id = self->generate_id()
            self->users[id] = { "id": id, "name": name, "email": email }
            UserManager->count += 1
            give id
        ;

        fn get_user(self, id):
            if id in self->users :
                give self->users[id]
            else :
                throw KeyError("User not found")
        ;

        fn delete_user(self, id):
            if id in self->users :
                self->users.remove(id)
                UserManager->count -= 1
            ;
        ;

        static fn get_user_count(self):
            give UserManager->count   # Static method can access static field
        ;
;

let mgr = UserManager()
let id1 = mgr->add_user("Alice", "alice@example.com")
let id2 = mgr->add_user("Bob", "bob@example.com")

print("Users created: " + str(UserManager->get_user_count()))   # 2

let user = mgr->get_user(id1)
print(user["name"])   # Alice

mgr->delete_user(id1)
print("Users remaining: " + str(UserManager->get_user_count()))   # 1

# mgr->users        # ❌ private – not accessible
# mgr->generate_id() # ❌ protected – not accessible from outside
```

**Expected output:**
```
Users created: 2
Alice
Users remaining: 1
```

---

## Edge Cases and Less Obvious Behavior

### 1. `static` Fields Are Not Shared Between Parent and Child

```xell
class Parent :
    static value = 10
;

class Child inherits Parent :
    # Child has its own static value initially 10
;

print(Parent->value)   # 10
print(Child->value)    # 10

Child->value = 99
print(Parent->value)   # 10 (still unchanged)
```

### 2. `protected` Access Inside Parent Methods

A parent method can access `protected` members of a child instance? **No** – access is based on the **currently executing method's class**, not the runtime type of the object.

```xell
class Parent :
    protected:
        value = 10
    public:
        fn set_child_value(self, child):
            child->value = 20   # ❌ error – Parent can't access Child's protected
        ;
;

class Child inherits Parent :
    protected:
        value = 20   # overrides
;

let c = Child()
# Parent::set_child_value() would fail because Parent cannot access Child->value
```

### 3. `private` Members Are Not Overridden

```xell
class Parent :
    private:
        value = 10
;

class Child inherits Parent :
    private:
        value = 20   # This is a completely different private member
;

let c = Child()
# Parent::value and Child::value are distinct – both inaccessible outside
```

### 4. Access Control and Mixins

Access control labels in mixins are respected when the mixin is composed into a class:

```xell
mixin Logger :
    private:
        log_level = "info"
    public:
        fn log(self, msg):
            print("[" + self->log_level + "] " + msg)   # ✅ accessible inside
        ;
;

class App with Logger :
    fn run(self):
        # self->log_level   # ❌ private – not accessible from App
        self->log("App started")   # ✅ public
    ;
;

let a = App()
a->log("Running")   # ✅ public
```

### 5. Access Control and Interfaces

Interfaces define public contracts – all methods in an interface are implicitly `public`.

```xell
interface Drawable :
    fn draw(self) ;   # implicitly public
;

class Circle implements Drawable :
    private:
        radius = 0
    public:
        fn draw(self):   # Must be public to satisfy interface
            print("drawing circle")
        ;
;
```

### 6. The `executingMethodClass_` Context

Xell tracks which class is executing the current method. This context determines access permissions:

- Inside a method defined in `Parent`, the context is `Parent`.
- Inside a method defined in `Child`, the context is `Child`.
- This is why `Parent`'s method cannot access `Child`'s `protected` members – even though the `self` object may be a `Child` instance, the **context** is `Parent`.

---

## What NOT to Do

### 1. Expecting `protected` to Be Accessible from Outside the Hierarchy

```xell
class Parent :
    protected:
        value = 42
;

let p = Parent()
print(p->value)   # ❌ AccessError – protected not accessible
```

### 2. Expecting `private` to Be Accessible from Subclasses

```xell
class Parent :
    private:
        secret = 42
;

class Child inherits Parent :
    fn peek(self):
        give self->secret   # ❌ AccessError – private not accessible
    ;
;
```

### 3. Using `static` Without the Class Name

```xell
class Counter :
    static count = 0
;

print(count)      # ❌ error – count is not a global variable
print(Counter->count)   # ✅ correct
```

### 4. Expecting `static` Members to Be Shared Across Parent and Child

```xell
class Parent :
    static value = 10
;

class Child inherits Parent :
    # Child->value is initially 10 but is a separate copy
;

Child->value = 99
print(Parent->value)   # 10 (still unchanged)
```

### 5. Forgetting That `static` Methods Can't Access Instance Members

```xell
class Bad :
    name = ""

    static fn greet():
        print(self->name)   # ❌ error – no `self` in static context
    ;
;
```

**Why it fails:** `static` methods don't receive a `self` parameter, so they can't access instance fields or methods.

### 6. Putting Sensitive Data in `public` Members

```xell
class Wallet :
    public:
        balance = 0.0
        password = "1234"   # ❌ should be private
;
```

**Why it's bad:** Anyone can read or modify `balance` and `password` from outside. Use `private` for sensitive data.

### 7. Using Access Labels Inside Structs

```xell
struct Point :
    public:
        x = 0   # ❌ error – access labels are not allowed in structs
    private:
        y = 0   # ❌ error – access labels not allowed in structs
;
```

**Why it fails:** Structs are value types with all fields public. Access control is a class feature only.

### 8. Expecting `protected` to Work in `static` Contexts

```xell
class Parent :
    protected:
        static value = 10   # ❌ protected static is allowed but unusual
;

class Child inherits Parent :
    static fn get_value():
        give Child->value   # ❌ error – `value` is protected, but static context has no `self`
    ;
;
```

**Why it fails:** `protected` access is tied to instance contexts (`self`). In a static context, there's no class hierarchy relationship to check, so access is denied.

### 9. Using `private` on a Field That Needs to Be Serializable

```xell
class Data :
    private:
        value = 42
;

# If you serialize this instance, the private field won't be included
```

**Why it's a problem:** Private fields are intentionally hidden – they won't appear in `to_json()` or `str()` representations unless you add explicit methods.

---

## See Also

- [Classes](oop-classes.md) – class syntax and features
- [Inheritance](oop-inheritance.md) – how `protected` works with subclasses
- [Mixins](oop-mixins.md) – access control in mixins
- [Properties](oop-properties.md) – `public`/`private` with getters/setters
- [Structs](oop-structs.md) – why structs don't have access control

---

**Previous:** [Interfaces](oop-interfaces.md)  
**Next:** [Properties](oop-properties.md)