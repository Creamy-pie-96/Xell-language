// ============================================================================
// Xell — Expression-mode if/elif/else, context-sensitive loops, loop keyword
// ============================================================================
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <functional>

static int g_passed = 0;
static int g_failed = 0;

#define XASSERT_EQ(a, b)                                               \
    do                                                                 \
    {                                                                  \
        auto _a = (a);                                                 \
        auto _b = (b);                                                 \
        if (_a != _b)                                                  \
        {                                                              \
            std::ostringstream os;                                     \
            os << "  ASSERTION FAILED: " << #a << " == " << #b << "\n" \
               << "        got: [" << _a << "] vs [" << _b << "]";     \
            throw std::runtime_error(os.str());                        \
        }                                                              \
    } while (0)

#define XASSERT_CONTAINS(haystack, needle)                                 \
    do                                                                     \
    {                                                                      \
        auto _h = (haystack);                                              \
        auto _n = (needle);                                                \
        if (_h.find(_n) == std::string::npos)                              \
        {                                                                  \
            std::ostringstream os;                                         \
            os << "  ASSERTION FAILED: output contains \"" << _n << "\"\n" \
               << "        got: [" << _h << "]";                           \
            throw std::runtime_error(os.str());                            \
        }                                                                  \
    } while (0)

#define XASSERT_THROWS(expr)                                                  \
    do                                                                        \
    {                                                                         \
        bool caught = false;                                                  \
        try                                                                   \
        {                                                                     \
            expr;                                                             \
        }                                                                     \
        catch (...)                                                           \
        {                                                                     \
            caught = true;                                                    \
        }                                                                     \
        if (!caught)                                                          \
            throw std::runtime_error("  Expected exception but none thrown"); \
    } while (0)

#define XASSERT_THROWS_MSG(expr, msg_fragment)                                \
    do                                                                        \
    {                                                                         \
        bool caught = false;                                                  \
        std::string caught_msg;                                               \
        try                                                                   \
        {                                                                     \
            expr;                                                             \
        }                                                                     \
        catch (const std::exception &e)                                       \
        {                                                                     \
            caught = true;                                                    \
            caught_msg = e.what();                                            \
        }                                                                     \
        if (!caught)                                                          \
            throw std::runtime_error("  Expected exception but none thrown"); \
        if (caught_msg.find(msg_fragment) == std::string::npos)               \
        {                                                                     \
            std::ostringstream os;                                            \
            os << "  Exception thrown but message doesn't contain \""         \
               << msg_fragment << "\"\n        got: [" << caught_msg << "]";  \
            throw std::runtime_error(os.str());                               \
        }                                                                     \
    } while (0)

#include "../src/lexer/lexer.cpp"
#include "../src/parser/parser.cpp"
#include "../src/interpreter/xobject.cpp"
#include "../src/interpreter/interpreter.cpp"

using namespace xell;

static std::vector<std::string> runXell(const std::string &code)
{
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();
    Interpreter interp;
    interp.run(program);
    return interp.output();
}

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
        std::cout << "  FAIL: " << name << "\n"
                  << e.what() << "\n";
        g_failed++;
    }
}

// ============================================================================
// EXPRESSION-MODE IF / ELIF / ELSE
// ============================================================================

void testIfExprSimple()
{
    auto out = runXell(R"XEL(
        x = if true: 42 else: 0
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "42");
}

void testIfExprwithoutelse()
{
    XASSERT_THROWS_MSG(
        runXell(R"XEL(
        x = if true: 42
        print(x)
    )XEL"),
        "requires an 'else' branch");
}

void testIfExprElseBranch()
{
    auto out = runXell(R"XEL(
        x = if false: 42 else: 99
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "99");
}

void testIfExprElif()
{
    auto out = runXell(R"XEL(
        val = 2
        x = if val == 1: "one" elif val == 2: "two" elif val == 3: "three" else: "other"
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "two");
}

void testIfExprElifFallthrough()
{
    auto out = runXell(R"XEL(
        val = 99
        x = if val == 1: "one" elif val == 2: "two" else: "many"
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "many");
}

