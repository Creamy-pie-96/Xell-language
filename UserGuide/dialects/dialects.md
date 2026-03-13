# Dialects and `@convert`

This section explains how Xell supports custom syntax dialects through conversion.

## What `@convert` does

`@convert` applies a dialect conversion step so alternate syntax can be translated into canonical Xell.

```xell
@convert "math.xesy"
print("dialect syntax can be converted before parse")
```

## Typical workflow

1. Write a dialect mapping file (`.xesy`).
2. Add `@convert "your_dialect.xesy"` in the source.
3. Run normally; source is converted before main parsing.

```xell
@convert "dsl.xesy"
# dialect-friendly source follows
```

## Relationship to modules

`@convert` can be used alongside module imports.

```xell
@convert "team_style.xesy"
bring "utils.xel"
print("converted + imported")
```

## Best practices

- Keep one dialect per project or directory when possible.
- Version-control `.xesy` files with source.
- Prefer clear conversion rules over ambiguous replacements.

```xell
# good: explicit conversion mapping in dialect file
# avoid: overlapping keyword replacements
```

## Important note

Decorator syntax supports `@convert` in ecosystem tooling and workflows, and docs/plans in this repository treat it as the main dialect entrypoint.
