````markdown
# .nxel Notebook Format Specification

## Overview

`.nxel` (Notebook Xell) is a JSON-based notebook format for Xell,
inspired by Jupyter's `.ipynb` format but tailored for Xell's colon/semicolon-scoped syntax.

## JSON Structure

```json
{
  "nxel_format": "1.0",
  "metadata": {
    "title": "My Notebook",
    "created": "2025-01-01T00:00:00Z",
    "modified": "2025-01-01T00:00:00Z",
    "kernel": "xell",
    "kernel_version": "0.1"
  },
  "cells": [
    {
      "id": "cell-uuid-1",
      "type": "code",
      "source": "x = 42\nprint(x)",
      "outputs": [
        {
          "type": "stdout",
          "text": "42\n"
        }
      ],
      "execution_count": 1,
      "metadata": {}
    },
    {
      "id": "cell-uuid-2",
      "type": "markdown",
      "source": "# This is a heading\n\nSome **bold** text.",
      "outputs": [],
      "execution_count": null,
      "metadata": {}
    }
  ]
}
```

## Cell Types

- **code**: Xell code cells. Executed by the kernel.
- **markdown**: Rich text cells. Rendered by the GUI.

## Output Types

- **stdout**: Standard output text
- **stderr**: Error output text
- **result**: Expression evaluation result (last expression value)

## Kernel Protocol (JSON over stdin/stdout)

### Request (GUI → Kernel)

```json
{"action": "execute", "cell_id": "cell-uuid-1", "code": "x = 42\nprint(x)"}
{"action": "reset"}
{"action": "complete", "code": "pri", "cursor": 3}
{"action": "shutdown"}
```

### Response (Kernel → GUI)

```json
{"cell_id": "cell-uuid-1", "status": "ok", "stdout": "42\n", "stderr": "", "result": "", "execution_count": 1}
{"cell_id": "cell-uuid-1", "status": "error", "stdout": "", "stderr": "[XELL ERROR] Line 1 — NameError: undefined 'y'", "result": "", "execution_count": 1}
{"status": "reset_ok"}
{"status": "shutdown_ok"}
```

## Xell Syntax Notes

- Blocks open with `:` and close with `;`
- Statements terminate with `.` or newline
- No semicolons as statement terminators (`;` closes blocks)
- String interpolation: `"Hello {name}"`
- Comments: `#` for line, `-->` ... `<--` for block
- Functions: `fn name(params): body;`
- Variables: `name = value` (no var/let keyword)
- Return: `give value`
- Import: `bring module` or `from module bring name`
- Map access: `map->key`

## File Extension

- `.nxel` — Notebook Xell file
- `.xel` — Xell script file (non-notebook)

## Example

```json
{
  "nxel_format": "1.0",
  "metadata": {
    "title": "Getting Started",
    "created": "2025-01-01T00:00:00Z",
    "modified": "2025-01-01T00:00:00Z",
    "kernel": "xell",
    "kernel_version": "0.1"
  },
  "cells": [
    {
      "id": "a1b2c3d4",
      "type": "markdown",
      "source": "# Welcome to Xell Notebooks\nRun each cell with Shift+Enter.",
      "outputs": [],
      "execution_count": null,
      "metadata": {}
    },
    {
      "id": "e5f6g7h8",
      "type": "code",
      "source": "name = \"World\"\nprint(\"Hello {name}!\")",
      "outputs": [{ "type": "stdout", "text": "Hello World!\n" }],
      "execution_count": 1,
      "metadata": {}
    },
    {
      "id": "i9j0k1l2",
      "type": "code",
      "source": "fn square(n):\n  give n * n\n;\n\nresult = square(7)\nprint(\"7² = {result}\")",
      "outputs": [{ "type": "stdout", "text": "7² = 49\n" }],
      "execution_count": 2,
      "metadata": {}
    }
  ]
}
```
````
