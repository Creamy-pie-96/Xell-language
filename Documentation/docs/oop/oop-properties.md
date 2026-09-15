# Properties

> **Previous:** [Access Control](oop-access-control.md)  
> **Next:** [Magic Methods](oop-magic-methods.md)

---

Properties allow you to define **computed attributes** that look like fields but are actually methods with custom logic. They let you encapsulate internal state, validate assignments, compute values on the fly, and create read‑only or write‑only attributes – all while keeping a clean, field‑like syntax.

Think of properties as **smart fields** – you access them with `obj->property`, but behind the scenes, a getter and/or setter method runs to compute or validate the value.

---

## Quick Reference

| Keyword/Syntax | What it does | Minimal example |
|----------------|--------------|-----------------|
| `get name(self) : ... ;` | Defines a property getter | `get area(self): give self->width * self->height ;` |
| `set name(self, val) : ... ;` | Defines a property setter | `set area(self, val): throw Error("read-only") ;` |
| `obj->property` | Accesses the property (calls getter or setter) | `r->area` |
| `obj->property = value` | Assigns to the property (calls setter) | `r->area = 100` |
| Read‑only property | Has `get` but no `set` | `get pi(self): give 3.14159 ;` |
| Write‑only property | Has `set` but no `get` (rare) | `set password(self, val): self->hash = hashme(val) ;` |
| Property access control | Properties follow `public`/`protected`/`private` rules | `private: get secret(self): ... ;` |

---

## Defining a Property

Properties are defined inside a class body using the `get` and/or `set` keywords.

```xell
class Rectangle :
    width = 0
    height = 0

    get area(self):
        give self->width * self->height
    ;

    get perimeter(self):
        give 2 * (self->width + self->height)
    ;
;
```

- `get` defines a getter – it's called when you read `obj->property`.
- The getter must return a value (using `give`).
- The getter takes only `self` – no additional parameters.
- If you define only a getter, the property is **read‑only**.

---

## Using Properties

Properties are accessed exactly like fields – with `->`.

```xell
let r = Rectangle({ width: 10, height: 5 })
print(r->area)        # 50 (computed)
print(r->perimeter)   # 30 (computed)

r->width = 20
print(r->area)        # 100 (re‑computed when accessed)
```

**Expected output:**
```
50
30
100
```

---

## Property Setters

Define a setter with `set` to allow assignment to the property.

```xell
class Rectangle :
    width = 0
    height = 0

    get area(self):
        give self->width * self->height
    ;

    set area(self, val):
        # Set area by adjusting width (maintaining aspect ratio)
        if self->height > 0 :
            self->width = val / self->height
        else :
            self->width = val
    ;
;

let r = Rectangle({ width: 10, height: 5 })
print(r->area)   # 50
r->area = 100
print(r->width)  # 20 (100 / 5)
```

- The setter takes `self` and the new value.
- It must have exactly one parameter after `self` (the value).
- It can perform validation, transformations, or side effects.
- If no setter is defined, assignment to the property is **not allowed** (throws `AttributeError`).

---

## Read‑Only Properties

If a property has only a `get` and no `set`, it's read‑only:

```xell
class Circle :
    radius = 0

    get diameter(self):
        give self->radius * 2
    ;

    get circumference(self):
        give 2 * PI * self->radius
    ;

    get area(self):
        give PI * self->radius ** 2
    ;
;

let c = Circle({ radius: 5 })
print(c->diameter)      # 10
print(c->circumference) # 31.4159...
print(c->area)          # 78.5398...

# c->diameter = 20      # ❌ error – read‑only property
```

**Expected output:**
```
10
31.41592653589793
78.53981633974483
```

---

## Write‑Only Properties

Properties can be write‑only (have a setter but no getter) – this is uncommon but useful for sensitive data.

```xell
class SecureStorage :
    private:
        hash = ""

    public:
        set password(self, val):
            self->hash = hashme(val)
            print("Password updated")
        ;

        fn verify(self, val):
            give hashme(val) == self->hash
    ;
;

let s = SecureStorage()
s->password = "secret"   # calls setter, prints "Password updated"

# print(s->password)     # ❌ error – write‑only property (no getter)
print(s->verify("secret"))   # true
```

---

## Properties and Access Control

Properties follow the same access rules as fields and methods – they can be `public`, `protected`, or `private`.

```xell
class Account :
    private:
        balance = 0.0

    protected:
        get _balance(self):   # protected getter
            give self->balance
        ;

    public:
        get balance(self):    # public getter
            give self->balance
        ;

        set balance(self, val):
            if val < 0 :
                throw ValueError("Balance cannot be negative")
            self->balance = val
        ;
;

class SavingsAccount inherits Account :
    fn check(self):
        give self->_balance   # ✅ protected – accessible in subclass
    ;
;

let a = Account()
a->balance = 100
print(a->balance)   # 100

# a->_balance       # ❌ protected – not accessible from outside
```

