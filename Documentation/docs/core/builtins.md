# Built‑ins – Complete Reference

> **Previous:** [Decorators](decorators.md)  
> **Next:** [Structs](../oop/oop-structs.md)

---

Xell ships with a comprehensive set of built‑in functions that are always available in the global scope, plus additional modules that can be loaded with `bring * from "module"`. They cover input/output, type conversion, collection manipulation, string handling, mathematics, filesystem operations, process management, networking, and more.

## Two‑Tier System

| Tier | Availability | Modules |
|------|--------------|---------|
| **Tier 1** | Always available – no import needed | `io`, `math`, `type`, `collection`, `util`, `os`, `hash`, `string`, `list`, `map`, `bytes`, `generator`, `shell`, `casting` |
| **Tier 2** | Requires `bring * from "module"` | `datetime`, `regex`, `fs`, `textproc`, `process`, `sysmon`, `net`, `archive`, `json` |

---

## Quick Reference

| Function | Module | What it does |
|----------|--------|--------------|
| `print(x, …)` | Tier 1 | Prints values to stdout, returns `0` |
| `input([prompt])` | Tier 1 | Reads a line from stdin |
| `exit([code])` | Tier 1 | Exits the process |
| `len(x)` | Tier 1 | Returns length of a string, list, tuple, set, map |
| `type(x)` | Tier 1 | Returns the type name as a string |
| `str(x)` | Tier 1 | Converts to string |
| `num(x)` | Tier 1 | Converts to number (int/float) |
| `to_int(x)` | Tier 1 | Converts to integer |
| `to_float(x)` | Tier 1 | Converts to float |
| `to_str(x)` | Tier 1 | Alias for `str` |
| `contains(container, elem)` | Tier 1 | Checks membership |
| `range(start, [end], [step])` | Tier 1 | Returns a list of integers |
| `assert(condition[, message])` | Tier 1 | Throws if condition is false |
| `hashme(x)` | Tier 1 | Returns hash of any hashable value |
| `format(template, …)` | Tier 1 | Python‑style string formatting |
| `push(list, item)` | Tier 1 | Appends to list |
| `pop(list)` | Tier 1 | Removes and returns last element |
| `shift(list)` | Tier 1 | Removes and returns first element |
| `unshift(list, item)` | Tier 1 | Inserts at front |
| `insert(list, index, item)` | Tier 1 | Inserts at position |
| `remove_val(list, val)` | Tier 1 | Removes first occurrence |
| `sort(list)` | Tier 1 | Returns sorted ascending (new list) |
| `sort_desc(list)` | Tier 1 | Returns sorted descending |
| `reverse(str_or_list)` | Tier 1 | Returns reversed copy |
| `slice(list, start, [end])` | Tier 1 | Returns a slice |
| `flatten(list)` | Tier 1 | Flattens one level |
| `unique(list)` | Tier 1 | Removes duplicates (preserves order) |
| `first(list)` | Tier 1 | Returns first element |
| `last(list)` | Tier 1 | Returns last element |
| `zip(list1, list2)` | Tier 1 | Zips two lists into pairs |
| `zip_longest(list1, list2, fill)` | Tier 1 | Zips with padding |
| `enumerate(list, start=0)` | Tier 1 | Returns `[index, value]` pairs |
| `sum(list)` | Tier 1 | Sums all numbers |
| `avg(list)` | Tier 1 | Average (always float) |
| `min(list_or_a, b)` | Tier 1 | Smallest value |
| `max(list_or_a, b)` | Tier 1 | Largest value |
| `size(collection)` | Tier 1 | Alias for `len` |
| `join(list, sep)` | Tier 1 | Joins strings with separator |
| `split(str, sep)` | Tier 1 | Splits string into list |
| `index_of(list_or_str, sub)` | Tier 1 | Finds first index, or `-1` |
| `keys(map)` | Tier 1 | Returns list of keys |
| `values(map)` | Tier 1 | Returns list of values |
| `entries(map)` | Tier 1 | Returns `[key, value]` pairs |
| `from_entries(list)` | Tier 1 | Builds map from `[key, value]` pairs |
| `get(map, key[, default])` | Tier 1 | Gets value or default |
| `merge(map1, map2)` | Tier 1 | Merges two maps |
| `delete_key(map, key)` | Tier 1 | Removes a key, returns removed value |
| `set(list_or_map, index_or_key, value)` | Tier 1 | Sets by index (list) or key (map) |
| `has(map_or_set, key)` | Tier 1 | Checks if key exists |
| `add(set, elem)` | Tier 1 | Adds element to set |
| `remove(list_or_set_or_map, index_or_key)` | Tier 1 | Removes from list, set, or map |
| `to_set(list_or_tuple_or_frozen)` | Tier 1 | Converts to set |
| `to_tuple(list_or_set_or_frozen)` | Tier 1 | Converts to tuple |
| `to_list(tuple_or_set_or_frozen)` | Tier 1 | Converts to list |
| `union_set(s1, s2)` | Tier 1 | Set union |
| `intersect(s1, s2)` | Tier 1 | Set intersection |
| `diff(s1, s2)` | Tier 1 | Set difference |
| `PI`, `E`, `INF` | Tier 1 | Math constants |
| `floor(x)` | Tier 1 | Rounds down |
| `ceil(x)` | Tier 1 | Rounds up |
| `round(x[, n])` | Tier 1 | Rounds to nearest integer or n decimals |
| `abs(x)` | Tier 1 | Absolute value |
| `mod(a, b)` | Tier 1 | Modulo (a % b) |
| `sqrt(x)` | Tier 1 | Square root |
| `pow(x, y)` | Tier 1 | Exponentiation (x ** y) |
| `sin(x)`, `cos(x)`, `tan(x)` | Tier 1 | Trigonometric (radians) |
| `cot(x)`, `sec(x)`, `csc(x)` | Tier 1 | Reciprocal trig |
| `asin(x)`, `acos(x)`, `atan(x)`, `atan2(y,x)` | Tier 1 | Inverse trig |
| `acot(x)`, `asec(x)`, `acsc(x)` | Tier 1 | Inverse reciprocal trig |
| `sinh(x)`, `cosh(x)`, `tanh(x)` | Tier 1 | Hyperbolic |
| `coth(x)`, `sech(x)`, `csch(x)` | Tier 1 | Reciprocal hyperbolic |
| `asinh(x)`, `acosh(x)`, `atanh(x)` | Tier 1 | Inverse hyperbolic |
| `acoth(x)`, `asech(x)`, `acsch(x)` | Tier 1 | Inverse reciprocal hyperbolic |
| `log(x)` | Tier 1 | Natural logarithm |
| `log10(x)` | Tier 1 | Base‑10 logarithm |
| `log2(x)` | Tier 1 | Base‑2 logarithm |
| `clamp(x, lo, hi)` | Tier 1 | Constrains value |
| `random()` | Tier 1 | Random float in [0,1) |
| `random_int(a, b)` | Tier 1 | Random integer in [a, b] |
| `random_choice(list)` | Tier 1 | Picks random element |
| `is_nan(x)` | Tier 1 | Checks if NaN |
| `is_inf(x)` | Tier 1 | Checks if infinite |
| `hex(n)` | Tier 1 | Integer → hex string |
| `bin(n)` | Tier 1 | Integer → binary string |
| `factorial(n)` | Tier 1 | n! |
| `gcd(a, b)` | Tier 1 | Greatest common divisor |
| `lcm(a, b)` | Tier 1 | Least common multiple |
| `type(x)` | Tier 1 | Type name |
| `str(x)` | Tier 1 | String representation |
| `int(x)` | Tier 1 | Convert to integer |
| `float(x)` | Tier 1 | Convert to float |
| `num(x)` | Tier 1 | Convert to number |
| `complex(real, imag)` | Tier 1 | Create complex number |
| `real(z)` | Tier 1 | Real part |
| `imag(z)` | Tier 1 | Imaginary part |
| `conjugate(z)` | Tier 1 | Complex conjugate |
| `magnitude(z)` | Tier 1 | Magnitude / absolute value |
| `Int(x)` | Tier 1 | Alias for `int` |
| `Float(x)` | Tier 1 | Alias for `float` |
| `String(x)` | Tier 1 | Alias for `str` |
| `Complex(a, b)` | Tier 1 | Alias for `complex` |
| `Bool(x)` | Tier 1 | Convert to bool |
| `number(x)` | Tier 1 | Alias for `num` |
| `auto(x)` | Tier 1 | Auto‑cast to best type |
| `List(x)` | Tier 1 | Convert to list |
| `Tuple(x)` | Tier 1 | Convert to tuple |
| `Set(x)` | Tier 1 | Convert to set |
| `iSet(x)` | Tier 1 | Convert to frozen set |
| `~List(x)` | Tier 1 | List with auto‑cast elements |
| `~Tuple(x)` | Tier 1 | Tuple with auto‑cast elements |
| `~Set(x)` | Tier 1 | Set with auto‑cast elements |
| `~iSet(x)` | Tier 1 | Frozen set with auto‑cast elements |
| `trim(str)` | Tier 1 | Removes leading/trailing whitespace |
| `trim_start(str)` | Tier 1 | Removes leading whitespace |
| `trim_end(str)` | Tier 1 | Removes trailing whitespace |
| `upper(str)` | Tier 1 | Converts to uppercase |
| `lower(str)` | Tier 1 | Converts to lowercase |
| `replace(str, old, new)` | Tier 1 | Replaces all occurrences |
| `replace_first(str, old, new)` | Tier 1 | Replaces first occurrence |
| `starts_with(str, prefix)` | Tier 1 | Checks prefix |
| `ends_with(str, suffix)` | Tier 1 | Checks suffix |
| `substr(str, start, len)` | Tier 1 | Extracts substring |
| `char_at(str, n)` | Tier 1 | Gets character at index |
| `repeat(str, n)` | Tier 1 | Repeats string |
| `pad_start(str, n, ch)` | Tier 1 | Pads at start |
| `pad_end(str, n, ch)` | Tier 1 | Pads at end |
| `center(str, width, fill)` | Tier 1 | Centers string |
| `ljust(str, width, fill)` | Tier 1 | Left‑justifies |
| `rjust(str, width, fill)` | Tier 1 | Right‑justifies |
| `zfill(str, width)` | Tier 1 | Zero‑pads on left |
| `count(str_or_list, sub)` | Tier 1 | Counts occurrences |
| `is_empty(str_or_list_or_map)` | Tier 1 | Checks emptiness |
| `is_numeric(str)` | Tier 1 | Checks if numeric |
| `is_alpha(str)` | Tier 1 | Checks if all alpha |
| `lines(str)` | Tier 1 | Splits into lines |
| `to_chars(str)` | Tier 1 | Splits into characters |
| `bytes(value)` | Tier 1 | Converts to bytes |
| `encode(str[, encoding])` | Tier 1 | String → bytes (UTF‑8) |
| `decode(bytes[, encoding])` | Tier 1 | Bytes → string (UTF‑8) |
| `byte_at(bytes, index)` | Tier 1 | Gets byte value |
| `byte_len(bytes)` | Tier 1 | Length in bytes |
| `bytes_concat(a, b)` | Tier 1 | Concatenates bytes |
| `bytes_slice(bytes, start[, end])` | Tier 1 | Slices bytes |
| `hash(value[, algo])` | Tier 1 | Hash value (fnv1a/djb2/murmur3/siphash) |
| `hash_seed(value, seed[, algo])` | Tier 1 | Seeded hash |
| `is_hashable(value)` | Tier 1 | Checks hashability |
| `next(generator)` | Tier 1 | Resumes generator, returns yielded value |
| `is_exhausted(generator)` | Tier 1 | Checks if generator is done |
| `gen_collect(generator)` | Tier 1 | Exhausts generator, returns list |
| `mkdir(path)` | Tier 1 | Creates directory |
| `rm(path)` | Tier 1 | Removes file/directory |
| `cp(src, dst)` | Tier 1 | Copies file/directory |
| `mv(src, dst)` | Tier 1 | Moves file/directory |
| `exists(path)` | Tier 1 | Checks existence |
| `is_file(path)` | Tier 1 | Checks if file |
| `is_dir(path)` | Tier 1 | Checks if directory |
| `ls(path)` | Tier 1 | Lists directory contents |
| `read(path)` | Tier 1 | Reads file as string |
| `write(path, content)` | Tier 1 | Writes string to file |
| `append(path, content)` | Tier 1 | Appends to file |
| `file_size(path)` | Tier 1 | Returns file size in bytes |
| `cwd()` | Tier 1 | Current working directory |
| `pwd()` | Tier 1 | Alias for `cwd` |
| `cd(path)` | Tier 1 | Changes working directory |
| `abspath(path)` | Tier 1 | Absolute path |
| `basename(path)` | Tier 1 | Filename component |
| `dirname(path)` | Tier 1 | Parent directory |
| `ext(path)` | Tier 1 | File extension |
| `realpath(path)` | Tier 1 | Canonical absolute path |
| `join_path(parts…)` | Tier 1 | Joins path components |
| `normalize(path)` | Tier 1 | Resolves `.` and `..` |
| `is_absolute(path)` | Tier 1 | Checks if absolute |
| `relative_path(path, base)` | Tier 1 | Path relative to base |
| `home_dir()` | Tier 1 | User home directory |
| `temp_dir()` | Tier 1 | OS temp directory |
| `env_get(name)` | Tier 1 | Gets environment variable |
| `env_set(name, value)` | Tier 1 | Sets environment variable |
| `env_unset(name)` | Tier 1 | Unsets environment variable |
| `env_has(name)` | Tier 1 | Checks environment variable |
| `run(command)` | Tier 1 | Runs command, returns exit code |
| `run_capture(command)` | Tier 1 | Runs command, returns `{exit_code, stdout, stderr}` |
| `pid()` | Tier 1 | Current process ID |
| `set_e()` | Tier 1 | Enables exit‑on‑error mode |
| `unset_e()` | Tier 1 | Disables exit‑on‑error mode |
| `exit_code()` | Tier 1 | Last command's exit code |
| `error(msg…)` | Tier 1 | Prints to stderr |
| `clear()` | Tier 1 | Clears terminal |
| `reset()` | Tier 1 | Resets terminal |
| `logout()` | Tier 1 | Exits with code 0 |
| `alias(name, cmd)` | Tier 1 | Defines shell alias |
| `unalias(name)` | Tier 1 | Removes shell alias |
| `export_env(name, val)` | Tier 1 | Sets environment variable |
| `env_list()` | Tier 1 | Returns all environment variables as map |
| `printenv([name])` | Tier 1 | Returns env var or all as map |
| `set_env(name, val)` | Tier 1 | Alias for `export_env` |
| `which(cmd)` | Tier 1 | Finds executable path |
| `whereis(cmd)` | Tier 1 | Finds binary, source, man page |
| `type_cmd(cmd)` | Tier 1 | Describes command type |
| `man(topic)` | Tier 1 | Shows manual page |
| `history()` | Tier 1 | Returns command history |
| `yes_cmd([text, count])` | Tier 1 | Returns repeated text |
| `true_val()` | Tier 1 | Returns `true` |
| `false_val()` | Tier 1 | Returns `false` |
| `source_file(path)` | Tier 1 | Reads file as string |

