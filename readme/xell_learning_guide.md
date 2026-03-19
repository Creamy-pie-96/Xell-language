# Xell Source Onboarding Guide (src/)

This guide is for a new maintainer who needs a practical, code-first path through the Xell core.

---

## 1) Big-picture architecture (how execution flows)

1. **Entry + mode selection**
   - `src/main.cpp` parses CLI flags (`--check`, `--convert`, REPL, kernel, terminal mode, etc.).
2. **Lexing**
   - `src/lexer/lexer.cpp` + `src/lexer/token.hpp` convert source text into tokens.
3. **Parsing**
   - `src/parser/parser.cpp` builds AST nodes from tokens.
   - AST node definitions are in `src/parser/ast.hpp`.
4. **Execution**
   - `src/interpreter/interpreter.cpp` walks the AST and evaluates statements/expressions.
   - Scope handling is via `src/interpreter/environment.hpp`.
   - Runtime values are all `XObject` in `src/interpreter/xobject.hpp` + `xobject.cpp`.
5. **Builtins + modules**
   - Builtins are registered via `src/builtins/register_all.hpp`.
   - Builtin module metadata is in `src/builtins/module_registry.hpp`.
   - `bring`/module file lookup uses `src/module/module_resolver.*`.
6. **Platform integration**
   - OS abstraction is in `src/os/*` and exposed through builtins.
7. **Dev/IDE tooling**
   - Static lint + symbol extraction: `src/analyzer/*`.
   - Debug tracing/IPC: `src/interpreter/trace_collector.hpp`, `debug_ipc.hpp`.

---

## 2) Where to start (recommended learning order)

## Phase A — orientation (first day)

1. `src/main.cpp`
2. `src/lexer/token.hpp`
3. `src/lexer/lexer.hpp` then `src/lexer/lexer.cpp`
4. `src/parser/ast.hpp`
5. `src/parser/parser.hpp` then `src/parser/parser.cpp`
6. `src/interpreter/interpreter.hpp`

Goal: know the full pipeline and where each language feature enters.

## Phase B — runtime internals (day 2–3)

1. `src/interpreter/xobject.hpp` and `src/interpreter/xobject.cpp`
2. `src/interpreter/environment.hpp`
3. `src/lib/errors/error.hpp`
4. `src/xobject/ordered_hash_table.hpp`
5. `src/xobject/gc.hpp` and `src/xobject/gc.cpp`

Goal: understand memory, value representation, mutability, hashing, and errors.

## Phase C — evaluator and language behavior (day 3–5)

1. `src/interpreter/interpreter.cpp` (core eval/exec dispatch)
2. `src/module/module_resolver.hpp` + `.cpp`
3. `src/module/xmodule.hpp`
4. `src/builtins/register_all.hpp`
5. Selected builtins for patterns:
   - `builtins_collection.hpp`
   - `builtins_string.hpp`
   - `builtins_list.hpp`
   - `builtins_type.hpp`

Goal: be able to trace any syntax from parse node to runtime behavior.

## Phase D — tooling and UX (day 5+)

1. `src/analyzer/static_analyzer.hpp`
2. `src/analyzer/symbol_collector.hpp`
3. `src/repl/*`
4. `src/interpreter/trace_collector.hpp` and `debug_ipc.hpp`

Goal: contribute to linting, IDE support, and interactive features safely.

---

## 3) File-by-file map (all files under src)

## Root

- `src/main.cpp` — CLI entrypoint; wires REPL/script/lint/kernel/convert/debug/terminal modes.

## analyzer/

- `src/analyzer/static_analyzer.hpp` — shallow semantic checks (undefined names, unreachable code, unused vars, typo hints).
- `src/analyzer/symbol_collector.hpp` — AST symbol extraction for IDE autocomplete/hover/go-to-definition.

## builtins/

