# Object-Oriented Programming

This section explains classes, structs, inheritance, interfaces, mixins, properties, and magic methods.

## Class definition

```xell
class Person:
    name = ""

    fn __init__(self, name):
        self->name = name
    ;

    fn greet(self):
        give "Hi, I am {self->name}"
    ;
;

p = Person("Ari")
print(p->greet())
```

## Struct definition

```xell
struct Point:
    x = 0
    y = 0
;

pt = Point(x: 3, y: 4)
print(pt->x, pt->y)
```

## Inheritance

```xell
class Animal:
    fn speak(self):
        give "..."
    ;
;

class Dog inherits Animal:
    fn speak(self):
        give "woof"
    ;
;

d = Dog()
print(d->speak())
```

## Interfaces

```xell
interface Shape:
    fn area(self);
;

class Square implements Shape:
    side = 1
    fn area(self):
        give self->side * self->side
    ;
;
```

## Mixins

```xell
mixin JsonPrintable:
    fn to_json(self):
        give "{name: \"{self->name}\"}"
    ;
;

class User with JsonPrintable:
    name = "Ari"
;

u = User()
print(u->to_json())
```

## Access control and static members

```xell
class Config:
    static env = "dev"
    private secret = "token"

    fn reveal(self):
        give self->secret
    ;
;

print(Config::env)
```

## Properties

```xell
class Temperature:
    c = 0

    get f: self->c * 9/5 + 32;
    set f(v): self->c = (v - 32) * 5/9;
;

t = Temperature()
t->f = 212
print(t->c)
```

## Magic method example

```xell
class Vec2:
    x = 0
    y = 0

    fn __init__(self, x, y):
        self->x = x
        self->y = y
    ;

    fn __add__(self, other):
        give Vec2(self->x + other->x, self->y + other->y)
    ;
;

a = Vec2(1, 2)
b = Vec2(3, 4)
c = a + b
print(c->x, c->y)
```

## Context manager protocol (`let ... be`)

```xell
class Resource:
    fn __enter__(self):
        print("open")
        give self
    ;
    fn __exit__(self):
        print("close")
    ;
;

let r be Resource():
    print("inside")
;
```
