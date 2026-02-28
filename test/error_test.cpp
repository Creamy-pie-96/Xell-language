// =============================================================================
// Xell Error Tests
// =============================================================================
// Verifies that every error type in src/lib/errors/error.hpp:
//   1. Can be constructed
//   2. Carries the correct line number
//   3. Carries the correct category
//   4. Produces a properly formatted what() message
//   5. Can be caught by its own type AND by XellError AND by std::runtime_error
//   6. Is actually thrown by the lexer / parser in the right situations
// =============================================================================

#include "../src/lib/errors/error.hpp"
#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <functional>
#include <cstring>

using namespace xell;

// ---- Minimal test framework ------------------------------------------------

static int g_passed = 0;
static int g_failed = 0;

#define XASSERT(cond)                                                      \
    do                                                                     \
    {                                                                      \
        if (!(cond))                                                       \
        {                                                                  \
            std::ostringstream os;                                         \
            os << "Assertion failed: " #cond " (line " << __LINE__ << ")"; \
            throw std::runtime_error(os.str());                            \
        }                                                                  \
    } while (0)

#define XASSERT_EQ(a, b)                                 \
    do                                                   \
    {                                                    \
        if ((a) != (b))                                  \
        {                                                \
            std::ostringstream os;                       \
            os << "Expected [" << (a) << "] == [" << (b) \
               << "] (line " << __LINE__ << ")";         \
            throw std::runtime_error(os.str());          \
        }                                                \
    } while (0)

static void runTest(const std::string &name, std::function<void()> fn)
{
    try
    {
        fn();
        std::cout << "  PASS: " << name << "\n";
        g_passed++;
    }
    catch (const std::exception &e)
    {
        std::cout << "  FAIL: " << name << "\n        " << e.what() << "\n";
        g_failed++;
    }
}

// ---- Helper: check that a string contains a substring ----------------------

static bool contains(const std::string &haystack, const std::string &needle)
{
    return haystack.find(needle) != std::string::npos;
}

// ============================================================================
// Section 1: Construction & metadata of every error type
// ============================================================================

