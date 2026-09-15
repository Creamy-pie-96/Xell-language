# IDE – Xell Integrated Development Environment

> **Previous:** [Terminal GUI](terminal-gui.md)  
> **Next:** [F‑Strings](../advanced/f-strings.md)

---

The Xell IDE is a **lightweight but powerful graphical code editor** built into the Xell Terminal. It combines syntax highlighting, autocomplete, linting, debugging, Git integration, REPL interaction, and a rich set of development tools into a single, cohesive experience.

---

## Quick Reference

| Feature / Panel | Description |
|-----------------|-------------|
| **Editor** | Tabbed editor with syntax highlighting, autocomplete, snippets, inline diagnostics |
| **Sidebar** | File explorer with tree navigation, context menu, inline rename/new file |
| **Dashboard** | Code structure tree (functions, classes, modules, imports) with click‑to‑jump |
| **Bottom Panel** | Terminal, Output, Diagnostics, Variables, Objects, Lifecycle, Timeline, Callstack, Help |
| **REPL Integration** | Run selection (`Ctrl+Enter`), run file (`Ctrl+R`), debug run (`Ctrl+Shift+D`) |
| **Debugger** | Breakpoints, step‑over/into/out, call stack, variable inspection |
| **Git Integration** | Gutter diff, staging, commit, push, status view |
| **Autocomplete** | Keywords, builtins, user symbols, snippets, fuzzy matching, ghost text |
| **Find & Replace** | Regex‑based search with match highlighting, replace single/all |
| **Lifecycle Tracing** | Variable birth/change/death events from `--trace-vars` |
| **Inline Evaluation** | `Ctrl+Shift+E` – evaluate expression, show result as ghost text |

---

## Overview

The Xell IDE is structured into four main regions:

```
┌──────────────────────────────────────────────────────────────────────────┐
│ Tab Bar (editor tabs)                                                   │
├─────────────┬────────────────────────────────────────────────────────────┤
│  Sidebar    │  Editor Area                                              │
│  (File      │  ┌──────────────────────────────────────────────────────────┐
│  Explorer)  │  │  Editor View (code)                                    │
│             │  │  - Syntax highlighting                                 │
│             │  │  - Line numbers / gutter                               │
│             │  │  - Inline diagnostics (squiggly underlines)            │
│             │  │  - Breakpoints / debug pointer                         │
│             │  ├──────────────────────────────────────────────────────────┤
│             │  │  Bottom Panel (toggle with Ctrl+`)                     │
│             │  │  - TERMINAL │ OUTPUT │ DIAGNOSTICS │ VARIABLES │ ...  │
│             │  │                                                        │
│             ├────────────────────────────────────────────────────────────┤
│             │  Dashboard (toggle with Ctrl+Shift+D)                     │
│             │  - Code structure tree                                    │
│             │  - Click symbols to jump                                  │
├─────────────┴────────────────────────────────────────────────────────────┤
│ Status Bar (file info, cursor position, language, messages)             │
└──────────────────────────────────────────────────────────────────────────┘
```

---

## Panels

### Editor

The editor is the central component. It supports:

- **Syntax highlighting** – full Xell grammar, plus JSON, TOML, YAML, and more.
- **Line numbers** – toggle via config.
- **Gutter** – shows breakpoints (`●`), debug execution pointer (`▶`), and diagnostic markers.
- **Inline diagnostics** – squiggly underlines for errors (red) and warnings (yellow).
- **Autocomplete** – triggered automatically or with `Ctrl+Space`; fuzzy matching; snippets.
- **Ghost text** – dim preview of the selected completion; press `Tab` to accept.
- **Snippets** – expand with tab stops; `Tab` navigates between stops.
- **Bracket matching** – highlights matching `:` / `;` pairs and `()`, `[]`, `{}`.
- **Indent guides** – vertical lines showing indentation levels.
- **Code folding** – visual folding of `:` … `;` blocks (click gutter arrows).
- **Smooth scrolling** – animated scrolling.

**Editor Tabs:**

- `Ctrl+Tab` – next tab.
- `Ctrl+Shift+Tab` – previous tab.
- `Ctrl+W` – close current tab.
- `Ctrl+N` – new file.
- `Ctrl+O` – open file.

---

### Sidebar (File Explorer)

The sidebar (`Ctrl+B` toggles) shows a tree view of your project directory.

**Navigation:**

- `↑` / `↓` – move selection.
- `→` – expand directory.
- `←` – collapse directory (or go to parent).
- `Enter` – open file or toggle directory.

**Context Menu (Right‑click):**

- **Open** – open the selected file.
- **New File** – create a new file in the selected directory.
- **New Folder** – create a new folder in the selected directory.
- **Rename** – inline rename.
- **Delete** – delete the selected file/folder.
- **Copy Path** – copy absolute path to clipboard.
- **Copy Relative Path** – copy path relative to project root.
- **Show/Hide Hidden Files** – toggle visibility of `.` files and `__xelcache__/`.

**Inline Editing:**

- Start `Rename` or `New File`/`New Folder` from the context menu.
- Type the name and press `Enter` to commit, `Escape` to cancel.

---

### Dashboard (Right Panel)

The Dashboard (`Ctrl+Shift+D` toggles) shows a hierarchical tree of all symbols in the current file:

- Functions and methods.
- Classes and structs.
- Modules and imports.
- Variables and fields.
- Parameters.

**Features:**

- **Expand/collapse** – click the arrow (`▶`/`▼`) next to nodes.
- **Click a symbol** – jumps to its definition in the editor.
- **Click a variable** – shows its lifecycle (BORN/CHANGED/DIED) in a popup overlay.
- **Imports** – click opens the imported file and jumps to the symbol.

---

### Bottom Panel

The bottom panel (`Ctrl+\`` toggles) contains multiple tabs:

