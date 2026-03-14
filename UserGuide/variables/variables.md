# Variables and Types

Xell supports dynamic values with rich literal syntax and strong built-in type helpers.

## Basic binding

```xell
name = "Ari"
age = 21
active = true
```

## Immutable binding

Use `immutable` when rebinding is not allowed.

```xell
immutable pi_label = "PI"
# pi_label = "new"   # error
```

## Primitive values

```xell
i = 42
f = 3.14
z = 2 + 3i      # complex-style value
s = "hello"
b = false
n = none
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

`hexv`, `octv`, and `binv` are all integer values.

## Strings

```xell
name = "Ari"
msg = "Hello, {name}!"
```

### Raw string

```xell
path = r"C:\Users\demo"
```

### Multiline string

```xell
text = """
line 1
line 2
"""
```

## Bytes

```xell
raw = b"abc"
```

## Collections

```xell
items = [1, 2, 3]              # list
point = (10, 20)               # tuple
tags = {"a", "b"}             # set
frozen = ~{1, 2, 3}            # frozen_set (immutable set)
user = {name: "Ari", id: 1}   # map
```

Note: frozen-set literal syntax is `~{...}`.

## Destructuring

```xell
[a, b, ...rest] = [10, 20, 30, 40]
{k: id, n: label} = {k: 7, n: "node"}
```

## Type inspection

```xell
print(type(items))    # list
print(type(point))    # tuple
print(type(tags))     # set
print(type(frozen))   # frozen_set
print(type(user))     # map
print(type(raw))      # bytes
```

## Conversions and helpers

```xell
x = "123"
xi = int(x)
xf = float(x)
xs = str(xi)
n = num(x)

print(len(items))
print(typeof(user))
```

## Smart-cast prefix `~`

```xell
value = "42"
as_int = ~int(value)
```

`~` is also used for frozen-set literals (`~{...}`).