void testIfExprNested()
{
    auto out = runXell(R"XEL(
        a = 5
        b = 10
        x = if a > b: "a" else: if b > 7: "b" else: "neither"
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "b");
}

void testIfExprInFunctionCall()
{
    auto out = runXell(R"XEL(
        fn double(n):
            give n * 2
        ;
        x = double(if true: 5 else: 10)
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "10");
}

void testIfExprStringValues()
{
    auto out = runXell(R"XEL(
        name = "Alice"
        greeting = if name == "Alice": "Hello Alice!" else: "Who are you?"
        print(greeting)
    )XEL");
    XASSERT_EQ(out[0], "Hello Alice!");
}

void testIfExprWithExpressions()
{
    auto out = runXell(R"XEL(
        x = 10
        result = if x > 5: x * 2 else: x + 1
        print(result)
    )XEL");
    XASSERT_EQ(out[0], "20");
}

void testIfExprRequiresElse()
{
    XASSERT_THROWS(runXell("x = if true: 42"));
}

// ============================================================================
// EXPRESSION-MODE FOR LOOP
// ============================================================================

void testForExprBreakValue()
{
    auto out = runXell(R"XEL(
        x = for i in [1, 2, 3, 4, 5]:
            if i == 3:
                break i * 10
            ;
        give -1 ;
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "30");
}

void testForExprDefaultValue()
{
    auto out = runXell(R"XEL(
        x = for i in [1, 2, 3]:
            if i > 100:
                break i
            ;
        give "not found" ;
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "not found");
}

void testForExprNoBreakNoDefault()
{
    auto out = runXell(R"XEL(
        x = for i in [1, 2, 3]:
            if i > 100:
                break i
            ;
        ;
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "none");
}

void testForExprFindFirst()
{
    auto out = runXell(R"XEL(
        items = ["apple", "banana", "cherry"]
        found = for item in items:
            if item == "banana":
                break item
            ;
        give "nothing" ;
        print(found)
    )XEL");
    XASSERT_EQ(out[0], "banana");
}

void testForExprWithContinue()
{
    // Test that continue skips iterations but loop continues
    auto out = runXell(R"XEL(
        result = for i in [1, 2, 3, 4, 5]:
            if i % 2 == 0:
                continue
            ;
            if i == 5:
                break i
            ;
        give 0 ;
        print(result)
    )XEL");
    XASSERT_EQ(out[0], "5");
}

void testForExprOverString()
{
    auto out = runXell(R"XEL(
        x = for ch in "hello":
            if ch == "l":
                break ch
            ;
        give "?" ;
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "l");
}

// ============================================================================
// EXPRESSION-MODE WHILE LOOP
// ============================================================================

void testWhileExprBreakValue()
{
    auto out = runXell(R"XEL(
        i = 0
        x = while i < 10:
            i = i + 1
            if i == 5:
                break i * 2
            ;
        give -1 ;
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "10");
}

void testWhileExprDefaultValue()
{
    auto out = runXell(R"XEL(
        i = 0
        x = while i < 3:
            i = i + 1
        give "done" ;
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "done");
}

void testWhileExprNoBreakNoDefault()
{
    auto out = runXell(R"XEL(
        i = 0
        x = while i < 3:
            i = i + 1
        ;
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "none");
}

// ============================================================================
// EXPRESSION-MODE LOOP (infinite)
// ============================================================================

void testLoopExprBreakValue()
{
    auto out = runXell(R"XEL(
        i = 0
        x = loop:
            i = i + 1
            if i == 5:
                break "found"
            ;
        give "never" ;
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "found");
}

void testLoopExprWithDefault()
{
    // This loop always breaks, but if it didn't the default would kick in
    auto out = runXell(R"XEL(
        x = loop:
            break 42
        give 0 ;
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "42");
}

void testLoopExprBreakWithCalculation()
{
    auto out = runXell(R"XEL(
        sum = 0
        i = 0
        result = loop:
            i = i + 1
            sum = sum + i
            if sum > 10:
                break sum
            ;
        give 0 ;
        print(result)
    )XEL");
    XASSERT_EQ(out[0], "15");
}