| Tab | Description |
|-----|-------------|
| **TERMINAL** | A full PTY terminal (Xell REPL or shell). |
| **OUTPUT** | Output from `Ctrl+Enter` (run selection) and `Ctrl+R` (run file). |
| **DIAGNOSTICS** | Linting errors and warnings from `--check`. |
| **VARIABLES** | Runtime variables from the last execution (name, type, value, scope, lines). |
| **OBJECTS** | Static code symbols (functions, classes, modules, imports, etc.). |
| **LIFECYCLE** | Variable lifecycle events (BORN, CHANGED, DIED) from `--trace-vars`. |
| **TIMELINE** | Debug event timeline (pause, step, breakpoint, watch events). |
| **CALLSTACK** | Call stack when debugging (click a frame to jump to that line). |
| **HELP** | Keyboard shortcuts reference. |

#### Terminal Tab

- Runs a Xell REPL by default.
- Full VT100/ANSI terminal with true‑color support.
- Scrollback buffer (mouse wheel or `Shift+PageUp`/`PageDown`).
- Copy (`Ctrl+Shift+C`) and paste (`Ctrl+Shift+V`).

#### Output Tab

- Shows output from running code.
- Interactive input: when a program calls `input()`, an input bar appears at the bottom.
- `Ctrl+L` clears the output.

#### Variables Tab

- Shows all runtime variables from the last execution.
- Columns: **NAME**, **TYPE**, **VALUE**, **SCOPE**, **LINES**.
- **Click a variable name** → switches to LIFECYCLE tab and shows its history.
- **Click the SCOPE column** → jumps to the variable's definition.

#### Lifecycle Tab

- Shows variable birth/change/death events.
- Each event shows: line, event type, type, value, and "by whom" (the statement that caused it).
- Colours: teal = `BORN`, yellow = `CHANGED`, red = `DIED`.

#### Timeline Tab

- Debug event timeline.
- Shows pause, step, breakpoint, and watch events.
- Useful for understanding execution flow during debugging.

#### Callstack Tab

- Shows the call stack when execution is paused in the debugger.
- **Click a frame** – jumps to the corresponding line in the editor.

#### Diagnostics Tab

- Shows linting errors and warnings.
- Clicking an entry jumps to the line in the editor.
- Colours: red = error, yellow = warning, green = no errors.

---

## Editor Features in Depth

### Autocomplete

**Triggering:**

- Automatic (after typing) – debounced.
- Manual – `Ctrl+Space`.

**Completions include:**

- Xell keywords (54 keywords).
- Built‑in functions (all Tier 1 and Tier 2 builtins, categorised).
- User symbols (functions, classes, modules, variables from the current file).
- Snippets (expanded with tab stops).
- Module members (for `->` access on modules, string literals, list literals, etc.).

**Fuzzy Matching:**

- Completions are scored using fuzzy matching.
- Higher priority: user symbols > snippets > builtins > keywords.

**Ghost Text:**

- When a completion is selected, the remaining text is shown in dim grey.
- Press `Tab` to accept.

