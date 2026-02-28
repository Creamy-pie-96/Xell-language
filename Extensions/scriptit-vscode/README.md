# ScriptIt Language Support for VS Code

Full-featured language support for [ScriptIt](https://github.com/user/scriptit) — a Python-inspired scripting language built on C++.

## Features

### 🎨 Syntax Highlighting

Rich coloring for all ScriptIt syntax:

- Keywords (`fn`, `give`, `if`, `elif`, `else`, `for`, `while`, `var`, ...)
- Builtin functions (`print`, `len`, `type`, `sorted`, `map`, ...)
- Math functions (`sin`, `cos`, `sqrt`, `abs`, ...)
- Type conversions (`str`, `int`, `double`, `bool`, ...)
- Strings, numbers, comments, operators
- Special tokens like `@` references and `.` / `;` terminators

### 🧠 IntelliSense

- **Auto-complete** for keywords, builtins, math functions, and type conversions
- **Method completions** after `.` (string, list, set, and file methods)
- **User-defined** function and variable completions extracted from your code

### 🔍 Hover Information

Hover over any keyword or builtin to see its signature and documentation.

### ✍️ Signature Help

Type a function name followed by `(` to see parameter info with active parameter highlighting.

### 🐛 Diagnostics / Linting

- **Static analysis**: bracket matching, unterminated strings, `=` vs `==` warnings, `fn` definition checks
- **Subprocess validation**: optionally runs `scriptit --script` for real error detection

### 📝 Snippets

Quick-insert templates for common patterns:

- `fn` — function definition
- `if` / `ife` / `ifee` — conditional blocks
- `forr` / `forin` / `forfts` — loop variants
- `while`, `var`, `give`, `with`, `let`, `new`, `of`

### ▶️ Run Support

- **Ctrl+Shift+R** — Run current ScriptIt file in terminal
- Run selection in terminal
- Launch ScriptIt Notebook (web-based notebook interface)

## File Associations

| Extension | Description          |
| --------- | -------------------- |
| `.si`     | ScriptIt script file |
| `.sit`    | ScriptIt script file |

## Configuration

| Setting                        | Default    | Description                             |
| ------------------------------ | ---------- | --------------------------------------- |
| `scriptit.scriptitPath`        | `scriptit` | Path to the ScriptIt executable         |
| `scriptit.maxNumberOfProblems` | `100`      | Maximum diagnostics to report           |
| `scriptit.enableLinting`       | `true`     | Enable/disable error detection          |
| `scriptit.trace.server`        | `off`      | Trace LSP communication (for debugging) |

## Requirements

- **ScriptIt** binary installed and in PATH (or configured via `scriptit.scriptitPath`)
- For notebook support: Python 3 (for the notebook server)

## Development

```bash
cd scriptit-vscode
npm install
npm run compile
# Press F5 in VS Code to launch Extension Development Host
```
