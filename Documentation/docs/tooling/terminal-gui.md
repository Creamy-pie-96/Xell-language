# Terminal GUI – Xell Terminal & IDE

> **Previous:** [Notebook Kernel](kernel.md)  
> **Next:** [IDE](ide.md)

---

The Xell Terminal is a **graphical terminal application** built with SDL2 that provides two integrated modes:

1. **Terminal Mode** – a full‑featured terminal emulator (VT100/ANSI) with scrollback, copy/paste, and true‑color support.
2. **IDE Mode** – a lightweight but powerful code editor with syntax highlighting, autocomplete, file explorer, Git integration, REPL, debugging, and more.

Think of it as a **terminal + IDE hybrid** – you can run your Xell scripts, debug them, and edit them all in one window.

---

## Quick Reference

| Command | What it does | Example |
|---------|--------------|---------|
| `xell --terminal` | Launches the Terminal GUI | `$ xell --terminal` |
| `xell --ide` | Alias for `--terminal` | `$ xell --ide` |
| `xell --customize` | Launches the color customizer web app | `$ xell --customize` |
| `Ctrl+Shift+D` | Toggle Dashboard (right panel) | – |
| `Ctrl+B` | Toggle Sidebar (file explorer) | – |
| `Ctrl+\`` | Toggle Bottom Panel (REPL/Output) | – |
| `Ctrl+T` | Switch to Terminal Mode | – |
| `:terminal` | Switch to Terminal Mode (command) | – |
| `:ide` | Switch to IDE Mode (command) | – |
| `:help` | Show help in Terminal Mode | – |
| `:quit` | Exit Xell | – |
| `Ctrl+Enter` | Run selection in REPL (IDE Mode) | – |
| `Ctrl+R` | Run current file (IDE Mode) | – |
| `Ctrl+Shift+D` | Debug Run (selection with lifecycle tracing) | – |
| `F5` | Start debug / Continue execution | – |
| `F9` | Toggle breakpoint on current line | – |
| `F10` | Step Over | – |
| `F11` | Step Into | – |
| `Shift+F11` | Step Out | – |
| `F12` | Stop debug session | – |
| `Ctrl+Shift+K` / `Ctrl+Shift+Q` | Emergency stop running program | – |
| `Ctrl+F` | Find (regex) in editor | – |
| `Ctrl+H` | Find & Replace (regex) in editor | – |
| `Ctrl+G` | Go to line number | – |
| `Ctrl+S` | Save current file | – |
| `Ctrl+N` | New file | – |
| `Ctrl+O` | Open file | – |
| `Ctrl+W` | Close current tab | – |
| `Ctrl+Tab` / `Ctrl+Shift+Tab` | Next / Previous editor tab | – |
| `Ctrl+L` | Clear output (in OUTPUT tab) | – |
| `Tab` | Cycle bottom panel tabs | – |
| `F3` / `Shift+F3` | Next / Previous match (in Find) | – |
| `Escape` | Close dialog / cancel | – |
| `Ctrl+Space` | Force autocomplete (IDE Mode) | – |
| `Ctrl+Shift+E` | Inline evaluate (show result as ghost text) | – |

---

## Launching the Terminal

```bash
$ xell --terminal
🖥️  Launching Xell Terminal...
```

Or:

```bash
$ xell --ide
```

### Font and Theme