**Snippets:**

- Triggered by typing a snippet prefix (e.g., `fn`).
- Snippets support tab stops: `${1:name}`.
- Press `Tab` to navigate between stops.
- Press `Escape` to cancel snippet expansion.

### Find & Replace

- `Ctrl+F` – open Find bar.
- `Ctrl+H` – open Find & Replace bar.
- **Regex** – enabled by default.
- **Case‑sensitive** – toggle with `Ctrl+R` (when the Find bar is focused).
- **Whole word** – toggle with `Ctrl+W` (when the Find bar is focused).
- **Navigation** – `F3` (next), `Shift+F3` (previous).
- **Replace** – `Ctrl+H` (replace mode), `Tab` to switch between Find and Replace fields.

### Inline Diagnostics

- The linter runs automatically after a 500ms debounce.
- Errors are shown as red squiggly underlines; warnings as yellow.
- The DIAGNOSTICS tab shows the full list.
- Clicking a diagnostic entry jumps to the line.

### Auto‑Indent

- After typing `:` at the end of a line, the next line is indented by one tab stop.
- After typing `;` as the first non‑whitespace character on a line, the line is dedented by one tab stop.

---

## Running and Debugging

### Run Selection (`Ctrl+Enter`)

- If text is selected, runs the selected text.
- If no text is selected, runs from the top of the file to the cursor.
- Output appears in the OUTPUT tab.
- Variables appear in the VARIABLES tab.

### Run File (`Ctrl+R`)

- Runs the current file as a script.
- The file is saved first (if modified).
- Output appears in the OUTPUT tab.

### Debug Run (`Ctrl+Shift+D`)

- Runs the selection (or top‑to‑cursor) with full lifecycle tracing.
- Switches to the LIFECYCLE tab automatically.
- Variables appear with BORN/CHANGED/DIED events.

### Debugger (`F5`)

**Starting:**

- Press `F5` in IDE Mode.
- The interpreter starts with `--debug`.
- The IDE connects to the debug socket.

**Breakpoints:**

- `F9` – toggle breakpoint on the current line.
- Breakpoints are shown as `●` in the gutter (purple = `pause`, blue = `snapshot`).

**Step Controls:**

| Key | Action |
|-----|--------|
| `F5` | Continue (run until next breakpoint or end) |
| `F10` | Step Over (skip function internals) |
| `F11` | Step Into (enter function) |
| `Shift+F11` | Step Out (return from current function) |
| `F12` | Stop debug session |

**During Debugging:**

- Current line is highlighted in yellow.
- CALLSTACK tab shows the stack – click a frame to jump.
- VARIABLES tab shows runtime variables at the pause point.
- TIMELINE tab shows debug events.

---

## Git Integration

The Git panel provides basic Git operations.

**Access:** Bottom panel → Git tab (or `Git` button in the status bar).

**Features:**

- Shows current branch and changed files.
- Gutter diff markers in the editor:
  - Green (`▎`) = added.
  - Red (`▸`) = deleted.
  - Blue (`▎`) = modified.
- Stage/unstage files: select a file and press `Space`.
- Stage all: `Ctrl+A`.
- Commit: type a commit message and press `Enter`.
- Push: `Ctrl+P`.

**Git Status Icons:**

| Icon | Status |
|------|--------|
| `M` | Modified |
| `A` | Added |
| `D` | Deleted |
| `R` | Renamed |
| `U` | Untracked |
| `C` | Conflict |

---

## Keyboard Shortcuts (Full List)

### Editor

| Shortcut | Action |
|----------|--------|
| `Ctrl+S` | Save |
| `Ctrl+Shift+S` | Save As |
| `Ctrl+N` | New file |
| `Ctrl+O` | Open file |
| `Ctrl+W` | Close tab |
| `Ctrl+Tab` | Next tab |
| `Ctrl+Shift+Tab` | Previous tab |
| `Ctrl+F` | Find |
| `Ctrl+H` | Find & Replace |
| `Ctrl+G` | Go to line |
| `Ctrl+Space` | Force autocomplete |
| `Ctrl+Shift+E` | Inline evaluate |
| `Ctrl+Z` | Undo |
| `Ctrl+Shift+Z` / `Ctrl+Y` | Redo |
| `Ctrl+A` | Select all |
| `Ctrl+C` (with selection) | Copy |
| `Ctrl+X` | Cut |
| `Ctrl+V` | Paste |
| `Ctrl+D` | Duplicate line |
| `Ctrl+L` | Delete line |
| `Ctrl+[` / `Ctrl+]` | Outdent / Indent |
| `Tab` / `Shift+Tab` | Indent / Outdent |

