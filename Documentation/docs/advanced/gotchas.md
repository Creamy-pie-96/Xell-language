# Gotchas & Pitfalls

> **Previous:** [Built‑in Index](../reference/builtin-index.md)  
> **Next:** _(End of documentation)_

---

This page documents Xell's **subtle behaviours, footguns, and edge cases** – things that work differently from other languages, or that might surprise you if you're not careful. Read this to avoid common mistakes.

---

## Control Flow

### 1. `break VALUE` Only Works in Expression‑Mode Loops

**The Gotcha:** In a statement‑mode loop (`for : ... ;`, `while : ... ;`, `loop : ... ;`), you **cannot** use `break VALUE`. It throws a runtime error.

```xell
# ❌ Error – break VALUE in statement-mode loop
for i in [1,2,3]:
    if i == 2 : break "found"   # ❌ RuntimeError
;
```

**Why:** `break VALUE` only makes sense when the loop's result is being captured. Use expression‑mode loops for that.

**Fix:** Use expression‑mode syntax:

```xell
let result = for i in [1,2,3]:
    if i == 2 : break "found"
;
```

---

### 2. `@safe_loop` Checks Are Static (Syntactic), Not Control‑Flow Reachability

**The Gotcha:** `@safe_loop` checks that the loop body contains a `break` **somewhere**, but it cannot detect if that `break` is unreachable (e.g., after a `return`). It also ignores `break`s inside nested loops.

```xell
@safe_loop
loop :
    fn inner(): break ;   # ❌ break inside nested function – ignored!
;
# This will throw a runtime error because no break was found at the loop's level.
```

**Fix:** Ensure there is a `break` at the **same nesting level** as the loop, and that it is syntactically present (the check is shallow).

---

### 3. `continue` Skips to the Next Iteration, But Does Not Re‑evaluate the Condition

**The Gotcha:** In a `while` loop, `continue` jumps to the next iteration **without** re‑evaluating the condition. This can lead to infinite loops if you're not careful.

```xell
let x = 5
while x > 0:
    if x % 2 == 0 : continue   # ❌ infinite loop – never decrements x
    x = x - 1
;
```

**Fix:** Ensure `continue` is not bypassing the condition update.

---

## Pattern Matching (`incase`)

### 4. `belong` Uses Exact Name Matching, Not Inheritance

**The Gotcha:** In `incase`, `belong TypeName` checks **exact type name** matching. It does **not** consider inheritance.

```xell
class Animal ;
class Dog inherits Animal ;

let d = Dog()

incase d :
    belong Animal : print("animal")   # ❌ does NOT match
    belong Dog : print("dog")         # ✅ matches
;
```

**Why:** `belong` uses the `matchesType()` function, which does a string comparison of the type name. It does not follow the inheritance chain.

**Fix:** Use the `is` operator for polymorphic checks:

```xell
if d is Animal : print("animal")   # ✅ works
```

---

### 5. `bind` Always Matches and Binds Before the Guard Is Evaluated

**The Gotcha:** In an `incase` clause, `bind name` always matches, and the guard is evaluated **after** the binding. This means the guard can reference the bound variable.

```xell
incase x :
    bind n if n > 0 : print("positive")   # ✅ n is bound before guard
    bind n : print("zero or negative")    # catches the rest
;
```

**Why:** This is intentional – `bind` is designed to capture the subject for use in the guard and the body.

**Be careful:** The binding is scoped **only** to that clause. It does **not** leak to other clauses or to the outer scope.

---

## Variables and Binding

### 6. `immutable` Binds the Variable, Not the Value

**The Gotcha:** `immutable` prevents **re‑assignment** of the variable, but it does **not** prevent mutation of the object it refers to.

```xell
immutable list = [1, 2, 3]
push(list, 4)   # ✅ allowed – mutates the list
list = []       # ❌ error – cannot reassign immutable binding
```

**Why:** Immutability applies to the **binding**, not the value. This is the same as Python's `const` in JavaScript.

**Fix:** If you need a truly immutable value, use `frozen_set` or `@immutable` classes with the `~` prefix.

---

### 7. `let` Variables Are Block‑Scoped, But Not Hoisted

**The Gotcha:** Variables defined with `let` are block‑scoped and **not** hoisted. They exist only from the point of definition to the end of the block.

```xell
print(x)   # ❌ error – x is not defined
let x = 10
```

