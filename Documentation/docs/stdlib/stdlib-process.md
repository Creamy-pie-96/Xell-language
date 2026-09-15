# Standard Library – Process Management

> **Previous:** [Environment Variables](stdlib-env.md)  
> **Next:** [Math Functions](stdlib-math.md)

---

Xell provides comprehensive process management capabilities, allowing you to run external commands, capture their output, manage child processes, and control process behaviour. Core functions are always available (Tier 1), while extended operations are available in the `process` module (Tier 2, requires `bring * from "process"`).

---

## Quick Reference

| Function | Module | What it does | Example |
|----------|--------|--------------|---------|
| `run(command)` | Tier 1 | Runs a command, returns exit code | `run("ls -la")` → `0` |
| `run_capture(command)` | Tier 1 | Runs a command, returns `{exit_code, stdout, stderr}` | `run_capture("ls")` |
| `pid()` | Tier 1 | Returns current process ID | `pid()` → `12345` |
| `sleep(ms)` | Tier 1 | Sleeps for N milliseconds | `sleep(100)` |
| `sleep_sec(s)` | Tier 1 | Sleeps for N seconds | `sleep_sec(2)` |
| `exit([code])` | Tier 1 | Exits the process | `exit(0)` |
| `set_e()` | Tier 1 | Enables exit‑on‑error mode | `set_e()` |
| `unset_e()` | Tier 1 | Disables exit‑on‑error mode | `unset_e()` |
| `exit_code()` | Tier 1 | Returns last command's exit code | `exit_code()` → `0` |
| `ppid()` | Tier 2 (process) | Parent process ID | `ppid()` → `12344` |
| `ps()` | Tier 2 (process) | Lists running processes | `ps()` |
| `kill(pid[, signal])` | Tier 2 (process) | Sends signal to process | `kill(12345)` |
| `kill_name(name)` | Tier 2 (process) | Kills processes by name | `kill_name("python")` |
| `pkill(pattern)` | Tier 2 (process) | Kills processes matching pattern | `pkill("python")` |
| `pgrep(pattern)` | Tier 2 (process) | Finds PIDs matching pattern | `pgrep("python")` |
| `pidof(name)` | Tier 2 (process) | Gets PID by exact name | `pidof("python")` |
| `pstree([pid])` | Tier 2 (process) | Process tree as string | `pstree()` |
| `jobs()` | Tier 2 (process) | Lists background jobs | `jobs()` |
| `bg(pid)` | Tier 2 (process) | Resumes process in background | `bg(12345)` |
| `fg(pid)` | Tier 2 (process) | Brings to foreground | `fg(12345)` |
| `nohup(cmd)` | Tier 2 (process) | Runs command immune to hangup | `nohup("server &")` |
| `nice(cmd[, level])` | Tier 2 (process) | Runs with adjusted priority | `nice("make", 10)` |
| `wait_pid(pid)` | Tier 2 (process) | Waits for process to finish | `wait_pid(12345)` |
| `spawn(cmd)` | Tier 2 (process) | Spawns subprocess, returns PID | `spawn("server")` |
| `run_timeout(cmd, ms)` | Tier 2 (process) | Runs with timeout | `run_timeout("build", 5000)` |
| `getuid()` | Tier 2 (process) | User ID (Unix only) | `getuid()` → `1000` |
| `is_root()` | Tier 2 (process) | Checks if running as root/admin | `is_root()` → `false` |
| `id()` | Tier 2 (process) | User info map | `id()` |
| `whoami()` | Tier 2 (process) | Current username | `whoami()` → `"alice"` |
| `hostname()` | Tier 2 (process) | Machine hostname | `hostname()` → `"my-machine"` |
| `uname()` | Tier 2 (process) | OS info map | `uname()` |
| `uptime()` | Tier 2 (process) | System uptime in seconds | `uptime()` → `12345` |
| `time_cmd(cmd)` | Tier 2 (process) | Measures command execution time | `time_cmd("sleep 1")` |
| `watch(cmd[, interval, count])` | Tier 2 (process) | Repeats command | `watch("ls", 2, 3)` |
| `strace(cmd)` | Tier 2 (process) | Traces system calls (Unix only) | `strace("ls")` |
| `lsof()` | Tier 2 (process) | Lists open files | `lsof()` |
| `sys_info()` | Tier 2 (process) | OS and hardware info map | `sys_info()` |
| `os_name()` | Tier 2 (process) | `"linux"`, `"windows"`, or `"macos"` | `os_name()` → `"linux"` |
| `arch()` | Tier 2 (process) | `"x86_64"`, `"arm64"`, etc. | `arch()` → `"x86_64"` |

---

## Tier 1 – Core Process Utilities

### `run(command)`

