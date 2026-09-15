# Keywords – Complete Reference

> **Previous:** [Garbage Collection](../advanced/garbage-collection.md)  
> **Next:** [Error Types](error-types.md)

---

This page lists every reserved keyword in Xell, organised by category. Keywords are reserved – they cannot be used as identifiers (variable names, function names, etc.).

---

## Control Flow

| Keyword | Description | Example |
|---------|-------------|---------|
| `if` | Begins a conditional branch | `if x > 0 : print("positive") ;` |
| `elif` | Alternative conditional branch | `elif x == 0 : print("zero") ;` |
| `else` | Default branch (statement) or ternary alternative (expression) | `else : print("negative") ;` |
| `for` | Iterates over a collection | `for i in [1,2,3] : print(i) ;` |
| `while` | Condition‑controlled loop | `while x > 0 : x = x - 1 ;` |
| `loop` | Infinite loop | `loop : break ;` |
| `do` | Do‑while loop (post‑condition) | `do : x = x - 1 ; while x > 0` |
| `break` | Exits a loop | `break` or `break 42` (expression mode) |
| `continue` | Skips to the next iteration | `continue` |
| `in` | Membership / iteration source | `for i in range(10)` |
| `is` | Instance‑of check or identity | `obj is ClassName` |
| `not` | Logical negation | `not x > 0` |
| `and` | Logical AND | `x > 0 and x < 10` |
| `or` | Logical OR | `x < 0 or x > 10` |

---

## Functions and Returns

| Keyword | Description | Example |
|---------|-------------|---------|
| `fn` | Defines a function | `fn square(x): give x * x ;` |
| `give` | Returns a value from a function | `give x * x` |
| `async` | Marks a function as asynchronous | `async fn fetch(): give await data ;` |
| `await` | Waits for a generator to finish | `let result = await generator()` |
| `yield` | Produces a value from a generator | `yield i` |

---

## Variables and Binding

| Keyword | Description | Example |
|---------|-------------|---------|
| `let` | Creates a mutable variable | `let x = 10` |
| `be` | Binds a resource in a `let … be` statement | `let f be file = File("data.txt") : ... ;` |
| `immutable` | Creates an immutable binding | `immutable PI = 3.14159` |

---

## Error Handling

| Keyword | Description | Example |
|---------|-------------|---------|
| `try` | Begins a block that may throw | `try : ... ;` |
| `catch` | Catches an error | `catch e : print(e->message) ;` |
| `finally` | Always executes after `try`/`catch` | `finally : close(file) ;` |
| `throw` | Throws an error | `throw "Something went wrong"` |
| `is` (in `catch`) | Filters error types | `catch e is TypeError : ... ;` |

---

## Pattern Matching

| Keyword | Description | Example |
|---------|-------------|---------|
| `incase` | Begins a pattern‑matching block | `incase x : is 0 : "zero" ... ;` |
| `belong` | Type check in `incase` | `belong int : print("integer")` |
| `bind` | Captures the subject in `incase` | `bind n if n > 0 : print(n)` |

---

## Object‑Oriented Programming

| Keyword | Description | Example |
|---------|-------------|---------|
| `class` | Defines a class | `class Animal : ... ;` |
| `struct` | Defines a struct (value type) | `struct Point : x = 0 y = 0 ;` |
| `abstract` | Marks a class as abstract | `abstract Shape : ... ;` |
| `mixin` | Defines a mixin | `mixin Loggable : ... ;` |
| `interface` | Defines an interface | `interface Drawable : fn draw(self) ; ;` |
| `inherits` | Declares parent classes | `class Dog inherits Animal` |
| `implements` | Declares implemented interfaces | `class Circle implements Drawable` |
| `with` | Composes mixins into a class | `class User with Loggable` |
| `public` | Access control – accessible everywhere | `public: let name = "" ;` |
| `protected` | Access control – class and subclasses | `protected: let id = 0 ;` |
| `private` | Access control – class only | `private: let secret = "" ;` |
| `static` | Declares a static member | `static count = 0` |
| `get` | Defines a property getter | `get area(self): give w * h ;` |
| `set` | Defines a property setter | `set area(self, val): ... ;` |
| `new` | (Future) – creates an instance | – |

---

## Modules and Imports

| Keyword | Description | Example |
|---------|-------------|---------|
| `module` | Defines a module | `module Math : ... ;` |
| `bring` | Imports from a module or file | `bring square of Math` |
| `from` | Specifies a directory for import | `from "lib" bring Math` |
| `as` | Aliases an import | `bring Math as M` |
| `export` | Exports a declaration from a module | `export fn square(x): ... ;` |
| `requires` | Declares a module dependency | `requires JSON` |
| `of` | Module path separator in `bring` | `bring parse of JSON` |

---

## Enums

| Keyword | Description | Example |
|---------|-------------|---------|
| `enum` | Defines an enumeration | `enum Color: Red, Green, Blue ;` |

---

## Literals

| Keyword | Description | Example |
|---------|-------------|---------|
| `true` | Boolean true | `true` |
| `false` | Boolean false | `false` |
| `none` | Null value | `none` |

---

## RAII / Context Managers

| Keyword | Description | Example |
|---------|-------------|---------|
| `let` | Acquires a resource | `let f be file = File("data.txt") : ... ;` |
| `be` | Binds the resource's `__enter__` result | `let f be file = ...` |

---

## Type Comparison Keywords

| Keyword | Description | Example |
|---------|-------------|---------|
| `is` | Instance‑of check (subtype‑aware) | `obj is Animal` |
| `eq` | Alias for `==` (comparison) | `x eq y` |
| `ne` | Alias for `!=` (comparison) | `x ne y` |
| `gt` | Alias for `>` (comparison) | `x gt y` |
| `lt` | Alias for `<` (comparison) | `x lt y` |
| `ge` | Alias for `>=` (comparison) | `x ge y` |
| `le` | Alias for `<=` (comparison) | `x le y` |

---

## Decorator Keywords

| Keyword | Description | Example |
|---------|-------------|---------|
| `@` | Decorator prefix (not a keyword, but token) | `@debug fn foo(): ... ;` |

---

## Comparison Keywords

| Keyword | Description | Example |
|---------|-------------|---------|
| `is` | Instance‑of / identity check | `obj is ClassName` |
| `not in` | Negated membership | `x not in list` |
| `is not` | Negated instance check | `obj is not Animal` |

---

## Reserved for Future Use

| Keyword | Status | Notes |
|---------|--------|-------|
| `new` | Reserved | Future constructor syntax |
| `use` | Reserved | Alternative to `bring` |
| `skip` | Reserved | Future `continue` alternative |
| `stop` | Reserved | Future `break` alternative |

---

## Keywords vs. Identifiers

All keywords are **reserved** – they cannot be used as variable, function, or module names.

```xell
let if = 10   # ❌ syntax error – 'if' is a keyword
```

**Exception:** Keywords can appear inside strings and comments without restriction.

---

## See Also

- [Syntax Basics](../core/syntax-basics.md) – lexical structure and identifiers
- [Error Types](error-types.md) – error hierarchy reference
- [Built‑in Index](builtin-index.md) – all built‑in functions

---

**Previous:** [Garbage Collection](../advanced/garbage-collection.md)  
**Next:** [Error Types](error-types.md)