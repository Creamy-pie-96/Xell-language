# @convert Dialect System — Implementation Plan

## Overview

Allow users to write Xell code in their own dialect (custom keyword names) using a `.xesy` mapping file. The system handles conversion between custom and canonical syntax transparently.

---

## Architecture

```
C++ (main.cpp)              LSP Server (server.ts)            Extension Client (extension.ts)
────────────────            ─────────────────────             ──────────────────────────────
--convert                   dialectMap.ts (new)               .xesy file watcher
--revert             ◄──►   per-file mapping cache       ◄──► semantic token color rules
--gen_xesy                  translation layer in              @convert decorator support
auto-detect in              completions/hover/
executeFile &               diagnostics/signature
lintSource
```

---

## .xesy File Format

JSON file mapping **canonical → custom** keywords:

```json
{
  "_meta": {
    "dialect_name": "Bengali",
    "author": "User",
    "xell_version": "0.1.0"
  },
  "if": "jodi",
  "else": "nahole",
  "for": "ghuro",
  "fn": "kaj",
  "give": "ferot",
  "while": "jotokhon",
  "print": "dekhao"
}
```

- Keys = canonical Xell keywords/builtins
- Values = single-word custom replacements
- Empty string values or omitted keys = keep canonical
- `_meta` key is reserved for metadata (ignored during conversion)

---

## @convert Decorator

Placed at the very first non-blank line of a `.xel` file:

```
@convert "dialect.xesy"
```

- The argument is a **string literal** — path to the `.xesy` file
- Path is resolved relative to the code file's directory
- If `@convert` with no argument → search for `*.xesy` in the same directory
- `path` is **NOT** used as a keyword to avoid restricting users

---

## Phase 1 — C++ main.cpp additions

### 1a. `convertFile(filePath, mappingPath)` — `xell --convert <file> [map.xesy]`

1. Load `.xesy` JSON → build `canonical_to_custom` map
2. Invert to `custom_to_canonical` map
3. Read source file
4. For each token (whole-word boundary): if in `custom_to_canonical` → replace with canonical
5. Strip the `@convert ...` line
6. Write back in-place
7. Store original mapping path in `.xesy.revert` sidecar for `--revert`

### 1b. `revertFile(filePath, mappingPath)` — `xell --revert <file> [map.xesy]`

1. Load `.xesy` forward map (`canonical_to_custom`)
2. Read canonical source file
3. For each token: if in `canonical_to_custom` → replace with custom
4. Re-inject `@convert "mappingPath"` as the first line
5. Write back in-place

### 1c. `genXesy(outputPath)` — `xell --gen_xesy [output.xesy]`

1. Extract all canonical keywords from `keywordMap()` in lexer
2. Extract all builtin function names
3. Output a template `.xesy` with empty values for user to fill in

### 1d. Auto-detect `@convert` in `executeFile` & `lintSource`

Before lexing, scan first 5 non-blank lines for:

```
@convert "path/to/mapping.xesy"
@convert           (no argument → auto-find .xesy in same dir)
```

- Parse the decorator → extract path
- Resolve `.xesy` file (explicit or auto-detect `*.xesy` in same directory)
- No `.xesy` found → clear error + exit
- Convert source **in-memory only** (no disk write)
- Strip `@convert` line from in-memory copy
- Feed converted source to lexer/parser

### CLI additions

```
xell --convert <file> [map.xesy]   Convert dialect file → canonical in-place
xell --revert  <file> [map.xesy]   Restore dialect from canonical file
xell --gen_xesy [output.xesy]      Generate a template mapping file
```

---

## Phase 2 — New LSP module: `dialectMap.ts`

### Interface

```typescript
interface DialectInfo {
  customToCanonical: Record<string, string>; // "jodi" → "if"
  canonicalToCustom: Record<string, string>; // "if" → "jodi"
  mappingPath: string; // absolute path to .xesy
  mtimeMs: number; // last modified time for cache invalidation
}
```

### Key functions

| Function                         | Purpose                                                                                        |
| -------------------------------- | ---------------------------------------------------------------------------------------------- |
| `extractConvertDirective(text)`  | Scan first 5 lines for `@convert "..."`. Return path string, `""` (no arg), or `null` (absent) |
| `resolveXesyPath(fileUri, path)` | Resolve relative path or glob `*.xesy` in same dir                                             |
| `loadXesyFile(xesyPath)`         | Read JSON, build forward+inverse maps, return `DialectInfo`                                    |
| `getDialect(fileUri, text)`      | Cached lookup — returns `DialectInfo \| null`                                                  |
| `translate(token, dialect)`      | Custom → canonical; passthrough if not mapped                                                  |
| `getCustomKeywords(dialect)`     | Return all custom word names                                                                   |
| `invalidateDialect(fileUri)`     | Remove from cache (called on file close/change)                                                |

