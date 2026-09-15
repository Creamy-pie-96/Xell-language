# Expressions

> **Previous:** [Operators](operators.md)  
> **Next:** [Control Flow](control-flow.md)

---

In Xell, **expressions** are code that produce a value. Almost everything that isn't a statement is an expression, including many control‑flow constructs.

---

## Literal Expressions

Literals are the simplest expressions:

```xell
42              # integer
3.14            # float
"hello"         # string
[1, 2, 3]       # list
{ "a": 1 }      # map
true            # boolean
none            # null
```

---

## Variable References

A variable name is an expression that evaluates to its current value:

```xell
let x = 10
x               # 10
```

---

## Arithmetic Expressions

Use operators to combine values:

```xell
x + y * 2       # multiplication before addition
(x + y) * 2     # parentheses override precedence
```

See [Operators](operators.md) for the full list.

---

## Ternary Expression (`if` … `else`)

Xell’s ternary is written as:

```xell
<value> if <condition> else <alternative>
```

```xell
let age = 18
let status = "adult" if age >= 18 else "minor"
print(status)   # "adult"
```

This is an expression that returns either the first value (if condition is true) or the alternative.

---

## `if` Expression

For multiple branches, use the expression‑mode `if`:

```xell
let result = if x > 0 : "positive"
             elif x == 0 : "zero"
             else : "negative" ;
```

- Must have an `else` branch (to always produce a value).
- Branches are separated by the normal `:` … `;` block syntax.
- The entire construct is a single expression.

---

## `for` Expression

A `for` loop can be used as an expression to collect values:

```xell
let squares = for i in [1, 2, 3, 4, 5]:
    give i * i
;
print(squares)   # [1, 4, 9, 16, 25]
```

**Syntax:**

```xell
<variable> = for <targets> in <iterables> :
    <body statements>
    [give <default>]   # optional default if no break
;
```

- The loop body is a block of statements (`;`‑terminated).
- Use `break <value>` inside the body to yield a value and exit.
- If a `give <default>` is present at the end, it’s used if the loop completes without a `break`.
- The result is a **list** of all yielded values (from `break` statements).
- If no `break` is hit and no `give` is present, the result is `none`.

**With `break`:**

```xell
let first_even = for i in [1, 2, 3, 4, 5]:
    if i % 2 == 0 : break i ;
;
print(first_even)   # 2
```

**Multiple iterables (parallel):**

```xell
let pairs = for a, b in [1, 2, 3], ["a", "b", "c"]:
    give [a, b]
;
print(pairs)   # [[1, "a"], [2, "b"], [3, "c"]]
```

---

## `while` Expression

Similar to `for`, a `while` loop can be used as an expression:

```xell
let result = while x > 0 :
    if x == 5 : break "found" ;
    x = x - 1 ;
    give "not found"   # default
;
```

- The body is a block of statements.
- `break <value>` yields a value and exits.
- `give <default>` (optional) is used if the loop ends without hitting a `break`.
- Result is a single value (the last `break` value, or the default).

---

## `loop` Expression

An infinite loop can also be an expression:

```xell
let val = loop :
    if condition : break "done" ;
    # ...
    give "timeout"   # optional default
;
```

Same rules as `while` expression.

---

## `incase` Expression

Pattern matching can be an expression:

```xell
let result = incase value :
    is 0 : "zero"
    belong int : "integer"
    bind x if x > 10 : "big: " + str(x)
    else : "unknown"
;
```

- Each clause must produce a value (an expression, not a block of statements).
- The `else` branch is **required** (expression must always return a value).
- Guards (`if condition`) are allowed.
- The `bind` clause creates a variable that is visible in the guard and result expression.

---

## `yield` Expression

Inside a generator function, `yield` suspends execution and produces a value:

```xell
fn count_up_to(n):
    let i = 0
    while i < n:
        yield i
        i = i + 1
    ;
;

let gen = count_up_to(3)
for val in gen: print(val)   # 0, 1, 2
```

- `yield` can appear anywhere an expression is allowed.
- It always produces `none` from the generator’s perspective (the yielded value goes to the caller).

---

## `await` Expression

Used with `async fn` to wait for a generator to complete:

```xell
async fn fetch_data():
    let data = await async_operation()
    give data
;
```

- `await` works on generator values; for non‑generators, it is a no‑op (returns the value as‑is).
- It exhausts the generator and returns its final value.

---

## `spread` (`...`) Expression

Used inside list literals and function calls to unpack a list:

```xell
let list = [1, 2, 3]
let expanded = [0, ...list, 4]   # [0, 1, 2, 3, 4]

fn sum(a, b, c): give a + b + c ;
let args = [1, 2, 3]
let total = sum(...args)         # 6
```

In function calls, `...` spreads a list into positional arguments.

---

## Grouping with Parentheses

Parentheses force evaluation order and group expressions:

```xell
(2 + 3) * 4     # 20
2 + (3 * 4)     # 14
```

They also create **tuples** when used with commas:

```xell
let t = (1, 2, 3)   # tuple
let single = (1,)   # tuple with one element (trailing comma required)
```

---

## Expression vs. Statement

| Expression | Statement |
|------------|-----------|
| Produces a value | Performs an action |
| Can be used anywhere a value is expected | Cannot be used in expressions |
| Examples: `x + y`, `if … else`, `for … give` | Examples: `if … ;`, `for … ;`, `assignment ;` |

The same keyword (e.g., `if`, `for`, `loop`) can be used as either a statement or an expression depending on context:

- **Statement mode:** `if x > 0 : print("positive") ;` (no value produced)
- **Expression mode:** `let msg = if x > 0 : "positive" else : "negative" ;` (produces a value)

---

## Next Steps

Now that you understand expressions, explore **[Control Flow](control-flow.md)** for the statement versions of these constructs.

---

**Previous:** [Operators](operators.md)  
**Next:** [Control Flow](control-flow.md)