// ============================================================================
// LOOP STATEMENT (infinite loop keyword)
// ============================================================================

void testLoopStmtBasic()
{
    auto out = runXell(R"XEL(
        i = 0
        loop:
            i = i + 1
            if i == 5:
                break
            ;
        ;
        print(i)
    )XEL");
    XASSERT_EQ(out[0], "5");
}

void testLoopStmtWithContinue()
{
    auto out = runXell(R"XEL(
        i = 0
        count = 0
        loop:
            i = i + 1
            if i > 10:
                break
            ;
            if i % 2 == 0:
                continue
            ;
            count = count + 1
        ;
        print(count)
    )XEL");
    XASSERT_EQ(out[0], "5");
}

void testLoopStmtBreakValueError()
{
    // Statement-mode loop must reject break VALUE
    XASSERT_THROWS_MSG(
        runXell(R"XEL(
            loop:
                break 42
            ;
        )XEL"),
        "statement-mode loop");
}

// ============================================================================
// STATEMENT-MODE LOOP VALUE ERRORS
// ============================================================================

void testForStmtBreakValueError()
{
    XASSERT_THROWS_MSG(
        runXell(R"XEL(
            for i in [1, 2, 3]:
                break i
            ;
        )XEL"),
        "statement-mode for");
}

void testWhileStmtBreakValueError()
{
    XASSERT_THROWS_MSG(
        runXell(R"XEL(
            i = 0
            while i < 5:
                i = i + 1
                break i
            ;
        )XEL"),
        "statement-mode while");
}

// ============================================================================
// @safe_loop DECORATOR
// ============================================================================

void testSafeLoopWithBreak()
{
    // Should succeed — body has a break
    auto out = runXell(R"XEL(
        i = 0
        @safe_loop
        loop:
            i = i + 1
            if i == 3:
                break
            ;
        ;
        print(i)
    )XEL");
    XASSERT_EQ(out[0], "3");
}

void testSafeLoopWithoutBreak()
{
    // Should throw — no break in body
    XASSERT_THROWS_MSG(
        runXell(R"XEL(
            @safe_loop
            loop:
                print("infinite")
            ;
        )XEL"),
        "@safe_loop");
}

void testSafeLoopBreakInIf()
{
    // Break inside an if — should still be detected as present
    auto out = runXell(R"XEL(
        i = 0
        @safe_loop
        loop:
            i = i + 1
            if i >= 5:
                break
            ;
        ;
        print(i)
    )XEL");
    XASSERT_EQ(out[0], "5");
}

// ============================================================================
// MIXED / COMPLEX TESTS
// ============================================================================

void testIfExprInsideForExpr()
{
    auto out = runXell(R"XEL(
        data = [1, -2, 3, -4, 5]
        first_pos = for item in data:
            if item > 0:
                break if item > 3: "big" else: "small"
            ;
        give "none found" ;
        print(first_pos)
    )XEL");
    XASSERT_EQ(out[0], "small");
}

void testExprLoopInAssignment()
{
    auto out = runXell(R"XEL(
        numbers = [10, 20, 30, 40, 50]
        idx = for i in [0, 1, 2, 3, 4]:
            if numbers[i] == 30:
                break i
            ;
        give -1 ;
        print(idx)
    )XEL");
    XASSERT_EQ(out[0], "2");
}

void testNestedExprLoops()
{
    auto out = runXell(R"XEL(
        result = for x in [1, 2, 3]:
            inner = for y in [10, 20, 30]:
                if x * y == 40:
                    break x * y
                ;
            give 0 ;
            if inner > 0:
                break inner
            ;
        give -1 ;
        print(result)
    )XEL");
    XASSERT_EQ(out[0], "40");
}

void testExprWhileSearchPattern()
{
    auto out = runXell(R"XEL(
        # Binary-search-like: find the square root floor of 50
        low = 0
        high = 50
        answer = while low <= high:
            mid = Int((low + high) / 2)
            if mid * mid == 50:
                break mid
            elif mid * mid < 50:
                low = mid + 1
            else:
                high = mid - 1
            ;
        give high ;
        print(answer)
    )XEL");
    XASSERT_EQ(out[0], "7");
}