---

## Tier 2 Modules

| Module | Functions | What it does |
|--------|-----------|--------------|
| **datetime** | `now()`, `timestamp()`, `timestamp_ms()`, `format_date()`, `parse_date()`, `sleep()`, `sleep_sec()`, `time_since()` | Date/time operations |
| **regex** | `regex_match()`, `regex_match_full()`, `regex_find()`, `regex_find_all()`, `regex_replace()`, `regex_replace_all()`, `regex_split()`, `regex_groups()` | Regular expressions |
| **fs** | `ls_all()`, `touch()`, `cat()`, `read_lines()`, `write_lines()`, `symlink()`, `hardlink()`, `ln()`, `readlink()`, `chmod()`, `chown()`, `chgrp()`, `stat()`, `modified_time()`, `created_time()`, `find()`, `find_regex()`, `locate()`, `glob()`, `file_diff()`, `tree()`, `extension()`, `stem()`, `disk_usage()`, `disk_free()`, `xxd()`, `strings()` | Extended filesystem operations |
| **textproc** | `head()`, `tail()`, `tail_follow()`, `grep()`, `grep_regex()`, `grep_recursive()`, `sed()`, `awk()`, `cut()`, `sort_file()`, `uniq()`, `wc()`, `tee()`, `tr()`, `patch()`, `less()`, `more()`, `xargs()` | Text processing (Unix‑style) |
| **process** | `ps()`, `kill()`, `kill_name()`, `pkill()`, `pgrep()`, `pidof()`, `pstree()`, `jobs()`, `bg()`, `fg()`, `nohup()`, `nice()`, `wait_pid()`, `ppid()`, `spawn()`, `run_timeout()`, `getuid()`, `is_root()`, `id()`, `whoami()`, `hostname()`, `uname()`, `uptime()`, `time_cmd()`, `watch()`, `strace()`, `lsof()`, `sys_info()`, `os_name()`, `arch()` | Process management |
| **sysmon** | `free()`, `vmstat()`, `iostat()`, `mpstat()`, `sar()`, `cpu_count()`, `cpu_usage()`, `mem_total()`, `mem_free()`, `mem_used()`, `lscpu()`, `lsmem()`, `lspci()`, `lsusb()`, `lsblk()`, `fdisk()`, `mount_fs()`, `umount_fs()`, `dmesg()`, `journalctl()`, `w_cmd()`, `last_logins()`, `ulimit_info()`, `cal()`, `date_str()` | System monitoring |
| **net** | `ping()`, `http_get()`, `http_post()`, `http_put()`, `http_delete()`, `download()`, `dns_lookup()`, `nslookup()`, `host_lookup()`, `whois()`, `traceroute()`, `netstat()`, `ss()`, `ifconfig()`, `ip_cmd()`, `route()`, `iptables()`, `ufw()`, `nc()`, `telnet_connect()`, `rsync()`, `local_ip()`, `public_ip()` | Networking |
| **archive** | `zip_archive()`, `unzip_archive()`, `tar_create()`, `tar_extract()`, `gzip_compress()`, `gunzip_decompress()`, `bzip2_compress()`, `bunzip2_decompress()`, `xz_compress()`, `xz_decompress()` | Archive & compression |
| **json** | `json_parse()`, `json_stringify()`, `json_pretty()`, `json_read()`, `json_write()`, `csv_parse()`, `csv_read()`, `csv_write()`, `toml_read()`, `yaml_read()` | Data format parsing/serialization |
| **threading** | `thread_spawn()`, `thread_join()`, `thread_done()`, `thread_count()`, `mutex_create()`, `mutex_lock()`, `mutex_unlock()`, `mutex_try_lock()` | Concurrency |

