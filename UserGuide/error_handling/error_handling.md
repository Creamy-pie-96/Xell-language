# Error Handling and Debugging

This section teaches runtime error handling and the built-in debug workflow.

## try / catch / finally

```xell
try:
    x = 10 / 0
catch e is ZeroDivisionError:
    print("cannot divide by zero")
catch e:
    print("other error: {e}")
finally:
    print("cleanup")
;
```

## throw

```xell
fn require_positive(n):
    if n <= 0:
        throw "n must be positive"
    ;
    give n
;
```

## Common error classes

```xell
try:
    val = int("abc")
catch e is ValueError:
    print("bad number")
;
```

Typical classes include `RuntimeError`, `TypeError`, `IndexError`, `FileNotFoundError`, `IOError`, `ArityError`, `HashError`, `ZeroDivisionError`, `ImmutableError`, `AssertionError`, and `ValueError`.

## Assertion

```xell
x = 5
assert(x > 0, "x must be positive")
```

## Debug tracing with decorators

### Toggle tracing

```xell
@debug on
x = 1
@debug off
```

### Breakpoints and watches

```xell
@debug on
@breakpoint("start")
@watch("x > 10")
```

### Tracking filters

```xell
@debug on
@track var(x, y) fn(run) perf
@notrack var(temp)
```

### Profiling and logs

```xell
@debug on
@profile fn heavy
@log "starting heavy()"
```

## CLI debug mode

Run with debug server mode:

```xell
# shell command
# xell --debug my_file.xel
```

In IDE mode, stepping/breakpoint controls are available during debug sessions.
