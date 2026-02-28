# ScriptIt Extension — How to Customize 🎨

This guide shows you how to change colors, add grammar rules, create snippets,
and tweak the extension to match your style. Everything is step-by-step with examples.

---

## Table of Contents

1. [Change Colors (The Easy Way — GUI)](#1-change-colors-the-easy-way--gui)
2. [Change Colors (The Manual Way)](#2-change-colors-the-manual-way)
3. [Add a New Keyword to Syntax Highlighting](#3-add-a-new-keyword-to-syntax-highlighting)
4. [Add a New Auto-Completion](#4-add-a-new-auto-completion)
5. [Add Hover Documentation](#5-add-hover-documentation)
6. [Add a Code Snippet](#6-add-a-code-snippet)
7. [Add a New Error Check](#7-add-a-new-error-check)
8. [Change Bracket/Comment Rules](#8-change-bracketcomment-rules)
9. [Rebuilding After Changes](#9-rebuilding-after-changes)

---

## 1. Change Colors (The Easy Way — GUI)

We have a visual color customizer! Open the file:

```
scriptit-vscode/color_customizer/index.html
```

in any web browser. It shows you all token types with color pickers.
Pick your colors, click "Copy Settings", and paste into your VS Code settings.

---

## 2. Change Colors (The Manual Way)

### Option A: Change for yourself only (VS Code settings)

1. Open VS Code Settings: `Ctrl+Shift+P` → "Preferences: Open Settings (JSON)"
2. Find the `editor.tokenColorCustomizations` section
3. Inside `textMateRules`, find or add the scope you want to change:

```json
{
  "editor.tokenColorCustomizations": {
    "textMateRules": [
      {
        "scope": "keyword.control.flow.scriptit",
        "settings": {
          "foreground": "#ff0000",
          "fontStyle": "bold"
        }
      }
    ]
  }
}
```

### Option B: Change for all users (in the extension code)

1. Open `src/client/extension.ts`
2. Find the `SCRIPTIT_TOKEN_RULES` array
3. Change the color hex code:

```typescript
// Change control keywords from red to blue:
{ scope: 'keyword.control.flow.scriptit',
  settings: { foreground: '#0088ff', fontStyle: 'bold' } },
```

4. Recompile: `npx tsc -b`
5. Repackage: `npx @vscode/vsce package --allow-missing-repository --skip-license`
6. Reinstall: `code --install-extension scriptit-lang-0.1.0.vsix --force`

### Color Reference

Here are all the scopes and their current colors:

| What                         | Scope                                     | Current Color           |
| ---------------------------- | ----------------------------------------- | ----------------------- |
| `if`, `else`, `for`, `while` | `keyword.control.flow.scriptit`           | `#e06c75` (red bold)    |
| `fn`, `give`                 | `keyword.declaration.function.scriptit`   | `#e5c07b` (yellow bold) |
| `let`, `be`, `new`           | `keyword.other.special.scriptit`          | `#e5c07b` (yellow bold) |
| `var`                        | `storage.type.var.scriptit`               | `#FFFFFF` (white bold)  |
| Variables                    | `variable.other.scriptit`                 | `#eeeeee` (white)       |
| Parameters                   | `variable.parameter.scriptit`             | `#eeeeee` (white)       |
| Function names               | `entity.name.function.call.scriptit`      | `#00ffff` (cyan)        |
| Builtin functions            | `support.function.builtin.scriptit`       | `#00ffff` (cyan)        |
| Numbers                      | `constant.numeric.integer.scriptit`       | `#d19a66` (orange)      |
| Booleans                     | `constant.language.boolean.true.scriptit` | `#c678dd` (purple)      |
| Strings                      | `string.quoted.double.scriptit`           | `#98c379` (green)       |
| `# comments`                 | `comment.line.number-sign.scriptit`       | `#5c6370` (gray italic) |
| `--> comments <--`           | `comment.block.arrow.scriptit`            | `#98c379` (green)       |
| Operators                    | `keyword.operator.arithmetic.scriptit`    | `#c678dd` (purple)      |
| Punctuation                  | `punctuation.bracket.round.scriptit`      | `#abb2bf` (light gray)  |
| Type conversions             | `support.type.conversion.scriptit`        | `#008080` (teal)        |
| `@` ref modifier             | `storage.modifier.reference.scriptit`     | `#e5c07b` (yellow bold) |

### Font Style Options

You can use these in `fontStyle`:

- `"bold"` — bold text
- `"italic"` — italic text
- `"bold italic"` — both
- `""` — normal (no style)

---

## 3. Add a New Keyword to Syntax Highlighting

**Example**: You added a `class` keyword to ScriptIt and want it colored.

### Step 1: Add the pattern to the grammar

Open `syntaxes/scriptit.tmLanguage.json` and add in the `repository`:

```json
"class-definition": {
    "patterns": [{
        "match": "\\b(class)\\s+([a-zA-Z_][a-zA-Z0-9_]*)\\s*(:)",
        "captures": {
            "1": { "name": "keyword.declaration.class.scriptit" },
            "2": { "name": "entity.name.type.class.scriptit" },
            "3": { "name": "punctuation.definition.class.scriptit" }
        }
    }]
}
```

### Step 2: Include it in the top-level patterns

Find the `"patterns"` array at the top of the file. Add your new pattern
**before** `"#identifiers"` (which should always be last):

```json
{ "include": "#class-definition" },
```

### Step 3: Add a color for the new scope

Open `src/client/extension.ts`, find `SCRIPTIT_TOKEN_RULES`, add:

```typescript
{ scope: 'keyword.declaration.class.scriptit',
  settings: { foreground: '#e5c07b', fontStyle: 'bold' } },
{ scope: 'entity.name.type.class.scriptit',
  settings: { foreground: '#61afef' } },
```

### Step 4: Rebuild and test

```bash
npx tsc -b
# Reload VS Code window (Ctrl+Shift+P → "Reload Window")
```

Use "Developer: Inspect Editor Tokens and Scopes" to verify the scope assignment.

---

## 4. Add a New Auto-Completion

Open `src/server/completions.ts`.

### Adding a builtin function:

```typescript
// In SCRIPTIT_BUILTINS array:
{ label: 'map', kind: CompletionItemKind.Function,
  detail: 'map(list, fn) — Apply fn to each element', data: 'fn_map' },
```

### Adding a keyword:

```typescript
// In SCRIPTIT_KEYWORDS array:
{ label: 'class', kind: CompletionItemKind.Keyword,
  detail: 'Define a class', data: 'kw_class' },
```

### Adding a method (appears after `.`):

Open `src/server/server.ts`, find `getMethodCompletions()`, and add:

```typescript
{ label: 'myMethod', detail: 'Does something cool',
  kind: CompletionItemKind.Method },
```

Rebuild: `npx tsc -b`

---

## 5. Add Hover Documentation

Open `src/server/hover.ts` and add a new entry:

```typescript
map: {
    signature: 'map(list, fn)',
    detail: 'Applies a function to each element of a list and returns a new list.',
    params: [
        'list — the list to iterate over',
        'fn — the function to apply to each element'
    ]
},
```

This gives you:

- Hover tooltip (when you hover over `map`)
- Signature help (when you type `map(`)

Rebuild: `npx tsc -b`

---

## 6. Add a Code Snippet

Open `snippets/scriptit.json` and add:

```json
"For Loop": {
    "prefix": "forl",
    "body": [
        "for ${1:i} in range(from ${2:0} to ${3:10}):",
        "    ${4:# body}",
        ";"
    ],
    "description": "Create a for loop with range"
}
```

Now typing `forl` + Tab gives you a complete for loop.

### Snippet syntax:

- `${1:placeholder}` — first tab stop with default text
- `${2:another}` — second tab stop
- `$0` — final cursor position (where cursor ends up after all tabs)
- `${1|option1,option2|}` — dropdown choice

No rebuild needed — just reload VS Code window.

---

## 7. Add a New Error Check

### Quick static check (text-only, no subprocess)

Open `src/server/server.ts`, find `validateTextDocument()`. Add after the
existing checks:

```typescript
// Warn about using deprecated 'oldFunc'
if (/\boldFunc\s*\(/.test(trimmed)) {
  diagnostics.push({
    severity: DiagnosticSeverity.Warning,
    range: {
      start: { line: i, character: codePart.indexOf("oldFunc") },
      end: { line: i, character: codePart.indexOf("oldFunc") + 7 },
    },
    message: "'oldFunc' is deprecated. Use 'newFunc' instead.",
    source: "scriptit",
  });
}
```

### New error pattern from ScriptIt output

Open `src/server/diagnostics.ts`, find `parseErrors()`. Add a new regex:

```typescript
match = trimmed.match(/MyCustomError at line (\d+): (.+)/i);
if (match) {
  diagnostics.push(
    this.createDiagnostic(
      parseInt(match[1]) - 1,
      match[2],
      sourceLines,
      DiagnosticSeverity.Error,
    ),
  );
  continue;
}
```

Rebuild: `npx tsc -b`

---

## 8. Change Bracket/Comment Rules

Open `language-configuration.json`:

```json
{
  "comments": {
    "lineComment": "#",
    "blockComment": ["-->", "<--"]
  },
  "brackets": [
    ["(", ")"],
    ["{", "}"],
    ["[", "]"]
  ],
  "autoClosingPairs": [
    { "open": "(", "close": ")" },
    { "open": "\"", "close": "\"" },
    { "open": "'", "close": "'" }
  ]
}
```

- **`comments`**: What characters toggle comments (Ctrl+/)
- **`brackets`**: What VS Code considers matching brackets
- **`autoClosingPairs`**: What gets auto-closed when you type the opening character

No rebuild needed — just reload window.

---

## 9. Rebuilding After Changes

### Grammar only (tmLanguage.json, snippets, language-configuration)

Just reload: `Ctrl+Shift+P` → "Reload Window"

### TypeScript files (server, client, notebook)

```bash
cd scriptit-vscode
npx tsc -b
```

Then reload window.

### Full rebuild + reinstall

```bash
python3 install.py --ext-only
```

### Clean rebuild from scratch

```bash
python3 install.py --clean
```

---

### Pro Tips

1. **Use the Token Inspector**: `Ctrl+Shift+P` → "Developer: Inspect Editor Tokens and Scopes" — click any character to see its scope and color.

2. **Watch mode**: Run `npx tsc -b -w` in a terminal. It auto-recompiles whenever you save a `.ts` file.

3. **Delete old colors**: If you change `SCRIPTIT_TOKEN_RULES` in `extension.ts`, old rules stay in your settings. To clear them:
   - Open Settings JSON (`Ctrl+Shift+P` → "Open Settings JSON")
   - Delete all `textMateRules` entries ending in `.scriptit`
   - Reload window
   - The extension will re-inject the updated rules

4. **Test one rule at a time**: When adding grammar patterns, add one, reload, check with the token inspector, then add the next.

---

_Happy customizing!_ 🎨
