## ⬅️ What's Previous?

Move back to the **[Operators](./Operators.md)** to review the foundational concepts.

---

# Functions

Functions allow you to group reusable blocks of code. In Xell, functions are first-class citizens and can be defined flexibly.

## 🛠️ Defining Functions

Functions are defined using the `fn` keyword. A function definition consists of the `fn` keyword, a name, a parameter list, and a body enclosed by `:` and `;`.

### Basic Syntax
```xell
fn greet(name, greeting) :
    print "{greeting}, {name}!"
;
```

### Returning Values
Use the `give` keyword to return a value from a function.

```xell
fn add(a, b) :
    give a + b
;

result = add(5, 10) # result is 15
```

If `give` is used without a value, or if the function reaches the end of its block without a `give` statement, it returns `none`.

---

## 📥 Parameters

Xell provides several ways to handle function arguments.

### Standard Parameters
The most common way to define parameters is by listing their names:
```xell
fn calculate(width, height) :
    give width * height
;
```

### Typed Parameters
For better clarity or specific requirements, you can annotate parameters with types using either the `Type(name)` or `name: Type` syntax:

```xell
# Style 1: Type(name)
fn set_age(number(age)) :
    print "Age set to {age}"
;

# Style 2: name: Type
fn set_name(name: string) :
    print "Name set to {name}"
;
```

### Variadic Parameters
If you don't know how many arguments will be passed, use the ellipsis `...` to define a variadic parameter. This collects all remaining arguments into a list.

```xell
fn sum_all(...numbers) :
    total = 0
    for n in numbers :
        total = total + n
    ;
    give total
;

print sum_all(1, 2, 3, 4) # Output: 10
```

---

## 📞 Calling Functions

Xell supports two calling styles to cater to both traditional programmers and shell users.

### 1. Standard Call (Parentheses)
This is the familiar style used in most languages.
```xell
greet("Alice", "Hello")
```

### 2. Shell-style Call (Paren-less)
If the function name is a bare identifier followed by arguments on the same line, you can omit the parentheses.
```xell
greet "Alice" "Hello"
```

---

## ⚡ Async Functions

For operations that might take time (like network requests), Xell supports asynchronous functions using the `async` keyword.

```xell
async fn fetch_data(url) :
    # Async logic here...
    give "Data from {url}"
;
```
*Note: Async functions are typically used in conjunction with the `await` keyword (covered in the Advanced sections).*

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Lambdas](./Lambdas.md)** guide to further your understanding of Xell.
