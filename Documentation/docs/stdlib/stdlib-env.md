# Standard Library – Environment Variables

> **Previous:** [Path Utilities](stdlib-path.md)  
> **Next:** [Process Management](stdlib-process.md)

---

Xell provides a set of functions for interacting with the operating system's environment variables. These are always available (Tier 1) and allow you to read, set, and list environment variables, as well as access command‑line arguments passed to your script.

---

## Quick Reference

| Function | What it does | Example |
|----------|--------------|---------|
| `env_get(name)` | Gets an environment variable (throws if missing) | `env_get("HOME")` → `"/home/alice"` |
| `env_set(name, value)` | Sets an environment variable | `env_set("DEBUG", "true")` |
| `env_unset(name)` | Unsets an environment variable | `env_unset("TEMP")` |
| `env_has(name)` | Checks if an environment variable exists | `env_has("PATH")` → `true` |
| `export_env(name, value)` | Alias for `env_set()` | `export_env("MY_VAR", "42")` |
| `set_env(name, value)` | Alias for `env_set()` | `set_env("LOG_LEVEL", "debug")` |
| `env_list()` | Returns all environment variables as a map | `env_list()` → `{"HOME": "/home", "PATH": "/bin"}` |
| `printenv([name])` | Returns a specific env var or all as a map | `printenv("HOME")` → `"/home/alice"` |
| `__args__` (global) | List of command‑line arguments passed to the script | `__args__[0]` → `"--verbose"` |

---

## Getting Environment Variables

### `env_get(name)`

Returns the value of an environment variable. Throws `EnvError` if the variable is not set.

```xell
let home = env_get("HOME")
print("Home directory: " + home)
```

**Output:**
```
Home directory: /home/alice
```

### `env_has(name)`

Checks whether an environment variable exists. Returns `true` or `false`. Does not throw.

```xell
if env_has("DEBUG"):
    print("Debug mode enabled")
else:
    print("Debug mode disabled")
;
```

### `printenv(name)`

Returns the value of an environment variable, or `none` if it is not set (does not throw). If called with no arguments, returns a map of all environment variables.

```xell
let path = printenv("PATH")
if path is none:
    print("PATH is not set")   # unlikely
else:
    print("PATH: " + path)
;

let all_vars = printenv()
print(all_vars)   # {"HOME": "/home", "PATH": "/bin", ...}
```

---

## Setting and Unsetting Environment Variables

### `env_set(name, value)`

Sets an environment variable to the given value. Returns `none`. This affects the current process and any child processes spawned after the call.

```xell
env_set("XELL_MODE", "production")
```

### `export_env(name, value)`

Alias for `env_set()`. Named after the shell `export` command.

```xell
export_env("LOG_LEVEL", "debug")
```

### `set_env(name, value)`

Another alias for `env_set()`.

```xell
set_env("TEMP_DIR", "/tmp/xell")
```

### `env_unset(name)`

Removes an environment variable. Returns `none`.

```xell
env_unset("TEMP_VAR")
```

---

## Listing All Environment Variables

### `env_list()`

Returns a map containing all environment variables (keys and values). This is equivalent to `printenv()` with no arguments.

```xell
let all = env_list()
for [key, val] in entries(all):
    print(key + " = " + val)
;
```

**Output (truncated):**
```
HOME = /home/alice
PATH = /usr/bin:/bin
SHELL = /bin/bash
...
```

---

## Command‑Line Arguments: `__args__`

When you run a Xell script, the arguments passed after the script name are available in the global variable `__args__`. This is a list of strings.

```bash
xell script.xel --verbose --config config.xel
```

```xell
print(__args__)   # ["--verbose", "--config", "config.xel"]
```

### Accessing Arguments

```xell
let verbose = false
let config_file = "default.xel"

for i in range(len(__args__)):
    let arg = __args__[i]
    if arg == "--verbose":
        verbose = true
    elif arg == "--config" and i + 1 < len(__args__):
        config_file = __args__[i + 1]
    ;
;

print("Verbose: " + str(verbose))
print("Config: " + config_file)
```

### `__file__` and `__name__`

In addition to `__args__`, Xell also provides:

- `__file__` – the absolute path of the current script.
- `__name__` – `"__main__"` if the script is the main entry point, otherwise the module name.

These are automatically injected by the interpreter.

---

## Complete Example: Configuration Loader

```xell
# config_loader.xel
# Usage: xell config_loader.xel --env production

# Read environment variable for mode, fallback to CLI arg
let mode = printenv("APP_MODE")
if mode is none:
    # Check for --env flag
    for i in range(len(__args__)):
        if __args__[i] == "--env" and i + 1 < len(__args__):
            mode = __args__[i + 1]
            break
    ;
;

if mode is none:
    mode = "development"
;

print("Running in " + mode + " mode")

# Set configuration based on mode
if mode == "production":
    env_set("LOG_LEVEL", "error")
    env_set("DB_URL", "postgres://prod:5432/db")
elif mode == "staging":
    env_set("LOG_LEVEL", "warning")
    env_set("DB_URL", "postgres://staging:5432/db")
else:
    env_set("LOG_LEVEL", "debug")
    env_set("DB_URL", "postgres://localhost:5432/db")
;

print("LOG_LEVEL = " + env_get("LOG_LEVEL"))
print("DB_URL = " + env_get("DB_URL"))
```

**Running with defaults:**
```
xell config_loader.xel
Running in development mode
LOG_LEVEL = debug
DB_URL = postgres://localhost:5432/db
```

**Running with CLI override:**
```
xell config_loader.xel --env production
Running in production mode
LOG_LEVEL = error
DB_URL = postgres://prod:5432/db
```

---

## What NOT to Do

### 1. Using `env_get()` Without Catching Missing Variables

```xell
let temp = env_get("TEMP")   # ❌ EnvError if TEMP is not set
```

**Fix:** Use `env_has()` to check first, or use `printenv()` which returns `none` for missing variables.

### 2. Assuming Environment Variables Are Case‑Sensitive on Windows

Windows environment variables are **case‑insensitive**, while on Unix they are case‑sensitive. Xell preserves case but the OS may not. Use consistent casing for portability.

```xell
# This works on both, but is better to use uppercase consistently
env_set("PATH", "/custom/bin")
print(env_get("PATH"))   # OK on both
```

### 3. Expecting `env_set()` to Persist Beyond the Process

Environment variables set with `env_set()` are only visible to the current process and its children. They are not written to the system's permanent environment.

```xell
env_set("MY_VAR", "hello")
# This does NOT modify the user's shell environment
```

### 4. Using `printenv()` Without Arguments and Assigning to a Variable

`printenv()` with no arguments returns a map, which can be large. Be mindful of memory usage.

### 5. Forgetting That `__args__` Is Only Available in Scripts

`__args__` is only populated when running a script file. In the REPL, `__args__` is `none` (unless manually defined).

```xell
# In REPL
print(__args__)   # none
```

### 6. Modifying `__args__` In‑Place

`__args__` is a list; you can modify it, but it will not affect the actual command‑line arguments (they are read‑only at startup). Modifying it is generally not recommended.

```xell
push(__args__, "--extra")   # allowed but pointless
```

---

## See Also

- [Variables & Binding](../core/variables-binding.md) – `__args__` and `__file__`
- [Process Management](stdlib-process.md) – `run`, `run_capture`, `pid`
- [CLI](../tooling/cli.md) – command‑line flags and options

---

**Previous:** [Path Utilities](stdlib-path.md)  
**Next:** [Process Management](stdlib-process.md)