---

## Detailed Module Reference

### Tier 1 – Always Available

#### Input / Output (`io`)

##### `print(value, …)`

Prints all arguments to stdout, separated by a space, followed by a newline. Returns `0`.

```xell
print("Hello", "world", 42)   # Hello world 42
```

**Output:**
```
Hello world 42
```

##### `input([prompt])`

Reads a line from stdin. An optional prompt string is printed before reading.

```xell
let name = input("Enter your name: ")
print("Hello, " + name)
```

**Expected interaction:**
```
Enter your name: Alice
Hello, Alice
```

##### `exit([code])`

Terminates the process with the given exit code (default `0`).

```xell
if not file_exists("data.txt") : exit(1)
```

---

#### Type Conversion and Inspection (`type`)

##### `type(x)` / `typeof(x)`

Returns the type name of `x` as a string.

```xell
print(type(42))      # int
print(type("text"))  # string
print(type([]))      # list
```

##### `str(x)` / `to_str(x)`

Converts `x` to its string representation.

```xell
let s = str(42)   # "42"
```

##### `num(x)`

Converts `x` to a number (integer or float). Works on strings and numbers.

```xell
num("3.14")   # 3.14
num("42")     # 42 (int)
```

##### `to_int(x)`

Explicitly converts to an integer. For strings, parses decimal; for floats, truncates.

