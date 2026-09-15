# Generators

> **Previous:** [RAII](raii.md)  
> **Next:** [Async](async.md)

---

Generators are **lazy sequences** – functions that can produce multiple values over time, suspending execution between each yield. They are memory‑efficient, can represent infinite streams, and enable elegant asynchronous patterns.

---

## Defining a Generator

Any function that contains a `yield` expression becomes a **generator**. It returns a generator object, not a regular value.

```xell
fn count_up_to(limit):
    let i = 0
    while i < limit:
        yield i
        i = i + 1
    ;
;
```

- The function body can contain any statements.
- `yield` suspends execution and produces a value.
- The function's local state (variables, execution position) is preserved between yields.
- When the function ends (or hits a `give`), the generator is exhausted.

---

## Consuming Generators

### 1. `for` Loop

The most common way to consume a generator:

```xell
let gen = count_up_to(3)
for val in gen:
    print(val)   # 0, 1, 2
;
```

`for` automatically drives the generator until it's exhausted.

### 2. Manual Iteration

You can manually advance a generator using `next()`:

```xell
let gen = count_up_to(3)
let val1 = next(gen)   # 0
let val2 = next(gen)   # 1
let val3 = next(gen)   # 2
let val4 = next(gen)   # none (exhausted)
```

- `next()` returns the next yielded value.
- When the generator is exhausted, `next()` returns `none`.
- If the generator throws an error, `next()` propagates it.

### 3. Collecting All Values

Use a list comprehension or `map` to materialise all values:

```xell
let values = [for val in count_up_to(5): give val ;]   # [0,1,2,3,4]
let evens = map(count_up_to(10), x => x * 2)
```

> **Caution:** Materialising a generator defeats its lazy benefits – use only for small sequences.

---

## `yield` Expression

`yield` can appear anywhere an expression is allowed. Its value from the generator's perspective is **always `none`** – the yielded value is sent to the caller.

```xell
fn generate():
    yield 10
    yield 20
    yield 30
;

let g = generate()
print(next(g))   # 10
print(next(g))   # 20
```

**`yield` without a value:**

```xell
fn emit_signal():
    yield   # yields none
    yield   # yields none again
;
```

---

## Generator Lifecycle

### States

1. **Created** – The generator object exists but hasn't started executing.
2. **Running** – The generator is executing (inside a `for` loop or `next()` call).
3. **Yielded** – The generator has paused at a `yield` and is waiting to be resumed.
4. **Done** – The generator finished (reached the end or hit a `give`).

### Resuming a Generator

Each call to `next()` resumes execution from the last `yield`:

```xell
fn steps():
    print("step 1")
    yield 1
    print("step 2")
    yield 2
    print("step 3")
;

let s = steps()
print(next(s))   # prints "step 1", then prints "1"
print(next(s))   # prints "step 2", then prints "2"
print(next(s))   # prints "step 3", then returns none
```

### Exhaustion

Once a generator is exhausted, calling `next()` again returns `none` (not an error):

```xell
let g = count_up_to(1)
next(g)   # 0
next(g)   # none
next(g)   # none
```

---

## Generators with `give`

A generator can use `give` to return a final value, but this **also exhausts** the generator:

```xell
fn generate():
    yield 1
    yield 2
    give 3      # final value
;

let g = generate()
next(g)   # 1
next(g)   # 2
next(g)   # 3 (final value, generator is now done)
next(g)   # none
```

- `give` exits the generator immediately, returning the specified value.
- Subsequent calls to `next()` return `none`.

---

## Generators and Errors

If an error occurs inside a generator, it propagates to the caller and the generator is **terminated**:

```xell
fn faulty():
    yield 1
    throw "oops"
    yield 2   # never reached
;

let g = faulty()
next(g)   # 1
next(g)   # throws "oops" – generator is now dead
```

After an error, the generator cannot be resumed – any further `next()` calls return `none`.

---

## Infinite Generators

Generators can represent infinite sequences:

```xell
fn infinite_count():
    let i = 0
    while true:
        yield i
        i = i + 1
    ;
;

let numbers = infinite_count()
for n in numbers:
    if n > 10 : break
    print(n)   # 0..10
;
```

Use `break` to stop consuming an infinite generator.

---

## Generator Implementation

Xell generators are **thread‑based** under the hood:

- Each generator runs in its own worker thread.
- The thread is paused at each `yield` using a mutex and condition variable.
- `next()` signals the thread to resume and waits for the next `yield` or completion.
- When a generator is garbage‑collected, its thread is joined (stopped).

This means:
- Generators are **safe** to use with `break` and `continue`.
- They are **concurrent** – multiple generators can be interleaved.
- There is minimal overhead for each `yield`/`resume` cycle.

---

## Common Patterns

### 1. Filtering generator

```xell
fn filter_generator(source, predicate):
    for val in source:
        if predicate(val):
            yield val
    ;
;

let numbers = count_up_to(10)
let evens = filter_generator(numbers, x => x % 2 == 0)
for n in evens:
    print(n)   # 0,2,4,6,8
;
```

### 2. Generator pipeline

```xell
fn squared(source):
    for x in source:
        yield x * x
    ;
;

fn negated(source):
    for x in source:
        yield -x
    ;
;

let pipeline = negated(squared(count_up_to(5)))   # [0, -1, -4, -9, -16]
```

### 3. Reading large files

```xell
fn read_lines(path):
    let f be file = File(path, "r") :
        while not f->eof():
            yield f->readline()
        ;
    ;
;

for line in read_lines("bigfile.txt"):
    process(line)
;
```

### 4. Random number generator

```xell
fn random_sequence(seed = 0):
    let state = seed
    while true:
        state = (state * 1103515245 + 12345) % 2147483647
        yield state
    ;
;
```

---

## `await` and Generators

Generators are the foundation of Xell's asynchronous support:

```xell
async fn fetch_all():
    let data = await fetch_from_api()   # waits for generator completion
    give data
;
```

`await` **exhausts** a generator and returns its final value (the last yielded or `give`n value). This allows generators to represent asynchronous operations that yield progress updates.

See [Async](async.md) for details.

---

## Pitfalls and Gotchas

### 1. Generators are single‑use

```xell
let g = count_up_to(3)
for x in g: print(x)   # 0,1,2
for x in g: print(x)   # nothing – g is exhausted
```

Create a new generator for each iteration.

### 2. `yield` cannot be used outside functions

```xell
let g = yield 10   # ❌ syntax error – yield only inside fn
```

### 3. Recursive generators

```xell
fn recursive_gen(n):
    if n > 0:
        yield n
        for x in recursive_gen(n-1):
            yield x
        ;
    ;
;
```

Recursion works, but each call creates a new generator – be mindful of depth.

### 4. Memory leaks

If a generator is abandoned (not fully consumed), its worker thread is still alive. The GC will join and clean it up when the generator object is collected, but until then it continues to hold resources.

---

## Next Steps

Now that you can create lazy sequences, learn about **[Async](async.md)** – combining generators with `await` for asynchronous programming.

---

**Previous:** [RAII](raii.md)  
**Next:** [Async](async.md)