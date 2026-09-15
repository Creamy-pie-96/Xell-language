# Lambdas

> **Previous:** [Functions](functions.md)  
> **Next:** [Error Handling](error-handling.md)

---

Lambdas are **anonymous functions** – functions without a name that can be defined inline. They are useful for short, single‑use operations like callbacks, predicates, and higher‑order function arguments.

---

## Lambda Syntax

### Single‑expression lambda

The most common form:

```xell
let double = (x) => x * 2
```

- Parameters are in parentheses (single parameter can omit parentheses).
- The body is a single expression – its value is automatically returned.
- No explicit `give` needed.

**Single parameter (parentheses optional):**

```xell
let square = x => x * x
```

**Multiple parameters:**

```xell
let add = (a, b) => a + b
```

**Zero parameters:**

```xell
let greet = () => "Hello!"
```

---

### Multi‑line lambda

For more complex logic, use a block body:

```xell
let process = (x) => :
    let y = x * 2
    if y > 10 :
        give y
    else :
        give 0
    ;
;
```

- The block is delimited by `:` … `;`.
- Use `give` to return a value (like a normal function).
- The block can contain multiple statements.

---

## Lambda as a Closure

Like normal functions, lambdas capture variables from their enclosing scope:

```xell
let factor = 3
let triple = x => x * factor
print(triple(10))   # 30
```

The lambda retains access to `factor` even after the outer scope ends.

**Mutable capture:**

```xell
let count = 0
let increment = () => count += 1
print(increment())   # 1
print(increment())   # 2
```

---

## Lambdas in Higher‑Order Functions

Lambdas shine when passed as arguments:

### `map`

```xell
let numbers = [1, 2, 3, 4, 5]
let doubled = map(numbers, x => x * 2)
print(doubled)   # [2, 4, 6, 8, 10]
```

### `filter`

```xell
let evens = filter(numbers, x => x % 2 == 0)
print(evens)   # [2, 4]
```

### `reduce`

```xell
let sum = reduce(numbers, (acc, x) => acc + x, 0)
print(sum)   # 15
```

### `any` / `all`

```xell
let has_negative = any(numbers, x => x < 0)   # false
let all_positive = all(numbers, x => x > 0)   # true
```

---

## Lambda vs. Function

| Feature | `fn` (named) | Lambda |
|---------|--------------|--------|
| Name | Required | Anonymous (can be assigned) |
| Body | Block (`:` … `;`) | Single expr or block (`:` … `;`) |
| Recursion | Yes (by name) | No (unless assigned to a variable) |
| Overloads | Yes | No |
| Use case | Reusable, complex logic | Short, inline callbacks |

---

## Lambda Type

Lambdas are first‑class values with type `function`. They can be stored, passed, and returned:

```xell
let operations = [
    x => x + 1,
    x => x * 2,
    x => x ** 2
]

for op in operations:
    print(op(5))   # 6, 10, 25
;
```

---

## Common Lambda Patterns

### Sorting with custom key

```xell
let people = [
    { "name": "Alice", "age": 30 },
    { "name": "Bob", "age": 25 },
    { "name": "Charlie", "age": 35 }
]

let sorted = sort(people, (a, b) => a["age"] < b["age"])
```

### Event handlers

```xell
let button = create_button()
button->on_click(() => print("Button clicked!"))
```

### Lazy evaluation

```xell
let maybe_value = condition ? (() => compute_expensive()) : (() => "default")
let result = maybe_value()
```

---

## Lambda Scope

Lambdas can access variables from their **definition scope** but cannot access the **caller's scope**.

```xell
let x = 10

fn outer():
    let y = 20
    let lambda = () => x + y   # captures x (global) and y (outer)
    give lambda
;

let l = outer()
print(l())   # 30
```

---

## Next Steps

Now that you understand lambdas, explore **[Error Handling](error-handling.md)** to learn how to handle exceptions.

---

**Previous:** [Functions](functions.md)  
**Next:** [Error Handling](error-handling.md)