docs/
├── README.md                           # Documentation home / navigation
├── getting-started.md                  # Quick start, installation, hello world
│
├── core/
│   ├── overview.md
│   ├── syntax-basics.md
│   ├── data-types.md
│   ├── variables-binding.md
│   ├── operators.md
│   ├── expressions.md
│   ├── control-flow.md
│   ├── functions.md
│   ├── lambdas.md
│   ├── error-handling.md
│   ├── pattern-matching.md
│   ├── raii.md
│   ├── generators.md
│   ├── async.md
│   ├── shell.md
│   ├── decorators.md
│   └── builtins.md
│
├── oop/
│   ├── oop-structs.md
│   ├── oop-classes.md
│   ├── oop-inheritance.md
│   ├── oop-mixins.md
│   ├── oop-interfaces.md
│   ├── oop-access-control.md
│   ├── oop-properties.md
│   ├── oop-magic-methods.md
│   └── oop-abstract.md
│
├── modules/
│   ├── modules-definition.md
│   ├── modules-import.md
│   ├── modules-resolution.md
│   └── modules-builtins.md
│
├── stdlib/
│   ├── stdlib-collections.md
│   ├── stdlib-hof.md
│   ├── stdlib-fs.md
│   ├── stdlib-path.md
│   ├── stdlib-env.md
│   ├── stdlib-process.md
│   ├── stdlib-math.md
│   └── stdlib-string.md
│
├── tooling/
│   ├── repl.md
│   ├── cli.md
│   ├── linting.md
│   ├── dialect-conversion.md
│   ├── module-indexing.md
│   ├── debugging.md
│   ├── kernel.md
│   └── terminal-gui.md
│
├── advanced/
│   ├── garbage-collection.md
│   ├── hash-system.md
│   ├── ordered-hash-table.md
│   ├── f-strings.md
│   └── gotchas.md
│
└── reference/
    ├── keywords.md
    ├── error-types.md
    └── builtin-index.md



# Plan: 

# Xell Documentation Architecture Plan

## 1. Feature Inventory

After analyzing the entire codebase, here is the complete inventory of every language feature, keyword, and construct that requires documentation.

---

### CORE LANGUAGE

| # | Feature | Keywords/Constructs | Notes |
|---|---------|---------------------|-------|
| 1 | **Data Types** | `none`, `int`, `float`, `complex`, `bool`, `string`, `bytes` | Includes integer literals (`0x`, `0o`, `0b`), scientific notation |
| 2 | **Compound Types** | `list`, `tuple`, `set`, `frozen_set` (`~{...}`), `map` | List spread (`...`), set/map comprehensions |
| 3 | **Variables & Binding** | `let` (mutable), `immutable` (immutable), destructuring assignment | `let...be` (RAII) is separate |
| 4 | **Operators** | All arithmetic, comparison, logical, bitwise, shell (`&&`, `||`, `\|>`), pipe | Precedence, short-circuiting, `not in`, `is`, `is not`, `in` |
| 5 | **Expressions** | All expression forms, ternary (`val if cond else alt`), expression-mode statements | `if` expression, `for` expression, `while` expression, `loop` expression, `incase` expression |
| 6 | **Blocks & Scoping** | `:` ... `;` block syntax, lexical scoping, closures | Environment model, variable capture |
| 7 | **Control Flow** | `if`/`elif`/`else`, `for` (with destructuring, rest, parallel iteration), `while`, `loop` (infinite), `do...while` | `break` (with value), `continue` |
| 8 | **Functions** | `fn`, parameters (typed, default, variadic `...`), return type annotations (`-> Type`), `give` (return) | Closures, generator functions (`yield`), async (`async fn`) |
| 9 | **Lambdas** | `x => expr`, `(a, b) => expr`, `x => : block ;` | Multi-line lambdas, closure capture |
| 10 | **Error Handling** | `try`/`catch`/`finally`, `throw`, error type hierarchy | `catch` with type filters (`is Type or Type`), structured errors |
| 11 | **Pattern Matching** | `incase` statement (with `is`, `belong`, `bind`, guards) | Expression-mode `incase` |
| 12 | **RAII / Context Managers** | `let EXPR be NAME, ... : BLOCK ;` | `__enter__`/`__exit__` magic methods |
| 13 | **Generators** | `yield` (in `fn`), `for` consuming generators | Lazy iteration, `await` draining generators |
| 14 | **Async** | `async fn`, `await` | Async functions, awaiting generators |
| 15 | **Shell Integration** | `$cmd`, `\|>` (pipe), `&&`, `\|\|`, PATH-based external command fallback | Exit code handling, `set_e()` mode |
| 16 | **Decorators** | `@decorator` on `fn`, `class`, `abstract`, `mixin`, `loop`, `bring` | Multiple decorators stack, built-in: `@debug`, `@breakpoint`, `@watch`, `@track`, `@profile`, `@log`, `@checkpoint`, `@safe_loop`, `@eager`, `@dataclass`, `@immutable`, `@singleton` |

---

### OBJECT-ORIENTED FEATURES

