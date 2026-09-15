# Built‑in Index – Complete Alphabetical Reference

> **Previous:** [Error Types](error-types.md)  
> **Next:** [Gotchas & Pitfalls](../advanced/gotchas.md)

---

This page provides an **alphabetical reference** for every built‑in function in Xell. Each entry includes:

- **Signature** – parameter names and types (where applicable).
- **Module** – the module that provides it (`"core"` for always‑available, or the Tier 2 module name).
- **Description** – what the function does.
- **Minimal example** – a quick usage snippet.

> **Note:** Tier 1 functions are always available. Tier 2 functions require `bring * from "module"` before use.

---

## A

### `abs(x)`
- **Module:** `math`
- **Signature:** `abs(number) -> number`
- **Description:** Returns the absolute value of `x`. Works on `int`, `float`, and `complex` (returns magnitude for complex).
- **Example:** `abs(-5) → 5`

### `abspath(path)`
- **Module:** `os`
- **Signature:** `abspath(path: string) -> string`
- **Description:** Returns the absolute (canonical) path of `path`, resolving `.` and `..`.
- **Example:** `abspath(".") → "/home/user/project"`

### `acos(x)`
- **Module:** `math`
- **Signature:** `acos(x: number) -> float`
- **Description:** Returns the arc cosine of `x` (in radians). Domain: `[-1, 1]`.
- **Example:** `acos(0) → 1.5707963267948966`

### `acosh(x)`
- **Module:** `math`
- **Signature:** `acosh(x: number) -> float`
- **Description:** Returns the inverse hyperbolic cosine of `x`. Domain: `[1, ∞)`.
- **Example:** `acosh(2) → 1.3169578969248166`

### `acot(x)`
- **Module:** `math`
- **Signature:** `acot(x: number) -> float`
- **Description:** Returns the arc cotangent of `x`.
- **Example:** `acot(1) → 0.7853981633974483`

### `acoth(x)`
- **Module:** `math`
- **Signature:** `acoth(x: number) -> float`
- **Description:** Returns the inverse hyperbolic cotangent of `x`. Domain: `|x| > 1`.
- **Example:** `acoth(2) → 0.5493061443340549`

### `add(set, elem)`
- **Module:** `collection`
- **Signature:** `add(set: set, elem) -> none`
- **Description:** Adds `elem` to `set`. Throws `HashError` if `elem` is not hashable.
- **Example:** `add({1,2}, 3) → {1,2,3}`

### `alias(name, cmd)`
- **Module:** `shell`
- **Signature:** `alias(name: string, cmd: string) -> none`
- **Description:** Defines a shell alias `name` for command `cmd`. With no args, returns all aliases as a map.
- **Example:** `alias("ll", "ls -la")`

### `all(list, fn)`
- **Module:** `core` (higher‑order)
- **Signature:** `all(list: list, fn: function) -> bool`
- **Description:** Returns `true` if `fn` returns `true` for **all** elements in `list`. Short‑circuits.
- **Example:** `all([1,2,3], x => x > 0) → true`

### `any(list, fn)`
- **Module:** `core` (higher‑order)
- **Signature:** `any(list: list, fn: function) -> bool`
- **Description:** Returns `true` if `fn` returns `true` for **any** element in `list`. Short‑circuits.
- **Example:** `any([1,2,3], x => x > 2) → true`

### `append(path, content)`
- **Module:** `os`
- **Signature:** `append(path: string, content: string) -> none`
- **Description:** Appends `content` to the file at `path` (creates if not exists).
- **Example:** `append("log.txt", "entry\n")`

### `arch()`
- **Module:** `process`
- **Signature:** `arch() -> string`
- **Description:** Returns the system architecture (`"x86_64"`, `"arm64"`, etc.).
- **Example:** `arch() → "x86_64"`

### `asin(x)`
- **Module:** `math`
- **Signature:** `asin(x: number) -> float`
- **Description:** Returns the arc sine of `x` (in radians). Domain: `[-1, 1]`.
- **Example:** `asin(1) → 1.5707963267948966`

### `asinh(x)`
- **Module:** `math`
- **Signature:** `asinh(x: number) -> float`
- **Description:** Returns the inverse hyperbolic sine of `x`.
- **Example:** `asinh(1) → 0.881373587019543`

### `assert(condition[, message])`
- **Module:** `util`
- **Signature:** `assert(condition: bool, message: string = "") -> none`
- **Description:** Throws `AssertionError` if `condition` is `false`.
- **Example:** `assert(x > 0, "x must be positive")`

### `atan(x)`
- **Module:** `math`
- **Signature:** `atan(x: number) -> float`
- **Description:** Returns the arc tangent of `x` (in radians).
- **Example:** `atan(1) → 0.7853981633974483`

### `atan2(y, x)`
- **Module:** `math`
- **Signature:** `atan2(y: number, x: number) -> float`
- **Description:** Returns the two‑argument arc tangent of `y` and `x` (in radians).
- **Example:** `atan2(1, 1) → 0.7853981633974483`

### `atanh(x)`
- **Module:** `math`
- **Signature:** `atanh(x: number) -> float`
- **Description:** Returns the inverse hyperbolic tangent of `x`. Domain: `(-1, 1)`.
- **Example:** `atanh(0.5) → 0.5493061443340549`

### `auto(x)`
- **Module:** `casting`
- **Signature:** `auto(x: any) -> any`
- **Description:** Auto‑casts a value to the best Xell type. For strings, parses numbers, bools, lists, tuples, sets.
- **Example:** `auto("42") → 42` (int)

### `avg(list)`
- **Module:** `list`
- **Signature:** `avg(list: list) -> float`
- **Description:** Returns the average of all numbers in `list`. Throws if the list is empty.
- **Example:** `avg([1,2,3]) → 2.0`

### `await` (expression)
- **Module:** `core` (language)
- **Signature:** `await generator -> any`
- **Description:** Waits for a generator to finish and returns its final value. Can only be used inside `async fn`.
- **Example:** `let result = await long_running()`

---

## B

### `basename(path)`
- **Module:** `os`
- **Signature:** `basename(path: string) -> string`
- **Description:** Returns the last component of a path (the filename).
- **Example:** `basename("/home/user/file.txt") → "file.txt"`

### `belong` (keyword in `incase`)
- **Module:** `core` (language)
- **Description:** Type check in `incase`. See [Pattern Matching](../core/pattern-matching.md).

### `bg(pid)`
- **Module:** `process`
- **Signature:** `bg(pid: int) -> bool`
- **Description:** Resumes a stopped process in the background (sends SIGCONT). Returns `true` on success.
- **Example:** `bg(12345) → true`

### `bin(n)`
- **Module:** `math`
- **Signature:** `bin(n: int) -> string`
- **Description:** Converts an integer to a binary string (no prefix).
- **Example:** `bin(10) → "1010"`

### `bind` (keyword in `incase`)
- **Module:** `core` (language)
- **Description:** Captures the subject in `incase`. See [Pattern Matching](../core/pattern-matching.md).

### `bring` (statement)
- **Module:** `core` (language)
- **Description:** Imports modules or files. See [Importing Modules](../modules/modules-import.md).

### `bytes(value)`
- **Module:** `bytes`
- **Signature:** `bytes(value: string | list[int]) -> bytes`
- **Description:** Converts a string or list of integers to a byte string.
- **Example:** `bytes("hello") → b"hello"`

### `byte_at(bytes, index)`
- **Module:** `bytes`
- **Signature:** `byte_at(bytes: bytes, index: int) -> int`
- **Description:** Returns the byte value (0‑255) at `index` in `bytes`.
- **Example:** `byte_at(b"hello", 0) → 104`

### `byte_len(bytes)`
- **Module:** `bytes`
- **Signature:** `byte_len(bytes: bytes) -> int`
- **Description:** Returns the length of `bytes` in bytes.
- **Example:** `byte_len(b"hello") → 5`

### `bytes_concat(a, b)`
- **Module:** `bytes`
- **Signature:** `bytes_concat(a: bytes, b: bytes) -> bytes`
- **Description:** Concatenates two byte strings.
- **Example:** `bytes_concat(b"he", b"llo") → b"hello"`

### `bytes_slice(bytes, start[, end])`
- **Module:** `bytes`
- **Signature:** `bytes_slice(bytes: bytes, start: int, end: int = -1) -> bytes`
- **Description:** Returns a slice of `bytes` from `start` to `end`.
- **Example:** `bytes_slice(b"hello", 1, 3) → b"el"`

---

## C

### `cal([month, year])`
- **Module:** `sysmon`
- **Signature:** `cal(month: int = 0, year: int = 0) -> string`
- **Description:** Returns a calendar. With no args, shows the current month.
- **Example:** `cal(7, 2026) →` (July 2026 calendar)

### `cat(path)`
- **Module:** `fs`
- **Signature:** `cat(path: string) -> string`
- **Description:** Returns the file contents as a string. Alias for `read()`.
- **Example:** `cat("data.txt")`

### `cd(path)`
- **Module:** `os`
- **Signature:** `cd(path: string) -> none`
- **Description:** Changes the current working directory.
- **Example:** `cd("src/")`

### `ceil(x)`
- **Module:** `math`
- **Signature:** `ceil(x: number) -> int`
- **Description:** Rounds `x` up to the nearest integer.
- **Example:** `ceil(3.1) → 4`

### `center(str, width[, fill])`
- **Module:** `string`
- **Signature:** `center(str: string, width: int, fill: string = " ") -> string`
- **Description:** Centers `str` within `width`, padding with `fill`.
- **Example:** `center("abc", 7, "-") → "--abc--"`

### `char_at(str, n)`
- **Module:** `string`
- **Signature:** `char_at(str: string, n: int) -> string`
- **Description:** Returns the character at index `n` as a single‑character string.
- **Example:** `char_at("hello", 1) → "e"`

### `chgrp(path, group)`
- **Module:** `fs`
- **Signature:** `chgrp(path: string, group: string) -> none`
- **Description:** Changes the group of `path` (Unix only; no‑op on Windows).
- **Example:** `chgrp("file.txt", "staff")`

### `chmod(path, mode)`
- **Module:** `fs`
- **Signature:** `chmod(path: string, mode: int) -> none`
- **Description:** Changes permissions of `path`.
- **Example:** `chmod("script.sh", 755)`

### `chown(path, user, group)`
- **Module:** `fs`
- **Signature:** `chown(path: string, user: string, group: string) -> none`
- **Description:** Changes owner and group of `path` (Unix only; no‑op on Windows).
- **Example:** `chown("file.txt", "alice", "staff")`

