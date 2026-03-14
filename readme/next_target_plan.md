# Next Target Plan: Native Shell Command Emulation in Xell

## Goal

Extend Xell with a new library of built-in functions that mimic common shell commands (e.g., ls, cp, mv, rm, cat, etc.) natively, supporting typical options/flags and cross-platform behavior. This will allow users to write scripts using commands like ls(), cp(), etc., without relying on external shells.

---

## Steps

1. **Design API for Native Commands**
   - Define function signatures for each command (e.g., ls(path, flags), cp(src, dst, options)).
   - Decide how to handle options/flags (e.g., as keyword arguments, enums, or option structs).

2. **Implement Core Command Functions**
   - Start with most-used commands: ls, cp, mv, rm, cat, echo, pwd, touch, mkdir, rmdir.
   - Implement cross-platform logic in C++ (Windows, Linux, macOS).
   - Ensure output formatting matches shell expectations (e.g., ls -ah).

3. **Add Option/Flag Support**
   - Support common flags (e.g., ls -a, -h, cp -r, rm -f).
   - Map shell flags to function arguments.

4. **Integrate with Xell Interpreter**
   - Register new functions in builtins.
   - Ensure they are callable as ls(), cp(), etc.

5. **Testing & Documentation**
   - Write tests for each command and option.
   - Document usage, options, and cross-platform notes.

---

## Architecture

- You do NOT need to extend the whole language or parser.
- Build the new command library as a set of built-in functions on top of the existing Xell language/interpreter.
- Use C++ for implementation, register in builtins, and expose to scripts.

---

## Future Extensions

- Add more commands (find, chmod, chown, etc.)
- Support piping and redirection natively
- Provide shell-like scripting syntax (optional)

---

_Last updated: March 14, 2026_
