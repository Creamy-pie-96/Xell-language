# Functions and Callables

This section explains function definitions, arguments, lambdas, closures, generators, and async functions.

## Basic function

```xell
fn add(a, b):
    give a + b
;

print(add(2, 3))
```

## Default parameters

```xell
fn greet(name, prefix = "Hello"):
    give "{prefix}, {name}!"
;

print(greet("Ari"))
print(greet("Ari", "Hi"))
```

## Variadic parameters (`...rest`)

```xell
fn sum_all(a, ...rest):
    total = a
    for x in rest:
        total = total + x
    ;
    give total
;

print(sum_all(1, 2, 3, 4))
```

## Keyword-style calls

```xell
fn point(x, y, z):
    print("{x}, {y}, {z}")
;

point(z: 3, x: 1, y: 2)
```

## Lambda expressions

```xell
square = x => x * x
add2 = (a, b) => a + b
print(square(5))
print(add2(2, 8))
```

## Closures

```xell
fn make_counter(start):
    count = start
    fn next_count():
        count = count + 1
        give count
    ;
    give next_count
;

c = make_counter(10)
print(c())
print(c())
```

## Generators with `yield`

```xell
fn countdown(n):
    i = n
    while i >= 0:
        yield i
        i = i - 1
    ;
;

g = countdown(3)
print(next(g))
print(next(g))
```

## Async and await

```xell
async fn fetch_json(url):
    body = await http_get(url)
    give body
;
```

## Function decorators

```xell
fn logger(fn_ref):
    fn wrapped(x):
        print("calling with {x}")
        give fn_ref(x)
    ;
    give wrapped
;

@logger
fn double(x):
    give x * 2
;

print(double(5))
```

## Pipe operator with callables

```xell
result = [1, 2, 3, 4]
    |> filter(x => x % 2 == 0)
    |> map(x => x * 10)
print(result)
```
