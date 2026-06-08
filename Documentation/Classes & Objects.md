## ⬅️ What's Previous?

Move back to the **[Strings](./Strings.md)** to review the foundational concepts.

---

# Classes & Objects

Xell provides a powerful object-oriented programming (OOP) system, allowing you to create complex data structures and encapsulate behavior.

## 🏗️ Basic Structures

### Structs
Structs are simple containers for data and methods. They are ideal for lightweight objects.

```xell
struct Point :
    x = 0
    y = 0

    fn move(dx, dy) :
        self->x = self->x + dx
        self->y = self->y + dy
    ;
;

p = Point()
p->x = 10
p->move(5, 5)
print p->x # Output: 15
```

### Classes
Classes are the foundation of OOP in Xell, supporting inheritance, interfaces, and access control.

```xell
class Animal :
    name = "Unknown"

    fn make_sound() :
        print "Some generic sound"
    ;
;
```

---

## 🧬 Advanced Class Features

### Inheritance, Mixins, and Interfaces
Xell supports a flexible inheritance model:
- **`inherits`**: Standard class inheritance.
- **`with`**: Adds functionality from a **Mixin**.
- **`implements`**: Ensures the class adheres to an **Interface**.

```xell
interface ICanFly :
    fn fly() : ;
;

mixin Logger :
    fn log(msg) :
        print "[LOG]: {msg}"
    ;
;

class Bird inherits Animal with Logger implements ICanFly :
    fn fly() :
        self->log("Taking off!")
        print "Flap flap!"
    ;
;
```

### Abstract Classes
Abstract classes cannot be instantiated directly and are used as templates for other classes.
```xell
abstract class Shape :
    abstract fn area() : ;
;
```

---

## 🛡️ Access Control

Xell uses access modifiers to protect the internal state of an object.

| Modifier | Visibility |
| :--- | :--- |
| `public:` | Accessible from anywhere. (Default) |
| `protected:` | Accessible within the class and its subclasses. |
| `private:` | Accessible only within the class itself. |

**Example:**
```xell
class Account :
    public:
        owner = "Guest"

    private:
        balance = 0

        fn update_balance(amt) :
            self->balance = self->balance + amt
        ;

    public:
        fn deposit(amt) :
            self->update_balance(amt)
            print "Deposited {amt}"
        ;
;
```

---

## ⚡ Static Members & Properties

### Static Members
Static members belong to the class itself rather than a specific instance.

```xell
class MathUtils :
    static PI = 3.14159
    static fn square(x) :
        give x * x
    ;
;

print MathUtils->PI        # 3.14159
print MathUtils->square(4) # 16
```

### Properties (Getters & Setters)
Properties allow you to define custom logic for reading or writing a field.

```xell
class User :
    private:
        _name = "Unknown"

    public:
        get name() :
            give self->_name
        ;

        set name(val) :
            if len(val) > 0 :
                self->_name = val
            ;
;

u = User()
u->name = "Alice" # Calls 'set name'
print u->name     # Calls 'get name'
```

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Modules & Imports](./Modules & Imports.md)** guide to further your understanding of Xell.