static void testErrorConstruction()
{
    std::cout << "\n===== Error Construction & Metadata =====\n";

    // -- XellError (base) --
    runTest("XellError: construction", []()
            {
        XellError err("TestCategory", "something broke", 42);
        XASSERT_EQ(err.line(), 42);
        XASSERT_EQ(err.category(), std::string("TestCategory"));
        XASSERT_EQ(err.detail(), std::string("something broke"));
        XASSERT(contains(err.what(), "[XELL ERROR]"));
        XASSERT(contains(err.what(), "Line 42"));
        XASSERT(contains(err.what(), "TestCategory"));
        XASSERT(contains(err.what(), "something broke")); });

    // -- LexerError --
    runTest("LexerError: construction", []()
            {
        LexerError err("Unterminated string literal", 5);
        XASSERT_EQ(err.line(), 5);
        XASSERT_EQ(err.category(), std::string("LexerError"));
        XASSERT(contains(err.what(), "LexerError"));
        XASSERT(contains(err.what(), "Unterminated string")); });

    // -- ParseError --
    runTest("ParseError: construction", []()
            {
        ParseError err("Expected ':' after if condition", 10);
        XASSERT_EQ(err.line(), 10);
        XASSERT_EQ(err.category(), std::string("ParseError"));
        XASSERT(contains(err.what(), "ParseError"));
        XASSERT(contains(err.what(), "Expected ':'")); });

    // -- TypeError --
    runTest("TypeError: construction", []()
            {
        TypeError err("cannot add number and list", 14);
        XASSERT_EQ(err.line(), 14);
        XASSERT_EQ(err.category(), std::string("TypeError"));
        XASSERT(contains(err.what(), "TypeError"));
        XASSERT(contains(err.what(), "cannot add number and list")); });

    // -- UndefinedVariableError --
    runTest("UndefinedVariableError: construction", []()
            {
        UndefinedVariableError err("projct", 7);
        XASSERT_EQ(err.line(), 7);
        XASSERT_EQ(err.category(), std::string("UndefinedVariable"));
        XASSERT(contains(err.what(), "'projct' is not defined")); });

    // -- UndefinedFunctionError --
    runTest("UndefinedFunctionError: construction", []()
            {
        UndefinedFunctionError err("deploi", 22);
        XASSERT_EQ(err.line(), 22);
        XASSERT_EQ(err.category(), std::string("UndefinedFunction"));
        XASSERT(contains(err.what(), "'deploi' is not defined")); });

    // -- IndexError --
    runTest("IndexError: construction", []()
            {
        IndexError err("index 5 out of range for list of size 3", 15);
        XASSERT_EQ(err.line(), 15);
        XASSERT_EQ(err.category(), std::string("IndexError"));
        XASSERT(contains(err.what(), "index 5 out of range")); });

    // -- KeyError --
    runTest("KeyError: construction", []()
            {
        KeyError err("hostt", 18);
        XASSERT_EQ(err.line(), 18);
        XASSERT_EQ(err.category(), std::string("KeyError"));
        XASSERT(contains(err.what(), "'hostt' not found in map")); });

    // -- DivisionByZeroError --
    runTest("DivisionByZeroError: construction", []()
            {
        DivisionByZeroError err(9);
        XASSERT_EQ(err.line(), 9);
        XASSERT_EQ(err.category(), std::string("DivisionByZero"));
        XASSERT(contains(err.what(), "division by zero")); });

    // -- OverflowError --
    runTest("OverflowError: construction", []()
            {
        OverflowError err("result exceeds maximum value", 30);
        XASSERT_EQ(err.line(), 30);
        XASSERT_EQ(err.category(), std::string("OverflowError"));
        XASSERT(contains(err.what(), "result exceeds maximum")); });

    // -- ArityError --
    runTest("ArityError: construction", []()
            {
        ArityError err("setup", 2, 1, 22);
        XASSERT_EQ(err.line(), 22);
        XASSERT_EQ(err.category(), std::string("ArityError"));
        XASSERT(contains(err.what(), "'setup' expects 2 arg(s), got 1")); });

    // -- BringError --
    runTest("BringError: construction", []()
            {
        BringError err("'deploy' not found in \"./helpers.xel\"", 3);
        XASSERT_EQ(err.line(), 3);
        XASSERT_EQ(err.category(), std::string("BringError"));
        XASSERT(contains(err.what(), "'deploy' not found")); });

    // -- FileNotFoundError --
    runTest("FileNotFoundError: construction", []()
            {
        FileNotFoundError err("/tmp/missing.txt", 11);
        XASSERT_EQ(err.line(), 11);
        XASSERT_EQ(err.category(), std::string("FileNotFound"));
        XASSERT(contains(err.what(), "'/tmp/missing.txt' does not exist")); });

    // -- IOError --
    runTest("IOError: construction", []()
            {
        IOError err("permission denied on '/etc/shadow'", 20);
        XASSERT_EQ(err.line(), 20);
        XASSERT_EQ(err.category(), std::string("IOError"));
        XASSERT(contains(err.what(), "permission denied")); });

    // -- ProcessError --
    runTest("ProcessError: construction", []()
            {
        ProcessError err("command 'npm' not found", 4);
        XASSERT_EQ(err.line(), 4);
        XASSERT_EQ(err.category(), std::string("ProcessError"));
        XASSERT(contains(err.what(), "command 'npm' not found")); });

    // -- ConversionError --
    runTest("ConversionError: construction", []()
            {
        ConversionError err("cannot convert 'abc' to number", 6);
        XASSERT_EQ(err.line(), 6);
        XASSERT_EQ(err.category(), std::string("ConversionError"));
        XASSERT(contains(err.what(), "cannot convert 'abc' to number")); });

    // -- EnvError --
    runTest("EnvError: construction", []()
            {
        EnvError err("MISSING_VAR", 8);
        XASSERT_EQ(err.line(), 8);
        XASSERT_EQ(err.category(), std::string("EnvError"));
        XASSERT(contains(err.what(), "'MISSING_VAR' not found")); });

    // -- RecursionError --
    runTest("RecursionError: construction", []()
            {
        RecursionError err(1000, 12);
        XASSERT_EQ(err.line(), 12);
        XASSERT_EQ(err.category(), std::string("RecursionError"));
        XASSERT(contains(err.what(), "maximum recursion depth (1000) exceeded")); });

    // -- AssertionError --
    runTest("AssertionError: construction", []()
            {
        AssertionError err("expected true but got false", 25);
        XASSERT_EQ(err.line(), 25);
        XASSERT_EQ(err.category(), std::string("AssertionError"));
        XASSERT(contains(err.what(), "expected true but got false")); });

    // -- NotImplementedError --
    runTest("NotImplementedError: construction", []()
            {
        NotImplementedError err("pattern matching", 50);
        XASSERT_EQ(err.line(), 50);
        XASSERT_EQ(err.category(), std::string("NotImplementedError"));
        XASSERT(contains(err.what(), "'pattern matching' is not yet implemented")); });
}

