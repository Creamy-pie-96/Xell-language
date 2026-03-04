# 🖥️ Xell Terminal IDE — Master Blueprint

> **Goal**: Transform `xell-terminal` from a raw terminal emulator into a legendary, fully-featured terminal-native IDE for the Xell language — **NeoTokyo meets Vim**, fast, colorful, GPU-aware.

---

## 📊 Current Foundation (What We Already Have)

| Component | Status | Location |
|---|---|---|
| **Cell-grid renderer** | ✅ SDL2 + TTF, True Color, per-cell fg/bg/bold/italic/underline, dirty flag, glyph cache (8192) | `xell-terminal/src/renderer/` |
| **Screen buffer** | ✅ 2D grid + 5000-line scrollback deque, cursor, text selection, scroll | `xell-terminal/src/terminal/screen_buffer.*` |
| **VT100/ANSI parser** | ✅ Full state machine (CSI, OSC, DCS, SGR), 24-bit color, 256-color | `xell-terminal/src/terminal/vt_parser.*` |
| **PTY layer** | ✅ Cross-platform (forkpty/ConPTY), resize, thread-safe reader | `xell-terminal/src/pty/` |
| **Input handler** | ✅ Full keyboard translation, Ctrl combos, F1-F12, mouse, kitty protocol | `xell-terminal/src/terminal/input_handler.*` |
| **REPL** | ✅ Multiline editing, tab completion (keywords+builtins+vars), history (1000 entries), shell passthrough, auto-indent | `src/repl/` |
| **Xell Lexer/Parser** | ✅ Full tokenizer + AST builder | `src/lexer/`, `src/parser/` |
| **Static analyzer** | ✅ `--check` mode, 372 builtins, all AST nodes | `src/analyzer/static_analyzer.hpp` |
| **Token color data** | ✅ 50+ tokens with scope→color→bold/italic mapping | `Extensions/xell-vscode/color_customizer/token_data.json` |
| **Language data** | ✅ 54 keywords + 372 builtins with categories, signatures, hover docs | `Extensions/xell-vscode/src/server/language_data.json` |
| **TextMate grammar** | ✅ Full Xell syntax rules | `Extensions/xell-vscode/syntaxes/xell.tmLanguage.json` |
| **Color customizer** | ✅ Web app to customize all token colors, generates VS Code JSON + preview | `Extensions/xell-vscode/color_customizer/` |
| **Text selection** | ✅ Mouse drag, double-click word select, Ctrl+C/V clipboard | `xell-terminal/src/main.cpp` |
| **Context menu** | ✅ Right-click overlay (Copy/Paste/Select All) | `xell-terminal/src/main.cpp` |
| **Scrollbar** | ✅ Draggable thumb, track rendering | `xell-terminal/src/renderer/` |

---

## 🏗️ Architecture Overview

```
┌──────────────────────────────────────────────────────────┐
│                    Xell Terminal IDE                       │
├──────────────────────────────────────────────────────────┤
│  ┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐   │
│  │ Tab Bar  │ │ Menu Bar │ │ Status   │ │ Toolbar  │   │
│  └──────────┘ └──────────┘ │ Bar      │ └──────────┘   │
│  ┌────────────────────┐    └──────────┘                  │
│  │  File Explorer     │ ┌────────────────────────────┐  │
│  │  (tree view)       │ │  Editor Panel              │  │
│  │                    │ │  ┌──────┬───────────────┐  │  │
│  │  📁 src/           │ │  │Gutter│ Code Area     │  │  │
│  │  ├── main.xel      │ │  │ 1  │ fn main():    │  │  │
│  │  ├── utils.xel     │ │  │ 2  │   say "hi"    │  │  │
│  │  └── lib/          │ │  │ 3  │ ;             │  │  │
│  │                    │ │  └──────┴───────────────┘  │  │
│  │                    │ │  ┌──────────────────────┐   │  │
│  │                    │ │  │ Autocomplete Popup   │   │  │
│  │                    │ │  │  fn_name()           │   │  │
│  │                    │ │  │  for x in ...        │   │  │
│  └────────────────────┘ │  └──────────────────────┘   │  │
│  ┌──────────────────────────────────────────────────┐   │
│  │  Bottom Panel (REPL / Terminal / Output / Diag)   │  │
│  │  >>> say "hello"                                   │  │
│  │  hello                                             │  │
│  └──────────────────────────────────────────────────┘   │
├──────────────────────────────────────────────────────────┤
│  Config Layer: xell_ide.json / token_colors.json         │
└──────────────────────────────────────────────────────────┘
```

