# Functions

> **Previous:** [Control Flow](control-flow.md)  
> **Next:** [Lambdas](lambdas.md)

---

Functions are the primary building block of reusable logic in Xell. They are **first‑class values** – can be assigned to variables, passed as arguments, and returned from other functions.

---

## Defining a Function

Use the `fn` keyword:

```xell
fn greet(name):
    print("Hello, " + name)
;
```

- The body is a block (`:` … `;`).
- If the body is a single expression, you can omit the block:

```xell
fn square(x): give x * x ;
```

---

## Parameters

Functions can have zero or more parameters:

```xell
fn zero_params():
    print("no arguments")
;

fn one_param(x):
    print(x)
;

fn two_params(x, y):
    give x + y
;
```

### Type Annotations (Optional)

You can annotate parameters with types for documentation and overload resolution:

```xell
fn add(a: int, b: int): give a + b ;
```

Type annotations do **not** enforce types at runtime by default – they are used for overload selection (see below).

---

## Default Parameter Values

Provide a default value using `=`:

```xell
fn greet(name = "world"):
    print("Hello, " + name)
;

greet()          # Hello, world
greet("Xell")    # Hello, Xell
```

Default values are evaluated **at call time** in the caller’s scope.

---

## Variadic Parameters (`...`)

Capture any number of remaining arguments into a list:

```xell
fn sum_all(first, ...rest):
    let total = first
    for n in rest:
        total += n
    ;
    give total
;

print(sum_all(1, 2, 3, 4))   # 10
```

- The `...` parameter must be the **last** parameter.
- It captures all remaining arguments as a list.

---

## Return Values: `give`

Use `give` to return a value from a function:

```xell
fn add(a, b):
    give a + b
;
```

A function without an explicit `give` returns `none`:

```xell
fn no_return(): print("hello") ;
let result = no_return()   # none
```

**Multiple return values** can be simulated with tuples:

```xell
fn divide(a, b):
    give [a / b, a % b]
;

let [quotient, remainder] = divide(10, 3)
```

---

## Return Type Annotations

You can annotate the return type for clarity:

```xell
fn square(x) -> int:
    give x * x
;
```

This is purely documentation (no runtime enforcement) but may be used by static analyzers.

---

## Closures

Functions capture variables from the scope where they are defined:

```xell
fn make_counter():
    let count = 0
    fn counter():
        count += 1
        give count
    ;
    give counter
;

let c = make_counter()
print(c())   # 1
print(c())   # 2
```

- The inner function retains access to `count` even after `make_counter` returns.
- Variables are captured **by reference** (mutable).

---

## Function Overloading

Xell supports overloading based on **parameter count** and **type annotations**.

```xell
fn add(a: int, b: int):
    give a + b
;

fn add(a: float, b: float):
    give a + b
;

fn add(a: string, b: string):
    give a + b
;
```

- When calling `add(x, y)`, the interpreter selects the best match:
  1. **Exact type match** (annotations match the argument types).
  2. **Arity match** (same number of parameters) if no type‑annotated match is found.
- A function with **no type annotations** is considered a “dynamic” overload – it will match any argument types, but only if no typed overload matches.

**Rules:**
- You cannot have both a dynamic and a typed overload with the **same arity** – this would be ambiguous and is disallowed.
- Overload resolution happens at **call time**.

---

## `async` Functions

Mark a function as asynchronous with `async`:

```xell
async fn fetch_data():
    let data = await some_generator()
    give data
;
```

- Inside `async fn`, you can use `await` to wait for generator completion.
- Async functions themselves are not generators – they are normal functions that happen to use `await`.
- Use `await` to drain a generator and get its final value.

---

## `yield` in Functions (Generators)

A function that contains `yield` becomes a **generator**. See [Generators](generators.md) for details.

---

## Function Calls

Call a function by its name followed by arguments in parentheses:

```xell
let result = add(10, 20)
```

**Paren‑less call shorthand:**  
If a function takes a single primary expression as its argument, you can omit parentheses:

```xell
print "Hello"    # same as print("Hello")
```

This works with any function when the argument is a primary expression (literal, identifier, etc.) but is most commonly used with built‑ins.

**Method calls:** `obj->method(args)` – see [Classes](../oop/oop-classes.md).

---

## Built‑in Functions

Xell provides a rich set of built‑ins – see [Built‑ins](builtins.md) and the [Standard Library](../stdlib/) sections.

---

## Common Patterns

### Simple predicate:

```xell
fn is_positive(n): give n > 0 ;
```

### Recursion:

```xell
fn factorial(n):
    if n <= 1 : give 1
    give n * factorial(n - 1)
;
```

### Higher‑order functions:

```xell
fn apply_twice(f, x): give f(f(x)) ;

let double = (x) => x * 2
print(apply_twice(double, 5))   # 20
```

---

## Scope and Variable Lookup

Functions follow **lexical scoping**:
1. Current function’s scope.
2. Enclosing functions (closures).
3. Global scope.

```xell
let global = 10

fn outer():
    let outer_var = 20
    fn inner():
        print(global)       # 10
        print(outer_var)    # 20
    ;
;
```

---

## Next Steps

Now that you know functions, explore **[Lambdas](lambdas.md)** – anonymous function syntax for short, inline operations.

---

**Previous:** [Control Flow](control-flow.md)  
**Next:** [Lambdas](lambdas.md)