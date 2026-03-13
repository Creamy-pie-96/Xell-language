# Loops and Comprehensions

This section covers iteration in statement mode and expression mode.

## for loop

```xell
for item in [10, 20, 30]:
    print(item)
;
```

## for with destructuring

```xell
pairs = [["a", 1], ["b", 2]]
for k, v in pairs:
    print("{k} => {v}")
;
```

## while loop

```xell
i = 0
while i < 3:
    print(i)
    i = i + 1
;
```

## loop (infinite loop)

```xell
i = 0
loop:
    if i == 3: break;
    print(i)
    i = i + 1
;
```

## do ... while

```xell
i = 0
do:
    print(i)
    i = i + 1
while i < 2;
```

## break and continue

```xell
for i in range(10):
    if i == 2: continue;
    if i == 6: break;
    print(i)
;
```

## Expression-mode loops

```xell
result = while true:
    break "done"
;
print(result)
```

## `@safe_loop` decorator

`@safe_loop` is supported on `loop` statements.

```xell
@safe_loop
loop:
    # guarded loop body
    break;
;
```

## List comprehension

```xell
squares = [x * x for x in range(6) if x % 2 == 0]
print(squares)
```

## Set and map comprehensions

```xell
letters = {c for c in "hello"}
lookup = {x: x * x for x in range(4)}
print(letters)
print(lookup)
```

## Nested comprehension

```xell
pairs = [x + y for x in [1, 2] for y in [10, 20] if x > 0]
print(pairs)
```