### Cache strategy

- `dialectCache: Map<fileUri, DialectInfo | null>` — `null` means "checked, no @convert"
- On `documents.onDidChangeContent` → re-check first 5 lines; if decorator unchanged → skip
- On `documents.onDidClose` → delete from cache
- `.xesy` file changes → evict all cache entries pointing to that `.xesy`
- `watchedXesy: Map<xesyPath, Set<fileUri>>` — tracks which files use which mapping

---

## Phase 3 — LSP `server.ts` modifications

### 3a. Diagnostics (`validateTextDocument`)

- Existing regex patterns for `if|elif|while` and `fn` become dialect-aware
- Build dynamic patterns using both canonical and custom keyword names
- `xell --check` subprocess handles `@convert` natively (Phase 1d)

### 3b. Completion (`onCompletion`)

- For dialect files: map canonical completion labels → custom equivalents
- Detail shows `(→ canonical: if)` for custom keywords
- User-defined identifiers still extracted normally

### 3c. Hover (`onHover`)

- Translate hovered word custom → canonical before lookup
- Show dialect alias info: `**jodi** *(dialect alias for \`if\`)\*`

### 3d. Signature Help (`onSignatureHelp`)

- Translate function name before HOVER_INFO lookup

### 3e. `extractUserIdentifiers`

- Skip both canonical AND custom keyword names

---

## Phase 4 — Extension client `extension.ts` changes

### 4a. Semantic Token Provider (new — for custom keyword coloring)

Register semantic tokens provider in the LSP server:

- Token types: `keyword`, `function`, `type`, `variable`, `namespace`
- Token modifiers: `declaration`, `control`, `loop`, `import`

When a file has `@convert`:

- Walk all tokens in the document
- For each word matching a custom keyword → emit semantic token with the type of its canonical equivalent
- This gives custom keywords the same color as their canonical counterparts

### 4b. `.xesy` file watcher

Add `**/*.xesy` to `fileEvents` in `clientOptions.synchronize`.

### 4c. `package.json` additions

- Add `.xesy` file association with JSON language
- Add `@convert` snippet

### 4d. Token color sync

Inject `semanticTokenColorCustomizations` matching the existing `textMateRules` colors so semantic-highlighted dialect keywords get identical styling.

---

## Phase 5 — `gen_xell_grammar.py` additions

### 5a. `--gen_xesy` mode

Add a new function `build_xesy_template(keywords, builtins)`:

- Reads all keywords from `keywordMap()`
- Reads all builtins from builtin headers
- Outputs a template `.xesy` with `_meta` section + empty values

### 5b. Snippet generation

Add `@convert` snippet to the existing snippet generator.

### 5c. `language_data.json` extension

Add a `convertDecorator` section with the regex pattern and metadata so the LSP can reference it.

---

## Phase 6 — Integration test

1. Run `xell --gen_xesy test_dialect.xesy` → verify template
2. Fill in a few mappings → create test dialect file with `@convert`
3. Run `xell dialect_test.xel` → verify auto-detection + execution
4. Run `xell --convert dialect_test.xel` → verify in-place conversion
5. Run `xell --revert dialect_test.xel` → verify restoration
6. Open in VS Code → verify linting, completion, hover, coloring

---

## Data Flow

```
File open / keystroke
      │
      ▼
 documents.onDidChangeContent (debounced 300ms)
      │
      ├─► getDialect(uri, text)
      │        └─ scan first 5 lines for @convert
      │        └─ cache hit? return cached DialectInfo
      │        └─ cache miss/stale? load .xesy, invert, cache
      │
      ├─► validateTextDocument (with dialect-aware regex patterns)
      │        └─ passes raw text to `xell --check` (C++ handles @convert natively)
      │
      ├─► onCompletion (with custom keyword labels)
      │
      ├─► onHover (custom → canonical translation before lookup)
      │
      └─► semanticTokens (custom keywords → typed tokens → correct colors)
```

---

## What stays untouched

- `xell.tmLanguage.json` — canonical files unchanged; custom keywords colored via semantic tokens
- `language_data.json` — still source of truth for canonical keywords
- `HOVER_INFO`, `LANG_DATA` — accessed via canonical names after translation
- Notebook support — dialect conversion happens before execution, transparent to notebook