- `src/builtins/builtin_registry.hpp` — core builtin function types (`BuiltinFn`, `BuiltinTable`).
- `src/builtins/module_registry.hpp` — maps builtin modules to function names and Tier1/Tier2 visibility.
- `src/builtins/register_all.hpp` — central builtin registration (flat + module-aware modes).
- `src/builtins/builtins_io.hpp` — `print`, `input`, `exit`.
- `src/builtins/builtins_math.hpp` — numeric/trig/random/number utility builtins.
- `src/builtins/builtins_type.hpp` — `type`, `typeof`, conversions (`int`, `float`, `complex`, etc.).
- `src/builtins/builtins_collection.hpp` — shared container builtins (`len`, `push`, `pop`, `range`, `contains`, etc.).
- `src/builtins/builtins_util.hpp` — utility builtins (`assert`, `format`).
- `src/builtins/builtins_os.hpp` — OS-facing builtins mapped to `src/os/*` abstractions.
- `src/builtins/builtins_hash.hpp` — hashing API (`hash`, `hash_seed`, `is_hashable`).
- `src/builtins/builtins_string.hpp` — string API + some polymorphic string/list helpers.
- `src/builtins/builtins_list.hpp` — list-focused mutating and aggregate operations.
- `src/builtins/builtins_map.hpp` — map-specific helpers (`get`, `merge`, `entries`, etc.).
- `src/builtins/builtins_bytes.hpp` — bytes conversion/indexing/slicing helpers.
- `src/builtins/builtins_generator.hpp` — generator control (`next`, `is_exhausted`, `gen_collect`).
- `src/builtins/builtins_datetime.hpp` — date/time formatting, timestamps, sleep/time helpers.
- `src/builtins/builtins_regex.hpp` — regex match/find/replace/split/groups.
- `src/builtins/builtins_fs.hpp` — large extended filesystem toolkit.
- `src/builtins/builtins_textproc.hpp` — Unix-like text processing builtins (`head`, `tail`, `grep`, etc.).
- `src/builtins/builtins_process.hpp` — process/system management builtins (`ps`, `kill`, `spawn`, etc.).
- `src/builtins/builtins_sysmon.hpp` — system monitoring and host inspection builtins.
- `src/builtins/builtins_network.hpp` — network, DNS, HTTP, transfer wrappers.
- `src/builtins/builtins_archive.hpp` — archive/compression wrappers (`zip`, `tar`, `gzip`, etc.).
- `src/builtins/builtins_json.hpp` — JSON/CSV/TOML/YAML parse/serialize implementation.
- `src/builtins/builtins_shell.hpp` — shell UX builtins (alias/history/which/man/etc.).
- `src/builtins/builtins_casting.hpp` — advanced casting and smart-casting (`~List`, `Set`, etc.).

## common/

- `src/common/access_level.hpp` — shared access enum (`PUBLIC`, `PROTECTED`, `PRIVATE`).
- `src/common/dialect_convert.hpp` — shared dialect conversion/reversion/preprocess utilities.

## hash/

- `src/hash/hash_algorithm.hpp` — hash algorithms and typed hash helpers used by runtime and hash builtins.

## interpreter/

- `src/interpreter/interpreter.hpp` — interpreter public API + execution/evaluation function declarations.
- `src/interpreter/interpreter.cpp` — core evaluator, statement execution, control flow, bring/module logic, builtins wiring.
- `src/interpreter/environment.hpp` — lexical scope chain, variable storage, immutable binding enforcement.
- `src/interpreter/xobject.hpp` — runtime value model (`XObject`, `XType`, function/instance/container payloads).
- `src/interpreter/xobject.cpp` — `XObject` behavior implementation (allocation, clone, stringify, equality, hashing, etc.).
- `src/interpreter/debug_ipc.hpp` — debugger IPC channel protocol/types (IDE ↔ interpreter).
- `src/interpreter/trace_collector.hpp` — trace/snapshot/debug event capture engine.
- `src/interpreter/shell_state.hpp` — shell-mode state (`set_e`, last exit code) shared with builtins.
- `src/interpreter/value.hpp` — currently empty placeholder header.

## lexer/

- `src/lexer/token.hpp` — `TokenType`, token-name map, and token struct.
- `src/lexer/lexer.hpp` — lexer API and scanning function declarations.
- `src/lexer/lexer.cpp` — concrete tokenizer implementation and keyword map.

## lib/errors/

- `src/lib/errors/error.hpp` — full typed error hierarchy (`TypeError`, `ParseError`, `BringError`, etc.).

## module/

- `src/module/module_resolver.hpp` — module path resolution, caching, import-cycle guard API.
- `src/module/module_resolver.cpp` — resolver implementation (`XELL_PATH`, stdlib paths, filesystem search).
- `src/module/module_metadata.hpp` — `.xell_meta` index structures (metadata model, future cache hooks).
- `src/module/xmodule.hpp` — runtime `XModule` payload and exports/submodule structure.

## os/

- `src/os/os.hpp` — cross-platform OS abstraction API used by builtins/interpreter.
- `src/os/fs.cpp` — filesystem operations implementation.
- `src/os/env.cpp` — environment variable operations implementation.
- `src/os/process.cpp` — process execution/capture implementation.
- `src/os/path_lookup.hpp` — PATH command discovery and command-string helpers.