// ============================================================================
// Section 2: Catch-by-hierarchy (every error is catchable as its own type,
//            as XellError, and as std::runtime_error)
// ============================================================================

static void testCatchHierarchy()
{
    std::cout << "\n===== Catch Hierarchy =====\n";

    runTest("LexerError caught as LexerError", []()
            {
        bool caught = false;
        try { throw LexerError("test", 1); }
        catch (const LexerError&) { caught = true; }
        XASSERT(caught); });

    runTest("LexerError caught as XellError", []()
            {
        bool caught = false;
        try { throw LexerError("test", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("LexerError caught as std::runtime_error", []()
            {
        bool caught = false;
        try { throw LexerError("test", 1); }
        catch (const std::runtime_error&) { caught = true; }
        XASSERT(caught); });

    runTest("ParseError caught as ParseError", []()
            {
        bool caught = false;
        try { throw ParseError("test", 1); }
        catch (const ParseError&) { caught = true; }
        XASSERT(caught); });

    runTest("ParseError caught as XellError", []()
            {
        bool caught = false;
        try { throw ParseError("test", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("TypeError caught as XellError", []()
            {
        bool caught = false;
        try { throw TypeError("test", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("UndefinedVariableError caught as XellError", []()
            {
        bool caught = false;
        try { throw UndefinedVariableError("x", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("UndefinedFunctionError caught as XellError", []()
            {
        bool caught = false;
        try { throw UndefinedFunctionError("foo", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("IndexError caught as XellError", []()
            {
        bool caught = false;
        try { throw IndexError("out of range", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("KeyError caught as XellError", []()
            {
        bool caught = false;
        try { throw KeyError("host", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("DivisionByZeroError caught as XellError", []()
            {
        bool caught = false;
        try { throw DivisionByZeroError(1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("ArityError caught as XellError", []()
            {
        bool caught = false;
        try { throw ArityError("f", 2, 0, 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("BringError caught as XellError", []()
            {
        bool caught = false;
        try { throw BringError("test", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("FileNotFoundError caught as XellError", []()
            {
        bool caught = false;
        try { throw FileNotFoundError("f.txt", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("IOError caught as XellError", []()
            {
        bool caught = false;
        try { throw IOError("test", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("ProcessError caught as XellError", []()
            {
        bool caught = false;
        try { throw ProcessError("test", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("ConversionError caught as XellError", []()
            {
        bool caught = false;
        try { throw ConversionError("test", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("EnvError caught as XellError", []()
            {
        bool caught = false;
        try { throw EnvError("X", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("RecursionError caught as XellError", []()
            {
        bool caught = false;
        try { throw RecursionError(500, 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("AssertionError caught as XellError", []()
            {
        bool caught = false;
        try { throw AssertionError("fail", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("NotImplementedError caught as XellError", []()
            {
        bool caught = false;
        try { throw NotImplementedError("x", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("OverflowError caught as XellError", []()
            {
        bool caught = false;
        try { throw OverflowError("too big", 1); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });
}

// ============================================================================
// Section 3: Lexer throws LexerError in the right situations
// ============================================================================

static void testLexerErrors()
{
    std::cout << "\n===== Lexer Error Integration =====\n";

    runTest("lexer: unterminated string → LexerError", []()
            {
        Lexer lex("\"hello");
        bool caught = false;
        try { lex.tokenize(); }
        catch (const LexerError& e) {
            caught = true;
            XASSERT_EQ(e.line(), 1);
            XASSERT(contains(e.what(), "Unterminated string"));
        }
        XASSERT(caught); });

    runTest("lexer: unterminated string on line 3 → correct line", []()
            {
        Lexer lex("x = 10\ny = 20\nz = \"oops");
        bool caught = false;
        try { lex.tokenize(); }
        catch (const LexerError& e) {
            caught = true;
            XASSERT_EQ(e.line(), 3);
        }
        XASSERT(caught); });

    runTest("lexer: unexpected character → LexerError", []()
            {
        Lexer lex("x = 10 @ y");
        bool caught = false;
        try { lex.tokenize(); }
        catch (const LexerError& e) {
            caught = true;
            XASSERT(contains(e.what(), "@"));
            XASSERT_EQ(e.line(), 1);
        }
        XASSERT(caught); });

    runTest("lexer: unexpected character $ → LexerError", []()
            {
        Lexer lex("$var");
        bool caught = false;
        try { lex.tokenize(); }
        catch (const LexerError& e) {
            caught = true;
            XASSERT(contains(e.what(), "$"));
        }
        XASSERT(caught); });

    runTest("lexer: unexpected character ~ → LexerError", []()
            {
        Lexer lex("a ~ b");
        bool caught = false;
        try { lex.tokenize(); }
        catch (const LexerError& e) {
            caught = true;
            XASSERT(contains(e.what(), "~"));
        }
        XASSERT(caught); });

    runTest("lexer: LexerError is also catchable as XellError", []()
            {
        Lexer lex("\"unterminated");
        bool caught = false;
        try { lex.tokenize(); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });
}

// ============================================================================
// Section 4: Parser throws ParseError in the right situations
// ============================================================================

static void testParserErrors()
{
    std::cout << "\n===== Parser Error Integration =====\n";

    auto parseSource = [](const std::string &src)
    {
        Lexer lex(src);
        auto tokens = lex.tokenize();
        Parser parser(tokens);
        return parser.parse();
    };

    runTest("parser: missing ':' after if → ParseError", [&]()
            {
        bool caught = false;
        try { parseSource("if x == 1\n    print \"yes\"\n;"); }
        catch (const ParseError& e) {
            caught = true;
            XASSERT(contains(e.what(), "ParseError"));
            XASSERT(contains(e.what(), "Expected ':'"));
        }
        XASSERT(caught); });

    runTest("parser: missing ';' to close if → ParseError", [&]()
            {
        bool caught = false;
        try { parseSource("if x == 1 :\n    print \"yes\""); }
        catch (const ParseError& e) {
            caught = true;
            XASSERT(contains(e.what(), "Expected ';'"));
        }
        XASSERT(caught); });

    runTest("parser: missing ')' after args → ParseError", [&]()
            {
        bool caught = false;
        try { parseSource("foo(1, 2"); }
        catch (const ParseError& e) {
            caught = true;
            XASSERT(contains(e.what(), "Expected ')'"));
        }
        XASSERT(caught); });

    runTest("parser: missing ']' after list → ParseError", [&]()
            {
        bool caught = false;
        try { parseSource("x = [1, 2, 3"); }
        catch (const ParseError& e) {
            caught = true;
            XASSERT(contains(e.what(), "Expected ']'"));
        }
        XASSERT(caught); });

    runTest("parser: missing '}' after map → ParseError", [&]()
            {
        bool caught = false;
        try { parseSource("x = { a: 1, b: 2"); }
        catch (const ParseError& e) {
            caught = true;
            XASSERT(contains(e.what(), "Expected '}'"));
        }
        XASSERT(caught); });

    runTest("parser: missing '(' after fn name → ParseError", [&]()
            {
        bool caught = false;
        try { parseSource("fn greet :\n;"); }
        catch (const ParseError& e) {
            caught = true;
            XASSERT(contains(e.what(), "Expected '('")); 
        }
        XASSERT(caught); });

    runTest("parser: missing 'from' in bring → ParseError", [&]()
            {
        bool caught = false;
        try { parseSource("bring setup \"file.xel\""); }
        catch (const ParseError& e) {
            caught = true;
            XASSERT(contains(e.what(), "Expected 'from'"));
        }
        XASSERT(caught); });

    runTest("parser: unexpected token → ParseError", [&]()
            {
        bool caught = false;
        try { parseSource("= = ="); }
        catch (const ParseError& e) {
            caught = true;
            XASSERT(contains(e.what(), "Unexpected token"));
        }
        XASSERT(caught); });

    runTest("parser: missing ':' after while → ParseError", [&]()
            {
        bool caught = false;
        try { parseSource("while x > 0\n    x = x - 1\n;"); }
        catch (const ParseError& e) {
            caught = true;
            XASSERT(contains(e.what(), "Expected ':'"));
        }
        XASSERT(caught); });

    runTest("parser: missing 'in' in for → ParseError", [&]()
            {
        bool caught = false;
        try { parseSource("for x items :\n;"); }
        catch (const ParseError& e) {
            caught = true;
            XASSERT(contains(e.what(), "Expected 'in'"));
        }
        XASSERT(caught); });

    runTest("parser: ParseError is also catchable as XellError", [&]()
            {
        bool caught = false;
        try { parseSource("if x == 1\n;"); }
        catch (const XellError&) { caught = true; }
        XASSERT(caught); });

    runTest("parser: ParseError carries correct line number", [&]()
            {
        bool caught = false;
        try { parseSource("x = 10\ny = 20\nif z == 1\n;"); }
        catch (const ParseError& e) {
            caught = true;
            XASSERT_EQ(e.line(), 3);
        }
        XASSERT(caught); });
}

// ============================================================================
// Section 5: Error specificity — catch most-derived first
// ============================================================================

static void testErrorSpecificity()
{
    std::cout << "\n===== Error Specificity =====\n";

    runTest("specific catch before generic: LexerError vs XellError", []()
            {
        std::string which;
        try { throw LexerError("test", 1); }
        catch (const LexerError&) { which = "LexerError"; }
        catch (const XellError&) { which = "XellError"; }
        XASSERT_EQ(which, std::string("LexerError")); });

    runTest("specific catch before generic: ParseError vs XellError", []()
            {
        std::string which;
        try { throw ParseError("test", 1); }
        catch (const ParseError&) { which = "ParseError"; }
        catch (const XellError&) { which = "XellError"; }
        XASSERT_EQ(which, std::string("ParseError")); });

    runTest("specific catch before generic: TypeError vs XellError", []()
            {
        std::string which;
        try { throw TypeError("test", 1); }
        catch (const TypeError&) { which = "TypeError"; }
        catch (const XellError&) { which = "XellError"; }
        XASSERT_EQ(which, std::string("TypeError")); });

    runTest("specific catch before generic: IndexError vs XellError", []()
            {
        std::string which;
        try { throw IndexError("test", 1); }
        catch (const IndexError&) { which = "IndexError"; }
        catch (const XellError&) { which = "XellError"; }
        XASSERT_EQ(which, std::string("IndexError")); });

    runTest("generic XellError catches all derived errors", []()
            {
        int caught = 0;
        auto tryIt = [&](std::function<void()> thrower) {
            try { thrower(); }
            catch (const XellError&) { caught++; }
        };
        tryIt([]{ throw LexerError("t", 1); });
        tryIt([]{ throw ParseError("t", 1); });
        tryIt([]{ throw TypeError("t", 1); });
        tryIt([]{ throw UndefinedVariableError("x", 1); });
        tryIt([]{ throw UndefinedFunctionError("f", 1); });
        tryIt([]{ throw IndexError("t", 1); });
        tryIt([]{ throw KeyError("k", 1); });
        tryIt([]{ throw DivisionByZeroError(1); });
        tryIt([]{ throw OverflowError("t", 1); });
        tryIt([]{ throw ArityError("f", 1, 0, 1); });
        tryIt([]{ throw BringError("t", 1); });
        tryIt([]{ throw FileNotFoundError("f", 1); });
        tryIt([]{ throw IOError("t", 1); });
        tryIt([]{ throw ProcessError("t", 1); });
        tryIt([]{ throw ConversionError("t", 1); });
        tryIt([]{ throw EnvError("E", 1); });
        tryIt([]{ throw RecursionError(100, 1); });
        tryIt([]{ throw AssertionError("t", 1); });
        tryIt([]{ throw NotImplementedError("x", 1); });
        XASSERT_EQ(caught, 19); });
}

// ============================================================================
// Section 6: Message format consistency
// ============================================================================

static void testMessageFormat()
{
    std::cout << "\n===== Message Format =====\n";

    runTest("all errors follow [XELL ERROR] Line N format", []()
            {
        // Spot-check a few different error types
        LexerError e1("msg", 5);
        XASSERT(contains(e1.what(), "[XELL ERROR] Line 5"));

        ParseError e2("msg", 10);
        XASSERT(contains(e2.what(), "[XELL ERROR] Line 10"));

        TypeError e3("msg", 100);
        XASSERT(contains(e3.what(), "[XELL ERROR] Line 100"));

        DivisionByZeroError e4(1);
        XASSERT(contains(e4.what(), "[XELL ERROR] Line 1"));

        ArityError e5("fn", 3, 1, 77);
        XASSERT(contains(e5.what(), "[XELL ERROR] Line 77"));

        RecursionError e6(500, 33);
        XASSERT(contains(e6.what(), "[XELL ERROR] Line 33")); });

    runTest("what() contains category and detail", []()
            {
        KeyError err("host", 2);
        std::string msg = err.what();
        // format: [XELL ERROR] Line 2 — KeyError: 'host' not found in map
        XASSERT(contains(msg, "KeyError"));
        XASSERT(contains(msg, "'host' not found in map"));
        XASSERT(contains(msg, "Line 2")); });
}

// ============================================================================
// main
// ============================================================================

int main()
{
    testErrorConstruction();
    testCatchHierarchy();
    testLexerErrors();
    testParserErrors();
    testErrorSpecificity();
    testMessageFormat();

    std::cout << "\n============================================\n";
    std::cout << "  Total: " << (g_passed + g_failed)
              << "  |  Passed: " << g_passed
              << "  |  Failed: " << g_failed << "\n";
    std::cout << "============================================\n";

    return g_failed == 0 ? 0 : 1;
}
