# RAII – Resource Acquisition Is Initialization

> **Previous:** [Pattern Matching](pattern-matching.md)  
> **Next:** [Generators](generators.md)

---

Xell provides built‑in **RAII (Resource Acquisition Is Initialization)** via the `let … be` construct. It guarantees that resources (files, database connections, locks, etc.) are properly acquired and automatically released when they go out of scope, even if an error occurs.

---

## The `let … be` Construct

### Syntax

```xell
let <expr1> be <name1>, <expr2> be <name2>, ... :
    <body statements>
;
```

- Each `<expr>` is a resource expression that must evaluate to an object implementing `__enter__` and `__exit__`.
- `<name>` is the variable that receives the result of calling `__enter__` (the acquired resource).
- Use `_` to discard the `__enter__` result (if you only need the side effects).
- The body is a block of statements that run with the resources acquired.
- After the body completes (or if an exception is thrown), `__exit__` is called on **all** resources in **reverse** order of acquisition.

---

## Magic Methods Required

For an object to work with `let … be`, it must implement:

- `__enter__(self)` – called when the resource is acquired. Returns the resource (or any value to be bound to the name).
- `__exit__(self)` – called when the resource is released. Should perform cleanup.

```xell
class File :
    fn __init__(self, path): self->path = path ;
    fn __enter__(self):
        self->handle = open(self->path, "r")
        give self->handle
    ;
    fn __exit__(self):
        if self->handle : close(self->handle) ;
    ;
;
```

If either method is missing, a `TypeError` is raised at runtime.

---

## Basic Usage

### File handling

```xell
let f be file = File("data.txt") :
    let content = f->read()
    print(content)
;
# file is automatically closed here
```

The `file` variable is bound to the value returned by `__enter__` (the open file handle). After the body, `__exit__` is called.

### Multiple resources

```xell
let src be source = File("input.txt"),
    dst be dest = File("output.txt", "w") :
    let data = source->read()
    dest->write(data)
;
# source->__exit__() called first (reverse order),
# then dest->__exit__()
```

Resources are released in reverse order: `dest` then `source`. This ensures that if `dest` depends on `source`, `source` stays alive until `dest` is done.

---

## Discarding the Enter Result

If you only need the resource for its side effects (e.g., a lock that manages concurrency) and don't need the value returned by `__enter__`, use `_` as the binding name:

```xell
let lock be _ = Mutex() :
    # critical section
;
# Mutex is released via __exit__
```

This is useful for `__enter__` methods that return nothing or return an object you don't need.

---

## Scope and Visibility

The bound names are **local to the `let … be` body**. They are not visible outside.

```xell
let f be file = File("data.txt") :
    print(file)      # OK
;
print(file)   # error – file is not defined
```

---

## Exception Safety

If an exception is thrown inside the body, `__exit__` is **still called** on all acquired resources before the exception propagates.

```xell
let f be file = File("data.txt") :
    throw "read error"   # file->__exit__() still runs
;
```

If `__enter__` fails (throws) for the *i*‑th resource, all previously acquired resources (1..i-1) are released in reverse order, and the original exception propagates.

---

## Interaction with `try`/`catch`/`finally`

You can combine `let … be` with explicit error handling:

```xell
try :
    let f be file = File("data.txt") :
        # process file
    ;
catch e :
    print("Error: " + e->message)
;
```

The `__exit__` runs before the `catch` block, so resources are always cleaned up before error handling begins.

---

## Common Resource Types

### 1. File I/O

```xell
let f be file = File("example.txt", "r") :
    let content = f->read()
    print(content)
;
```

### 2. Mutex locks (threading)

```xell
let m = Mutex()
# in a thread:
let _ be lock = m :
    # protected section
;
# lock is released after the body
```

### 3. Database transactions

```xell
let conn be db = Database.connect("mydb") :
    db->begin()
    db->execute("INSERT INTO users(name) VALUES('Alice')")
    db->commit()
;
# if commit fails, rollback is automatically called in __exit__
```

### 4. Temporary directories

```xell
let dir be temp = TempDir() :
    # work with temp directory
;
# temp directory is deleted when __exit__ runs
```

---

## `__exit__` Return Value

Xell does **not** inspect the return value of `__exit__`. It is called purely for side effects. If `__exit__` throws an exception, it will replace any exception from the body (or propagate if none).

---

## Implementation Details (from the interpreter)

- **Acquisition order:** Resources are acquired left‑to‑right.
- **Cleanup order:** `__exit__` is called in **reverse** order.
- **Error handling:** If an `__enter__` fails, previously entered resources are cleaned up.
- **Body exceptions:** If the body throws, `__exit__` runs, and then the body exception is re‑thrown *unless* `__exit__` itself throws – in that case the `__exit__` exception replaces the body exception.
- **No return value:** The `let … be` expression always yields `none` (it is a statement, not an expression).

---

## Common Pitfalls

### 1. Forgetting to bind a name

```xell
let _ be resource = acquire() :
    # OK
;
```

If you omit the `be` part, that is a syntax error.

### 2. Using a resource without `__exit__`

```xell
let x be bad = SomeObject() : ... ;   # TypeError if SomeObject lacks __exit__
```

### 3. Modifying the bound variable

```xell
let f be file = File("data.txt") :
    file = None   # ✅ allowed – but doesn't affect cleanup
;
# file is still closed via __exit__
```

Changing the variable does **not** prevent `__exit__` from being called.

### 4. Nested `let … be` with same name

```xell
let f be file = File("a.txt") :
    let f be file = File("b.txt") :
        print(file)   # prints b.txt's handle
    ;
    print(file)       # prints a.txt's handle
;
```

Inner bindings shadow outer ones, and each is cleaned up independently.

---

## When to Use `let … be`

Use `let … be` whenever you acquire a resource that needs explicit release:

- Files, sockets, database connections
- Locks, semaphores
- Temporary directories or files
- Context managers in general

Do **not** use it for simple values like integers or strings – it adds unnecessary overhead.

---

## Next Steps

Now that you can manage resources safely, learn about **[Generators](generators.md)** – lazy sequences with `yield`.

---

**Previous:** [Pattern Matching](pattern-matching.md)  
**Next:** [Generators](generators.md)