| # | Feature | Keywords/Constructs | Notes |
|---|---------|---------------------|-------|
| 17 | **Structs** | `struct Name : field = default fn method(...) : ... ; ;` | Value types, fields, methods |
| 18 | **Classes** | `class Name [inherits Parent] [with Mixin] [implements Interface] : ... ;` | Reference types, `__init__`, `__str__`, `__print__`, etc. |
| 19 | **Abstract Classes** | `abstract Name : ... ;` | Cannot instantiate, abstract methods (`fn name(...);`) |
| 20 | **Mixins** | `mixin Name : ... ;` | No `__init__`, can be mixed into classes |
| 21 | **Interfaces** | `interface Name : fn method(self) ; fn method2(self, x) ; ;` | Contract-only, no implementation |
| 22 | **Inheritance** | `inherits Parent1, Parent2` | Multiple inheritance, MRO, `super`/`parent` |
| 23 | **Access Control** | `public:`, `protected:`, `private:`, `static` | Class-level access, static members |
| 24 | **Properties** | `get name(self) : ... ; set name(self, val) : ... ;` | Getters/setters inside classes |
| 25 | **Magic Methods** | `__init__`, `__str__`, `__print__`, `__len__`, `__contains__`, `__iter__`, `__next__`, `__call__`, `__hash__`, `__enter__`, `__exit__`, `__add__`, `__sub__`, etc. | Operator overloading, protocol methods |
| 26 | **Instances** | `~Name(args...)` (frozen instance) | Immutable instances, `frozen` field |

---

### MODULE SYSTEM

| # | Feature | Keywords/Constructs | Notes |
|---|---------|---------------------|-------|
| 27 | **Modules** | `module name : body ;`, `export` | First-class modules, `__name__`, `__file__`, `__version__`, `__exports__` |
| 28 | **Imports** | `bring name1, name2 from "file" [as a, b]`, `bring X of module->path [as alias]`, `bring * of module` | `from` directory, `and` chaining, `@eager` |
| 29 | **Dependencies** | `requires name`, `requires X, Y of module->path` | Module-level dependency declarations |
| 30 | **Module Resolution** | `.xell_meta` index, `__xelcache__/`, source hashing, circular import detection | Search path: from dir → CWD → `XELL_PATH` → stdlib |

---

### STANDARD LIBRARY (BUILTINS)

| # | Module/Functions | Notes |
|---|------------------|-------|
| 31 | **Core Builtins** | `print`, `input`, `exit`, `len`, `type`, `str`, `num`, `typeof`, `to_int`, `to_float`, `to_str`, `contains` |
| 32 | **Collections** | `push`, `pop`, `keys`, `values`, `range`, `set`, `has`, `sort`, `reverse`, `slice`, `join`, `split`, `index_of` |
| 33 | **Higher-Order** | `map`, `filter`, `reduce`, `any`, `all` |
| 34 | **File System** | `mkdir`, `rm`, `cp`, `mv`, `exists`, `is_file`, `is_dir`, `ls`, `read`, `write`, `append`, `file_size` |
| 35 | **Path** | `cwd`, `cd`, `abspath`, `basename`, `dirname`, `ext` |
| 36 | **Environment** | `env_get`, `env_set`, `env_unset`, `env_has` |
| 37 | **Process** | `run`, `run_capture`, `pid`, `sleep`, `exit` |
| 38 | **Math** | `PI`, `E`, `INF`, `floor`, `ceil`, `round`, `abs`, `mod`, `sqrt`, `pow`, `sin`, `cos`, `tan`, `log`, `log10`, `max`, `min`, `random`, `random_int` |
| 39 | **String** | `upper`, `lower`, `trim`, `starts_with`, `ends_with`, `replace`, `substr`, `char_at` |
| 40 | **Concurrency** | `thread_spawn`, `thread_join`, `thread_done`, `thread_count`, `mutex_create`, `mutex_lock`, `mutex_unlock`, `mutex_try_lock` (`bring threading`) |

---

### TOOLING & LSP

| # | Feature | Notes |
|---|---------|-------|
| 41 | **REPL** | Interactive shell, multi-line input, history |
| 42 | **Linting** | `--check`, `--lint`, static analysis, unused variable detection, symbol collection |
| 43 | **Dialect Conversion** | `@convert "map.xesy"`, `--convert`, `--revert`, `--gen_xesy` | Canonical ↔ custom keyword mapping |
| 44 | **Module Indexing** | `--make_module [--update]`, `.xell_meta`, `__xelcache__/` | Fast module resolution |
| 45 | **Debugging** | `--trace-vars` (variable lifecycle JSON), `--debug` (IPC step-through) | `@breakpoint`, `@watch`, `@checkpoint`, `@profile`, `@log` |
| 46 | **Notebook Kernel** | `--kernel` | JSON-over-stdout kernel protocol |
| 47 | **Terminal GUI** | `--terminal`, `--ide` | SDL2-based terminal interface |
| 48 | **Color Customizer** | `--customize` | Python web app for theme customization |
| 49 | **LSP / Editor Integration** | `--check-symbols` (symbol map JSON), `--check-string` (buffer linting) | Language Server Protocol support |

---

### INTERNALS / ADVANCED

| # | Feature | Notes |
|---|---------|-------|
| 50 | **Garbage Collection** | Reference counting + cycle-collecting GC (`gc_collect()`), container types, `gc_refs` algorithm |
| 51 | **Hash System** | `fnv1a` (default), `djb2`, `murmur3`, `siphash`, SHA-256 for cache | Set/map hashing, `__hash__` magic method |
| 52 | **Ordered Hash Table** | `OrderedHashTable` — insertion-order-preserving, no tombstones | Used by `set`, `map` |
| 53 | **F-Strings** | `"hello {expr} world"` | Interpolation, format specifiers `{expr:format}` |
| 54 | **Lexer/Parse Quirks** | Significant newlines, indentation sensitivity, `--> ... <--` comments, `#` comments | Edge cases, error recovery |

---

## 2. Planned Documentation Files

### 2.1 Core Language (`docs/core/`)