```xell
to_int("42")     # 42
to_int(3.9)      # 3
```

##### `to_float(x)`

Converts to a float.

```xell
to_float("3.14")   # 3.14
to_float(42)       # 42.0
```

##### `complex(real, imag)` / `Complex(a, b)`

Creates a complex number.

```xell
let z = complex(2, 3)   # 2 + 3i
let w = complex(5.5)    # 5.5 + 0i
```

##### `real(z)`, `imag(z)`, `conjugate(z)`, `magnitude(z)`

Complex number utilities.

```xell
real(2 + 3i)      # 2.0
imag(2 + 3i)      # 3.0
conjugate(2 + 3i) # 2 - 3i
magnitude(3 + 4i) # 5.0
```

##### `Int(x)`, `Float(x)`, `String(x)`, `Complex(a,b)`, `Bool(x)`

Capitalised aliases for `int`, `float`, `str`, `complex`, and boolean conversion.

```xell
Int("42")   # 42
Float("3.14") # 3.14
Bool(0)     # false
```

##### `number(x)`

Alias for `num()` – auto‑detects int or float from string.

##### `auto(x)`

Auto‑casts a string to the best Xell type (int, float, complex, bool, none, list, tuple, set, or string).

```xell
auto("42")      # 42 (int)
auto("3.14")    # 3.14 (float)
auto("true")    # true (bool)
auto("[1,2,3]") # [1, 2, 3] (list)
```

##### `List(x)`, `Tuple(x)`, `Set(x)`, `iSet(x)`

Converts to the target container type. Elements are kept as strings when coming from a string.

```xell
List("1,2,3")      # ["1", "2", "3"]
Tuple("a,b,c")     # ("a", "b", "c")
Set("1,2,2,3")     # {"1", "2", "3"} (deduplicated)
```

##### `~List(x)`, `~Tuple(x)`, `~Set(x)`, `~iSet(x)`

Smart‑cast versions: auto‑detect the best type for each element.

```xell
~List("1,2,3")     # [1, 2, 3] (ints)
~Tuple("true,3.14") # (true, 3.14) (bool, float)
```

---

#### Utility (`util`)

##### `assert(condition, [message])`

If `condition` is false, throws an `AssertionError` with the optional message.

```xell
assert(x > 0, "x must be positive")
```

##### `format(template, …)`

Python‑style string formatting with `{}` placeholders.

```xell
format("Hello, {}!", "world")   # "Hello, world!"
format("{1} + {0} = {2}", 2, 3, 5) # "3 + 2 = 5"
format("Pi = {:.2f}", PI)       # "Pi = 3.14"
```

**Supported format specifiers:** `d` (integer), `f` (float), alignment (`<`, `>`, `^`), width, precision.

---

#### Collections (`collection`)

##### `len(x)`

Returns the length of strings, lists, tuples, sets, frozen sets, and maps.

```xell
len("abc")       # 3
len([1,2,3])     # 3
len({"a":1})     # 1
```

##### `contains(container, elem)` / `has(container, key)`

Checks membership.

```xell
contains([1,2,3], 2)          # true
contains("hello", "lo")       # true
contains({"a":1}, "a")        # true
```

##### `range([start], end, [step])`

Returns a list of integers.

```xell
range(5)            # [0,1,2,3,4]
range(1, 5)         # [1,2,3,4]
range(0, 10, 2)     # [0,2,4,6,8]
```

##### `push(list, item)`, `pop(list)`, `shift(list)`, `unshift(list, item)`, `insert(list, index, item)`, `remove_val(list, val)`

