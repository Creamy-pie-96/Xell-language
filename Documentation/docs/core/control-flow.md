# Control Flow

> **Previous:** [Expressions](expressions.md)  
> **Next:** [Functions](functions.md)

---

Xell provides familiar control‑flow constructs for conditionals and loops. All of these can be used as **statements** (performing actions) or as **expressions** (producing values – see [Expressions](expressions.md) for the expression‑mode versions).

---

## Conditional: `if` / `elif` / `else`

### Statement Mode

Used to execute blocks of code conditionally.

```xell
if x > 0 :
    print("positive")
elif x == 0 :
    print("zero")
else :
    print("negative")
;
```

- `if` and `elif` conditions are expressions that must evaluate to a boolean (or truthy/falsy).
- The `;` after the `else` block closes the entire construct.
- You can have zero or more `elif` clauses.
- The `else` clause is optional.

**Single‑line form:** (body can be a single statement on the same line)

```xell
if x > 0 : print("positive") ;
```

**Nested `if`:**

```xell
if x > 0 :
    if x > 10 :
        print("large positive")
    else :
        print("small positive")
    ;
;
```

---

### Expression Mode (Ternary)

For returning a value, see the [Expressions](expressions.md) section. The `if` expression uses the same keywords but with a different syntax:

```xell
let sign = if x > 0 : "positive" elif x == 0 : "zero" else : "negative" ;
```

---

## Loops

### `for` – Iteration over collections

```xell
for i in [1, 2, 3] :
    print(i)
;
```

**Multiple targets:** destructuring

```xell
for [a, b] in [[1, 2], [3, 4]] :
    print(a + b)
;
```

**Multiple sources (parallel iteration):**

```xell
for a, b in [1, 2, 3], ["a", "b", "c"] :
    print(a, b)    # prints: 1 a, 2 b, 3 c
;
```

- Sources are zipped to the **shortest** length (extra elements in longer sources are ignored).
- You can also use a **rest** pattern to capture remaining elements:

```xell
for first, ...rest in [[1, 2, 3], [4, 5, 6]] :
    print(first)   # 1, then 4
    print(rest)    # [2, 3], then [5, 6]
;
```

**Iterable types:** lists, tuples, sets, frozen sets, maps (iterates over key‑value pairs as lists `[key, value]`), strings (iterates characters), generators, and any object with `__iter__`.

---

### `while` – Condition‑controlled loop

```xell
let count = 5
while count > 0 :
    print(count)
    count = count - 1
;
```

- The condition is checked **before** each iteration.
- If the condition is false initially, the body is skipped entirely.

---

### `loop` – Infinite loop

```xell
loop :
    print("forever")
    break
;
```

- Must contain a `break` (or `continue`) to exit; otherwise it’s an infinite loop.
- The `@safe_loop` decorator can enforce that a `break` exists statically.

```xell
@safe_loop
loop :
    if condition : break
;
# If no break is found, a compile‑time error is raised at runtime (parse‑time check).
```

---

### `do…while` – Post‑condition loop

Executes the body **at least once**, then checks the condition.

```xell
do :
    print("at least once")
    x = x - 1
; while x > 0
```

- The condition is evaluated after the body.
- The loop continues while the condition is true.

---

## Loop Control: `break` and `continue`

### `break`

Exits the nearest enclosing loop (or `incase`/`switch`).

**Without a value:** used in statement‑mode loops.

```xell
for i in [1,2,3,4,5] :
    if i == 3 : break
    print(i)    # prints 1, 2
;
```

**With a value:** used in **expression‑mode** loops to yield a result.  
In statement‑mode loops, `break <value>` is **not allowed** and will raise a runtime error.

```xell
let first = for i in [1,2,3,4,5] :
    if i % 2 == 0 : break i   # yields 2
;
print(first)   # 2
```

### `continue`

Skips the rest of the current iteration and jumps to the next iteration.

```xell
for i in [1,2,3,4,5] :
    if i % 2 == 0 : continue   # skip even numbers
    print(i)    # prints 1, 3, 5
;
```

---

## `give` – Return from a function

`give` is used to return a value from a function (see [Functions](functions.md)). It also serves as the **default** value in expression‑mode loops.

```xell
fn square(x): give x * x ;   # returns value

let result = for i in [1,2,3] :
    if i == 2 : break "found"   # break yields a value
    give "not found"            # default if no break
;
```

---

## Block Scoping

Each control‑flow construct introduces a new **scope** for its body. Variables declared inside the body are not visible outside.

```xell
if true :
    let x = 10
;
# x is not defined here
```

---

## Truthiness in Conditions

Conditions are evaluated for **truthiness** (not just strict booleans):

- `false`, `none`, `0`, `0.0`, `0+0i`, empty collections (`[]`, `""`, `{}`, `< >`) → **false**
- Everything else → **true**

```xell
if "hello" :    # true (non‑empty string)
    print("string is truthy")
;
```

---

## Common Patterns

**Iterating over map entries:**

```xell
let m = { "a": 1, "b": 2 }
for [key, val] in m :
    print(key, val)
;
```

**Range‑like iteration:**

```xell
for i in range(5) : print(i) ;   # 0,1,2,3,4
```

**Infinite loop with break:**

```xell
loop :
    let cmd = input("> ")
    if cmd == "quit" : break
    print("You said: " + cmd)
;
```

---

## Next Steps

Now that you can control execution flow, learn how to define reusable logic with **[Functions](functions.md)**.

---

**Previous:** [Expressions](expressions.md)  
**Next:** [Functions](functions.md)