**Why:** This is the same as Python and modern JavaScript. Variables are not hoisted.

**Fix:** Define variables before using them.

---

## Functions

### 8. Functions Capture Variables by Reference, Not by Value

**The Gotcha:** Closures capture variables **by reference**, not by value. If a variable changes after the closure is created, the closure sees the new value.

```xell
let count = 0
let inc = () => count += 1
count = 10
print(inc())   # 11 (not 1)
```

**Why:** Xell uses lexical scoping with mutable closures. Variables are captured by reference.

**Fix:** If you need to capture the current value, create a new variable or use a function that takes the value as an argument.

---

### 9. Overloading with Dynamic Functions Blocks Type‑Annotated Overloads

**The Gotcha:** If you have a function without type annotations (dynamic) at a given arity, you **cannot** add a type‑annotated overload with the same arity.

```xell
fn add(a, b): give a + b ;   # dynamic

fn add(a: int, b: int): give a + b ;   # ❌ error – can't add typed overload
```

**Why:** The dynamic function would match any type, making the typed overload unreachable and ambiguous.

**Fix:** Either make all overloads typed, or keep the dynamic one and add overloads with different arities.

---

## Object‑Oriented Programming

### 10. `parent` Is Static, Not Dynamic

**The Gotcha:** `parent` refers to the **immediate parent of the class that defines the currently executing method**, not the runtime type of `self`.

```xell
class A :
    fn greet(self): print("A") ;

class B inherits A :
    fn greet(self):
        print("B")
        parent->greet(self)   # always calls A::greet()
    ;

class C inherits B :   # no own greet()
    ;

let c = C()
c->greet()   # B, A
```

**Why:** `parent` is resolved at the method call site based on the **static** class hierarchy of the defining class.

**Fix:** Use the `is` operator to check runtime types if you need dynamic dispatch.

---

### 11. `__init__` Is Not Automatically Called on Parents

**The Gotcha:** If you override `__init__` in a subclass, the parent's `__init__` is **not** called automatically. You must call it explicitly.

```xell
class Parent :
    name = ""
    fn __init__(self, name): self->name = name ;
;

class Child inherits Parent :
    age = 0
    fn __init__(self, name, age):
        # ❌ missing parent->__init__ – name stays ""
        self->age = age
    ;
;

let c = Child("Alice", 30)
print(c->name)   # "" (never set)
```

**Fix:** Always call `parent->__init__(self, ...)` in your subclass constructors.

---

### 12. Mixins Cannot Have `__init__`

**The Gotcha:** Mixins cannot have `__init__` methods. The parser rejects them.

```xell
mixin Logger :
    fn __init__(self):   # ❌ error – mixins cannot have __init__
        print("Logger init")
    ;
;
```

**Why:** Mixins are not meant to be instantiated on their own. They are composed into classes.

**Fix:** Provide an `init()` method that the class's `__init__` calls explicitly.

---

### 13. Structs Do Not Support Inheritance

**The Gotcha:** Structs are value types and do **not** support inheritance. You cannot use `inherits` with a struct.

```xell
struct Point : x = 0 y = 0 ;
struct ColoredPoint inherits Point : color = "red" ;   # ❌ error – structs don't inherit
```

**Fix:** Use `class` if you need inheritance.

---

### 14. Interfaces, Abstract Classes, and Mixins Cannot Be Instantiated

**The Gotcha:** You cannot call `new` or `ClassName()` on an interface, abstract class, or mixin.

```xell
interface Drawable ;
let d = Drawable()   # ❌ TypeError – cannot instantiate interface

abstract Shape ;
let s = Shape()      # ❌ TypeError – cannot instantiate abstract class

mixin Loggable ;
let l = Loggable()   # ❌ TypeError – cannot instantiate mixin
```

**Why:** These are not complete types. They are contracts or partial implementations.

---

### 15. `@dataclass` Is Currently a No‑Op

**The Gotcha:** `@dataclass` sets a flag on the class but does **not** generate `__init__`, `__eq__`, `__repr__`, or any other methods.

```xell
@dataclass
class Point :
    x = 0
    y = 0
;
# No __init__ is generated – you still need to define it manually.
```

**Why:** The `@dataclass` decorator is a placeholder for future code generation. Currently, it only marks the class as a data class for tooling (e.g., the IDE).

**Fix:** Define `__init__` and other methods manually, or use the default struct‑style construction.

