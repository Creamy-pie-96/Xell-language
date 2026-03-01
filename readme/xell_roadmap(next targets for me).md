# Xell — Complete Feature & Command Roadmap
> This document tracks everything Xell needs to be a full shell + scripting language.
> Check off items as they get implemented.

---

## Table of Contents
1. [How Commands Work in Xell](#1-how-commands-work-in-xell)
2. [Command Style vs Function Style](#2-command-style-vs-function-style)
3. [Missing Language Features](#3-missing-language-features)
4. [Missing Built-in String Methods](#4-missing-built-in-string-methods)
5. [Missing Built-in List Methods](#5-missing-built-in-list-methods)
6. [Missing Built-in Map Methods](#6-missing-built-in-map-methods)
7. [Missing Math & Numbers](#7-missing-math--numbers)
8. [Missing Date & Time](#8-missing-date--time)
9. [Missing Regex](#9-missing-regex)
10. [File System — Built-ins](#10-file-system--built-ins)
11. [Text / File Content — Built-ins](#11-text--file-content--built-ins)
12. [Process & System — Built-ins](#12-process--system--built-ins)
13. [System Monitoring — Built-ins](#13-system-monitoring--built-ins)
14. [Network — Built-ins](#14-network--built-ins)
15. [Archive & Compression — Built-ins](#15-archive--compression--built-ins)
16. [JSON / Data — Built-ins](#16-json--data--built-ins)
17. [Shell Utilities — Built-ins](#17-shell-utilities--built-ins)
18. [External Commands via PATH (Free — No Implementation Needed)](#18-external-commands-via-path-free--no-implementation-needed)
19. [Missing REPL Features](#19-missing-repl-features)
20. [Nice To Have (Future)](#20-nice-to-have-future)

---

## 1. How Commands Work in Xell

There are **two kinds** of commands in Xell and they work completely differently.

### Kind 1 — Xell Built-ins
Implemented in C++ inside Xell itself. Cross-platform, return values, integrate with the language.
```
mkdir, ls, cd, cp, mv, rm, cat, grep, find, chmod ...
```

### Kind 2 — External Commands via PATH
Binaries on the OS. Xell doesn't implement them — it finds and launches them by searching `$PATH`, exactly like bash does.

```
User types:  git commit -m "hello"
Bash does:   search PATH → find /usr/bin/git → execv("/usr/bin/git", args)
Xell does:   exact same thing — zero difference
```

Every binary installed on the system works in Xell automatically. Three things needed:

```cpp
// 1. PATH searching
std::string find_in_path(const std::string& cmd) {
    std::string path_env = OS::env_get("PATH");
    // split by : on Unix, ; on Windows
    // check each directory for cmd binary
    // return full path or empty string
}

// 2. Argument passing — pass everything after cmd name as argv to the binary

// 3. Exit code handling — for && and || to work correctly
int exit_code = run_process("git", {"commit", "-m", "hello"});
// && only runs next command if exit_code == 0
// || only runs next command if exit_code != 0
```

---

## 2. Command Style vs Function Style

Every Xell built-in supports **both styles**. The parser detects which one based on context.

### The Two Styles

```xell
# Command style — no parens, space separated, bash-like
ls .
mkdir src
cp a.txt b.txt
print "hello"
cd /home/user

# Function style — parens, comma separated, scripting-like
ls(".")
mkdir("src")
cp("a.txt", "b.txt")
print("hello")
cd("/home/user")
```

Both are **100% identical** in behavior.

### When to Use Which

**Command style** — interactive REPL and simple one-liners:
```xell
ls .
cd projects
git status
mkdir new-feature
```

**Function style** — when you need the return value in a script:
```xell
files = ls(".")
content = read("notes.txt")
ok = exists("config.xel")

for f in files :
    print "{f}"
;
```

### Capture vs Fire-and-Forget

```xell
ls .                   # prints to terminal — no capture
files = ls(".")        # captures into variable AND prints

mkdir "src"            # just do it
ok = exists("src")     # need the bool — use function style
```

This mirrors bash — `ls` just prints, `$(ls)` captures. Command style = print form. Function style = capture form.

### How the Parser Tells Them Apart

```
Statement starts with known built-in name?
  YES, followed by (   → function style — parse normally
  YES, followed by anything else → command style — consume rest of line as args
  NO  → variable or user-defined function call
```

### Subcommands Work Too

```xell
env set DEBUG true           # → env_set("DEBUG", "true")
env get HOME                 # → env_get("HOME")
json read config.json        # → json_read("config.json")
http get https://example.com # → http_get("https://example.com")
```

---

## 3. Missing Language Features

### 🔴 Critical

| Feature | Syntax | What It Does |
|---------|--------|--------------|
| `try / catch / finally` | `try : ... ; catch e : ... ;` | Handle runtime errors without crashing |
| `input()` | `x = input("Enter name: ")` | Read line from stdin with optional prompt |
| `exit()` | `exit(0)` | Terminate script with exit code |
| Default parameters | `fn greet(name = "World") :` | Fallback values when args not provided |
| `break` | `break` inside loop | Exit loop immediately |
| `continue` | `continue` inside loop | Skip to next iteration |
| Ternary operator | `x = a if cond else b` | Inline conditional expression |
| Multi-line strings | `"""..."""` | String spanning multiple lines |
| Raw strings | `r"no\escape"` | Backslashes are literal |
| Spread operator | `[...list1, ...list2]` | Merge lists or expand into args |
| Destructuring | `a, b = [1, 2]` | Unpack list or map into variables |
| `switch / match` | `match x : case 1 : ... ;` | Multi-branch pattern matching |
| Pipe operator | `data \| filter \| sort` | Chain operations left to right |
| Augmented assignment | `x += 1`, `x -= 1`, `x *= 2` | Shorthand for `x = x + 1` etc. |

### 🟡 Important

| Feature | Syntax | What It Does |
|---------|--------|--------------|
| Lambda / anonymous fn | `fn(x) : x * 2 ;` | Nameless inline function |
| Variadic functions | `fn foo(...args) :` | Accept any number of arguments |
| String formatting | `format("{:.2f}", 3.14)` | Formatted string with precision |
| `assert` | `assert x > 0` | Throw error if condition false |
| `typeof` | `typeof(x)` | Return type name as string |
| Chained comparison | `0 < x < 10` | Natural range check |

### 🟢 Future

| Feature | What It Does |
|---------|--------------|
| Generators / `yield` | Lazy evaluation — values one at a time |
| Async / await | Non-blocking concurrency |
| Decorators | Wrap functions to add behavior |
| Enums | `enum Color : Red, Green, Blue ;` |
| Type annotations | Optional `fn add(a: number, b: number) -> number :` |
| Set data type | Unordered unique values |
| Tuple | Immutable ordered collection |
| Bytes / binary | Raw binary data type |

---

## 4. Missing Built-in String Methods

| Method | Example | What It Does |
|--------|---------|--------------|
| `split(str, sep)` | `split("a,b,c", ",")` → `["a","b","c"]` | Split string into list by separator |
| `join(list, sep)` | `join(["a","b"], ",")` → `"a,b"` | Join list of strings with separator |
| `trim(str)` | `trim("  hi  ")` → `"hi"` | Remove leading and trailing whitespace |
| `trim_start(str)` | `trim_start("  hi")` → `"hi"` | Remove leading whitespace only |
| `trim_end(str)` | `trim_end("hi  ")` → `"hi"` | Remove trailing whitespace only |
| `upper(str)` | `upper("hello")` → `"HELLO"` | Convert to uppercase |
| `lower(str)` | `lower("HELLO")` → `"hello"` | Convert to lowercase |
| `replace(str, old, new)` | `replace("hello", "l", "r")` → `"herro"` | Replace all occurrences |
| `replace_first(str, old, new)` | `replace_first("hello", "l", "r")` → `"herlo"` | Replace only first occurrence |
| `starts_with(str, prefix)` | `starts_with("hello", "he")` → `true` | Check if starts with prefix |
| `ends_with(str, suffix)` | `ends_with("hello", "lo")` → `true` | Check if ends with suffix |
| `contains(str, sub)` | `contains("hello", "ell")` → `true` | Check if contains substring |
| `index_of(str, sub)` | `index_of("hello", "l")` → `2` | First position of substring, -1 if not found |
| `substr(str, start, len)` | `substr("hello", 1, 3)` → `"ell"` | Extract substring |
| `char_at(str, n)` | `char_at("hello", 0)` → `"h"` | Get character at index |
| `repeat(str, n)` | `repeat("ab", 3)` → `"ababab"` | Repeat string N times |
| `pad_start(str, n, ch)` | `pad_start("5", 3, "0")` → `"005"` | Pad on left to length n |
| `pad_end(str, n, ch)` | `pad_end("5", 3, "0")` → `"500"` | Pad on right to length n |
| `reverse(str)` | `reverse("hello")` → `"olleh"` | Reverse a string |
| `count(str, sub)` | `count("hello", "l")` → `2` | Count occurrences of substring |
| `is_empty(str)` | `is_empty("")` → `true` | Check if string has zero length |
| `is_numeric(str)` | `is_numeric("42")` → `true` | Check if string is a valid number |
| `is_alpha(str)` | `is_alpha("abc")` → `true` | Check if only letters |
| `lines(str)` | `lines("a\nb\nc")` → `["a","b","c"]` | Split into list of lines |
| `to_chars(str)` | `to_chars("hi")` → `["h","i"]` | Split into individual characters |

---

## 5. Missing Built-in List Methods

| Method | Example | What It Does |
|--------|---------|--------------|
| `push(list, val)` | `push(items, "x")` | Append to end |
| `pop(list)` | `pop(items)` | Remove and return last item |
| `shift(list)` | `shift(items)` | Remove and return first item |
| `unshift(list, val)` | `unshift(items, "x")` | Insert at beginning |
| `insert(list, i, val)` | `insert(items, 2, "x")` | Insert at specific index |
| `remove(list, i)` | `remove(items, 0)` | Remove item at index |
| `remove_val(list, val)` | `remove_val(items, "x")` | Remove first occurrence of value |
| `sort(list)` | `sort([3,1,2])` → `[1,2,3]` | Sort ascending |
| `sort_desc(list)` | `sort_desc([3,1,2])` → `[3,2,1]` | Sort descending |
| `reverse(list)` | `reverse([1,2,3])` → `[3,2,1]` | Reverse a list |
| `contains(list, val)` | `contains(items, "x")` → `true` | Check if value exists |
| `index_of(list, val)` | `index_of(items, "x")` → `2` | Find index, -1 if not found |
| `slice(list, start, end)` | `slice(items, 1, 3)` | Extract sublist |
| `join(list, sep)` | `join(["a","b"], ",")` → `"a,b"` | Join into string |
| `flatten(list)` | `flatten([[1,2],[3,4]])` → `[1,2,3,4]` | Flatten one level deep |
| `unique(list)` | `unique([1,1,2,2])` → `[1,2]` | Remove duplicates |
| `map(list, fn)` | `map([1,2,3], fn(x): x*2;)` | Apply function to every element |
| `filter(list, fn)` | `filter([1,2,3], fn(x): x>1;)` | Keep elements where function is true |
| `reduce(list, fn, init)` | `reduce([1,2,3], fn(a,x): a+x;, 0)` | Reduce to single value |
| `any(list, fn)` | `any(items, fn(x): x > 0;)` | True if at least one element matches |
| `all(list, fn)` | `all(items, fn(x): x > 0;)` | True if all elements match |
| `count(list, val)` | `count(items, "x")` | Count occurrences of value |
| `first(list)` | `first(items)` | Get first element |
| `last(list)` | `last(items)` | Get last element |
| `zip(list1, list2)` | `zip([1,2],["a","b"])` → `[[1,"a"],[2,"b"]]` | Pair elements from two lists |
| `range(start, end, step)` | `range(0, 10, 2)` → `[0,2,4,6,8]` | Generate list of numbers |
| `sum(list)` | `sum([1,2,3])` → `6` | Sum all numbers |
| `min(list)` | `min([3,1,2])` → `1` | Smallest value |
| `max(list)` | `max([3,1,2])` → `3` | Largest value |
| `avg(list)` | `avg([1,2,3])` → `2.0` | Average of all numbers |

---

## 6. Missing Built-in Map Methods

| Method | Example | What It Does |
|--------|---------|--------------|
| `keys(map)` | `keys(config)` → `["host","port"]` | Get all keys as list |
| `values(map)` | `values(config)` | Get all values as list |
| `has(map, key)` | `has(config, "host")` → `true` | Check if key exists |
| `delete(map, key)` | `delete(config, "host")` | Remove a key |
| `set(map, key, val)` | `set(config, "port", 8080)` | Set a key to a value |
| `get(map, key, default)` | `get(config, "port", 3000)` | Get value or default if missing |
| `merge(map1, map2)` | `merge(defaults, overrides)` | Merge two maps, second wins on conflict |
| `entries(map)` | `entries(config)` → `[["host","localhost"],...]` | All key-value pairs as list |
| `size(map)` | `size(config)` → `2` | Number of keys |
| `is_empty(map)` | `is_empty({})` → `true` | Check if map has no keys |
| `from_entries(list)` | `from_entries([["a",1],["b",2]])` | Build map from list of pairs |

---

## 7. Missing Math & Numbers

| Function | Example | What It Does |
|----------|---------|--------------|
| `abs(x)` | `abs(-5)` → `5` | Absolute value |
| `floor(x)` | `floor(3.7)` → `3` | Round down |
| `ceil(x)` | `ceil(3.2)` → `4` | Round up |
| `round(x, n)` | `round(3.14159, 2)` → `3.14` | Round to N decimal places |
| `sqrt(x)` | `sqrt(16)` → `4` | Square root |
| `pow(x, y)` | `pow(2, 10)` → `1024` | x to the power y |
| `log(x)` | `log(2.718)` → `1.0` | Natural logarithm |
| `log10(x)` | `log10(100)` → `2` | Base-10 logarithm |
| `sin(x)` | `sin(0)` → `0` | Sine in radians |
| `cos(x)` | `cos(0)` → `1` | Cosine in radians |
| `tan(x)` | `tan(0)` → `0` | Tangent in radians |
| `min(a, b)` | `min(3, 5)` → `3` | Smaller of two values |
| `max(a, b)` | `max(3, 5)` → `5` | Larger of two values |
| `clamp(x, lo, hi)` | `clamp(15, 0, 10)` → `10` | Constrain between lo and hi |
| `random()` | `random()` → `0.472...` | Random float 0.0–1.0 |
| `random_int(a, b)` | `random_int(1, 6)` → `4` | Random integer between a and b |
| `random_choice(list)` | `random_choice(["a","b","c"])` | Pick random item from list |
| `is_nan(x)` | `is_nan(0/0)` → `true` | Check if not a number |
| `is_inf(x)` | `is_inf(1/0)` → `true` | Check if infinity |
| `to_int(x)` | `to_int(3.9)` → `3` | Truncate to integer |
| `to_float(x)` | `to_float("3.14")` → `3.14` | Parse string as float |
| `hex(n)` | `hex(255)` → `"ff"` | Integer to hex string |
| `bin(n)` | `bin(10)` → `"1010"` | Integer to binary string |
| `PI` | `3.14159265...` | Math constant π |
| `E` | `2.71828182...` | Math constant e |
| `INF` | infinity | Positive infinity constant |

---

## 8. Missing Date & Time

| Function | Example | What It Does |
|----------|---------|--------------|
| `now()` | `now()` → `{ year:2026, month:3, day:1, ... }` | Current date/time as map |
| `timestamp()` | `timestamp()` → `1740000000` | Unix timestamp in seconds |
| `timestamp_ms()` | — | Unix timestamp in milliseconds |
| `format_date(ts, fmt)` | `format_date(now(), "%Y-%m-%d")` → `"2026-03-01"` | Format date strftime-style |
| `parse_date(str, fmt)` | `parse_date("2026-03-01", "%Y-%m-%d")` | Parse date string into map |
| `sleep(ms)` | `sleep(1000)` | Pause for N milliseconds |
| `sleep_sec(s)` | `sleep_sec(2)` | Pause for N seconds |
| `time_since(ts)` | `time_since(start)` | Seconds elapsed since timestamp |

---

## 9. Missing Regex

Uses C++ `<regex>` internally — zero external dependencies.

| Function | Example | What It Does |
|----------|---------|--------------|
| `regex_match(str, pattern)` | `regex_match("hello123", "[0-9]+")` → `true` | Check if pattern matches anywhere |
| `regex_match_full(str, pattern)` | `regex_match_full("123", "^[0-9]+$")` → `true` | Check if entire string matches |
| `regex_find(str, pattern)` | `regex_find("hello123", "[0-9]+")` → `"123"` | Find first match |
| `regex_find_all(str, pattern)` | `regex_find_all("a1b2c3", "[0-9]")` → `["1","2","3"]` | Find all matches as list |
| `regex_replace(str, pattern, repl)` | `regex_replace("hello", "l+", "r")` → `"hero"` | Replace first match |
| `regex_replace_all(str, pattern, repl)` | `regex_replace_all("aabb", "[ab]", "x")` → `"xxxx"` | Replace all matches |
| `regex_split(str, pattern)` | `regex_split("a1b2c", "[0-9]")` → `["a","b","c"]` | Split using regex as delimiter |
| `regex_groups(str, pattern)` | `regex_groups("2026-03-01", "(\d+)-(\d+)-(\d+)")` → `["2026","03","01"]` | Capture groups from first match |

---

## 10. File System — Built-ins

Implemented in C++ using `std::filesystem`. Cross-platform, no shelling out.

| Function | Command Style | What It Does |
|----------|--------------|--------------|
| `ls(path)` | `ls .` | List files and directories |
| `ls_all(path)` | `ls_all .` | List including hidden files |
| `cd(path)` | `cd /home/user` | Change working directory |
| `pwd()` | `pwd` | Print working directory |
| `mkdir(path)` | `mkdir src` | Create directory recursively |
| `rm(path)` | `rm temp/` | Delete file or directory |
| `cp(src, dest)` | `cp a.txt b.txt` | Copy file or directory |
| `mv(src, dest)` | `mv old.txt new.txt` | Move or rename |
| `touch(path)` | `touch file.txt` | Create empty file or update timestamp |
| `cat(path)` | `cat notes.txt` | Print file contents to terminal |
| `read(path)` | `content = read("file.txt")` | Read file as string |
| `write(path, content)` | `write "out.txt" data` | Write string to file (overwrites) |
| `append(path, content)` | `append "log.txt" line` | Append to file without overwriting |
| `read_lines(path)` | `lines = read_lines("file.txt")` | Read file into list of lines |
| `write_lines(path, lines)` | — | Write list of lines to file |
| `exists(path)` | `exists "config.xel"` | Check if file or directory exists |
| `is_file(path)` | `is_file "main.xel"` | Check if path is a file |
| `is_dir(path)` | `is_dir "src"` | Check if path is a directory |
| `is_symlink(path)` | — | Check if path is a symbolic link |
| `symlink(target, link)` | `symlink a b` | Create symbolic link |
| `hardlink(target, link)` | `hardlink a b` | Create hard link |
| `ln(target, link, soft)` | `ln a b` | Create link — hard or soft |
| `readlink(path)` | — | Resolve symbolic link to real path |
| `chmod(path, mode)` | `chmod 755 script.xel` | Change permissions — Unix only |
| `chown(path, user, group)` | `chown root:root file` | Change ownership — Unix only |
| `chgrp(path, group)` | `chgrp staff file` | Change group — Unix only |
| `stat(path)` | `stat file.txt` | Get full file metadata as map |
| `file_size(path)` | — | Get file size in bytes |
| `modified_time(path)` | — | Get last modified timestamp |
| `created_time(path)` | — | Get creation timestamp |
| `find(dir, pattern)` | `find . "*.xel"` | Recursively find files matching pattern |
| `find_regex(dir, pattern)` | — | Find files matching regex |
| `locate(name)` | `locate main.xel` | Find file by name across whole system |
| `glob(pattern)` | `glob "src/*.xel"` | Expand glob into list of paths |
| `diff(file1, file2)` | `diff a.txt b.txt` | Show differences between two files |
| `tree(path)` | `tree .` | Print directory structure as tree |
| `basename(path)` | `basename "/a/b/c.txt"` → `"c.txt"` | Filename without directory |
| `dirname(path)` | `dirname "/a/b/c.txt"` → `"/a/b"` | Directory part of path |
| `extension(path)` | `extension "file.txt"` → `".txt"` | Get file extension |
| `stem(path)` | `stem "file.txt"` → `"file"` | Filename without extension |
| `realpath(path)` | `realpath "../../etc"` | Resolve to absolute path |
| `join_path(...parts)` | — | Join path parts with correct OS separator |
| `normalize(path)` | — | Resolve `..` and `.` in path |
| `is_absolute(path)` | — | Check if path is absolute |
| `relative_path(path, base)` | — | Get path relative to base |
| `home_dir()` | `home_dir` | Get current user home directory |
| `temp_dir()` | `temp_dir` | Get OS temp directory |
| `cwd()` | `cwd` | Get working directory as string |
| `disk_usage(path)` | `du .` | Total size of directory in bytes |
| `disk_free(path)` | `df .` | Free disk space on drive |
| `xxd(path)` | `xxd file.bin` | Print hex dump of file |
| `strings(path)` | `strings binary` | Extract printable strings from binary |

---

## 11. Text / File Content — Built-ins

| Function | Command Style | What It Does |
|----------|--------------|--------------|
| `echo(text)` | `echo "hello"` | Print text to stdout — bash-style |
| `printf(fmt, ...args)` | `printf "%s=%d" key val` | Formatted print like C printf |
| `head(path, n)` | `head file.txt 10` | Print first N lines (default 10) |
| `tail(path, n)` | `tail file.txt 10` | Print last N lines (default 10) |
| `tail_follow(path)` | `tail -f log.txt` | Live-follow file as it grows |
| `grep(pattern, path)` | `grep "error" log.txt` | Search file for lines matching pattern |
| `grep_regex(pattern, path)` | — | Search using regex |
| `grep_recursive(pattern, dir)` | `grep -r "TODO" src/` | Search recursively in all files |
| `sed(pattern, repl, path)` | `sed "s/foo/bar" file.txt` | Search and replace in file |
| `awk(program, path)` | `awk "{print $1}" file.txt` | Run awk program on file |
| `cut(path, delim, fields)` | `cut -d, -f1 data.csv` | Extract specific columns from lines |
| `sort_file(path)` | `sort file.txt` | Sort lines alphabetically |
| `uniq(path)` | `uniq file.txt` | Remove duplicate consecutive lines |
| `wc(path)` | `wc file.txt` | Count lines, words, bytes |
| `tee(path)` | `tee output.txt` | Write to both terminal and file |
| `tr(from, to, input)` | `tr a-z A-Z` | Translate or delete characters |
| `patch(file, patchfile)` | `patch file.txt changes.patch` | Apply a diff patch to a file |
| `less(path)` | `less file.txt` | Paginated interactive file view |
| `more(path)` | `more file.txt` | Paginated file view (simpler) |
| `xargs(cmd)` | `xargs rm` | Build and run commands from stdin |

---

## 12. Process & System — Built-ins

| Function | Command Style | What It Does |
|----------|--------------|--------------|
| `ps()` | `ps` | List running processes as list of maps |
| `kill(pid)` | `kill 1234` | Terminate process by PID |
| `kill_name(name)` | `killall node` | Terminate all processes by name |
| `pkill(pattern)` | `pkill "node*"` | Kill processes matching pattern |
| `pgrep(pattern)` | `pgrep node` | Find PIDs matching pattern |
| `pidof(name)` | `pidof nginx` | Get PID by exact name |
| `pstree()` | `pstree` | Display process tree |
| `jobs()` | `jobs` | List background jobs in session |
| `bg(job)` | `bg 1` | Resume job in background |
| `fg(job)` | `fg 1` | Bring background job to foreground |
| `nohup(cmd)` | `nohup server.xel` | Run command immune to terminal close |
| `nice(cmd, level)` | `nice -n 10 cmd` | Run with adjusted CPU priority |
| `wait(pid)` | `wait 1234` | Wait for process to finish |
| `pid()` | `pid` | Get current process ID |
| `ppid()` | `ppid` | Get parent process ID |
| `exec(path, args)` | — | Replace current process — Unix execv |
| `spawn(cmd, args)` | — | Spawn subprocess, return PID immediately |
| `run_timeout(cmd, ms)` | — | Run command, kill if exceeds timeout |
| `signal(sig, fn)` | — | Register OS signal handler |
| `exit(code)` | `exit 0` | Exit with code |
| `getuid()` | — | Get user ID — Unix only |
| `is_root()` | — | Check if running as root/admin |
| `id()` | `id` | Print UID, GID, groups |
| `whoami()` | `whoami` | Print current username |
| `hostname()` | `hostname` | Print machine hostname |
| `uname()` | `uname` | Print OS and kernel info |
| `uptime()` | `uptime` | How long system has been running |
| `time_cmd(fn)` | `time some_fn()` | Measure how long a function takes |
| `watch(cmd, secs)` | `watch ls 2` | Repeat command every N seconds |
| `strace(cmd)` | `strace myprogram` | Trace system calls — Unix only |
| `lsof()` | `lsof` | List open files and owning processes |
| `sys_info()` | — | Map of OS name, version, architecture |
| `os_name()` | — | `"linux"`, `"windows"`, or `"macos"` |
| `arch()` | — | `"x86_64"`, `"arm64"`, etc. |

---

## 13. System Monitoring — Built-ins

| Function | Command Style | What It Does |
|----------|--------------|--------------|
| `free()` | `free` | Show total, used, free RAM and swap |
| `vmstat()` | `vmstat` | Virtual memory statistics |
| `iostat()` | `iostat` | CPU and disk I/O statistics |
| `mpstat()` | `mpstat` | Per-CPU statistics |
| `sar()` | `sar` | System activity report |
| `cpu_count()` | — | Number of logical CPU cores |
| `cpu_usage()` | — | Current CPU usage percentage |
| `mem_total()` | — | Total RAM in bytes |
| `mem_free()` | — | Free RAM in bytes |
| `mem_used()` | — | Used RAM in bytes |
| `lscpu()` | `lscpu` | Detailed CPU info — Unix only |
| `lsmem()` | `lsmem` | Detailed memory info — Unix only |
| `lspci()` | `lspci` | List PCI devices — Unix only |
| `lsusb()` | `lsusb` | List USB devices — Unix only |
| `lsblk()` | `lsblk` | List block storage devices — Unix only |
| `fdisk(device)` | `fdisk /dev/sda` | Show partition info — Unix only |
| `mount(device, point)` | `mount /dev/sdb1 /mnt` | Mount filesystem — Unix only |
| `umount(point)` | `umount /mnt` | Unmount filesystem — Unix only |
| `dmesg()` | `dmesg` | Kernel ring buffer messages — Unix only |
| `journalctl()` | `journalctl` | Query systemd journal — Linux only |
| `w()` | `w` | Show logged in users and what they're doing |
| `last()` | `last` | Show login history |
| `ulimit()` | `ulimit` | Show or set resource limits |
| `cal()` | `cal` | Print a calendar |
| `date()` | `date` | Print current date and time |

---

## 14. Network — Built-ins

| Function | Command Style | What It Does |
|----------|--------------|--------------|
| `ping(host, count)` | `ping google.com` | Test connectivity to host |
| `http_get(url, headers)` | `http get https://example.com` | Send HTTP GET request |
| `http_post(url, body, headers)` | `http post url body` | Send HTTP POST request |
| `http_put(url, body, headers)` | — | Send HTTP PUT request |
| `http_delete(url, headers)` | — | Send HTTP DELETE request |
| `download(url, path)` | `download url file.zip` | Download file from URL |
| `dns_lookup(hostname)` | `dig google.com` | Resolve hostname to IP |
| `nslookup(hostname)` | `nslookup google.com` | Name server lookup |
| `host(hostname)` | `host google.com` | Simple DNS lookup |
| `whois(domain)` | `whois google.com` | Domain registration info |
| `traceroute(host)` | `traceroute google.com` | Trace network path to host |
| `netstat()` | `netstat` | Show active network connections |
| `ss()` | `ss` | Socket statistics — faster than netstat |
| `ifconfig()` | `ifconfig` | Show network interfaces |
| `ip(args)` | `ip addr` | Modern network interface management |
| `route()` | `route` | Show routing table |
| `iptables(args)` | `iptables -L` | Linux firewall rules — Unix only |
| `ufw(args)` | `ufw status` | Uncomplicated firewall frontend — Unix only |
| `nc(host, port)` | `nc google.com 80` | Raw TCP/UDP connection |
| `telnet(host, port)` | `telnet host 23` | Raw TCP connection |
| `rsync(src, dest)` | `rsync src/ dest/` | Sync files locally or over network |
| `local_ip()` | — | Get local machine IP address |
| `public_ip()` | — | Get public IP via external request |

> **Note:** `ssh`, `scp`, `sftp`, `ftp`, `curl`, `wget`, `nmap` are external binaries — they work automatically via PATH searching once installed. No implementation needed.

---

## 15. Archive & Compression — Built-ins

| Function | Command Style | What It Does |
|----------|--------------|--------------|
| `zip(src, dest)` | `zip src/ out.zip` | Create zip archive |
| `unzip(src, dest)` | `unzip out.zip ./` | Extract zip archive |
| `tar_create(src, dest)` | `tar -czf out.tar.gz src/` | Create tar archive |
| `tar_extract(src, dest)` | `tar -xf out.tar.gz ./` | Extract tar archive |
| `gzip(path)` | `gzip file.txt` | Compress with gzip |
| `gunzip(path)` | `gunzip file.txt.gz` | Decompress gzip |
| `bzip2(path)` | `bzip2 file.txt` | Compress with bzip2 |
| `bunzip2(path)` | `bunzip2 file.txt.bz2` | Decompress bzip2 |
| `xz_compress(path)` | `xz file.txt` | Compress with xz |
| `xz_decompress(path)` | `xz -d file.txt.xz` | Decompress xz |

> **Note:** `7z` and `rar/unrar` are external binaries — use them directly via PATH. No implementation needed.

---

## 16. JSON / Data — Built-ins

| Function | Command Style | What It Does |
|----------|--------------|--------------|
| `json_parse(str)` | — | Parse JSON string into Xell value |
| `json_stringify(val)` | — | Convert Xell value to JSON string |
| `json_pretty(val)` | — | Pretty-printed JSON with indentation |
| `json_read(path)` | `json read config.json` | Read and parse JSON file |
| `json_write(path, val)` | `json write config.json data` | Write value to JSON file |
| `csv_parse(str, sep)` | — | Parse CSV string into list of maps |
| `csv_read(path)` | `csv read data.csv` | Read CSV into list of maps |
| `csv_write(path, data)` | — | Write list of maps to CSV |
| `toml_read(path)` | `toml read config.toml` | Read TOML config into map |
| `yaml_read(path)` | `yaml read config.yaml` | Read YAML file into map |

---

## 17. Shell Utilities — Built-ins

| Function | Command Style | What It Does |
|----------|--------------|--------------|
| `print(text)` | `print "hello"` | Print to stdout — Xell native |
| `echo(text)` | `echo "hello"` | Print to stdout — bash-style alias |
| `printf(fmt, ...args)` | `printf "%s=%d\n" k v` | Formatted print |
| `print_err(text)` | — | Print to stderr |
| `clear()` | `clear` | Clear terminal screen |
| `reset()` | `reset` | Full terminal reset including scrollback |
| `exit(code)` | `exit 0` | Exit with code |
| `logout()` | `logout` | Logout of current session |
| `alias(name, cmd)` | `alias ll "ls -la"` | Create command shorthand |
| `unalias(name)` | `unalias ll` | Remove an alias |
| `source(path)` | `source ./setup.xel` | Execute file in current scope |
| `export(name, val)` | `export NODE_ENV production` | Export variable to child processes |
| `env()` | `env` | List all environment variables |
| `printenv(name)` | `printenv HOME` | Print specific environment variable |
| `set_env(name, val)` | — | Set environment variable |
| `which(cmd)` | `which git` | Find executable path in PATH |
| `whereis(cmd)` | `whereis git` | Find binary, source, and man page |
| `type(cmd)` | `type ls` | Show what a command is — builtin/alias/binary |
| `man(cmd)` | `man ls` | Show manual page for command |
| `history()` | `history` | Get command history as list |
| `yes(text)` | `yes` | Repeatedly print string until killed |
| `true()` | `true` | Do nothing, exit code 0 |
| `false()` | `false` | Do nothing, exit code 1 |

---

## 18. External Commands via PATH (Free — No Implementation Needed)

These all work automatically once Xell has PATH searching. Just type them — Xell finds the binary.

### Version Control
| Command | What It Does |
|---------|--------------|
| `git` | Version control |
| `svn` | Subversion |
| `hg` | Mercurial |

### Compilers & Build Tools
| Command | What It Does |
|---------|--------------|
| `gcc` / `g++` | GNU C/C++ compiler |
| `clang` / `clang++` | LLVM C/C++ compiler |
| `nvcc` | NVIDIA CUDA compiler |
| `rustc` | Rust compiler |
| `javac` | Java compiler |
| `go` | Go compiler |
| `tsc` | TypeScript compiler |
| `make` | GNU Make |
| `cmake` | CMake |
| `ninja` | Ninja build system |
| `msbuild` | Microsoft Build — Windows |

### Runtimes & Interpreters
| Command | What It Does |
|---------|--------------|
| `python` / `python3` | Python interpreter |
| `node` | Node.js runtime |
| `java` | Java runtime |
| `ruby` | Ruby interpreter |
| `perl` | Perl interpreter |
| `php` | PHP interpreter |
| `lua` | Lua interpreter |

### Package Managers
| Command | What It Does |
|---------|--------------|
| `apt` / `apt-get` | Debian/Ubuntu packages |
| `yum` / `dnf` | RedHat/Fedora/CentOS packages |
| `pacman` | Arch Linux packages |
| `brew` | macOS Homebrew |
| `winget` | Windows package manager |
| `pip` / `pip3` | Python packages |
| `npm` / `npx` | Node packages |
| `cargo` | Rust packages |
| `gem` | Ruby packages |
| `composer` | PHP packages |

### Containers & Cloud
| Command | What It Does |
|---------|--------------|
| `docker` | Container runtime |
| `docker-compose` | Multi-container apps |
| `kubectl` | Kubernetes CLI |
| `helm` | Kubernetes package manager |
| `terraform` | Infrastructure as code |
| `ansible` | Configuration management |
| `vagrant` | VM management |
| `aws` | Amazon Web Services CLI |
| `gcloud` | Google Cloud CLI |
| `az` | Azure CLI |

### Network Tools (External Binaries)
| Command | What It Does |
|---------|--------------|
| `curl` | HTTP requests |
| `wget` | File downloader |
| `ssh` | Secure shell |
| `scp` | Secure copy over SSH |
| `sftp` | Secure FTP |
| `ftp` | FTP client |
| `nmap` | Network scanner |

### Text Editors (Terminal)
| Command | What It Does |
|---------|--------------|
| `vim` / `nvim` | Vi improved |
| `nano` | Simple terminal editor |
| `emacs` | GNU Emacs |
| `micro` | Modern terminal editor |

### Media & Utilities
| Command | What It Does |
|---------|--------------|
| `ffmpeg` | Video/audio processing |
| `convert` (ImageMagick) | Image processing |
| `pandoc` | Document converter |
| `sqlite3` | SQLite database CLI |
| `htop` | Interactive process viewer |
| `btop` | Modern resource monitor |
| `ncdu` | Disk usage analyzer |

---

## 19. Missing REPL Features

| Feature | What It Does |
|---------|--------------|
| `:history` | Show command history |
| `:clear` | Clear screen |
| `:help` | Show all available commands and built-ins |
| `:reload` | Reload the xell runtime |
| `history()` | Access history as a list from scripts |
| Tab completion | Complete variable names, built-ins, file paths |
| Syntax highlighting | Color keywords, strings, errors as you type in REPL |
| Multi-line paste | Paste multi-line scripts cleanly without each line executing |
| `.xellrc` | Startup config file — runs on every session start |

---

## 20. Nice To Have (Future)

| Feature | What It Does |
|---------|--------------|
| Package manager | `xell install some-package` — third-party Xell modules |
| `bring` from URL | `bring setup from "https://example.com/helpers.xel"` |
| Generators / `yield` | Lazy sequences — values one at a time |
| Async / await | Non-blocking I/O |
| Decorators | Wrap functions to add logging, timing, caching |
| Enums | `enum Color : Red, Green, Blue ;` |
| Type annotations | Optional `fn add(a: number, b: number) -> number :` |
| Set data type | `{1, 2, 3}` — unique unordered values |
| Tuple | `(1, "hello", true)` — immutable ordered |
| Pipe operator | `names \| filter(fn(x): x != "admin";) \| sort` |
| REPL themes | Configurable colors for prompt, output, errors |
| VS Code extension | Syntax highlighting, snippets, error squiggles |
| Man pages | `xell --help command` — built-in documentation |
| Native SSH module | SSH without depending on system ssh binary |
| Native TLS/HTTPS | HTTPS without depending on curl |