Runs an external command and returns its exit code. The output is printed to stdout/stderr (not captured).

```xell
let code = run("ls -la")
if code == 0:
    print("Command succeeded")
else:
    print("Command failed with code: " + str(code))
;
```

**Output (varies):**
```
total 42
-rw-r--r-- 1 user user 1234 Jan 1 10:00 file.txt
...
Command succeeded
```

**Throws:** `ProcessError` if the command cannot be executed; `CommandFailedError` if `set_e()` is enabled and the command returns a non‑zero exit code.

### `run_capture(command)`

Runs an external command and captures its stdout, stderr, and exit code. Returns a map with the following fields:

| Field | Type | Description |
|-------|------|-------------|
| `exit_code` | int | The command's exit code |
| `stdout` | string | Standard output |
| `stderr` | string | Standard error |

```xell
let result = run_capture("ls -la")
print("Exit code: " + str(result["exit_code"]))
print("Stdout: " + result["stdout"])
print("Stderr: " + result["stderr"])
```

**Throws:** Same as `run()`.

### `pid()`

Returns the current process ID (integer).

```xell
print("My PID: " + str(pid()))
```

### `sleep(ms)`

Pauses execution for `ms` milliseconds.

```xell
print("Starting...")
sleep(500)   # pause for 500ms
print("...done")
```

### `sleep_sec(s)`

Pauses execution for `s` seconds.

```xell
print("Counting down...")
sleep_sec(1)
print("1")
sleep_sec(1)
print("0")
```

### `exit([code])`

Terminates the process with the given exit code (default `0`).

```xell
if not file_exists("config.xel"):
    exit(1)
```

### `set_e()` / `unset_e()`

`set_e()` enables "exit‑on‑error" mode. When enabled, any command (run via `run`, `run_capture`, `$cmd`, or PATH fallback) that exits with a non‑zero code will throw a `CommandFailedError` immediately.

```xell
# Without set_e():
run("ls /nonexistent")   # exits with code 2, but script continues
print("This still runs")

# With set_e():
set_e()
run("ls /nonexistent")   # ❌ throws CommandFailedError
print("This does NOT run")
```

`unset_e()` disables this mode.

```xell
set_e()
try:
    run("ls /nonexistent")
catch e:
    print("Command failed: " + e->message)
;
unset_e()
```

### `exit_code()`

Returns the exit code of the last executed command.

```xell
run("ls /tmp")
print("Last exit code: " + str(exit_code()))   # 0

run("ls /nonexistent")
print("Last exit code: " + str(exit_code()))   # non‑zero
```

---

## Tier 2 – Extended Process Utilities (`bring * from "process"`)

### Process Listing and Information

#### `ps()`

Returns a list of all running processes, each as a map with:

| Field | Type | Description |
|-------|------|-------------|
| `pid` | int | Process ID |
| `name` | string | Process name |
| `ppid` | int | Parent process ID |
| `mem` | int | Memory usage in bytes |

```xell
bring * from "process"

let processes = ps()
for p in processes:
    print("PID " + str(p["pid"]) + ": " + p["name"])
;
```

#### `ppid()`

Returns the parent process ID.

```xell
bring * from "process"

print("Parent PID: " + str(ppid()))
```

#### `pstree([pid])`

Returns a process tree as a string. Shows the hierarchy of processes. If `pid` is provided, shows the tree rooted at that process; otherwise shows the tree from the current process.

```xell
bring * from "process"

let tree = pstree()
print(tree)
```

#### `sys_info()`

Returns a map with OS and hardware information:

| Field | Type | Description |
|-------|------|-------------|
| `os` | string | `"linux"`, `"windows"`, or `"macos"` |
| `arch` | string | `"x86_64"`, `"arm64"`, etc. |
| `kernel` | string | Kernel version (Unix) |
| `hostname` | string | Machine hostname |
| `cpu_count` | int | Number of logical CPU cores |

```xell
bring * from "process"

let info = sys_info()
print("OS: " + info["os"])
print("Arch: " + info["arch"])
print("CPU cores: " + str(info["cpu_count"]))
```

#### `os_name()`

Returns a string: `"linux"`, `"windows"`, or `"macos"`.

```xell
bring * from "process"

if os_name() == "linux":
    print("Running on Linux")
elif os_name() == "windows":
    print("Running on Windows")
else:
    print("Running on macOS")
;
```

#### `arch()`

Returns the architecture: `"x86_64"`, `"arm64"`, `"x86"`, or `"unknown"`.

```xell
bring * from "process"

print("Architecture: " + arch())
```

#### `hostname()`

Returns the machine's hostname.

```xell
bring * from "process"

print("Hostname: " + hostname())
```

#### `uptime()`