---

### 16. `@immutable` Freezes Instances After `__init__`

**The Gotcha:** `@immutable` freezes the instance **after** `__init__` completes. If you try to modify fields inside `__init__`, they are writable until `__init__` returns.

```xell
@immutable
class Config :
    url = ""
    fn __init__(self, url):
        self->url = url   # ✅ allowed inside __init__
    ;
;

let c = Config("https://api.example.com")
c->url = "new"   # ❌ ImmutabilityError – frozen after __init__
```

**Why:** The interpreter sets `frozen = true` after `__init__` returns. This allows construction logic to run.

---

### 17. `~` (Frozen Instance) Works on Any Class, Not Just `@immutable`

**The Gotcha:** You can create a frozen instance of **any** class using the `~` prefix, even if the class is not marked `@immutable`.

```xell
class Mutable :
    value = 0
;

let m = ~Mutable({ value: 10 })   # frozen instance
m->value = 20   # ❌ ImmutabilityError
```

**Why:** `~` is a per‑instance freeze, independent of the class's `@immutable` flag.

**Fix:** Use `~` when you need a one‑off immutable instance.

---

### 18. `protected` Members Are Not Accessible from Outside the Hierarchy

**The Gotcha:** `protected` members are accessible only from the class itself and its subclasses. They are **not** accessible from outside, even if the object is the same type but in a different context.

```xell
class Parent :
    protected:
        secret = 42
;

let p = Parent()
p->secret   # ❌ AccessError – protected
```

---

### 19. Private Members Are Not Inherited

**The Gotcha:** `private` members are **not** accessible in subclasses, and they are **not** inherited.

```xell
class Parent :
    private:
        secret = 42
;

class Child inherits Parent :
    fn peek(self):
        give self->secret   # ❌ AccessError – private
    ;
;
```

---

## Modules

### 20. `bring * from "file"` Imports Everything, Not Just Exports

**The Gotcha:** `bring * from "file"` imports **all** top‑level names from the file's global environment, including non‑exported names.

```xell
# file.xel
let helper = 42
export fn process(x): give x * 2 ;

# main.xel
bring * from "file.xel"
print(helper)   # 42 – imported even though not exported!
```

**Why:** `bring * from "file"` is a file‑level import, not a module‑level import. It imports the entire file's global scope.

**Fix:** Use `bring * of Module` for module‑level imports that only bring exported names.

---

### 21. Modules Are Cached Globally

**The Gotcha:** Once a module is brought, it is cached globally. Changes to the source file after the first import do **not** affect subsequent imports.

```xell
bring Math from "math.xel"   # loads and caches
# Edit math.xel
bring Math from "math.xel"   # still uses cached version
```

**Why:** Modules are loaded once per session for performance and consistency.

**Fix:** Restart the interpreter to reload modules, or use a development mode that invalidates the cache.

---

### 22. `requires` Does Not Bring Items into Global Scope

**The Gotcha:** `requires` inside a module brings dependencies **only** into the module's scope, not into the global scope.

```xell
module App :
    requires JSON
;

# JSON is NOT available here
print(JSON)   # ❌ error – JSON not defined
```

**Fix:** Use `bring` in the global scope, or access via the module that required it.

---

## Debugging

### 23. `@breakpoint pause` Without IPC Blocks on stdin

**The Gotcha:** If you use `@breakpoint pause` without a debug IPC session, it blocks waiting for **Enter** on stdin. This can hang scripts in headless environments.

```xell
@breakpoint pause   # ❌ blocks waiting for stdin
```

**Fix:** Use timed pauses (`@breakpoint pause 5`) or ensure a debug IPC session is active.

---

### 24. `@debug sample N` Traces First and Last N/2, Not Middle

**The Gotcha:** `@debug sample N` traces the first `N/2` and last `N/2` iterations, not a random sample or the middle.

```xell
@debug sample 10
for i in range(1000):
    # Only ~10 iterations are traced (first 5 and last 5)
;
```

**Why:** This is intentional – it gives a representative view of the whole execution without flooding the log.

**Fix:** If you need a different sampling strategy, use manual breakpoints or watches.

---

### 25. `@track` Unknown Categories Are Silently Ignored

**The Gotcha:** If you specify an unknown category in `@track`, it is ignored without warning.

```xell
@track catagory   # ❌ typo – "catagory" is silently ignored
```

