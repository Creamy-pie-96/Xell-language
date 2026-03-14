# Xell Scripting Commands (Cross-Platform)

This document lists the scripting-style commands supported natively by Xell for cross-platform scripting (Windows, Linux, macOS). These commands are implemented in C++ and do not rely on external shells like bash.

---

## File and Directory Operations

- `make_dir` — Create directory (recursive)
- `remove_path` — Remove file or directory
- `copy_path` — Copy file or directory
- `move_path` — Move file or directory
- `write_file` — Write string to file
- `append_file` — Append string to file
- `change_dir` — Change working directory

## Environment Variable Management

- `env_set` — Set environment variable
- `env_unset` — Unset environment variable

## Process Management

- `run` — Execute external command (cross-platform)
- `run_capture` — Execute command and capture output

## Text Processing (Native, Not Shell)

- `head` — Output first lines of file
- `tail` — Output last lines of file
- `grep` — Search for patterns in text
- `cut` — Extract columns from text
- `sort` — Sort lines
- `uniq` — Remove duplicate lines
- `wc` — Count words/lines
- `sed` — Stream edit text
- `awk` — Pattern scanning and processing

---

All commands above are implemented natively and work on Windows, Linux, and macOS. No shell hacks or external dependencies required.

_Last updated: March 14, 2026_