Returns the system uptime in seconds.

```xell
bring * from "process"

let up = uptime()
print("Uptime: " + str(up / 3600) + " hours")
```

### Process Termination

#### `kill(pid[, signal])`

Sends a signal to a process. Default signal is `15` (SIGTERM). Returns `true` on success, `false` on failure. On Windows, signal is not supported – it always terminates the process.

```xell
bring * from "process"

kill(12345)          # sends SIGTERM (15)
kill(12345, 9)       # sends SIGKILL (9)
```

#### `kill_name(name)`

Kills all processes with the given name (using `taskkill` on Windows, `killall` on Unix). Returns the exit code of the underlying command.

```xell
bring * from "process"

kill_name("firefox")   # kills all Firefox processes
```

#### `pkill(pattern)`

Kills processes whose names match a pattern (on Unix, using `pkill`; on Windows, uses process listing). Returns the exit code of the underlying command.

```xell
bring * from "process"

pkill("python")   # kills all Python processes
```

### Process Search

#### `pgrep(pattern)`

Returns a list of PIDs for processes whose names match a pattern.

```xell
bring * from "process"

let pids = pgrep("python")
for p in pids:
    print("Python process: " + str(p))
;
```

#### `pidof(name)`

Returns a list of PIDs for processes with the exact given name.

```xell
bring * from "process"

let pids = pidof("bash")
print("Bash PIDs: " + str(pids))
```

### Process Control

#### `jobs()`

Returns a string listing background jobs (shell integration). On systems without job control, returns an empty string.

```xell
bring * from "process"

let job_list = jobs()
print(job_list)
```

#### `bg(pid)`

Resumes a stopped process in the background (sends SIGCONT). Returns `true` on success.

```xell
bring * from "process"

bg(12345)
```

#### `fg(pid)`

Resumes a stopped process in the foreground (sends SIGCONT). Returns `true` on success.

```xell
bring * from "process"

fg(12345)
```

#### `nohup(command)`

Runs a command immune to hangup signals (SIGHUP). Returns a map with `{command, pid}`.

```xell
bring * from "process"

let result = nohup("sleep 30 &")
print("PID: " + str(result["pid"]))
```

#### `nice(command[, level])`

Runs a command with adjusted priority. The default nice level is `10`. Returns the command's exit code.

```xell
bring * from "process"

nice("make", 10)   # runs make with low priority
```

### Waiting for Processes

#### `wait_pid(pid)`

Waits for a process to finish and returns its exit code. If the process is not a child, it polls `kill(pid, 0)` until the process no longer exists.

```xell
bring * from "process"

let code = wait_pid(12345)
print("Process exited with code: " + str(code))
```

### Spawning and Timeouts

#### `spawn(command)`

Spawns a new subprocess in the background and returns its PID immediately (does not wait). The child process is detached from the parent.

```xell
bring * from "process"

let child_pid = spawn("long_running_task")
print("Spawned PID: " + str(child_pid))
```

#### `run_timeout(command, timeout_ms)`

Runs a command with a timeout. Returns a map with:

| Field | Type | Description |
|-------|------|-------------|
| `exit_code` | int | Command's exit code, or `-1` if timed out |
| `timed_out` | bool | `true` if the command was killed due to timeout |

```xell
bring * from "process"

let result = run_timeout("sleep 5", 2000)
if result["timed_out"]:
    print("Command timed out")
else:
    print("Command completed with code: " + str(result["exit_code"]))
;
```

### User Information

#### `whoami()`

Returns the current username.

```xell
bring * from "process"

print("You are: " + whoami())
```

#### `getuid()`

Returns the numeric user ID (Unix only). On Windows, returns `0`.

```xell
bring * from "process"

print("UID: " + str(getuid()))
```

#### `is_root()`

Returns `true` if the current process is running as root (Unix) or administrator (Windows).

```xell
bring * from "process"

if is_root():
    print("Running with administrator privileges")
else:
    print("Running as normal user")
;
```

#### `id()`

Returns a map with user information:

| Field | Type | Description |
|-------|------|-------------|
| `uid` | int | User ID (Unix) |
| `gid` | int | Group ID (Unix) |
| `user` | string | Username |
| `groups` | list | List of group names (Unix) |

```xell
bring * from "process"

let user_info = id()
print("User: " + user_info["user"])
print("Groups: " + join(user_info["groups"], ", "))
```

### System Information

#### `uname()`

Returns a map with OS and kernel information:

| Field | Type | Description |
|-------|------|-------------|
| `sysname` | string | Operating system name |
| `nodename` | string | Network node hostname |
| `release` | string | OS release version |
| `version` | string | OS version |
| `machine` | string | Hardware architecture |