---

## 🎯 Phased Implementation Plan

### Phase 0 — Color Bridge (Prerequisite)
> **Connect the color customizer to the terminal**

#### 0.1 — Generate `terminal_colors.json`
- Extend `gen_xell_grammar.py` (or add a new function) to produce a JSON file consumable by xell-terminal
- Schema:
  ```json
  {
    "version": 1,
    "theme": "xell-default",
    "colors": {
      "editor_bg": "#121212",
      "editor_fg": "#cccccc",
      "gutter_bg": "#1a1a1a",
      "gutter_fg": "#555555",
      "line_highlight": "#1e1e2e",
      "selection_bg": "#264f78",
      "cursor": "#ffffff",
      "scrollbar_bg": "#1a1a1a",
      "scrollbar_thumb": "#555555",
      "panel_border": "#333333",
      "tab_active_bg": "#1e1e2e",
      "tab_inactive_bg": "#121212",
      "status_bar_bg": "#007acc",
      "status_bar_fg": "#ffffff",
      "popup_bg": "#252526",
      "popup_border": "#454545",
      "popup_selected": "#094771"
    },
    "token_colors": [
      {
        "scope": "keyword.control.conditional.xell",
        "fg": "#e06c75",
        "bold": false,
        "italic": false
      },
      {
        "scope": "keyword.control.loop.xell",
        "fg": "#e06c75",
        "bold": false,
        "italic": false
      }
    ]
  }
  ```
- The customizer HTML already generates `token_data.json` — add a second export button: **"Export Terminal Colors"**
- Install path: `~/.config/xell/terminal_colors.json` or `/usr/local/share/xell/terminal_colors.json`

#### 0.2 — Terminal color loader
- New C++ module: `xell-terminal/src/theme/theme_loader.hpp`
- Parses `terminal_colors.json` at startup
- Provides `Color getTokenColor(const std::string& scope)` API
- Falls back to built-in defaults if no JSON found

#### 0.3 — Terminal tokenizer bridge
- New C++ module: `xell-terminal/src/highlight/highlighter.hpp`
- Uses the existing Xell `Lexer` (from `src/lexer/lexer.cpp`)
- Maps each `Token::Type` → TextMate scope string → color from theme
- API: `std::vector<ColoredSpan> highlight(const std::string& line)`
- Each `ColoredSpan` = `{ start_col, end_col, fg, bg, bold, italic }`

---

### Phase 1 — Core Editor Component
> **Turn the terminal from "shell emulator" into "code editor"**

#### 1.1 — Editor buffer (text model)
- New: `xell-terminal/src/editor/text_buffer.hpp`
- Rope or gap-buffer backed text storage
- Line-indexed access: `getLine(n)`, `insertAt(row, col, text)`, `deleteRange(start, end)`
- Undo/redo stack (command pattern: Insert, Delete, Replace)
- Modified flag, save/load from disk
- UTF-8 aware cursor positioning

#### 1.2 — Editor viewport
- New: `xell-terminal/src/editor/editor_view.hpp`
- Maps text_buffer lines → screen rows (with scroll offset)
- Horizontal scroll support
- Visible range tracking: `first_visible_line`, `last_visible_line`
- Cursor: row, col, selection anchor
- Render to a region of the ScreenBuffer (not the whole screen)

#### 1.3 — Line numbers & gutter
- Left margin: configurable width (auto-size based on line count)
- Line numbers rendered with `gutter_fg` / `gutter_bg` from theme
- Current line highlight
- Gutter markers area (for breakpoints, errors, git diff later)

#### 1.4 — Keyboard editing
- Character insertion, backspace, delete
- Arrow keys (with Shift for selection)
- Ctrl+Left/Right (word jump), Ctrl+Shift+Left/Right (word select)
- Home/End (line), Ctrl+Home/End (document)
- Tab (indent), Shift+Tab (outdent)
- Enter (newline + auto-indent based on `:` depth)
- Ctrl+Z / Ctrl+Y (undo/redo)
- Ctrl+D (duplicate line)
- Ctrl+Shift+K (delete line)