List manipulation – see [Standard Library Collections](../stdlib/stdlib-collections.md) for details.

##### `sort(list)`, `sort_desc(list)`, `reverse(str_or_list)`, `slice(list, start, [end])`, `flatten(list)`, `unique(list)`, `first(list)`, `last(list)`

More list operations.

##### `zip(list1, list2)`, `zip_longest(list1, list2, fill)`, `enumerate(list, start=0)`

List combinators.

```xell
zip([1,2], ["a","b"])          # [[1,"a"], [2,"b"]]
zip_longest([1,2], ["a"], "?") # [[1,"a"], [2,"?"]]
enumerate(["a","b"], 1)        # [[1,"a"], [2,"b"]]
```

##### `sum(list)`, `avg(list)`, `min(list_or_a, b)`, `max(list_or_a, b)`, `size(collection)`

Numeric list utilities.

```xell
sum([1,2,3])     # 6
avg([1,2,3])     # 2.0
min(5, 10)       # 5
max([1,5,3])     # 5
```

##### `join(list, sep)`, `split(str, sep)`, `index_of(list_or_str, sub)`, `count(str_or_list, sub)`

String/list utilities.

##### `keys(map)`, `values(map)`, `entries(map)`, `from_entries(list)`, `get(map, key[, default])`, `merge(map1, map2)`, `delete_key(map, key)`

Map utilities.

```xell
let m = { "x": 10, "y": 20 }
keys(m)          # ["x", "y"]
entries(m)       # [["x", 10], ["y", 20]]
get(m, "z", 0)   # 0
merge(m, { "z": 30 })  # { "x": 10, "y": 20, "z": 30 }
```

##### `set(list_or_map, index_or_key, value)`

Sets by index (list) or key (map).

```xell
let l = [1,2,3]
set(l, 1, 99)    # [1,99,3]

let m = { "a": 1 }
set(m, "b", 2)   # { "a": 1, "b": 2 }
```

##### `add(set, elem)`, `remove(list_or_set_or_map, index_or_key)`

Set and collection removal.

##### `to_set(list_or_tuple_or_frozen)`, `to_tuple(list_or_set_or_frozen)`, `to_list(tuple_or_set_or_frozen)`

Type conversions between collection types.

##### `union_set(s1, s2)`, `intersect(s1, s2)`, `diff(s1, s2)`

Set operations.

```xell
union_set({1,2}, {2,3})   # {1,2,3}
intersect({1,2}, {2,3})   # {2}
diff({1,2}, {2,3})        # {1}
```

---

#### Mathematics (`math`)

##### Constants

- `PI` – π ≈ 3.141592653589793
- `E` – Euler's number ≈ 2.718281828459045
- `INF` – positive infinity

##### Basic Arithmetic

| Function | Description |
|----------|-------------|
| `floor(x)` | Rounds down to the nearest integer. |
| `ceil(x)` | Rounds up to the nearest integer. |
| `round(x[, n])` | Rounds to the nearest integer (ties to even) or n decimals. |
| `abs(x)` | Absolute value (works on int, float, complex). |
| `mod(a, b)` | Modulo (a % b). |
| `sqrt(x)` | Square root (returns complex for negative input). |
| `pow(x, y)` | x ** y. |
| `clamp(x, lo, hi)` | Constrains x to [lo, hi]. |

##### Trigonometric

| Function | Description |
|----------|-------------|
| `sin(x)`, `cos(x)`, `tan(x)` | Trigonometric (radians). |
| `cot(x)`, `sec(x)`, `csc(x)` | Reciprocal trig. |
| `asin(x)`, `acos(x)`, `atan(x)`, `atan2(y,x)` | Inverse trig. |
| `acot(x)`, `asec(x)`, `acsc(x)` | Inverse reciprocal trig. |
| `sinh(x)`, `cosh(x)`, `tanh(x)` | Hyperbolic. |
| `coth(x)`, `sech(x)`, `csch(x)` | Reciprocal hyperbolic. |
| `asinh(x)`, `acosh(x)`, `atanh(x)` | Inverse hyperbolic. |
| `acoth(x)`, `asech(x)`, `acsch(x)` | Inverse reciprocal hyperbolic. |

##### Logarithms

| Function | Description |
|----------|-------------|
| `log(x)` | Natural logarithm. |
| `log10(x)` | Base‑10 logarithm. |
| `log2(x)` | Base‑2 logarithm. |

##### Random

| Function | Description |
|----------|-------------|
| `random()` | Random float in [0, 1). |
| `random_int(a, b)` | Random integer in [a, b] (inclusive). |
| `random_choice(list)` | Picks a random element from a list. |

##### Other

| Function | Description |
|----------|-------------|
| `is_nan(x)` | Checks if x is NaN. |
| `is_inf(x)` | Checks if x is infinite. |
| `hex(n)` | Integer → hex string (no prefix). |
| `bin(n)` | Integer → binary string (no prefix). |
| `factorial(n)` | n! (n ≤ 20). |
| `gcd(a, b)` | Greatest common divisor. |
| `lcm(a, b)` | Least common multiple. |

```xell
floor(3.9)      # 3
round(3.5)      # 4
sqrt(-1)        # 0 + 1i
random_int(1, 6) # 1..6
gcd(12, 18)     # 6
```

---

#### String (`string`)

##### Trimming

`trim(str)`, `trim_start(str)`, `trim_end(str)` – removes whitespace.

##### Case Conversion

`upper(str)`, `lower(str)` – converts case.

##### Search

`starts_with(str, prefix)`, `ends_with(str, suffix)`, `index_of(str_or_list, sub)`, `count(str_or_list, sub)`.

##### Substring

`substr(str, start, len)`, `char_at(str, n)`.

##### Padding

`pad_start(str, n, ch)`, `pad_end(str, n, ch)`, `center(str, width, fill)`, `ljust(str, width, fill)`, `rjust(str, width, fill)`, `zfill(str, width)`.

```xell
pad_start("42", 5, "0")   # "00042"
center("abc", 7, "-")     # "--abc--"
zfill("42", 5)            # "00042"
```