### `clamp(x, lo, hi)`
- **Module:** `math`
- **Signature:** `clamp(x: number, lo: number, hi: number) -> number`
- **Description:** Constrains `x` to the range `[lo, hi]`.
- **Example:** `clamp(5, 0, 3) → 3`

### `clear()`
- **Module:** `shell`
- **Signature:** `clear() -> none`
- **Description:** Clears the terminal screen.
- **Example:** `clear()`

### `complex(real[, imag])`
- **Module:** `type`
- **Signature:** `complex(real: number, imag: number = 0) -> complex`
- **Description:** Creates a complex number.
- **Example:** `complex(2, 3) → (2+3i)`

### `contains(container, elem)`
- **Module:** `collection`
- **Signature:** `contains(container: list|tuple|set|map|string, elem) -> bool`
- **Description:** Checks if `elem` is in `container`.
- **Example:** `contains([1,2,3], 2) → true`

### `conjugate(z)`
- **Module:** `type`
- **Signature:** `conjugate(z: complex) -> complex`
- **Description:** Returns the complex conjugate of `z`.
- **Example:** `conjugate(2+3i) → (2-3i)`

### `cos(x)`
- **Module:** `math`
- **Signature:** `cos(x: number) -> float`
- **Description:** Returns the cosine of `x` (radians).
- **Example:** `cos(0) → 1.0`

### `cosh(x)`
- **Module:** `math`
- **Signature:** `cosh(x: number) -> float`
- **Description:** Returns the hyperbolic cosine of `x`.
- **Example:** `cosh(1) → 1.5430806348152437`

### `cot(x)`
- **Module:** `math`
- **Signature:** `cot(x: number) -> float`
- **Description:** Returns the cotangent of `x` (1/tan(x)).
- **Example:** `cot(1) → 0.6420926159343306`

### `coth(x)`
- **Module:** `math`
- **Signature:** `coth(x: number) -> float`
- **Description:** Returns the hyperbolic cotangent of `x` (1/tanh(x)).
- **Example:** `coth(1) → 1.3130352854993312`

### `count(str_or_list, sub)`
- **Module:** `string` / `collection` (polymorphic)
- **Signature:** `count(str_or_list: string|list, sub) -> int`
- **Description:** Counts occurrences of `sub` in `str_or_list`.
- **Example:** `count("hello", "l") → 2`

### `cp(src, dst)`
- **Module:** `os`
- **Signature:** `cp(src: string, dst: string) -> none`
- **Description:** Copies `src` to `dst` (files or directories recursively).
- **Example:** `cp("a.txt", "b.txt")`

### `cpu_count()`
- **Module:** `sysmon`
- **Signature:** `cpu_count() -> int`
- **Description:** Returns the number of logical CPU cores.
- **Example:** `cpu_count() → 8`

### `cpu_usage()`
- **Module:** `sysmon`
- **Signature:** `cpu_usage() -> float`
- **Description:** Returns the current CPU usage percentage.
- **Example:** `cpu_usage() → 12.5`

### `csc(x)`
- **Module:** `math`
- **Signature:** `csc(x: number) -> float`
- **Description:** Returns the cosecant of `x` (1/sin(x)).
- **Example:** `csc(1) → 1.1883951057781212`

### `csch(x)`
- **Module:** `math`
- **Signature:** `csch(x: number) -> float`
- **Description:** Returns the hyperbolic cosecant of `x` (1/sinh(x)).
- **Example:** `csch(1) → 0.8509181282393216`

### `csv_parse(str[, sep])`
- **Module:** `json`
- **Signature:** `csv_parse(str: string, sep: string = ",") -> list[map]`
- **Description:** Parses CSV `str` into a list of maps (first row = headers).
- **Example:** `csv_parse("name,age\nAlice,30\nBob,25")`

### `csv_read(path[, sep])`
- **Module:** `json`
- **Signature:** `csv_read(path: string, sep: string = ",") -> list[map]`
- **Description:** Reads CSV file into a list of maps.
- **Example:** `csv_read("data.csv")`

### `csv_write(path, data)`
- **Module:** `json`
- **Signature:** `csv_write(path: string, data: list[map]) -> none`
- **Description:** Writes list of maps as CSV to `path`.
- **Example:** `csv_write("out.csv", rows)`

### `cut(path, delim, fields)`
- **Module:** `textproc`
- **Signature:** `cut(path: string, delim: string, fields: list[int]) -> list[string]`
- **Description:** Extracts columns from each line.
- **Example:** `cut("data.txt", ",", [1, 3])`

### `cwd()`
- **Module:** `os`
- **Signature:** `cwd() -> string`
- **Description:** Returns the current working directory.
- **Example:** `cwd() → "/home/user/project"`

---

## D

### `date_str([format])`
- **Module:** `sysmon`
- **Signature:** `date_str(format: string = "%Y-%m-%d %H:%M:%S") -> string`
- **Description:** Returns the current date/time formatted with `strftime`.
- **Example:** `date_str("%Y-%m-%d") → "2026-07-23"`

### `decode(bytes[, encoding])`
- **Module:** `bytes`
- **Signature:** `decode(bytes: bytes, encoding: string = "UTF-8") -> string`
- **Description:** Decodes `bytes` to a string (UTF‑8 default).
- **Example:** `decode(b"hello") → "hello"`

### `delete_key(map, key)`
- **Module:** `map`
- **Signature:** `delete_key(map: map, key) -> any`
- **Description:** Removes `key` from `map` and returns its value (or `none`).
- **Example:** `delete_key({"a":1}, "a") → 1`

### `diff(s1, s2)`
- **Module:** `collection`
- **Signature:** `diff(s1: set, s2: set) -> set`
- **Description:** Returns a new set with elements in `s1` but not `s2`.
- **Example:** `diff({1,2}, {2,3}) → {1}`

### `dirname(path)`
- **Module:** `os`
- **Signature:** `dirname(path: string) -> string`
- **Description:** Returns the parent directory of `path`.
- **Example:** `dirname("/home/user/file.txt") → "/home/user"`

### `disk_free(path)`
- **Module:** `fs`
- **Signature:** `disk_free(path: string) -> int`
- **Description:** Returns the free disk space on the volume containing `path`.
- **Example:** `disk_free(".") → 123456789`

### `disk_usage(path)`
- **Module:** `fs`
- **Signature:** `disk_usage(path: string) -> int`
- **Description:** Returns the total size of `path` in bytes.
- **Example:** `disk_usage(".") → 4096`

### `dmesg()`
- **Module:** `sysmon`
- **Signature:** `dmesg() -> string`
- **Description:** Returns kernel ring buffer messages (Unix only).
- **Example:** `dmesg()`

### `dns_lookup(hostname)`
- **Module:** `net`
- **Signature:** `dns_lookup(hostname: string) -> list[string]`
- **Description:** Resolves `hostname` to a list of IP addresses.
- **Example:** `dns_lookup("example.com") → ["93.184.216.34"]`

### `download(url, path)`
- **Module:** `net`
- **Signature:** `download(url: string, path: string) -> bool`
- **Description:** Downloads a file from `url` to `path`.
- **Example:** `download("https://example.com/file", "file.txt")`

---

## E

### `encode(str[, encoding])`
- **Module:** `bytes`
- **Signature:** `encode(str: string, encoding: string = "UTF-8") -> bytes`
- **Description:** Encodes `str` to bytes (UTF‑8 default).
- **Example:** `encode("hello") → b"hello"`

### `ends_with(str, suffix)`
- **Module:** `string`
- **Signature:** `ends_with(str: string, suffix: string) -> bool`
- **Description:** Returns `true` if `str` ends with `suffix`.
- **Example:** `ends_with("hello", "lo") → true`

### `entries(map)`
- **Module:** `map`
- **Signature:** `entries(map: map) -> list[[key, value]]`
- **Description:** Returns a list of `[key, value]` pairs from `map`.
- **Example:** `entries({"a":1}) → [["a", 1]]`

