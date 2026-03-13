# Audit: Missing or Underdocumented Features

This page summarizes gaps found after reviewing `LANGUAGE_FEATURES.md`, `VERIFIED_LANGUAGE_FEATURES.md`, and parser/interpreter sources.

## 1) Decorators that were underdocumented

The original feature docs did not fully document all debug decorators and statement decorators.

### Fully supported debug/trace decorators

```xell
@debug on
@debug sample 10
@breakpoint("checkpoint")
@watch("x > 5")
@checkpoint("state-A")
@track var(x) fn(run) loop
@notrack var(i)
@profile fn run
@log "running"
@debug off
```

Supported forms from parser:

- `@debug on`, `@debug off`, `@debug sample N`, and `@debug` before a function definition
- `@breakpoint`, `@breakpoint("name")`, `@breakpoint("name") when EXPR`, `@breakpoint pause`, `@breakpoint pause N`
- `@watch("expr")`
- `@checkpoint("name")`
- `@track ...` and `@notrack ...` with categories and grouped targets
- `@profile fn name` and `@profile` (profile next statement)
- `@log "message"` and `@log when EXPR "message"`

### Statement decorators also supported

```xell
@safe_loop
loop:
    # protected loop behavior
;

@eager bring "my_module.xel"
```

- `@safe_loop` applies to `loop` statements.
- `@eager` applies to `bring`/`from` imports.

### Class decorators supported in interpreter

```xell
@dataclass
@immutable
@singleton
class Config:
    host = "localhost"
;
```

- Built-in class decorators: `@dataclass`, `@immutable`, `@singleton`
- User-defined decorators are also supported for functions/classes.

## 2) Builtins inventory differences

`LANGUAGE_FEATURES.md` lists broad builtin categories, but `VERIFIED_LANGUAGE_FEATURES.md` provides a stricter source-verified inventory (393 callables + constants).

Newly important/underdocumented areas:

- `casting` module (for smart casts and conversion helpers)
- `threading` module (thread and mutex builtins)
- Some module names differ (`net` in verified inventory vs `network` wording in prose)

Example using verified modules:

```xell
bring "threading"
thread = thread_spawn(fn(): give 42 ;)
result = thread_join(thread)
print(result)
```

## 3) Keyword/source alignment notes

The lexer keyword table confirms support for:

```xell
incase value:
    belong int: print("int") ;
    bind v if v > 10: print(v) ;
    else: print("other") ;
;
```

Notable keywords that should stay visible in docs:

- `belong`, `bind` (pattern matching)
- `of` (comparison/type context)
- `enum`, `yield`, `async`, `await`

## 4) Practical takeaway

Use this modular guide as the beginner path, and treat `VERIFIED_LANGUAGE_FEATURES.md` as the canonical source-verified inventory when you need exact builtin names.
