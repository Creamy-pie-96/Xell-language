#pragma once

// =============================================================================
// Xell Error Hierarchy
// =============================================================================
// Every error Xell can produce lives here. All inherit from XellError, which
// inherits from std::runtime_error, so a single `catch (XellError&)` will
// catch any Xell-specific error. Each subclass carries the source line where
// the error occurred and provides a formatted `.what()` message.
// =============================================================================

#include <stdexcept>
#include <string>

namespace xell
{

    // ========================================================================
    // Base: XellError
    // ========================================================================
    // Every Xell error inherits from this. It adds a line number and a
    // standardised "[XELL ERROR] Line N — Category: message" format.
    // ========================================================================

    class XellError : public std::runtime_error
    {
    public:
        XellError(const std::string &category, const std::string &message, int line)
            : std::runtime_error(formatMessage(category, message, line)),
              line_(line), category_(category), detail_(message) {}

        int line() const noexcept { return line_; }
        const std::string &category() const noexcept { return category_; }
        const std::string &detail() const noexcept { return detail_; }

    private:
        int line_;
        std::string category_;
        std::string detail_;

        static std::string formatMessage(const std::string &category,
                                         const std::string &message, int line)
        {
            return "[XELL ERROR] Line " + std::to_string(line) +
                   " \xe2\x80\x94 " + category + ": " + message;
        }
    };

    // ========================================================================
    // 1. Lexer errors — problems during tokenization
    // ========================================================================

    /// Unterminated string literal, malformed number, etc.
    class LexerError : public XellError
    {
    public:
        LexerError(const std::string &message, int line)
            : XellError("LexerError", message, line) {}
    };

    // ========================================================================
    // 2. Parse errors — problems during parsing
    // ========================================================================

    /// Unexpected token, missing delimiter, malformed statement.
    class ParseError : public XellError
    {
    public:
        ParseError(const std::string &message, int line)
            : XellError("ParseError", message, line) {}
    };

    // ========================================================================
    // 3. Runtime errors — problems during interpretation / execution
    // ========================================================================

    // ---- 3a. Type errors ----------------------------------------------------

    /// Wrong type used in an operation (e.g. adding a list and a number).
    class TypeError : public XellError
    {
    public:
        TypeError(const std::string &message, int line)
            : XellError("TypeError", message, line) {}
    };

    // ---- 3b. Name resolution ------------------------------------------------

    /// Variable used before being assigned.
    class UndefinedVariableError : public XellError
    {
    public:
        UndefinedVariableError(const std::string &name, int line)
            : XellError("UndefinedVariable",
                        "'" + name + "' is not defined", line) {}
    };

    /// Function called that doesn't exist.
    class UndefinedFunctionError : public XellError
    {
    public:
        UndefinedFunctionError(const std::string &name, int line)
            : XellError("UndefinedFunction",
                        "'" + name + "' is not defined", line) {}
    };

    // ---- 3c. Collection access ----------------------------------------------

    /// List index out of range.
    class IndexError : public XellError
    {
    public:
        IndexError(const std::string &message, int line)
            : XellError("IndexError", message, line) {}
    };

    /// Map key doesn't exist.
    class KeyError : public XellError
    {
    public:
        KeyError(const std::string &key, int line)
            : XellError("KeyError",
                        "'" + key + "' not found in map", line) {}
    };

    // ---- 3d. Arithmetic -----------------------------------------------------

    /// Division (or modulo) by zero.
    class DivisionByZeroError : public XellError
    {
    public:
        DivisionByZeroError(int line)
            : XellError("DivisionByZero", "division by zero", line) {}
    };

    /// Numeric overflow / underflow (future-proofing).
    class OverflowError : public XellError
    {
    public:
        OverflowError(const std::string &message, int line)
            : XellError("OverflowError", message, line) {}
    };

    // ---- 3e. Function call errors -------------------------------------------