The terminal uses a **monospace font** (loaded from `assets/`). The theme is loaded from `terminal_colors.json` (see [Themes](#themes) below).

### `xell --customize`

Launches a Python web app that lets you preview and customize the terminal's color scheme.

```bash
$ xell --customize
🎨 Launching Xell Color Customizer...
   Server: /usr/local/share/xell/color_customizer/customizer_server.py
```

---

## Two Modes

### Terminal Mode

Terminal Mode is a **full VT100/ANSI terminal emulator**. It behaves like a standard terminal:

- Runs a **Xell REPL** by default (or your shell if configured).
- Supports true‑color (24‑bit), bold, italic, underline.
- Scrollback buffer (scroll up with mouse or `Shift+PageUp`/`Shift+PageDown`).
- Copy/paste via `Ctrl+Shift+C` / `Ctrl+Shift+V`.
- Resizes dynamically with the window.

**Switching to Terminal Mode:**

- Press `Ctrl+T` (from IDE Mode).
- Or type `:terminal` in the command input.

**In Terminal Mode:**

- You can run Xell code interactively.
- Use `:ide` to switch back to IDE Mode.
- Use `:help` to see available commands.
- Use `:quit` to exit Xell.

---

### IDE Mode

IDE Mode is a **lightweight code editor** with:

- Tabbed editor with syntax highlighting (Xell, JSON, TOML, YAML, etc.).
- File explorer (sidebar).
- Autocomplete (keywords, builtins, user symbols, snippets).
- Inline diagnostics (linting via `--check`).
- REPL integration (run code directly from the editor).
- Variable inspector (LIFECYCLE tab) – variable birth/change/death events.
- Debugging (breakpoints, step‑over/into/out).
- Git integration (gutter diff, staging, commit).
- Dashboard (code structure tree with click‑to‑jump navigation).
- Bottom panel with tabs: **TERMINAL**, **OUTPUT**, **DIAGNOSTICS**, **VARIABLES**, **OBJECTS**, **LIFECYCLE**, **TIMELINE**, **CALLSTACK**, **HELP**.

**Switching to IDE Mode:**

- Type `:ide` in the Terminal Mode command input.
- Or press `Ctrl+T` from anywhere (toggles between modes if already in IDE).

---

## IDE Mode – Detailed Features

### Editor

The editor is the central component of IDE Mode.

**Syntax Highlighting:**

- Full Xell grammar: keywords, literals, operators, comments, builtins, function calls, method calls, decorators, string interpolation.
- Supports Xell, Xell Notebook (`.nxel`), Xell Meta (`.xell_meta`), and Xell Dialect (`.xesy`) files.

**Line Numbers & Gutter:**

- Line numbers on the left.
- Gutter markers for breakpoints (`●`) and debug execution pointer (`▶`).
- Inline diagnostics (squiggly underlines for errors/warnings).

**Autocomplete:**

- Triggered automatically after typing (debounced) or with `Ctrl+Space`.
- Completions include keywords, builtins, user symbols, snippets, and module members (for `->` access).
- Fuzzy matching with scoring.
- **Snippets** – expandable with tab stops (`${1:name}`). Press `Tab` to navigate stops.

**Ghost Text:**

- After autocomplete, the remaining part of the selected completion is shown in dim grey.
- Press `Tab` to accept the ghost text.

**Find & Replace:**

- `Ctrl+F` – Find (regex support).
- `Ctrl+H` – Find & Replace (regex support).
- `F3` / `Shift+F3` – Next / Previous match.

**Go to Line:**

- `Ctrl+G` – opens a small overlay to enter a line number.

**Tab Management:**

- `Ctrl+Tab` – next tab.
- `Ctrl+Shift+Tab` – previous tab.
- `Ctrl+W` – close current tab.
- `Ctrl+N` – new file.
- `Ctrl+O` – open file.

**File Explorer (Sidebar):**

- `Ctrl+B` toggles the sidebar.
- Right‑click for context menu: **Open**, **Rename**, **Delete**, **New File**, **New Folder**, **Copy Path**, **Copy Relative Path**, **Show/Hide Hidden Files**.
- Inline rename/new file editing (enter name, press Enter to commit).

**Dashboard (Right Panel):**

- `Ctrl+Shift+D` toggles the Dashboard.
- Shows a hierarchical tree of all symbols in the current file (functions, classes, modules, variables, imports, parameters).
- **Click a symbol** to jump to its definition.
- **Click a variable** to show its lifecycle (BORN/CHANGED/DIED events) in a popup.

---

### REPL & Output

The bottom panel (`Ctrl+\`` toggles it) has several tabs:

| Tab | Description |
|-----|-------------|
| **TERMINAL** | A full terminal (PTY) running the Xell REPL or your shell. |
| **OUTPUT** | Output from `Ctrl+Enter` (run selection) and `Ctrl+R` (run file). |
| **DIAGNOSTICS** | Linting errors and warnings from `--check`. |
| **VARIABLES** | Runtime variables from the last execution (name, type, value, scope, lines). Click a variable to see its lifecycle. |
| **OBJECTS** | Static code symbols (functions, classes, modules, imports, etc.). Click to jump to definition. |
| **LIFECYCLE** | Variable lifecycle events (BORN, CHANGED, DIED) from `--trace-vars`. |
| **TIMELINE** | Debug event timeline (pause, step, breakpoint, watch events). |
| **CALLSTACK** | Call stack when debugging (click a frame to jump to that line). |
| **HELP** | Keyboard shortcuts reference. |

**Running Code:**

- `Ctrl+Enter` – Run the selected text (or the entire file up to the cursor) in the REPL. Output appears in the OUTPUT tab.
- `Ctrl+R` – Run the current file. Output appears in the OUTPUT tab.
- `Ctrl+Shift+D` – **Debug Run** – runs the selection with full lifecycle tracing (variables appear in LIFECYCLE tab) and debug breakpoints.

**Inline Evaluation:**

- `Ctrl+Shift+E` – Evaluate the current line (or selected text) and show the result as ghost text next to the cursor.

**Interactive Input:**

- When a running program calls `input()` (or `ask()`), the OUTPUT tab shows an input bar at the bottom. Type your input and press Enter.

---

### Debugging

The debugger integrates with the `xell --debug` server.

**Starting a Debug Session:**

- Press `F5` while in IDE Mode.
- The interpreter starts with `--debug` and the IDE connects to the debug socket.

**Breakpoints:**

- `F9` – Toggle breakpoint on the current line.
- Breakpoints are shown in the gutter as `●`.
- Purple `●` = `pause` breakpoint; Blue `●` = `snapshot` breakpoint.

**Step Controls:**

| Key | Action |
|-----|--------|
| `F5` | Continue execution (until next breakpoint or end) |
| `F10` | Step Over (skip function internals) |
| `F11` | Step Into (enter function) |
| `Shift+F11` | Step Out (return from current function) |
| `F12` | Stop debug session |

**Debug State:**

- When paused, the current line is highlighted in yellow.
- The **CALLSTACK** tab shows the call stack – click a frame to jump to that line.
- The **TIMELINE** tab shows all debug events.
- The **VARIABLES** tab shows runtime variables and their values at the pause point.

**Breakpoint Sync:**

- Breakpoints set in the editor are automatically sent to the interpreter.
- Breakpoints toggled in the editor while debugging are synced immediately.

---

### Git Integration

Git integration is available in the **Git Panel** (part of the bottom panel, but also accessible via the Git tab).

**Features:**

- Shows current branch and changed files.
- Gutter diff markers in the editor (green = added, red = deleted, blue = modified).
- Stage/unstage files (`Space`).
- Stage all (`Ctrl+A`).
- Commit (`Enter` after typing a commit message).
- Push (`Ctrl+P`).

**Git Panel Layout:**

| Row | Content |
|-----|---------|
| 0 | Branch name and file count |
| 1..N | Changed files (status icon, staged indicator, path) |
| N-1 | Commit message input |
| N | Status / help bar |

---

### Themes

The terminal loads its theme from `terminal_colors.json` (located in `assets/` or `~/.config/xell/`).

**Customizing the Theme:**

1. Use `xell --customize` to launch the color customizer web app.
2. Edit `terminal_colors.json` directly.
3. The theme is hot‑reloaded when the file changes (no restart needed).

**Theming Scopes:**

- `editor_bg`, `editor_fg`
- `gutter_bg`, `gutter_fg`, `gutter_active_fg`
- `selection_bg`
- `line_highlight`
- `tab_bar_bg`, `tab_active_bg`, `tab_inactive_bg`
- `statusbar_bg`, `statusbar_fg`
- Token styles for every Xell keyword, literal, operator, and builtin.

---

### Configuration (`xell_ide.json`)

The IDE reads configuration from `~/.config/xell/xell_ide.json` (or `%APPDATA%/xell/xell_ide.json` on Windows).

**Settings:**

| Section | Setting | Default | Description |
|---------|---------|---------|-------------|
| `editor` | `font_size` | `14` | Font size in points. |
| `editor` | `tab_size` | `4` | Number of spaces per tab. |
| `editor` | `word_wrap` | `false` | Wrap long lines. |
| `editor` | `line_numbers` | `true` | Show line numbers. |
| `editor` | `minimap` | `true` | Show code minimap. |
| `editor` | `bracket_matching` | `true` | Highlight matching `:` / `;` and `()`, `[]`, `{}`. |
| `editor` | `indent_guides` | `true` | Show vertical indent guides. |
| `editor` | `cursor_style` | `"block"` | `"block"`, `"bar"`, or `"underline"`. |
| `editor` | `cursor_blink` | `true` | Blink the cursor. |
| `editor` | `cursor_blink_rate` | `530` | Blink rate in milliseconds. |
| `editor` | `auto_indent` | `true` | Auto‑indent after `:` and dedent after `;`. |
| `editor` | `highlight_current_line` | `true` | Highlight the current line. |
| `editor` | `code_folding` | `true` | Enable code folding (visual only). |
| `editor` | `smooth_scrolling` | `true` | Smooth scroll animation. |
| `editor` | `smooth_scroll_speed` | `0.3` | Scroll animation speed (0.0–1.0). |
| `panels` | `file_explorer_visible` | `true` | Show sidebar on startup. |
| `panels` | `file_explorer_width` | `30` | Sidebar width in cells. |
| `panels` | `bottom_panel_visible` | `false` | Show bottom panel on startup. |
| `panels` | `bottom_panel_height` | `12` | Bottom panel height in cells. |
| `panels` | `minimap_visible` | `true` | Show minimap. |
| `panels` | `minimap_width` | `10` | Minimap width in cells. |
| `repl` | `history_size` | `1000` | Max REPL history entries. |
| `repl` | `auto_execute_on_enter` | `false` | Execute on Enter (vs needing `\` for multi‑line). |
| `repl` | `shell_passthrough` | `true` | Pass shell commands directly (vs via REPL). |
| `git` | `git_enabled` | `true` | Enable Git integration. |
| `git` | `git_gutter` | `true` | Show gutter diff markers. |
| `git` | `git_auto_refresh` | `true` | Auto‑refresh Git status. |
| `git` | `git_refresh_interval` | `5000` | Git refresh interval in ms. |
| `session` | `restore_session` | `true` | Restore open files on startup. |
| `session` | `max_recent_files` | `20` | Number of recent files to remember. |

**Example `xell_ide.json`:**

```json
{
  "editor": {
    "font_size": 16,
    "tab_size": 2,
    "cursor_style": "bar",
    "smooth_scrolling": true
  },
  "panels": {
    "file_explorer_visible": false,
    "bottom_panel_visible": true
  },
  "git": {
    "git_enabled": true,
    "git_gutter": true
  }
}
```

---

## Common Workflows

### 1. Edit and Run a Script

1. Open a file: `Ctrl+O` → select `.xel`.
2. Edit the code.
3. Save: `Ctrl+S`.
4. Run: `Ctrl+R`. Output appears in the OUTPUT tab.

### 2. Debug a Script

1. Set breakpoints: `F9` on lines.
2. Start debug: `F5`.
3. Use `F10`, `F11`, `Shift+F11`, `F5` to step through.
4. Inspect variables in the VARIABLES tab.
5. Stop: `F12`.

### 3. Interactive Development (REPL)

1. Open a file.
2. Select a block of code (or leave the cursor where you want).
3. `Ctrl+Enter` – runs the selection (or file up to cursor) in the REPL.
4. Output appears in the OUTPUT tab.
5. Variables appear in the VARIABLES tab.
6. Click a variable name → LIFECYCLE tab shows its full history.

### 4. Git Workflow

1. Make changes to a file.
2. The gutter shows diff markers (green/red/blue).
3. Open the Git panel (bottom panel → Git tab).
4. Stage files: select and press `Space`.
5. Type a commit message and press `Enter`.
6. Push: `Ctrl+P`.

### 5. Inline Evaluation

1. Place the cursor on an expression (or select it).
2. `Ctrl+Shift+E` – the result appears as ghost text next to the cursor.

---

## What NOT to Do

### 1. Expecting the Terminal to Work Without SDL2

The Xell Terminal requires SDL2 and SDL2_ttf. Install them with:

```bash
# Ubuntu/Debian
sudo apt install libsdl2-dev libsdl2-ttf-dev

# macOS
brew install sdl2 sdl2_ttf
```

### 2. Using `--terminal` in a Headless Environment

The Terminal GUI requires a display. Use `--kernel` or `--debug` for headless debugging.

### 3. Forgetting to Save Before Running

`Ctrl+R` runs the file as saved on disk. Unsaved changes are **not** included. Save (`Ctrl+S`) first, or use `Ctrl+Enter` to run the unsaved selection.

### 4. Expecting `Ctrl+Z` to Suspend the Process

`Ctrl+Z` is not bound to `SIGTSTP` in the terminal GUI. Use `Ctrl+Shift+K` / `Ctrl+Shift+Q` to stop a running program.

### 5. Using `Ctrl+C` for Copy When There's No Selection

If there is no text selection, `Ctrl+C` sends a `SIGINT` to the running process (in Terminal Mode) or is ignored (in IDE Mode). Use `Ctrl+Shift+C` for copy.

### 6. Expecting All Linting Errors to Be Caught in Real Time

The linter runs after a 500ms debounce on buffer changes. It may not catch errors immediately after typing.

### 7. Forgetting That Breakpoints Are File‑Specific

Breakpoints are tied to the file and line number. They are not preserved across editor tabs.

### 8. Using `:quit` in IDE Mode

`:quit` works in both Terminal Mode and IDE Mode (entered in the command input area). It exits Xell.

---

## See Also

- [CLI Commands](cli.md) – `--terminal`, `--ide`, `--customize`
- [Debugging](debugging.md) – `--debug`, `--trace-vars`
- [REPL Usage](repl.md) – interactive shell
- [Built‑ins Reference](../core/builtins.md) – functions available in the REPL

---

**Previous:** [Notebook Kernel](kernel.md)  
**Next:** [IDE](ide.md)