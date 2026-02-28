# .nsit Notebook Format Specification

## Overview

`.nsit` (Notebook ScriptIt) is a JSON-based notebook format for ScriptIt,
inspired by Jupyter's `.ipynb` format but tailored for ScriptIt's dot-terminated syntax.

## JSON Structure

```json
{
  "nsit_format": "1.0",
  "metadata": {
    "title": "My Notebook",
    "created": "2025-01-01T00:00:00Z",
    "modified": "2025-01-01T00:00:00Z",
    "kernel": "scriptit",
    "kernel_version": "2.0"
  },
  "cells": [
    {
      "id": "cell-uuid-1",
      "type": "code",
      "source": "var x = 42.\nprint(x).",
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

- **code**: ScriptIt code cells. Executed by the kernel.
- **markdown**: Rich text cells. Rendered by the GUI.

## Output Types

- **stdout**: Standard output text
- **stderr**: Error output text
- **result**: Expression evaluation result (last expression value)

## Kernel Protocol (JSON over stdin/stdout)

### Request (GUI → Kernel)

```json
{"action": "execute", "cell_id": "cell-uuid-1", "code": "var x = 42.\nprint(x)."}
{"action": "reset"}
{"action": "complete", "code": "pri", "cursor": 3}
{"action": "shutdown"}
```

### Response (Kernel → GUI)

```json
{"cell_id": "cell-uuid-1", "status": "ok", "stdout": "42\n", "stderr": "", "result": "", "execution_count": 1}
{"cell_id": "cell-uuid-1", "status": "error", "stdout": "", "stderr": "Error: ...", "result": "", "execution_count": 1}
{"status": "reset_ok"}
{"status": "shutdown_ok"}
```