##### Other

`repeat(str, n)`, `reverse(str_or_list)`, `is_empty(str_or_list_or_map)`, `is_numeric(str)`, `is_alpha(str)`, `lines(str)`, `to_chars(str)`.

```xell
repeat("ha", 3)      # "hahaha"
lines("a\nb\nc")     # ["a", "b", "c"]
to_chars("abc")      # ["a", "b", "c"]
```

---

#### Bytes (`bytes`)

##### `bytes(value)`

Converts string to bytes, or creates from list of ints.

```xell
bytes("hello")          # b"hello"
bytes([72, 101, 108])   # b"Hel"
```

##### `encode(str[, encoding])`

String → bytes (UTF‑8 default).

```xell
encode("hello")         # b"hello"
```

##### `decode(bytes[, encoding])`

Bytes → string (UTF‑8 default).

```xell
decode(b"hello")        # "hello"
```

##### `byte_at(bytes, index)`

Gets byte value (0‑255) at index.

```xell
let b = b"hello"
byte_at(b, 0)           # 104
```

##### `byte_len(bytes)`

Returns length in bytes.

##### `bytes_concat(a, b)`

Concatenates two byte strings.

##### `bytes_slice(bytes, start[, end])`

Slices bytes.

---

#### Hash (`hash`)

##### `hash(value[, algorithm])`

Returns hash of any hashable value. Algorithms: `fnv1a` (default), `djb2`, `murmur3`, `siphash`.

```xell
hash("hello")          # 123456789
hash("hello", "djb2")  # 987654321
```

##### `hash_seed(value, seed[, algorithm])`

Seeded hashing.

```xell
hash_seed("hello", 42)   # seeded FNV-1a
```

##### `is_hashable(value)`

Returns `true` if the value can be used as a set element or map key.

```xell
is_hashable([1,2,3])   # false (list is mutable)
is_hashable((1,2,3))   # true (tuple is immutable)
```

---

#### Generator (`generator`)

##### `next(generator)`

Resumes a generator, returns the next yielded value. Throws `RuntimeError` if exhausted.

##### `is_exhausted(generator)`

Returns `true` if the generator has finished.

##### `gen_collect(generator)`

Exhausts the generator and returns a list of all yielded values.

```xell
fn gen(): yield 1; yield 2; yield 3 ;
let g = gen()
gen_collect(g)   # [1, 2, 3]
```

---

#### OS & Shell (`os`, `shell`)

##### Filesystem (Tier 1)

`mkdir`, `rm`, `cp`, `mv`, `exists`, `is_file`, `is_dir`, `ls`, `read`, `write`, `append`, `file_size`, `cwd`, `pwd`, `cd`, `abspath`, `basename`, `dirname`, `ext`, `realpath`, `join_path`, `normalize`, `is_absolute`, `relative_path`, `home_dir`, `temp_dir`

See [Standard Library FS](../stdlib/stdlib-fs.md) for details.

##### Environment (Tier 1)

`env_get`, `env_set`, `env_unset`, `env_has`, `export_env`, `env_list`, `printenv`, `set_env`

##### Process (Tier 1)

`run`, `run_capture`, `pid`, `set_e`, `unset_e`, `exit_code`

```xell
let result = run_capture("ls -la")
print(result["exit_code"])   # 0
print(result["stdout"])
```

##### Shell Utilities (Tier 1)

`error`, `clear`, `reset`, `logout`, `alias`, `unalias`, `which`, `whereis`, `type_cmd`, `man`, `history`, `yes_cmd`, `true_val`, `false_val`, `source_file`

```xell
alias("ll", "ls -la")
ll()   # runs ls -la
which("python")   # "/usr/bin/python"
```

---

### Tier 2 – Require `bring`

#### DateTime (`bring * from "datetime"`)

| Function | Description |
|----------|-------------|
| `now()` | Returns current date/time as a map `{year, month, day, hour, minute, second, weekday, yearday}`. |
| `timestamp()` | Unix timestamp in seconds (integer). |
| `timestamp_ms()` | Unix timestamp in milliseconds (integer). |
| `format_date(date_map, fmt)` | Formats a date map using `strftime` patterns. |
| `parse_date(str, fmt)` | Parses a date string into a map. |
| `sleep(ms)` | Sleeps for N milliseconds. |
| `sleep_sec(s)` | Sleeps for N seconds. |
| `time_since(ts)` | Seconds elapsed since a timestamp (float). |

```xell
bring * from "datetime"

let d = now()
print(d["year"])   # 2026
format_date(d, "%Y-%m-%d")   # "2026-07-23"
sleep(100)   # pause 100ms
```

---

#### Regex (`bring * from "regex"`)

| Function | Description |
|----------|-------------|
| `regex_match(str, pattern)` | `true` if pattern matches anywhere. |
| `regex_match_full(str, pattern)` | `true` only if the entire string matches. |
| `regex_find(str, pattern)` | First match string, or `""`. |
| `regex_find_all(str, pattern)` | List of all match strings. |
| `regex_replace(str, pattern, replacement)` | Replace first occurrence. |
| `regex_replace_all(str, pattern, replacement)` | Replace all occurrences. |
| `regex_split(str, pattern)` | Split string by regex pattern. |
| `regex_groups(str, pattern)` | Capture groups from the first match. |

```xell
bring * from "regex"

regex_match("hello123", "[0-9]+")  # true
regex_find_all("a1 b2 c3", "[a-z][0-9]")  # ["a1", "b2", "c3"]
regex_replace_all("foo bar", "o", "a")   # "faa bar"
```

---

#### FS – Extended Filesystem (`bring * from "fs"`)