#### 1.5 — Syntax highlighting (live)
- On every edit, re-highlight the visible lines using `highlighter.hpp`
- Incremental: only re-tokenize changed lines + continuation
- Write colored cells directly into the editor viewport region of ScreenBuffer

#### 1.6 — Multi-tab
- Tab bar at top of editor area
- Each tab = { filename, text_buffer, editor_view, modified_flag }
- Ctrl+Tab / Ctrl+Shift+Tab to switch
- Ctrl+W to close
- Visual indicators: modified dot, active highlight

---

### Phase 2 — Panels & Layout System
> **Split the screen into resizable regions**

#### 2.1 — Panel manager
- New: `xell-terminal/src/ui/panel_manager.hpp`
- Manages a tree of panels (horizontal/vertical splits)
- Each panel has: `Rect { x, y, w, h }`, content type, focus state
- Panel types: `Editor`, `Terminal`, `FileExplorer`, `Output`, `Diagnostics`
- Drag-to-resize borders (mouse on border → resize adjacent panels)

#### 2.2 — File explorer panel
- Tree view of project directory
- Icons: 📁 folder, 📄 file (with extension-based coloring)
- Keyboard navigation: Up/Down, Enter (open/expand), Left (collapse)
- Mouse: click to select, double-click to open in editor
- File operations: rename (F2), delete (Delete), new file (Ctrl+N)

#### 2.3 — Bottom panel (REPL / Terminal / Output)
- Tabbed bottom panel with multiple sub-panels:
  - **REPL**: Existing Xell REPL, integrated directly
  - **Terminal**: Raw shell (existing PTY)
  - **Output**: stdout/stderr from running scripts
  - **Diagnostics**: Error/warning list from `--check`
- Toggle with Ctrl+` (backtick)
- Resize by dragging the top border

#### 2.4 — Status bar
- Bottom-most row of the window
- Left: file name, line:col, encoding, line ending
- Center: mode indicator (NORMAL / INSERT / SEARCH)
- Right: language (Xell), git branch, diagnostics count (⚠ 3 ✗ 1)

---

### Phase 3 — Intelligence (Autocomplete, Diagnostics, Navigation)
> **Make it smart — leverage existing analyzers**

#### 3.1 — Autocompletion popup
- New: `xell-terminal/src/ui/autocomplete.hpp`
- Floating overlay rendered on top of editor
- Triggered by: typing (after `.`, after keyword, etc.), or Ctrl+Space
- Sources:
  - Keywords (from `language_data.json` — 54)
  - Builtins (from `language_data.json` — 372, with categories)
  - User-defined names (from current buffer's AST: variables, functions, classes)
  - Snippets (from `snippets/xell.json`)
- Fuzzy matching with scoring
- Up/Down to navigate, Enter/Tab to accept, Esc to dismiss
- Show signature + category in a detail column

#### 3.2 — Inline diagnostics
- Run static analyzer (`static_analyzer.hpp`) on save or after idle timeout
- Render inline: red squiggly underline (series of ~ chars in diagnostic color)
- Gutter markers: 🔴 error, 🟡 warning
- Hover over error → tooltip popup with message
- Diagnostics panel (Phase 2.3) lists all errors with jump-to-line

#### 3.3 — Hover information
- Mouse hover or keyboard shortcut (Ctrl+K Ctrl+I) over a symbol
- Show popup with:
  - Signature (from `language_data.json` hover docs)
  - Parameter descriptions
  - Category
- For user-defined functions: show the `fn` signature from AST

#### 3.4 — Go to definition
- Ctrl+Click or F12 on a symbol
- For `bring` imports: jump to the source file
- For function calls: jump to the `fn` definition
- For module references: jump to the `module` declaration
- Uses the AST from the parser — build a simple symbol table

#### 3.5 — Search & replace
- Ctrl+F → search bar at top of editor
- Ctrl+H → search + replace bar
- Highlight all matches in editor (colored overlay)
- F3 / Shift+F3 to navigate matches
- Regex support
- Ctrl+Shift+F → search across all files (in file explorer scope)

---

### Phase 4 — REPL Integration (Deep)
> **Seamless code ↔ REPL workflow**

#### 4.1 — Run selection in REPL
- Select code in editor → Ctrl+Enter → send to REPL panel
- REPL executes and shows result in bottom panel

#### 4.2 — Run file
- Ctrl+Shift+B → run current file with `xell <file>`
- Output appears in Output panel
- Errors are parsed and shown as diagnostics

#### 4.3 — REPL variable inspector
- Side panel showing all variables in the current REPL session
- Updates live after each execution
- Shows type, value preview, expandable for collections

#### 4.4 — Inline evaluation
- Ctrl+Shift+E on a line → evaluate in REPL → show result as ghost text at end of line
- E.g., `let x = 42 * 2  # → 84` (in dim gray)

