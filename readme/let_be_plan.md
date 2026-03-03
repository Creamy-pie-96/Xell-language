# `let ... be` — RAII / Context Manager Plan for Xell

> **Status:** Confirmed Design — ready for implementation.
> **Date:** 2026
> **Note:** Xell's resource management construct. Equivalent to Python's
> `with ... as`, but using `let ... be` for Xell's English-like style.
> Guarantees automatic cleanup on block exit — whether normal or via error.

---

## Table of Contents

1. [Design Philosophy](#1-design-philosophy)
2. [Syntax](#2-syntax)
3. [Magic Methods — `__enter__` and `__exit__`](#3-magic-methods----enter--and-__exit__)
4. [Execution Model](#4-execution-model)
5. [Multiple Resources](#5-multiple-resources)
6. [Error Handling Behavior](#6-error-handling-behavior)
7. [Full Class Example](#7-full-class-example)
8. [Use Cases & Patterns](#8-use-cases--patterns)
9. [Grammar Reference](#9-grammar-reference)
10. [Key Decisions](#10-key-decisions)
11. [Implementation Roadmap](#11-implementation-roadmap)
12. [Appendix A: Magic Method List Update](#12-appendix-a-magic-method-list-update)
13. [Appendix B: Comparison with Python](#13-appendix-b-comparison-with-python)
14. [Appendix C: What We're NOT Doing](#14-appendix-c-what-were-not-doing)

---

## 1. Design Philosophy

- **RAII by convention** — any class that implements `__enter__` and `__exit__` works. No special
  base class, no interface required.
- **Guaranteed cleanup** — `__exit__` is always called when the block ends, whether it exits
  normally, via `give`, or via an error/exception. No exceptions to this rule.
- **Reverse order teardown** — when multiple resources are bound, `__exit__` is called on them
  in **reverse** order of binding. The last resource opened is the first closed.
- **English-like** — `let File("a.txt") be f :` reads naturally as a sentence.
- **Consistent with existing magic method design** — `__enter__` and `__exit__` follow the same
  `__dunder__` naming convention as the rest of Xell's OOP magic methods.

---

## 2. Syntax

### Single resource

```xell
let File("a.txt", "r") be f :
    content = f->read()
    print content
;
```

### Multiple resources — comma separated

```xell
let File("a.txt", "r") be f1, File("b.txt", "w") be f2 :
    f2->write(f1->read())
;
```

### Any expression on the left — not just constructors

```xell
let Database->connect("localhost") be db :
    db->query("SELECT * FROM users")
;

let Mutex->acquire() be lock :
    shared_data += 1
;

let db->transaction() be tx :
    tx->execute("INSERT INTO users ...")
;
```

### No binding name needed — resource managed but not accessed by name

```xell
let Lock() be _ :
    # critical section — don't need the lock object itself
    shared_data += 1
;
```

---

## 3. Magic Methods — `__enter__` and `__exit__`

Two new magic methods are added to the Xell magic method list:

```xell
fn __enter__(self)    # called when let block starts
                      # return value is bound to the name after "be"

fn __exit__(self)     # called when let block ends — ALWAYS, even on error
                      # return value is ignored
```

### `__enter__`

- Called immediately after the resource expression is evaluated.
- The **return value** of `__enter__` is what gets bound to the name after `be`.
- In most cases this is just `give self` — but it can return a different proxy object.

### `__exit__`

- Called when the `let` block exits, regardless of how it exits.
- No arguments beyond `self` — error details are not passed (keep it simple).
- Return value is **ignored**.
- If `__exit__` itself raises an error, it propagates (same as Python).

### Minimal protocol

A class only needs to implement both `__enter__` and `__exit__` to be usable with `let ... be`.
No base class, no interface required. Pure duck typing.

```xell
class Minimal :
    fn __enter__(self) : give self ;
    fn __exit__(self)  : print "cleaned up" ;
;

let Minimal() be m :
    print "inside"
;
# output:
# inside
# cleaned up
```

---

## 4. Execution Model

Step-by-step for `let EXPR be NAME : BLOCK ;`:

```
1. Evaluate EXPR → produces an object (call it resource)
2. Call resource->__enter__() → result bound to NAME
3. Execute BLOCK with NAME in scope
4. When BLOCK exits (any reason):
       → Call resource->__exit__()
       → NAME goes out of scope
5. If BLOCK raised an error:
       → __exit__() is still called first (step 4)
       → then the error propagates normally
```

The binding is to the **return value of `__enter__`**, not to the raw resource object. This
matters when `__enter__` returns a proxy or wrapper.

```xell
class Connection :
    fn __enter__(self) :
        self->_cursor = self->_db->open_cursor()
        give self->_cursor      # bind the cursor, not the connection
    ;
    fn __exit__(self) :
        self->_cursor->close()
        self->_db->close()
    ;
;

let Connection() be cursor :
    cursor->execute("SELECT 1")   # cursor, not Connection object
;
```

---

## 5. Multiple Resources

Multiple `EXPR be NAME` bindings are comma-separated in one `let` statement.

```xell
let File("a.txt", "r") be f1, File("b.txt", "w") be f2 :
    f2->write(f1->read())
;
```

### Binding order

Resources are bound **left to right**:

```
1. Evaluate File("a.txt", "r") → call __enter__ → bind f1
2. Evaluate File("b.txt", "w") → call __enter__ → bind f2
3. Execute block
4. Exit: call f2->__exit__()   ← reverse order: last in, first out
5. Exit: call f1->__exit__()
```

### Why reverse order matters

```xell
let DatabasePool() be pool, pool->get_connection() be conn :
    conn->query("...")
;
# Correct teardown order:
# 1. conn->__exit__()    (close connection first — it depends on pool)
# 2. pool->__exit__()    (then close pool)
```

If teardown were in forward order, `pool` could be destroyed before `conn` is released back to it,
causing undefined behavior.

### If `__enter__` of a later resource fails

```
1. f1->__enter__() → success, f1 bound
2. f2->__enter__() → FAILS with error
        → f1->__exit__() is called  (already entered resources are cleaned up)
        → error propagates
```

No resource leak even on partial initialization.

---

## 6. Error Handling Behavior

### Normal exit

```xell
let File("a.txt", "r") be f :
    content = f->read()
;
# __exit__ called — file closed
```

### Error inside block

```xell
let File("a.txt", "r") be f :
    content = f->read()
    error("something went wrong")    # error raised here
;
# __exit__ called FIRST — file closed
# THEN error propagates to caller
```

### Early return (`give`) inside block

```xell
fn process() :
    let File("a.txt", "r") be f :
        data = f->read()
        give data           # early return
    ;
    # __exit__ called before give unwinds — file closed correctly
;
```

### `__exit__` itself raises an error

If `__exit__` raises an error:

- If the block exited normally → `__exit__`'s error propagates.
- If the block raised an error AND `__exit__` also raises → `__exit__`'s error propagates
  (original block error is lost — same behavior as Python).

> This is a known limitation. User should not raise errors in `__exit__` unless intentional.

---

## 7. Full Class Example

```xell
class File :
    fn __init__(self, path, mode) :
        self->path = path
        self->mode = mode
        self->_handle = null
    ;

    fn __enter__(self) :
        self->_handle = os_open(self->path, self->mode)
        give self
    ;

    fn __exit__(self) :
        if self->_handle != null :
            self->_handle->close()
            self->_handle = null
        ;
    ;

    fn read(self) :
        give self->_handle->read()
    ;

    fn write(self, content) :
        self->_handle->write(content)
    ;
;


# Usage
let File("data.txt", "r") be f :
    print f->read()
;
# file always closed after this point


class Timer :
    fn __init__(self) :
        self->_start = null
        self->elapsed = 0
    ;

    fn __enter__(self) :
        self->_start = time_now()
        give self
    ;

    fn __exit__(self) :
        self->elapsed = time_now() - self->_start
        print "elapsed: {self->elapsed}ms"
    ;
;


let Timer() be t :
    do_heavy_work()
;
# prints: elapsed: 142ms


class TempDir :
    fn __enter__(self) :
        self->path = fs_mktemp_dir()
        give self
    ;

    fn __exit__(self) :
        fs_remove_dir(self->path)    # always cleaned up
    ;
;


let TempDir() be tmp :
    write("{tmp->path}/data.txt", "hello")
    process_files(tmp->path)
;
# temp directory deleted automatically
```

---

## 8. Use Cases & Patterns

### File I/O

```xell
let File("config.json", "r") be f :
    cfg = json_parse(f->read())
;
```

### Copy file — two handles

```xell
let File("src.txt", "r") be src, File("dst.txt", "w") be dst :
    dst->write(src->read())
;
```

### Database transaction

```xell
let db->transaction() be tx :
    tx->execute("INSERT INTO log ...")
    tx->execute("UPDATE counter SET n = n + 1")
;
# commits on success, rolls back on error (implemented in __exit__)
```

### Mutex / lock

```xell
let Mutex->acquire() be lock :
    shared_counter += 1
;
# lock released when block exits, even if error occurs
```

### Network socket

```xell
let Socket("example.com", 80) be sock :
    sock->send("GET / HTTP/1.0\r\n\r\n")
    print sock->recv()
;
# socket closed automatically
```

### Timer / profiling

```xell
let Timer() be t :
    result = expensive_computation()
;
# prints elapsed time, result usable after block
```

### Temporary directory

```xell
let TempDir() be tmp :
    write("{tmp->path}/input.csv", data)
    output = run_process("./tool", tmp->path)
;
# temp dir deleted after block
```

### Scoped log context

```xell
let LogContext("request-123") be log :
    log->info("processing started")
    do_work()
    log->info("processing done")
;
# log context flushed and closed automatically
```

---

## 9. Grammar Reference

```
LET_STMT    := "let" LET_BINDING { "," LET_BINDING } ":" BLOCK ";"

LET_BINDING := EXPRESSION "be" IDENTIFIER

BLOCK       := { STATEMENT }
```

### Examples of valid forms

```xell
# 1 binding
let EXPR be NAME : BLOCK ;

# 2 bindings
let EXPR1 be NAME1, EXPR2 be NAME2 : BLOCK ;

# N bindings
let E1 be n1, E2 be n2, E3 be n3 : BLOCK ;

# Discard binding (no name needed)
let EXPR be _ : BLOCK ;
```

### Not valid

```xell
let File("a.txt") :           # Error: missing "be NAME"
let be f : ... ;              # Error: missing expression
let f1, f2 be x : ... ;       # Error: each binding must be EXPR be NAME
```

---

## 10. Key Decisions

| Decision                 | Choice                   | Reason                                                  |
| ------------------------ | ------------------------ | ------------------------------------------------------- |
| Keyword                  | `let ... be`             | Reads as a natural English sentence; fits Xell's style  |
| vs Python `with ... as`  | Different                | `with`/`as` feel Python-specific; `let`/`be` is fresher |
| `__enter__` return value | Bound to name            | Allows proxy pattern (cursor from connection, etc.)     |
| `__exit__` parameters    | `self` only              | Keep it simple — no error info passed                   |
| Teardown order           | Reverse                  | Correct RAII: last opened, first closed                 |
| Multiple bindings        | Comma in one `let`       | Avoids nesting; flat and readable                       |
| Partial init failure     | Clean up already-entered | No resource leak on mid-binding failure                 |
| `give` inside block      | `__exit__` still called  | Guaranteed cleanup on all exit paths                    |
| `error` inside block     | `__exit__` still called  | Guaranteed cleanup on all exit paths                    |
| `__exit__` raises        | Its error propagates     | Keep behavior predictable                               |
| Protocol                 | Duck typing only         | No base class or interface required                     |
| `_` as discard name      | Supported                | For when resource is managed but not used by name       |

---

## 11. Implementation Roadmap

### Phase 1: Lexer (~0.5 days)

- [ ] Add `LET` token (`let` keyword)
- [ ] Add `BE` token (`be` keyword)
- [ ] Verify `let` doesn't collide with any existing keyword
- [ ] Tests: `let` and `be` tokenize correctly

### Phase 2: AST Node (~0.5 days)

- [ ] Add `LetStmt` AST node:
  - `bindings`: list of `(expression, identifier)` pairs
  - `body`: block of statements
- [ ] `LetBinding` sub-node: holds `expr` + `name`
- [ ] Tests: AST node construction

### Phase 3: Parser (~1 day)

- [ ] Parse `let EXPR be NAME : BLOCK ;`
- [ ] Parse multiple bindings: `let E1 be n1, E2 be n2 : BLOCK ;`
- [ ] Parse `_` as a valid discard name
- [ ] Error: missing `be` after expression
- [ ] Error: missing `:` after last binding
- [ ] Tests: all binding forms, error cases

### Phase 4: Interpreter — Basic Execution (~1 day)

- [ ] `evalLetStmt()` function:
  - For each binding left-to-right:
    - Evaluate expression → resource object
    - Call `resource->__enter__()`
    - Bind return value to name in current scope
    - Track resource in binding list
  - Execute body block
  - On exit (any path): call `__exit__()` on all bound resources in reverse order
- [ ] `_` discard: `__enter__` still called, but return value not stored
- [ ] Tests: basic let/be with simple class

### Phase 5: Interpreter — Guaranteed Cleanup (~1 day)

- [ ] Wrap body execution in try/catch at interpreter level
- [ ] On any error thrown in body: call all `__exit__` in reverse order, then re-throw error
- [ ] On `give`/return inside body: call all `__exit__` before unwinding
- [ ] On `break`/`continue` if inside loop: call `__exit__` before jumping
- [ ] Partial init: if `__enter__` of binding N fails, call `__exit__` on bindings 0..N-1 in reverse
- [ ] Tests: error in body, give inside block, partial init failure

### Phase 6: Magic Method Registration (~0.5 days)

- [ ] Add `__enter__` and `__exit__` to the recognized magic method list in interpreter
- [ ] Validate at `let` execution time: if resource has no `__enter__` → runtime error with clear message
- [ ] Validate at `let` execution time: if resource has no `__exit__` → runtime error with clear message
- [ ] Error message: `"TypeError: let ... be requires __enter__ and __exit__ on <typename>"`
- [ ] Tests: missing `__enter__`, missing `__exit__`

### Phase 7: Tests (~1 day)

- [ ] Basic single binding — file-like class
- [ ] Multiple bindings — teardown reverse order verified
- [ ] Error inside block — `__exit__` called before propagation
- [ ] `give` inside block — `__exit__` called
- [ ] Partial init failure — only entered resources cleaned up
- [ ] `_` discard name
- [ ] `__enter__` returns proxy, proxy bound to name
- [ ] Nested `let` blocks
- [ ] `__exit__` raises — its error propagates
- [ ] Missing `__enter__` / `__exit__` — clear error message

**Total estimated: ~5 days**

---

## 12. Appendix A: Magic Method List Update

Two new entries added to the full magic method list (from `oop_plan.md`):

```xell
fn __enter__(self)    # let ... be — called at block start, return value bound to name
fn __exit__(self)     # let ... be — called at block end, ALWAYS, even on error
```

Updated complete list now has **26 magic methods** (was 24).

---

## 13. Appendix B: Comparison with Python

| Feature                   | Python `with ... as`                               | Xell `let ... be`                         |
| ------------------------- | -------------------------------------------------- | ----------------------------------------- |
| Keyword                   | `with ... as`                                      | `let ... be`                              |
| Multiple resources        | `with A() as a, B() as b :`                        | `let A() be a, B() be b :`                |
| Enter method              | `__enter__(self)`                                  | `__enter__(self)` — same                  |
| Exit method               | `__exit__(self, exc_type, exc_val, tb)`            | `__exit__(self)` — simpler, no error args |
| Exit on error             | ✅ Always                                          | ✅ Always                                 |
| Exit on return            | ✅ Always                                          | ✅ Always                                 |
| Reverse teardown order    | ✅                                                 | ✅                                        |
| Suppress errors from exit | ✅ (return True from `__exit__`)                   | ❌ Not supported — keep it simple         |
| Partial init cleanup      | ✅                                                 | ✅                                        |
| Protocol                  | `contextlib.AbstractContextManager` or duck typing | Duck typing only                          |
| Nested `with`             | Supported                                          | Supported (nested `let`)                  |

The main simplification: Xell's `__exit__` takes no error arguments and cannot suppress errors.
This keeps the protocol clean — if you need error-aware cleanup, handle it inside the block.

---

## 14. Appendix C: What We're NOT Doing

| Feature                            | Status         | Reason                                          |
| ---------------------------------- | -------------- | ----------------------------------------------- |
| Error args in `__exit__`           | ❌ Not planned | Keeps protocol simple; handle errors in block   |
| Error suppression from `__exit__`  | ❌ Not planned | Surprising behavior; not worth the complexity   |
| `let` without `be` (no binding)    | ❌ Not planned | Every resource should be named or `_`           |
| Async context managers             | ⏳ Maybe later | Depends on async/await being added to Xell      |
| Generator-based context managers   | ❌ Not planned | No `@contextmanager` decorator style            |
| `let` as general scoping construct | ❌ Not planned | `let ... be` is only for RAII — not for scoping |
| Reusing the name after block exits | ❌ Not planned | Name goes out of scope when block ends          |

---

_This document reflects the confirmed `let ... be` design. Implement after OOP phases are stable._