| File | Description | Keywords/Constructs | Dependencies |
|------|-------------|---------------------|--------------|
| `overview.md` | Introduction to Xell | Philosophy, design goals, hello world | None (first read) |
| `syntax-basics.md` | Lexical structure | Comments (`#`, `--> ... <--`), significant newlines, indentation, identifiers, literals, `;` statement terminator | `overview.md` |
| `data-types.md` | All primitive types | `none`, `int` (0x, 0o, 0b, `_` separators), `float` (scientific `E`), `complex` (`2i`, `3.14i`), `bool` (`true`/`false`), `string` (single/double, triple-quoted, raw `r"..."`), `bytes` (`b"..."`), `f-string` (`f"..."`) | `syntax-basics.md` |
| `variables-binding.md` | Variables and assignment | `let name = expr`, `immutable name = expr`, assignment (`name = expr`), destructuring assignment (`a, b = list`, `{x, y} = obj`), `_` discard, scope, immutability | `data-types.md`, `syntax-basics.md` |
| `operators.md` | All operators | Arithmetic (`+`, `-`, `*`, `/`, `%`, `**`), comparison (`==`, `!=`, `>`, `<`, `>=`, `<=`, `is`, `is not`, `in`, `not in`), logical (`and`, `or`, `not`), bitwise (`\|`, `&`, `^`, `<<`, `>>`, `~`), shell (`\|>`, `&&`, `\|\|`), precedence, short-circuiting | `data-types.md` |
| `expressions.md` | All expression forms | Ternary (`val if cond else alt`), `if` expression (`if cond: val elif cond: val else: val`), `for` expression (`x = for i in list: BLOCK give default;`), `while` expression, `loop` expression, `incase` expression, `yield`, `await`, `spread` (`...expr`) | `operators.md`, `data-types.md` |
| `control-flow.md` | Statements that control execution | `if`/`elif`/`else`, `for` (with destructuring, parallel, rest `...name`), `while`, `loop` (infinite), `do...while`, `break` (with value), `continue`, `give` (return) | `expressions.md`, `syntax-basics.md` |
| `functions.md` | Function definitions and calls | `fn name(params) : body ;`, parameters (typed `param: Type`, default `param = expr`, variadic `...name`), return type `-> Type`, `give`, closures, overloading (type-annotated overloads), `async fn` | `control-flow.md`, `data-types.md` |
| `lambdas.md` | Anonymous functions | `x => expr`, `(a, b) => expr`, `x => : block ;`, closure capture | `functions.md` |
| `error-handling.md` | Exceptions | `try`/`catch`/`finally`, `throw`, error types (`TypeError`, `IndexError`, `KeyError`, etc.), `catch` filters (`is Type or Type`), structured error objects (map/instance) | `functions.md`, `data-types.md` |
| `pattern-matching.md` | InCase pattern matching | `incase expr : is value or value : body ; belong TypeName : body ; bind name [if guard] : body ; else : body ;`, expression-mode `incase`, guards (`if condition`), type matching (`belong int`), value capture (`bind`) | `control-flow.md`, `data-types.md` |
| `raii.md` | Context managers | `let EXPR be NAME, EXPR be NAME : BLOCK ;`, `__enter__`, `__exit__`, resource cleanup, reverse-order teardown | `error-handling.md`, `variables-binding.md` |
| `generators.md` | Lazy sequences | `yield` inside `fn`, `for` over generators, `await` draining generators, `next()`, generator state (thread-based) | `functions.md`, `control-flow.md` |
| `async.md` | Asynchronous programming | `async fn`, `await expr`, awaiting generators, concurrency | `generators.md`, `functions.md` |
| `shell.md` | Shell integration | `$cmd` (command execution), `\|>` (pipe), `&&` (AND), `\|\|` (OR), `set_e()` (exit-on-error mode), PATH-based external command fallback | `operators.md`, `control-flow.md` |
| `decorators.md` | Function/class decorators | `@decorator` syntax, multiple decorators, built-in decorators (`@debug`, `@breakpoint`, `@watch`, `@track`, `@profile`, `@log`, `@checkpoint`, `@safe_loop`, `@eager`, `@dataclass`, `@immutable`, `@singleton`), user-defined decorators | `functions.md`, `oop-classes.md`, `debugging.md` |
| `builtins.md` | Core built-in functions | `print`, `input`, `exit`, `len`, `type`, `str`, `num`, `typeof`, `to_int`, `to_float`, `to_str`, `contains`, `range`, `assert`, `hashme` | `data-types.md`, `functions.md` |

---

### 2.2 Object-Oriented Programming (`docs/oop/`)

| File | Description | Keywords/Constructs | Dependencies |
|------|-------------|---------------------|--------------|
| `oop-structs.md` | Struct definitions | `struct Name : field = expr fn method(...) : ... ; ;`, fields, methods, field access (`obj->field`), method calls | `functions.md`, `data-types.md` |
| `oop-classes.md` | Class definitions | `class Name [inherits Parents] [with Mixins] [implements Interfaces] : body ;`, `__init__` (constructor), `__str__`, `__print__`, `__len__`, `__contains__`, `__iter__`, `__next__`, `__call__`, `__hash__`, instantiation (`Name(args...)`), frozen instances (`~Name(args...)`), `self` | `oop-structs.md` |
| `oop-inheritance.md` | Inheritance | Multiple inheritance (`inherits A, B`), method resolution order (MRO), `parent` keyword for super calls, `parent->method()`, abstract methods, `is` instance-of check | `oop-classes.md` |
| `oop-mixins.md` | Mixins | `mixin Name : ... ;`, `with Mixin1, Mixin2`, no `__init__`, method composition, lower priority than parents | `oop-classes.md`, `oop-inheritance.md` |
| `oop-interfaces.md` | Interfaces | `interface Name : fn method(params) ; ... ;`, `implements Interface`, interface method validation, abstract methods | `oop-classes.md`, `oop-inheritance.md` |
| `oop-access-control.md` | Visibility and static members | `public:`, `protected:`, `private:`, `static` fields/methods, access rules, `executingMethodClass_` | `oop-classes.md` |
| `oop-properties.md` | Getters and setters | `get name(self) : ... ;`, `set name(self, val) : ... ;`, property access (`obj->property`), read-only/write-only properties | `oop-classes.md` |
| `oop-magic-methods.md` | Operator overloading | `__add__`, `__sub__`, `__mul__`, `__div__`, `__mod__`, `__pow__`, `__and__`, `__or__`, `__xor__`, `__lshift__`, `__rshift__`, `__pipe__`, `__eq__`, `__ne__`, `__lt__`, `__gt__`, `__le__`, `__ge__`, `__neg__`, `__invert__`, `__get__`, `__set__` | `oop-classes.md`, `operators.md` |
| `oop-abstract.md` | Abstract classes and methods | `abstract Name : ... ;`, `fn name(...) ;` (abstract method — no body), must implement all abstract methods from ancestors | `oop-classes.md`, `oop-inheritance.md` |