---

### Phase 5 — Git Integration
> **Visual diff and version control**

#### 5.1 — Gutter diff markers
- Green bar: added lines
- Red bar: deleted lines  
- Blue bar: modified lines
- Uses `libgit2` or shells out to `git diff`

#### 5.2 — Inline diff view
- Ctrl+D → show diff of current file vs HEAD
- Side-by-side or inline mode
- Color-coded additions/deletions

#### 5.3 — Git status in file explorer
- Modified files: yellow dot
- New files: green dot
- Deleted: red strikethrough

#### 5.4 — Commit panel
- Git panel in bottom area
- Stage/unstage files
- Write commit message
- Push/pull buttons

---

### Phase 6 — Visual Effects & Polish
> **Make it legendary**

#### 6.1 — Cursor styles
- Block (default), bar (|), underline (_)
- Blink rate configurable
- Different cursor in different modes (block for normal, bar for insert)

#### 6.2 — Smooth scrolling
- Animate scroll offset over 2-3 frames
- Mouse wheel = smooth, keyboard = instant (configurable)

#### 6.3 — Code minimap
- Right side of editor (narrow column)
- Each line = 1-2 pixel row, colored by syntax
- Viewport indicator (rectangle showing visible area)
- Click to jump

#### 6.4 — Bracket matching
- Highlight matching `:` / `;` pairs
- Rainbow brackets (depth-based coloring)

#### 6.5 — Indent guides
- Thin vertical lines at each indent level
- Active indent guide highlighted

#### 6.6 — Code folding
- Click gutter arrow or Ctrl+Shift+[ to collapse block
- `:` opens fold, `;` closes fold
- Show `...` indicator for collapsed blocks
- Ctrl+Shift+] to expand

#### 6.7 — Animations
- Popup fade-in/out (3-4 frame alpha transition)
- Status messages slide in from right
- Cursor smooth caret (optional, cell-based approximation)

---

### Phase 7 — Configuration & Extensibility
> **User-customizable everything**