## parser/

- `src/parser/ast.hpp` — all AST node structs for expressions/statements/features.
- `src/parser/parser.hpp` — parser API and parse method declarations.
- `src/parser/parser.cpp` — recursive-descent parser + precedence + recovery parsing.

## repl/

- `src/repl/repl.hpp` — interactive shell orchestration and command dispatch.
- `src/repl/terminal.hpp` — raw terminal I/O abstraction.
- `src/repl/line_editor.hpp` — multiline editor, cursor movement, key handling.
- `src/repl/history.hpp` — history persistence/navigation.
- `src/repl/completer.hpp` — completion over keywords, builtins, and environment symbols.

## xobject/

- `src/xobject/gc.hpp` — cycle collector design/API (`GCHeap`) for ref-count cycle cleanup.
- `src/xobject/gc.cpp` — collector traversal and sweep implementation.
- `src/xobject/ordered_hash_table.hpp` — insertion-order-preserving hash table for set/map internals.

---

## 4) How to change the language safely

## A) Add new syntax/operator/keyword

1. Add/update token in `src/lexer/token.hpp`.
2. Teach scanner in `src/lexer/lexer.cpp`.
3. Add AST node (if needed) in `src/parser/ast.hpp`.
4. Parse it in `src/parser/parser.cpp` at correct precedence/statement point.
5. Evaluate/execute it in `src/interpreter/interpreter.cpp`.
6. Add/update diagnostics in `src/analyzer/static_analyzer.hpp` if needed.

## B) Add a new builtin function

1. Pick module file in `src/builtins/builtins_*.hpp` (or create one).
2. Register function in that file’s `register...Builtins()`.
3. Wire module registration in `src/builtins/register_all.hpp`.
4. If Tier2 import semantics matter, update `src/builtins/module_registry.hpp` usage via registration path.
5. Ensure type/arity/error handling uses classes from `src/lib/errors/error.hpp`.

## C) Add a runtime type / object behavior

1. Extend `XType` and payload structs in `src/interpreter/xobject.hpp`.
2. Implement constructors/accessors/stringify/equality/hash in `src/interpreter/xobject.cpp`.
3. Update interpreter operations in `src/interpreter/interpreter.cpp`.
4. Update hashability/containers/GC traversal if needed (`src/xobject/gc.*`).

## D) Add module resolution behavior

1. Update `src/module/module_resolver.hpp/.cpp`.
2. If metadata/caching behavior changes, align with `src/module/module_metadata.hpp`.
3. Verify bring/import flow in `src/interpreter/interpreter.cpp`.

---

## 5) Common pitfalls in this codebase

1. **Layer mixing**: keep lexer/parser/interpreter responsibilities separate.
2. **Mutation vs immutability**: tuples/frozen sets/immutable bindings must be enforced consistently.
3. **Error quality**: always throw typed Xell errors with source line.
4. **Builtin visibility**: Tier1 vs Tier2 matters; registration path can silently change behavior.
5. **Performance traps**: `xobject.hpp` is hot-path runtime code; avoid unnecessary copies/allocations.
6. **GC safety**: if new container-like types are added, update GC container handling.
7. **Cross-platform assumptions**: `os/*`, network/process/sysmon builtins differ by platform.
8. **REPL raw mode**: I/O changes can break terminal behavior if canonical/raw mode transitions are mishandled.

---

## 6) Quick glossary of core symbols

- `XObject` — universal runtime value wrapper.
- `XType` — runtime type tag enum.
- `Environment` — lexical scope storage and lookup chain.
- `Interpreter` — AST executor/evaluator.
- `Program` — root AST container.
- `SliceExpr`, `CallExpr`, `IndexAccess`, `MemberAccess` — key expression AST nodes.
- `BuiltinFn`, `BuiltinTable` — builtin function registry types.
- `ModuleResolver` — module search/caching/import guard engine.
- `XModule` — runtime module payload (exports + metadata + env).
- `GCHeap` — cycle collector complementing reference counting.

---

## 7) Practical first-week checklist

- [ ] Run script mode and REPL from `main.cpp` paths.
- [ ] Trace one expression from tokenization to eval result.
- [ ] Add one tiny builtin in a non-critical module and verify registration.
- [ ] Add one parser feature experiment (small AST node) in a local branch.
- [ ] Inspect one error path from throw site to CLI output formatting.
- [ ] Trace one `bring` from import statement to module cache.

If you follow the order above, you will understand the system by architecture first, then runtime internals, then feature work paths.
