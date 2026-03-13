# Xell Language Features

Verified from the current workspace source and the passing test binaries on 2026-03-12.

This file intentionally lists only features that are present in the codebase now.

## Core language features

- Variables, assignment, immutable bindings, and block scoping
- Functions, lambdas, default parameters, variadic parameters, closures, recursion
- Statements and expressions for `if`, `for`, `while`, `loop`, `do ... while`, `incase`, `break`, `continue`, `give`
- Statement-mode and expression-mode loops
- `try` / `catch` / `finally` and `throw`
- String interpolation with expression support and format specs such as `{value:.2f}` and `{n:05d}`
- Destructuring assignment:
  - flat list destructuring
  - nested list destructuring
  - rest capture in list destructuring
  - map destructuring
  - object/class-instance destructuring by field/property name
- Comprehensions:
  - list comprehensions
  - set comprehensions
  - map comprehensions
  - nested `for` and `if` filters
- Chained comparisons
- Ternary expressions
- Slice syntax for strings, lists, and tuples
- `in` and `not in`
- `is` and `is not`
- Spread/rest syntax with `...`
- Pattern matching with `incase`, type patterns, guards, binding, and expression form

## Data types and values

- `int`
- `float`
- `complex`
- `string`
- `bool`
- `none`
- `list`
- `tuple`
- `map`
- `set`
- `frozen_set`
- `bytes`
- functions
- generators
- modules
- struct/class/interface instances
- error instances

## Literals and text features

- Decimal, hexadecimal, octal, and binary integer literals
- Scientific-notation floats
- Imaginary and complex-number literals
- Digit separators in numeric literals
- Double-quoted and single-quoted strings
- Raw strings
- Triple-quoted multiline strings
- Escape sequences including hex and Unicode escapes

## Operators and callable behavior

- Arithmetic: `+`, `-`, `*`, `/`, `%`, `**`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Augmented assignment for variables, members, nested members, and index targets
- Prefix/postfix `++` and `--`
- Member access with `->`
- Indexing and slicing
- Function calls and paren-less call forms where supported by the parser
- Pipe operator `|>`

## OOP and meta-features

- `struct`, `class`, `interface`, `abstract`, `mixin`
- Inheritance and interface implementation
- Static members
- Access control (`public`, `protected`, `private`)
- Properties with getters/setters
- Magic methods, including support exercised by tests for:
  - `__add__`, `__sub__`, `__mul__`, `__div__`, `__mod__`
  - `__eq__`, `__ne__`, `__lt__`, `__gt__`, `__le__`, `__ge__`
  - `__print__`, `__str__`, `__neg__`, `__len__`
  - `__get__`, `__set__`, `__call__`, `__contains__`
  - `__iter__`, `__next__`, `__hash__`
  - `__enter__`, `__exit__`
- `let ... be` context-manager / RAII support
- Decorator syntax is present; verified uses in the workspace include `@eager` and `@convert`

## Generators, async, and iteration

- `yield`
- Lazy generator consumption in `for` loops
- Generator destructuring in `for`
- Expression-mode `for` over generators
- `async` / `await`
- Iterator protocol support for:
  - generators
  - objects whose `__iter__` returns a generator
  - objects whose `__iter__` returns an iterator object with `__next__`
  - objects that implement `__next__` directly

## Modules and importing

- `module` definitions
- `export`
- `bring * from module`
- selective `bring`
- aliasing with `as`
- `bring X of module`
- nested modules
- file-based module loading
- `requires`
- module dunder metadata such as `__name__`, `__exports__`, `__submodules__`, `__module__`, `__version__`, `__cached__`, `__args__`

## Errors and debugging features

- Builtin/runtime error hierarchy surfaced to user code as error instances
- Typed catches
- Multi-catch patterns
- Traceback capture on errors
- Debug/trace support in the interpreter
- REPL and shell integration exist in the workspace

## Builtin inventory

The current source registers 393 builtin/interpreter-provided callable names, plus constants `PI`, `E`, and `INF`.

### Always-available modules (Tier 1)

#### `io`

`exit`, `input`, `print`

#### `math`

`abs`, `acos`, `acosh`, `acot`, `acoth`, `acsc`, `acsch`, `asec`, `asech`, `asin`, `asinh`, `atan`, `atan2`, `atanh`, `bin`, `ceil`, `clamp`, `cos`, `cosh`, `cot`, `coth`, `csc`, `csch`, `factorial`, `floor`, `gcd`, `hex`, `is_inf`, `is_nan`, `lcm`, `log`, `log10`, `log2`, `mod`, `pow`, `random`, `random_choice`, `random_int`, `round`, `sec`, `sech`, `sin`, `sinh`, `sqrt`, `tan`, `tanh`, `to_float`, `to_int`

#### `type`

`complex`, `conjugate`, `float`, `imag`, `int`, `magnitude`, `num`, `real`, `str`, `type`, `typeof`

#### `collection`

`add`, `contains`, `diff`, `has`, `intersect`, `keys`, `len`, `pop`, `push`, `range`, `remove`, `set`, `to_list`, `to_set`, `to_tuple`, `union_set`, `values`

#### `util`

`assert`, `format`

#### `os`

