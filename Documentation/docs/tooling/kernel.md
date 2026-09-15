# Notebook Kernel

> **Previous:** [Debugging](debugging.md)  
> **Next:** [Terminal GUI](terminal-gui.md)

---

Xell can run as a **notebook kernel** – a headless process that executes code cells, captures output, and communicates via JSON‑over‑stdin/stdout. This enables integration with Jupyter‑like notebooks, custom frontends, and any application that needs to evaluate Xell code programmatically.

Think of this as Xell's **REPL‑over‑the‑wire** – you send code, it executes, and returns the result with stdout, stderr, and execution metadata.

---

## Quick Reference

| Command | What it does | Example |
|---------|--------------|---------|
| `xell --kernel` | Starts the kernel process | `$ xell --kernel` |
| `{"action":"execute","cell_id":"...","code":"..."}` | Executes a code cell | Send over stdin |
| `{"action":"reset"}` | Resets the interpreter state | Send over stdin |
| `{"action":"shutdown"}` | Shuts down the kernel | Send over stdin |
| Response JSON | `{"cell_id":"...","status":"ok","stdout":"...","stderr":"...","execution_count":N}` | Output on stdout |

---

## Starting the Kernel

```bash
$ xell --kernel
{"status":"kernel_ready","version":"0.1.0"}
```

The kernel starts, prints a ready message to stdout, and then begins listening for JSON messages on stdin.

**Important:** The kernel uses **line‑delimited JSON** – each command must be a single JSON object ending with a newline (`\n`). Responses are also line‑delimited.

---

## Protocol

### Input Format (stdin)

Each command is a JSON object with an `"action"` field.

#### `execute` – Execute a Code Cell

```json
{
  "action": "execute",
  "cell_id": "cell_1",
  "code": "let x = 10\nprint(x * 2)"
}
```

- `cell_id` – any string to identify the cell (echoed back in the response).
- `code` – the Xell source code to execute (can contain multiple lines).

#### `reset` – Reset the Interpreter

```json
{
  "action": "reset"
}
```

Clears all global state (variables, imports, output buffer) and resets the execution counter.

#### `shutdown` – Shut Down the Kernel

```json
{
  "action": "shutdown"
}
```

The kernel exits after sending a final response.

---

### Output Format (stdout)

Each response is a JSON object ending with a newline.

#### Success Response

```json
{
  "cell_id": "cell_1",
  "status": "ok",
  "stdout": "20\n",
  "stderr": "",
  "result": "",
  "execution_count": 1
}
```

| Field | Type | Description |
|-------|------|-------------|
| `cell_id` | string | Echo of the request's `cell_id` |
| `status` | string | `"ok"` if execution succeeded, `"error"` if it failed |
| `stdout` | string | Captured output from `print()` (may be empty) |
| `stderr` | string | Error messages (if any) |
| `result` | string | The last expression's string representation (reserved for future use – currently empty) |
| `execution_count` | integer | Incremented on every `execute` (starts at 1) |

#### Error Response

```json
{
  "cell_id": "cell_2",
  "status": "error",
  "stdout": "",
  "stderr": "[XELL ERROR] Line 1 — TypeError: cannot multiply int by string",
  "result": "",
  "execution_count": 2
}
```

#### Reset Response

```json
{
  "status": "ok",
  "message": "Kernel reset"
}
```

#### Shutdown Response

```json
{
  "status": "shutdown"
}
```

---

## Complete Example Session

### Terminal 1 (Kernel)

```bash
$ xell --kernel
{"status":"kernel_ready","version":"0.1.0"}
```

### Terminal 2 (Frontend)

```bash
$ echo '{"action":"execute","cell_id":"cell_1","code":"let x = 10\nprint(x * 2)"}' | nc localhost 9999
# (or directly pipe to the kernel's stdin)
```

**Kernel output:**

```json
{"cell_id":"cell_1","status":"ok","stdout":"20\n","stderr":"","result":"","execution_count":1}
```

**Another cell:**

```json
{"action":"execute","cell_id":"cell_2","code":"print(x + 5)"}
```

**Response:**

```json
{"cell_id":"cell_2","status":"ok","stdout":"15\n","stderr":"","result":"","execution_count":2}
```

**Reset:**

```json
{"action":"reset"}
```

**Response:**

```json
{"status":"ok","message":"Kernel reset"}
```

**Shutdown:**

```json
{"action":"shutdown"}
```

**Response:**

```json
{"status":"shutdown"}
```

---

## State and Execution