---

### 2.3 Module System (`docs/modules/`)

| File | Description | Keywords/Constructs | Dependencies |
|------|-------------|---------------------|--------------|
| `modules-definition.md` | Defining modules | `module name : body ;`, `export` (declaration), `requires`, `__name__`, `__file__`, `__version__`, `__exports__`, `__submodules__`, first-class modules | `functions.md`, `oop-classes.md` |
| `modules-import.md` | Importing modules | `bring name from "file" [as alias]`, `bring X of module->path [as alias]`, `bring * of module [as alias]`, `from "dir" bring ...`, `and` chaining, `@eager` | `modules-definition.md` |
| `modules-resolution.md` | Module resolution algorithm | `.xell_meta` index, `__xelcache__/` cache, search path (`from` dir → CWD → `XELL_PATH` → stdlib), circular import detection, `.xesy` dialect mapping | `modules-import.md` |
| `modules-builtins.md` | Built-in modules | `threading` module (`thread_spawn`, `thread_join`, `thread_done`, `thread_count`, `mutex_create`, `mutex_lock`, `mutex_unlock`, `mutex_try_lock`) | `modules-import.md`, `builtins.md` |

---

### 2.4 Standard Library (`docs/stdlib/`)

| File | Description | Functions | Dependencies |
|------|-------------|-----------|--------------|
| `stdlib-collections.md` | Collection builtins | `push`, `pop`, `shift`, `unshift`, `insert`, `remove_val`, `sort`, `sort_desc`, `reverse`, `slice`, `join`, `split`, `contains`, `index_of`, `range`, `set`, `has`, `keys`, `values` | `data-types.md` |
| `stdlib-hof.md` | Higher-order functions | `map(list, fn)`, `filter(list, fn)`, `reduce(list, fn, init)`, `any(list, fn)`, `all(list, fn)` | `functions.md`, `data-types.md` |
| `stdlib-fs.md` | File system operations | `mkdir`, `rm`, `cp`, `mv`, `exists`, `is_file`, `is_dir`, `ls`, `read`, `write`, `append`, `file_size` | `error-handling.md` |
| `stdlib-path.md` | Path utilities | `cwd`, `cd`, `abspath`, `basename`, `dirname`, `ext` | `stdlib-fs.md` |
| `stdlib-env.md` | Environment variables | `env_get`, `env_set`, `env_unset`, `env_has`, `__args__` (CLI arguments) | `variables-binding.md` |
| `stdlib-process.md` | Process management | `run`, `run_capture`, `pid`, `sleep`, `exit`, `set_e()`, `last_exit_code` | `shell.md`, `error-handling.md` |
| `stdlib-math.md` | Math functions | `PI`, `E`, `INF`, `floor`, `ceil`, `round`, `abs`, `mod`, `sqrt`, `pow`, `sin`, `cos`, `tan`, `log`, `log10`, `max`, `min`, `random`, `random_int` | `data-types.md`, `operators.md` |
| `stdlib-string.md` | String functions | `upper`, `lower`, `trim`, `starts_with`, `ends_with`, `replace`, `substr`, `char_at`, f-string interpolation | `data-types.md` |

---

### 2.5 Tooling & LSP (`docs/tooling/`)

