# Decorators

This section documents every decorator form currently supported by Xell.

## 1) Function and class decorators (general)

You can stack decorators and they apply bottom-up.

```xell
@dec1
@dec2
fn work(x):
    give x * 2
;
```

## 2) Built-in class decorators

### `@dataclass`

```xell
@dataclass
class User:
    name = ""
    age = 0
;
```

### `@immutable`

```xell
@immutable
class FrozenConfig:
    mode = "prod"
;
```

### `@singleton`

```xell
@singleton
class AppState:
    count = 0
;
```

## 3) Import/loop statement decorators

### `@eager` on `bring` / `from`

```xell
@eager bring "db.xel"
```

### `@safe_loop` on `loop`

```xell
@safe_loop
loop:
    break;
;
```

## 4) Debug and tracing decorators

These are standalone statements (not only function/class wrappers).

### `@debug on` / `@debug off`

```xell
@debug on
x = 1
@debug off
y = 2
```

### `@debug sample N`

```xell
@debug on
@debug sample 10
for i in range(100):
    x = i
;
```

### `@breakpoint`

```xell
@debug on
@breakpoint
@breakpoint("after init")
@breakpoint("big value") when x > 100
@breakpoint pause
@breakpoint pause 3
```

### `@watch("expr")`

```xell
@debug on
@watch("loss < 0.01")
```

### `@checkpoint("name")`

```xell
@debug on
@checkpoint("epoch_start")
```

### `@track` / `@notrack`

```xell
@debug on
@track var(loss, acc) fn(train_step) class(Model) obj(model) loop perf
@notrack var(temp, i)
```

### `@profile fn name` and `@profile`

```xell
@debug on
@profile fn slow_fn

@profile
result = slow_fn(100)
```

### `@log`

```xell
@log "program started"
@log when x > 10 "x is {x}"
```

## 5) `@debug` as a function decorator

`@debug` can appear directly before a function definition.

```xell
@debug
fn critical(x):
    give x + 1
;
```

## 6) `@convert` dialect decorator

`@convert` is used with dialect conversion workflows.

```xell
@convert "my_dialect.xesy"
print("dialect-converted source")
```

See the dialect guide for details.
