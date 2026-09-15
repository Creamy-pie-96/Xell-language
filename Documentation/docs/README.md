# Xell Language Documentation

Welcome to the official documentation for **Xell** – a modern, readable, and expressive scripting language.

Xell blends the clarity of Python with the performance of C++, offering:

- **Crystal‑clear syntax** – English‑like keywords and consistent rules.
- **Functional & OOP** – Functions, lambdas, classes, mixins, interfaces.
- **Built‑in safety** – Immutable bindings, RAII, and a cycle‑collecting GC.
- **First‑class modules** – Powerful import/export system with fast indexing.
- **Integrated shell** – Run commands, pipe, and check exit codes effortlessly.
- **Rich tooling** – REPL, linter, debugger, notebook kernel, and a full IDE.

---

## Getting Started

New to Xell? Start here:

- **[Getting Started](getting-started.md)** – Install, run the REPL, write “Hello, World!”.
- **[Overview](core/overview.md)** – A high‑level tour of the language.

---

## Core Language

| File | Description | Featured Topics |
|------|-------------|-----------------|
| [Overview](core/overview.md) | Philosophy and high‑level tour | Design goals, quick examples |
| [Syntax Basics](core/syntax-basics.md) | Lexical structure | Comments, newlines, keywords, literals, `;` terminator |
| [Data Types](core/data-types.md) | All primitive and compound types | `int`, `float`, `complex`, `string`, `bytes`, `list`, `tuple`, `set`, `frozen_set`, `map`, `none` |
| [Variables & Binding](core/variables-binding.md) | Variables and assignment | `let`, `immutable`, destructuring, scope, closures |
| [Operators](core/operators.md) | All operators | Arithmetic, comparison, logical, bitwise, shell (`|>`, `&&`, `||`), precedence |
| [Expressions](core/expressions.md) | All expression forms | Ternary, `if`/`for`/`while`/`loop`/`incase` expressions, `yield`, `await`, `spread` |
| [Control Flow](core/control-flow.md) | Statements that control execution | `if`/`elif`/`else`, `for` (destructuring, parallel, rest), `while`, `loop`, `do…while`, `break`/`continue` |
| [Functions](core/functions.md) | Function definitions and calls | `fn`, parameters (typed, default, variadic), return types, `give`, closures, overloading, `async fn` |
| [Lambdas](core/lambdas.md) | Anonymous functions | `x => expr`, `(a,b) => expr`, multi‑line lambdas |
| [Error Handling](core/error-handling.md) | Exceptions | `try`/`catch`/`finally`, `throw`, error hierarchy, structured errors |
| [Pattern Matching](core/pattern-matching.md) | InCase pattern matching | `incase`, `is`, `belong`, `bind`, guards, expression‑mode |
| [RAII](core/raii.md) | Context managers | `let … be`, `__enter__`/`__exit__`, resource cleanup |
| [Generators](core/generators.md) | Lazy sequences | `yield`, `for` over generators, `await` draining, `next()` |
| [Async](core/async.md) | Asynchronous programming | `async fn`, `await`, awaiting generators |
| [Shell](core/shell.md) | Shell integration | `$cmd`, `|>`, `&&`, `||`, `set_e()`, PATH fallback |
| [Decorators](core/decorators.md) | Function/class decorators | `@debug`, `@breakpoint`, `@watch`, `@track`, `@profile`, `@log`, `@checkpoint`, `@safe_loop`, `@eager`, `@dataclass`, `@immutable`, `@singleton` |
| [Built‑ins](core/builtins.md) | Core built‑in functions | `print`, `input`, `len`, `type`, `str`, `num`, `range`, `assert`, `hashme` |

---

## Object‑Oriented Programming

| File | Description | Featured Topics |
|------|-------------|-----------------|
| [Structs](oop/oop-structs.md) | Struct definitions | `struct`, fields, methods, positional/named construction |
| [Classes](oop/oop-classes.md) | Class definitions | `class`, `__init__`, `__str__`, `__print__`, instantiation, frozen instances (`~`) |
| [Inheritance](oop/oop-inheritance.md) | Inheritance | Multiple inheritance, `parent`, MRO, abstract methods |
| [Mixins](oop/oop-mixins.md) | Mixins | `mixin`, `with`, composition, no `__init__` |
| [Interfaces](oop/oop-interfaces.md) | Interfaces | `interface`, `implements`, contract validation |
| [Access Control](oop/oop-access-control.md) | Visibility and static members | `public`, `protected`, `private`, `static` |
| [Properties](oop/oop-properties.md) | Getters and setters | `get`, `set`, read‑only/write‑only properties |
| [Magic Methods](oop/oop-magic-methods.md) | Operator overloading | `__add__`, `__sub__`, `__iter__`, `__len__`, `__call__`, … |
| [Abstract](oop/oop-abstract.md) | Abstract classes and methods | `abstract`, abstract methods, implementation enforcement |

---

## Modules

