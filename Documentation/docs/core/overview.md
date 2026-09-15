# Xell Overview

> **Previous:** [Getting Started](../getting-started.md)  
> **Next:** [Syntax Basics](syntax-basics.md)

---

## Welcome to Xell

**Xell** is a modern, expressive scripting language that blends the clarity of Python, the power of functional programming, and the robustness of a systems‑level foundation. It's designed for developers who want to **write code that's a pleasure to read and maintain**, without sacrificing performance or safety.

Whether you're automating tasks, building web applications, or prototyping algorithms, Xell gives you the tools to express your ideas clearly and concisely.

---

## What Makes Xell Special?

### 1. Crystal‑Clear Syntax

Xell's syntax is deliberately English‑like and consistent. Every keyword and construct is chosen to be obvious even to newcomers. The language is designed so that **the code tells you what it does**.

```xell
# This reads like plain English
if score >= 90 : give "A"
elif score >= 80 : give "B"
else : give "F" ;
```

### 2. Safety by Default

- **Immutable bindings** (`immutable`) prevent accidental reassignment.
- **RAII** (`let … be`) guarantees resource cleanup.
- **Reference counting + cycle‑collecting GC** prevents memory leaks while keeping performance predictable.

### 3. Batteries Included

Xell ships with a **comprehensive standard library** – from file I/O and shell integration to HTTP, JSON, regex, threading, and Git integration. The REPL, linter, debugger, and IDE come built in.

### 4. Functional and Object‑Oriented

You can write in the style that best suits your problem:

- **Functional**: first‑class functions, lambdas, `map`/`filter`/`reduce`, generators, pattern matching.
- **Object‑oriented**: classes, inheritance, mixins, interfaces, properties, access control, and magic methods.

### 5. Seamless Shell Integration

Xell treats the shell as a first‑class citizen. Run commands with `$`, pipe them with `|>`, chain with `&&`/`||`, and control error handling with `set_e()`.

```xell
$git status && $echo "Repository clean"
```

### 6. Rich Tooling

- **REPL** – experiment interactively.
- **Linter** – catch errors before runtime.
- **Debugger** – set breakpoints, step through code, inspect variables.
- **IDE** – syntax highlighting, autocomplete, git integration, and more.

---

## A Quick Tour of the Language

### 1. Variables and Data Types

Xell has a rich set of built‑in types, all with intuitive literals.

```xell
# Primitives
let count = 42                     # integer
let pi = 3.14159                   # float
let z = 2 + 3i                     # complex number (2+3i)
let name = "Xell"                  # string
let ready = true                   # boolean
let nothing = none                 # null value
let data = b"\x48\x65\x6c\x6c\x6f" # byte string

# Collections
let list = [1, 2, 3]
let tuple = (1, "two", 3.0)
let set = {1, 2, 3}                 # mutable set
let frozen = <1, 2, 3>              # immutable frozen_set
let map = { "a": 1, "b": 2 }
```

### 2. Control Flow

Xell's control flow is familiar yet refined, with explicit block delimiters (`:` … `;`).

```xell
# Conditionals
if x > 0 : print("positive")
elif x == 0 : print("zero")
else : print("negative") ;

# Loops
for i in range(5): print(i) ;

while count > 0 : count = count - 1 ;

# Infinite loop
loop :
    print("forever")
    break ;
;

# Expression‑mode loops return a value
let squares = for i in [1,2,3,4,5]:
    give i * i ;
# squares = [1, 4, 9, 16, 25]
```

### 3. Functions and Lambdas

Functions are first‑class citizens with optional type annotations, default parameters, and variadic arguments.

```xell
# A simple function
fn add(a, b): give a + b ;

# With type annotations and default values
fn greet(name = "world"): print("Hello, " + name) ;

# Variadic parameters
fn sum_all(first, ...rest):
    let total = first
    for n in rest: total += n ;
    give total
;

# Lambdas
let double = (x) => x * 2
print(double(5))   # 10
```

### 4. Error Handling

Errors are exceptions with a rich hierarchy, structured error objects, and clean `try`/`catch`/`finally` semantics.

```xell
try :
    let result = risky_operation()
catch e is TypeError :
    print("Type error: " + e->message)
catch e :
    print("Other error: " + e->message)
finally :
    print("Cleanup") ;
```

### 5. Pattern Matching (`incase`)

`incase` combines value equality, type checking, and variable binding in a clean, expressive syntax.

```xell
incase value :
    is 0 : print("zero")
    belong int : print("integer")
    bind x if x > 10 : print("big: " + str(x))
    else : print("unknown") ;
```

### 6. Object‑Oriented Programming

Xell supports classes, structs, inheritance, mixins, interfaces, access control, properties, and magic methods.

```xell
class Animal :
    name = ""
    fn speak(self): print("?") ;

class Dog inherits Animal :
    fn speak(self): print("Woof!") ;

let d = Dog({ name: "Rex" })
d->speak()   # Woof!
```

### 7. Modules and Imports

Modules are first‑class objects. `bring` imports from files or module paths, and `export` controls visibility.

```xell
module math_lib :
    export fn square(x): give x * x ;
    export immutable VERSION = "1.0"
;

bring square, VERSION of math_lib
print(square(5))   # 25
```

### 8. Shell Integration

Run external commands, capture output, and build pipelines directly in your Xell code.

```xell
$ls -la               # execute and print output
let lines = $git log  # capture as list of lines
set_e()               # throw on command failure
```

### 9. Decorators

Decorators add metadata and behaviour to functions, classes, and loops. Built‑in decorators cover debugging, profiling, logging, and more.

```xell
@debug
fn factorial(n):
    if n <= 1: give 1
    give n * factorial(n - 1) ;

@profile
fn heavy_computation(): ... ;

@breakpoint("check") when x > 100
```

---

## What's Next?

Now that you have the big picture, dive into the details:

- **[Syntax Basics](syntax-basics.md)** – lexical structure, comments, literals.
- **[Data Types](data-types.md)** – all primitive and compound types.
- **[Variables & Binding](variables-binding.md)** – `let`, `immutable`, destructuring.

---

**Previous:** [Getting Started](../getting-started.md)  
**Next:** [Syntax Basics](syntax-basics.md)