| Function | Description |
|----------|-------------|
| `ls_all(path)` | List including hidden files. |
| `touch(path)` | Creates empty file or updates timestamp. |
| `cat(path)` | File contents as string. |
| `read_lines(path)` | Returns list of lines. |
| `write_lines(path, list)` | Writes list of strings to file. |
| `symlink(target, link_path)` | Creates symbolic link. |
| `hardlink(target, link_path)` | Creates hard link. |
| `ln(target, link, soft=true)` | Create link (default soft). |
| `readlink(path)` | Resolves symlink to real path. |
| `chmod(path, mode)` | Changes permissions. |
| `chown(path, user, group)` | Changes owner (Unix only). |
| `chgrp(path, group)` | Changes group (Unix only). |
| `stat(path)` | Full metadata as map. |
| `modified_time(path)` | Last modification timestamp. |
| `created_time(path)` | Creation timestamp. |
| `find(dir, pattern)` | Recursively find files matching glob. |
| `find_regex(dir, pattern)` | Recursively find files matching regex. |
| `locate(name)` | Finds file by exact name from CWD. |
| `glob(pattern)` | Expands glob relative to CWD. |
| `file_diff(file1, file2)` | Line‑by‑line diff. |
| `tree(path[, depth])` | Directory tree as string. |
| `extension(path)` | Alias for `ext`. |
| `stem(path)` | Filename without extension. |
| `disk_usage(path)` | Total size in bytes. |
| `disk_free(path)` | Free disk space on the volume. |
| `xxd(path)` | Hex dump of file. |
| `strings(path[, min_len])` | Extract printable ASCII strings. |

```xell
bring * from "fs"

touch("newfile.txt")
let info = stat("newfile.txt")
print(info["size"])   # 0
tree(".")   # prints directory tree
```

---

#### Text Processing (`bring * from "textproc"`)

| Function | Description |
|----------|-------------|
| `head(path, n=10)` | First N lines → list. |
| `tail(path, n=10)` | Last N lines → list. |
| `tail_follow(path, n=10, duration_ms=5000)` | Follow file growth. |
| `grep(pattern, path)` | Search for pattern → list of lines. |
| `grep_regex(pattern, path)` | Search with regex. |
| `grep_recursive(pattern, dir)` | Search recursively → `{file, line_number, text}`. |
| `sed(pattern, replacement, path[, in_place])` | Search/replace in file. |
| `awk(program, path[, delim])` | Field extraction (e.g., `"$1 $3"`). |
| `cut(path, delim, fields)` | Extract columns. |
| `sort_file(path[, reverse])` | Sort lines alphabetically. |
| `uniq(path)` | Remove consecutive duplicate lines. |
| `wc(path)` | Count lines, words, bytes. |
| `tee(content, path)` | Write content to file AND return it. |
| `tr(from, to, input)` | Translate characters. |
| `patch(file, patchfile)` | Apply simple unified diff patch. |
| `less(path, page_size=20)` | Paginated file view. |
| `more(path, page_size=20)` | Simpler paginated view. |
| `xargs(cmd_template, input_list)` | Build commands from list items. |

```xell
bring * from "textproc"

let first10 = head("data.txt")
grep("error", "log.txt")   # returns matching lines
wc("report.txt")   # {lines: 42, words: 512, bytes: 4096}
```

---

#### Process (`bring * from "process"`)

| Function | Description |
|----------|-------------|
| `ps()` | List processes → `{pid, name, ppid, mem}`. |
| `kill(pid[, signal])` | Send signal to process. |
| `kill_name(name)` | Kill all processes by name. |
| `pkill(pattern)` | Kill processes matching pattern. |
| `pgrep(pattern)` | Find PIDs matching pattern → list of ints. |
| `pidof(name)` | Get PID by exact process name. |
| `pstree([pid])` | Process tree as string. |
| `jobs()` | List background jobs. |
| `bg(pid)` | Resume process in background. |
| `fg(pid)` | Bring process to foreground. |
| `nohup(cmd)` | Run command immune to hangup. |
| `nice(cmd[, level])` | Run with adjusted priority. |
| `wait_pid(pid)` | Wait for process to finish. |
| `ppid()` | Parent process ID. |
| `spawn(cmd)` | Spawn subprocess, return PID. |
| `run_timeout(cmd, timeout_ms)` | Run command with timeout. |
| `getuid()` | User ID (Unix only). |
| `is_root()` | Check if running as root/admin. |
| `id()` | Return map with uid, gid, groups. |
| `whoami()` | Current username. |
| `hostname()` | Machine hostname. |
| `uname()` | OS and kernel info → map. |
| `uptime()` | System uptime in seconds. |
| `time_cmd(cmd)` | Measure command execution time. |
| `watch(cmd, interval_secs=2, count=1)` | Repeat command. |
| `strace(cmd)` | Trace system calls (Unix only). |
| `lsof()` | List open files. |
| `sys_info()` | OS name, version, architecture → map. |
| `os_name()` | `"linux"`, `"windows"`, or `"macos"`. |
| `arch()` | `"x86_64"`, `"arm64"`, etc. |

```xell
bring * from "process"

let procs = ps()
for p in procs:
    if p["name"] == "python" : print(p["pid"])
;
whoami()   # "alice"
uptime()   # 12345 (seconds)
```

---

#### System Monitoring (`bring * from "sysmon"`)

| Function | Description |
|----------|-------------|
| `free()` | RAM info → `{total, used, free, swap_total, swap_free}`. |
| `vmstat()` | Virtual memory statistics. |
| `iostat()` | CPU and disk I/O statistics. |
| `mpstat()` | Per‑CPU statistics. |
| `sar()` | System activity report. |
| `cpu_count()` | Number of logical CPU cores. |
| `cpu_usage()` | Current CPU usage percentage. |
| `mem_total()` | Total RAM in bytes. |
| `mem_free()` | Free RAM in bytes. |
| `mem_used()` | Used RAM in bytes. |
| `lscpu()` | Detailed CPU info. |
| `lsmem()` | Memory info. |
| `lspci()` | List PCI devices. |
| `lsusb()` | List USB devices. |
| `lsblk()` | List block storage devices. |
| `fdisk(device)` | Partition info. |
| `mount_fs(device, mount_point)` | Mount filesystem (Unix). |
| `umount_fs(mount_point)` | Unmount filesystem (Unix). |
| `dmesg()` | Kernel ring buffer messages. |
| `journalctl([n])` | Query systemd journal. |
| `w_cmd()` | Show logged‑in users. |
| `last_logins([n])` | Login history. |
| `ulimit_info()` | Resource limits. |
| `cal([month, year])` | Print calendar. |
| `date_str([format])` | Current date/time as formatted string. |

