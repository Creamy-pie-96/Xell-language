# ScriptIt VS Code Extension — How It All Works 🧩

Hey! If you're reading this, you probably want to understand how this extension
works from the inside. Don't worry — I'll explain everything step by step, like
we're building it together from scratch. By the time you finish reading this,
you'll be able to modify, extend, or even rewrite the whole thing yourself.

Let's go! 🚀

---

## Table of Contents

1. [What Even IS a VS Code Extension?](#1-what-even-is-a-vs-code-extension)
2. [The File Map — What's Where](#2-the-file-map--whats-where)
3. [How VS Code Knows About Our Extension](#3-how-vs-code-knows-about-our-extension)
4. [Syntax Highlighting — Making Code Colorful](#4-syntax-highlighting--making-code-colorful)
5. [The Language Server — The Brain](#5-the-language-server--the-brain)
6. [Auto-Completions — The Suggestions](#6-auto-completions--the-suggestions)
7. [Hover Info — The Tooltips](#7-hover-info--the-tooltips)
8. [Error Squiggles — The Red Lines](#8-error-squiggles--the-red-lines)
9. [Notebooks — The Interactive Cells](#9-notebooks--the-interactive-cells)
10. [Colors — How They Get Applied](#10-colors--how-they-get-applied)
11. [Putting It All Together](#11-putting-it-all-together)
12. [Quick Reference Table](#12-quick-reference-table)

---

## 1. What Even IS a VS Code Extension?

Think of VS Code as a house. By default, it's an empty house — it knows nothing
about ScriptIt. Our extension is like moving furniture in:

- "Hey VS Code, files ending in `.sit` are **ScriptIt** files"
- "Here's how to color the code"
- "Here are the error checks"
- "Here's how to run notebooks"

An extension is just a folder with some files. The most important file is
**`package.json`** — it's like the instruction manual that tells VS Code
everything about our extension.

The code itself is written in **TypeScript** (a nicer version of JavaScript).
We write `.ts` files, then compile them to `.js` files that VS Code actually runs.

```
You write:  src/client/extension.ts   (TypeScript, human-friendly)
    ↓ compile (npx tsc -b)
VS Code runs: out/client/extension.js  (JavaScript, machine-friendly)
```

---

## 2. The File Map — What's Where

Here's every file in the extension and what it does:

```
scriptit-vscode/
│
├── package.json                ← 🎯 THE BOSS. Tells VS Code everything.
│                                  "What files are ours, what features we have,
│                                   what commands exist, what colors to use."
│
├── language-configuration.json ← 📐 Bracket rules, comment characters, indent rules.
│                                  "When you type (, auto-insert )"
│
├── syntaxes/
│   └── scriptit.tmLanguage.json ← 🎨 The PAINTING RULES. Regex patterns that say
│                                    "if you see 'fn', color it yellow"
│                                    "if you see '42', color it orange"
│
├── snippets/
│   └── scriptit.json           ← ✂️ Code templates. Type "fn" + Tab → get a
│                                   whole function skeleton.
│
├── themes/
│   └── scriptit-icon-theme.json ← 📁 File icons. Makes .sit files show our icon.
│
├── images/
│   ├── icon.png                ← Extension marketplace icon
│   ├── sit-icon.svg            ← Icon for .sit files
│   └── nsit-icon.svg           ← Icon for .nsit notebook files
│
├── src/
│   ├── client/                 ← 🖥️ Runs IN VS Code (the "client" side)
│   │   ├── extension.ts        ← 🚀 THE ENTRY POINT. First thing that runs.
│   │   │                          Sets up colors, starts the server, registers
│   │   │                          notebooks, creates commands.
│   │   ├── notebookSerializer.ts ← 📓 Reads/writes .nsit notebook files.
│   │   │                           Converts JSON ↔ VS Code notebook format.
│   │   └── notebookController.ts ← 🧠 Runs notebook cells. Spawns the ScriptIt
│   │                               kernel and sends code to it.
│   │
│   └── server/                 ← 🔧 Runs as a SEPARATE PROCESS (the "server")
│       ├── server.ts           ← 🏠 Main server. Receives events from VS Code
│       │                          ("user typed something", "user wants completions")
│       │                          and responds.
│       ├── diagnostics.ts      ← 🔴 Error detection. Runs `scriptit --script`
│       │                          and parses the error messages.
│       ├── completions.ts      ← 💡 All the auto-complete items. Every keyword,
│       │                          builtin, and function suggestion lives here.
│       └── hover.ts            ← 📖 Hover tooltips. What you see when you hover
│                                  over `print` or `len` etc.
│
├── out/                        ← 📦 Compiled JavaScript (auto-generated, don't edit)
├── node_modules/               ← 📚 Libraries we depend on (auto-installed)
├── install.py                  ← 🔧 One-click installer for everything
├── how_to_use.md               ← 📘 User guide
├── how_to_customize.md         ← 🎨 Guide for changing colors, grammar, etc.
└── extension_internals.md      ← 📖 This file! You're reading it right now.
```

**The most important takeaway**: There are only TWO sides:

- **Client** (runs in VS Code): `extension.ts`, notebooks
- **Server** (separate process): diagnostics, completions, hover

They talk to each other using a protocol called **LSP** (Language Server Protocol).
Think of it like texting — VS Code sends messages ("hey, user typed something")
and the server replies ("here are the errors I found").

---

## 3. How VS Code Knows About Our Extension

Everything starts with **`package.json`**. When VS Code starts, it reads
every extension's `package.json` and learns:

### "When should I activate?"

```json
"activationEvents": [
    "onLanguage:scriptit",           // When a .sit file is opened
    "onNotebook:scriptit-notebook"   // When a .nsit file is opened
]
```

### "What files belong to this language?"

```json
"languages": [{
    "id": "scriptit",
    "extensions": [".si", ".sit"]    // These file types = ScriptIt
}]
```

### "Where's the coloring grammar?"

```json
"grammars": [{
    "language": "scriptit",
    "path": "./syntaxes/scriptit.tmLanguage.json"
}]
```

### "What commands exist?"

```json
"commands": [
    { "command": "scriptit.runFile", "title": "ScriptIt: Run Current File" }
]
```

### The full flow when you open a `.sit` file:

```
1. You open hello.sit
2. VS Code sees .sit → matches "scriptit" language
3. VS Code activates the extension → runs extension.ts
4. extension.ts does 3 things:
   a) Injects colors into your VS Code settings
   b) Registers the notebook serializer (for .nsit files)
   c) Starts the Language Server (separate process)
5. VS Code loads the TextMate grammar → colors your code
6. You start typing → VS Code asks the server for completions
7. You make a mistake → server detects it → red squiggle appears
```

---

## 4. Syntax Highlighting — Making Code Colorful

This is probably the part you're most curious about. Here's how it works:

### The Big Idea

VS Code uses something called **TextMate grammars**. It's a fancy name for:
**"A list of regex patterns that say what color each piece of code should be."**

Every piece of code gets assigned a **scope** (a label). Then the theme
(or our custom colors) maps that scope to an actual color.

```
Code:    fn add(x, y):
         ↓
Grammar: "fn"  → keyword.declaration.function.scriptit
         "add" → entity.name.function.definition.scriptit
         "("   → punctuation.bracket.round.scriptit
         "x"   → variable.parameter.scriptit
         ","   → punctuation.separator.comma.scriptit
         "y"   → variable.parameter.scriptit
         ")"   → punctuation.bracket.round.scriptit
         ":"   → punctuation.definition.function.scriptit
         ↓
Colors:  "fn"  → Yellow bold     (because we mapped that scope to yellow)
         "add" → Cyan            (functions are cyan)
         "x"   → White           (variables are white)
         ...
```

### The Grammar File

Open `syntaxes/scriptit.tmLanguage.json`. It has two main parts:

**1. `patterns`** — The top-level list. VS Code tries them **in order**, top to bottom.
The **first match wins**. This is super important!

```json
"patterns": [
    { "include": "#block-comment" },     // Try comments FIRST
    { "include": "#line-comment" },      // (so # in a comment doesn't break things)
    { "include": "#strings" },           // Then strings (so "if" isn't a keyword)
    { "include": "#function-definition" },
    ...
    { "include": "#identifiers" }        // LAST: anything left = variable
]
```

**Why order matters**: If `"if"` appears inside a string `"what if"`, the string
pattern matches FIRST (because strings come before keywords), so "if" stays
green (string color), not red (keyword color).

**2. `repository`** — Where each pattern is defined. There are 3 types:

#### Type 1: Simple Match (one regex, one scope)

```json
"line-comment": {
    "name": "comment.line.number-sign.scriptit",
    "match": "#.*$"
}
```

Translation: "If you see `#` followed by anything until end of line,
color it as `comment.line.number-sign.scriptit`."

#### Type 2: Match with Captures (different parts get different scopes)

```json
"match": "\\b(fn)\\s+([a-zA-Z_]\\w*)\\s*\\(",
"captures": {
    "1": { "name": "keyword.declaration.function.scriptit" },
    "2": { "name": "entity.name.function.definition.scriptit" }
}
```

Translation: "When you see `fn add(`, color `fn` as a keyword and `add` as a function name."

#### Type 3: Begin/End (for regions like strings or function bodies)

```json
"begin": "\"",
"end": "\"",
"name": "string.quoted.double.scriptit"
```

Translation: "Everything between opening `"` and closing `"` is a string."

### Scope Naming Cheat Sheet

VS Code has standard scope names. Here's what matters for us:

| You want to color...   | Use this scope prefix         | Our suffix  |
| ---------------------- | ----------------------------- | ----------- |
| Keywords (if, while)   | `keyword.control.*`           | `.scriptit` |
| Function names         | `entity.name.function.*`      | `.scriptit` |
| Variables              | `variable.other.*`            | `.scriptit` |
| Function parameters    | `variable.parameter.*`        | `.scriptit` |
| Builtin functions      | `support.function.*`          | `.scriptit` |
| Numbers                | `constant.numeric.*`          | `.scriptit` |
| Strings                | `string.quoted.*`             | `.scriptit` |
| Comments               | `comment.*`                   | `.scriptit` |
| Operators (+, -, ==)   | `keyword.operator.*`          | `.scriptit` |
| Brackets, commas       | `punctuation.*`               | `.scriptit` |
| Booleans (True, False) | `constant.language.boolean.*` | `.scriptit` |

Always end with `.scriptit` so our colors don't accidentally affect other languages!

---

## 5. The Language Server — The Brain

The **Language Server** is a separate program that runs alongside VS Code.
It's like having a smart assistant running in the background.

### Why a Separate Process?

VS Code's UI runs on one thread. If we did heavy processing (like running
the ScriptIt binary to check for errors) directly in the UI, VS Code would
freeze every time you typed. So instead:

```
┌──────────────┐     messages      ┌───────────────┐
│   VS Code    │  ←──────────→     │  Language      │
│   (Client)   │  "user typed X"  │  Server        │
│              │  ←──────────→    │  (server.ts)   │
│              │  "here are errors"│               │
└──────────────┘                  └───────────────┘
```

They communicate using **LSP** (Language Server Protocol) — a standard protocol
that many editors and languages use. This means our server could theoretically
work with Sublime Text, Neovim, etc.!

### What Messages Get Sent?

| VS Code says...           | Server responds with...           |
| ------------------------- | --------------------------------- |
| "Document changed"        | Error diagnostics (red squiggles) |
| "User wants completions"  | List of completion items          |
| "User hovers over a word" | Hover tooltip with documentation  |
| "User typed `(`"          | Signature help (parameter info)   |

### The Server Code

`src/server/server.ts` is the main server file. Here's the simplified flow:

```typescript
// 1. Create the connection to VS Code
const connection = createConnection();

// 2. Tell VS Code what we can do
connection.onInitialize(() => ({
  capabilities: {
    completionProvider: {}, // We can do completions
    hoverProvider: true, // We can do hover
    signatureHelpProvider: {}, // We can do signature help
  },
}));

// 3. When a document changes, check for errors
documents.onDidChangeContent((change) => {
  validateTextDocument(change.document);
});

// 4. When user wants completions, give them
connection.onCompletion(() => ALL_COMPLETIONS);

// 5. When user hovers, show info
connection.onHover((params) => {
  /* look up hover info */
});
```

---

## 6. Auto-Completions — The Suggestions

When you type `pr` and see `print` suggested — that's the completion system.

### Where They Live

Open `src/server/completions.ts`. You'll see arrays of completion items:

```typescript
export const SCRIPTIT_BUILTINS: CompletionItem[] = [
  {
    label: "print",
    kind: CompletionItemKind.Function,
    detail: "Print to console",
  },
  {
    label: "len",
    kind: CompletionItemKind.Function,
    detail: "Get length of a value",
  },
  {
    label: "input",
    kind: CompletionItemKind.Function,
    detail: "Read user input",
  },
  // ... more builtins
];

export const SCRIPTIT_KEYWORDS: CompletionItem[] = [
  {
    label: "var",
    kind: CompletionItemKind.Keyword,
    detail: "Declare a variable",
  },
  {
    label: "fn",
    kind: CompletionItemKind.Keyword,
    detail: "Define a function",
  },
  {
    label: "if",
    kind: CompletionItemKind.Keyword,
    detail: "Conditional statement",
  },
  // ... more keywords
];
```

### How to Add a New Completion

Just add a new item to the right array! Example — adding a `map()` builtin:

```typescript
{ label: 'map', kind: CompletionItemKind.Function, detail: 'Apply a function to each element' },
```

### Method Completions (After a Dot)

When you type `myList.` and see `push`, `pop`, `len` — that's method completion.
This is handled in `server.ts` → `getMethodCompletions()`. It returns a list of
methods that can be called on any value.

---

## 7. Hover Info — The Tooltips

When you hover over `print` and see a tooltip with the function signature —
that's the hover system.

### Where It Lives

Open `src/server/hover.ts`. You'll see a dictionary of hover information:

```typescript
export const HOVER_INFO: Record<string, HoverEntry> = {
  print: {
    signature: "print(value1, value2, ...)",
    detail: "Prints values to the console.",
    params: ["value — any value to print"],
  },
  len: {
    signature: "len(value)",
    detail: "Returns the length of a string, list, or map.",
    params: ["value — the value to measure"],
  },
  // ... more entries
};
```

### How It Works

1. User hovers over the word `print`
2. VS Code sends: "What's at position line 5, column 3?"
3. Server extracts the word under the cursor → `"print"`
4. Server looks up `HOVER_INFO["print"]` → found!
5. Server sends back the tooltip text
6. VS Code shows it in a little popup

### How to Add New Hover Info

Just add a new entry to the `HOVER_INFO` dictionary:

```typescript
myNewFunc: {
    signature: 'myNewFunc(x, y)',
    detail: 'Does something cool with x and y.',
    params: ['x — first value', 'y — second value']
},
```

---

## 8. Error Squiggles — The Red Lines

There are **two layers** of error detection:

### Layer 1: Static Analysis (instant, no subprocess)

This runs in `server.ts` → `validateTextDocument()` every time you type.
It checks things purely by looking at the text:

- **Unmatched brackets**: `if (x > 5` ← missing `)`
- **Unterminated strings**: `var x = "hello` ← missing closing `"`
- **Assignment in conditions**: `if x = 5:` ← should be `==`
- **Missing colons**: `fn add(x, y)` ← needs `:` or `.`

These checks are fast because they don't run any external program.

### Layer 2: Subprocess Validation (runs the real ScriptIt binary)

This runs in `diagnostics.ts`. It actually runs your code through
`scriptit --script` and parses the error messages:

```
1. Your code gets sent to `scriptit --script` via stdin
2. ScriptIt tries to parse/run it
3. If there's an error, ScriptIt prints: "Error: Unexpected token at line 5"
4. diagnostics.ts reads that output
5. It parses the line number and message
6. It sends it to VS Code → red squiggle appears at line 5
```

### Important: ScriptIt Writes Errors to STDOUT

Most programs write errors to stderr. ScriptIt writes them to **stdout**.
That's why `diagnostics.ts` reads BOTH stdout and stderr, and filters
stdout for lines that look like errors.

---

## 9. Notebooks — The Interactive Cells

When you open a `.nsit` file, you get an interactive notebook (like Jupyter).
Here's how it works:

### The .nsit File Format

It's just JSON:

```json
{
  "cells": [
    {
      "cell_type": "code",
      "source": "var x = 10\nprint(x).",
      "outputs": [{ "text": "10\n" }]
    },
    {
      "cell_type": "markdown",
      "source": "# This is a heading"
    }
  ]
}
```

### How It Opens

1. You open `hello.nsit`
2. VS Code sees `.nsit` → checks `package.json` → it's a notebook!
3. VS Code calls our **NotebookSerializer** (`notebookSerializer.ts`)
4. The serializer reads the JSON and converts it to VS Code's notebook format
5. VS Code displays the cells

### How Cells Execute

1. You click "Run" on a cell
2. VS Code calls our **NotebookController** (`notebookController.ts`)
3. The controller checks: is the kernel running? If not, spawn it:
   ```
   scriptit --kernel
   ```
4. The kernel sends back: `{"status": "kernel_ready"}`
5. Controller sends the cell code: `{"action": "execute", "code": "print(42)."}`
6. Kernel runs the code, captures output
7. Kernel replies: `{"status": "ok", "stdout": "42\n"}`
8. Controller shows "42" as the cell output

### How input() Works in Notebooks

When your code calls `input("Enter name: ")`:

1. The kernel can't read from terminal (there is no terminal!)
2. Instead, the kernel sends: `{"status": "input_request", "prompt": "Enter name: "}`
3. The controller sees this and shows a VS Code input dialog box
4. You type your answer and press Enter
5. Controller sends: `{"action": "input_reply", "text": "your answer"}`
6. Kernel receives it and continues executing

### The Kernel Protocol (Full Reference)

| Direction        | Message                                        | Meaning               |
| ---------------- | ---------------------------------------------- | --------------------- |
| Kernel → VS Code | `{"status": "kernel_ready"}`                   | Kernel is ready       |
| VS Code → Kernel | `{"action": "execute", "code": "..."}`         | Run this code         |
| Kernel → VS Code | `{"status": "ok", "stdout": "..."}`            | Code ran successfully |
| Kernel → VS Code | `{"status": "error", "stderr": "..."}`         | Code had an error     |
| Kernel → VS Code | `{"status": "input_request", "prompt": "..."}` | Need user input       |
| VS Code → Kernel | `{"action": "input_reply", "text": "..."}`     | Here's the input      |
| VS Code → Kernel | `{"action": "reset"}`                          | Clear all variables   |
| VS Code → Kernel | `{"action": "shutdown"}`                       | Stop the kernel       |

---

## 10. Colors — How They Get Applied

This was tricky to get right. Here's the story:

### The Problem

VS Code themes (like Dark+, One Dark Pro) set their own colors for scopes.
If we just rely on the theme, `keyword.control` might be purple in one theme
and red in another. We want consistent colors for ScriptIt regardless of theme.

### The Solution: Programmatic Color Injection

In `extension.ts`, there's a function called `applyScriptItColors()`. When the
extension activates, it:

1. Reads your current VS Code settings
2. Checks if ScriptIt color rules already exist
3. If not, it INJECTS our custom rules into `editor.tokenColorCustomizations`
4. These rules override the theme's colors but only for `.scriptit` scopes

```typescript
const SCRIPTIT_TOKEN_RULES = [
  {
    scope: "keyword.control.flow.scriptit",
    settings: { foreground: "#e06c75", fontStyle: "bold" },
  }, // Red bold
  {
    scope: "entity.name.function.call.scriptit",
    settings: { foreground: "#00ffff" },
  }, // Cyan
  // ... more rules
];
```

### The Color Scheme

| Token Type       | Color       | Hex       | Example             |
| ---------------- | ----------- | --------- | ------------------- |
| Control keywords | Red bold    | `#e06c75` | `if`, `else`, `for` |
| Special keywords | Yellow bold | `#e5c07b` | `fn`, `give`, `let` |
| Functions        | Cyan        | `#00ffff` | `print`, `myFunc`   |
| Variables        | White       | `#eeeeee` | `x`, `name`         |
| Numbers          | Orange      | `#d19a66` | `42`, `3.14`        |
| Booleans         | Purple      | `#c678dd` | `True`, `False`     |
| Operators        | Purple      | `#c678dd` | `+`, `==`, `&&`     |
| Strings          | Green       | `#98c379` | `"hello"`           |
| Block comments   | Green       | `#98c379` | `--> ... <--`       |
| Line comments    | Gray italic | `#5c6370` | `# comment`         |
| Punctuation      | Light gray  | `#abb2bf` | `(`, `)`, `,`       |
| Type conversions | Teal        | `#008080` | `int()`, `str()`    |
| var keyword      | White bold  | `#FFFFFF` | `var`               |

Want to change these colors? See `how_to_customize.md`!

---

## 11. Putting It All Together

Here's the complete lifecycle of typing code in a `.sit` file:

```
1. You open hello.sit
   └→ VS Code matches .sit → activates extension
      └→ extension.ts runs:
         ├→ Injects custom colors
         ├→ Registers notebook serializer
         └→ Starts language server (server.ts)

2. VS Code applies grammar (scriptit.tmLanguage.json)
   └→ Every word gets a scope → gets a color

3. You type: var name = input("Enter: ")
   └→ VS Code sends text to server
      └→ server.ts: validateTextDocument()
         ├→ Static analysis (bracket check, string check)
         └→ Subprocess validation (runs scriptit --script)
            └→ If errors found → red squiggles

4. You type "pri" and pause
   └→ VS Code asks for completions
      └→ server.ts → completions.ts → returns "print", "pprint"

5. You hover over "input"
   └→ VS Code asks for hover info
      └→ server.ts → hover.ts → returns signature + description

6. You press Ctrl+Shift+R
   └→ "ScriptIt: Run File" command fires
      └→ extension.ts opens terminal, runs: scriptit --script < hello.sit

7. You open hello.nsit
   └→ notebookSerializer.ts reads JSON → shows cells
   └→ You click Run
      └→ notebookController.ts → spawns kernel → sends code → shows output
```

---

## 12. Quick Reference Table

| I want to...                                | Edit this file                                   |
| ------------------------------------------- | ------------------------------------------------ |
| Add a new keyword to syntax highlighting    | `syntaxes/scriptit.tmLanguage.json`              |
| Change a color                              | `src/client/extension.ts` (SCRIPTIT_TOKEN_RULES) |
| Add a new auto-completion                   | `src/server/completions.ts`                      |
| Add hover documentation for a function      | `src/server/hover.ts`                            |
| Add a new error check (fast, text-based)    | `src/server/server.ts`                           |
| Add an error pattern (from ScriptIt output) | `src/server/diagnostics.ts`                      |
| Add a code snippet                          | `snippets/scriptit.json`                         |
| Change bracket/comment rules                | `language-configuration.json`                    |
| Add a VS Code command                       | `extension.ts` + `package.json`                  |
| Change file icon                            | `images/` + `themes/scriptit-icon-theme.json`    |
| Modify notebook cell execution              | `src/client/notebookController.ts`               |
| Modify .nsit file format                    | `src/client/notebookSerializer.ts`               |
| Add a ScriptIt language feature             | `REPL/perser.hpp` (tokenizer) + rebuild          |
| Add to the installer                        | `install.py`                                     |

---

## Build Commands Cheat Sheet

```bash
# Install dependencies (one time)
npm install

# Compile TypeScript → JavaScript
npx tsc -b

# Compile in watch mode (auto-recompiles on save)
npx tsc -b -w

# Package into .vsix file
npx @vscode/vsce package --allow-missing-repository --skip-license

# Install into VS Code
code --install-extension scriptit-lang-0.1.0.vsix

# Full install (build ScriptIt + extension + install everything)
python3 install.py

# Clean reinstall
python3 install.py --clean
```

### Testing Your Changes

- **Grammar changes**: Just reload VS Code (Ctrl+Shift+P → "Reload Window")
- **Server changes**: Recompile (`npx tsc -b`) → Reload Window
- **Client changes**: Recompile → Restart VS Code
- **Snippet changes**: Reload Window

### Debugging Tip

Use `Ctrl+Shift+P` → "Developer: Inspect Editor Tokens and Scopes"
Click on any character to see what scope it got assigned. If the color is wrong,
this tells you whether the problem is in the grammar (wrong scope) or the colors
(wrong mapping).

---

_That's it! You now know how every piece of this extension works. Go build something amazing!_ 🎉