void testLoopExprAccumulate()
{
    // Accumulate until threshold
    auto out = runXell(R"XEL(
        total = 0
        n = 0
        final = loop:
            n = n + 1
            total = total + n
            if total >= 21:
                break n
            ;
        give 0 ;
        print(final)
    )XEL");
    XASSERT_EQ(out[0], "6");
}

void testBreakWithoutValueInExprLoop()
{
    // break without value in expression loop → no value captured → uses default
    auto out = runXell(R"XEL(
        x = for i in [1, 2, 3]:
            if i == 2:
                break
            ;
        give "default" ;
        print(x)
    )XEL");
    XASSERT_EQ(out[0], "default");
}

// ============================================================================
// MAIN
// ============================================================================

int main()
{
    std::cout << "=== Expression-mode if/elif/else ===\n";
    runTest("if expr - simple true", testIfExprSimple);
    runTest("if - expr - without else", testIfExprwithoutelse);
    runTest("if expr - else branch", testIfExprElseBranch);
    runTest("if expr - elif", testIfExprElif);
    runTest("if expr - elif fallthrough", testIfExprElifFallthrough);
    runTest("if expr - nested", testIfExprNested);
    runTest("if expr - in function call", testIfExprInFunctionCall);
    runTest("if expr - string values", testIfExprStringValues);
    runTest("if expr - with expressions", testIfExprWithExpressions);
    runTest("if expr - requires else", testIfExprRequiresElse);

    std::cout << "\n=== Expression-mode for loop ===\n";
    runTest("for expr - break value", testForExprBreakValue);
    runTest("for expr - default value", testForExprDefaultValue);
    runTest("for expr - no break no default", testForExprNoBreakNoDefault);
    runTest("for expr - find first", testForExprFindFirst);
    runTest("for expr - with continue", testForExprWithContinue);
    runTest("for expr - over string", testForExprOverString);

    std::cout << "\n=== Expression-mode while loop ===\n";
    runTest("while expr - break value", testWhileExprBreakValue);
    runTest("while expr - default value", testWhileExprDefaultValue);
    runTest("while expr - no break no default", testWhileExprNoBreakNoDefault);

    std::cout << "\n=== Expression-mode loop (infinite) ===\n";
    runTest("loop expr - break value", testLoopExprBreakValue);
    runTest("loop expr - with default", testLoopExprWithDefault);
    runTest("loop expr - break with calculation", testLoopExprBreakWithCalculation);

    std::cout << "\n=== Loop statement (infinite loop keyword) ===\n";
    runTest("loop stmt - basic", testLoopStmtBasic);
    runTest("loop stmt - with continue", testLoopStmtWithContinue);
    runTest("loop stmt - break value error", testLoopStmtBreakValueError);

    std::cout << "\n=== Statement-mode loop value errors ===\n";
    runTest("for stmt - break value error", testForStmtBreakValueError);
    runTest("while stmt - break value error", testWhileStmtBreakValueError);

    std::cout << "\n=== @safe_loop decorator ===\n";
    runTest("safe_loop - with break", testSafeLoopWithBreak);
    runTest("safe_loop - without break", testSafeLoopWithoutBreak);
    runTest("safe_loop - break in if", testSafeLoopBreakInIf);

    std::cout << "\n=== Mixed / Complex tests ===\n";
    runTest("if expr inside for expr", testIfExprInsideForExpr);
    runTest("expr loop in assignment", testExprLoopInAssignment);
    runTest("nested expr loops", testNestedExprLoops);
    runTest("while expr search pattern", testExprWhileSearchPattern);
    runTest("loop expr accumulate", testLoopExprAccumulate);
    runTest("break without value in expr loop", testBreakWithoutValueInExprLoop);

    std::cout << "\n=== RESULTS: " << g_passed << " passed, "
              << g_failed << " failed out of " << (g_passed + g_failed) << " ===\n";

    return g_failed > 0 ? 1 : 0;
}