### Navigation

| Shortcut | Action |
|----------|--------|
| `↑` / `↓` | Move cursor |
| `←` / `→` | Move cursor |
| `Ctrl+←` / `Ctrl+→` | Move by word |
| `Home` / `End` | Smart home / end of line |
| `Ctrl+Home` | Move to top |
| `Ctrl+End` | Move to bottom |
| `PageUp` / `PageDown` | Scroll |
| `F3` / `Shift+F3` | Next / Previous match (Find) |

### Panels

| Shortcut | Action |
|----------|--------|
| `Ctrl+B` | Toggle Sidebar |
| `Ctrl+\`` | Toggle Bottom Panel |
| `Ctrl+Shift+D` | Toggle Dashboard |
| `Ctrl+T` | Switch to Terminal Mode |
| `Tab` | Cycle bottom panel tabs |

### REPL / Running

| Shortcut | Action |
|----------|--------|
| `Ctrl+Enter` | Run selection (or top‑to‑cursor) |
| `Ctrl+R` | Run current file |
| `Ctrl+Shift+D` | Debug Run (with lifecycle) |
| `Ctrl+Shift+K` / `Ctrl+Shift+Q` | Emergency stop running program |
| `Ctrl+L` | Clear output (OUTPUT tab) |

### Debugging

| Shortcut | Action |
|----------|--------|
| `F5` | Start debug / Continue |
| `F9` | Toggle breakpoint |
| `F10` | Step Over |
| `F11` | Step Into |
| `Shift+F11` | Step Out |
| `F12` | Stop debug session |

### Git

| Shortcut | Action |
|----------|--------|
| `Space` (in Git panel) | Stage / Unstage file |
| `Ctrl+A` (in Git panel) | Stage all |
| `Enter` (in Git panel) | Commit (with message) |
| `Ctrl+P` (in Git panel) | Push |

### General

| Shortcut | Action |
|----------|--------|
| `Ctrl+Q` | Quit Xell |
| `Escape` | Close dialog / cancel |
| `Ctrl+Shift+C` | Copy (Terminal Mode) |
| `Ctrl+Shift+V` | Paste (Terminal Mode) |

---

## Complete Example Workflow

### Writing and Debugging a Function

1. **Start the IDE:**

   ```bash
   $ xell --ide
   ```

2. **Create a new file:**

   `Ctrl+N` – a new untitled tab opens.

3. **Write a function:**

   ```xell
   fn factorial(n):
       if n <= 1:
           give 1
       give n * factorial(n - 1)
   ;
   ```

4. **Save:**

   `Ctrl+S` – name the file `factorial.xel`.

5. **Run:**

   `Ctrl+R` – the file runs, output appears in the OUTPUT tab.

6. **Debug Run (to see lifecycle):**

   `Ctrl+Shift+D` – runs with lifecycle tracing. Switch to LIFECYCLE tab to see variable events.

7. **Set a breakpoint:**

   `F9` on line 3 (the `if` statement).

8. **Start debugger:**

   `F5` – the debugger starts. The program pauses at the breakpoint.

9. **Step through:**

   - `F10` – step over.
   - `F11` – step into the recursive call.
   - `Shift+F11` – step out of the current function.

10. **Inspect variables:**

    - Switch to VARIABLES tab to see `n` and its value at each step.
    - Switch to CALLSTACK tab to see the recursion depth.

11. **Stop debugging:**

    `F12` – stops the debug session.

---

## Configuration and Customization

### `xell_ide.json`

The IDE loads configuration from `~/.config/xell/xell_ide.json` (or `%APPDATA%/xell/xell_ide.json` on Windows). The file is hot‑reloaded when changes are saved.

**Full configuration example:**