| File | Description | Featured Topics |
|------|-------------|-----------------|
| [Defining Modules](modules/modules-definition.md) | Defining modules | `module`, `export`, `requires`, first‑class modules, dunders |
| [Importing Modules](modules/modules-import.md) | Importing modules | `bring`, aliases, `from`, `and` chaining, `@eager` |
| [Module Resolution](modules/modules-resolution.md) | Module resolution | `.xell_meta`, `__xelcache__`, search path, circular import detection |
| [Built‑in Modules](modules/modules-builtins.md) | Built‑in modules | `threading` (threads, mutexes) |

---

## Standard Library

| File | Description | Featured Functions |
|------|-------------|-------------------|
| [Collections](stdlib/stdlib-collections.md) | Collection builtins | `push`, `pop`, `sort`, `keys`, `values`, `set`, `has`, `unique`, `zip` |
| [Higher‑Order](stdlib/stdlib-hof.md) | Higher‑order functions | `map`, `filter`, `reduce`, `any`, `all` |
| [File System](stdlib/stdlib-fs.md) | File system operations | `mkdir`, `rm`, `read`, `write`, `exists`, `stat`, `find`, `tree` |
| [Path](stdlib/stdlib-path.md) | Path utilities | `cwd`, `cd`, `abspath`, `basename`, `dirname`, `ext`, `join_path` |
| [Environment](stdlib/stdlib-env.md) | Environment variables | `env_get`, `env_set`, `env_unset`, `env_has`, `__args__` |
| [Process](stdlib/stdlib-process.md) | Process management | `run`, `run_capture`, `pid`, `sleep`, `exit`, `set_e()`, `exit_code` |
| [Math](stdlib/stdlib-math.md) | Math functions | `PI`, `E`, `floor`, `ceil`, `round`, `abs`, `sqrt`, `pow`, `sin`, `cos`, `random` |
| [String](stdlib/stdlib-string.md) | String functions | `upper`, `lower`, `trim`, `replace`, `split`, `join`, `substr`, `pad_start` |

---

## Tooling

| File | Description | Featured Commands |
|------|-------------|------------------|
| [REPL](tooling/repl.md) | Interactive REPL usage | Multi‑line input, history, shell commands, debug decorators |
| [CLI](tooling/cli.md) | Command‑line interface | `xell <file>`, `-e`, `--check`, `--convert`, `--make_module`, `--terminal`, `--kernel` |
| [Linting](tooling/linting.md) | Static analysis | `--check`, `--lint`, `--check-string`, `--check-symbols`, diagnostics |
| [Dialect Conversion](tooling/dialect-conversion.md) | Dialect system | `@convert`, `.xesy` mapping, `--convert`, `--revert`, `--gen_xesy` |
| [Module Indexing](tooling/module-indexing.md) | Module indexing | `--make_module [--update]`, `.xell_meta`, `__xelcache__` |
| [Debugging](tooling/debugging.md) | Debug & trace | `--trace-vars`, `--debug`, `@breakpoint`, `@watch`, `@profile`, `@log`, `@track` |
| [Notebook Kernel](tooling/kernel.md) | Notebook kernel mode | `--kernel`, JSON protocol, cell execution |
| [Terminal GUI](tooling/terminal-gui.md) | Terminal & IDE | `--terminal`, `--ide`, `--customize`, Terminal Mode, IDE Mode |

---

## Advanced Topics

| File | Description | Featured Topics |
|------|-------------|-----------------|
| [F‑Strings](advanced/f-strings.md) | String interpolation | `f"..."`, format specifiers, nesting, escaping, `__str__` |
| [Hash System](advanced/hash-system.md) | Hash algorithms | `fnv1a`, `djb2`, `murmur3`, `siphash`, SHA‑256, `__hash__` |
| [Ordered Hash Table](advanced/ordered-hash-table.md) | OrderedHashTable internals | Insertion‑order iteration, no tombstones, separate chaining, free‑list recycling |
| [Garbage Collection](advanced/garbage-collection.md) | GC internals | Reference counting, cycle collector, trial‑deletion algorithm, container tracking |
| [Gotchas & Pitfalls](advanced/gotchas.md) | Common mistakes and edge cases | `break VALUE`, `@safe_loop`, `parent` resolution, bare `throw`, `belong` vs `is` |

---

## Reference

| File | Description |
|------|-------------|
| [Keywords](reference/keywords.md) | Complete alphabetical keyword list |
| [Error Types](reference/error-types.md) | All exception classes |
| [Built‑in Index](reference/builtin-index.md) | Alphabetical built‑in function reference (signatures, modules, examples) |

---

## Suggested Reading Order

For a linear path from zero to hero, follow the **“Next”** links at the bottom of each page, starting with **[Getting Started](getting-started.md)**.

If you prefer to jump around, the table above gives you a full map of all topics.

---

## Contributing

Xell is open source. For issues, feature requests, or to contribute, visit the [GitHub repository](https://github.com/Creamy-pie-96/Xell-language).