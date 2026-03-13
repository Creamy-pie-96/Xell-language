# Builtins and Standard Modules

This section explains Xell builtins by module, with practical examples.

## Core always-available modules (Tier 1)

### `io` (`print`, `input`, `exit`)

```xell
name = input("Name: ")
print("Hello {name}")
```

### `math` (numeric helpers)

Examples: `abs`, `sqrt`, `pow`, `round`, `random`, `gcd`, `lcm`, `sin`, `cos`.

```xell
print(abs(-4))
print(sqrt(81))
print(random_int(10))
```

### `type` (type and conversion)

Examples: `type`, `str`, `int`, `float`, `complex`, `magnitude`, `real`, `imag`.

```xell
z = complex(3, 4)
print(type(z))
print(magnitude(z))
```

### `collection` (generic containers)

Examples: `len`, `contains`, `keys`, `values`, `set`, `range`, `pop`, `push`.

```xell
nums = [1, 2, 3]
push(nums, 4)
print(len(nums))
```

### `string` (text processing)

Examples: `split`, `join`, `trim`, `upper`, `lower`, `replace`, `substr`, `starts_with`.

```xell
parts = split("a,b,c", ",")
print(join(parts, "|"))
```

### `list`

Examples: `sum`, `avg`, `min`, `max`, `slice`, `zip`, `zip_longest`, `flatten`, `unique`.

```xell
vals = [1, 2, 3, 4]
print(sum(vals))
print(avg(vals))
```

### `map`

Examples: `get`, `entries`, `merge`, `delete_key`, `from_entries`.

```xell
cfg = {host: "localhost"}
print(get(cfg, "host", "none"))
```

### `bytes`

Examples: `bytes`, `encode`, `decode`, `byte_at`, `byte_len`, `bytes_slice`.

```xell
b = encode("hello")
print(byte_len(b))
print(decode(b))
```

### `generator`

Examples: `next`, `is_exhausted`, `gen_collect`.

```xell
fn g():
    yield 1
    yield 2
;

it = g()
print(next(it))
```

### `os`

Examples: `cwd`, `cd`, `ls`, `exists`, `is_file`, `is_dir`, `read`, `write`, `run`, `run_capture`.

```xell
print(cwd())
print(ls("."))
```

### `hash`

Examples: `hash`, `hash_seed`, `is_hashable`.

```xell
print(hash("abc"))
```

### `shell`

Examples: `alias`, `which`, `whereis`, `history`, `clear`, `reset`, `type_cmd`.

```xell
print(which("python"))
```

### `casting`

Examples include conversion helpers like `Int`, `Float`, `Bool`, `String`, `List`, `Set`, `Tuple`, `Complex`, and `auto`.

```xell
x = Int("42")
s = String(99)
print(x, s)
```

### Core higher-order functions

`map`, `filter`, `reduce`, `any`, `all` are interpreter-provided.

```xell
vals = [1, 2, 3, 4]
print(filter(x => x % 2 == 0, vals))
print(reduce((a, b) => a + b, vals, 0))
```

## Bring-required modules (Tier 2)

### `datetime`

```xell
bring "datetime"
print(timestamp())
print(format_date(now(), "%Y-%m-%d"))
```

### `regex`

```xell
bring "regex"
print(regex_match("hello123", "[a-z]+[0-9]+"))
```

### `fs`

```xell
bring "fs"
print(find(".", "*.xel"))
```

### `textproc`

```xell
bring "textproc"
print(head("README.md", 5))
```

### `process`

```xell
bring "process"
print(ps())
print(hostname())
```

### `sysmon`

```xell
bring "sysmon"
print(cpu_count())
print(mem_total())
```

### `net`

```xell
bring "net"
print(http_get("https://example.com"))
```

### `archive`

```xell
bring "archive"
zip_archive("out.zip", ["a.txt", "b.txt"])
```

### `json`

```xell
bring "json"
obj = json_parse("{\"x\":1}")
print(json_stringify(obj))
```

### `threading`

```xell
bring "threading"
t = thread_spawn(fn(): give 123 ;)
print(thread_join(t))
```

## Shell command operator `$`

`$` is a language-level shell execution operator.

```xell
$ ls -la
lines = $ echo "hello"
print(lines)
```

## Constants

```xell
print(PI)
print(E)
print(INF)
```