    /// Wrong number of arguments passed to a function.
    class ArityError : public XellError
    {
    public:
        ArityError(const std::string &fnName, int expected, int got, int line)
            : XellError("ArityError",
                        "'" + fnName + "' expects " + std::to_string(expected) +
                            " arg(s), got " + std::to_string(got),
                        line) {}
    };

    // ---- 3f. Import / module errors -----------------------------------------

    /// File not found, name missing in file, or circular bring.
    class BringError : public XellError
    {
    public:
        BringError(const std::string &message, int line)
            : XellError("BringError", message, line) {}
    };

    // ---- 3g. File-system / IO errors ----------------------------------------

    /// Built-in file operation on a missing or inaccessible path.
    class FileNotFoundError : public XellError
    {
    public:
        FileNotFoundError(const std::string &path, int line)
            : XellError("FileNotFound",
                        "'" + path + "' does not exist", line) {}
    };

    /// General IO failure (permission denied, disk full, etc.).
    class IOError : public XellError
    {
    public:
        IOError(const std::string &message, int line)
            : XellError("IOError", message, line) {}
    };

    // ---- 3h. Process execution errors ---------------------------------------

    /// External command failed (non-zero exit code, command not found, etc.).
    class ProcessError : public XellError
    {
    public:
        ProcessError(const std::string &message, int line)
            : XellError("ProcessError", message, line) {}
    };

    // ---- 3i. Conversion errors ----------------------------------------------

    /// Failed to convert a value to the expected type (e.g. to_number("abc")).
    class ConversionError : public XellError
    {
    public:
        ConversionError(const std::string &message, int line)
            : XellError("ConversionError", message, line) {}
    };

    // ---- 3j. Environment errors ---------------------------------------------

    /// Environment variable not found.
    class EnvError : public XellError
    {
    public:
        EnvError(const std::string &name, int line)
            : XellError("EnvError",
                        "environment variable '" + name + "' not found", line) {}
    };

    // ---- 3k. Recursion errors -----------------------------------------------

    /// Maximum call depth exceeded (stack overflow protection).
    class RecursionError : public XellError
    {
    public:
        RecursionError(int depth, int line)
            : XellError("RecursionError",
                        "maximum recursion depth (" + std::to_string(depth) + ") exceeded",
                        line) {}
    };

    // ---- 3l. Assertion errors -----------------------------------------------

    /// User-level assert statement failed (future `assert` keyword).
    class AssertionError : public XellError
    {
    public:
        AssertionError(const std::string &message, int line)
            : XellError("AssertionError", message, line) {}
    };

    // ---- 3m. Hash errors ----------------------------------------------------

    /// Attempt to hash a mutable (non-hashable) type, or other hash failure.
    class HashError : public XellError
    {
    public:
        HashError(const std::string &message, int line)
            : XellError("HashError", message, line) {}
    };

    // ---- 3n. Immutability errors --------------------------------------------

    /// Attempt to mutate an immutable object (e.g. tuple).
    class ImmutabilityError : public XellError
    {
    public:
        ImmutabilityError(const std::string &message, int line)
            : XellError("ImmutabilityError", message, line) {}
    };

    // ---- 3o. Not-implemented placeholder ------------------------------------

    /// Feature or built-in that is stubbed but not yet implemented.
    class NotImplementedError : public XellError
    {
    public:
        NotImplementedError(const std::string &feature, int line)
            : XellError("NotImplementedError",
                        "'" + feature + "' is not yet implemented", line) {}
    };

    // ---- 3n. Command failure (set_e mode) -----------------------------------

    /// Generic runtime error for situations that don't fit other categories.
    class RuntimeError : public XellError
    {
    public:
        RuntimeError(const std::string &message, int line)
            : XellError("RuntimeError", message, line) {}
    };

    /// Raised when a command exits non-zero and set_e() is active.
    class CommandFailedError : public XellError
    {
    public:
        CommandFailedError(const std::string &command, int exitCode, int line)
            : XellError("CommandFailed",
                        "command exited with code " + std::to_string(exitCode) +
                            (command.empty() ? "" : ": " + command),
                        line) {}
    };

} // namespace xell