```xell
bring * from "process"

let info = uname()
print("OS: " + info["sysname"])
print("Kernel: " + info["release"])
```

### Measurement

#### `time_cmd(command)`

Measures the execution time of a command. Returns a map with:

| Field | Type | Description |
|-------|------|-------------|
| `elapsed_ms` | int | Elapsed time in milliseconds |
| `exit_code` | int | Command's exit code |

```xell
bring * from "process"

let result = time_cmd("ls -la")
print("Time: " + str(result["elapsed_ms"]) + " ms")
```

#### `watch(command[, interval_secs, count])`

Repeats a command multiple times with a fixed interval. Returns a list of outputs (strings). Default: `interval_secs = 2`, `count = 1`.

```xell
bring * from "process"

let outputs = watch("date", 1, 3)
for out in outputs:
    print(out)
;
```

**Output:**
```
Mon Jan 1 10:00:00 UTC 2026
Mon Jan 1 10:00:01 UTC 2026
Mon Jan 1 10:00:02 UTC 2026
```

### Debugging and Tracing

#### `strace(command)`

Traces system calls of a command (Unix only). Returns the strace output as a string. On Windows, throws an error.

```xell
bring * from "process"

let trace = strace("ls")
print(trace)
```

#### `lsof()`

Lists open files for the current process. Returns a string.

```xell
bring * from "process"

let files = lsof()
print(files)
```

---

## Complete Example: Process Monitor

```xell
# process_monitor.xel
bring * from "process"

# Get system info
let info = sys_info()
print("System: " + info["os"] + " " + info["arch"])
print("CPU cores: " + str(info["cpu_count"]))

# List processes
let processes = ps()
print("\nFirst 5 processes:")
for i in range(min(5, len(processes))):
    let p = processes[i]
    print("  " + str(p["pid"]) + ": " + p["name"])
;

# Find all Python processes
let python_pids = pgrep("python")
if not is_empty(python_pids):
    print("\nPython processes: " + join(python_pids, ", "))
else:
    print("\nNo Python processes found")
;

# Check if we're root
if is_root():
    print("\n⚠️  Running as root! Be careful.")
else:
    print("\nRunning as " + whoami())
;

# Measure command time
let time_result = time_cmd("sleep 1")
print("\nSleep 1 took: " + str(time_result["elapsed_ms"]) + " ms")
```

**Expected output:**
```
System: linux x86_64
CPU cores: 8

First 5 processes:
  1: systemd
  2: kthreadd
  3: ksoftirqd/0
  4: kworker/0:0
  5: kworker/0:0H

No Python processes found

Running as alice

Sleep 1 took: 1002 ms
```

---

## What NOT to Do

### 1. Using `run()` Without Checking the Exit Code

```xell
run("rm -rf /tmp/data")   # ❌ command could fail silently
```

**Fix:** Always check the exit code, or use `set_e()` to fail fast.

### 2. Assuming `run_capture()` Always Returns a Map with All Fields

`run_capture()` always returns a map with `exit_code`, `stdout`, and `stderr`. These are always present.

### 3. Forgetting to `bring` the `process` Module for Extended Functions

```xell
ps()   # ❌ error – ps is not defined
```

**Fix:** `bring * from "process"` first.

### 4. Using `kill()` Without Checking if the Process Exists

```xell
kill(999999)   # ❌ returns false, but you might not notice
```

**Fix:** Check the return value of `kill()`.

### 5. Expecting `strace()` to Work on Windows

```xell
strace("ls")   # ❌ throws RuntimeError on Windows
```

### 6. Using `sleep()` or `sleep_sec()` with Negative Values

```xell
sleep(-1)   # ❌ does nothing (sleeps for 0ms)
```

### 7. Assuming `set_e()` Affects All Future Commands in All Threads

`set_e()` is per‑interpreter. If you spawn threads, each thread has its own `ShellState`.

### 8. Using `time_cmd()` on a Command That Takes a Long Time

`time_cmd()` is blocking; it will wait for the command to finish. For long‑running commands, consider using `spawn()` or `run_timeout()`.

### 9. Forgetting That `run_timeout()` Kills the Process on Timeout

`run_timeout()` sends `SIGKILL` (Unix) or terminates the process (Windows) when the timeout expires. This may leave the system in an inconsistent state if the process was in the middle of writing a file.

---

## See Also

- [Shell Integration](../core/shell.md) – `$cmd`, `|>`, `&&`, `||`, `set_e()`
- [File System Operations](stdlib-fs.md) – `run` vs file operations
- [Error Handling](../core/error-handling.md) – `CommandFailedError`

---

**Previous:** [Environment Variables](stdlib-env.md)  
**Next:** [Math Functions](stdlib-math.md)