### `enum` (statement)
- **Module:** `core` (language)
- **Description:** Defines an enumeration. See [Enums](../core/data-types.md#enum).

### `enumerate(list[, start])`
- **Module:** `list`
- **Signature:** `enumerate(list: list, start: int = 0) -> list[[int, any]]`
- **Description:** Returns a list of `[index, value]` pairs.
- **Example:** `enumerate(["a","b"], 1) → [[1,"a"],[2,"b"]]`

### `env_get(name)`
- **Module:** `os`
- **Signature:** `env_get(name: string) -> string`
- **Description:** Returns the value of environment variable `name`. Throws `EnvError` if missing.
- **Example:** `env_get("HOME") → "/home/alice"`

### `env_has(name)`
- **Module:** `os`
- **Signature:** `env_has(name: string) -> bool`
- **Description:** Returns `true` if environment variable `name` exists.
- **Example:** `env_has("PATH") → true`

### `env_list()`
- **Module:** `shell`
- **Signature:** `env_list() -> map`
- **Description:** Returns all environment variables as a map.
- **Example:** `env_list() → {"HOME": "/home", "PATH": "/bin"}`

### `env_set(name, value)`
- **Module:** `os`
- **Signature:** `env_set(name: string, value: string) -> none`
- **Description:** Sets environment variable `name` to `value`.
- **Example:** `env_set("DEBUG", "true")`

### `env_unset(name)`
- **Module:** `os`
- **Signature:** `env_unset(name: string) -> none`
- **Description:** Unsets environment variable `name`.
- **Example:** `env_unset("TEMP")`

### `exists(path)`
- **Module:** `os`
- **Signature:** `exists(path: string) -> bool`
- **Description:** Returns `true` if `path` exists.
- **Example:** `exists("data.txt") → true`

### `exit([code])`
- **Module:** `io`
- **Signature:** `exit(code: int = 0) -> never`
- **Description:** Exits the process with the given exit code.
- **Example:** `exit(0)`

### `exit_code()`
- **Module:** `os`
- **Signature:** `exit_code() -> int`
- **Description:** Returns the exit code of the last executed command.
- **Example:** `exit_code() → 0`

### `export` (statement)
- **Module:** `core` (language)
- **Description:** Exports a declaration from a module. See [Defining Modules](../modules/modules-definition.md).

### `export_env(name, value)`
- **Module:** `shell`
- **Signature:** `export_env(name: string, value: string) -> none`
- **Description:** Alias for `env_set()`.
- **Example:** `export_env("MY_VAR", "42")`

### `ext(path)`
- **Module:** `os`
- **Signature:** `ext(path: string) -> string`
- **Description:** Returns the file extension (including the leading dot).
- **Example:** `ext("file.txt") → ".txt"`

### `extension(path)`
- **Module:** `fs`
- **Signature:** `extension(path: string) -> string`
- **Description:** Alias for `ext()`.
- **Example:** `extension("file.txt") → ".txt"`

---

## F

### `factorial(n)`
- **Module:** `math`
- **Signature:** `factorial(n: int) -> int`
- **Description:** Returns `n!`. Throws `ValueError` if `n < 0` or `n > 20`.
- **Example:** `factorial(5) → 120`

### `false_val()`
- **Module:** `shell`
- **Signature:** `false_val() -> bool`
- **Description:** Returns `false`.
- **Example:** `false_val() → false`

### `fg(pid)`
- **Module:** `process`
- **Signature:** `fg(pid: int) -> bool`
- **Description:** Brings a process to the foreground (sends SIGCONT). Returns `true` on success.
- **Example:** `fg(12345) → true`

### `file_diff(file1, file2)`
- **Module:** `fs`
- **Signature:** `file_diff(file1: string, file2: string) -> list[string]`
- **Description:** Returns a line‑by‑line diff between two files.
- **Example:** `file_diff("a.txt", "b.txt")`

### `file_size(path)`
- **Module:** `os`
- **Signature:** `file_size(path: string) -> int`
- **Description:** Returns the size of `path` in bytes.
- **Example:** `file_size("data.txt") → 1024`

### `filter(list, fn)`
- **Module:** `core` (higher‑order)
- **Signature:** `filter(list: list, fn: function) -> list`
- **Description:** Returns a new list with elements where `fn` returns `true`.
- **Example:** `filter([1,2,3,4], x => x%2==0) → [2,4]`

### `find(dir, pattern)`
- **Module:** `fs`
- **Signature:** `find(dir: string, pattern: string) -> list[string]`
- **Description:** Recursively finds files matching a glob pattern.
- **Example:** `find(".", "*.xel")`

### `find_regex(dir, pattern)`
- **Module:** `fs`
- **Signature:** `find_regex(dir: string, pattern: string) -> list[string]`
- **Description:** Recursively finds files matching a regex pattern.
- **Example:** `find_regex(".", "[a-z]+_test\.xel")`

### `first(list)`
- **Module:** `list`
- **Signature:** `first(list: list) -> any`
- **Description:** Returns the first element of `list`. Throws `IndexError` if empty.
- **Example:** `first([1,2,3]) → 1`

### `flatten(list)`
- **Module:** `list`
- **Signature:** `flatten(list: list) -> list`
- **Description:** Flattens a nested list by one level.
- **Example:** `flatten([1,[2,3],4]) → [1,2,3,4]`

### `float(x)`
- **Module:** `type`
- **Signature:** `float(x: any) -> float`
- **Description:** Converts `x` to a float.
- **Example:** `float("3.14") → 3.14`

### `floor(x)`
- **Module:** `math`
- **Signature:** `floor(x: number) -> int`
- **Description:** Rounds `x` down to the nearest integer.
- **Example:** `floor(3.9) → 3`

### `format(template, …)`
- **Module:** `util`
- **Signature:** `format(template: string, ...args) -> string`
- **Description:** Python‑style string formatting with `{}` placeholders.
- **Example:** `format("Hello, {}!", "world") → "Hello, world!"`

### `format_date(date_map, fmt)`
- **Module:** `datetime`
- **Signature:** `format_date(date_map: map, fmt: string) -> string`
- **Description:** Formats a date map using `strftime`.
- **Example:** `format_date(now(), "%Y-%m-%d") → "2026-07-23"`

### `free()`
- **Module:** `sysmon`
- **Signature:** `free() -> map`
- **Description:** Returns RAM info: `{total, used, free, swap_total, swap_free}`.
- **Example:** `free()["total"] → 17179869184`

### `from` (keyword)
- **Module:** `core` (language)
- **Description:** Specifies a directory for `bring`. See [Importing Modules](../modules/modules-import.md).

### `from_entries(list)`
- **Module:** `map`
- **Signature:** `from_entries(list: list[[key, value]]) -> map`
- **Description:** Builds a map from a list of `[key, value]` pairs.
- **Example:** `from_entries([["a",1]]) → {"a":1}`

---

## G

### `gcd(a, b)`
- **Module:** `math`
- **Signature:** `gcd(a: int, b: int) -> int`
- **Description:** Returns the greatest common divisor of `a` and `b`.
- **Example:** `gcd(12, 18) → 6`

### `gen_collect(generator)`
- **Module:** `generator`
- **Signature:** `gen_collect(generator: generator) -> list`
- **Description:** Exhausts a generator and returns a list of all yielded values.
- **Example:** `gen_collect(g)`

### `get(map, key[, default])`
- **Module:** `map`
- **Signature:** `get(map: map, key, default: any = none) -> any`
- **Description:** Returns the value for `key`, or `default` if not found.
- **Example:** `get({"a":1}, "b", 0) → 0`

### `getuid()`
- **Module:** `process`
- **Signature:** `getuid() -> int`
- **Description:** Returns the numeric user ID (Unix only; 0 on Windows).
- **Example:** `getuid() → 1000`

### `glob(pattern)`
- **Module:** `fs`
- **Signature:** `glob(pattern: string) -> list[string]`
- **Description:** Expands a glob pattern relative to CWD.
- **Example:** `glob("*.xel")`

### `grep(pattern, path)`
- **Module:** `textproc`
- **Signature:** `grep(pattern: string, path: string) -> list[string]`
- **Description:** Searches `path` for lines containing `pattern`.
- **Example:** `grep("error", "log.txt")`

### `grep_recursive(pattern, dir)`
- **Module:** `textproc`
- **Signature:** `grep_recursive(pattern: string, dir: string) -> list[map]`
- **Description:** Recursively searches `dir` for `pattern`, returns `{file, line_number, text}`.
- **Example:** `grep_recursive("TODO", ".")`

### `grep_regex(pattern, path)`
- **Module:** `textproc`
- **Signature:** `grep_regex(pattern: string, path: string) -> list[string]`
- **Description:** Searches `path` using a regex pattern.
- **Example:** `grep_regex("[0-9]+", "data.txt")`

### `gzip_compress(path)`
- **Module:** `archive`
- **Signature:** `gzip_compress(path: string) -> string`
- **Description:** Compresses `path` with gzip, returns the new file path.
- **Example:** `gzip_compress("data.txt") → "data.txt.gz"`

---

## H

### `hardlink(target, link_path)`
- **Module:** `fs`
- **Signature:** `hardlink(target: string, link_path: string) -> none`
- **Description:** Creates a hard link.
- **Example:** `hardlink("data.txt", "backup.txt")`

### `has(map_or_set, key)`
- **Module:** `collection`
- **Signature:** `has(map_or_set: map|set, key) -> bool`
- **Description:** Checks if `key` exists in `map_or_set`.
- **Example:** `has({"a":1}, "a") → true`

### `hash(value[, algorithm])`
- **Module:** `hash`
- **Signature:** `hash(value: any, algorithm: string = "fnv1a") -> int`
- **Description:** Returns the hash of `value` using the specified algorithm.
- **Example:** `hash("hello", "murmur3")`

### `hash_seed(value, seed[, algorithm])`
- **Module:** `hash`
- **Signature:** `hash_seed(value: any, seed: int, algorithm: string = "fnv1a") -> int`
- **Description:** Returns a seeded hash of `value`.
- **Example:** `hash_seed("hello", 42)`

### `head(path, n=10)`
- **Module:** `textproc`
- **Signature:** `head(path: string, n: int = 10) -> list[string]`
- **Description:** Returns the first `n` lines of a file.
- **Example:** `head("data.txt", 5)`

### `hex(n)`
- **Module:** `math`
- **Signature:** `hex(n: int) -> string`
- **Description:** Converts an integer to a hex string (no prefix).
- **Example:** `hex(255) → "ff"`

### `history()`
- **Module:** `shell`
- **Signature:** `history() -> list[map]`
- **Description:** Returns the command history as a list of `{index, command}`.
- **Example:** `history()`

### `home_dir()`
- **Module:** `fs`
- **Signature:** `home_dir() -> string`
- **Description:** Returns the user's home directory.
- **Example:** `home_dir() → "/home/alice"`

### `host_lookup(hostname)`
- **Module:** `net`
- **Signature:** `host_lookup(hostname: string) -> string`
- **Description:** Wrapper around the `host` command (Unix) or `nslookup` (Windows).
- **Example:** `host_lookup("example.com")`

### `hostname()`
- **Module:** `process`
- **Signature:** `hostname() -> string`
- **Description:** Returns the machine's hostname.
- **Example:** `hostname() → "my-machine"`

### `http_delete(url[, headers])`
- **Module:** `net`
- **Signature:** `http_delete(url: string, headers: map = {}) -> map`
- **Description:** Sends an HTTP DELETE request. Returns `{status, body}`.
- **Example:** `http_delete("https://api.example.com/resource")`

### `http_get(url[, headers])`
- **Module:** `net`
- **Signature:** `http_get(url: string, headers: map = {}) -> map`
- **Description:** Sends an HTTP GET request. Returns `{status, body, headers}`.
- **Example:** `http_get("https://api.example.com/data")`

### `http_post(url, body[, headers])`
- **Module:** `net`
- **Signature:** `http_post(url: string, body: string, headers: map = {}) -> map`
- **Description:** Sends an HTTP POST request. Returns `{status, body}`.
- **Example:** `http_post("https://api.example.com", '{"key":"value"}')`

### `http_put(url, body[, headers])`
- **Module:** `net`
- **Signature:** `http_put(url: string, body: string, headers: map = {}) -> map`
- **Description:** Sends an HTTP PUT request. Returns `{status, body}`.
- **Example:** `http_put("https://api.example.com/resource", '{"key":"value"}')`

---

## I

### `id()`
- **Module:** `process`
- **Signature:** `id() -> map`
- **Description:** Returns user info: `{uid, gid, user, groups}`.
- **Example:** `id()["user"] → "alice"`

### `ifconfig()`
- **Module:** `net`
- **Signature:** `ifconfig() -> string`
- **Description:** Shows network interface configuration.
- **Example:** `ifconfig()`

### `imag(z)`
- **Module:** `type`
- **Signature:** `imag(z: complex) -> float`
- **Description:** Returns the imaginary part of complex number `z`.
- **Example:** `imag(2+3i) → 3.0`

### `index_of(str_or_list, sub)`
- **Module:** `string` / `collection` (polymorphic)
- **Signature:** `index_of(str_or_list: string|list, sub) -> int`
- **Description:** Returns the first index of `sub`, or `-1` if not found.
- **Example:** `index_of([1,2,3], 2) → 1`

### `input([prompt])`
- **Module:** `io`
- **Signature:** `input(prompt: string = "") -> string`
- **Description:** Reads a line from stdin, optionally with a `prompt`.
- **Example:** `input("Enter name: ")`

### `insert(list, index, item)`
- **Module:** `list`
- **Signature:** `insert(list: list, index: int, item: any) -> none`
- **Description:** Inserts `item` at `index` in `list`.
- **Example:** `insert([1,3], 1, 2) → [1,2,3]`

### `intersect(s1, s2)`
- **Module:** `collection`
- **Signature:** `intersect(s1: set, s2: set) -> set`
- **Description:** Returns a new set with elements in both `s1` and `s2`.
- **Example:** `intersect({1,2}, {2,3}) → {2}`

### `int(x)`
- **Module:** `type`
- **Signature:** `int(x: any) -> int`
- **Description:** Converts `x` to an integer.
- **Example:** `int("42") → 42`

### `Int(x)` (capitalised alias)
- **Module:** `casting`
- **Signature:** `Int(x: any) -> int`
- **Description:** Alias for `int()`.
- **Example:** `Int("42") → 42`

### `iSet(x)`
- **Module:** `casting`
- **Signature:** `iSet(x: any) -> frozen_set`
- **Description:** Converts to a frozen set (immutable set).
- **Example:** `iSet([1,2,3]) → <1,2,3>`

### `is_absolute(path)`
- **Module:** `fs`
- **Signature:** `is_absolute(path: string) -> bool`
- **Description:** Returns `true` if `path` is absolute.
- **Example:** `is_absolute("/tmp") → true`

### `is_alpha(str)`
- **Module:** `string`
- **Signature:** `is_alpha(str: string) -> bool`
- **Description:** Returns `true` if every character in `str` is alphabetic.
- **Example:** `is_alpha("abc") → true`

### `is_dir(path)`
- **Module:** `os`
- **Signature:** `is_dir(path: string) -> bool`
- **Description:** Returns `true` if `path` is a directory.
- **Example:** `is_dir("data/") → true`

### `is_empty(str_or_list_or_map)`
- **Module:** `string` / `collection`
- **Signature:** `is_empty(container: string|list|map) -> bool`
- **Description:** Returns `true` if the container is empty.
- **Example:** `is_empty([]) → true`

### `is_exhausted(generator)`
- **Module:** `generator`
- **Signature:** `is_exhausted(generator: generator) -> bool`
- **Description:** Returns `true` if the generator is exhausted.
- **Example:** `is_exhausted(g)`

### `is_file(path)`
- **Module:** `os`
- **Signature:** `is_file(path: string) -> bool`
- **Description:** Returns `true` if `path` is a regular file.
- **Example:** `is_file("data.txt") → true`

### `is_hashable(value)`
- **Module:** `hash`
- **Signature:** `is_hashable(value: any) -> bool`
- **Description:** Returns `true` if `value` can be hashed.
- **Example:** `is_hashable([1,2,3]) → false`

### `is_inf(x)`
- **Module:** `math`
- **Signature:** `is_inf(x: float) -> bool`
- **Description:** Returns `true` if `x` is infinite.
- **Example:** `is_inf(INF) → true`

### `is_nan(x)`
- **Module:** `math`
- **Signature:** `is_nan(x: float) -> bool`
- **Description:** Returns `true` if `x` is NaN.
- **Example:** `is_nan(0.0/0.0) → true`

### `is_numeric(str)`
- **Module:** `string`
- **Signature:** `is_numeric(str: string) -> bool`
- **Description:** Returns `true` if `str` can be parsed as a number.
- **Example:** `is_numeric("42") → true`

### `is_root()`
- **Module:** `process`
- **Signature:** `is_root() -> bool`
- **Description:** Returns `true` if running as root/admin.
- **Example:** `is_root() → false`

### `is_symlink(path)`
- **Module:** `fs`
- **Signature:** `is_symlink(path: string) -> bool`
- **Description:** Returns `true` if `path` is a symbolic link.
- **Example:** `is_symlink("link") → true`

---

## J

### `jobs()`
- **Module:** `process`
- **Signature:** `jobs() -> string`
- **Description:** Returns a string listing background jobs.
- **Example:** `jobs()`

### `join(list, sep)`
- **Module:** `string`
- **Signature:** `join(list: list[string], sep: string) -> string`
- **Description:** Joins a list of strings with `sep`.
- **Example:** `join(["a","b"], "-") → "a-b"`

### `join_path(parts…)`
- **Module:** `fs`
- **Signature:** `join_path(parts: string...) -> string`
- **Description:** Joins path components.
- **Example:** `join_path("home", "user", "file.txt") → "home/user/file.txt"`

### `journalctl([n])`
- **Module:** `sysmon`
- **Signature:** `journalctl(n: int = 50) -> string`
- **Description:** Returns the last `n` entries from the systemd journal.
- **Example:** `journalctl(10)`

---

## K

### `keys(map)`
- **Module:** `collection`
- **Signature:** `keys(map: map) -> list`
- **Description:** Returns a list of keys (insertion order).
- **Example:** `keys({"a":1}) → ["a"]`

### `kill(pid[, signal])`
- **Module:** `process`
- **Signature:** `kill(pid: int, signal: int = 15) -> bool`
- **Description:** Sends a signal to a process. Returns `true` on success.
- **Example:** `kill(12345, 9)`

### `kill_name(name)`
- **Module:** `process`
- **Signature:** `kill_name(name: string) -> int`
- **Description:** Kills all processes with the given name. Returns exit code.
- **Example:** `kill_name("firefox")`

---

## L

### `last(list)`
- **Module:** `list`
- **Signature:** `last(list: list) -> any`
- **Description:** Returns the last element of `list`. Throws `IndexError` if empty.
- **Example:** `last([1,2,3]) → 3`

### `last_logins([n])`
- **Module:** `sysmon`
- **Signature:** `last_logins(n: int = 10) -> string`
- **Description:** Returns the last `n` login history entries.
- **Example:** `last_logins(5)`

### `lcm(a, b)`
- **Module:** `math`
- **Signature:** `lcm(a: int, b: int) -> int`
- **Description:** Returns the least common multiple of `a` and `b`.
- **Example:** `lcm(12, 18) → 36`

### `len(x)`
- **Module:** `collection`
- **Signature:** `len(x: string|list|tuple|set|frozen_set|map) -> int`
- **Description:** Returns the length of `x`.
- **Example:** `len([1,2,3]) → 3`

### `less(path, page_size=20)`
- **Module:** `textproc`
- **Signature:** `less(path: string, page_size: int = 20) -> list[list[string]]`
- **Description:** Paginated file view – returns a list of pages.
- **Example:** `less("large.txt", 10)`

### `lines(str)`
- **Module:** `string`
- **Signature:** `lines(str: string) -> list[string]`
- **Description:** Splits `str` into lines.
- **Example:** `lines("a\nb\nc") → ["a","b","c"]`

### `List(x)` (capitalised alias)
- **Module:** `casting`
- **Signature:** `List(x: any) -> list`
- **Description:** Converts to a list.
- **Example:** `List("1,2,3") → ["1","2","3"]`

### `ljust(str, width[, fill])`
- **Module:** `string`
- **Signature:** `ljust(str: string, width: int, fill: string = " ") -> string`
- **Description:** Left‑justifies `str` in `width`, padding with `fill`.
- **Example:** `ljust("abc", 5, "-") → "abc--"`

### `ln(target, link[, soft])`
- **Module:** `fs`
- **Signature:** `ln(target: string, link: string, soft: bool = true) -> none`
- **Description:** Creates a link (soft by default).
- **Example:** `ln("data.txt", "link.txt")`

### `locate(name)`
- **Module:** `fs`
- **Signature:** `locate(name: string) -> list[string]`
- **Description:** Finds files by exact name, searching recursively from CWD.
- **Example:** `locate("config.xel")`

### `local_ip()`
- **Module:** `net`
- **Signature:** `local_ip() -> string`
- **Description:** Returns the local machine IP address.
- **Example:** `local_ip() → "192.168.1.100"`

### `log(x)`
- **Module:** `math`
- **Signature:** `log(x: number) -> float`
- **Description:** Returns the natural logarithm of `x`.
- **Example:** `log(E) → 1.0`

### `log10(x)`
- **Module:** `math`
- **Signature:** `log10(x: number) -> float`
- **Description:** Returns the base‑10 logarithm of `x`.
- **Example:** `log10(100) → 2.0`

### `log2(x)`
- **Module:** `math`
- **Signature:** `log2(x: number) -> float`
- **Description:** Returns the base‑2 logarithm of `x`.
- **Example:** `log2(8) → 3.0`

### `logout()`
- **Module:** `shell`
- **Signature:** `logout() -> never`
- **Description:** Exits the process with code 0.
- **Example:** `logout()`

### `lower(str)`
- **Module:** `string`
- **Signature:** `lower(str: string) -> string`
- **Description:** Converts `str` to lowercase.
- **Example:** `lower("HELLO") → "hello"`

### `ls(path)`
- **Module:** `os`
- **Signature:** `ls(path: string) -> list[string]`
- **Description:** Lists directory contents.
- **Example:** `ls(".")`

### `ls_all(path)`
- **Module:** `fs`
- **Signature:** `ls_all(path: string) -> list[string]`
- **Description:** Lists directory contents including hidden files.
- **Example:** `ls_all(".")`

### `lsblk()`
- **Module:** `sysmon`
- **Signature:** `lsblk() -> string`
- **Description:** Lists block storage devices.
- **Example:** `lsblk()`

### `lscpu()`
- **Module:** `sysmon`
- **Signature:** `lscpu() -> string`
- **Description:** Returns detailed CPU info.
- **Example:** `lscpu()`

### `lsmem()`
- **Module:** `sysmon`
- **Signature:** `lsmem() -> string`
- **Description:** Returns memory info.
- **Example:** `lsmem()`

### `lsof()`
- **Module:** `process`
- **Signature:** `lsof() -> string`
- **Description:** Lists open files for the current process.
- **Example:** `lsof()`

### `lspci()`
- **Module:** `sysmon`
- **Signature:** `lspci() -> string`
- **Description:** Lists PCI devices.
- **Example:** `lspci()`

### `lsusb()`
- **Module:** `sysmon`
- **Signature:** `lsusb() -> string`
- **Description:** Lists USB devices.
- **Example:** `lsusb()`

---

## M

### `magnitude(z)`
- **Module:** `type`
- **Signature:** `magnitude(z: complex) -> float`
- **Description:** Returns the magnitude (absolute value) of complex number `z`.
- **Example:** `magnitude(3+4i) → 5.0`

### `man(topic)`
- **Module:** `shell`
- **Signature:** `man(topic: string) -> string`
- **Description:** Shows the manual page for `topic`.
- **Example:** `man("xell")`

### `map(list, fn)`
- **Module:** `core` (higher‑order)
- **Signature:** `map(list: list, fn: function) -> list`
- **Description:** Applies `fn` to every element of `list`, returns a new list.
- **Example:** `map([1,2,3], x => x*2) → [2,4,6]`

### `max(a, b) or max(list)`
- **Module:** `math` / `list`
- **Signature:** `max(a: number, b: number) -> number` or `max(list: list) -> number`
- **Description:** Returns the maximum value.
- **Example:** `max(5, 10) → 10`

### `mem_free()`
- **Module:** `sysmon`
- **Signature:** `mem_free() -> int`
- **Description:** Returns free RAM in bytes.
- **Example:** `mem_free() → 8589934592`

### `mem_total()`
- **Module:** `sysmon`
- **Signature:** `mem_total() -> int`
- **Description:** Returns total RAM in bytes.
- **Example:** `mem_total() → 17179869184`

### `mem_used()`
- **Module:** `sysmon`
- **Signature:** `mem_used() -> int`
- **Description:** Returns used RAM in bytes.
- **Example:** `mem_used() → 8589934592`

### `merge(map1, map2)`
- **Module:** `map`
- **Signature:** `merge(map1: map, map2: map) -> map`
- **Description:** Returns a new map merging `map1` and `map2` (map2 wins conflicts).
- **Example:** `merge({"a":1}, {"b":2}) → {"a":1,"b":2}`

### `min(a, b) or min(list)`
- **Module:** `math` / `list`
- **Signature:** `min(a: number, b: number) -> number` or `min(list: list) -> number`
- **Description:** Returns the minimum value.
- **Example:** `min(5, 10) → 5`

### `mkdir(path)`
- **Module:** `os`
- **Signature:** `mkdir(path: string) -> none`
- **Description:** Creates a directory (and parent directories).
- **Example:** `mkdir("data/subdir")`

### `mod(a, b)`
- **Module:** `math`
- **Signature:** `mod(a: number, b: number) -> number`
- **Description:** Returns `a % b`. Throws `DivisionByZeroError` if `b == 0`.
- **Example:** `mod(10, 3) → 1`

### `modified_time(path)`
- **Module:** `fs`
- **Signature:** `modified_time(path: string) -> int`
- **Description:** Returns the last modification timestamp (Unix seconds).
- **Example:** `modified_time("data.txt") → 1234567890`

### `more(path, page_size=20)`
- **Module:** `textproc`
- **Signature:** `more(path: string, page_size: int = 20) -> list[list[string]]`
- **Description:** Alias for `less()` – paginated file view.
- **Example:** `more("large.txt", 10)`

### `mount_fs(device, mount_point)`
- **Module:** `sysmon`
- **Signature:** `mount_fs(device: string, mount_point: string) -> string`
- **Description:** Mounts a filesystem (Unix only).
- **Example:** `mount_fs("/dev/sdb1", "/mnt")`

### `mv(src, dst)`
- **Module:** `os`
- **Signature:** `mv(src: string, dst: string) -> none`
- **Description:** Moves `src` to `dst`.
- **Example:** `mv("old.txt", "new.txt")`

---

## N

### `nc(host, port)`
- **Module:** `net`
- **Signature:** `nc(host: string, port: int) -> map`
- **Description:** Raw TCP connection test; returns `{connected, host, port}`.
- **Example:** `nc("example.com", 80)`

### `netstat()`
- **Module:** `net`
- **Signature:** `netstat() -> string`
- **Description:** Shows active network connections.
- **Example:** `netstat()`

### `next(generator)`
- **Module:** `generator`
- **Signature:** `next(generator: generator) -> any`
- **Description:** Resumes the generator and returns the next yielded value. Throws `RuntimeError` if exhausted.
- **Example:** `next(g)`

### `nice(command[, level])`
- **Module:** `process`
- **Signature:** `nice(command: string, level: int = 10) -> int`
- **Description:** Runs a command with adjusted priority. Returns exit code.
- **Example:** `nice("make", 10)`

### `nohup(command)`
- **Module:** `process`
- **Signature:** `nohup(command: string) -> map`
- **Description:** Runs a command immune to hangup signals. Returns `{command, pid}`.
- **Example:** `nohup("server &")`

### `normalize(path)`
- **Module:** `fs`
- **Signature:** `normalize(path: string) -> string`
- **Description:** Resolves `.` and `..` in a path.
- **Example:** `normalize("a/../b/./c") → "b/c"`

### `now()`
- **Module:** `datetime`
- **Signature:** `now() -> map`
- **Description:** Returns the current date/time as a map: `{year, month, day, hour, minute, second, weekday, yearday}`.
- **Example:** `now()["year"] → 2026`

### `nslookup(hostname)`
- **Module:** `net`
- **Signature:** `nslookup(hostname: string) -> string`
- **Description:** Wrapper around the `nslookup` command.
- **Example:** `nslookup("example.com")`

### `num(x)`
- **Module:** `type`
- **Signature:** `num(x: any) -> number`
- **Description:** Converts `x` to a number (int or float).
- **Example:** `num("3.14") → 3.14`

### `number(x)`
- **Module:** `casting`
- **Signature:** `number(x: any) -> number`
- **Description:** Alias for `num()`.
- **Example:** `number("42") → 42`

---

## O

### `of` (keyword)
- **Module:** `core` (language)
- **Description:** Module path separator in `bring`. See [Importing Modules](../modules/modules-import.md).

### `os_name()`
- **Module:** `process`
- **Signature:** `os_name() -> string`
- **Description:** Returns `"linux"`, `"windows"`, or `"macos"`.
- **Example:** `os_name() → "linux"`

---

## P

### `pad_end(str, n, ch)`
- **Module:** `string`
- **Signature:** `pad_end(str: string, n: int, ch: string) -> string`
- **Description:** Pads `str` on the right with `ch` until length `n`.
- **Example:** `pad_end("42", 5, "0") → "42000"`

### `pad_start(str, n, ch)`
- **Module:** `string`
- **Signature:** `pad_start(str: string, n: int, ch: string) -> string`
- **Description:** Pads `str` on the left with `ch` until length `n`.
- **Example:** `pad_start("42", 5, "0") → "00042"`

### `parse_date(str, fmt)`
- **Module:** `datetime`
- **Signature:** `parse_date(str: string, fmt: string) -> map`
- **Description:** Parses a date string using `strptime` format.
- **Example:** `parse_date("2026-07-23", "%Y-%m-%d")`

### `patch(file, patchfile)`
- **Module:** `textproc`
- **Signature:** `patch(file: string, patchfile: string) -> int`
- **Description:** Applies a simple unified diff patch. Returns number of operations applied.
- **Example:** `patch("data.txt", "data.patch")`

### `pid()`
- **Module:** `os`
- **Signature:** `pid() -> int`
- **Description:** Returns the current process ID.
- **Example:** `pid() → 12345`

### `pidof(name)`
- **Module:** `process`
- **Signature:** `pidof(name: string) -> list[int]`
- **Description:** Returns PIDs of processes with the exact name `name`.
- **Example:** `pidof("python")`

### `ping(host, count=4)`
- **Module:** `net`
- **Signature:** `ping(host: string, count: int = 4) -> string`
- **Description:** Tests connectivity to `host`.
- **Example:** `ping("example.com")`

### `pkill(pattern)`
- **Module:** `process`
- **Signature:** `pkill(pattern: string) -> int`
- **Description:** Kills processes matching `pattern`. Returns exit code.
- **Example:** `pkill("python")`

### `pop(list)`
- **Module:** `collection`
- **Signature:** `pop(list: list) -> any`
- **Description:** Removes and returns the last element of `list`. Throws `IndexError` if empty.
- **Example:** `pop([1,2]) → 2`

### `pow(x, y)`
- **Module:** `math`
- **Signature:** `pow(x: number, y: number) -> number`
- **Description:** Returns `x` raised to the power of `y`.
- **Example:** `pow(2, 8) → 256`

### `ppid()`
- **Module:** `process`
- **Signature:** `ppid() -> int`
- **Description:** Returns the parent process ID.
- **Example:** `ppid() → 12344`

### `print(value, …)`
- **Module:** `io`
- **Signature:** `print(values...) -> int`
- **Description:** Prints values to stdout, separated by spaces. Returns `0`.
- **Example:** `print("Hello", 42)`

### `printenv([name])`
- **Module:** `shell`
- **Signature:** `printenv(name: string = "") -> string|map`
- **Description:** With a name, returns the environment variable value (or `none`). With no args, returns all vars as a map.
- **Example:** `printenv("HOME") → "/home/alice"`

### `ps()`
- **Module:** `process`
- **Signature:** `ps() -> list[map]`
- **Description:** Returns a list of running processes: `{pid, name, ppid, mem}`.
- **Example:** `ps()`

### `pstree([pid])`
- **Module:** `process`
- **Signature:** `pstree(pid: int = 0) -> string`
- **Description:** Returns a process tree as a string.
- **Example:** `pstree()`

### `public_ip()`
- **Module:** `net`
- **Signature:** `public_ip() -> string`
- **Description:** Returns the public IP address via external service.
- **Example:** `public_ip() → "203.0.113.42"`

### `push(list, item)`
- **Module:** `collection`
- **Signature:** `push(list: list, item: any) -> none`
- **Description:** Appends `item` to the end of `list`.
- **Example:** `push([1,2], 3) → [1,2,3]`

### `pwd()`
- **Module:** `os`
- **Signature:** `pwd() -> string`
- **Description:** Alias for `cwd()` – returns current working directory.
- **Example:** `pwd() → "/home/user/project"`

---

## R

### `random()`
- **Module:** `math`
- **Signature:** `random() -> float`
- **Description:** Returns a random float in `[0.0, 1.0)`.
- **Example:** `random() → 0.374...`

### `random_choice(list)`
- **Module:** `math`
- **Signature:** `random_choice(list: list) -> any`
- **Description:** Returns a random element from `list`.
- **Example:** `random_choice(["a","b","c"])`

### `random_int(a, b)`
- **Module:** `math`
- **Signature:** `random_int(a: int, b: int) -> int`
- **Description:** Returns a random integer in `[a, b]` (inclusive).
- **Example:** `random_int(1, 6) → 4`

### `range([start], end, [step])`
- **Module:** `collection`
- **Signature:** `range(start: int = 0, end: int, step: int = 1) -> list[int]`
- **Description:** Returns a list of integers.
- **Example:** `range(5) → [0,1,2,3,4]`

### `read(path)`
- **Module:** `os`
- **Signature:** `read(path: string) -> string`
- **Description:** Reads the entire file as a string.
- **Example:** `read("data.txt")`

### `read_lines(path)`
- **Module:** `fs`
- **Signature:** `read_lines(path: string) -> list[string]`
- **Description:** Returns a list of lines from `path`.
- **Example:** `read_lines("data.txt")`

### `readlink(path)`
- **Module:** `fs`
- **Signature:** `readlink(path: string) -> string`
- **Description:** Resolves a symlink to its target path.
- **Example:** `readlink("link.txt")`

### `real(z)`
- **Module:** `type`
- **Signature:** `real(z: complex) -> float`
- **Description:** Returns the real part of complex number `z`.
- **Example:** `real(2+3i) → 2.0`

### `realpath(path)`
- **Module:** `fs`
- **Signature:** `realpath(path: string) -> string`
- **Description:** Returns the canonical absolute path (resolves symlinks).
- **Example:** `realpath("link.txt")`

### `reduce(list, fn, initial)`
- **Module:** `core` (higher‑order)
- **Signature:** `reduce(list: list, fn: function, initial: any) -> any`
- **Description:** Reduces `list` to a single value using `fn(acc, elem)`.
- **Example:** `reduce([1,2,3], (a,b)=>a+b, 0) → 6`

### `regex_find(str, pattern)`
- **Module:** `regex`
- **Signature:** `regex_find(str: string, pattern: string) -> string`
- **Description:** Returns the first match, or `""`.
- **Example:** `regex_find("hello", "[a-z]") → "h"`

### `regex_find_all(str, pattern)`
- **Module:** `regex`
- **Signature:** `regex_find_all(str: string, pattern: string) -> list[string]`
- **Description:** Returns a list of all matches.
- **Example:** `regex_find_all("a1 b2", "[0-9]") → ["1", "2"]`

### `regex_groups(str, pattern)`
- **Module:** `regex`
- **Signature:** `regex_groups(str: string, pattern: string) -> list[string]`
- **Description:** Returns capture groups from the first match.
- **Example:** `regex_groups("hello123", "([a-z]+)([0-9]+)") → ["hello", "123"]`

### `regex_match(str, pattern)`
- **Module:** `regex`
- **Signature:** `regex_match(str: string, pattern: string) -> bool`
- **Description:** Returns `true` if the pattern matches anywhere.
- **Example:** `regex_match("hello", "[0-9]") → false`

### `regex_match_full(str, pattern)`
- **Module:** `regex`
- **Signature:** `regex_match_full(str: string, pattern: string) -> bool`
- **Description:** Returns `true` if the entire string matches.
- **Example:** `regex_match_full("123", "[0-9]+") → true`

### `regex_replace(str, pattern, replacement)`
- **Module:** `regex`
- **Signature:** `regex_replace(str: string, pattern: string, replacement: string) -> string`
- **Description:** Replaces the first occurrence.
- **Example:** `regex_replace("foo bar", "o", "a") → "fao bar"`

### `regex_replace_all(str, pattern, replacement)`
- **Module:** `regex`
- **Signature:** `regex_replace_all(str: string, pattern: string, replacement: string) -> string`
- **Description:** Replaces all occurrences.
- **Example:** `regex_replace_all("foo bar", "o", "a") → "faa bar"`

### `regex_split(str, pattern)`
- **Module:** `regex`
- **Signature:** `regex_split(str: string, pattern: string) -> list[string]`
- **Description:** Splits `str` by the regex pattern.
- **Example:** `regex_split("a,b,c", ",") → ["a","b","c"]`

### `relative_path(path, base)`
- **Module:** `fs`
- **Signature:** `relative_path(path: string, base: string) -> string`
- **Description:** Returns `path` relative to `base`.
- **Example:** `relative_path("/a/b/c", "/a") → "b/c"`

### `remove(list_or_set_or_map, index_or_key)`
- **Module:** `collection`
- **Signature:** `remove(container: list|set|map, index_or_key) -> any`
- **Description:** Removes from list (by index), set (by value), or map (by key).
- **Example:** `remove([1,2,3], 1) → 2`

### `remove_val(list, val)`
- **Module:** `list`
- **Signature:** `remove_val(list: list, val: any) -> bool`
- **Description:** Removes the first occurrence of `val` from `list`. Returns `true` if found.
- **Example:** `remove_val([1,2,3], 2) → true`

### `repeat(str, n)`
- **Module:** `string`
- **Signature:** `repeat(str: string, n: int) -> string`
- **Description:** Repeats `str` `n` times.
- **Example:** `repeat("ha", 3) → "hahaha"`

### `replace(str, old, new)`
- **Module:** `string`
- **Signature:** `replace(str: string, old: string, new: string) -> string`
- **Description:** Replaces all occurrences of `old` with `new`.
- **Example:** `replace("foo bar", "o", "a") → "faa bar"`

### `replace_first(str, old, new)`
- **Module:** `string`
- **Signature:** `replace_first(str: string, old: string, new: string) -> string`
- **Description:** Replaces only the first occurrence of `old` with `new`.
- **Example:** `replace_first("foo bar", "o", "a") → "fao bar"`

### `reset()`
- **Module:** `shell`
- **Signature:** `reset() -> none`
- **Description:** Resets the terminal state.
- **Example:** `reset()`

### `reverse(str_or_list)`
- **Module:** `string` / `list`
- **Signature:** `reverse(container: string|list) -> string|list`
- **Description:** Reverses the container **in‑place**.
- **Example:** `reverse("hello") → "olleh"`

### `rjust(str, width[, fill])`
- **Module:** `string`
- **Signature:** `rjust(str: string, width: int, fill: string = " ") -> string`
- **Description:** Right‑justifies `str` in `width`, padding with `fill`.
- **Example:** `rjust("abc", 5, "-") → "--abc"`

### `rm(path)`
- **Module:** `os`
- **Signature:** `rm(path: string) -> none`
- **Description:** Removes a file or directory (recursively).
- **Example:** `rm("temp.txt")`

### `round(x[, n])`
- **Module:** `math`
- **Signature:** `round(x: number, n: int = 0) -> number`
- **Description:** Rounds `x` to the nearest integer (ties to even), or to `n` decimal places.
- **Example:** `round(3.5) → 4`

### `route()`
- **Module:** `net`
- **Signature:** `route() -> string`
- **Description:** Shows the routing table.
- **Example:** `route()`

### `rsync(src, dest)`
- **Module:** `net`
- **Signature:** `rsync(src: string, dest: string) -> string`
- **Description:** Syncs files using `rsync`; returns the output.
- **Example:** `rsync("data/", "backup/")`

### `run(command)`
- **Module:** `os`
- **Signature:** `run(command: string) -> int`
- **Description:** Runs an external command and returns the exit code.
- **Example:** `run("ls -la") → 0`

### `run_capture(command)`
- **Module:** `os`
- **Signature:** `run_capture(command: string) -> map`
- **Description:** Runs an external command and returns `{exit_code, stdout, stderr}`.
- **Example:** `run_capture("ls")`

### `run_timeout(command, timeout_ms)`
- **Module:** `process`
- **Signature:** `run_timeout(command: string, timeout_ms: int) -> map`
- **Description:** Runs a command with a timeout. Returns `{exit_code, timed_out}`.
- **Example:** `run_timeout("sleep 5", 2000)`

---

## S

### `sar()`
- **Module:** `sysmon`
- **Signature:** `sar() -> string`
- **Description:** System activity report.
- **Example:** `sar()`

### `sec(x)`
- **Module:** `math`
- **Signature:** `sec(x: number) -> float`
- **Description:** Returns the secant of `x` (1/cos(x)).
- **Example:** `sec(1) → 1.8508157176809255`

### `sech(x)`
- **Module:** `math`
- **Signature:** `sech(x: number) -> float`
- **Description:** Returns the hyperbolic secant of `x` (1/cosh(x)).
- **Example:** `sech(1) → 0.6480542736638855`

### `sed(pattern, replacement, path[, in_place])`
- **Module:** `textproc`
- **Signature:** `sed(pattern: string, replacement: string, path: string, in_place: bool = false) -> string`
- **Description:** Search/replace in a file. With `in_place`, modifies the file.
- **Example:** `sed("foo", "bar", "data.txt")`

### `set(list_or_map, index_or_key, value)`
- **Module:** `collection`
- **Signature:** `set(container: list|map, index_or_key, value) -> none`
- **Description:** Sets by index (list) or key (map).
- **Example:** `set([1,2], 1, 99) → [1,99]`

### `Set(x)`
- **Module:** `casting`
- **Signature:** `Set(x: any) -> set`
- **Description:** Converts to a mutable set.
- **Example:** `Set([1,2,2]) → {1,2}`

### `set_env(name, value)`
- **Module:** `shell`
- **Signature:** `set_env(name: string, value: string) -> none`
- **Description:** Alias for `env_set()`.
- **Example:** `set_env("LOG_LEVEL", "debug")`

### `shift(list)`
- **Module:** `list`
- **Signature:** `shift(list: list) -> any`
- **Description:** Removes and returns the first element of `list`.
- **Example:** `shift([1,2]) → 1`

### `sin(x)`
- **Module:** `math`
- **Signature:** `sin(x: number) -> float`
- **Description:** Returns the sine of `x` (radians).
- **Example:** `sin(PI/2) → 1.0`

### `sinh(x)`
- **Module:** `math`
- **Signature:** `sinh(x: number) -> float`
- **Description:** Returns the hyperbolic sine of `x`.
- **Example:** `sinh(1) → 1.1752011936438014`

### `size(collection)`
- **Module:** `list`
- **Signature:** `size(collection: string|list|tuple|set|frozen_set|map) -> int`
- **Description:** Alias for `len()`.
- **Example:** `size([1,2,3]) → 3`

### `sleep(ms)`
- **Module:** `datetime`
- **Signature:** `sleep(ms: int) -> none`
- **Description:** Sleeps for `ms` milliseconds.
- **Example:** `sleep(100)`

### `sleep_sec(s)`
- **Module:** `datetime`
- **Signature:** `sleep_sec(s: number) -> none`
- **Description:** Sleeps for `s` seconds.
- **Example:** `sleep_sec(2.5)`

### `slice(list, start[, end])`
- **Module:** `list`
- **Signature:** `slice(list: list, start: int, end: int = -1) -> list`
- **Description:** Returns a sub‑list from `start` to `end`.
- **Example:** `slice([0,1,2,3], 1, 3) → [1,2]`

### `sort(list)`
- **Module:** `list`
- **Signature:** `sort(list: list) -> list`
- **Description:** Sorts `list` in ascending order **in‑place**. Returns the list.
- **Example:** `sort([3,1,2]) → [1,2,3]`

### `sort_desc(list)`
- **Module:** `list`
- **Signature:** `sort_desc(list: list) -> list`
- **Description:** Sorts `list` in descending order **in‑place**. Returns the list.
- **Example:** `sort_desc([1,3,2]) → [3,2,1]`

### `sort_file(path[, reverse])`
- **Module:** `textproc`
- **Signature:** `sort_file(path: string, reverse: bool = false) -> list[string]`
- **Description:** Returns sorted lines from a file.
- **Example:** `sort_file("data.txt")`

### `source_file(path)`
- **Module:** `shell`
- **Signature:** `source_file(path: string) -> string`
- **Description:** Reads a file and returns its content (like `cat`).
- **Example:** `source_file("data.txt")`

### `spawn(command)`
- **Module:** `process`
- **Signature:** `spawn(command: string) -> int`
- **Description:** Spawns a subprocess and returns its PID immediately.
- **Example:** `spawn("server")`

### `split(str, sep)`
- **Module:** `string`
- **Signature:** `split(str: string, sep: string) -> list[string]`
- **Description:** Splits `str` by `sep` into a list.
- **Example:** `split("a,b,c", ",") → ["a","b","c"]`

### `sqrt(x)`
- **Module:** `math`
- **Signature:** `sqrt(x: number) -> number`
- **Description:** Returns the square root of `x` (complex for negative).
- **Example:** `sqrt(16) → 4`

### `ss()`
- **Module:** `net`
- **Signature:** `ss() -> string`
- **Description:** Socket statistics (alias for `netstat` on some systems).
- **Example:** `ss()`

### `starts_with(str, prefix)`
- **Module:** `string`
- **Signature:** `starts_with(str: string, prefix: string) -> bool`
- **Description:** Returns `true` if `str` starts with `prefix`.
- **Example:** `starts_with("hello", "he") → true`

### `stat(path)`
- **Module:** `fs`
- **Signature:** `stat(path: string) -> map`
- **Description:** Returns full file metadata: `{path, is_file, is_dir, is_symlink, size, modified_time, permissions, type}`.
- **Example:** `stat("data.txt")`

### `stem(path)`
- **Module:** `fs`
- **Signature:** `stem(path: string) -> string`
- **Description:** Returns the filename without extension.
- **Example:** `stem("file.txt") → "file"`

### `strace(command)`
- **Module:** `process`
- **Signature:** `strace(command: string) -> string`
- **Description:** Traces system calls (Unix only).
- **Example:** `strace("ls")`

### `str(x)`
- **Module:** `type`
- **Signature:** `str(x: any) -> string`
- **Description:** Converts `x` to its string representation.
- **Example:** `str(42) → "42"`

### `String(x)` (capitalised alias)
- **Module:** `casting`
- **Signature:** `String(x: any) -> string`
- **Description:** Alias for `str()`.
- **Example:** `String(42) → "42"`

### `strings(path[, min_len])`
- **Module:** `fs`
- **Signature:** `strings(path: string, min_len: int = 4) -> list[string]`
- **Description:** Extracts printable ASCII strings from a binary file.
- **Example:** `strings("binary.dat", 4)`

### `substr(str, start, len)`
- **Module:** `string`
- **Signature:** `substr(str: string, start: int, len: int) -> string`
- **Description:** Extracts a substring of length `len` starting at `start`.
- **Example:** `substr("hello", 1, 3) → "ell"`

### `sum(list)`
- **Module:** `list`
- **Signature:** `sum(list: list) -> number`
- **Description:** Returns the sum of all numbers in `list`.
- **Example:** `sum([1,2,3]) → 6`

### `symlink(target, link_path)`
- **Module:** `fs`
- **Signature:** `symlink(target: string, link_path: string) -> none`
- **Description:** Creates a symbolic link.
- **Example:** `symlink("data.txt", "link.txt")`

### `sys_info()`
- **Module:** `process`
- **Signature:** `sys_info() -> map`
- **Description:** Returns OS and hardware info: `{os, arch, kernel, hostname, cpu_count}`.
- **Example:** `sys_info()["os"] → "linux"`

---

## T

### `tail(path, n=10)`
- **Module:** `textproc`
- **Signature:** `tail(path: string, n: int = 10) -> list[string]`
- **Description:** Returns the last `n` lines of a file.
- **Example:** `tail("log.txt", 20)`

### `tail_follow(path, n=10, duration_ms=5000)`
- **Module:** `textproc`
- **Signature:** `tail_follow(path: string, n: int = 10, duration_ms: int = 5000) -> list[string]`
- **Description:** Follows a file for `duration_ms`, returning new lines.
- **Example:** `tail_follow("log.txt", 5, 10000)`

### `tan(x)`
- **Module:** `math`
- **Signature:** `tan(x: number) -> float`
- **Description:** Returns the tangent of `x` (radians).
- **Example:** `tan(PI/4) → 1.0`

### `tanh(x)`
- **Module:** `math`
- **Signature:** `tanh(x: number) -> float`
- **Description:** Returns the hyperbolic tangent of `x`.
- **Example:** `tanh(0) → 0.0`

### `tar_create(src, dest)`
- **Module:** `archive`
- **Signature:** `tar_create(src: string, dest: string) -> bool`
- **Description:** Creates a tar archive (auto‑detects compression from extension).
- **Example:** `tar_create("project/", "project.tar.gz")`

### `tar_extract(src[, dest])`
- **Module:** `archive`
- **Signature:** `tar_extract(src: string, dest: string = ".") -> bool`
- **Description:** Extracts a tar archive.
- **Example:** `tar_extract("project.tar.gz", "out/")`

### `tee(content, path)`
- **Module:** `textproc`
- **Signature:** `tee(content: string, path: string) -> string`
- **Description:** Writes `content` to `path` AND returns it.
- **Example:** `tee("hello", "out.txt")`

### `telnet_connect(host, port)`
- **Module:** `net`
- **Signature:** `telnet_connect(host: string, port: int) -> bool`
- **Description:** Tests TCP connectivity to `host:port`.
- **Example:** `telnet_connect("example.com", 80)`

### `temp_dir()`
- **Module:** `fs`
- **Signature:** `temp_dir() -> string`
- **Description:** Returns the OS temporary directory.
- **Example:** `temp_dir() → "/tmp"`

### `time_cmd(command)`
- **Module:** `process`
- **Signature:** `time_cmd(command: string) -> map`
- **Description:** Measures command execution time. Returns `{elapsed_ms, exit_code}`.
- **Example:** `time_cmd("sleep 1")`

### `time_since(ts)`
- **Module:** `datetime`
- **Signature:** `time_since(ts: number) -> float`
- **Description:** Returns seconds elapsed since timestamp `ts`.
- **Example:** `time_since(1234567890)`

### `timestamp()`
- **Module:** `datetime`
- **Signature:** `timestamp() -> int`
- **Description:** Returns the current Unix timestamp (seconds).
- **Example:** `timestamp() → 1234567890`

### `timestamp_ms()`
- **Module:** `datetime`
- **Signature:** `timestamp_ms() -> int`
- **Description:** Returns the current Unix timestamp (milliseconds).
- **Example:** `timestamp_ms() → 1234567890123`

### `to_chars(str)`
- **Module:** `string`
- **Signature:** `to_chars(str: string) -> list[string]`
- **Description:** Splits `str` into individual characters.
- **Example:** `to_chars("abc") → ["a","b","c"]`

### `to_float(x)`
- **Module:** `type`
- **Signature:** `to_float(x: any) -> float`
- **Description:** Converts `x` to a float.
- **Example:** `to_float("3.14") → 3.14`

### `to_int(x)`
- **Module:** `type`
- **Signature:** `to_int(x: any) -> int`
- **Description:** Converts `x` to an integer.
- **Example:** `to_int("42") → 42`

### `to_list(tuple_or_set_or_frozen)`
- **Module:** `collection`
- **Signature:** `to_list(container: tuple|set|frozen_set) -> list`
- **Description:** Converts to a list.
- **Example:** `to_list((1,2)) → [1,2]`

### `to_set(list_or_tuple_or_frozen)`
- **Module:** `collection`
- **Signature:** `to_set(container: list|tuple|frozen_set) -> set`
- **Description:** Converts to a set (deduplicates).
- **Example:** `to_set([1,2,2]) → {1,2}`

### `to_str(x)`
- **Module:** `type`
- **Signature:** `to_str(x: any) -> string`
- **Description:** Alias for `str()`.
- **Example:** `to_str(42) → "42"`

### `to_tuple(list_or_set_or_frozen)`
- **Module:** `collection`
- **Signature:** `to_tuple(container: list|set|frozen_set) -> tuple`
- **Description:** Converts to a tuple.
- **Example:** `to_tuple([1,2]) → (1,2)`

### `toml_read(path)`
- **Module:** `json`
- **Signature:** `toml_read(path: string) -> map`
- **Description:** Reads a TOML file and returns a map.
- **Example:** `toml_read("config.toml")`

### `touch(path)`
- **Module:** `fs`
- **Signature:** `touch(path: string) -> none`
- **Description:** Creates an empty file or updates its timestamp.
- **Example:** `touch("newfile.txt")`

### `tr(from, to, input)`
- **Module:** `textproc`
- **Signature:** `tr(from: string, to: string, input: string) -> string`
- **Description:** Translates characters from `from` to `to` in `input`.
- **Example:** `tr("a-z", "A-Z", "hello") → "HELLO"`

### `traceroute(host)`
- **Module:** `net`
- **Signature:** `traceroute(host: string) -> string`
- **Description:** Traces the network path to `host`.
- **Example:** `traceroute("example.com")`

### `tree(path[, depth])`
- **Module:** `fs`
- **Signature:** `tree(path: string, depth: int = -1) -> string`
- **Description:** Returns a directory tree as a string.
- **Example:** `tree(".", 2)`

### `trim(str)`
- **Module:** `string`
- **Signature:** `trim(str: string) -> string`
- **Description:** Removes leading and trailing whitespace.
- **Example:** `trim("  hi  ") → "hi"`

### `trim_end(str)`
- **Module:** `string`
- **Signature:** `trim_end(str: string) -> string`
- **Description:** Removes trailing whitespace.
- **Example:** `trim_end("  hi  ") → "  hi"`

### `trim_start(str)`
- **Module:** `string`
- **Signature:** `trim_start(str: string) -> string`
- **Description:** Removes leading whitespace.
- **Example:** `trim_start("  hi  ") → "hi  "`

### `true_val()`
- **Module:** `shell`
- **Signature:** `true_val() -> bool`
- **Description:** Returns `true`.
- **Example:** `true_val() → true`

### `Tuple(x)` (capitalised alias)
- **Module:** `casting`
- **Signature:** `Tuple(x: any) -> tuple`
- **Description:** Converts to a tuple.
- **Example:** `Tuple([1,2]) → (1,2)`

### `type(x)`
- **Module:** `type`
- **Signature:** `type(x: any) -> string`
- **Description:** Returns the type name of `x`.
- **Example:** `type(42) → "int"`

### `type_cmd(cmd)`
- **Module:** `shell`
- **Signature:** `type_cmd(cmd: string) -> string`
- **Description:** Describes the type of a command (alias, builtin, etc.).
- **Example:** `type_cmd("ls")`

### `typeof(x)`
- **Module:** `type`
- **Signature:** `typeof(x: any) -> string`
- **Description:** Alias for `type()`.
- **Example:** `typeof(42) → "int"`

---

## U

### `ulimit_info()`
- **Module:** `sysmon`
- **Signature:** `ulimit_info() -> string`
- **Description:** Returns resource limits.
- **Example:** `ulimit_info()`

### `umount_fs(mount_point)`
- **Module:** `sysmon`
- **Signature:** `umount_fs(mount_point: string) -> string`
- **Description:** Unmounts a filesystem (Unix only).
- **Example:** `umount_fs("/mnt")`

### `unalias(name)`
- **Module:** `shell`
- **Signature:** `unalias(name: string) -> none`
- **Description:** Removes an alias.
- **Example:** `unalias("ll")`

### `uname()`
- **Module:** `process`
- **Signature:** `uname() -> map`
- **Description:** Returns OS and kernel info: `{sysname, nodename, release, version, machine}`.
- **Example:** `uname()["sysname"] → "Linux"`

### `union_set(s1, s2)`
- **Module:** `collection`
- **Signature:** `union_set(s1: set, s2: set) -> set`
- **Description:** Returns a new set with elements from both `s1` and `s2`.
- **Example:** `union_set({1,2}, {2,3}) → {1,2,3}`

### `uniq(path)`
- **Module:** `textproc`
- **Signature:** `uniq(path: string) -> list[string]`
- **Description:** Removes consecutive duplicate lines from a file.
- **Example:** `uniq("data.txt")`

### `unique(list)`
- **Module:** `list`
- **Signature:** `unique(list: list) -> list`
- **Description:** Removes duplicates from `list` (preserves order).
- **Example:** `unique([1,2,2,3]) → [1,2,3]`

### `unshift(list, item)`
- **Module:** `list`
- **Signature:** `unshift(list: list, item: any) -> none`
- **Description:** Inserts `item` at the front of `list`.
- **Example:** `unshift([2,3], 1) → [1,2,3]`

### `unzip_archive(src[, dest])`
- **Module:** `archive`
- **Signature:** `unzip_archive(src: string, dest: string = ".") -> bool`
- **Description:** Extracts a zip archive.
- **Example:** `unzip_archive("backup.zip", "out/")`

### `upper(str)`
- **Module:** `string`
- **Signature:** `upper(str: string) -> string`
- **Description:** Converts `str` to uppercase.
- **Example:** `upper("hello") → "HELLO"`

### `uptime()`
- **Module:** `process`
- **Signature:** `uptime() -> int`
- **Description:** Returns system uptime in seconds.
- **Example:** `uptime() → 12345`

---

## V

### `values(map)`
- **Module:** `collection`
- **Signature:** `values(map: map) -> list`
- **Description:** Returns a list of values (insertion order).
- **Example:** `values({"a":1}) → [1]`

### `vmstat()`
- **Module:** `sysmon`
- **Signature:** `vmstat() -> string`
- **Description:** Virtual memory statistics.
- **Example:** `vmstat()`

---

## W

### `w_cmd()`
- **Module:** `sysmon`
- **Signature:** `w_cmd() -> string`
- **Description:** Shows logged‑in users and activity.
- **Example:** `w_cmd()`

### `wait_pid(pid)`
- **Module:** `process`
- **Signature:** `wait_pid(pid: int) -> int`
- **Description:** Waits for the process to finish, returns exit code.
- **Example:** `wait_pid(12345)`

### `watch(command[, interval_secs, count])`
- **Module:** `process`
- **Signature:** `watch(command: string, interval_secs: int = 2, count: int = 1) -> list[string]`
- **Description:** Repeats a command and returns a list of outputs.
- **Example:** `watch("date", 1, 3)`

### `wc(path)`
- **Module:** `textproc`
- **Signature:** `wc(path: string) -> map`
- **Description:** Counts lines, words, bytes. Returns `{lines, words, bytes}`.
- **Example:** `wc("data.txt")`

### `whereis(cmd)`
- **Module:** `shell`
- **Signature:** `whereis(cmd: string) -> string`
- **Description:** Finds the binary, source, and man page of a command.
- **Example:** `whereis("python")`

### `which(cmd)`
- **Module:** `shell`
- **Signature:** `which(cmd: string) -> string`
- **Description:** Returns the full path of an executable.
- **Example:** `which("python") → "/usr/bin/python"`

### `whoami()`
- **Module:** `process`
- **Signature:** `whoami() -> string`
- **Description:** Returns the current username.
- **Example:** `whoami() → "alice"`

### `whois(domain)`
- **Module:** `net`
- **Signature:** `whois(domain: string) -> string`
- **Description:** Returns domain registration info.
- **Example:** `whois("example.com")`

### `write(path, content)`
- **Module:** `os`
- **Signature:** `write(path: string, content: string) -> none`
- **Description:** Writes `content` to `path` (overwrites).
- **Example:** `write("out.txt", "hello")`

### `write_lines(path, list)`
- **Module:** `fs`
- **Signature:** `write_lines(path: string, list: list[string]) -> none`
- **Description:** Writes a list of strings to `path`, each on a new line.
- **Example:** `write_lines("out.txt", ["a","b"])`

---

## X

### `xargs(cmd_template, input_list)`
- **Module:** `textproc`
- **Signature:** `xargs(cmd_template: string, input_list: list) -> list[map]`
- **Description:** Builds and runs commands from list items. Returns `{command, exit_code, stdout}`.
- **Example:** `xargs("echo {}", ["a","b"])`

### `xxd(path)`
- **Module:** `fs`
- **Signature:** `xxd(path: string) -> string`
- **Description:** Returns a hex dump of a file.
- **Example:** `xxd("binary.dat")`

### `xz_compress(path)`
- **Module:** `archive`
- **Signature:** `xz_compress(path: string) -> string`
- **Description:** Compresses `path` with xz. Returns the new file path.
- **Example:** `xz_compress("data.txt") → "data.txt.xz"`

### `xz_decompress(path)`
- **Module:** `archive`
- **Signature:** `xz_decompress(path: string) -> string`
- **Description:** Decompresses an `.xz` file. Returns the output path.
- **Example:** `xz_decompress("data.txt.xz")`

---

## Y

### `yaml_read(path)`
- **Module:** `json`
- **Signature:** `yaml_read(path: string) -> map|list`
- **Description:** Reads a YAML file and returns the parsed data.
- **Example:** `yaml_read("config.yaml")`

### `yes_cmd([text, count])`
- **Module:** `shell`
- **Signature:** `yes_cmd(text: string = "y", count: int = 100) -> string`
- **Description:** Returns `text` repeated `count` times.
- **Example:** `yes_cmd("hello", 3) → "hello\nhello\nhello"`

### `yield` (expression)
- **Module:** `core` (language)
- **Description:** Produces a value from a generator. See [Generators](../core/generators.md).

---

## Z

### `zfill(str, width)`
- **Module:** `string`
- **Signature:** `zfill(str: string, width: int) -> string`
- **Description:** Zero‑pads `str` on the left to reach `width`. Preserves sign.
- **Example:** `zfill("42", 5) → "00042"`

### `zip(list1, list2)`
- **Module:** `list`
- **Signature:** `zip(list1: list, list2: list) -> list[[any, any]]`
- **Description:** Zips two lists into a list of `[a, b]` pairs (truncates to shortest).
- **Example:** `zip([1,2], ["a","b"]) → [[1,"a"],[2,"b"]]`

### `zip_archive(src, dest)`
- **Module:** `archive`
- **Signature:** `zip_archive(src: string, dest: string) -> bool`
- **Description:** Creates a zip archive.
- **Example:** `zip_archive("data/", "backup.zip")`

### `zip_longest(list1, list2[, fill])`
- **Module:** `list`
- **Signature:** `zip_longest(list1: list, list2: list, fill: any = none) -> list[[any, any]]`
- **Description:** Zips two lists, padding the shorter with `fill`.
- **Example:** `zip_longest([1,2], ["a"], "?") → [[1,"a"],[2,"?"]]`

---

## See Also

- [Built‑ins Reference](../core/builtins.md) – detailed descriptions by category
- [Keywords](keywords.md) – reserved keywords
- [Error Types](error-types.md) – error hierarchy

---

**Previous:** [Error Types](error-types.md)  
**Next:** [Gotchas & Pitfalls](../advanced/gotchas.md)