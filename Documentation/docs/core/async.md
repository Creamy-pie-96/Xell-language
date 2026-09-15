# Async – Asynchronous Programming

> **Previous:** [Generators](generators.md)  
> **Next:** [Shell](shell.md)

---

Xell supports **asynchronous programming** through a simple `async` / `await` model built on top of generators. `async fn` allows you to write functions that can be paused and resumed, while `await` waits for generators to complete, making it easy to compose asynchronous workflows without callback hell.

---

## Quick Reference

| Keyword/Syntax | What it does | Minimal example |
|----------------|--------------|-----------------|
| `async fn` | Declares a function that can contain `await` expressions | `async fn fetch(): give await get_data() ;` |
| `await <expr>` | Waits for a generator to finish and returns its final value | `let result = await long_running()` |
| Generators + `await` | Non‑generator values passed to `await` are returned as‑is | `await 42` → `42` |

---

## `async fn` – Asynchronous Functions

Mark a function with `async` to enable the use of `await` inside it:

```xell
async fn fetch_data():
    let raw = await read_from_network()
    let processed = process(raw)
    give processed
;
```

- `async fn` is syntactically identical to regular `fn`, but allows `await` inside its body.
- It returns a normal value (like any function) – it does **not** automatically become a generator unless it contains `yield`.
- If an `async fn` contains only `await` (no `yield`), it executes synchronously but can wait for generators to finish.

---

## `await` – Waiting for Completion

`await` takes an expression and waits for it to complete:

```xell
let final_value = await some_generator()
```

- If the expression evaluates to a **generator**, `await` exhausts it and returns the **last yielded value** (or the value passed to `give` inside the generator).
- If the expression evaluates to a **non‑generator**, `await` is a **no‑op** – it simply returns the value as‑is.
- `await` can only be used inside an `async fn`.

### Example: Awaiting a generator

```xell
fn count_up_to(n):
    let i = 0
    while i < n:
        yield i
        i = i + 1
    ;
    give "done"
;

async fn print_final():
    let result = await count_up_to(3)
    print(result)   # "done"
;

print_final()
```

**Output:**
```
done
```

- `count_up_to` yields `0`, `1`, `2`, then gives `"done"`.
- `await` exhausts the generator and returns `"done"`.

---

## Awaiting Non‑Generators

If you accidentally (or intentionally) `await` a non‑generator, it simply passes through:

```xell
async fn example():
    let x = await 42
    print(x)   # 42
;
```

This is useful for making APIs that work with both async and sync values transparently.

---

## Combining `async` and `yield`

You can mix `async` and `yield` in the same function. The function becomes a generator that can be awaited:

```xell
async fn process_stream():
    let i = 0
    while i < 5:
        yield i
        i = i + 1
    ;
    give "stream done"
;

async fn consume():
    let final = await process_stream()   # exhausts the generator
    print(final)   # "stream done"
;

consume()
```

- The function is both `async` and a generator because it contains `yield`.
- `await` will exhaust it and return the final `give` value.

---

## Error Propagation in Async

If a generator throws an error, `await` propagates that error to the caller:

```xell
fn faulty():
    yield 1
    throw "oops"
;

async fn test():
    let val = await faulty()   # throws "oops"
;

# Calling test() will propagate the error
try :
    test()
catch e :
    print("Caught: " + e->message)
;
```

---

## Async and Control Flow

You can use `await` inside loops, conditionals, and other control structures:

```xell
async fn fetch_all(urls):
    let results = []
    for url in urls:
        let data = await fetch(url)
        push(results, data)
    ;
    give results
;
```

---

## Async Execution Model

Under the hood:

- `async fn` is just a normal function that may contain `await`.
- `await` works by checking if the value is a generator. If so, it drives it to completion by repeatedly calling `next()` until exhausted.
- There is **no event loop** – the interpreter simply executes the generator synchronously until it finishes. This makes `async` useful for composing generators without introducing concurrency or parallelism.

> **Note:** Xell's `async` does **not** provide parallel execution. It is purely a way to **compose** generators and `yield`/`give` workflows. For concurrency, use the `threading` module.

---

## Common Patterns

### 1. Retry logic

```xell
async fn retry_operation(times):
    let attempt = 0
    while attempt < times:
        try :
            let result = await fragile_operation()
            give result
        catch e :
            attempt += 1
            if attempt >= times : throw e
        ;
    ;
;
```

### 2. Timeout wrapper

```xell
async fn timeout(gen, seconds):
    let start = now()
    for val in gen:
        if now() - start > seconds:
            throw "timeout"
        yield val
    ;
;
```

### 3. Transforming generator output

```xell
async fn map_async(source, fn):
    for val in source:
        yield fn(val)
    ;
    give "mapped"
;
```

---

## Async and the REPL

You can call `async fn` directly in the REPL – it runs synchronously (blocking) and returns its result:

```
> async fn greet(): give "hello" ;
> greet()
"hello"
```

---

## Pitfalls and Gotchas

### 1. `await` only inside `async fn`

```xell
let x = await some_gen()   # ❌ error – await outside async fn
```

### 2. `await` does not parallelise

```xell
async fn fetch_two():
    let a = await fetch("url1")
    let b = await fetch("url2")   # fetches sequentially, not in parallel
    give [a, b]
;
```

Use the `threading` module for true parallelism.

### 3. Awaiting the same generator twice

```xell
let g = some_gen()
await g   # exhausts g
await g   # returns none (generator already done)
```

Generators are single‑use – create a fresh generator for each `await`.

### 4. `async fn` without `await` is just a regular function

Marking a function `async` but not using `await` has no effect – it behaves like a normal function.

---

## When to Use Async

Use `async` when:
- You need to compose generators that produce values over time.
- You want to await the final result of a generator (e.g., after a sequence of yields).
- You are building APIs that can handle both sync and async values transparently.

Do **not** use `async` for parallel/concurrent tasks – that's what the `threading` module is for.

---

## Next Steps

Now that you understand async, learn about **[Shell](shell.md)** – integrating external commands and pipelines.

---

**Previous:** [Generators](generators.md)  
**Next:** [Shell](shell.md)