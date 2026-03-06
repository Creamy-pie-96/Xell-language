# Xell IDE — Debug & Development Features Guide

## Table of Contents

- [IDE Layout Overview](#ide-layout-overview)
- [Dashboard Panel (Right Sidebar)](#dashboard-panel-right-sidebar)
- [Variables Tab](#variables-tab)
- [Objects Tab](#objects-tab)
- [Keyboard Shortcuts](#keyboard-shortcuts)
- [Tracing & Debug System](#tracing--debug-system)
- [Upcoming Features](#upcoming-features)

---

## IDE Layout Overview

The Xell IDE terminal has four main regions:

```
┌──────────┬──────────────────────┬────────────┐
│ Sidebar  │       Editor         │ Dashboard  │
│ (files)  │   (code editing)     │ (symbols)  │
│          │                      │            │
│ Ctrl+B   │                      │ Ctrl+Sh+D  │
│ toggle   │                      │ toggle     │
├──────────┴──────────────────────┴────────────┤
│  Bottom Panel: TERMINAL | OUTPUT | ...       │
│  Ctrl+` to toggle                            │
└──────────────────────────────────────────────┘
```

- **Left Sidebar**: File tree navigator. Toggle with `Ctrl+B`.
- **Editor**: Main code editing area with syntax highlighting, autocomplete.
- **Dashboard** (right): Code structure overview — functions, classes, modules at a glance.
  Toggle with `Ctrl+Shift+D`.
- **Bottom Panel**: Tabs for Terminal, Output, Diagnostics, Variables, Objects, Help.
  Toggle with `Ctrl+` ` (backtick).

All panel borders are draggable to resize.

---

## Dashboard Panel (Right Sidebar)

The Dashboard displays a structural overview of the currently open `.xel` file,
extracted via static analysis (`--check-symbols`).

### Symbols Displayed

| Icon | Meaning           |
| ---- | ----------------- |
| ƒ    | Function / Method |
| ◆    | Class             |
| ◇    | Module            |
| ◊    | Struct            |
| ↻    | Loop              |
| •    | Variable          |

### Features

- **Click to jump**: Click any symbol to jump to its definition line in the editor.
- **Tree view**: Parent symbols (classes, modules, functions) can be **expanded/collapsed**
  by clicking. Children (methods, fields) are shown indented.
- **Hover highlight**: Hovering over a symbol row highlights it.
- **Line numbers**: Right-aligned line numbers (e.g., `L10-25`) show the scope range.
- **Auto-refresh**: Symbols update automatically as you type (lazy analysis).

### Toggle

- `Ctrl+Shift+D` — Toggle dashboard visibility.
- Double-click the dashboard border to toggle.
- Drag the border to resize.

---

## Variables Tab

The **VARIABLES** tab in the bottom panel shows **runtime variables** — only actual
variables that exist after running your code (via `Ctrl+R`).

### Columns

| Column | Description                                           |
| ------ | ----------------------------------------------------- |
| NAME   | Variable name (clickable — jumps to definition)       |
| TYPE   | Inferred type (int, str, list, tuple, etc.)           |
| VALUE  | Current runtime value                                 |
| SCOPE  | Where it lives: `global`, `fn:name`, `for-loop`, etc. |
| LINES  | Definition line or range                              |

### How It Works

1. **Before running** (lazy analysis): Shows variables detected statically from
   the editor buffer with their inferred types and scopes.
2. **After Ctrl+R** (runtime): Shows actual runtime variable values, enriched with
   static type/scope information.

### Click to Jump

Click any variable row to jump to its definition line in the editor.

---

## Objects Tab

The **OBJECTS** tab shows **all symbols** detected by static analysis — not just
variables, but also functions, classes, modules, imports, parameters, etc.

This provides a complete picture of every named entity in your code, useful for:

- Understanding code structure
- Finding all imports and their sources
- Seeing function signatures and parameter details
- Navigating to any symbol definition

### Separation of Concerns

- **VARIABLES tab** → Only runtime variables (name, type, value, scope)
- **OBJECTS tab** → All symbols from static analysis (functions, classes, modules, imports, etc.)

---

## Keyboard Shortcuts

### Layout

| Shortcut            | Action                           |
| ------------------- | -------------------------------- |
| `Ctrl+B`            | Toggle left sidebar (file tree)  |
| `Ctrl+Shift+D`      | Toggle right sidebar (dashboard) |
| `Ctrl+` `           | Toggle bottom panel              |
| `Tab`               | Cycle bottom panel tabs          |
| Drag border         | Resize any panel                 |
| Double-click border | Toggle panel to default size     |

### Editor

| Shortcut          | Action                 |
| ----------------- | ---------------------- |
| `Ctrl+S`          | Save current file      |
| `Ctrl+N`          | New file               |
| `Ctrl+W`          | Close current tab      |
| `Ctrl+Tab`        | Next editor tab        |
| `Ctrl+Shift+Tab`  | Previous editor tab    |
| `Ctrl+G`          | Go to line number      |
| `Ctrl+F`          | Find (regex)           |
| `Ctrl+H`          | Find & Replace (regex) |
| `F3` / `Shift+F3` | Next / Previous match  |

### Execution

| Shortcut         | Action                           |
| ---------------- | -------------------------------- |
| `Ctrl+Enter`     | Run selection (or top-to-cursor) |
| `Ctrl+R`         | Run the current file             |
| `Ctrl+Shift+K/Q` | Emergency stop running program   |
| `Ctrl+L`         | Clear output (in OUTPUT tab)     |

### Mode

| Shortcut    | Action                                |
| ----------- | ------------------------------------- |
| `Ctrl+T`    | Switch to terminal mode               |
| `:ide`      | Switch to IDE mode (in terminal)      |
| `:terminal` | Switch to terminal mode (in terminal) |
| `:quit`     | Exit Xell                             |

---

## Tracing & Debug System

The Xell debug system is **language-native** — it traces program execution at the
language level, not the machine level.

### TraceCollector (Engine)

The `TraceCollector` records events during program execution:

| Event            | Description                 |
| ---------------- | --------------------------- |
| `VAR_BORN`       | New variable created        |
| `VAR_CHANGED`    | Variable reassigned         |
| `FN_CALLED`      | Function call begins        |
| `FN_RETURNED`    | Function returns a value    |
| `LOOP_STARTED`   | Loop begins                 |
| `LOOP_ITERATION` | Each loop iteration         |
| `LOOP_BROKE`     | `break` hit                 |
| `LOOP_COMPLETED` | Loop finishes naturally     |
| `BRANCH_IF`      | `if` condition true         |
| `BRANCH_ELIF`    | `elif` condition true       |
| `BRANCH_ELSE`    | `else` taken                |
| `BRANCH_SKIPPED` | Condition was false         |
| `ERROR_CAUGHT`   | Error caught by `try/catch` |
| `MODULE_LOADED`  | `bring` resolved a module   |

### TrackFilter (Selective Tracing)

You can filter what gets traced:

- **Whitelist**: Only trace specific variables, functions, or classes.
- **Blacklist**: Exclude specific variables or functions.
- **Category flags**: Enable/disable tracing for conditions, loops, calls, etc.

### Snapshots

Snapshots capture a **value copy** of all visible variables at a point in time,
including the full scope chain and call stack. They use string serialization,
never raw pointers — safe to read long after execution ends.

### Zero-Cost When Disabled

All trace hooks check `if (trace_ && trace_->enabled)` before any work.
When tracing is off, there is **zero overhead**.

---

## Upcoming Features

The debug system follows a 13-phase plan (see `readme/debug_ide_plan.md`):

- [x] Phase 1: Extended `--check-symbols` JSON
- [x] Phase 2: Enhanced Variables Tab
- [x] Phase 3: Dashboard Panel
- [x] Phase 4: TraceCollector engine + interpreter hooks
- [ ] Phase 5: `@breakpoint` + `@watch` decorators
- [ ] Phase 6: Cross-module debug
- [ ] Phase 7: Debug socket IPC
- [ ] Phase 8: Timeline Tab
- [ ] Phase 9: Step-through execution (F10/F11/F12)
- [ ] Phase 10: Time Travel engine
- [ ] Phase 11: Variable Lifecycle view
- [ ] Phase 12: Call Stack live view
- [ ] Phase 13: Polish & finishing