---

## Properties with Validation

Setters are ideal for input validation and maintaining invariants.

```xell
class Person :
    name = ""
    age = 0

    set age(self, val):
        if val < 0 :
            throw ValueError("Age cannot be negative")
        if val > 150 :
            throw ValueError("Age seems unrealistic")
        self->age = val
    ;

    get age(self):
        give self->age
    ;
;

let p = Person()
p->age = 30         # ✅ valid
# p->age = -5       # ❌ throws ValueError
```

---

## Properties with Computed Dependencies

Properties can depend on multiple fields and automatically stay in sync.

```xell
class Temperature :
    celsius = 0.0

    get fahrenheit(self):
        give self->celsius * 9/5 + 32
    ;

    set fahrenheit(self, val):
        self->celsius = (val - 32) * 5/9
    ;

    get kelvin(self):
        give self->celsius + 273.15
    ;

    set kelvin(self, val):
        if val < 0 :
            throw ValueError("Kelvin cannot be negative")
        self->celsius = val - 273.15
    ;
;

let t = Temperature()
t->celsius = 25
print(t->fahrenheit)   # 77.0
print(t->kelvin)       # 298.15

t->fahrenheit = 32
print(t->celsius)      # 0.0
```

**Expected output:**
```
77.0
298.15
0.0
```

---

## Properties and Inheritance

Properties are inherited by subclasses and can be overridden.

```xell
class Shape :
    get area(self): give 0 ;
;

class Rectangle inherits Shape :
    width = 0
    height = 0

    get area(self):
        give self->width * self->height
    ;
;

let r = Rectangle({ width: 10, height: 5 })
print(r->area)   # 50 (overridden)
```

### Overriding Getter/Setter

You can override either the getter, the setter, or both.

```xell
class Parent :
    private:
        _value = 0

    get value(self):
        give self->_value
    ;

    set value(self, val):
        self->_value = val
    ;
;

class Child inherits Parent :
    get value(self):
        # override getter – add logging
        print("Getting value")
        give parent->value(self)
    ;

    set value(self, val):
        # override setter – add validation
        if val < 0 :
            throw ValueError("value must be non‑negative")
        parent->value(self, val)
    ;
;

let c = Child()
c->value = 10        # setter runs: validation passes
print(c->value)      # getter runs: prints "Getting value", then 10
c->value = -5        # ❌ throws ValueError
```

---

## Properties vs. Fields

| Aspect | Field | Property |
|--------|-------|----------|
| Storage | Stores data directly | May or may not store data (computed) |
| Syntax | `self->field` | `self->property` (same syntax) |
| Assignment | Direct storage | Calls setter (if defined) |
| Access | Direct read | Calls getter (if defined) |
| Validation | ❌ No automatic validation | ✅ Can validate in setter |
| Computation | ❌ No computation | ✅ Can compute in getter |
| Overhead | Minimal | Slightly more (method call) |
| Use when | Simple data storage | Logic, validation, computed values |

**Use a field when:**
- You just need to store a value.
- There's no validation or computation needed.
- Performance is critical.

**Use a property when:**
- You need to validate assignments.
- The value is computed from other fields.
- You need to control read/write access.
- You want to hide implementation details.

---

## Property and Access Control Combined

```xell
class Config :
    private:
        _settings = {}

    public:
        get settings(self):
            # Return a read‑only view (not modifiable)
            give self->_settings.clone()
        ;

        set settings(self, val):
            if not val is map :
                throw TypeError("settings must be a map")
            self->_settings = val
        ;

        private:
            get _raw_settings(self):
                # Internal use only
                give self->_settings
            ;
;
```

---

## Complete Example: Shopping Cart with Properties

```xell
class CartItem :
    name = ""
    price = 0.0
    quantity = 1

    get total(self):
        give self->price * self->quantity
    ;

    set quantity(self, val):
        if val < 0 :
            throw ValueError("Quantity cannot be negative")
        if val > 100 :
            throw ValueError("Quantity cannot exceed 100")
        self->quantity = val
    ;
;

class ShoppingCart :
    private:
        items = []

    get total(self):
        let sum = 0.0
        for item in self->items:
            sum += item->total
        ;
        give sum
    ;

    get count(self):
        give len(self->items)
    ;

    get item_count(self):
        let count = 0
        for item in self->items:
            count += item->quantity
        ;
        give count
    ;

    fn add_item(self, name, price, quantity = 1):
        push(self->items, CartItem({ name: name, price: price, quantity: quantity }))
    ;

    fn remove_item(self, index):
        if index >= 0 and index < len(self->items) :
            remove_val(self->items, self->items[index])
    ;
;

let cart = ShoppingCart()
cart->add_item("Apple", 0.50, 3)
cart->add_item("Banana", 0.30, 2)
cart->add_item("Orange", 0.75, 1)

print("Items: " + str(cart->count))          # 3
print("Total units: " + str(cart->item_count))  # 6
print("Total price: $" + str(cart->total))   # $3.15 (3*0.50 + 2*0.30 + 1*0.75)
```

