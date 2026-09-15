# REPL – Interactive Shell

> **Previous:** [String Functions](../stdlib/stdlib-string.md)  
> **Next:** [CLI Commands](cli.md)

---

The Xell REPL (Read‑Eval‑Print Loop) is an interactive environment for experimenting with Xell code. It's the quickest way to try out expressions, test functions, explore the language, and debug snippets without writing a full script.

---

## Quick Reference

| Feature | What it does | Example |
|---------|--------------|---------|
| `xell` | Starts the REPL | `$ xell` |
| Multi-line input | Continues input with `\` at end of line | `fn add(a,b): \` |
| History | Command history (up/down arrows) | Press `↑` to recall previous commands |
| Expression evaluation | Evaluates and prints the result | `42 * 2` → `84` |
| Statement execution | Executes statements, no output unless `print` | `let x = 10` (no output) |
| Shell integration | Run shell commands with `$` | `$ls` |
| `@profile` | Profiles the next expression | `@profile fib(30)` |
| `@track` | Tracks variables in the REPL session | `@track var(x)` |
| `exit()` / `Ctrl‑D` | Exits the REPL | `exit()` |

---

## Starting the REPL

Start the REPL by running `xell` with no arguments:

```bash
$ xell
Xell v0.1.0
>
```

The prompt `>` indicates that Xell is ready to accept input.

---

## Basic Usage

### Expressions

Type any expression and press Enter. The result is printed automatically.

```
> 42 + 5
47

> PI
3.141592653589793

> [1, 2, 3] |> map(x => x * 2)
[2, 4, 6]
```

### Statements

Statements are executed, but they don't produce output unless they use `print`.

```
> let x = 10
> print(x)
10

> if x > 5 : print("x is big") ;
x is big
```

### Assignments

Variables defined in the REPL persist across commands.

```
> let name = "Alice"
> print("Hello, " + name)
Hello, Alice

> let greet = (n) => "Hello, " + n
> greet("Bob")
Hello, Bob
```

---

## Multi‑line Input

For multi‑line statements (function definitions, `if` blocks, etc.), use `\` at the end of a line to continue.

```
> fn factorial(n): \
    if n <= 1 : give 1 \
    give n * factorial(n - 1) \
;
> factorial(5)
120
```

**Important:** The `\` must be the **last** character on the line (even after whitespace). The REPL will show a continuation prompt (`... `) on subsequent lines.

---

## Command History

The REPL maintains a history of all commands entered during the session.

- **Up arrow** (`↑`): Recall the previous command.
- **Down arrow** (`↓`): Recall the next command (if you've scrolled back).

History is **not** persisted across sessions (it lives only in memory).

---

## REPL‑Specific Decorators

Some decorators work especially well in the REPL for interactive debugging and profiling.

### `@profile`

Profiles the next expression and prints the execution time.

```
> @profile factorial(30)
[PROFILE] <expr>: 0.042ms
```

### `@track`

Tracks variable changes in the REPL session.

```
> let x = 10
> @track var(x)
> x = 20
[TRACK] x changed from 10 to 20
```

---

## Shell Integration in the REPL

You can run shell commands directly from the REPL using the `$` prefix.

```
> $ls -la
total 42
-rw-r--r-- 1 user user 1234 Jan 1 10:00 file.txt
...

> $git status
On branch main
Your branch is up to date with 'origin/main'.
...
```

This is the same shell integration as in scripts – see [Shell Integration](../core/shell.md) for details.

---

## Debugging in the REPL

You can use debug decorators in the REPL just as you would in a script.

```
> @debug on
> @breakpoint("check")
> let result = complex_calculation()
[BREAKPOINT] check at line 1
> 
```

When a `@breakpoint` is hit, the REPL pauses and you can inspect variables. Press `Enter` to continue.

See [Debugging](debugging.md) for full details.

---

## Exiting the REPL

Exit the REPL with:

- `exit()` or `exit`
- `Ctrl‑D` (Unix/Linux/macOS) or `Ctrl‑Z` (Windows)

```
> exit()
```

---

## Complete Example Session

```
$ xell
Xell v0.1.0
> let numbers = [1, 2, 3, 4, 5]
> map(numbers, x => x * 2)
[2, 4, 6, 8, 10]
> filter(numbers, x => x % 2 == 0)
[2, 4]
> reduce(numbers, (acc, x) => acc + x, 0)
15
> fn square(x): give x * x ;
> square(5)
25
> $date
Thu 23 Jul 2026 10:00:00 AM UTC
> exit()
```

---

## What NOT to Do

### 1. Forgetting the Semicolon in the REPL

```
> let x = 10   # ← no semicolon, this is a valid expression that prints "10"
```

In the REPL, `let x = 10` is parsed as an expression and prints `10`. If you want it to be a statement, add a semicolon.

### 2. Using `\` for Multi‑line Input Without a Space

```
> fn add(a,b):\
    give a + b    # ❌ error – `\` must be at the end of the line
;
```

### 3. Expecting History to Persist Across Sessions

The REPL history is in‑memory only. If you exit and restart, the history is lost.

### 4. Using `@debug on` Without Enabling the Trace Collector

The trace collector must be enabled at startup for debug decorators to work. In the REPL, it's enabled by default.

### 5. Confusing the REPL with the Interpreter's `run()`

The REPL is the interactive shell. The `run()` built‑in executes external commands, not Xell code.

### 6. Running Long‑Running Commands Without `$`

```
> sleep(5000)   # ❌ blocks the REPL for 5 seconds
```

Use `$sleep 5` if you want to run a shell command instead.

### 7. Using `exit()` in a Script and Expecting It to Work in the REPL

`exit()` works in both contexts, but in the REPL it exits the REPL process entirely.

---

## See Also

- [CLI Commands](cli.md) – all command‑line flags and options
- [Debugging](debugging.md) – `@breakpoint`, `@watch`, `@profile` in the REPL
- [Shell Integration](../core/shell.md) – `$cmd`, `|>`, `&&`, `||`
- [Built‑ins Reference](../core/builtins.md) – functions available in the REPL

---

**Previous:** [String Functions](../stdlib/stdlib-string.md)  
**Next:** [CLI Commands](cli.md)