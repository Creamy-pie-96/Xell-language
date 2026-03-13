# Variables and Types

This section explains how to create values, store them, and convert between types in Xell.

## Basic assignment

```xell
name = "Ari"
age = 21
active = true
```

## Immutable binding

Use `immutable` when a name must not be rebound.

```xell
immutable pi_label = "PI"
# pi_label = "new"   # error: immutable binding
```

## Core value types

```xell
i = 42
f = 3.14
c = 2 + 3i
s = "hello"
b = false
n = none
```

## Collections

```xell
items = [1, 2, 3]           # list
point = (10, 20)            # tuple
tags = {"a", "b"}          # set
frozen = ~{1, 2, 3}          # frozen_set
user = {name: "Ari", id: 1} # map
raw = b"abc"                # bytes
```

## Numeric literals

```xell
dec = 255
hexv = 0xFF
octv = 0o755
binv = 0b1010
sci = 1.25e3
imag = 7i
```

## Strings and interpolation

```xell
name = "Ari"
msg = "Hello, {name}!"
print(msg)
```

## Raw and multiline strings

```xell
path = r"C:\Users\demo"
text = """
line 1
line 2
"""
```

## Destructuring

```xell
[a, b, ...rest] = [10, 20, 30, 40]
{k: id, n: label} = {k: 7, n: "node"}
print(a, b, rest, id, label)
```

## Type conversion

```xell
x = "123"
xi = int(x)
xf = float(x)
xs = string(xi)
ok = bool(xi)
```

## Smart cast with `~`

```xell
value = "42"
as_int = ~int(value)
print(as_int + 1)
```

## Useful type helpers

```xell
v = [1, 2, 3]
print(type(v))
print(len(v))
```
