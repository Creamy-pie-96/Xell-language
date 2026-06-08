## ⬅️ What's Previous?

Move back to the **[Error Handling](./Error Handling.md)** to review the foundational concepts.

---

# Built-in Functions

Xell comes with a vast library of built-in functions that provide access to system resources, mathematical operations, and data manipulation.

## 📁 Filesystem & OS

These functions allow you to interact with your computer's files and environment.

### File Operations
| Function | Description | Example |
| :--- | :--- | :--- |
| `mkdir(path)` | Creates a directory (recursive). | `mkdir("logs/2026")` |
| `rm(path)` | Deletes a file or directory. | `rm("temp.txt")` |
| `cp(src, dst)` | Copies a file or directory. | `cp("old.txt", "new.txt")` |
| `mv(src, dst)` | Moves or renames a file/dir. | `mv("old.txt", "new.txt")` |
| `exists(path)` | Checks if a path exists. | `exists("config.json")` |
| `read(path)` | Reads file content as a string. | `content = read("info.txt")` |
| `write(path, str)`| Writes string to a file. | `write("out.txt", "hello")` |
| `append(path, str)`| Appends string to a file. | `append("log.txt", "entry\\n")` |
| `ls(path)` | Lists contents of a directory. | `files = ls("./src")` |
| `touch(path)` | Creates empty file / updates timestamp. | `touch("marker.txt")` |
| `read_lines(path)` | Returns a list of lines from a file. | `lines = read_lines("list.txt")` |

### System & Environment
| Function | Description | Example |
| :--- | :--- | :--- |
| `env_get(name)` | Gets an environment variable. | `home = env_get("HOME")` |
| `env_set(name, v)` | Sets an environment variable. | `env_set("DEBUG", "true")` |
| `cwd()` | Returns current working directory. | `path = cwd()` |
| `run(cmd)` | Runs external command (output to stdout). | `run("ls -la")` |
| `run_capture(cmd)` | Runs command and returns stdout. | `out = run_capture("whoami")` |

---

## 📐 Mathematics

Xell provides comprehensive math support, including trigonometry and randomness.

### Basic Math
| Function | Description | Example |
| :--- | :--- | :--- |
| `abs(x)` | Absolute value. | `abs(-10)` $\rightarrow$ `10` |
| `mod(a, b)` | Modulo (remainder). | `mod(10, 3)` $\rightarrow$ `1` |
| `pow(a, b)` | a raised to power b. | `pow(2, 3)` $\rightarrow$ `8` |
| `sqrt(x)` | Square root. | `sqrt(16)` $\rightarrow$ `4` |
| `round(x, n)` | Rounds to `n` decimal places. | `round(3.1415, 2)` $\rightarrow$ `3.14` |
| `floor(x)` | Rounds down. | `floor(3.9)` $\rightarrow$ `3` |
| `ceil(x)` | Rounds up. | `ceil(3.1)` $\rightarrow$ `4` |

### Trigonometry & Logs
| Function | Description | Example |
| :--- | :--- | :--- |
| `sin(x)`, `cos(x)`, `tan(x)` | Trig functions (radians). | `sin(0)` $\rightarrow$ `0` |
| `log(x)` | Natural logarithm. | `log(2.718)` $\rightarrow$ `1` |
| `log10(x)` | Base-10 logarithm. | `log10(100)` $\rightarrow$ `2` |

### Randomness
| Function | Description | Example |
| :--- | :--- | :--- |
| `random()` | Random float between 0.0 and 1.0. | `random()` |
| `random_int(min, max)` | Random integer in range. | `random_int(1, 100)` |
| `random_choice(list)` | Random element from a list. | `random_choice(["a", "b"])` |

---

## 🌐 Networking

Xell can perform HTTP requests and network diagnostics.

| Function | Description | Example |
| :--- | :--- | :--- |
| `ping(host, count)` | Pings a host. | `ping("google.com", 4)` |
| `http_get(url, hdrs)`| Performs HTTP GET request. | `http_get("https://api.com")` |
| `http_post(url, body)`| Performs HTTP POST request. | `http_post(url, json_data)` |
| `dns_lookup(host)` | Resolves hostname to IP. | `dns_lookup("google.com")` |
| `public_ip()` | Returns your public IP address. | `public_ip()` |

---

## 🛠️ Utilities & Types

| Function | Description | Example |
| :--- | :--- | :--- |
| `type_of(val)` | Returns the type of a value. | `type_of(10)` $\rightarrow$ `"number"` |
| `len(obj)` | Length of string, list, or map. | `len([1, 2, 3])` $\rightarrow$ `3` |
| `to_int(val)` | Casts value to integer. | `to_int("42")` $\rightarrow$ `42` |
| `to_string(val)` | Casts value to string. | `to_string(100)` $\rightarrow$ `"100"` |
| `json_parse(str)` | Parses JSON string to map/list. | `json_parse('{"a":1}')` |
| `json_stringify(obj)`| Converts map/list to JSON string. | `json_stringify({a:1})` |

---

## 📖 What's Next?

Now that you have mastered this topic, move on to the **[Dialects](./Dialects.md)** guide to further your understanding of Xell.