**Why:** The parser does not validate category names against a fixed list; unrecognised categories are ignored.

**Fix:** Check the spelling against the supported list: `loop`, `for`, `while`, `conditions`, `scope`, `imports`, `returns`, `calls`, `mutations`, `types`, `perf`, `recursion`.

---

## Shell Integration

### 26. `$cmd` in Statement Mode Prints Directly; Expression Mode Returns a List

**The Gotcha:** `$cmd` behaves differently depending on context:

- In **statement mode**: prints output directly to stdout.
- In **expression mode**: returns a list of output lines.

```xell
$ls -la   # prints output directly

let lines = $ls   # returns list of lines
```

**Why:** This is intentional – statement mode is for side effects, expression mode is for capturing data.

**Fix:** Be aware of the context when using `$cmd`.

---

### 27. `set_e()` Only Affects Commands Run After It Is Enabled

**The Gotcha:** `set_e()` does **not** affect commands that are already running or that were run before it was enabled.

```xell
$ls /nonexistent   # runs, prints error, continues
set_e()
$ls /nonexistent   # ❌ throws CommandFailedError
```

---

### 28. External Command Fallback Only Works for Non‑Keywords

**The Gotcha:** When you call an identifier that is not a built‑in or defined variable, Xell checks if it is an executable on `PATH`. But **keywords** are never treated as commands.

```xell
fn x:... ;   # ❌ if "fn" is not a command, this is a syntax error
```

**Why:** Keywords are reserved and cannot be used as command names.

---

## F‑Strings

### 29. F‑Strings Require the `f` Prefix

**The Gotcha:** Without the `f` prefix, `{...}` is treated as literal text.

```xell
let name = "Xell"
print("{name}")   # ❌ prints "{name}", not "Xell"
```

**Fix:** Use `f"{name}"`.

---

### 30. F‑Strings and Raw Strings (`r"..."`) Are Mutually Exclusive

**The Gotcha:** You cannot combine `f` and `r` prefixes. `fr"..."` is not valid.

```xell
let s = fr"hello {name}"   # ❌ syntax error
```

**Fix:** Use `f"..."` for interpolation. Raw strings do not support interpolation.

---

### 31. Backslashes Cannot Be Used Inside `{}` in F‑Strings

**The Gotcha:** The expression inside `{}` cannot contain backslashes (escape sequences).

```xell
print(f"{1\n2}")   # ❌ syntax error
```

**Fix:** Compute the value beforehand or use string concatenation.

---

## Collections and Hash Tables

### 32. Lists Are Mutable; Tuples Are Immutable

**The Gotcha:** Lists are mutable, but tuples are immutable. This affects what you can use as map keys and set elements.

```xell
let t = (1, 2, 3)
t[1] = 99   # ❌ ImmutabilityError – tuple is immutable
```

---

### 33. `map` Keys Can Be Any Hashable Type, Not Just Strings

**The Gotcha:** Unlike many languages, Xell's `map` keys can be **any hashable value** – integers, floats, tuples, frozen sets, etc.

```xell
let m = {}
set(m, (1, 2), "tuple key")
set(m, 42, "integer key")
set(m, <1,2>, "frozen set key")
```

**Why:** This is by design – maps support arbitrary hashable keys.

**Be careful:** Using mutable types as keys will cause `HashError`.

---

### 34. `frozen_set` Is Not a Syntactic Sugar for `set`

**The Gotcha:** `frozen_set` is a **different type** from `set`. It is immutable and hashable, but `set` is mutable and non‑hashable.

```xell
let s = set([1,2,3])        # mutable set
let fs = frozen_set([1,2,3]) # immutable set
let m = {}
set(m, s, "value")   # ❌ error – set is not hashable
set(m, fs, "value")  # ✅ works – frozen_set is hashable
```

---

## Error Handling

### 35. Bare `throw` Is Not Implemented (Re‑Throw)

**The Gotcha:** `throw` with no value is **not** implemented. It throws a `RuntimeError` with a message about being unsupported.

```xell
try :
    throw "error"
catch e :
    throw   # ❌ RuntimeError – bare throw not supported
;
```

**Why:** Re‑throw semantics are planned for a future version.

**Fix:** Re‑throw with a new error value.

---

### 36. `throw` with a Map Uses the `"type"` Key for Dispatch

