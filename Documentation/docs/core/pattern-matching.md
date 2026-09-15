# Pattern Matching

> **Previous:** [Error Handling](error-handling.md)  
> **Next:** [RAII](raii.md)

---

Xell provides powerful pattern matching via the **`incase`** construct. It combines value equality checks, type membership tests, and variable binding – all with optional guard conditions.

---

## `incase` Statement

The basic syntax is:

```xell
incase <subject> :
    <clause1>
    <clause2>
    ...
    [else : <body>]
;
```

- The `subject` is evaluated once.
- Clauses are checked in order.
- The **first matching clause** executes its body.
- `else` is optional in statement mode.

---

## Clause Types

### 1. `is` – Value Equality

Matches when the subject equals one of the listed values.

```xell
incase x :
    is 0 : print("zero")
    is 1 or 2 : print("one or two")
    is "done" : print("finished")
;
```

- Values are separated by `or`.
- Uses `==` semantics (deep equality for collections).
- The list of values is evaluated at match time.

### 2. `belong` – Type Check

Matches when the subject is of the specified type or class.

```xell
incase value :
    belong int : print("integer")
    belong string : print("string")
    belong list : print("list")
    belong Dog : print("a Dog instance")
;
```

**Supported type names:**
- Built‑ins: `int`, `float`, `complex`, `bool`, `string`, `list`, `tuple`, `set`, `frozen_set`, `map`, `function`, `bytes`, `none`, `num` (int or float)
- Custom classes/structs: **exact name match** (case‑sensitive)

> **Important:** `belong` uses **exact name matching** for custom classes – it does **not** check inheritance. If you have `class Animal` and `class Dog inherits Animal`, then `belong Animal` will **not** match a `Dog` instance. For polymorphic type checks, use the `is` operator instead (e.g., `obj is Animal` in a condition).

### 3. `bind` – Capture

Always matches and binds the subject to a variable for use in the clause body and guard.

```xell
incase x :
    bind y : print("captured: " + str(y))
;
```

The binding is scoped **only** to that clause.

---

## Guards (`if`)

Add a guard condition to any clause. The guard is evaluated **after** value/type matching but **before** the body executes.

```xell
incase x :
    is 10 if y > 0 : print("x is 10 and y positive")
    belong int if x > 100 : print("large integer")
    bind n if n % 2 == 0 : print("even number: " + str(n))
    else : print("no match")
;
```

- For `bind`, the guard can reference the bound variable.
- If the guard evaluates to `false`, the clause is skipped and matching continues.
- Guard expressions must be truthy/falsy (see [Truthiness](control-flow.md#truthiness-in-conditions)).

---

## `else` Clause

Optional default branch when no clause matches.

```xell
incase value :
    is 0 : print("zero")
    belong int : print("integer")
    else : print("other")
;
```

In **statement mode**, `else` is optional – if no clause matches and no `else` is present, nothing happens.

---

## `incase` Expression

`incase` can also be used as an **expression** that returns a value:

```xell
let description = incase x :
    is 0 : "zero"
    belong int : "integer"
    else : "other"
;
```

**Differences from statement mode:**
- Each clause body is an **expression** (not a block of statements).
- The `else` branch is **required** (the expression must always produce a value).
- The result is the value of the matching clause.

**With guards in expression mode:**

```xell
let result = incase value :
    is 10 if y > 0 : "ten and positive"
    belong int if value > 100 : "big int"
    else : "default"
;
```

---

## Scope and Binding Visibility

Variables bound with `bind` are **only** visible inside that clause’s body and guard. They do not leak to outer scopes.

```xell
incase x :
    bind z : print(z)    # z is visible here
;
# print(z)   # error – z is not defined
```

Each clause creates its own environment, so bindings in different clauses do not conflict.

---

## Complete Examples

### 1. Parsing user input

```xell
let input = "42"

incase input :
    belong int : print("It's a number")
    belong string : print("It's a string")
    else : print("unknown")
;
```

### 2. Command dispatcher

```xell
fn handle_command(cmd):
    incase cmd :
        is "quit" or "exit" : give "shutting down"
        is "help" : give "available commands: quit, help, status"
        bind c if c->starts_with("echo ") : give "Echo: " + c->substr(5)
        else : give "unknown command"
    ;
;

print(handle_command("echo hello"))   # "Echo: hello"
```

### 3. Type‑aware processing

```xell
fn process(value):
    incase value :
        belong int : give value * 2
        belong string : give value->upper()
        belong list : give len(value)
        else : give none
    ;
;
```

### 4. Pattern matching with maps

```xell
let point = { "x": 10, "y": 20 }

incase point :
    is { "x": 0, "y": 0 } : print("origin")
    belong map if point["x"] > 0 and point["y"] > 0 : print("quadrant I")
    else : print("other")
;
```

---

## Common Patterns

### Type‑safe branching

```xell
incase data :
    belong int : process_int(data)
    belong string : process_string(data)
    belong list : process_list(data)
    else : print("unhandled type")
;
```

### Value range matching

```xell
incase score :
    belong int if score >= 90 : print("A")
    belong int if score >= 80 : print("B")
    belong int if score >= 70 : print("C")
    else : print("F")
;
```

### Error type handling

```xell
try :
    # ...
catch e :
    incase e->type :
        is "TypeError" : handle_type_error(e)
        is "IndexError" : handle_index_error(e)
        else : handle_generic(e)
    ;
;
```

---

## Next Steps

Now that you can match patterns, learn about **[RAII](raii.md)** – resource management with `let … be`.

---

**Previous:** [Error Handling](error-handling.md)  
**Next:** [RAII](raii.md)