| File | Description | Commands/APIs | Dependencies |
|------|-------------|---------------|--------------|
| `repl.md` | Interactive REPL | `xell` (no args), multi-line input (`\` at end of line), history | None |
| `cli.md` | Command-line interface | `xell <file>`, `-e "<code>"`, `--check`, `--lint`, `--check-string`, `--check-symbols`, `--trace-vars`, `--debug`, `--make_module`, `--convert`, `--revert`, `--gen_xesy`, `--terminal`, `--kernel`, `--version`, `--help`, `--source-dir` | None |
| `linting.md` | Static analysis | `--check`, `--lint`, `--check-string`, `--check-symbols` (symbol map JSON), static analyzer, unused variable detection, `"error"`/`"warning"`/`"hint"` severity levels | `cli.md` |
| `dialect-conversion.md` | Dialect system | `@convert "map.xesy"`, `--convert`, `--revert`, `--gen_xesy`, `.xesy` file format, canonical ↔ custom keyword mapping | `cli.md` |
| `module-indexing.md` | Module indexing tool | `--make_module [--update]`, `.xell_meta` generation, `__xelcache__/`, source hashing, bytecode cache placeholder | `modules-resolution.md`, `cli.md` |
| `debugging.md` | Debug & trace system | `--trace-vars` (variable lifecycle JSON), `--debug` (IPC step-through), `@breakpoint`, `@watch`, `@checkpoint`, `@profile`, `@log`, `@track`, `@notrack`, `@debug on/off/sample N`, `XELL_DEBUG_SOCKET` | `decorators.md`, `cli.md` |
| `kernel.md` | Notebook kernel mode | `--kernel`, JSON protocol (`execute`, `reset`, `shutdown`), cell execution, `stdout`/`stderr` capture, `execution_count` | `cli.md` |
| `terminal-gui.md` | Xell Terminal GUI | `--terminal`, `--ide`, SDL2-based UI, color themes, `--customize` | `cli.md` |

---

### 2.6 Advanced Topics (`docs/advanced/`)

| File | Description | Topics | Dependencies |
|------|-------------|--------|--------------|
| `garbage-collection.md` | GC internals | Reference counting, cycle-collecting GC, container types (`list`, `tuple`, `set`, `frozen_set`, `map`, `function`, `enum`, `instance`, `struct_def`, `module`), `gc_collect()`, `gc_refs` algorithm, tracking list | None (advanced) |
| `hash-system.md` | Hash algorithms | `fnv1a`, `djb2`, `murmur3`, `siphash`, SHA-256, `__hash__` magic method, hashable types (immutable only), `hash_combine`, `hash_int`, `hash_float` | `data-types.md`, `oop-magic-methods.md` |
| `ordered-hash-table.md` | OrderedHashTable internals | Insertion-order iteration, separate chaining, no tombstones, free list recycling, load factor 75%, `keys()`, `values()`, `entries()` | `garbage-collection.md`, `hash-system.md` |
| `f-strings.md` | String interpolation | `f"hello {expr} world"`, `{expr:format}` (format specifiers), nested braces, string-aware replacement, `__str__`/`__print__` magic methods | `data-types.md`, `oop-magic-methods.md` |
| `gotchas.md` | Common pitfalls and edge cases | Implicit behaviors, parser quirks, footguns, expression-mode `break VALUE`, `@safe_loop`, `__init__` in mixins, `parent` resolution, `self` capture, `set_e()` external command behavior | All (reference) |

---

### 2.7 Language Reference (`docs/reference/`)

| File | Description | Topics | Dependencies |
|------|-------------|--------|--------------|
| `keywords.md` | Complete keyword reference | All keywords with descriptions and examples | None (reference) |
| `error-types.md` | Error hierarchy reference | All error types (`TypeError`, `IndexError`, `KeyError`, `ValueError`, `ArityError`, `DivisionByZero`, `IOError`, `FileNotFound`, `ConversionError`, `AttributeError`, `ImmutabilityError`, `IterationError`, `HashError`, `ParseError`, `LexerError`, `RecursionError`, `AccessError`, `NotImplementedError`, `AssertionError`, `BringError`, `ModuleError`, `RequireError`, `EnvError`, `ProcessError`, `CommandFailed`, `RuntimeError`) | `error-handling.md` |
| `builtin-index.md` | Alphabetical builtin index | All built-in functions with signatures and descriptions | `builtins.md` |

---

## 3. Folder Structure

```
docs/
├── README.md                           # Documentation home / navigation
├── getting-started.md                  # Quick start, installation, hello world
│
├── core/
│   ├── overview.md
│   ├── syntax-basics.md
│   ├── data-types.md
│   ├── variables-binding.md
│   ├── operators.md
│   ├── expressions.md
│   ├── control-flow.md
│   ├── functions.md
│   ├── lambdas.md
│   ├── error-handling.md
│   ├── pattern-matching.md
│   ├── raii.md
│   ├── generators.md
│   ├── async.md
│   ├── shell.md
│   ├── decorators.md
│   └── builtins.md
│
├── oop/
│   ├── oop-structs.md
│   ├── oop-classes.md
│   ├── oop-inheritance.md
│   ├── oop-mixins.md
│   ├── oop-interfaces.md
│   ├── oop-access-control.md
│   ├── oop-properties.md
│   ├── oop-magic-methods.md
│   └── oop-abstract.md
│
├── modules/
│   ├── modules-definition.md
│   ├── modules-import.md
│   ├── modules-resolution.md
│   └── modules-builtins.md
│
├── stdlib/
│   ├── stdlib-collections.md
│   ├── stdlib-hof.md
│   ├── stdlib-fs.md
│   ├── stdlib-path.md
│   ├── stdlib-env.md
│   ├── stdlib-process.md
│   ├── stdlib-math.md
│   └── stdlib-string.md
│
├── tooling/
│   ├── repl.md
│   ├── cli.md
│   ├── linting.md
│   ├── dialect-conversion.md
│   ├── module-indexing.md
│   ├── debugging.md
│   ├── kernel.md
│   └── terminal-gui.md
│
├── advanced/
│   ├── garbage-collection.md
│   ├── hash-system.md
│   ├── ordered-hash-table.md
│   ├── f-strings.md
│   └── gotchas.md
│
└── reference/
    ├── keywords.md
    ├── error-types.md
    └── builtin-index.md
```

---

## 4. Suggested Reading Order (Linear Path)

This is the recommended order for a reader to go from "hello world" to mastering the language. This also serves as the sidebar/index order.

| Order | File | Why this order? |
|-------|------|-----------------|
| 1 | `getting-started.md` | First contact: installation, hello world, basic REPL usage |
| 2 | `core/overview.md` | Philosophy and design goals |
| 3 | `core/syntax-basics.md` | Lexical structure: comments, newlines, indentation, identifiers, literals, `;` terminator |
| 4 | `core/data-types.md` | All primitive and compound types |
| 5 | `core/variables-binding.md` | Variables, assignment, immutability, destructuring |
| 6 | `core/operators.md` | All operators and precedence |
| 7 | `core/control-flow.md` | `if`, `for`, `while`, `loop`, `do...while`, `break`, `continue`, `give` |
| 8 | `core/expressions.md` | Expression forms (ternary, `if`/`for`/`while`/`loop`/`incase` expressions), `yield`, `await`, `spread` |
| 9 | `core/functions.md` | Function definitions, parameters, defaults, variadic, return types, `give`, closures, overloading |
| 10 | `core/lambdas.md` | Anonymous functions |
| 11 | `core/error-handling.md` | `try`/`catch`/`finally`, `throw`, error types, structured errors |
| 12 | `core/pattern-matching.md` | `incase`, `is`, `belong`, `bind`, guards |
| 13 | `core/raii.md` | `let...be`, `__enter__`, `__exit__` |
| 14 | `core/generators.md` | `yield`, lazy iteration, `for` over generators |
| 15 | `core/async.md` | `async fn`, `await`, awaiting generators |
| 16 | `core/shell.md` | `$cmd`, `\|>`, `&&`, `\|\|`, `set_e()`, external command fallback |
| 17 | `core/decorators.md` | `@` syntax, built-in decorators, user-defined decorators |
| 18 | `oop/oop-structs.md` | Structs, fields, methods |
| 19 | `oop/oop-classes.md` | Classes, `__init__`, `__str__`, `__print__`, instantiation, frozen instances |
| 20 | `oop/oop-inheritance.md` | Multiple inheritance, `parent`, MRO, abstract methods |
| 21 | `oop/oop-mixins.md` | Mixins, `with`, composition |
| 22 | `oop/oop-interfaces.md` | Interfaces, `implements`, contract enforcement |
| 23 | `oop/oop-access-control.md` | `public`, `protected`, `private`, `static` |
| 24 | `oop/oop-properties.md` | `get`/`set` properties |
| 25 | `oop/oop-magic-methods.md` | Operator overloading, magic methods |
| 26 | `oop/oop-abstract.md` | Abstract classes and methods |
| 27 | `modules/modules-definition.md` | `module`, `export`, `requires` |
| 28 | `modules/modules-import.md` | `bring`, aliases, `from`, `and`, `@eager` |
| 29 | `modules/modules-resolution.md` | `.xell_meta`, `__xelcache__`, search path, circular import |
| 30 | `modules/modules-builtins.md` | `threading` module |
| 31 | `stdlib/stdlib-collections.md` | Collection builtins |
| 32 | `stdlib/stdlib-hof.md` | Higher-order functions |
| 33 | `stdlib/stdlib-fs.md` | File system |
| 34 | `stdlib/stdlib-path.md` | Path utilities |
| 35 | `stdlib/stdlib-env.md` | Environment variables |
| 36 | `stdlib/stdlib-process.md` | Process management |
| 37 | `stdlib/stdlib-math.md` | Math functions |
| 38 | `stdlib/stdlib-string.md` | String functions |
| 39 | `tooling/repl.md` | REPL usage |
| 40 | `tooling/cli.md` | CLI commands |
| 41 | `tooling/linting.md` | Static analysis |
| 42 | `tooling/dialect-conversion.md` | Dialect system |
| 43 | `tooling/module-indexing.md` | `--make_module` |
| 44 | `tooling/debugging.md` | Debug & trace |
| 45 | `tooling/kernel.md` | Notebook kernel |
| 46 | `tooling/terminal-gui.md` | Terminal GUI |
| 47 | `advanced/f-strings.md` | String interpolation details |
| 48 | `advanced/hash-system.md` | Hash algorithms |
| 49 | `advanced/ordered-hash-table.md` | OrderedHashTable internals |
| 50 | `advanced/garbage-collection.md` | GC internals |
| 51 | `reference/keywords.md` | Reference: all keywords |
| 52 | `reference/error-types.md` | Reference: error hierarchy |
| 53 | `reference/builtin-index.md` | Reference: builtins |
| 54 | `advanced/gotchas.md` | Pitfalls and edge cases (read when comfortable) |
| 55 | `core/builtins.md` | Complete built-in reference (can be read anytime after basics) |

---

## 5. Undocumented Implicit Behaviors / Footguns (Needs `gotchas.md`)

These are features or behaviors that are not obvious from the keyword names and require explicit documentation:

| # | Behavior | Source Evidence | Severity |
|---|----------|-----------------|----------|
| 1 | **`break VALUE` only works in expression-mode loops** | `execFor` throws `RuntimeError` for `break VALUE` in statement-mode loops | ⚠️ High |
| 2 | **`@safe_loop` checks that the body contains a `break` at definition time** | `execLoop` checks `bodyContainsBreak()` and throws if no break found | ⚠️ Medium |
| 3 | **`__init__` in mixins is not allowed** | `execClassDef` throws `TypeError` if a mixin defines `__init__` | ⚠️ Medium |
| 4 | **`parent` binding inside methods depends on the class that defines the method, not the actual instance type** | `callUserFn` uses `parentClassDef` parameter set by the caller (`ownerClass->parents[0]`), not the instance's class | ⚠️ High |
| 5 | **Generator functions are detected by scanning the AST for `yield` before execution** | `containsYield()` traverses the AST in `execFnDef`; the `isGenerator` flag is set at definition time | ℹ️ Low |
| 6 | **`await` on a non-generator is a no-op (returns the value as-is)** | `evalAwait` checks `isGenerator()`; if not, returns the value unchanged | ℹ️ Low |
| 7 | **`@track` categories are parsed directly from the source, not validated at runtime** | `exec` for `TrackStmt` parses `var(fn, etc.)` categories; unknown categories are ignored | ℹ️ Low |
| 8 | **`@debug sample N` traces first N/2 and last N/2 iterations (not the middle)** | `SamplingState::shouldTrace` uses `headCount = sampleSize / 2` for leading and trailing samples | ⚠️ Medium |
| 9 | **`map` keys can be any hashable type, not just strings** | `XMap::set` uses `XObject` as key, `XObjectHash` hashes all hashable types | ⚠️ High |
| 10 | **`frozen_set` (`~{...}`) is a distinct type from `set`, not a syntax sugar** | Separate `FrozenSetLiteral` AST node, `XType::FROZEN_SET` runtime type | ⚠️ Medium |
| 11 | **`immutable` variables cannot be reassigned at runtime** | `Environment::set` checks `immutables_` and throws `ImmutabilityError` | ⚠️ High |
| 12 | **`throw` with a map uses the `"type"` key to dispatch to C++ error types** | `execThrow` reads `type` from the map and throws the corresponding C++ exception | ⚠️ Medium |
| 13 | **`throw` with an instance uses the instance's `typeName` and `message` field** | `execThrow` checks `inst.typeName` and `inst.fields["message"]` | ⚠️ Medium |
| 14 | **`catch` clauses match on error category name and also the parent `"Error"` type** | `execTryCatch` checks `typeName == category || typeName == "Error"` | ℹ️ Low |
| 15 | **`set_e()` mode makes external commands throw `CommandFailedError` on non-zero exit** | `shellState_.exitOnError` checked in builtin `run`/`run_capture` | ⚠️ High |
| 16 | **`$cmd` in statement mode prints output directly; in expression mode returns a list of lines** | `execExprStmt` special-cases `ShellCmdExpr` to print; `evalShellCmd` returns a list | ⚠️ High |
| 17 | **`for` loop with multiple sources zips them (shortest wins)** | `execFor` uses `iterCount = min(allItems[i].size())` | ⚠️ Medium |
| 18 | **`for` loop with a generator source is lazy; with list sources it's eager/materialized** | `execFor` has separate lazy (`isLazyIterableSource`) and materialized paths | ⚠️ High |
| 19 | **`parent` is only bound in methods that have a `self` parameter** | `callUserFn` checks `fn.params` for `"self"` before binding `parent` | ⚠️ Medium |
| 20 | **`parent` binding falls back to `self`'s class's parent if no explicit parent provided** | `callUserFn` uses `parentClassDef` if provided; otherwise looks at `self`'s `structDef->parents[0]` | ⚠️ Medium |
| 21 | **`class` instances inherit field defaults from all ancestors (allFields() merges)** | `XStructDef::allFields()` merges parent fields, child overrides parent | ℹ️ Low |
| 22 | **`struct` does NOT support inheritance; only `class` does** | `execClassDef` validates `parents` only for `isClass`; `execStructDef` ignores parents | ⚠️ High |
| 23 | **Interfaces cannot be instantiated** | `evalCall` throws `TypeError` if `def.isInterface` is true | ⚠️ Medium |
| 24 | **Abstract classes cannot be instantiated** | `evalCall` throws `TypeError` if `def.isAbstract` is true | ⚠️ Medium |
| 25 | **Mixins cannot be instantiated** | `evalCall` throws `TypeError` if `def.isMixin` is true | ⚠️ Medium |
| 26 | **`@dataclass` does NOT generate `__init__`; it uses struct-style field initialization** | `execDecoratedClassDef` sets `isDataclass` flag but no code generation | ⚠️ Medium |
| 27 | **`@immutable` freezes the instance after `__init__` completes** | `execCall` sets `frozen = true` after `callUserFn` returns | ℹ️ Low |
| 28 | **`@singleton` caches the instance after `__init__` completes** | `execCall` sets `defPtr->singletonInstance = result` after construction | ℹ️ Low |
| 29 | **`~Name(args...)` (frozen instance) still calls `__init__` before freezing** | `execCall` for `~` prefix calls `__init__` then sets `frozen = true` | ⚠️ Medium |
| 30 | **`bring * from "file"` imports all names from the file's global environment, not just exports** | `execBring` uses `childEnv.allNames()` and imports everything | ⚠️ High |
| 31 | **`bring * of module` with no alias checks for collisions and throws if any name already exists** | `execBring` iterates `mod->exports` and checks `hasLocal(name)` | ⚠️ Medium |
| 32 | **`requires` in a module definition auto-brings modules into the module's scope** | `execModuleDef` calls `moduleResolver_.getSessionCached()` and defines names in `modEnv` | ⚠️ Medium |
| 33 | **`module` body executes in a separate environment that is owned by the `XModule`** | `execModuleDef` creates `modEnvPtr` and stores it in `xmod->ownedEnv` | ℹ️ Low |
| 34 | **Exported modules become submodules (`mod->submodules`) with `parentName` tracking** | `execModuleDef` checks `val.isModule()` and sets `sub->parentName = xmod->name` | ℹ️ Low |
| 35 | **`requires` with `X, Y of path` resolves the path and imports specific items** | `execModuleDef` walks `path` through submodules and imports `items` | ℹ️ Low |
| 36 | **`.xell_meta` is optional; without it, `xell --make_module` falls back to slow-path scanning** | `ModuleResolver::resolveFromFileSystem` uses `readMetaIndex()` then `slowPathScan()` | ℹ️ Low |
| 37 | **`__xelcache__/` stores `.hash` and `.xelc` placeholder files** | `makeModule` writes `file.hash` and `file.xelc` with source hash and placeholder | ℹ️ Low |

---

## 6. Ambiguities / Areas Needing Clarification

These are areas where I had to infer behavior from the code and may have misinterpreted the intended design. Please clarify before I write the actual documentation:

| # | Area | Question | Code Reference |
|---|------|----------|----------------|
| 1 | **`parent` in inheritance** | Does `parent` refer to the immediate parent class, or the first parent that defines the method being called? The code suggests `ownerClass->parents[0]` (first parent of the defining class), but I'm not 100% sure. | `callUserFn` in `interpreter.cpp` |
| 2 | **`@debug sample N`** | The sampling algorithm traces first N/2 and last N/2 iterations. Is this intentional? What about the middle? | `SamplingState::shouldTrace` in `trace_collector.hpp` |
| 3 | **`@safe_loop`** | The check only verifies that a `break` exists somewhere in the body (ignoring nested loops). Is that sufficient? | `bodyContainsBreak` in `interpreter.cpp` |
| 4 | **`@dataclass`** | The code sets a flag but doesn't generate any code. Is this planned for the future, or is the flag used elsewhere? | `execDecoratedClassDef` in `interpreter.cpp` |
| 5 | **`@immutable`** | Does this apply to all instances of the class, or only instances created with the `~` prefix? The code suggests it freezes after `__init__` for all instances. | `execCall` in `interpreter.cpp` |
| 6 | **`with` (mixins)** | Is `with` order significant? The code doesn't show priority resolution — I assume mixins are searched after parents but before own methods? | `XStructDef::findMethod` in `xobject.hpp` |
| 7 | **`for` loop rest capture** | Does `...rest` capture remaining items from the source when destructuring? E.g., `for a, ...rest in list: ...` | `execFor` in `interpreter.cpp` |
| 8 | **`parallel iteration`** | Does `for a, b in list1, list2` zip the lists (shortest wins) or iterate until the longest ends? | `execFor` uses `iterCount = min` (shortest) |
| 9 | **`belong` in `incase`** | Does `belong` check type/class name matching via `matchesType()` which supports `isInstance()` and built-in type names like `"int"`, `"string"`? | `matchesType` in `interpreter.cpp` |
| 10 | **`bind` in `incase`** | Does `bind name` always match and bind the subject to the name, with the guard applied afterward? | `execInCase` in `interpreter.cpp` |
| 11 | **`throws` statement** | Can `throw` accept any value, or only strings/maps/instances? Code shows string, map, instance, and bare (re-throw) support. | `execThrow` in `interpreter.cpp` |
| 12 | **`bare throw`** | Is bare throw (re-throw) fully supported? It throws `RuntimeError` with a message about being unsupported, so it seems like a placeholder. | `execThrow` in `interpreter.cpp` |
| 13 | **`await` without a generator** | Returns the value as-is. Is this intentional (allows async-compatible functions)? | `evalAwait` in `interpreter.cpp` |
| 14 | **`for` loop with `break VALUE` in statement mode** | Throws `RuntimeError`. Is this intentional, or will it be supported in the future? | `execFor` in `interpreter.cpp` |
| 15 | **`set_e()`** | Is `set_e()` a built-in function or a runtime flag? I see `ShellState::exitOnError` but no built-in named `set_e` in the code. | `shell_state.hpp` |
| 16 | **`last_exit_code`** | Is this a built-in variable or a function? I see `ShellState::lastExitCode` but no built-in exposed. | `shell_state.hpp` |
| 17 | **`.xelc` cache** | The code writes placeholder `.xelc` files but doesn't actually load them. Is bytecode caching planned for a future version? | `makeModule` in `main.cpp` |
| 18 | **`XELL_PATH`** | The environment variable is read but the stdlib path isn't fully implemented. What should the default search path be? | `ModuleResolver::buildSearchPath` in `module_resolver.cpp` |
| 19 | **`@track` categories** | The code supports `loop`, `conditions`, `scope`, `imports`, `returns`, `calls`, `mutations`, `types`, `perf`, `recursion`. Is there a complete list? | `exec` for `TrackStmt` in `interpreter.cpp` |
| 20 | **`f-string` format specifiers** | The code parses `{expr:format}` but `applyFormatSpec` doesn't appear to be implemented. What format specifiers are supported? | `interpolate` in `interpreter.cpp` |
| 21 | **`--make_module` with nested modules** | The code collects nested modules recursively. How are nested module hierarchies represented in `.xell_meta`? | `writeModuleJson` in `main.cpp` |
| 22 | **`--update` in `--make_module`** | The code preserves unchanged entries from the existing `.xell_meta` by file hash. Is this correct? | `makeModule` in `main.cpp` |
| 23 | **`@breakpoint pause`** | Does `@breakpoint pause` block until the user presses "resume" (Tab), or does it just pause for a fixed time? | `exec` for `BreakpointStmt` in `interpreter.cpp` |

---

## Summary Statistics

| Category | File Count |
|----------|------------|
| Core Language | 16 |
| Object-Oriented | 9 |
| Module System | 4 |
| Standard Library | 8 |
| Tooling & LSP | 8 |
| Advanced | 5 |
| Reference | 3 |
| **Total** | **53** |

---

## Next Steps

1. **Clarify the ambiguities** listed in Section 6 (the author should answer these before documentation writing begins)
2. **Review the `gotchas.md` list** and confirm which behaviors should be explicitly documented as warnings
3. **Define the sidebar navigation** in the docs build tool (e.g., MkDocs, Docusaurus) following the suggested reading order
4. **Begin writing docs** in the order of the reading order (start with `getting-started.md`, then proceed sequentially)