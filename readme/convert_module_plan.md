# @convert + Module System Fix — Implementation Plan

## Problem

Three places read `.xel` source files and feed them to the lexer **without** running `@convert` preprocessing:

| #   | Location                                      | Triggered by                                    |
| --- | --------------------------------------------- | ----------------------------------------------- |
| 1   | `main.cpp::makeModule()`                      | `xell --make_module`                            |
| 2   | `interpreter.cpp::executeModuleFile()`        | Module resolver finding modules in `.xel` files |
| 3   | `interpreter.cpp::execBring()::bringFromFile` | `bring X from "file.xel"`                       |

Dialect files hitting these paths crash because the lexer sees custom keywords.

## Cross-Dialect Safety

Works naturally: `@convert` only translates keywords/builtins, NOT identifiers. Each file resolves its own `.xesy` independently. After preprocessing, all files are canonical Xell.

## Implementation

### Phase 1 — Shared Header: `src/common/dialect_convert.hpp`

Extract from `main.cpp` into a reusable header:

- `ConvertDirective` struct
- `parseXesyFile()`, `invertMapping()`, `replaceWords()`
- `parseConvertDirective()`, `stripConvertLine()`
- `findXesyInDirectory()`, `resolveXesyPath()`
- `applyConvertIfNeeded()`

`main.cpp` then `#include "common/dialect_convert.hpp"` and removes local copies.

### Phase 2 — Fix `--make_module`

In `makeModule()`, after `readFile(filePath)`:

```cpp
auto directive = dialect::parseConvertDirective(source);
std::string convertXesy = "";
if (directive.found) {
    convertXesy = dialect::resolveXesyPath(filePath, directive.mappingPath);
}
source = dialect::applyConvertIfNeeded(source, filePath);
```

Add `"convert"` field to `.xell_meta`:

```json
"math_lib": { "file": "x.xel", "hash": "...", "convert": "bangla.xesy", ... }
```

### Phase 3 — Fix `bring` in interpreter

Include `common/dialect_convert.hpp` in `interpreter.hpp`.

- **`executeModuleFile()`** — after reading source, before lexing:
  ```cpp
  source = dialect::applyConvertIfNeeded(source, filePath);
  ```
- **`bringFromFile()`** — after `source = ss.str()`:
  ```cpp
  source = dialect::applyConvertIfNeeded(source, resolvedPath);
  ```

### Phase 4 — Fix Dialect Syntax Highlighting

The tmLanguage grammar has fine-grained scopes per keyword category:

- `keyword.declaration.function.xell` (fn)
- `keyword.control.conditional.xell` (if, elif, else)
- `keyword.control.loop.xell` (for, while, loop)
- etc.

But the semantic tokens provider lumps ALL dialect keywords into one `keyword` type → one color.

**Fix**: Expand semantic token types to match tmLanguage categories:

| Token Type      | Scope                                 | Keywords                                        |
| --------------- | ------------------------------------- | ----------------------------------------------- |
| xellFnDecl      | keyword.declaration.function.xell     | fn                                              |
| xellReturn      | keyword.control.return.xell           | give                                            |
| xellConditional | keyword.control.conditional.xell      | if, elif, else, incase                          |
| xellLoop        | keyword.control.loop.xell             | for, while, loop, in                            |
| xellControlFlow | keyword.control.flow.xell             | break, continue                                 |
| xellTryCatch    | keyword.control.trycatch.xell         | try, catch, finally                             |
| xellBinding     | keyword.other.binding.xell            | let, be, immutable                              |
| xellImport      | keyword.control.import.xell           | bring, from, as                                 |
| xellModule      | keyword.control.module.xell           | module, export, requires                        |
| xellOopDecl     | keyword.declaration.type.xell         | class, struct, enum, interface, abstract, mixin |
| xellOopMod      | keyword.other.modifier.xell           | inherits, implements, with                      |
| xellAccess      | storage.modifier.xell                 | private, protected, public, static              |
| xellGenerator   | keyword.control.yield.xell            | yield                                           |
| xellAsync       | keyword.control.async.xell            | async, await                                    |
| xellLogical     | keyword.operator.logical.xell         | and, or, not                                    |
| xellComparison  | keyword.operator.comparison.word.xell | is, eq, ne, gt, lt, ge, le                      |
| xellConstant    | constant.language.boolean.true.xell   | true, false, none                               |
| xellBuiltin     | support.function.builtin.xell         | all builtins                                    |
| xellSpecial     | keyword.other.special.xell            | of                                              |

Files: `server.ts` (token legend + provider), `package.json` (semanticTokenScopes).