- **Global scope:** Variables defined in one cell persist across subsequent cells (unless `reset` is called).
- **Execution counter:** Increments on every `execute` action, starting at `1`.
- **Output capture:** `print()` output is captured and included in the `stdout` field.
- **Error handling:** If the code throws a runtime error, `stderr` contains the formatted error message and `status` is `"error"`. The kernel does **not** crash.
- **Streaming:** The kernel does **not** stream output incrementally – all output for a cell is captured and returned in a single response.

---

## Integration Examples

### Python Frontend

```python
import subprocess, json, time

# Start the kernel
proc = subprocess.Popen(
    ["xell", "--kernel"],
    stdin=subprocess.PIPE,
    stdout=subprocess.PIPE,
    stderr=subprocess.PIPE,
    text=True
)

# Send an execute command
cmd = json.dumps({
    "action": "execute",
    "cell_id": "cell_1",
    "code": "let x = 10\nprint(x * 2)"
}) + "\n"
proc.stdin.write(cmd)
proc.stdin.flush()

# Read the response
line = proc.stdout.readline()
resp = json.loads(line)
print(resp["stdout"])   # "20\n"
```

### Node.js Frontend

```javascript
const { spawn } = require('child_process');
const kernel = spawn('xell', ['--kernel']);

kernel.stdout.on('data', (data) => {
    const lines = data.toString().split('\n').filter(l => l.trim());
    for (const line of lines) {
        const resp = JSON.parse(line);
        console.log('Response:', resp);
    }
});

kernel.stdin.write(JSON.stringify({
    action: 'execute',
    cell_id: 'cell_1',
    code: 'print("Hello from Node!")'
}) + '\n');
```

### Shell Script (Simple)

```bash
#!/bin/bash
# Start kernel in background
xell --kernel &
KERNEL_PID=$!

# Send a command
echo '{"action":"execute","cell_id":"test","code":"print(42)"}' > /proc/$KERNEL_PID/fd/0

# Wait for response (simplified)
sleep 1

# Shutdown
echo '{"action":"shutdown"}' > /proc/$KERNEL_PID/fd/0
wait $KERNEL_PID
```

---

## Edge Cases and Less Obvious Behaviour

### 1. Empty `code` or Whitespace‑Only

If the `code` field is empty or contains only whitespace, the kernel returns `status: "ok"` with no output and increments the execution counter.

### 2. Invalid JSON

If the kernel receives malformed JSON, it may ignore the command and return nothing (or the frontend may need to handle the error).

### 3. Infinite Loops

If the code contains an infinite loop, the kernel will hang indefinitely. Use timeouts on the frontend side.

### 4. Multi‑Line Code

The `code` field can contain multiple lines. Use `\n` to separate lines.

```json
{
  "action": "execute",
  "cell_id": "test",
  "code": "fn add(a, b):\n    give a + b\n;\nprint(add(5, 3))"
}
```

### 5. `execution_count` Persistence

The execution counter persists across cells and resets only on `reset`.

### 6. `result` Field

Currently the `result` field is empty (reserved for future use when the kernel returns the last expression's value).

---

## What NOT to Do

### 1. Expecting Interactive Prompts

The kernel does **not** support `input()` or interactive prompts. Those functions will hang indefinitely.

```xell
# ❌ In kernel mode, this will hang
let name = input("Enter name: ")
```

### 2. Sending Multiple Commands Without Reading Responses

The kernel processes commands sequentially and writes one response per command. Ensure you read the response before sending the next command to avoid blocking.

### 3. Using the Kernel in a Human‑Facing Terminal

The kernel is for programmatic use. It does not support ANSI escape sequences, interactive editing, or history.

### 4. Assuming `stdout` and `stderr` Are Interleaved in Real Time

All output for a cell is captured and returned in a single response. There is no streaming.

### 5. Forgetting the Trailing Newline

Each command **must end with a newline**. Without it, the kernel will not parse the message.

### 6. Expecting `reset` to Clear Output Buffers

`reset` clears the interpreter state but does not reset the output capture buffer. Any output from the last cell remains in the response.

### 7. Using `exit()` in Kernel Mode

`exit()` terminates the kernel process. Avoid using it unless you intend to shut down.

---

## See Also

- [REPL Usage](repl.md) – interactive shell (human)
- [CLI Commands](cli.md) – `--kernel` flag
- [Debugging](debugging.md) – `--trace-vars`, `--debug`
- [Built‑ins Reference](../core/builtins.md) – functions available in kernel mode

---

**Previous:** [Debugging](debugging.md)  
**Next:** [Terminal GUI](terminal-gui.md)