**The Gotcha:** When you `throw` a map, the `"type"` key determines which C++ error class is thrown.

```xell
throw { "message": "Invalid input", "type": "ValueError" }   # throws ValueError
```

**Why:** This allows structured errors that map to the error hierarchy.

**Fix:** Use the correct `"type"` value (`"TypeError"`, `"IndexError"`, etc.), or use an instance of the error class.

---

### 37. `catch` Matches Error Types by Name and Also `"Error"`

**The Gotcha:** `catch e is TypeError` matches errors whose `type` field is `"TypeError"`. It also matches `"Error"` (the base class).

```xell
catch e is Error :   # ✅ matches any error
    print("Any error caught")
;
```

**Why:** The parent `"Error"` type is checked for every error, allowing base‑class catches.

---

## Garbage Collection

### 38. The GC Does Not Run Immediately After a Cycle Forms

**The Gotcha:** The GC runs only when the allocation threshold (700 container allocations) is reached. A cycle may persist for a while before collection.

```xell
let a = []
push(a, a)   # cycle
# a may not be collected immediately
```

**Why:** The GC runs periodically to avoid overhead.

**Fix:** There is currently no user‑facing `gc_collect()` builtin. Design your code to avoid cycles where possible.

---

### 39. Non‑Container Types Are Not Tracked

**The Gotcha:** Types like `int`, `float`, `string`, and `bytes` cannot form cycles and are **not** tracked by the GC.

```xell
# These are never tracked by the GC
let x = 42
let s = "hello"
let b = b"data"
```

**Why:** They cannot reference other objects, so they cannot participate in cycles.

---

## Syntax and Lexer

### 40. Newlines Are Significant (But Not Indentation)

**The Gotcha:** Xell uses `:` and `;` for block delimiters, not indentation. However, newlines still separate statements.

```xell
let x = 10   # statement ends at newline
let y = 20   # new line means new statement
```

**Why:** This is a design choice to avoid indentation sensitivity while keeping the code readable.

**Fix:** If you need to split a statement across lines, use `\` at the end of the line.

---

### 41. Comments Are Not Recursive

**The Gotcha:** Single‑line comments (`#`) and multi‑line comments (`--> ... <--`) are not recursively parsed. A multi‑line comment cannot contain another multi‑line comment.

```xell
--> outer
    --> inner   # ❌ the outer comment ends at the first <-- ?
    <--         # ❌ this is just plain text
<--
```

**Why:** Multi‑line comments are delimited by `-->` and `<--`; nesting is not supported.

---

### 42. `E` Is Euler's Number, Not Scientific Notation

**The Gotcha:** Floating‑point literals use **uppercase `E`** for scientific notation. Lowercase `e` is **not** allowed and is instead treated as Euler's number.

```xell
let a = 1.2E10   # ✅ scientific notation
let b = 1.2e10   # ❌ error – 'e' is not recognized here
```

**Why:** Lowercase `e` is reserved for Euler's number.

---

### 43. `_` Is a Valid Identifier (Discard)

**The Gotcha:** `_` is a valid identifier and is used as a discard variable in destructuring and `let ... be`.

```xell
let [_, value] = [1, 2]   # _ discards the first value
```

**Be careful:** `_` can be reassigned and used like a normal variable, but it is conventionally used for discarding.

---

## Performance

### 44. Recursion Depth Is Limited to 512

**The Gotcha:** The maximum call depth is **512**. Exceeding this throws `RecursionError`.

```xell
fn infinite(): infinite() ;
infinite()   # ❌ RecursionError
```

**Why:** To prevent stack overflow.

**Fix:** Use iterative solutions for deep recursion.

---

### 45. Large Cycles Can Cause Collection Pauses

**The Gotcha:** Collecting a very large cyclic structure may cause a noticeable pause.

**Why:** The GC visits all tracked objects and their children.

**Fix:** Avoid creating huge cyclic structures in performance‑critical code.

---

## See Also

- [Error Handling](../core/error-handling.md) – exceptions and error types
- [Pattern Matching](../core/pattern-matching.md) – `incase` details
- [Debugging](../tooling/debugging.md) – `@debug`, `@breakpoint`, `@watch`
- [Shell Integration](../core/shell.md) – `$cmd`, `set_e()`, `||`, `&&`

---

**Previous:** [Built‑in Index](../reference/builtin-index.md)  
**Next:** _(End of documentation)_