```xell
bring * from "sysmon"

let mem = free()
print("Free RAM: " + str(mem["free"] / 1024 / 1024) + " MB")
cpu_usage()   # 12.5
cal(7, 2026)   # July 2026 calendar
```

---

#### Network (`bring * from "net"`)

| Function | Description |
|----------|-------------|
| `ping(host, count=4)` | Test connectivity. |
| `http_get(url[, headers])` | HTTP GET → `{status, body, headers}`. |
| `http_post(url, body[, headers])` | HTTP POST. |
| `http_put(url, body[, headers])` | HTTP PUT. |
| `http_delete(url[, headers])` | HTTP DELETE. |
| `download(url, path)` | Download file. |
| `dns_lookup(hostname)` | Resolve hostname → list of IPs. |
| `nslookup(hostname)` | System `nslookup` wrapper. |
| `host_lookup(hostname)` | `host` command wrapper. |
| `whois(domain)` | Domain registration info. |
| `traceroute(host)` | Trace network path. |
| `netstat()` | Show active network connections. |
| `ss()` | Socket statistics. |
| `ifconfig()` | Show network interfaces. |
| `ip_cmd(args)` | Modern network interface management. |
| `route()` | Show routing table. |
| `iptables(args)` | Linux firewall rules. |
| `ufw(args)` | Uncomplicated firewall frontend. |
| `nc(host, port)` | Raw TCP connection test. |
| `telnet_connect(host, port)` | Telnet connectivity test. |
| `rsync(src, dest)` | Sync files using `rsync`. |
| `local_ip()` | Local machine IP address. |
| `public_ip()` | Public IP via external service. |

```xell
bring * from "net"

let result = http_get("https://api.example.com/data")
print(result["status"])   # 200
print(result["body"])

let ips = dns_lookup("example.com")
print(ips[0])   # "93.184.216.34"
public_ip()     # "203.0.113.42"
```

---

#### Archive (`bring * from "archive"`)

| Function | Description |
|----------|-------------|
| `zip_archive(src, dest)` | Create zip archive. |
| `unzip_archive(src[, dest])` | Extract zip archive. |
| `tar_create(src, dest)` | Create tar archive (auto‑detects compression). |
| `tar_extract(src[, dest])` | Extract tar archive. |
| `gzip_compress(path)` | Compress with gzip. |
| `gunzip_decompress(path)` | Decompress `.gz` file. |
| `bzip2_compress(path)` | Compress with bzip2 (Unix only). |
| `bunzip2_decompress(path)` | Decompress `.bz2` file. |
| `xz_compress(path)` | Compress with xz (Unix only). |
| `xz_decompress(path)` | Decompress `.xz` file. |

```xell
bring * from "archive"

zip_archive("data/", "backup.zip")
tar_create("project/", "project.tar.gz")
gzip_compress("large_file.txt")   # creates large_file.txt.gz
```

---

#### JSON & Data Formats (`bring * from "json"`)

| Function | Description |
|----------|-------------|
| `json_parse(str)` | Parse JSON → XObject. |
| `json_stringify(obj)` | Serialize to JSON (compact). |
| `json_pretty(obj[, indent])` | Serialize to JSON (pretty). |
| `json_read(path)` | Read JSON file. |
| `json_write(path, obj)` | Write JSON file. |
| `csv_parse(str[, sep])` | Parse CSV → list of maps. |
| `csv_read(path[, sep])` | Read CSV file → list of maps. |
| `csv_write(path, data)` | Write CSV from list of maps. |
| `toml_read(path)` | Read TOML file → map. |
| `yaml_read(path)` | Read YAML file → map/list. |

```xell
bring * from "json"

let obj = json_parse('{"name": "Alice", "age": 30}')
print(obj["name"])   # Alice
let json = json_pretty(obj, 2)
write("data.json", json)

let rows = csv_read("data.csv")
for row in rows:
    print(row["name"])
;
```

---

## What NOT to Do

### 1. Using `to_int` with Hex Strings

```xell
to_int("0x10")   # ❌ throws ConversionError – only decimal strings are supported
```

**Fix:** Use `num("0x10")` or the literal `0x10`.

### 2. Using `split` with an Empty Separator

```xell
split("abc", "")   # ❌ throws ValueError
```

### 3. Forgetting to `bring` Tier 2 Modules

```xell
json_parse('{"a":1}')   # ❌ error – json_parse not defined
```

**Fix:** `bring * from "json"` first.

### 4. Passing a Non‑List to `sort` / `sort_desc`

```xell
sort("hello")   # ❌ TypeError
```

### 5. Modifying a Frozen Set

```xell
let s = iSet([1,2,3])
add(s, 4)   # ❌ ImmutabilityError
```

### 6. Using `random_int` with the Same Min and Max

Works (returns that value), but not useful.

### 7. Expecting `run_capture` to Return a Map Without `bring`

`run_capture` is Tier 1 – it's always available.

### 8. Using `exit` Without an Argument

Works (exits with code 0).

### 9. Forgetting That `json_read` and `json_write` Are Tier 2

```xell
json_read("data.json")   # ❌ requires bring * from "json"
```

### 10. Expecting `http_get` to Work Without `curl`

`http_get` and other network functions rely on `curl` being installed.

---

## See Also

- [Decorators](decorators.md) – `@debug`, `@track`, `@profile`, etc.
- [Shell](shell.md) – `$cmd`, `|>`, `&&`, `||`, `set_e()`
- [Functions](functions.md) – function definitions and overloading
- [Error Handling](error-handling.md) – `throw` and `try`/`catch`

---

**Previous:** [Decorators](decorators.md)  
**Next:** [Structs](../oop/oop-structs.md)