#### 7.1 — Config file: `~/.config/xell/xell_ide.json`
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
    "auto_indent": true,
    "highlight_current_line": true
  },
  "theme": "terminal_colors.json",
  "keybindings": {
    "save": "Ctrl+S",
    "find": "Ctrl+F",
    "run_file": "Ctrl+Shift+B",
    "toggle_terminal": "Ctrl+`",
    "go_to_definition": "F12",
    "autocomplete": "Ctrl+Space"
  },
  "panels": {
    "file_explorer": { "visible": true, "width": 30 },
    "bottom_panel": { "visible": true, "height": 12 },
    "minimap": { "visible": true, "width": 10 }
  },
  "repl": {
    "history_size": 1000,
    "auto_execute_on_enter": false,
    "shell_passthrough": true
  }
}
```

#### 7.2 — Hot-reload
- Watch config file for changes → apply without restart
- Watch `terminal_colors.json` → re-apply theme instantly

#### 7.3 — Plugin system (future)
- Xell scripts as plugins: `~/.config/xell/plugins/*.xel`
- Plugin API: register commands, key bindings, panel providers
- Sandboxed execution through the interpreter

---

## 📋 Implementation Priority & Ordering

```
Phase 0 (Color Bridge)          ← START HERE
  ↓
Phase 1.1-1.3 (Text buffer,    ← Core editor
               viewport,
               line numbers)
  ↓
Phase 1.4-1.5 (Keyboard +      ← Usable editor
               syntax highlight)
  ↓
Phase 1.6 (Multi-tab)          ← Multiple files
  ↓
Phase 2.1-2.3 (Panels,         ← IDE layout
               file explorer,
               bottom panel)
  ↓
Phase 3.1 (Autocomplete)       ← Intelligence
  ↓
Phase 3.2 (Diagnostics)        ← Linting
  ↓
Phase 3.5 (Search)             ← Navigation
  ↓
Phase 4.1-4.2 (REPL run)       ← Integration
  ↓
Phase 2.4 (Status bar)         ← Polish
  ↓
Phase 6.x (Visual effects)     ← Legendary
  ↓
Phase 5.x (Git)                ← Pro features
  ↓
Phase 7.x (Config/plugins)     ← Extensibility
  ↓
Phase 3.3-3.4 (Hover, GoTo)    ← Advanced intelligence
  ↓
Phase 4.3-4.4 (Variable        ← Advanced REPL
               inspector,
               inline eval)
```

---

## 🧱 New File Structure

```
xell-terminal/
├── CMakeLists.txt                    (updated)
├── src/
│   ├── main.cpp                      (updated — mode switching: terminal vs IDE)
│   ├── pty/                          (existing — unchanged)
│   ├── terminal/                     (existing — used for raw terminal mode)
│   ├── renderer/                     (existing — extended)
│   │   ├── renderer.hpp              (extended with new draw methods)
│   │   └── renderer.cpp
│   ├── theme/                        ★ NEW
│   │   ├── theme_loader.hpp          (parse terminal_colors.json)
│   │   └── default_theme.hpp         (built-in fallback colors)
│   ├── highlight/                    ★ NEW
│   │   ├── highlighter.hpp           (Lexer → colored spans)
│   │   └── scope_map.hpp             (Token::Type → scope string mapping)
│   ├── editor/                       ★ NEW
│   │   ├── text_buffer.hpp           (gap buffer / rope text storage)
│   │   ├── editor_view.hpp           (viewport, cursor, selection)
│   │   ├── editor_commands.hpp       (undo/redo command pattern)
│   │   └── code_folding.hpp          (fold regions, collapse state)
│   ├── ui/                           ★ NEW
│   │   ├── panel_manager.hpp         (layout tree, resize, focus)
│   │   ├── tab_bar.hpp               (multi-tab with indicators)
│   │   ├── file_explorer.hpp         (tree view of project dir)
│   │   ├── autocomplete.hpp          (popup overlay with fuzzy match)
│   │   ├── search_bar.hpp            (find/replace overlay)
│   │   ├── status_bar.hpp            (bottom info bar)
│   │   ├── diagnostics_panel.hpp     (error/warning list)
│   │   └── hover_popup.hpp           (tooltip for symbols)
│   ├── lsp/                          ★ NEW
│   │   ├── diagnostics_engine.hpp    (run static analyzer, collect errors)
│   │   ├── symbol_table.hpp          (definitions from AST)
│   │   └── completion_provider.hpp   (keywords + builtins + user symbols)
│   └── config/                       ★ NEW
│       ├── config_loader.hpp         (parse xell_ide.json)
│       └── keybindings.hpp           (key → action mapping)
└── assets/
    ├── fonts/                        (existing)
    └── default_theme.json            (bundled default terminal_colors.json)
```

---

## 🔑 Key Design Decisions

1. **SDL2 stays** — It's already working beautifully for rendering. No need to switch.

2. **Two modes**: `xell-terminal` (raw terminal) vs `xell-terminal --ide` (editor mode). Default remains terminal. The IDE mode shares the same renderer and PTY but adds editor panels on top.

3. **Lexer reuse**: The existing `src/lexer/lexer.cpp` is compiled into both `xell` and `xell-terminal`. Syntax highlighting uses the real Xell tokenizer — no regex hacks.

4. **Color source of truth**: `terminal_colors.json` generated by the customizer. One file controls ALL colors in the IDE. Hot-reloadable.

5. **Header-heavy design**: Following the existing pattern (`repl.hpp`, `line_editor.hpp`), most modules are header-only for simplicity and fast iteration.

6. **Incremental approach**: Each phase produces a usable improvement. Phase 0+1 alone gives you a syntax-highlighted code editor. Phase 2 adds the IDE layout. Phase 3 makes it smart.

---

## 🏁 Let's Start — Phase 0 First

The immediate next step is Phase 0: **Color Bridge** — the foundation that everything else builds on:

1. **`terminal_colors.json` generation** — extend the customizer to export terminal-consumable colors
2. **`theme_loader.hpp`** — C++ JSON parser to load the theme  
3. **`highlighter.hpp`** — Lexer → colored spans using theme colors

Once Phase 0 is done, we have the bridge between the Xell ecosystem (grammar, colors, keywords) and the terminal renderer — and everything else flows from there.