**Expected output:**
```
Items: 3
Total units: 6
Total price: $3.15
```

---

## Edge Cases and Less Obvious Behavior

### 1. Property Access in `__init__`

Properties work inside `__init__` – they use getters/setters normally.

```xell
class Person :
    age = 0
    set age(self, val):
        if val < 0 : throw ValueError("bad age")
        self->age = val
    ;
    fn __init__(self, age):
        self->age = age   # calls setter with validation
    ;
;

let p = Person(30)   # ✅ works
# Person(-5)         # ❌ throws
```

### 2. Recursive Property Access

Be careful not to create infinite recursion in getters/setters.

```xell
class Bad :
    private:
        _value = 0
    get value(self):
        give self->value   # ❌ infinite recursion – calls itself
    ;
;
```

**Fix:** Use the backing field (e.g., `_value`) inside the getter.

### 3. Properties Are Not Polymorphic in Static Context

Static methods cannot access instance properties (they need a `self` instance).

```xell
class Example :
    get value(self): give 42 ;
    static fn get_value(): give Example()->value ;   # ❌ can't call instance method from static
;
```

### 4. Properties and `clone()`

When you clone an object, the cloned instance has the same property definitions, but each instance gets its own backing fields.

```xell
class WithProp :
    private:
        _data = 10
    get data(self): give self->_data ;
    set data(self, val): self->_data = val ;
;

let a = WithProp()
a->data = 20
let b = a.clone()
b->data = 30
print(a->data)   # 20 (unchanged)
```

### 5. Properties and `frozen` Instances

Frozen instances (`~` prefix) do **not** allow setting properties that would modify state – they throw `ImmutabilityError`.

```xell
@immutable
class Config :
    url = ""
    get full_url(self):
        give self->url + "/api"
;

let c = ~Config({ url: "https://api.example.com" })
print(c->full_url)   # ✅ getter works (read‑only)
c->url = "new"       # ❌ ImmutabilityError – field modified
```

### 6. Setter Return Value

Setters should **not** return a value – they are called for side effects.

```xell
set area(self, val):
    give val * 2   # ❌ setter cannot return a value – it's ignored
;
```

### 7. Getter Side Effects

While possible, it's generally a bad idea to have side effects in getters (e.g., logging, modifying state). Keep getters pure and idempotent.

---

## What NOT to Do

### 1. Defining a Property Without a Getter or Setter

```xell
class Bad :
    get nothing(self)   # ❌ error – getter needs a body
    set nothing(self, val)   # ❌ error – setter needs a body
;
```

**Why it fails:** Both `get` and `set` must have a body (`:` … `;`).

### 2. Forgetting That Properties Can't Have Default Values

```xell
class Point :
    x = 0          # ✅ field
    get x(self): ...   # ❌ cannot have both field and property with same name
;
```

**Why it fails:** A property and a field cannot share the same name. Use a backing field with a different name (e.g., `_x`).

### 3. Using Properties for Simple Data Storage

```xell
class Person :
    private:
        _name = ""
    get name(self): give self->_name ;
    set name(self, val): self->_name = val ;
;
```

**Why it's bad:** This is just unnecessary overhead. Use a field unless you need validation or computation.

### 4. Expecting Properties to Be Serializable Automatically

```xell
class Example :
    get computed(self): give 42 ;
;
```

When serializing with `to_json()`, the computed property **won't** appear unless you add it explicitly.

### 5. Putting Heavy Computation in a Getter

```xell
get expensive(self):
    give heavy_computation()   # ❌ called every time you access the property
;
```

**Better:** Compute once and cache the result, or use a method (e.g., `compute_expensive()`).

### 6. Accessing `self` in a Static Property

```xell
static get count(self):   # ❌ error – static getter doesn't receive self
    give 0
;
```

**Why it fails:** Static members don't receive `self` – they can't be properties (they are just static methods or fields).

---

## See Also

- [Access Control](oop-access-control.md) – `public`/`protected`/`private` for properties
- [Classes](oop-classes.md) – where properties live
- [Magic Methods](oop-magic-methods.md) – operator overloading and protocols
- [Structs](oop-structs.md) – why structs don't have properties

---

**Previous:** [Access Control](oop-access-control.md)  
**Next:** [Magic Methods](oop-magic-methods.md)