# Shell Block in Xell

Xell provides first-class `shell` blocks for executing shell scripts directly within your programs, combining the convenience of shell scripting with rigorous security, resource limits, and structured error handling.

---

## 1. Syntax Overview

### Plain Block
A plain `shell` block inherits standard I/O (stdin, stdout, stderr) and raises an exception on non-zero exit or signal death:

```xell
shell {
    echo "Starting build process..."
    make -j4
}
```

### Capture Block
A capture block captures stdout and stderr, returning a structured result object:

```xell
res = shell->capture() {
    git rev-parse HEAD
}

print("Commit:", res->stdout.trim())
```

### Options
Options can be passed to configure block execution:

```xell
res = shell->capture(
    timeout: 10,
    max_output: 1048576,    // 1 MiB
    stdin: "closed",
    check: true,
    cwd: "/path/to/project",
    env: {"BUILD_ENV": "production"}
) {
    npm test
}
```

#### Option Defaults

| Option | Plain `shell {}` | `shell->capture {}` | Description |
|---|---|---|---|
| `stdin` | `"inherit"` | `"closed"` | Stdin mode (`"inherit"` or `"closed"`). |
| `check` | `true` | `false` | When `true`, non-zero exit or signal raises `ShellProcessError`. |
| `timeout` | none (0) | none (0) | Max execution time in seconds. Raises `ShellTimeoutError` when exceeded. |
| `max_output`| n/a | 16 MiB (16,777,216 bytes) | Max stdout + stderr bytes captured. Raises `ShellOutputLimitError` on breach. |
| `cwd` | current directory | current directory | Working directory for child process. |
| `env` | inherits environment | inherits environment | Map of additional environment variables. Keys starting with `XELL_` are forbidden. |

---

## 2. Result Object

Capture blocks return a dictionary-like object with the following fields:

- `stdout`: String containing captured stdout (decoded as UTF-8 with lossy replacement `\uFFFD`).
- `stderr`: String containing captured stderr.
- `code` / `exit_code`: Process exit code (integer, `0` on normal success).
- `signal`: Signal number that terminated process (`0` if not terminated by signal).
- `success` / `ok`: Boolean, `true` if and only if normal exit with `code == 0` (signal death never counts as success).

You can also chain results with `&&` and `||`:

```xell
res1 = shell->capture() { test -f config.json } && shell->capture() { cat config.json }
```

---

## 3. Variable Interpolation (`@{expr}`)

You can interpolate Xell expressions directly into shell blocks using `@{expr}`:

```xell
target = "build/app"
flags = ["-Wall", "-O2"]

shell {
    gcc @{flags} main.c -o @{target}
}
```

### Interpolation Mechanics & Guarantees
- **Transport via Environment Variables**: Interpolated values are evaluated by Xell and transported to the child process exclusively through dedicated environment variables (`XELL_0`, `XELL_1_0`, etc.).
- **No Parser Vulnerability**: Values are never concatenated into raw shell syntax or re-parsed by the shell. Shell metacharacters like `;`, `|`, `&`, `$()`, and quotes inside interpolated values remain strictly inert data.
- **Quoting Rules**:
  - **Unquoted**: `@{var}` expands to `"$XELL_<n>"` (preserving word boundaries).
  - **Double Quotes**: `"{@{var}}"` expands to `$XELL_<n>`.
  - **Single Quotes**: `'@{var}'` is preserved literally and is **not** interpolated.
- **Lists**: A list expression expands to a space-separated sequence of quoted variables (`"$XELL_0_0" "$XELL_0_1"`). An empty list `[]` expands to nothing.
- **Dictionaries**: Dictionary interpolation inside shell blocks is forbidden and raises `TypeError`.

---

## 4. Guarantees and What You Must Know

### 1. Argument Injection vs Syntax Injection
Values are never parsed as shell syntax, preventing classic shell injection (e.g. `; rm -rf ~`). However, **argument injection is still possible** if untrusted input begins with dashes:
```xell
// If userInput is "-rf", rm interprets it as a flag!
shell { rm @{userInput} }

// SAFE PRACTICE: Always use '--' before untrusted arguments:
shell { rm -- @{userInput} }
```

### 2. Command Name Interpolation
If `@{cmd}` appears in the command position, data determines which executable runs:
```xell
shell { @{cmd} --version }
```

### 3. Fail-Fast Execution
- On POSIX shells (`sh`, `bash`, `zsh`), shell blocks execute with fail-fast prefix (`set -e\n`). Any command failure will abort execution immediately unless explicitly handled (e.g. `cmd || true`).
- Multi-line blocks exit with the exit code of the final executed command.

### 4. Working Directory (`cd`)
`cd` inside a shell block only affects the subshell process and does not persist in the parent Xell runtime. To change directory for a shell block, use the `cwd` option:
```xell
shell(cwd: "/var/log") {
    tar -czf logs.tar.gz *.log
}
```

### 5. UTF-8 Output Decoding
Captured output is stored as bytes and decoded into strings as UTF-8 with replacement (`\uFFFD`). Arbitrary binary sequences will not crash the interpreter.

### 6. Stdin Behavior
Commands that read from stdin (such as `cat`, `grep`, `python`) will hang indefinitely unless stdin is closed or timed out. In `shell->capture`, `stdin: "closed"` is the default to prevent accidental hangs. In plain `shell {}`, stdin is inherited by default to allow interactive tools.

---

## 5. Errors and Diagnostics

When execution fails, Xell raises specialized errors:

- `ShellProcessError`: Command exited with non-zero code or killed by signal (when `check: true`).
- `ShellTimeoutError`: Execution exceeded `timeout` seconds. The process group is sent `SIGTERM`, followed by a 2-second grace period and `SIGKILL`.
- `ShellOutputLimitError`: Captured output exceeded `max_output` limit.

### Diagnostic Information
All `ShellProcessError` exceptions include:
- `line()`: The Xell source code line where the `shell` block is located.
- `shellLine`: Best-effort line number within the shell script where the error occurred (extracted from shell stderr).
- `exitCode`: Non-zero exit code (or `-1` if killed by signal).
- `signalNum`: Terminating signal number if killed by a signal.
- `stdoutStr` / `stderrStr`: Captured partial stdout and stderr prior to failure.