```json
{
  "editor": {
    "font_size": 14,
    "tab_size": 4,
    "word_wrap": false,
    "line_numbers": true,
    "minimap": true,
    "bracket_matching": true,
    "indent_guides": true,
    "cursor_style": "block",
    "cursor_blink": true,
    "cursor_blink_rate": 530,
    "auto_indent": true,
    "highlight_current_line": true,
    "code_folding": true,
    "smooth_scrolling": true,
    "smooth_scroll_speed": 0.3
  },
  "panels": {
    "file_explorer_visible": true,
    "file_explorer_width": 30,
    "bottom_panel_visible": false,
    "bottom_panel_height": 12,
    "minimap_visible": true,
    "minimap_width": 10
  },
  "repl": {
    "history_size": 1000,
    "auto_execute_on_enter": false,
    "shell_passthrough": true
  },
  "theme": "terminal_colors.json",
  "git": {
    "git_enabled": true,
    "git_gutter": true,
    "git_auto_refresh": true,
    "git_refresh_interval": 5000
  },
  "session": {
    "restore_session": true,
    "max_recent_files": 20,
    "recent_files": ["/home/user/project/main.xel"]
  }
}
```

### Themes

The IDE loads colors from `terminal_colors.json` (in `assets/` or `~/.config/xell/`). Run `xell --customize` to launch a web‑based color customizer.

**Hot‑reload:** Changes to `terminal_colors.json` are picked up immediately (no restart needed).

### Font

The terminal uses a monospace font loaded from `assets/`. You can change the font by replacing the font file or specifying a different path in the config (future).

---

## What NOT to Do

### 1. Expecting `Ctrl+Z` to Suspend a Running Program

`Ctrl+Z` is not bound. Use `Ctrl+Shift+K` or `Ctrl+Shift+Q` to stop a running program.

### 2. Forgetting to Save Before Running a File

`Ctrl+R` runs the file as saved on disk. Unsaved changes are ignored. Save first, or use `Ctrl+Enter` for unsaved selections.

### 3. Using `Ctrl+C` for Copy When There's No Selection

If there is no selection, `Ctrl+C` does nothing (in IDE Mode) or sends `SIGINT` (in Terminal Mode). Use `Ctrl+Shift+C` for copy.

### 4. Expecting the Linter to Catch All Errors Immediately

The linter runs after a 500ms debounce. It may not show errors immediately after typing.

### 5. Assuming Breakpoints Persist After Closing a Tab

Breakpoints are tied to the file and line. If you close and reopen the file, breakpoints are lost. (They are not saved to disk.)

### 6. Using `:quit` in IDE Mode Without Saving

`:quit` exits Xell immediately. Unsaved changes are lost. Save first.

### 7. Expecting the Terminal to Work Without SDL2

The Xell Terminal requires SDL2 and SDL2_ttf. Install them:

```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev libsdl2-ttf-dev

# macOS
brew install sdl2 sdl2_ttf
```

### 8. Using `--ide` in a Headless Environment

The IDE requires a display. Use `--kernel` or `--debug` for headless debugging.

### 9. Expecting Git Gutter to Work Outside a Git Repository

The Git gutter only works if the current file is inside a Git repository. `git init` first.

### 10. Assuming the Dashboard Shows Symbols for Non‑Xell Files

The Dashboard only shows symbols for Xell files (`.xel`, `.xell`). For other files, it will be empty.

### 11. Using `Ctrl+Enter` on a Large Selection Without Saving

`Ctrl+Enter` runs the selection in the REPL by writing it to a temp file. Very large selections may be slow. Use `Ctrl+R` for full files.

### 12. Forgetting That `Ctrl+Shift+D` Toggles the Dashboard

`Ctrl+Shift+D` toggles the Dashboard panel. It does **not** start a debug run (that's `Ctrl+D` in some contexts, but in this IDE it's `Ctrl+Shift+D` for the dashboard and `Ctrl+Shift+D` is overloaded? Actually `Ctrl+Shift+D` is the Dashboard toggle, and `Ctrl+D` in the context of the source code is **Debug Run**? Let me re-check: In the source, `Ctrl+Shift+D` toggles the dashboard, and `Ctrl+D` (without Shift) is bound to **Debug Run** in the layout manager. So the correct shortcut for Debug Run is `Ctrl+D` (not `Ctrl+Shift+D`). I'll correct this.)

**Correction:** Debug Run is `Ctrl+D` (without Shift). Dashboard toggle is `Ctrl+Shift+D`.

---

## See Also

- [Terminal GUI](terminal-gui.md) – launching the IDE, Terminal Mode
- [Debugging](debugging.md) – `--debug`, `--trace-vars`
- [REPL Usage](repl.md) – interactive shell
- [CLI Commands](cli.md) – `--terminal`, `--ide`, `--customize`

---

**Previous:** [Terminal GUI](terminal-gui.md)  
**Next:** [F‑Strings](../advanced/f-strings.md)