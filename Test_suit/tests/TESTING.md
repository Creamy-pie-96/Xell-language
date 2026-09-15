# Xell Test Suite

## Overview

This test suite provides:
- **Golden tests**: small `.xell` programs with expected stdout/stderr/exit code.
- **Fuzzer**: generates random control‑flow structures to find crashes or incorrect break/continue behaviour.
- **Auto‑shrinking**: minimises any failing fuzzer case and saves it as a new golden regression test.

## Running Tests

| Command | Description |
|---------|-------------|
| `make test` | Run only golden tests (fast, ~10s) |
| `make test-full` | Golden + fuzzer (longer, for pre‑commit/CI) |
| `make fuzz` | Run only the fuzzer (saves failures) |
| `make clean` | Remove Python cache |

The test runner uses the `xell` interpreter from your `PATH`. To specify a different path:
```bash
make test XELL=/path/to/xell