# Xell IDE + Terminal Performance Optimization Plan

Date: 2026-04-09
Scope: `xell-terminal` and VS Code language tooling (`Extensions/xell-vscode`)  
Goal: Reduce CPU/memory/resource usage significantly **without removing features**.

---

## 1) Current Performance Culprits (Root Causes)

### A. Render loop runs too aggressively

- Main loop renders continuously and idles at very short delay (`SDL_Delay(4)`), causing high idle wake frequency.
- PTY reader thread polls frequently (`sleep_for(2ms)`), increasing wakeups.

### B. Large full-frame allocations and copies every frame

- `LayoutManager::render()` builds full `rows x cols` cell buffers each frame.
- Editor and bottom panels also build their own full intermediate grids before compositing.
- Main loop copies IDE output into `ScreenBuffer` cell-by-cell every frame.

### C. Dirty-cell optimization is currently undermined

- Renderer clears dirty flags by writing cells back.
- `ScreenBuffer::set_cell()` forces `dirty=true` on every write, so many cells stay dirty forever.

### D. Repeated syntax work during render

- `EditorView::render()` recomputes multiline state before visible region and re-highlights visible lines each frame.
- Highlighter tokenizes lines repeatedly (`Lexer::tokenize`) even when content is unchanged.

### E. Expensive lint subprocesses in typing workflow

- IDE live lint runs `xell --check-symbols` after typing idle window.
- Language server also runs subprocess diagnostics (`xell --check`) on document changes.

### F. Tree and git operations can be heavy on refresh

- File tree recursively scans directories.
- Git panel shells out to git for status/log/diff.
- These are feature-valid but need controlled refresh policy.

---

## 2) Constraints and Requirements

1. Keep all existing user-facing features.
2. Preserve correctness first; optimize second.
3. Integrate with existing build/test/install pipelines.
4. Prefer deterministic behavior over speculative caching.

---

## 3) Optimization Phases

## Phase 1 — Immediate, low-risk wins (start here)

### 1.1 Fix dirty-flag lifecycle correctness

- Ensure dirty flags can actually be cleared and remain cleared when cells are unchanged.
- Avoid forcing dirty on every `set_cell` write.

### 1.2 Reduce redundant IDE buffer writes

- Before writing IDE output cell into `ScreenBuffer`, compare with existing cell and skip identical writes.
- Keep cursor/overlay correctness intact.

### 1.3 Lower idle wake pressure

- Increase idle delay modestly and tune PTY poll interval.
- Keep responsiveness acceptable while reducing CPU overhead.

Expected impact: significant idle CPU drop with no feature change.

---

## Phase 2 — Render-path structural optimization

### 2.1 Introduce retained frame buffers

- Reuse large cell grids instead of reallocating every frame.
- Minimize full-grid fill operations when only small regions changed.

### 2.2 Damage-region compositing

- Track changed rectangles from editor, sidebar, bottom panel, overlays.
- Composite and write only damaged regions.

### 2.3 Remove unnecessary copy hops

- Reduce intermediate full-grid copies between components.

Expected impact: lower memory churn and reduced frame time variance.

---

## Phase 3 — Syntax/highlight optimization

### 3.1 Incremental line highlight cache

- Cache highlight spans per line using versioned line hashes/revisions.

### 3.2 Persist multiline lexer state checkpoints

- Store comment/string state per logical line boundary to avoid scanning from line 0 for viewport renders.

Expected impact: faster scrolling and typing in large files.

---

## Phase 4 — Async analysis and lint pipeline

### 4.1 Offload lint/check-symbols to worker

- Make live lint fully non-blocking for UI.

### 4.2 Add cancellation/coalescing

- Cancel stale jobs on new edits.
- Keep only latest-result publication.

### 4.3 Keep output parity

- Ensure diagnostics/symbol data are semantically identical to pre-optimization behavior.

Expected impact: eliminates typing stalls while retaining diagnostics richness.

---

## Phase 5 — VS Code extension server efficiency

### 5.1 Multi-stage validation strategy

- Lightweight syntax checks on rapid edits.
- Subprocess validation on idle/save/explicit trigger.

### 5.2 Process reuse / queue policy

- Avoid subprocess churn where possible.

Expected impact: less CPU/process overhead in editor background.

---

## 4) Verification Plan

1. Build xell + xell-terminal successfully.
2. Run tests (core + terminal-relevant where available).
3. Manual checks:
   - Terminal mode responsiveness
   - IDE typing/scrolling smoothness
   - Lint/autocomplete/debug/repl behavior
   - File tree and git panel behavior
4. Install using `install.sh` and validate executable and assets.

---

## 5) Success Metrics

- Idle CPU: substantially lower than baseline.
- Typing latency: no visible stutter in medium/large files.
- No regressions in diagnostics, debug, REPL, rendering correctness.
- Build/test/install pipelines remain green.

---

## 6) Work Order (today)

1. Implement Phase 1 changes now.
2. Build and run tests.
3. Install with existing installer script.
4. Report measured deltas and next-phase tasks.