`abspath`, `append`, `basename`, `cd`, `cp`, `cwd`, `dirname`, `env_get`, `env_has`, `env_set`, `env_unset`, `exists`, `exit_code`, `ext`, `file_size`, `is_dir`, `is_file`, `ls`, `mkdir`, `mv`, `pid`, `read`, `rm`, `run`, `run_capture`, `set_e`, `unset_e`, `write`

#### `hash`

`hash`, `hash_seed`, `is_hashable`

#### `string`

`center`, `char_at`, `count`, `ends_with`, `index_of`, `is_alpha`, `is_empty`, `is_numeric`, `join`, `lines`, `ljust`, `lower`, `pad_end`, `pad_start`, `repeat`, `replace`, `replace_first`, `reverse`, `rjust`, `split`, `starts_with`, `substr`, `to_chars`, `trim`, `trim_end`, `trim_start`, `upper`, `zfill`

#### `list`

`avg`, `enumerate`, `first`, `flatten`, `insert`, `last`, `max`, `min`, `remove_val`, `shift`, `size`, `slice`, `sort`, `sort_desc`, `sum`, `unique`, `unshift`, `zip`, `zip_longest`

#### `map`

`delete_key`, `entries`, `from_entries`, `get`, `merge`

#### `bytes`

`byte_at`, `byte_len`, `bytes`, `bytes_concat`, `bytes_slice`, `decode`, `encode`, `to_bytes`

#### `generator`

`gen_collect`, `is_exhausted`, `next`

#### `shell`

`alias`, `clear`, `env_list`, `error`, `export_env`, `false_val`, `history`, `history_add`, `logout`, `man`, `printenv`, `reset`, `set_env`, `source_file`, `true_val`, `type_cmd`, `unalias`, `whereis`, `which`, `yes_cmd`

#### `casting`

`auto`, `Bool`, `Complex`, `Float`, `Int`, `~iSet`, `iSet`, `~List`, `List`, `number`, `~Set`, `Set`, `String`, `~Tuple`, `Tuple`

#### Interpreter-provided core callables

`any`, `all`, `map`, `reduce`, `filter`

### Bring-required modules (Tier 2)

#### `datetime`

`format_date`, `now`, `parse_date`, `sleep`, `sleep_sec`, `time_since`, `timestamp`, `timestamp_ms`

#### `regex`

`regex_find`, `regex_find_all`, `regex_groups`, `regex_match`, `regex_match_full`, `regex_replace`, `regex_replace_all`, `regex_split`

#### `fs`

`cat`, `chgrp`, `chmod`, `chown`, `created_time`, `disk_free`, `disk_usage`, `extension`, `file_diff`, `find`, `find_regex`, `glob`, `hardlink`, `home_dir`, `is_absolute`, `is_symlink`, `join_path`, `ln`, `locate`, `ls_all`, `modified_time`, `normalize`, `pwd`, `read_lines`, `readlink`, `realpath`, `relative_path`, `stat`, `stem`, `strings`, `symlink`, `temp_dir`, `touch`, `tree`, `write_lines`, `xxd`

#### `textproc`

`awk`, `cut`, `grep`, `grep_recursive`, `grep_regex`, `head`, `less`, `more`, `patch`, `sed`, `sort_file`, `tail`, `tail_follow`, `tee`, `tr`, `uniq`, `wc`, `xargs`

#### `process`

`arch`, `bg`, `exec_proc`, `fg`, `getuid`, `hostname`, `id`, `is_root`, `jobs`, `kill`, `kill_name`, `lsof`, `nice`, `nohup`, `os_name`, `pgrep`, `pidof`, `pkill`, `ppid`, `ps`, `pstree`, `run_timeout`, `signal_send`, `spawn`, `strace`, `sys_info`, `time_cmd`, `uname`, `uptime`, `wait_pid`, `watch`, `whoami`

#### `sysmon`

`cal`, `cpu_count`, `cpu_usage`, `date_str`, `dmesg`, `fdisk`, `free`, `iostat`, `journalctl`, `last_logins`, `lsblk`, `lscpu`, `lsmem`, `lspci`, `lsusb`, `mem_free`, `mem_total`, `mem_used`, `mount_fs`, `mpstat`, `sar`, `ulimit_info`, `umount_fs`, `vmstat`, `w_cmd`

#### `net`

`dns_lookup`, `download`, `host_lookup`, `http_delete`, `http_get`, `http_post`, `http_put`, `ifconfig`, `ip_cmd`, `iptables`, `local_ip`, `nc`, `netstat`, `nslookup`, `ping`, `public_ip`, `route`, `rsync`, `ss`, `telnet_connect`, `traceroute`, `ufw`, `whois`

#### `archive`

`bunzip2_decompress`, `bzip2_compress`, `gunzip_decompress`, `gzip_compress`, `tar_create`, `tar_extract`, `unzip_archive`, `xz_compress`, `xz_decompress`, `zip_archive`

#### `json`

`csv_parse`, `csv_read`, `csv_write`, `json_parse`, `json_pretty`, `json_read`, `json_stringify`, `json_write`, `toml_read`, `yaml_read`

#### `threading`

`thread_spawn`, `thread_join`, `thread_done`, `thread_count`, `mutex_create`, `mutex_lock`, `mutex_unlock`, `mutex_try_lock`
