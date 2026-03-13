# Xell Language — Comprehensive Audit

> **Generated from a full source-code review** of the lexer, parser, interpreter, type system,
> builtins, analyzer, error hierarchy, REPL, hash library, OS layer, and module system.
> No existing documentation files were referenced — this audit reflects **only what the code does today**.

---

## Table of Contents

1. [Architecture Snapshot](#architecture-snapshot)
2. [Tier 1 — Necessary (Language Correctness & Safety)](#tier-1--necessary)
3. [Tier 2 — Important (Expected by Any Modern Scripting Language)](#tier-2--important)
4. [Tier 3 — Valuable (Productivity & Ecosystem)](#tier-3--valuable)
5. [Tier 4 — Nice to Have (Advanced / Aspirational)](#tier-4--nice-to-have)
6. [Subsystem Summaries](#subsystem-summaries)
7. [Full Gap Inventory](#full-gap-inventory)

---

## Architecture Snapshot

| Aspect              | Current State                                                                                                      |
| ------------------- | ------------------------------------------------------------------------------------------------------------------ |
| **Execution model** | Tree-walking interpreter (no bytecode)                                                                             |
| **Memory**          | Intrusive ref-counting (`atomic<uint32_t>`), no cycle collector                                                    |
| **Types**           | 18 value types (`XType` enum), all heap-allocated via `XData` control block                                        |
| **Scope**           | Lexical, block-scoped, linked-list environment chain                                                               |
| **Concurrency**     | OS-thread-per-generator; `async`/`await` is syntactic sugar over generators                                        |
| **Error handling**  | 22 C++ exception classes; caught errors exposed as plain `XMap` to user code                                       |
| **OOP**             | Classes, structs, interfaces, abstract classes, mixins, single inheritance, decorators, properties, access control |
| **Module system**   | Tier 1 (auto-loaded) + Tier 2 (`bring` required); search paths + circular-import guard                             |
| **Static analysis** | Lightweight linter: undefined-name detection, typo correction, edit-distance suggestions. No type checking.        |
| **REPL**            | Full line editor, multiline, tab completion, shell passthrough, persistent history                                 |

---

<a id="tier-1--necessary"></a>

## Tier 1 — Necessary (Language Correctness & Safety)

These are bugs, missing safety guarantees, or missing core features that cause incorrect behavior
or prevent users from writing reliable programs.

### 1.1 · `throw` / `raise` Statement

**Status**: ❌ Missing  
**Impact**: Critical

Users can `try`/`catch` errors thrown by builtins and the runtime, but they **cannot throw their
own errors**. There is no `throw`, `raise`, or `error` statement in the parser. This means:

- No user-defined exceptions
- No way to signal domain-specific errors
- `catch` blocks cannot re-throw

**Recommendation**: Add a `throw <expr>` statement. The expression should evaluate to a string
(message) or a map with `"message"`, `"type"` keys to match the current catch-variable shape.

---

### 1.2 · Cycle Detection / Weak References 

**NOTE --> not done yet**

**Status**: ❌ Missing  
**Impact**: Critical (silent memory leaks)

The memory model is pure reference counting with **no cycle collector** and **no weak refs**.
Circular references (parent↔child, self-referential closures, observer patterns) leak silently.
The codebase itself acknowledges "potentially-dangling closure environments" for regular
functions (raw `Environment*` pointers).

**Recommendation** (pick one):

- Add a **mark-and-sweep backup GC** that runs periodically (like CPython's `gc` module).
- Add **weak references** (`WeakRef` type) so users can break cycles manually.
- At minimum, add cycle-leak detection in debug/test mode.

---

### 1.3 · Stack Traces / Call-Stack Info in Errors

**Status**: ❌ Missing  
**Impact**: Critical (debugging is near-impossible for non-trivial programs)

Errors carry only a **line number** — no column, no filename, no call stack. When an error
propagates through multiple function calls or modules, the user sees only the innermost line.

**Recommendation**: Maintain a call-stack frame list (`vector<Frame>` with function name, file,
line). Attach it to every `XellError`. Print a Python-style traceback on uncaught errors.

---

### 1.4 · Chained Comparisons Are Broken

**Status**: 🐛 Bug  
**Impact**: High

`a < b < c` parses as `(a < b) < c`, i.e., it compares the **boolean result** of `a < b`
against `c`. Most users expect Python-style chained comparisons: `a < b and b < c`.

**Recommendation**:  implement chained comparisons in the parser (emit `AND` of pairwise
comparisons).

---

### 1.5 · Closure Environment Safety

**Status**: 🐛 Bug (latent)  
**Impact**: High

Regular functions store a **raw `Environment*`** to their defining scope. If the scope is
destroyed before the function is called (e.g., returning a function from a function),
the pointer dangles. Lambdas are safe (they snapshot into a `shared_ptr`), but regular
functions are not.

**Recommendation**: Make all function closures capture via `shared_ptr<Environment>`, same as
lambdas. The performance cost is minimal (one heap allocation per function definition). Or use ref_count interally

---

### 1.6 · `in` Containment Operator

**Status**: ❌ Missing  
**Impact**: High

There is no `x in collection` expression. Users must call `.has(x)` or `.contains(x)` methods.
The parser has no `in` binary operator outside of `for..in` loops.

**Recommendation**: Add `in` as a binary operator at the comparison precedence level. Dispatch
to `__contains__` magic method or built-in `.has()`.

---

### 1.7 · Slice Syntax `[start:end:step]`

**Status**: ❌ Missing  
**Impact**: High

Index expressions only support single-element access `[i]`. There is no slice syntax for
strings, lists, or tuples. Users must call `.slice()` methods.

**Recommendation**: Parse `[a:b]` and `[a:b:c]` as slice expressions. Implement for strings,
lists, tuples, and bytes. Support negative indices.

---

### 1.8 · Error Objects Should Be Instances, Not Maps

**Status**: ⚠️ Design issue  
**Impact**: Medium-High

Caught errors are plain `XMap` objects with `"message"`, `"type"`, `"line"` keys. This means:

- No `instanceof` check for error types
- No error class hierarchy
- No custom error classes with extra fields
- Users cannot distinguish `TypeError` from `NameError` except by string comparison

**Recommendation**: Make errors proper class instances. Provide a built-in `Error` base class
with subclasses matching the C++ hierarchy. Support `catch (e: TypeError)` syntax.

---

### 1.9 · Augmented Assignment on Member Access

**Status**: ⚠️ Incomplete  
**Impact**: Medium

`obj->field += 1` has limited support. The parser handles `a->b op= expr` but chained member
access like `a->b->c += 1` may not resolve correctly because the assignment target resolution
only goes one level deep.

**Recommendation**: Generalize augmented assignment to work with any valid l-value expression
(member chains, index chains, combined).

---

### 1.10 · Generator Eager Drain in `for` Loops

**Status**: ⚠️ Design issue  
**Impact**: Medium

When a generator is used in a `for` loop, **all values are eagerly collected into a vector**
before iteration begins. This defeats the purpose of generators for:

- Infinite sequences
- Memory-efficient streaming
- Large data pipelines

**Recommendation**: Change `for` loop to consume generators lazily — call `next()` per
iteration, check for `DONE` phase, break when exhausted. also check if same happens in `while` and `loop`

---

---

<a id="tier-2--important"></a>

## Tier 2 — Important (Expected by Any Modern Scripting Language)

These are features that users coming from Python, JavaScript, Ruby, etc. will expect and be
confused by their absence.

### 2.1 · Comprehensions (List, Map, Set)

**Status**: ❌ Missing  
**Impact**: High

No list comprehensions (`[x * 2 for x in items]`), map comprehensions, or set comprehensions.
These are a defining productivity feature of Python-family languages.

**Recommendation**: Add comprehension syntax for lists, maps, and sets. Support filtering
(`if` clause) and nested `for`.

---

### 2.2 · Hex, Octal, Binary Number Literals

**Status**: ❌ Missing  
**Impact**: Medium-High

The lexer only recognizes decimal integers and floats. No `0x1F`, `0o77`, `0b1010`.
Essential for systems programming, bit manipulation, color codes, permissions.

**Recommendation**: Extend `scanNumber()` to detect `0x`, `0o`, `0b` prefixes. Also add
`_` as a digit separator for readability (`1_000_000`).

---

### 2.3 · Bitwise Operators

**Status**: ❌ Missing  
**Impact**: Medium-High

No `&`, `|`, `^`, `~`, `<<`, `>>`. The lexer has no tokens for these. Required for:

- Flag/bitmask manipulation
- Binary protocols
- Hash algorithms in user space
- Low-level I/O

**Recommendation**: Add all six bitwise operators + corresponding augmented assignments
(`&=`, `|=`, etc.). Add precedence levels between logical and comparison.

---

### 2.4 · Exponentiation Operator `**`

**Status**: ❌ Missing  
**Impact**: Medium

No power operator. Users must call `math.pow(a, b)` or `pow(a, b)`. Most modern scripting
languages have `**` (Python, JS, Ruby).

**Recommendation**: Add `**` as a right-associative binary operator at highest arithmetic
precedence. Add `**=` augmented assignment. Map to `__pow__` magic method.

---

### 2.5 · Ternary / Conditional Expression

**Status**: ❌ Missing (standalone)  
**Impact**: Medium

There is no `x if cond else y` expression. The `in_case` (switch) exists
as a statement but has no expression form. bt wait i thought we had <expr> if <cond> elif <cond> : <expr> okay maybe we have  a gap, check and varify and we maybe dont have ternary expression 

**Recommendation**: Add `<expr> if <cond> else <expr>`consistent with Xell's style.

---

### 2.6 · Multi-Catch / Typed Catch

**Status**: ❌ Missing  
**Impact**: Medium

`catch` takes a single variable name. There is no way to:

- Catch specific error types: `catch (e: TypeError)`
- Catch multiple types: `catch (e: TypeError | ValueError)`
- Have multiple catch blocks

**Recommendation**: Support `catch (name: ErrorType)` syntax. Fall through to the first
matching catch block.

---

### 2.7 · `not in` / `is not` Compound Operators

**Status**: ❌ Missing  
**Impact**: Medium

No `not in` or `is not`. Users must negate manually: `not (x in list)` (once `in` exists)
or `not (x is y)`. I think we already did not in, now we need is not.

**Recommendation**: Add compound operators in the parser. `not in` → negate containment check.
`is not` → negate identity check.

---

### 2.8 · Computed / Dynamic Map Keys

**Status**: ❌ Missing  
**Impact**: Medium

Map literals only support identifier or string-literal keys. No computed keys like
`{[expr]: value}` (JS) or `{(expr): value}`.

**Recommendation**: Support `{[<expr>]: <expr>}` syntax for dynamic map keys. we already support user's defined class or variables to be hashed via the __hash__ and have mutability controll too. check so we have to let user use those as key too.

---

### 2.9 · Scientific Notation for Floats

**Status**: ❌ Missing  
**Impact**: Medium

The lexer does not handle `1.5e10`, `3.14e-2`, `6.022E23`. Important for scientific and
data-processing use cases.

**NOTE: e is euler numebr and E is exponent**

**Recommendation**: Extend `scanNumber()` to recognize `e`/`E` followed by optional `+`/`-`
and digits.
Also i suggest you to checkout the math built in module for constants we have there(dont make all part of lang itself, tthe e an E will be part of lang)
---

### 2.10 · Spread / Rest in Function Parameters

**Status**: ⚠️ Partial  
**Impact**: Medium

Spread `...` works for list unpacking in calls and list literals, but there is no `*args` or
`**kwargs` rest-parameter syntax in function definitions to collect variadic arguments.

**Recommendation**: Add rest parameters: `fun foo(a, b, ...rest):` collects remaining positional
args into a list. Consider `**kwrest` for keyword arguments.

---

### 2.11 · String Escape Sequences

**Status**: ⚠️ Partial  
**Impact**: Medium

The lexer handles `\n`, `\t`, `\\`, `\"`, `\r`, `\0`. Missing:

- `\uXXXX` / `\UXXXXXXXX` — Unicode escapes
- `\xHH` — hex byte escapes
- `\a`, `\b`, `\f`, `\v` — less common but standard

**Recommendation**: Add `\uXXXX` at minimum. `\xHH` is important for binary/protocol work.

---

### 2.12 · Unused Variable / Unreachable Code Detection

**Status**: ❌ Missing (claimed but not implemented)  
**Impact**: Medium

The analyzer header comments claim unused-variable and unreachable-code detection, but neither
is implemented. The analyzer only checks for undefined names and typos.

**Recommendation**: Implement unused-variable and unreachable code warnings. Track which names are read
in Pass 2 and report names that were defined but never accessed.

---

### 2.13 · Function Arity Checking in Analyzer

**Status**: ❌ Missing  
**Impact**: Medium

The static analyzer does not check function call arity. Calling `foo(1, 2, 3)` when `foo`
takes two parameters produces a runtime error, not a static warning.

**Recommendation**: Track function signatures in the analyzer's scope. Emit warnings for
obvious arity mismatches on known functions.

---

---

<a id="tier-3--valuable"></a>

## Tier 3 — Valuable (Productivity & Ecosystem)

These improve developer experience and fill out the standard library.

### 3.1 · Generics / Type Parameters

**Status**: ❌ Missing  
**Impact**: Medium

No way to parameterize classes, functions, or interfaces with type variables. Not critical
for a dynamically-typed language, but useful for documentation and IDE support.

---

### 3.2 · Single-Quote Strings

**Status**: ❌ Missing  
**Impact**: Low-Medium

Only double-quoted strings are supported. Single-quote strings reduce escaping when the
content contains double quotes and are expected by most polyglot programmers.

---

### 3.3 · Raw Strings

**Status**: ❌ Missing  
**Impact**: Low-Medium

No `r"..."` raw-string syntax. Important for regex patterns and Windows file paths.

---

### 3.4 · Multi-Line String Improvements

**Status**: ⚠️ Basic  
**Impact**: Low-Medium

Triple-quoted strings (`"""..."""`) exist but there's no dedent/strip-margin behavior.
Leading whitespace in multi-line strings is preserved verbatim.

---

### 3.5 · Destructuring Assignment Enhancements

**Status**: ⚠️ Partial  
**Impact**: Medium

Basic destructuring exists (`let [a, b] = list`) but:

- No nested destructuring (`let [a, [b, c]] = nested`)
- No rest element in destructuring (`let [head, ...tail] = list`)
- No map/object destructuring (`let {x, y} = point`)

---

### 3.6 · String `.format()` / f-String Expressions

**Status**: ⚠️ Partial  
**Impact**: Medium

String interpolation exists (`"Hello {name}"`) but:

- No format specifiers (`{value:.2f}`, `{num:04d}`)
- No expression evaluation inside `{}` (only variable names)

---

### 3.7 · `do...while` Loop

**Status**: ❌ Missing  
**Impact**: Low-Medium

No post-condition loop. Users must use `loop:` with a conditional `break`.

---

### 3.8 · Expression-Mode `in_case` (Switch Expression)

**Status**: ❌ Missing  
**Impact**: Medium

`in_case` (switch/match) is statement-only. No expression form like Rust's `match` or
Kotlin's `when` that returns a value.

---

### 3.9 · Pattern Matching in `in_case`

**Status**: ❌ Missing  
**Impact**: Medium

`in_case` does equality matching only. No:

- Type patterns (`case int:`)
- Destructuring patterns (`case [x, y]:`)
- Guard clauses (`case x if x > 0:`)
- Range patterns (`case 1..10:`)

---

### 3.10 · Iterator Protocol Improvements

**Status**: ⚠️ Limited  
**Impact**: Medium

`__iter__` must return a **list or tuple** — it cannot return a lazy iterator/generator.
This forces eager materialization for custom iterables.

**Recommendation**: Allow `__iter__` to return a generator or an object with `__next__`.

---

### 3.11 · Context Manager Protocol (`with` Statement)

**Status**: ⚠️ Partial  
**Impact**: Medium

`__enter__` and `__exit__` magic methods exist, but there's no explicit `with` statement. wait i think i intentionally desinged it with `let...be` key word instead of `with....as` please do check if we implimented it completely!? i dont remember. in shell let should be context manager.
syntax in the parser to invoke them automatically for resource cleanup.

---

### 3.12 · REPL Tab Completion — Missing Builtins

**Status**: ⚠️ Incomplete  
**Impact**: Low-Medium

The REPL completer registers only a small subset of builtins. The analyzer knows ~400+
names but the REPL completer has far fewer. No context-aware (dot-triggered) completion. need this. we probably already have a autocomplete system for our built in ide check the shell-terminal sub-project files and maybe we could use that? i actually forgot where i had implimented that so you will have to search for it and check

---

### 3.13 · Unicode Identifiers

**Status**: ❌ Missing  
**Impact**: Low-Medium

Identifiers are ASCII-only (`[a-zA-Z_][a-zA-Z0-9_]*`). Non-Latin scripts cannot be used
for variable/function names. This limits internationalization — ironic given the `@convert`
dialect system.
but we want it to work at preprocessor level so it does not touch the core of the language(i hope our convert already works that way?!)
---

### 3.14 · Standard Library Gaps

| Module        | Gap                                                                                                                                                                       |
| ------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **util**      | Very thin — only `timer()` and `benchmark()`. Missing: sorting algorithms, searching, functional combinators (`map`/`filter`/`reduce` as builtins), itertools equivalents |
| **math**      | No `log2`, `log10`, `factorial`, `gcd`, `lcm`, `comb`, `perm` (if missing)                                                                                                |
| **string**    | No `center`, `ljust`, `rjust`, `zfill`, `encode`/`decode`, `maketrans`/`translate`                                                                                        |
| **TOML/YAML** | Read-only — `toml_write()` and `yaml_write()` are missing                                                                                                                 |
| **crypto**    | No crypto module at all — no MD5, no HMAC, no encryption (SHA-256 exists in C++ hash lib but not exposed)                                                                 |
| **database**  | No SQLite or database module                                                                                                                                              |
| **socket**    | Network module has HTTP client but no TCP/UDP server or raw socket support                                                                                                |
| **threading** | No user-space threading/concurrency module (generators use threads internally but this isn't exposed)                                                                     |

---

---

<a id="tier-4--nice-to-have"></a>

## Tier 4 — Nice to Have (Advanced / Aspirational)

These are longer-term goals that would elevate Xell from a scripting language to a more
complete platform.

### 4.1 · Bytecode Compiler + VM

**Status**: ❌ Not started (multiple TODO comments reference `.xelc` cache)  
**Impact**: Performance

Tree-walking is ~10-50× slower than bytecode VMs. A bytecode layer would also enable:

- AOT compilation
- Better debugging (source maps)
- Serializable closures

---

### 4.2 · True Async Event Loop

**Status**: ❌ Not started  
**Impact**: Scalability

Current async is generator-based (one OS thread per async call). A proper event loop
(epoll/kqueue-based) would enable:

- Concurrent I/O without thread overhead
- Async networking
- Timer-based scheduling

---

### 4.3 · Per-Process Hash Randomization

**Status**: ❌ Missing  
**Impact**: Security

SipHash uses a **deterministic default key**. Hash flooding DoS is theoretically possible
for programs that accept untrusted input as map keys.

**Recommendation**: Seed the SipHash key from `/dev/urandom` at process start.

---

### 4.4 · Interface Default Methods

**Status**: ❌ Missing  
**Impact**: Low-Medium

Interfaces can declare method signatures but cannot provide default implementations.
This forces implementing classes to define every method even when a sensible default exists.
**NOTE: but this is exactly why the interface was created for: only a template. if user need some defaults they should use abstract class we have right? so maybe it's okay as design choice**
---

### 4.5 · Enum Associated Values / Methods

**Status**: ❌ Missing  
**Impact**: Low-Medium

Enums have only auto-increment integer values. No:

- Custom values per variant
- String values
- Methods on enums
- Enum variants with associated data (algebraic data types)

---

### 4.6 · Windows REPL Support

**Status**: ⚠️ Stub  
**Impact**: Platform reach

The REPL's raw terminal I/O is POSIX-only. Windows has stubs that fall back to basic
`getline()` mode — no line editing, no tab completion, no multiline.

---

### 4.7 · Incremental / Streaming Hash API

**Status**: ❌ Missing  
**Impact**: Low

All hash functions are one-shot. No `hasher.update(chunk)` / `hasher.finalize()` for
hashing large data streams.

---

### 4.8 · Language Server — Type Inference

**Status**: ❌ Not started  
**Impact**: IDE experience

The LSP extension provides diagnostics via `xell --check` (the static analyzer), but there's
no type inference, no go-to-definition across files, no rename refactoring, no call hierarchy.

Also do check the built-in terminal and ide we are shipping with it please! 
---

### 4.9 · Package Manager / Registry

**Status**: ❌ Not started  
**Impact**: Ecosystem

No package manager, no package registry, no dependency resolution. Third-party code sharing
requires manual file copying.

---

### 4.10 · Debugger / Step-Through Execution

**Status**: ❌ Not started  
**Impact**: Developer experience

No breakpoints, no step-in/step-over, no variable inspection. The only debugging tool is
`print()`.
**NOTE: wait maybe i had added a huge debugging system  for it check the decorators**
---

---

<a id="subsystem-summaries"></a>

## Subsystem Summaries

### Lexer (845 lines)

| Aspect                     | Status           |
| -------------------------- | ---------------- |
| 54 keywords                | ✅ Comprehensive |
| String interpolation       | ✅ `"{expr}"`    |
| Triple-quoted strings      | ✅               |
| Complex literals (`3i`)    | ✅               |
| Block + line comments      | ✅               |
| Hex/octal/binary literals  | ❌ Missing       |
| Scientific notation        | ❌ Missing       |
| Single-quote strings       | ❌ Missing       |
| Raw strings                | ❌ Missing       |
| Unicode identifiers        | ❌ Missing       |
| Digit separators (`1_000`) | ❌ Missing       |
| Nested block comments      | ❌ Not supported |
| Bitwise operator tokens    | ❌ Missing       |
| Power operator token       | ❌ Missing       |

### Parser (2,979 lines)

| Aspect                                           | Status           |
| ------------------------------------------------ | ---------------- |
| 30 expression node types                         | ✅ Comprehensive |
| 22+ statement node types                         | ✅               |
| Full OOP (class/struct/interface/abstract/mixin) | ✅               |
| Decorators                                       | ✅               |
| Properties (get/set)                             | ✅               |
| `in_case` (switch/match)                         | ✅ (basic)       |
| Generators (`yield`)                             | ✅               |
| `async`/`await`                                  | ✅ (sugar)       |
| `let..be` blocks                                 | ✅               |
| `throw`/`raise`                                  | ❌ Missing       |
| Slice expressions                                | ❌ Missing       |
| Comprehensions                                   | ❌ Missing       |
| Ternary expression                               | ❌ Missing       |
| Bitwise expressions                              | ❌ Missing       |
| Exponentiation                                   | ❌ Missing       |
| Chained comparisons                              | 🐛 Broken        |
| Computed map keys                                | ❌ Missing       |
| Pattern matching                                 | ❌ Missing       |
| `with` statement                                 | ❌ Missing       |
| `do..while`                                      | ❌ Missing       |

### Interpreter (4,921 lines)

| Aspect                                    | Status                       |
| ----------------------------------------- | ---------------------------- |
| Tree-walking evaluator                    | ✅                           |
| Try/catch/finally                         | ✅                           |
| Generators (thread-based)                 | ✅                           |
| Decorators (function + class)             | ✅                           |
| Properties (getter/setter/access control) | ✅                           |
| Magic methods (14 supported)              | ✅                           |
| Operator overloading                      | ✅                           |
| Closure capture (lambdas)                 | ✅ (heap snapshot)           |
| Closure capture (functions)               | 🐛 Raw pointer (can dangle)  |
| Generator for-loop consumption            | ⚠️ Eager drain               |
| Error objects                             | ⚠️ Plain maps, not instances |
| Async implementation                      | ⚠️ Just generator sugar      |
| Recursion protection                      | ✅ (configurable limit)      |

### Type System (18 types)

| Type                                    | Status     |
| --------------------------------------- | ---------- |
| none, int, float, complex, bool, string | ✅         |
| list, tuple, set, frozen_set, map       | ✅         |
| function, enum, bytes                   | ✅         |
| generator, class, instance, module      | ✅         |
| Weak references                         | ❌ Missing |
| Optional/nullable type                  | ❌ Missing |
| Result/Either type                      | ❌ Missing |
| Type aliases                            | ❌ Missing |

### Builtins (23 modules, ~372 functions)

| Module                                  | Maturity                              |
| --------------------------------------- | ------------------------------------- |
| io, type, math, string, list, map       | ✅ Solid                              |
| hash, bytes, generator, datetime, regex | ✅ Solid                              |
| json, csv, toml, yaml                   | ✅ Read; ⚠️ TOML/YAML write missing   |
| fs, shell, textproc                     | ✅ Solid                              |
| process, sysmon, network, archive       | ✅ Functional                         |
| os, set                                 | ✅ Adequate                           |
| util                                    | ⚠️ Very thin (only timer + benchmark) |
| crypto                                  | ❌ Does not exist                     |
| database/sqlite                         | ❌ Does not exist                     |
| socket (server)                         | ❌ Does not exist                     |
| threading                               | ❌ Does not exist                     |

### Static Analyzer

| Feature                          | Status                                    |
| -------------------------------- | ----------------------------------------- |
| Undefined name detection         | ✅                                        |
| Typo correction (hardcoded list) | ✅                                        |
| Edit-distance suggestions        | ✅                                        |
| Scope tracking                   | ✅                                        |
| Wildcard import handling         | ✅ (coarse)                               |
| Type checking                    | ❌ Not implemented                        |
| Unused variable detection        | ❌ Not implemented (despite header claim) |
| Unreachable code detection       | ❌ Not implemented (despite header claim) |
| Function arity checking          | ❌ Not implemented                        |
| Duplicate definition warnings    | ❌ Not implemented                        |
| Method call validation           | ❌ Skipped entirely                       |

### Error System (22 error types)

| Feature                                | Status     |
| -------------------------------------- | ---------- |
| Comprehensive error hierarchy (C++)    | ✅         |
| Line number in errors                  | ✅         |
| Column number                          | ❌ Missing |
| Source filename                        | ❌ Missing |
| Stack trace / traceback                | ❌ Missing |
| Error codes                            | ❌ Missing |
| User-space error classes               | ❌ Missing |
| Warning-level runtime diagnostics      | ❌ Missing |
| Source-context display (caret markers) | ❌ Missing |

### REPL

| Feature                                   | Status                     |
| ----------------------------------------- | -------------------------- |
| Line editing (cursor, delete, kill)       | ✅                         |
| Multiline editing                         | ✅                         |
| Persistent history                        | ✅                         |
| Tab completion (keywords + some builtins) | ✅ (partial)               |
| Shell passthrough                         | ✅                         |
| REPL commands (`:help`, `:vars`, etc.)    | ✅                         |
| Kitty keyboard protocol                   | ✅                         |
| UTF-8 visible-width calculation           | ✅                         |
| Reverse search (Ctrl+R)                   | ❌ Wired but not connected |
| Syntax highlighting                       | ❌ Missing                 |
| Dot-triggered completion                  | ❌ Missing                 |
| Full builtin completion list              | ❌ Incomplete              |
| Undo/redo                                 | ❌ Missing                 |
| Windows support                           | ⚠️ Stub                    |

---

<a id="full-gap-inventory"></a>

## Full Gap Inventory

A flat list of every identified gap, for tracking purposes.

| #   | Category     | Gap                                    | Tier |
| --- | ------------ | -------------------------------------- | ---- |
| 1   | Parser       | `throw`/`raise` statement              | 1    |
| 2   | Memory       | Cycle collector or weak refs           | 1    |
| 3   | Errors       | Stack traces / traceback               | 1    |
| 4   | Parser       | Chained comparisons bug                | 1    |
| 5   | Interpreter  | Closure env safety (raw pointer)       | 1    |
| 6   | Parser       | `in` containment operator              | 1    |
| 7   | Parser       | Slice syntax `[a:b:c]`                 | 1    |
| 8   | Interpreter  | Error objects as class instances       | 1    |
| 9   | Interpreter  | Augmented member assignment depth      | 1    |
| 10  | Interpreter  | Generator eager drain in for-loops     | 1    |
| 11  | Parser       | List/map/set comprehensions            | 2    |
| 12  | Lexer        | Hex/octal/binary number literals       | 2    |
| 13  | Lexer+Parser | Bitwise operators                      | 2    |
| 14  | Parser       | Exponentiation `**`                    | 2    |
| 15  | Parser       | Ternary/conditional expression         | 2    |
| 16  | Parser       | Multi-catch / typed catch              | 2    |
| 17  | Parser       | `not in` / `is not`                    | 2    |
| 18  | Parser       | Computed map keys                      | 2    |
| 19  | Lexer        | Scientific notation                    | 2    |
| 20  | Parser       | Spread/rest in function params         | 2    |
| 21  | Lexer        | `\uXXXX` / `\xHH` escape sequences     | 2    |
| 22  | Analyzer     | Unused variable detection              | 2    |
| 23  | Analyzer     | Function arity checking                | 2    |
| 24  | Parser       | Generics / type parameters             | 3    |
| 25  | Lexer        | Single-quote strings                   | 3    |
| 26  | Lexer        | Raw strings `r"..."`                   | 3    |
| 27  | Strings      | Multi-line string dedent               | 3    |
| 28  | Parser       | Nested destructuring + rest            | 3    |
| 29  | Strings      | Format specifiers in interpolation     | 3    |
| 30  | Parser       | `do..while` loop                       | 3    |
| 31  | Parser       | Expression-mode `in_case`              | 3    |
| 32  | Parser       | Pattern matching in `in_case`          | 3    |
| 33  | Interpreter  | `__iter__` returning generators        | 3    |
| 34  | Parser       | `with` statement (context managers)    | 3    |
| 35  | REPL         | Full builtin completion list           | 3    |
| 36  | Lexer        | Unicode identifiers                    | 3    |
| 37  | Builtins     | `util` module (very thin)              | 3    |
| 38  | Builtins     | TOML/YAML write support                | 3    |
| 39  | Builtins     | Crypto module                          | 3    |
| 40  | Builtins     | Database/SQLite module                 | 3    |
| 41  | Builtins     | Socket server                          | 3    |
| 42  | Builtins     | Threading module                       | 3    |
| 43  | Interpreter  | Bytecode compiler + VM                 | 4    |
| 44  | Interpreter  | True async event loop                  | 4    |
| 45  | Hash         | Per-process hash randomization         | 4    |
| 46  | OOP          | Interface default methods              | 4    |
| 47  | OOP          | Enum associated values / methods       | 4    |
| 48  | REPL         | Windows support                        | 4    |
| 49  | Hash         | Incremental/streaming hash API         | 4    |
| 50  | LSP          | Type inference                         | 4    |
| 51  | Ecosystem    | Package manager / registry             | 4    |
| 52  | Tooling      | Debugger / step-through                | 4    |
| 53  | Errors       | Column numbers in errors               | 4    |
| 54  | Errors       | Source-context display (carets)        | 4    |
| 55  | Errors       | User-space error class hierarchy       | 4    |
| 56  | Analyzer     | Unreachable code detection             | 4    |
| 57  | Analyzer     | Duplicate definition warnings          | 4    |
| 58  | REPL         | Reverse search (Ctrl+R)                | 4    |
| 59  | REPL         | Syntax highlighting                    | 4    |
| 60  | Lexer        | Digit separators `1_000_000`           | 4    |
| 61  | Builtins     | Math extras (log2, factorial, gcd…)    | 4    |
| 62  | Builtins     | String extras (center, zfill, encode…) | 4    |

---

> **Total gaps identified: 62**  
> **Tier 1 (Necessary): 10** · **Tier 2 (Important): 13** · **Tier 3 (Valuable): 19** · **Tier 4 (Nice to Have): 20**

---

_Audit performed by full source-code review. No existing documentation was referenced._
