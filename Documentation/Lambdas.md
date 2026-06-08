## ⬅️ What's Previous?

Move back to the **[Functions](./Functions.md)** to review the foundational concepts.

---

# Lambdas

Lambdas are anonymous functions—functions that are defined without a name. They are incredibly useful for short-lived tasks, such as passing a function as an argument to another function (e.g., for filtering or mapping a list).

## ⚡ Basic Syntax

Lambdas use the **Fat Arrow** `=>` to separate the parameter list from the function body.

### Single Parameter
If a lambda has only one parameter, you can omit the parentheses.

```xell
# Single expression body
square = x => x * x

print square(5) # Output: 25
```

### Multiple Parameters
For multiple parameters, wrap them in parentheses.

```xell
# Single expression body
add = (a, b) => a + b

print add(10, 20) # Output: 30
```

---

## 📦 Lambda Bodies

A lambda can have either a **single expression** as its body or a **full block** of statements.

### Expression Body
If the body is a single expression, it is automatically returned. No `give` keyword is needed.

```xell
is_even = n => n % 2 == 0
```

### Block Body
If you need multiple statements, use a block (colon `:` and semicolon `;`). In a block body, you must use the `give` keyword to return a value.

```xell
complex_task = (x, y) => :
    temp = x + y
    print "Calculating... {temp}"
    give temp * 2
;
```

---

## 🔒 Closures

Lambdas in Xell are **closures**. This means they can "capture" and remember variables from the scope where they were created, even after that scope has finished executing.

**Example:**
```xell
fn create_multiplier(factor) :
    # The lambda captures the 'factor' variable from the outer function
    return_lambda = x => x * factor
    give return_lambda
;

double = create_multiplier(2)
triple = create_multiplier(3)

print double(5) # Output: 10
print triple(5) # Output: 15
```

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Control Flow](./Control Flow.md)** guide to further your understanding of Xell.
