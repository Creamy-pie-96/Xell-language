# Control Flow

This section shows how Xell makes decisions with `if`, `incase`, and conditional expressions.

## if / elif / else

```xell
score = 82

if score >= 90:
    print("A")
elif score >= 80:
    print("B")
else:
    print("C or below")
;
```

## if expression

```xell
age = 20
label = if age >= 18: "adult" else: "minor"
print(label)
```

## Ternary style expression

```xell
age = 16
msg = "adult" if age >= 18 else "minor"
print(msg)
```

## Pattern matching with `incase`

```xell
value = 3

incase value:
    is 1: print("one") ;
    is 2 or 3: print("two or three") ;
    else: print("other") ;
;
```

## Type pattern with `belong`

```xell
obj = [1, 2]

incase obj:
    belong int: print("int") ;
    belong list: print("list") ;
    else: print("unknown") ;
;
```

## Capture pattern with `bind`

```xell
score = 95

incase score:
    bind s if s >= 90: print("A: {s}") ;
    bind s if s >= 80: print("B: {s}") ;
    else: print("below B") ;
;
```

## Expression-form `incase`

```xell
x = 10
result = incase x:
    is 0: "zero"
    belong int if x > 0: "positive int"
    else: "other"
;
print(result)
```

## Boolean operators

```xell
x = 5
if x > 0 and x < 10:
    print("in range")
;

if not (x == 0):
    print("non-zero")
;
```

## Comparison helpers

```xell
x = 7
if x gt 5 and x le 10:
    print("between 6 and 10")
;
```
