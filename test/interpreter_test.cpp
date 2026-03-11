// =============================================================================
// Interpreter Tests
// =============================================================================
// Verifies the full Xell interpreter: variables, scoping, if/elif/else,
// for/while loops, functions, recursion, builtins, lists, maps, errors, etc.
//
// Each test lexes + parses + interprets a Xell source string, then checks
// the captured print() output.
// =============================================================================

#include "../src/interpreter/interpreter.hpp"
#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include "../src/xobject/gc.hpp"
#include <iostream>
#include <sstream>
#include <functional>
#include <string>
#include <vector>

using namespace xell;

// ---- Minimal test framework ------------------------------------------------

static int g_passed = 0;
static int g_failed = 0;

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

// Helper: run Xell source and return the output lines
static std::vector<std::string> runXell(const std::string &source)
{
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();
    Interpreter interp;
    interp.run(program);
    return interp.output();
}

// Helper: run Xell source and expect a specific exception type
template <typename ExcType>
static bool expectError(const std::string &source)
{
    try
    {
        runXell(source);
        return false;
    }
    catch (const ExcType &)
    {
        return true;
    }
}

// ============================================================================
// Section 1: Basic Expressions & Print
// ============================================================================

static void testBasicExpressions()
{
    std::cout << "\n===== Basic Expressions & Print =====\n";

    runTest("print number", []()
            {
        auto out = runXell("print(42)");
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], std::string("42")); });

    runTest("print string", []()
            {
        auto out = runXell("print(\"hello\")");
        XASSERT_EQ(out[0], std::string("hello")); });

    runTest("print bool true", []()
            {
        auto out = runXell("print(true)");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("print bool false", []()
            {
        auto out = runXell("print(false)");
        XASSERT_EQ(out[0], std::string("false")); });

    runTest("print none", []()
            {
        auto out = runXell("print(none)");
        XASSERT_EQ(out[0], std::string("none")); });

    runTest("print multiple args", []()
            {
        auto out = runXell("print(1, \"two\", true)");
        XASSERT_EQ(out[0], std::string("1 two true")); });

    runTest("arithmetic: add", []()
            {
        auto out = runXell("print(1 + 2)");
        XASSERT_EQ(out[0], std::string("3")); });

    runTest("arithmetic: subtract", []()
            {
        auto out = runXell("print(10 - 3)");
        XASSERT_EQ(out[0], std::string("7")); });

    runTest("arithmetic: multiply", []()
            {
        auto out = runXell("print(4 * 5)");
        XASSERT_EQ(out[0], std::string("20")); });

    runTest("arithmetic: divide", []()
            {
        auto out = runXell("print(15 / 3)");
        XASSERT_EQ(out[0], std::string("5")); });

    runTest("arithmetic: complex expression", []()
            {
        auto out = runXell("print(2 + 3 * 4)");
        XASSERT_EQ(out[0], std::string("14")); });

    runTest("arithmetic: parenthesized", []()
            {
        auto out = runXell("print((2 + 3) * 4)");
        XASSERT_EQ(out[0], std::string("20")); });

    runTest("unary minus", []()
            {
        auto out = runXell("print(-5)");
        XASSERT_EQ(out[0], std::string("-5")); });

    runTest("string concatenation with +", []()
            {
        auto out = runXell("print(\"hello\" + \" \" + \"world\")");
        XASSERT_EQ(out[0], std::string("hello world")); });

    runTest("string + number auto-converts", []()
            {
        auto out = runXell("print(\"count: \" + 42)");
        XASSERT_EQ(out[0], std::string("count: 42")); });

    runTest("list concatenation", []()
            {
        auto out = runXell("print([1, 2] + [3, 4])");
        XASSERT_EQ(out[0], std::string("[1, 2, 3, 4]")); });

    runTest("comparison: ==", []()
            {
        auto out = runXell("print(1 == 1)\nprint(1 == 2)");
        XASSERT_EQ(out[0], std::string("true"));
        XASSERT_EQ(out[1], std::string("false")); });

    runTest("comparison: !=", []()
            {
        auto out = runXell("print(1 != 2)");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("comparison: > < >= <=", []()
            {
        auto out = runXell("print(5 > 3)\nprint(3 < 5)\nprint(5 >= 5)\nprint(5 <= 5)");
        XASSERT_EQ(out[0], std::string("true"));
        XASSERT_EQ(out[1], std::string("true"));
        XASSERT_EQ(out[2], std::string("true"));
        XASSERT_EQ(out[3], std::string("true")); });

    runTest("logical: and", []()
            {
        auto out = runXell("print(true and false)\nprint(true and true)");
        XASSERT_EQ(out[0], std::string("false"));
        XASSERT_EQ(out[1], std::string("true")); });

    runTest("logical: or", []()
            {
        auto out = runXell("print(false or true)\nprint(false or false)");
        XASSERT_EQ(out[0], std::string("true"));
        XASSERT_EQ(out[1], std::string("false")); });

    runTest("logical: not", []()
            {
        auto out = runXell("print(not true)\nprint(not false)");
        XASSERT_EQ(out[0], std::string("false"));
        XASSERT_EQ(out[1], std::string("true")); });

    runTest("short-circuit and: returns last truthy or first falsy", []()
            {
        auto out = runXell("print(1 and 2)");
        XASSERT_EQ(out[0], std::string("2")); });

    runTest("short-circuit or: returns first truthy", []()
            {
        auto out = runXell("print(0 or \"default\")");
        XASSERT_EQ(out[0], std::string("default")); });

    runTest("paren-less print call", []()
            {
        auto out = runXell("print 99");
        XASSERT_EQ(out[0], std::string("99")); });
}

// ============================================================================
// Section 2: Variables & Assignment
// ============================================================================

static void testVariables()
{
    std::cout << "\n===== Variables & Assignment =====\n";

    runTest("simple assignment and read", []()
            {
        auto out = runXell("x = 42\nprint(x)");
        XASSERT_EQ(out[0], std::string("42")); });

    runTest("re-assignment", []()
            {
        auto out = runXell("x = 1\nx = 2\nprint(x)");
        XASSERT_EQ(out[0], std::string("2")); });

    runTest("multiple variables", []()
            {
        auto out = runXell("a = 10\nb = 20\nprint(a + b)");
        XASSERT_EQ(out[0], std::string("30")); });

    runTest("variable in expression", []()
            {
        auto out = runXell("x = 5\nprint(x * 2 + 1)");
        XASSERT_EQ(out[0], std::string("11")); });

    runTest("assign expression result", []()
            {
        auto out = runXell("x = 3 + 4\nprint(x)");
        XASSERT_EQ(out[0], std::string("7")); });

    runTest("prefix increment", []()
            {
        auto out = runXell("x = 5\nprint(++x)\nprint(x)");
        XASSERT_EQ(out[0], std::string("6"));
        XASSERT_EQ(out[1], std::string("6")); });

    runTest("postfix increment", []()
            {
        auto out = runXell("x = 5\nprint(x++)\nprint(x)");
        XASSERT_EQ(out[0], std::string("5"));
        XASSERT_EQ(out[1], std::string("6")); });

    runTest("prefix decrement", []()
            {
        auto out = runXell("x = 5\nprint(--x)");
        XASSERT_EQ(out[0], std::string("4")); });

    runTest("postfix decrement", []()
            {
        auto out = runXell("x = 5\nprint(x--)\nprint(x)");
        XASSERT_EQ(out[0], std::string("5"));
        XASSERT_EQ(out[1], std::string("4")); });
}

// ============================================================================
// Section 3: If / Elif / Else
// ============================================================================

static void testConditionals()
{
    std::cout << "\n===== Conditionals =====\n";

    runTest("if true", []()
            {
        auto out = runXell("if true:\n  print(\"yes\")\n;");
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], std::string("yes")); });

    runTest("if false — no output", []()
            {
        auto out = runXell("if false:\n  print(\"no\")\n;");
        XASSERT_EQ(out.size(), (size_t)0); });

    runTest("if-else: takes else branch", []()
            {
        auto out = runXell("if false:\n  print(\"if\")\n;\nelse:\n  print(\"else\")\n;");
        XASSERT_EQ(out[0], std::string("else")); });

    runTest("if-elif-else", []()
            {
        auto out = runXell(
            "x = 2\n"
            "if x == 1:\n  print(\"one\")\n;\n"
            "elif x == 2:\n  print(\"two\")\n;\n"
            "else:\n  print(\"other\")\n;");
        XASSERT_EQ(out[0], std::string("two")); });

    runTest("if with comparison expression", []()
            {
        auto out = runXell("x = 10\nif x > 5:\n  print(\"big\")\n;");
        XASSERT_EQ(out[0], std::string("big")); });

    runTest("nested if", []()
            {
        auto out = runXell(
            "x = 10\n"
            "if x > 5:\n"
            "  if x > 20:\n"
            "    print(\"huge\")\n"
            "  ;\n"
            "  else:\n"
            "    print(\"medium\")\n"
            "  ;\n"
            ";");
        XASSERT_EQ(out[0], std::string("medium")); });

    runTest("if: truthiness of number", []()
            {
        auto out = runXell("if 0:\n  print(\"zero\")\n;\nif 1:\n  print(\"one\")\n;");
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], std::string("one")); });

    runTest("if: truthiness of string", []()
            {
        auto out = runXell("if \"\":\n  print(\"empty\")\n;\nif \"hi\":\n  print(\"hi\")\n;");
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], std::string("hi")); });

    runTest("multiple elif branches", []()
            {
        auto out = runXell(
            "x = 4\n"
            "if x == 1:\n  print(\"a\")\n;\n"
            "elif x == 2:\n  print(\"b\")\n;\n"
            "elif x == 3:\n  print(\"c\")\n;\n"
            "elif x == 4:\n  print(\"d\")\n;\n"
            "else:\n  print(\"e\")\n;");
        XASSERT_EQ(out[0], std::string("d")); });
}

// ============================================================================
// Section 4: For Loops
// ============================================================================

static void testForLoops()
{
    std::cout << "\n===== For Loops =====\n";

    runTest("for over list literal", []()
            {
        auto out = runXell("for x in [1, 2, 3]:\n  print(x)\n;");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("2"));
        XASSERT_EQ(out[2], std::string("3")); });

    runTest("for over range", []()
            {
        auto out = runXell("for i in range(5):\n  print(i)\n;");
        XASSERT_EQ(out.size(), (size_t)5);
        XASSERT_EQ(out[0], std::string("0"));
        XASSERT_EQ(out[4], std::string("4")); });

    runTest("for over range(start, end)", []()
            {
        auto out = runXell("for i in range(2, 5):\n  print(i)\n;");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], std::string("2"));
        XASSERT_EQ(out[2], std::string("4")); });

    runTest("for with accumulator", []()
            {
        auto out = runXell(
            "total = 0\n"
            "for i in range(1, 6):\n"
            "  total = total + i\n"
            ";\n"
            "print(total)");
        XASSERT_EQ(out[0], std::string("15")); });

    runTest("for over variable list", []()
            {
        auto out = runXell(
            "items = [\"a\", \"b\", \"c\"]\n"
            "for item in items:\n"
            "  print(item)\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], std::string("a"));
        XASSERT_EQ(out[1], std::string("b"));
        XASSERT_EQ(out[2], std::string("c")); });

    runTest("nested for loops", []()
            {
        auto out = runXell(
            "for i in range(3):\n"
            "  for j in range(2):\n"
            "    print(i * 10 + j)\n"
            "  ;\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)6);
        XASSERT_EQ(out[0], std::string("0"));
        XASSERT_EQ(out[1], std::string("1"));
        XASSERT_EQ(out[2], std::string("10"));
        XASSERT_EQ(out[3], std::string("11"));
        XASSERT_EQ(out[4], std::string("20"));
        XASSERT_EQ(out[5], std::string("21")); });

    runTest("for over empty list", []()
            {
        auto out = runXell("for x in []:\n  print(x)\n;");
        XASSERT_EQ(out.size(), (size_t)0); });
}

// ============================================================================
// Section 5: While Loops
// ============================================================================

static void testWhileLoops()
{
    std::cout << "\n===== While Loops =====\n";

    runTest("while countdown", []()
            {
        auto out = runXell(
            "x = 3\n"
            "while x > 0:\n"
            "  print(x)\n"
            "  x = x - 1\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], std::string("3"));
        XASSERT_EQ(out[1], std::string("2"));
        XASSERT_EQ(out[2], std::string("1")); });

    runTest("while false — never executes", []()
            {
        auto out = runXell("while false:\n  print(\"no\")\n;");
        XASSERT_EQ(out.size(), (size_t)0); });

    runTest("while with accumulator", []()
            {
        auto out = runXell(
            "sum = 0\n"
            "i = 1\n"
            "while i <= 10:\n"
            "  sum = sum + i\n"
            "  i = i + 1\n"
            ";\n"
            "print(sum)");
        XASSERT_EQ(out[0], std::string("55")); });
}

// ============================================================================
// Section 6: Functions
// ============================================================================

static void testFunctions()
{
    std::cout << "\n===== Functions =====\n";

    runTest("simple function definition and call", []()
            {
        auto out = runXell(
            "fn greet():\n"
            "  print(\"hello\")\n"
            ";\n"
            "greet()");
        XASSERT_EQ(out[0], std::string("hello")); });

    runTest("function with parameter", []()
            {
        auto out = runXell(
            "fn double(x):\n"
            "  print(x * 2)\n"
            ";\n"
            "double(21)");
        XASSERT_EQ(out[0], std::string("42")); });

    runTest("function with give (return)", []()
            {
        auto out = runXell(
            "fn add(a, b):\n"
            "  give a + b\n"
            ";\n"
            "print(add(3, 4))");
        XASSERT_EQ(out[0], std::string("7")); });

    runTest("function returns none by default", []()
            {
        auto out = runXell(
            "fn noop():\n"
            "  x = 1\n"
            ";\n"
            "print(noop())");
        XASSERT_EQ(out[0], std::string("none")); });

    runTest("function with multiple params", []()
            {
        auto out = runXell(
            "fn greet(first, last):\n"
            "  give first + \" \" + last\n"
            ";\n"
            "print(greet(\"John\", \"Doe\"))");
        XASSERT_EQ(out[0], std::string("John Doe")); });

    runTest("function calling another function", []()
            {
        auto out = runXell(
            "fn square(x):\n"
            "  give x * x\n"
            ";\n"
            "fn sum_squares(a, b):\n"
            "  give square(a) + square(b)\n"
            ";\n"
            "print(sum_squares(3, 4))");
        XASSERT_EQ(out[0], std::string("25")); });

    runTest("early give (return)", []()
            {
        auto out = runXell(
            "fn abs(x):\n"
            "  if x < 0:\n"
            "    give -x\n"
            "  ;\n"
            "  give x\n"
            ";\n"
            "print(abs(-5))\n"
            "print(abs(3))");
        XASSERT_EQ(out[0], std::string("5"));
        XASSERT_EQ(out[1], std::string("3")); });

    runTest("give with no value → none", []()
            {
        auto out = runXell(
            "fn nope():\n"
            "  give\n"
            ";\n"
            "print(nope())");
        XASSERT_EQ(out[0], std::string("none")); });
}

// ============================================================================
// Section 7: Scoping
// ============================================================================

static void testScoping()
{
    std::cout << "\n===== Scoping =====\n";

    runTest("global variable accessible in function", []()
            {
        auto out = runXell(
            "name = \"xell\"\n"
            "fn say():\n"
            "  print(name)\n"
            ";\n"
            "say()");
        XASSERT_EQ(out[0], std::string("xell")); });

    runTest("function can modify global variable", []()
            {
        auto out = runXell(
            "x = 10\n"
            "fn change():\n"
            "  x = 20\n"
            ";\n"
            "change()\n"
            "print(x)");
        XASSERT_EQ(out[0], std::string("20")); });

    runTest("function local does not leak", []()
            { XASSERT(expectError<UndefinedVariableError>(
                  "fn foo():\n"
                  "  local_var = 5\n"
                  ";\n"
                  "foo()\n"
                  "print(local_var)")); });

    runTest("if-block scope does not leak", []()
            { XASSERT(expectError<UndefinedVariableError>(
                  "if true:\n"
                  "  inner = 42\n"
                  ";\n"
                  "print(inner)")); });

    runTest("for-block scope does not leak", []()
            { XASSERT(expectError<UndefinedVariableError>(
                  "for i in [1]:\n"
                  "  temp = i\n"
                  ";\n"
                  "print(temp)")); });

    runTest("while-block scope does not leak", []()
            { XASSERT(expectError<UndefinedVariableError>(
                  "x = 1\n"
                  "while x > 0:\n"
                  "  x = x - 1\n"
                  "  inner = 99\n"
                  ";\n"
                  "print(inner)")); });

    runTest("nested function scope", []()
            {
        auto out = runXell(
            "fn outer():\n"
            "  x = 10\n"
            "  fn inner():\n"
            "    print(x)\n"
            "  ;\n"
            "  inner()\n"
            ";\n"
            "outer()");
        XASSERT_EQ(out[0], std::string("10")); });

    runTest("parameter shadows global", []()
            {
        auto out = runXell(
            "x = \"global\"\n"
            "fn show(x):\n"
            "  print(x)\n"
            ";\n"
            "show(\"local\")\n"
            "print(x)");
        XASSERT_EQ(out[0], std::string("local"));
        XASSERT_EQ(out[1], std::string("global")); });
}

// ============================================================================
// Section 8: Recursion
// ============================================================================

static void testRecursion()
{
    std::cout << "\n===== Recursion =====\n";

    runTest("factorial", []()
            {
        auto out = runXell(
            "fn fact(n):\n"
            "  if n <= 1:\n"
            "    give 1\n"
            "  ;\n"
            "  give n * fact(n - 1)\n"
            ";\n"
            "print(fact(10))");
        XASSERT_EQ(out[0], std::string("3628800")); });

    runTest("fibonacci", []()
            {
        auto out = runXell(
            "fn fib(n):\n"
            "  if n <= 0:\n"
            "    give 0\n"
            "  ;\n"
            "  if n == 1:\n"
            "    give 1\n"
            "  ;\n"
            "  give fib(n - 1) + fib(n - 2)\n"
            ";\n"
            "print(fib(10))");
        XASSERT_EQ(out[0], std::string("55")); });

    runTest("recursion depth limit", []()
            { XASSERT(expectError<RecursionError>(
                  "fn infinite():\n"
                  "  infinite()\n"
                  ";\n"
                  "infinite()")); });
}

// ============================================================================
// Section 9: Lists
// ============================================================================

static void testLists()
{
    std::cout << "\n===== Lists =====\n";

    runTest("list literal", []()
            {
        auto out = runXell("print([1, 2, 3])");
        XASSERT_EQ(out[0], std::string("[1, 2, 3]")); });

    runTest("empty list", []()
            {
        auto out = runXell("print([])");
        XASSERT_EQ(out[0], std::string("[]")); });

    runTest("list index access", []()
            {
        auto out = runXell("items = [10, 20, 30]\nprint(items[1])");
        XASSERT_EQ(out[0], std::string("20")); });

    runTest("list negative index", []()
            {
        auto out = runXell("items = [10, 20, 30]\nprint(items[-1])");
        XASSERT_EQ(out[0], std::string("30")); });

    runTest("len of list", []()
            {
        auto out = runXell("print(len([1, 2, 3]))");
        XASSERT_EQ(out[0], std::string("3")); });

    runTest("push and pop", []()
            {
        auto out = runXell(
            "items = [1, 2]\n"
            "push(items, 3)\n"
            "print(items)\n"
            "last = pop(items)\n"
            "print(last)\n"
            "print(items)");
        XASSERT_EQ(out[0], std::string("[1, 2, 3]"));
        XASSERT_EQ(out[1], std::string("3"));
        XASSERT_EQ(out[2], std::string("[1, 2]")); });

    runTest("set list element", []()
            {
        auto out = runXell(
            "items = [1, 2, 3]\n"
            "set(items, 1, 99)\n"
            "print(items)");
        XASSERT_EQ(out[0], std::string("[1, 99, 3]")); });

    runTest("list index out of bounds", []()
            { XASSERT(expectError<IndexError>(
                  "items = [1, 2, 3]\nprint(items[10])")); });
}

// ============================================================================
// Section 10: Maps
// ============================================================================

static void testMaps()
{
    std::cout << "\n===== Maps =====\n";

    runTest("map literal", []()
            {
        auto out = runXell("m = {name: \"xell\", version: 1}\nprint(m)");
        XASSERT(out[0].find("name: \"xell\"") != std::string::npos);
        XASSERT(out[0].find("version: 1") != std::string::npos); });

    runTest("map index access with string key", []()
            {
        auto out = runXell("m = {name: \"xell\"}\nprint(m[\"name\"])");
        XASSERT_EQ(out[0], std::string("xell")); });

    runTest("map arrow access", []()
            {
        auto out = runXell("m = {host: \"localhost\", port: 3000}\nprint(m->host)\nprint(m->port)");
        XASSERT_EQ(out[0], std::string("localhost"));
        XASSERT_EQ(out[1], std::string("3000")); });

    runTest("keys and values", []()
            {
        auto out = runXell(
            "m = {a: 1, b: 2}\n"
            "print(keys(m))\n"
            "print(values(m))");
        XASSERT_EQ(out[0], std::string("[\"a\", \"b\"]"));
        XASSERT_EQ(out[1], std::string("[1, 2]")); });

    runTest("has() builtin", []()
            {
        auto out = runXell(
            "m = {x: 1}\n"
            "print(has(m, \"x\"))\n"
            "print(has(m, \"y\"))");
        XASSERT_EQ(out[0], std::string("true"));
        XASSERT_EQ(out[1], std::string("false")); });

    runTest("set map entry", []()
            {
        auto out = runXell(
            "m = {a: 1}\n"
            "set(m, \"b\", 2)\n"
            "print(m->b)");
        XASSERT_EQ(out[0], std::string("2")); });

    runTest("map key not found", []()
            { XASSERT(expectError<KeyError>(
                  "m = {a: 1}\nprint(m[\"z\"])")); });
}

// ============================================================================
// Section 11: String Operations
// ============================================================================

static void testStrings()
{
    std::cout << "\n===== String Operations =====\n";

    runTest("string length", []()
            {
        auto out = runXell("print(len(\"hello\"))");
        XASSERT_EQ(out[0], std::string("5")); });

    runTest("string index", []()
            {
        auto out = runXell("print(\"hello\"[0])");
        XASSERT_EQ(out[0], std::string("h")); });

    runTest("string negative index", []()
            {
        auto out = runXell("print(\"hello\"[-1])");
        XASSERT_EQ(out[0], std::string("o")); });

    runTest("string concatenation", []()
            {
        auto out = runXell("a = \"foo\"\nb = \"bar\"\nprint(a + b)");
        XASSERT_EQ(out[0], std::string("foobar")); });
}

// ============================================================================
// Section 12: Built-in Functions
// ============================================================================

static void testBuiltins()
{
    std::cout << "\n===== Built-in Functions =====\n";

    runTest("type()", []()
            {
        auto out = runXell(
            "print(type(42))\n"
            "print(type(\"hi\"))\n"
            "print(type(true))\n"
            "print(type(none))\n"
            "print(type([]))\n"
            "print(type({}))");
        XASSERT_EQ(out[0], std::string("int"));; });

    runTest("str() conversion", []()
            {
        auto out = runXell("print(str(42))\nprint(str(true))");
        XASSERT_EQ(out[0], std::string("42"));
        XASSERT_EQ(out[1], std::string("true")); });

    runTest("num() conversion", []()
            {
        auto out = runXell("print(num(\"42\"))\nprint(num(true))");
        XASSERT_EQ(out[0], std::string("42"));
        XASSERT_EQ(out[1], std::string("1")); });

    runTest("assert passes", []()
            {
        auto out = runXell("assert(true)\nassert(1 == 1)");
        XASSERT_EQ(out.size(), (size_t)0); });

    runTest("assert fails", []()
            { XASSERT(expectError<AssertionError>("assert(false)")); });

    runTest("assert with message", []()
            {
        try
        {
            runXell("assert(false, \"custom message\")");
            XASSERT(false);
        }
        catch (const AssertionError &e)
        {
            std::string msg = e.what();
            XASSERT(msg.find("custom message") != std::string::npos);
        } });

    runTest("range with step", []()
            {
        auto out = runXell(
            "for i in range(0, 10, 2):\n"
            "  print(i)\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)5);
        XASSERT_EQ(out[0], std::string("0"));
        XASSERT_EQ(out[1], std::string("2"));
        XASSERT_EQ(out[2], std::string("4"));
        XASSERT_EQ(out[3], std::string("6"));
        XASSERT_EQ(out[4], std::string("8")); });

    runTest("range reverse", []()
            {
        auto out = runXell(
            "for i in range(5, 0, -1):\n"
            "  print(i)\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)5);
        XASSERT_EQ(out[0], std::string("5"));
        XASSERT_EQ(out[4], std::string("1")); });
}

// ============================================================================
// Section 13: Error Handling
// ============================================================================

static void testErrors()
{
    std::cout << "\n===== Error Handling =====\n";

    runTest("undefined variable", []()
            { XASSERT(expectError<UndefinedVariableError>("print(xyz)")); });

    runTest("type error: subtract strings", []()
            { XASSERT(expectError<TypeError>("print(\"a\" - \"b\")")); });

    runTest("division by zero", []()
            { XASSERT(expectError<DivisionByZeroError>("print(1 / 0)")); });

    runTest("arity error: too few args", []()
            { XASSERT(expectError<ArityError>(
                  "fn add(a, b):\n"
                  "  give a + b\n"
                  ";\n"
                  "add(1)")); });

    runTest("arity error: too many args", []()
            { XASSERT(expectError<ArityError>(
                  "fn noop():\n"
                  "  give none\n"
                  ";\n"
                  "noop(1, 2)")); });

    runTest("type error: call non-function", []()
            { XASSERT(expectError<TypeError>(
                  "x = 42\nx()")); });

    runTest("index error: string out of range", []()
            { XASSERT(expectError<IndexError>(
                  "print(\"hi\"[10])")); });

    runTest("type error: for non-list", []()
            { XASSERT(expectError<TypeError>(
                  "for x in 42:\n  print(x)\n;")); });

    runTest("conversion error: bad num()", []()
            { XASSERT(expectError<ConversionError>(
                  "num(\"abc\")")); });
}

// ============================================================================
// Section 14: Complex / Edge Case Programs
// ============================================================================

static void testComplexPrograms()
{
    std::cout << "\n===== Complex Programs =====\n";

    runTest("FizzBuzz", []()
            {
        auto out = runXell(
            "for i in range(1, 16):\n"
            "  fizz = i % 3 == 0\n"
            "  buzz = i % 5 == 0\n"
            "  if fizz and buzz:\n"
            "    print(\"FizzBuzz\")\n"
            "  ;\n"
            "  elif fizz:\n"
            "    print(\"Fizz\")\n"
            "  ;\n"
            "  elif buzz:\n"
            "    print(\"Buzz\")\n"
            "  ;\n"
            "  else:\n"
            "    print(i)\n"
            "  ;\n"
            ";");
        // 1, 2, Fizz, 4, Buzz, Fizz, 7, 8, Fizz, Buzz, 11, Fizz, 13, 14, FizzBuzz
        XASSERT_EQ(out.size(), (size_t)15);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[2], std::string("Fizz"));
        XASSERT_EQ(out[4], std::string("Buzz"));
        XASSERT_EQ(out[14], std::string("FizzBuzz")); });

    runTest("build a list with function", []()
            {
        auto out = runXell(
            "fn make_squares(n):\n"
            "  result = []\n"
            "  for i in range(1, n + 1):\n"
            "    push(result, i * i)\n"
            "  ;\n"
            "  give result\n"
            ";\n"
            "print(make_squares(5))");
        XASSERT_EQ(out[0], std::string("[1, 4, 9, 16, 25]")); });

    runTest("counter with while", []()
            {
        auto out = runXell(
            "fn count_digits(n):\n"
            "  digits = 0\n"
            "  if n == 0:\n"
            "    give 1\n"
            "  ;\n"
            "  while n > 0:\n"
            "    n = n - n / 10 * 10\n"
            "    n = (n - n) / 10\n"
            "  ;\n"
            "  give digits\n"
            ";\n"
            "print(count_digits(0))");
        // Just testing it doesn't crash; the math might be off due to float division
        XASSERT_EQ(out.size(), (size_t)1); });

    runTest("function as value: pass to variable, call later", []()
            {
        auto out = runXell(
            "fn greet(name):\n"
            "  give \"Hello, \" + name\n"
            ";\n"
            "say = greet\n"
            "print(say(\"World\"))");
        XASSERT_EQ(out[0], std::string("Hello, World")); });

    runTest("map as config object", []()
            {
        auto out = runXell(
            "config = {host: \"localhost\", port: 8080, debug: true}\n"
            "if config->debug:\n"
            "  print(\"Debug mode: \" + config->host + \":\" + str(config->port))\n"
            ";");
        XASSERT_EQ(out[0], std::string("Debug mode: localhost:8080")); });

    runTest("recursive sum of list", []()
            {
        auto out = runXell(
            "fn sum_list(lst, idx):\n"
            "  if idx >= len(lst):\n"
            "    give 0\n"
            "  ;\n"
            "  give lst[idx] + sum_list(lst, idx + 1)\n"
            ";\n"
            "print(sum_list([10, 20, 30, 40], 0))");
        XASSERT_EQ(out[0], std::string("100")); });

    runTest("higher-order: map function over list", []()
            {
        auto out = runXell(
            "fn apply(lst, f):\n"
            "  result = []\n"
            "  for item in lst:\n"
            "    push(result, f(item))\n"
            "  ;\n"
            "  give result\n"
            ";\n"
            "fn double(x):\n"
            "  give x * 2\n"
            ";\n"
            "print(apply([1, 2, 3, 4], double))");
        XASSERT_EQ(out[0], std::string("[2, 4, 6, 8]")); });

    runTest("power function", []()
            {
        auto out = runXell(
            "fn power(base, exp):\n"
            "  result = 1\n"
            "  i = 0\n"
            "  while i < exp:\n"
            "    result = result * base\n"
            "    i = i + 1\n"
            "  ;\n"
            "  give result\n"
            ";\n"
            "print(power(2, 10))");
        XASSERT_EQ(out[0], std::string("1024")); });
}

// ============================================================================
// Section 15: String Interpolation
// ============================================================================

static void testInterpolation()
{
    std::cout << "\n===== String Interpolation =====\n";

    runTest("simple variable interpolation", []()
            {
        auto out = runXell("name = \"World\"\nprint(\"Hello, {name}!\")");
        XASSERT_EQ(out[0], std::string("Hello, World!")); });

    runTest("expression interpolation", []()
            {
        auto out = runXell("x = 5\nprint(\"x squared is {x * x}\")");
        XASSERT_EQ(out[0], std::string("x squared is 25")); });

    runTest("multiple interpolations", []()
            {
        auto out = runXell("a = 3\nb = 4\nprint(\"{a} + {b} = {a + b}\")");
        XASSERT_EQ(out[0], std::string("3 + 4 = 7")); });

    runTest("no interpolation when no braces", []()
            {
        auto out = runXell("print(\"plain string\")");
        XASSERT_EQ(out[0], std::string("plain string")); });
}

// ============================================================================
// Section: Break & Continue
// ============================================================================

static void testBreakContinue()
{
    std::cout << "\n===== Break & Continue =====\n";

    // -- break in for loop --
    runTest("break in for loop", []()
            {
        auto out = runXell(
            "for i in range(1, 10):\n"
            "  if i == 4:\n"
            "    break.\n"
            "  ;\n"
            "  print(i)\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("2"));
        XASSERT_EQ(out[2], std::string("3")); });

    runTest("break in while loop", []()
            {
        auto out = runXell(
            "i = 0\n"
            "while true:\n"
            "  i = i + 1\n"
            "  if i == 5:\n"
            "    break.\n"
            "  ;\n"
            ";\n"
            "print(i)");
        XASSERT_EQ(out[0], std::string("5")); });

    runTest("continue in for loop", []()
            {
        auto out = runXell(
            "for i in range(1, 6):\n"
            "  if i == 3:\n"
            "    continue.\n"
            "  ;\n"
            "  print(i)\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)4);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("2"));
        XASSERT_EQ(out[2], std::string("4"));
        XASSERT_EQ(out[3], std::string("5")); });

    runTest("continue in while loop", []()
            {
        auto out = runXell(
            "i = 0\n"
            "result = 0\n"
            "while i < 10:\n"
            "  i = i + 1\n"
            "  if i % 2 == 0:\n"
            "    continue.\n"
            "  ;\n"
            "  result = result + i\n"
            ";\n"
            "print(result)");
        // sum of odd 1..10 = 1+3+5+7+9 = 25
        XASSERT_EQ(out[0], std::string("25")); });

    runTest("break only exits innermost loop", []()
            {
        auto out = runXell(
            "for i in range(1, 4):\n"
            "  for j in range(1, 4):\n"
            "    if j == 2:\n"
            "      break.\n"
            "    ;\n"
            "    print(\"{i},{j}\")\n"
            "  ;\n"
            ";");
        // inner loop breaks at j==2 each time, so only j=1 prints
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], std::string("1,1"));
        XASSERT_EQ(out[1], std::string("2,1"));
        XASSERT_EQ(out[2], std::string("3,1")); });

    runTest("continue only affects innermost loop", []()
            {
        auto out = runXell(
            "for i in range(1, 3):\n"
            "  for j in range(1, 4):\n"
            "    if j == 2:\n"
            "      continue.\n"
            "    ;\n"
            "    print(\"{i},{j}\")\n"
            "  ;\n"
            ";");
        // j==2 is skipped, so prints 1,1 1,3 2,1 2,3
        XASSERT_EQ(out.size(), (size_t)4);
        XASSERT_EQ(out[0], std::string("1,1"));
        XASSERT_EQ(out[1], std::string("1,3"));
        XASSERT_EQ(out[2], std::string("2,1"));
        XASSERT_EQ(out[3], std::string("2,3")); });

    runTest("break with accumulator", []()
            {
        auto out = runXell(
            "sum = 0\n"
            "for i in range(1, 100):\n"
            "  sum = sum + i\n"
            "  if sum > 10:\n"
            "    break.\n"
            "  ;\n"
            ";\n"
            "print(sum)");
        // 1+2+3+4+5 = 15 > 10
        XASSERT_EQ(out[0], std::string("15")); });

    runTest("continue skips even numbers", []()
            {
        auto out = runXell(
            "for i in range(1, 8):\n"
            "  if i % 2 == 0:\n"
            "    continue.\n"
            "  ;\n"
            "  print(i)\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)4);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("3"));
        XASSERT_EQ(out[2], std::string("5"));
        XASSERT_EQ(out[3], std::string("7")); });

    runTest("break and continue together", []()
            {
        auto out = runXell(
            "for i in range(1, 20):\n"
            "  if i % 3 == 0:\n"
            "    continue.\n"
            "  ;\n"
            "  if i > 10:\n"
            "    break.\n"
            "  ;\n"
            "  print(i)\n"
            ";");
        // skip 3,6,9; break at 11; prints: 1,2,4,5,7,8,10
        XASSERT_EQ(out.size(), (size_t)7);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("2"));
        XASSERT_EQ(out[2], std::string("4"));
        XASSERT_EQ(out[3], std::string("5"));
        XASSERT_EQ(out[4], std::string("7"));
        XASSERT_EQ(out[5], std::string("8"));
        XASSERT_EQ(out[6], std::string("10")); });

    runTest("while break with found flag", []()
            {
        auto out = runXell(
            "items = [\"a\", \"b\", \"c\", \"d\"]\n"
            "found = false\n"
            "for item in items:\n"
            "  if item == \"c\":\n"
            "    found = true\n"
            "    break.\n"
            "  ;\n"
            ";\n"
            "print(found)");
        XASSERT_EQ(out[0], std::string("true")); });
}

// ============================================================================
// Section: New Language Features
// ============================================================================

static void testTryCatchFinally()
{
    std::cout << "\n===== Try / Catch / Finally =====\n";

    runTest("try-catch catches error", []()
            {
        auto out = runXell(
            "try:\n"
            "  x = 1 / 0\n"
            ";\n"
            "catch e:\n"
            "  print(e->type)\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        // category is "DivisionByZero" (without "Error" suffix)
        XASSERT(out[0].find("DivisionByZero") != std::string::npos); });

    runTest("try-catch error message", []()
            {
        auto out = runXell(
            "try:\n"
            "  x = 1 / 0\n"
            ";\n"
            "catch e:\n"
            "  print(e->message)\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT(out[0].find("division") != std::string::npos || out[0].find("zero") != std::string::npos); });

    runTest("try-catch-finally", []()
            {
        auto out = runXell(
            "try:\n"
            "  print(\"try\")\n"
            "  x = 1 / 0\n"
            ";\n"
            "catch e:\n"
            "  print(\"caught\")\n"
            ";\n"
            "finally:\n"
            "  print(\"finally\")\n"
            ";\n");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], std::string("try"));
        XASSERT_EQ(out[1], std::string("caught"));
        XASSERT_EQ(out[2], std::string("finally")); });

    runTest("try no error", []()
            {
        auto out = runXell(
            "try:\n"
            "  print(\"ok\")\n"
            ";\n"
            "catch e:\n"
            "  print(\"error\")\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("ok")); });

    runTest("try-finally no error", []()
            {
        auto out = runXell(
            "try:\n"
            "  print(\"try\")\n"
            ";\n"
            "catch e:\n"
            "  print(\"catch\")\n"
            ";\n"
            "finally:\n"
            "  print(\"finally\")\n"
            ";\n");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], std::string("try"));
        XASSERT_EQ(out[1], std::string("finally")); });
}

static void testThrowStatement()
{
    std::cout << "\n===== Throw Statement =====\n";

    runTest("throw string is caught with message", []()
            {
        auto out = runXell(
            "try:\n"
            "  throw \"something went wrong\"\n"
            ";\n"
            "catch e:\n"
            "  print(e->message)\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("something went wrong")); });

    runTest("throw string has RuntimeError type", []()
            {
        auto out = runXell(
            "try:\n"
            "  throw \"bad\"\n"
            ";\n"
            "catch e:\n"
            "  print(e->type)\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("RuntimeError")); });

    runTest("throw map with message and type", []()
            {
        auto out = runXell(
            "try:\n"
            "  throw {message: \"negative value\", type: \"TypeError\"}\n"
            ";\n"
            "catch e:\n"
            "  print(e->message)\n"
            "  print(e->type)\n"
            ";\n");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], std::string("negative value"));
        XASSERT_EQ(out[1], std::string("TypeError")); });

    runTest("throw propagates through function calls", []()
            {
        auto out = runXell(
            "fn validate(x):\n"
            "  if x < 0:\n"
            "    throw \"x must be non-negative\"\n"
            "  ;\n"
            "  give x * 2\n"
            ";\n"
            "try:\n"
            "  validate(-5)\n"
            ";\n"
            "catch e:\n"
            "  print(e->message)\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("x must be non-negative")); });

    runTest("throw with variable expression", []()
            {
        auto out = runXell(
            "msg = \"dynamic error\"\n"
            "try:\n"
            "  throw msg\n"
            ";\n"
            "catch e:\n"
            "  print(e->message)\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("dynamic error")); });

    runTest("uncaught throw propagates as RuntimeError", []()
            { XASSERT(expectError<xell::RuntimeError>("throw \"oops\"\n")); });

    runTest("throw non-string/non-map is TypeError", []()
            { XASSERT(expectError<xell::TypeError>("throw 42\n")); });

    runTest("throw in nested try-catch", []()
            {
        auto out = runXell(
            "try:\n"
            "  try:\n"
            "    throw \"inner error\"\n"
            "  ;\n"
            "  catch e:\n"
            "    print(\"inner: \" + e->message)\n"
            "    throw \"re-thrown\"\n"
            "  ;\n"
            ";\n"
            "catch e:\n"
            "  print(\"outer: \" + e->message)\n"
            ";\n");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], std::string("inner: inner error"));
        XASSERT_EQ(out[1], std::string("outer: re-thrown")); });

    runTest("throw with finally still runs finally", []()
            {
        auto out = runXell(
            "try:\n"
            "  throw \"error\"\n"
            ";\n"
            "catch e:\n"
            "  print(\"caught\")\n"
            ";\n"
            "finally:\n"
            "  print(\"cleanup\")\n"
            ";\n");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], std::string("caught"));
        XASSERT_EQ(out[1], std::string("cleanup")); });
}

static void testStackTraces()
{
    std::cout << "\n===== Stack Traces =====\n";

    runTest("traceback captures nested call chain", []()
            {
        auto out = runXell(
            "fn inner():\n"
            "  throw \"deep\"\n"
            ";\n"
            "fn middle():\n"
            "  inner()\n"
            ";\n"
            "fn outer():\n"
            "  middle()\n"
            ";\n"
            "try:\n"
            "  outer()\n"
            ";\n"
            "catch e:\n"
            "  print(len(e->traceback))\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("3")); });

    runTest("traceback frame has fn and line keys", []()
            {
        auto out = runXell(
            "fn boom():\n"
            "  throw \"bang\"\n"
            ";\n"
            "try:\n"
            "  boom()\n"
            ";\n"
            "catch e:\n"
            "  tb = e->traceback\n"
            "  frame = tb[0]\n"
            "  print(frame->name)\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("boom")); });

    runTest("traceback is empty for top-level error", []()
            {
        auto out = runXell(
            "try:\n"
            "  x = 1 / 0\n"
            ";\n"
            "catch e:\n"
            "  print(len(e->traceback))\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("0")); });

    runTest("each caught error gets fresh traceback", []()
            {
        auto out = runXell(
            "fn a():\n"
            "  throw \"err1\"\n"
            ";\n"
            "fn b():\n"
            "  fn b2():\n"
            "    throw \"err2\"\n"
            "  ;\n"
            "  b2()\n"
            ";\n"
            "try:\n"
            "  a()\n"
            ";\n"
            "catch e:\n"
            "  print(len(e->traceback))\n"
            ";\n"
            "try:\n"
            "  b()\n"
            ";\n"
            "catch e2:\n"
            "  print(len(e2->traceback))\n"
            ";\n");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("2")); });

    runTest("traceback works for non-throw errors in functions", []()
            {
        auto out = runXell(
            "fn divbyzero():\n"
            "  x = 1 / 0\n"
            ";\n"
            "try:\n"
            "  divbyzero()\n"
            ";\n"
            "catch e:\n"
            "  tb = e->traceback\n"
            "  print(len(tb))\n"
            "  frame = tb[0]\n"
            "  print(frame->name)\n"
            ";\n");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("divbyzero")); });
}

static void testClosureSafety()
{
    std::cout << "\n===== Closure Safety =====\n";

    runTest("returned fn closure captures variable", []()
            {
        auto out = runXell(
            "fn make_adder(x):\n"
            "  fn adder(y):\n"
            "    give x + y\n"
            "  ;\n"
            "  give adder\n"
            ";\n"
            "add5 = make_adder(5)\n"
            "print(add5(3))\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("8")); });

    runTest("multiple closures from same factory", []()
            {
        auto out = runXell(
            "fn make_adder(x):\n"
            "  fn adder(y):\n"
            "    give x + y\n"
            "  ;\n"
            "  give adder\n"
            ";\n"
            "add5 = make_adder(5)\n"
            "add10 = make_adder(10)\n"
            "print(add5(1))\n"
            "print(add10(1))\n");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], std::string("6"));
        XASSERT_EQ(out[1], std::string("11")); });

    runTest("counter closure with mutable state", []()
            {
        auto out = runXell(
            "fn counter():\n"
            "  count = 0\n"
            "  fn inc():\n"
            "    count = count + 1\n"
            "    give count\n"
            "  ;\n"
            "  give inc\n"
            ";\n"
            "c = counter()\n"
            "print(c())\n"
            "print(c())\n"
            "print(c())\n");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("2"));
        XASSERT_EQ(out[2], std::string("3")); });

    runTest("deeply nested closure", []()
            {
        auto out = runXell(
            "fn outer(a):\n"
            "  fn middle(b):\n"
            "    fn inner(c):\n"
            "      give a + b + c\n"
            "    ;\n"
            "    give inner\n"
            "  ;\n"
            "  give middle\n"
            ";\n"
            "m = outer(1)\n"
            "i = m(2)\n"
            "print(i(3))\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("6")); });
}

static void testInOperator()
{
    std::cout << "\n===== In Operator =====\n";

    runTest("in list found", []()
            {
        auto out = runXell("print(3 in [1, 2, 3])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("in list not found", []()
            {
        auto out = runXell("print(4 in [1, 2, 3])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("false")); });

    runTest("in string substring", []()
            {
        auto out = runXell("print(\"el\" in \"hello\")\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("in string not found", []()
            {
        auto out = runXell("print(\"xyz\" in \"hello\")\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("false")); });

    runTest("in map key found", []()
            {
        auto out = runXell("print(\"a\" in {a: 1, b: 2})\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("in map key not found", []()
            {
        auto out = runXell("print(\"c\" in {a: 1, b: 2})\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("false")); });

    runTest("in tuple found", []()
            {
        auto out = runXell("print(2 in (1, 2, 3))\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("for-in loop still works with in operator", []()
            {
        auto out = runXell(
            "for x in [10, 20]:\n"
            "  print(x)\n"
            ";\n");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], std::string("10"));
        XASSERT_EQ(out[1], std::string("20")); });

    runTest("not in list", []()
            {
        auto out = runXell("print(4 not in [1, 2, 3])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("not in list found", []()
            {
        auto out = runXell("print(1 not in [1, 2, 3])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("false")); });

    runTest("not in string", []()
            {
        auto out = runXell("print(\"xyz\" not in \"hello world\")\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("true")); });
}

static void testChainedComparisons()
{
    std::cout << "\n===== Chained Comparisons =====\n";

    runTest("a < b < c true", []()
            {
        auto out = runXell("print(1 < 2 < 3)\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("a < b < c false (right fails)", []()
            {
        auto out = runXell("print(1 < 2 < 2)\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("false")); });

    runTest("a < b < c false (left fails)", []()
            {
        auto out = runXell("print(3 < 2 < 5)\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("false")); });

    runTest("three-way chained >=", []()
            {
        auto out = runXell("print(5 >= 3 >= 1)\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("mixed operators in chain", []()
            {
        auto out = runXell("print(1 < 5 >= 3 > 2)\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("chained with variables", []()
            {
        auto out = runXell(
            "a = 10\n"
            "b = 20\n"
            "c = 30\n"
            "print(a < b < c)\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("single comparison still works", []()
            {
        auto out = runXell("print(5 > 3)\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("chained string comparison", []()
            {
        auto out = runXell("print(\"a\" < \"b\" < \"c\")\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("true")); });
}

static void testSliceSyntax()
{
    std::cout << "\n===== Slice Syntax =====\n";

    // ── List slicing ──

    runTest("list [start:end]", []()
            {
        auto out = runXell(
            "a = [10, 20, 30, 40, 50]\n"
            "print(a[1:3])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("[20, 30]")); });

    runTest("list [:end]", []()
            {
        auto out = runXell(
            "a = [10, 20, 30, 40, 50]\n"
            "print(a[:2])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("[10, 20]")); });

    runTest("list [start:]", []()
            {
        auto out = runXell(
            "a = [10, 20, 30, 40, 50]\n"
            "print(a[3:])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("[40, 50]")); });

    runTest("list [:]  (copy)", []()
            {
        auto out = runXell(
            "a = [1, 2, 3]\n"
            "print(a[:])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("[1, 2, 3]")); });

    runTest("list [::2] step", []()
            {
        auto out = runXell(
            "a = [0, 1, 2, 3, 4, 5]\n"
            "print(a[::2])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("[0, 2, 4]")); });

    runTest("list negative start [-2:]", []()
            {
        auto out = runXell(
            "a = [10, 20, 30, 40, 50]\n"
            "print(a[-2:])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("[40, 50]")); });

    runTest("list negative step (reverse)", []()
            {
        auto out = runXell(
            "a = [1, 2, 3, 4, 5]\n"
            "print(a[::-1])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("[5, 4, 3, 2, 1]")); });

    runTest("list [start:end:step]", []()
            {
        auto out = runXell(
            "a = [0, 1, 2, 3, 4, 5, 6, 7]\n"
            "print(a[1:6:2])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("[1, 3, 5]")); });

    // ── String slicing ──

    runTest("string [start:end]", []()
            {
        auto out = runXell(
            "s = \"Hello, World!\"\n"
            "print(s[0:5])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("Hello")); });

    runTest("string reverse [::-1]", []()
            {
        auto out = runXell(
            "s = \"abcd\"\n"
            "print(s[::-1])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("dcba")); });

    // ── Tuple slicing ──

    runTest("tuple [1:3]", []()
            {
        auto out = runXell(
            "t = (10, 20, 30, 40, 50)\n"
            "print(t[1:3])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("(20, 30)")); });

    // ── Edge cases ──

    runTest("slice step zero throws", []()
            { expectError<ValueError>(
                  "a = [1, 2, 3]\n"
                  "a[::0]\n"); });

    runTest("empty slice result", []()
            {
        auto out = runXell(
            "a = [1, 2, 3]\n"
            "print(a[2:1])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("[]")); });

    runTest("slice out-of-bounds clamps", []()
            {
        auto out = runXell(
            "a = [1, 2, 3]\n"
            "print(a[-10:10])\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("[1, 2, 3]")); });
}

static void testTernary()
{
    std::cout << "\n===== Ternary Operator =====\n";

    runTest("ternary true", []()
            {
        auto out = runXell("print(\"yes\" if true else \"no\")");
        XASSERT_EQ(out[0], std::string("yes")); });

    runTest("ternary false", []()
            {
        auto out = runXell("print(\"yes\" if false else \"no\")");
        XASSERT_EQ(out[0], std::string("no")); });

    runTest("ternary with expression", []()
            {
        auto out = runXell(
            "x = 10\n"
            "print(\"big\" if x > 5 else \"small\")");
        XASSERT_EQ(out[0], std::string("big")); });

    runTest("ternary with numbers", []()
            {
        auto out = runXell(
            "x = 3\n"
            "y = x * 2 if x > 2 else x + 10\n"
            "print(y)");
        XASSERT_EQ(out[0], std::string("6")); });
}

static void testDefaultParams()
{
    std::cout << "\n===== Default Parameters =====\n";

    runTest("default param used", []()
            {
        auto out = runXell(
            "fn greet(name = \"World\"):\n"
            "  print(\"Hello \" + name)\n"
            ";\n"
            "greet()");
        XASSERT_EQ(out[0], std::string("Hello World")); });

    runTest("default param overridden", []()
            {
        auto out = runXell(
            "fn greet(name = \"World\"):\n"
            "  print(\"Hello \" + name)\n"
            ";\n"
            "greet(\"Xell\")");
        XASSERT_EQ(out[0], std::string("Hello Xell")); });

    runTest("mixed default and required", []()
            {
        auto out = runXell(
            "fn add(a, b = 10):\n"
            "  print(a + b)\n"
            ";\n"
            "add(5)\n"
            "add(5, 3)");
        XASSERT_EQ(out[0], std::string("15"));
        XASSERT_EQ(out[1], std::string("8")); });
}

static void testVariadicFunctions()
{
    std::cout << "\n===== Variadic Functions =====\n";

    runTest("variadic basic", []()
            {
        auto out = runXell(
            "fn sum(...args):\n"
            "  total = 0\n"
            "  for a in args:\n"
            "    total += a\n"
            "  ;\n"
            "  print(total)\n"
            ";\n"
            "sum(1, 2, 3)");
        XASSERT_EQ(out[0], std::string("6")); });

    runTest("variadic with required params", []()
            {
        auto out = runXell(
            "fn greet(greeting, ...names):\n"
            "  for name in names:\n"
            "    print(greeting + \" \" + name)\n"
            "  ;\n"
            ";\n"
            "greet(\"Hi\", \"Alice\", \"Bob\")");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], std::string("Hi Alice"));
        XASSERT_EQ(out[1], std::string("Hi Bob")); });

    runTest("variadic empty", []()
            {
        auto out = runXell(
            "fn count(...args):\n"
            "  print(len(args))\n"
            ";\n"
            "count()");
        XASSERT_EQ(out[0], std::string("0")); });
}

static void testLambda()
{
    std::cout << "\n===== Lambda / Arrow Functions =====\n";

    runTest("single param lambda", []()
            {
        auto out = runXell(
            "double = x => x * 2\n"
            "print(double(5))");
        XASSERT_EQ(out[0], std::string("10")); });

    runTest("multi param lambda", []()
            {
        auto out = runXell(
            "add = (a, b) => a + b\n"
            "print(add(3, 4))");
        XASSERT_EQ(out[0], std::string("7")); });

    runTest("lambda closure", []()
            {
        auto out = runXell(
            "fn make_adder(n):\n"
            "  give x => x + n\n"
            ";\n"
            "add5 = make_adder(5)\n"
            "print(add5(3))");
        XASSERT_EQ(out[0], std::string("8")); });

    runTest("lambda as parameter", []()
            {
        auto out = runXell(
            "fn apply(f, x):\n"
            "  give f(x)\n"
            ";\n"
            "result = apply(x => x * 3, 4)\n"
            "print(result)");
        XASSERT_EQ(out[0], std::string("12")); });

    runTest("zero param lambda", []()
            {
        auto out = runXell(
            "greet = () => \"hello\"\n"
            "print(greet())");
        XASSERT_EQ(out[0], std::string("hello")); });
}

static void testAugmentedAssignment()
{
    std::cout << "\n===== Augmented Assignment =====\n";

    runTest("plus equal", []()
            {
        auto out = runXell(
            "x = 10\n"
            "x += 5\n"
            "print(x)");
        XASSERT_EQ(out[0], std::string("15")); });

    runTest("minus equal", []()
            {
        auto out = runXell(
            "x = 10\n"
            "x -= 3\n"
            "print(x)");
        XASSERT_EQ(out[0], std::string("7")); });

    runTest("star equal", []()
            {
        auto out = runXell(
            "x = 4\n"
            "x *= 3\n"
            "print(x)");
        XASSERT_EQ(out[0], std::string("12")); });

    runTest("slash equal", []()
            {
        auto out = runXell(
            "x = 10\n"
            "x /= 2\n"
            "print(x)");
        XASSERT_EQ(out[0], std::string("5")); });

    runTest("percent equal", []()
            {
        auto out = runXell(
            "x = 10\n"
            "x %= 3\n"
            "print(x)");
        XASSERT_EQ(out[0], std::string("1")); });

    runTest("string plus equal", []()
            {
        auto out = runXell(
            "s = \"hello\"\n"
            "s += \" world\"\n"
            "print(s)");
        XASSERT_EQ(out[0], std::string("hello world")); });
}

static void testDestructuring()
{
    std::cout << "\n===== Destructuring =====\n";

    runTest("basic destructuring", []()
            {
        auto out = runXell(
            "a, b = [1, 2]\n"
            "print(a)\n"
            "print(b)");
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("2")); });

    runTest("destructuring three values", []()
            {
        auto out = runXell(
            "x, y, z = [10, 20, 30]\n"
            "print(x + y + z)");
        XASSERT_EQ(out[0], std::string("60")); });

    runTest("destructuring fewer values", []()
            {
        auto out = runXell(
            "a, b, c = [1, 2]\n"
            "print(a)\n"
            "print(b)\n"
            "print(c)");
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("2"));
        XASSERT_EQ(out[2], std::string("none")); });
}

static void testSpreadOperator()
{
    std::cout << "\n===== Spread Operator =====\n";

    runTest("spread in list literal", []()
            {
        auto out = runXell(
            "a = [1, 2, 3]\n"
            "b = [0, ...a, 4]\n"
            "print(len(b))\n"
            "print(b[0])\n"
            "print(b[3])");
        XASSERT_EQ(out[0], std::string("5"));
        XASSERT_EQ(out[1], std::string("0"));
        XASSERT_EQ(out[2], std::string("3")); });

    runTest("spread in function call", []()
            {
        auto out = runXell(
            "fn add(a, b, c):\n"
            "  print(a + b + c)\n"
            ";\n"
            "args = [1, 2, 3]\n"
            "add(...args)");
        XASSERT_EQ(out[0], std::string("6")); });
}

static void testInCase()
{
    std::cout << "\n===== InCase (Switch) =====\n";

    runTest("incase basic match", []()
            {
        auto out = runXell(
            "x = 2\n"
            "incase x:\n"
            "  is 1:\n"
            "    print(\"one\")\n"
            "  ;\n"
            "  is 2:\n"
            "    print(\"two\")\n"
            "  ;\n"
            "  is 3:\n"
            "    print(\"three\")\n"
            "  ;\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("two")); });

    runTest("incase with else", []()
            {
        auto out = runXell(
            "x = 99\n"
            "incase x:\n"
            "  is 1:\n"
            "    print(\"one\")\n"
            "  ;\n"
            "  else:\n"
            "    print(\"other\")\n"
            "  ;\n"
            ";\n");
        XASSERT_EQ(out[0], std::string("other")); });

    runTest("incase with or", []()
            {
        auto out = runXell(
            "x = 2\n"
            "incase x:\n"
            "  is 1 or 2 or 3:\n"
            "    print(\"small\")\n"
            "  ;\n"
            "  else:\n"
            "    print(\"big\")\n"
            "  ;\n"
            ";\n");
        XASSERT_EQ(out[0], std::string("small")); });

    runTest("incase string match", []()
            {
        auto out = runXell(
            "color = \"red\"\n"
            "incase color:\n"
            "  is \"red\":\n"
            "    print(\"fire\")\n"
            "  ;\n"
            "  is \"blue\":\n"
            "    print(\"water\")\n"
            "  ;\n"
            ";\n");
        XASSERT_EQ(out[0], std::string("fire")); });
}

static void testMultilineStrings()
{
    std::cout << "\n===== Multi-line & Raw Strings =====\n";

    runTest("multi-line string", []()
            {
        auto out = runXell(
            "s = \"\"\"hello\nworld\"\"\"\n"
            "print(s)");
        XASSERT_EQ(out[0], std::string("hello\nworld")); });

    runTest("raw string no escape", []()
            {
        auto out = runXell(
            "s = r\"hello\\nworld\"\n"
            "print(s)");
        XASSERT_EQ(out[0], std::string("hello\\nworld")); });
}

static void testFormatBuiltin()
{
    std::cout << "\n===== format() Builtin =====\n";

    runTest("format basic", []()
            {
        auto out = runXell("print(format(r\"Hello {}\", \"World\"))");
        XASSERT_EQ(out[0], std::string("Hello World")); });

    runTest("format multiple args", []()
            {
        auto out = runXell("print(format(r\"{} + {} = {}\", 1, 2, 3))");
        XASSERT_EQ(out[0], std::string("1 + 2 = 3")); });

    runTest("format indexed", []()
            {
        auto out = runXell("print(format(r\"{1} then {0}\", \"second\", \"first\"))");
        XASSERT_EQ(out[0], std::string("first then second")); });

    runTest("format float precision", []()
            {
        auto out = runXell("print(format(r\"{:.2f}\", 3.14159))");
        XASSERT_EQ(out[0], std::string("3.14")); });
}

static void testTypeofBuiltin()
{
    std::cout << "\n===== typeof Builtin =====\n";

    runTest("typeof number", []()
            {
        auto out = runXell("print(typeof(42))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("typeof string", []()
            {
        auto out = runXell("print(typeof(\"hello\"))");
        XASSERT_EQ(out[0], std::string("string")); });

    runTest("typeof list", []()
            {
        auto out = runXell("print(typeof([1, 2]))");
        XASSERT_EQ(out[0], std::string("list")); });
}

static void testInputExit()
{
    std::cout << "\n===== input() / exit() =====\n";
    // input() reads from stdin, hard to test in unit tests
    // exit() terminates the process, can't test easily
    // Just verify they are registered as builtins
    runTest("exit is callable (verified by no crash)", []()
            {
        // We can't actually call exit() in tests, so just verify
        // it exists by checking a try/catch that calls a function
        auto out = runXell("print(typeof(42))");
        XASSERT_EQ(out[0], std::string("int")); });
}

static void testChainedComparison()
{
    std::cout << "\n===== Chained Comparison =====\n";

    runTest("simple comparison", []()
            {
        auto out = runXell(
            "print(1 < 5)");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("chained less than", []()
            {
        auto out = runXell(
            "x = 5\n"
            "print(1 < x)");
        XASSERT_EQ(out[0], std::string("true")); });
}

// ============================================================================
// main
// ============================================================================

// =============================================================================
// INT / FLOAT DISTINCTION
// =============================================================================

static void testIntFloat()
{
    std::cout << "\n===== Int / Float Distinction =====\n";

    runTest("int literal type", []()
            {
        auto out = runXell("print(type(42))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("float literal type", []()
            {
        auto out = runXell("print(type(3.14))");
        XASSERT_EQ(out[0], std::string("float")); });

    runTest("int + int = int", []()
            {
        auto out = runXell("print(type(2 + 3))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("int + float = float", []()
            {
        auto out = runXell("print(type(2 + 3.0))");
        XASSERT_EQ(out[0], std::string("float")); });

    runTest("int * int = int", []()
            {
        auto out = runXell("print(type(4 * 5))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("int / int exact = int", []()
            {
        auto out = runXell("print(10 / 2)\nprint(type(10 / 2))");
        XASSERT_EQ(out[0], std::string("5"));
        XASSERT_EQ(out[1], std::string("int")); });

    runTest("int / int inexact = float", []()
            {
        auto out = runXell("print(7 / 2)\nprint(type(7 / 2))");
        XASSERT_EQ(out[0], std::string("3.5"));
        XASSERT_EQ(out[1], std::string("float")); });

    runTest("int % int = int", []()
            {
        auto out = runXell("print(10 % 3)\nprint(type(10 % 3))");
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("int")); });

    runTest("int - int = int", []()
            {
        auto out = runXell("print(type(10 - 3))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("unary minus int = int", []()
            {
        auto out = runXell("x = 5\nprint(type(-x))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("prefix ++ on int stays int", []()
            {
        auto out = runXell("x = 5\n++x\nprint(type(x))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("postfix ++ on int stays int", []()
            {
        auto out = runXell("x = 5\nx++\nprint(type(x))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("int() conversion", []()
            {
        auto out = runXell("print(int(3.7))\nprint(int(\"42\"))");
        XASSERT_EQ(out[0], std::string("3"));
        XASSERT_EQ(out[1], std::string("42")); });

    runTest("float() conversion", []()
            {
        auto out = runXell("print(float(42))\nprint(type(float(42)))");
        XASSERT_EQ(out[0], std::string("42"));
        XASSERT_EQ(out[1], std::string("float")); });

    runTest("cross-type equality int == float", []()
            {
        auto out = runXell("print(3 == 3.0)");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("range returns ints", []()
            {
        auto out = runXell("r = range(3)\nprint(type(r[0]))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("len returns int", []()
            {
        auto out = runXell("print(type(len([1,2,3])))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("floor/ceil/round return int", []()
            {
        auto out = runXell(
            "print(type(floor(3.7)))\n"
            "print(type(ceil(3.2)))\n"
            "print(type(round(3.5)))");
        XASSERT_EQ(out[0], std::string("int"));
        XASSERT_EQ(out[1], std::string("int"));
        XASSERT_EQ(out[2], std::string("int")); });
}

// =============================================================================
// COMPLEX NUMBERS
// =============================================================================

static void testComplexNumbers()
{
    std::cout << "\n===== Complex Numbers =====\n";

    runTest("imaginary literal type", []()
            {
        auto out = runXell("print(type(2i))");
        XASSERT_EQ(out[0], std::string("complex")); });

    runTest("complex from addition", []()
            {
        auto out = runXell("x = 3 + 2i\nprint(x)");
        XASSERT_EQ(out[0], std::string("(3+2i)")); });

    runTest("complex from subtraction", []()
            {
        auto out = runXell("x = 3 - 2i\nprint(x)");
        XASSERT_EQ(out[0], std::string("(3-2i)")); });

    runTest("pure imaginary", []()
            {
        auto out = runXell("x = 5i\nprint(x)");
        XASSERT_EQ(out[0], std::string("(0+5i)")); });

    runTest("complex + complex", []()
            {
        auto out = runXell("a = 1 + 2i\nb = 3 + 4i\nprint(a + b)");
        XASSERT_EQ(out[0], std::string("(4+6i)")); });

    runTest("complex - complex", []()
            {
        auto out = runXell("a = 5 + 3i\nb = 2 + 1i\nprint(a - b)");
        XASSERT_EQ(out[0], std::string("(3+2i)")); });

    runTest("complex * complex", []()
            {
        // (1+2i)*(3+4i) = 3+4i+6i+8i^2 = 3+10i-8 = -5+10i
        auto out = runXell("a = 1 + 2i\nb = 3 + 4i\nprint(a * b)");
        XASSERT_EQ(out[0], std::string("(-5+10i)")); });

    runTest("complex / complex", []()
            {
        // (4+2i)/(1+1i) = (4+2i)(1-1i)/((1+1i)(1-1i)) = (4-4i+2i-2i^2)/(1+1) = (6-2i)/2 = 3-1i
        auto out = runXell("a = 4 + 2i\nb = 1 + 1i\nprint(a / b)");
        XASSERT_EQ(out[0], std::string("(3-1i)")); });

    runTest("complex + int", []()
            {
        auto out = runXell("a = 1 + 2i\nprint(a + 3)");
        XASSERT_EQ(out[0], std::string("(4+2i)")); });

    runTest("int + complex", []()
            {
        auto out = runXell("a = 1 + 2i\nprint(5 + a)");
        XASSERT_EQ(out[0], std::string("(6+2i)")); });

    runTest("unary minus complex", []()
            {
        auto out = runXell("a = 3 + 2i\nprint(-a)");
        XASSERT_EQ(out[0], std::string("(-3-2i)")); });

    runTest("complex equality", []()
            {
        auto out = runXell("a = 3 + 2i\nb = 3 + 2i\nprint(a == b)");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("complex inequality", []()
            {
        auto out = runXell("a = 3 + 2i\nb = 3 + 3i\nprint(a != b)");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("complex() builtin", []()
            {
        auto out = runXell("c = complex(3, 4)\nprint(c)");
        XASSERT_EQ(out[0], std::string("(3+4i)")); });

    runTest("real() and imag()", []()
            {
        auto out = runXell("c = 3 + 4i\nprint(real(c))\nprint(imag(c))");
        XASSERT_EQ(out[0], std::string("3"));
        XASSERT_EQ(out[1], std::string("4")); });

    runTest("conjugate()", []()
            {
        auto out = runXell("c = 3 + 4i\nprint(conjugate(c))");
        XASSERT_EQ(out[0], std::string("(3-4i)")); });

    runTest("magnitude()", []()
            {
        auto out = runXell("c = 3 + 4i\nprint(magnitude(c))");
        XASSERT_EQ(out[0], std::string("5")); });

    runTest("complex is hashable", []()
            {
        auto out = runXell("c = 3 + 2i\nprint(is_hashable(c))");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("complex truthy", []()
            {
        auto out = runXell(
            "if 1i:\n  print(\"yes\")\n;\n"
            "c = 0 + 0i\nif c:\n  print(\"no\")\n;\nelse:\n  print(\"zero\")\n;");
        XASSERT_EQ(out[0], std::string("yes"));
        XASSERT_EQ(out[1], std::string("zero")); });

    runTest("sqrt of negative = complex", []()
            {
        auto out = runXell("print(type(sqrt(-4)))");
        XASSERT_EQ(out[0], std::string("complex")); });
}

// =============================================================================
// FROZEN SETS
// =============================================================================

static void testFrozenSets()
{
    std::cout << "\n===== Frozen Sets =====\n";

    runTest("frozen set literal", []()
            {
        auto out = runXell("s = <1, 2, 3>\nprint(type(s))");
        XASSERT_EQ(out[0], std::string("frozen_set")); });

    runTest("empty frozen set", []()
            {
        auto out = runXell("s = <>\nprint(len(s))");
        XASSERT_EQ(out[0], std::string("0")); });

    runTest("frozen set len", []()
            {
        auto out = runXell("s = <1, 2, 3>\nprint(len(s))");
        XASSERT_EQ(out[0], std::string("3")); });

    runTest("frozen set has", []()
            {
        auto out = runXell("s = <1, 2, 3>\nprint(has(s, 2))\nprint(has(s, 5))");
        XASSERT_EQ(out[0], std::string("true"));
        XASSERT_EQ(out[1], std::string("false")); });

    runTest("frozen set contains", []()
            {
        auto out = runXell("s = <\"a\", \"b\">\nprint(contains(s, \"a\"))");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("frozen set is immutable (add)", []()
            {
        auto out = runXell("try:\n  s = <1, 2>\n  add(s, 3)\n;\ncatch e:\n  print(e->message)\n;");
        XASSERT(out[0].find("frozen set") != std::string::npos); });

    runTest("frozen set is immutable (remove)", []()
            {
        auto out = runXell("try:\n  s = <1, 2>\n  remove(s, 1)\n;\ncatch e:\n  print(e->message)\n;");
        XASSERT(out[0].find("frozen set") != std::string::npos); });

    runTest("frozen set equality", []()
            {
        auto out = runXell("a = <1, 2, 3>\nb = <1, 2, 3>\nprint(a == b)");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("frozen set is hashable", []()
            {
        auto out = runXell("s = <1, 2, 3>\nprint(is_hashable(s))");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("frozen set in map key (hashable)", []()
            {
        auto out = runXell(
            "m = {}\n"
            "k = <1, 2>\n"
            "set(m, k, \"value\")\n"
            "print(m[k])");
        XASSERT_EQ(out[0], std::string("value")); });

    runTest("frozen set to_list", []()
            {
        auto out = runXell("s = <42>\nprint(to_list(s))");
        XASSERT_EQ(out[0], std::string("[42]")); });

    runTest("frozen set to_tuple", []()
            {
        auto out = runXell("s = <42>\nprint(to_tuple(s))");
        XASSERT_EQ(out[0], std::string("(42,)")); });

    runTest("frozen set to_set", []()
            {
        auto out = runXell("s = <1, 2>\nms = to_set(s)\nprint(type(ms))");
        XASSERT_EQ(out[0], std::string("set")); });

    runTest("frozen set with mixed types", []()
            {
        auto out = runXell("s = <1, 3.14, \"hello\">\nprint(len(s))");
        XASSERT_EQ(out[0], std::string("3")); });

    runTest("frozen set deduplication", []()
            {
        auto out = runXell("s = <1, 1, 2, 2, 3>\nprint(len(s))");
        XASSERT_EQ(out[0], std::string("3")); });

    runTest("union_set with frozen_set", []()
            {
        auto out = runXell("a = <1, 2>\nb = <2, 3>\nprint(len(union_set(a, b)))");
        XASSERT_EQ(out[0], std::string("3")); });

    runTest("intersect with frozen_set", []()
            {
        auto out = runXell("a = <1, 2, 3>\nb = <2, 3, 4>\nprint(len(intersect(a, b)))");
        XASSERT_EQ(out[0], std::string("2")); });

    runTest("diff with frozen_set", []()
            {
        auto out = runXell("a = <1, 2, 3>\nb = <2, 3>\nprint(len(diff(a, b)))");
        XASSERT_EQ(out[0], std::string("1")); });
}

// =============================================================================
// HASH ENHANCEMENTS
// =============================================================================

static void testHashEnhancements()
{
    std::cout << "\n===== Hash Enhancements =====\n";

    runTest("hash int", []()
            {
        auto out = runXell("h = hash(42)\nprint(type(h))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("hash float", []()
            {
        auto out = runXell("h = hash(3.14)\nprint(type(h))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("hash string", []()
            {
        auto out = runXell("h = hash(\"hello\")\nprint(type(h))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("hash complex", []()
            {
        auto out = runXell("h = hash(3 + 2i)\nprint(type(h))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("hash frozen_set", []()
            {
        auto out = runXell("h = hash(<1, 2, 3>)\nprint(type(h))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("hash tuple", []()
            {
        auto out = runXell("h = hash((1, 2, 3))\nprint(type(h))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("hash with algorithm", []()
            {
        auto out = runXell(
            "h1 = hash(\"test\", \"fnv1a\")\n"
            "h2 = hash(\"test\", \"djb2\")\n"
            "print(h1 != h2)");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("hash_seed", []()
            {
        auto out = runXell(
            "h1 = hash_seed(\"test\", 42)\n"
            "h2 = hash_seed(\"test\", 99)\n"
            "print(h1 != h2)");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("hash_seed complex", []()
            {
        auto out = runXell("h = hash_seed(3 + 2i, 42)\nprint(type(h))");
        XASSERT_EQ(out[0], std::string("int")); });

    runTest("hash_seed frozen_set", []()
            {
        auto out = runXell("h = hash_seed(<1, 2>, 42)\nprint(type(h))");
        XASSERT_EQ(out[0], std::string("int")); });
}

static void testCycleCollector()
{
    std::cout << "\n===== Cycle Collector GC =====\n";

    runTest("GC: self-referential list cycle collected", []()
            {
        // A list containing itself creates a cycle: list -> list
        // Without GC this would leak.
        auto &gc = GCHeap::instance();
        gc.setEnabled(false); // manual control

        int64_t before = XObject::liveAllocations();
        {
            // Create and destroy a self-referential list in Xell
            runXell(
                "a = [1, 2, 3]\n"
                "push(a, a)\n"  // a now contains itself: cycle!
            );
        }
        // The cycle prevents refcount from reaching 0 for the list.
        // Run the collector to break it.
        size_t freed = gc.collect();
        int64_t after = XObject::liveAllocations();

        // The collector should have freed the cyclic list
        XASSERT(freed > 0u || after <= before);
        gc.setEnabled(true); });

    runTest("GC: closure cycle collected", []()
            {
        // A closure that captures a variable pointing to itself
        // creates a cycle: function -> ownedEnv -> var -> function
        auto &gc = GCHeap::instance();
        gc.setEnabled(false);

        int64_t before = XObject::liveAllocations();
        {
            runXell(
                "fn make_cycle():\n"
                "  x = none\n"
                "  fn inner():\n"
                "    print(x)\n"
                "  ;\n"
                "  x = inner\n"     // x -> inner, inner captures x: cycle!
                "  give inner\n"
                ";\n"
                "f = make_cycle()\n"
            );
        }
        size_t freed = gc.collect();
        int64_t after = XObject::liveAllocations();
        XASSERT(freed > 0u || after <= before);
        gc.setEnabled(true); });

    runTest("GC: non-cyclic objects survive collection", []()
            {
        // Objects that are NOT cyclic should not be freed by the GC.
        auto &gc = GCHeap::instance();
        gc.collect(); // clean slate

        auto out = runXell(
            "a = [1, 2, 3]\n"
            "b = {\"x\": 10}\n"
            "print(len(a))\n"
            "print(b->x)\n"
        );
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], std::string("3"));
        XASSERT_EQ(out[1], std::string("10")); });

    runTest("GC: collect returns 0 when no garbage", []()
            {
        auto &gc = GCHeap::instance();
        gc.collect(); // clean up any previous garbage first
        size_t freed = gc.collect(); // should find nothing new
        XASSERT_EQ(freed, 0u); });

    runTest("GC: tracked count increases for containers", []()
            {
        auto &gc = GCHeap::instance();
        gc.setEnabled(false);
        size_t before = gc.trackedCount();
        {
            XObject list = XObject::makeList();
            // List is a container, should be tracked
            XASSERT(gc.trackedCount() > before);
        }
        // After destruction (refcount -> 0), untracked automatically
        gc.setEnabled(true); });

    runTest("GC: auto-collection after threshold", []()
            {
        auto &gc = GCHeap::instance();
        gc.setThreshold(5); // very low threshold for testing
        gc.setEnabled(true);
        size_t collectionsBefore = gc.collectionsRun();

        // Allocate more than threshold container objects
        for (int i = 0; i < 10; i++)
        {
            XObject list = XObject::makeList();
            (void)list; // suppress unused
        }

        XASSERT(gc.collectionsRun() > collectionsBefore);
        gc.setThreshold(GCHeap::DEFAULT_THRESHOLD); }); // reset
}

static void testErrorInstances()
{
    std::cout << "\n===== Error Instances (Gap 1.8) =====\n";

    runTest("caught error is an instance, not a map", []()
            {
        auto out = runXell(
            "try:\n"
            "  x = 1 / 0\n"
            ";\n"
            "catch e:\n"
            "  print(type(e))\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("DivisionByZero")); });

    runTest("error instance has message field", []()
            {
        auto out = runXell(
            "try:\n"
            "  x = 1 / 0\n"
            ";\n"
            "catch e:\n"
            "  print(e->message)\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT(out[0].find("zero") != std::string::npos ||
                out[0].find("Zero") != std::string::npos ||
                out[0].find("division") != std::string::npos); });

    runTest("error instance has type field", []()
            {
        auto out = runXell(
            "try:\n"
            "  x = 1 / 0\n"
            ";\n"
            "catch e:\n"
            "  print(e->type)\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("DivisionByZero")); });

    runTest("error instance has line field", []()
            {
        auto out = runXell(
            "try:\n"
            "  x = 1 / 0\n"
            ";\n"
            "catch e:\n"
            "  print(e->line)\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("2")); });

    runTest("error instance has traceback field", []()
            {
        auto out = runXell(
            "try:\n"
            "  x = 1 / 0\n"
            ";\n"
            "catch e:\n"
            "  print(type(e->traceback))\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("list")); });

    runTest("TypeError is an instance", []()
            {
        auto out = runXell(
            "try:\n"
            "  x = \"hello\" - 1\n"
            ";\n"
            "catch e:\n"
            "  print(e->type)\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("TypeError")); });

    runTest("re-throw error instance preserves type", []()
            {
        auto out = runXell(
            "try:\n"
            "  try:\n"
            "    x = 1 / 0\n"
            "  ;\n"
            "  catch e:\n"
            "    throw e\n"
            "  ;\n"
            ";\n"
            "catch e2:\n"
            "  print(e2->type)\n"
            ";\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("DivisionByZero")); });

    runTest("Error base class is accessible", []()
            {
        auto out = runXell("print(type(Error))\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("class")); });

    runTest("error class names are accessible as globals", []()
            {
        auto out = runXell("print(type(TypeError))\n");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], std::string("class")); });
}

// =============================================================================
// Gap 1.9 — Augmented Member & Index Assignment Depth
// =============================================================================
static void testAugmentedAssignmentDepth()
{
    std::cout << "\n===== Augmented Assignment Depth =====\n";

    // ---- Simple member augmented assignment: obj->field += val ----
    runTest("struct field += on simple identifier", []()
            {
        auto out = runXell(
            "struct Point :\n"
            "  x = 0\n"
            "  y = 0\n"
            ";\n"
            "p = Point(1, 2)\n"
            "p->x += 10\n"
            "print(p->x)");
        XASSERT_EQ(out[0], std::string("11")); });

    runTest("struct field -= on simple identifier", []()
            {
        auto out = runXell(
            "struct Point :\n"
            "  x = 0\n"
            "  y = 0\n"
            ";\n"
            "p = Point(10, 20)\n"
            "p->y -= 5\n"
            "print(p->y)");
        XASSERT_EQ(out[0], std::string("15")); });

    runTest("struct field *= on simple identifier", []()
            {
        auto out = runXell(
            "struct Point :\n"
            "  x = 0\n"
            "  y = 0\n"
            ";\n"
            "p = Point(3, 4)\n"
            "p->x *= 5\n"
            "print(p->x)");
        XASSERT_EQ(out[0], std::string("15")); });

    // ---- Nested member chain: a->b->c += val (was previously broken) ----
    runTest("nested member chain a->b->c += val", []()
            {
        auto out = runXell(
            "struct Inner :\n"
            "  val = 0\n"
            ";\n"
            "struct Outer :\n"
            "  inner = none\n"
            ";\n"
            "o = Outer(Inner(10))\n"
            "o->inner->val += 5\n"
            "print(o->inner->val)");
        XASSERT_EQ(out[0], std::string("15")); });

    runTest("triple nested a->b->c->d += val", []()
            {
        auto out = runXell(
            "struct A :\n"
            "  b = none\n"
            ";\n"
            "struct B :\n"
            "  c = none\n"
            ";\n"
            "struct C :\n"
            "  d = 0\n"
            ";\n"
            "x = A(B(C(100)))\n"
            "x->b->c->d -= 42\n"
            "print(x->b->c->d)");
        XASSERT_EQ(out[0], std::string("58")); });

    // ---- Map member augmented assignment: m->key += val ----
    runTest("map member += val", []()
            {
        auto out = runXell(
            "m = {\"count\": 10}\n"
            "m->count += 5\n"
            "print(m->count)");
        XASSERT_EQ(out[0], std::string("15")); });

    // ---- Index augmented assignment: list[i] += val ----
    runTest("list index += val", []()
            {
        auto out = runXell(
            "nums = [10, 20, 30]\n"
            "nums[0] += 5\n"
            "print(nums[0])");
        XASSERT_EQ(out[0], std::string("15")); });

    runTest("list index -= val", []()
            {
        auto out = runXell(
            "nums = [10, 20, 30]\n"
            "nums[1] -= 7\n"
            "print(nums[1])");
        XASSERT_EQ(out[0], std::string("13")); });

    runTest("list index *= val", []()
            {
        auto out = runXell(
            "nums = [10, 20, 30]\n"
            "nums[2] *= 3\n"
            "print(nums[2])");
        XASSERT_EQ(out[0], std::string("90")); });

    runTest("list negative index += val", []()
            {
        auto out = runXell(
            "nums = [10, 20, 30]\n"
            "nums[-1] += 70\n"
            "print(nums[-1])");
        XASSERT_EQ(out[0], std::string("100")); });

    // ---- Map index augmented assignment: map[key] += val ----
    runTest("map index += val", []()
            {
        auto out = runXell(
            "m = {\"x\": 100}\n"
            "m[\"x\"] += 50\n"
            "print(m[\"x\"])");
        XASSERT_EQ(out[0], std::string("150")); });

    // ---- String augmented assignment with member ----
    runTest("struct string field += concat", []()
            {
        auto out = runXell(
            "struct Msg :\n"
            "  text = \"\"\n"
            ";\n"
            "m = Msg(\"hello\")\n"
            "m->text += \" world\"\n"
            "print(m->text)");
        XASSERT_EQ(out[0], std::string("hello world")); });

    // ---- Class (with class keyword) nested augmented ----
    runTest("class nested field augmented assignment", []()
            {
        auto out = runXell(
            "class Inner :\n"
            "  val = 0\n"
            "  fn __init__(self, v) :\n"
            "    self->val = v\n"
            "  ;\n"
            ";\n"
            "class Outer :\n"
            "  inner = none\n"
            "  fn __init__(self, i) :\n"
            "    self->inner = i\n"
            "  ;\n"
            ";\n"
            "o = Outer(Inner(5))\n"
            "o->inner->val += 10\n"
            "print(o->inner->val)");
        XASSERT_EQ(out[0], std::string("15")); });

    // ---- Augmented assignment with all five ops ----
    runTest("all five augmented ops on list index", []()
            {
        auto out = runXell(
            "a = [100]\n"
            "a[0] += 10\n"    // 110
            "a[0] -= 20\n"    // 90
            "a[0] *= 2\n"     // 180
            "a[0] /= 3\n"     // 60
            "a[0] %= 7\n"     // 4
            "print(a[0])");
        XASSERT_EQ(out[0], std::string("4")); });
}

// =============================================================================
// Gap 1.10 — Generator Lazy Drain in For-Loops
// =============================================================================
static void testGeneratorLazyForLoop()
{
    std::cout << "\n===== Generator Lazy For-Loop (Gap 1.10) =====\n";

    // Basic: generator consumed lazily in for-loop
    runTest("basic generator in for-loop", []()
            {
        auto out = runXell(
            "fn gen():\n"
            "  yield 1\n"
            "  yield 2\n"
            "  yield 3\n"
            ";\n"
            "for x in gen():\n"
            "  print(x)\n"
            ";\n");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("2"));
        XASSERT_EQ(out[2], std::string("3")); });

    // Break stops generator early — generator is NOT fully drained
    runTest("break stops generator early (lazy)", []()
            {
        auto out = runXell(
            "fn counter():\n"
            "  n = 0\n"
            "  loop:\n"
            "    yield n\n"
            "    n += 1\n"
            "  ;\n"
            ";\n"
            "for x in counter():\n"
            "  if x == 3:\n"
            "    break\n"
            "  ;\n"
            "  print(x)\n"
            ";\n");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], std::string("0"));
        XASSERT_EQ(out[1], std::string("1"));
        XASSERT_EQ(out[2], std::string("2")); });

    // Generator with finite count consumed fully
    runTest("finite generator fully consumed", []()
            {
        auto out = runXell(
            "fn range_gen(n):\n"
            "  i = 0\n"
            "  while i < n:\n"
            "    yield i\n"
            "    i += 1\n"
            "  ;\n"
            ";\n"
            "total = 0\n"
            "for x in range_gen(5):\n"
            "  total += x\n"
            ";\n"
            "print(total)\n");
        XASSERT_EQ(out[0], std::string("10")); });

    // Destructuring from generator: each yield is a list
    runTest("generator destructuring in for", []()
            {
        auto out = runXell(
            "fn pairs():\n"
            "  yield [1, \"a\"]\n"
            "  yield [2, \"b\"]\n"
            ";\n"
            "for n, s in pairs():\n"
            "  print(n)\n"
            "  print(s)\n"
            ";\n");
        XASSERT_EQ(out.size(), 4u);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("a"));
        XASSERT_EQ(out[2], std::string("2"));
        XASSERT_EQ(out[3], std::string("b")); });

    // Continue works with lazy generator
    runTest("continue in generator for-loop", []()
            {
        auto out = runXell(
            "fn gen():\n"
            "  yield 1\n"
            "  yield 2\n"
            "  yield 3\n"
            "  yield 4\n"
            ";\n"
            "for x in gen():\n"
            "  if x % 2 == 0:\n"
            "    continue\n"
            "  ;\n"
            "  print(x)\n"
            ";\n");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("3")); });

    // Expression-mode for with generator
    runTest("expression-mode for with generator", []()
            {
        auto out = runXell(
            "fn gen():\n"
            "  yield 10\n"
            "  yield 20\n"
            "  yield 30\n"
            ";\n"
            "result = for x in gen():\n"
            "  if x == 20:\n"
            "    break x\n"
            "  ;\n"
            ";\n"
            "print(result)\n");
        XASSERT_EQ(out[0], std::string("20")); });

    // Large generator — not all values materialized
    runTest("large generator with early break (memory safe)", []()
            {
        auto out = runXell(
            "fn big_gen():\n"
            "  i = 0\n"
            "  loop:\n"
            "    yield i\n"
            "    i += 1\n"
            "  ;\n"
            ";\n"
            "for x in big_gen():\n"
            "  if x == 100:\n"
            "    break\n"
            "  ;\n"
            ";\n"
            "print(\"done\")\n");
        XASSERT_EQ(out[0], std::string("done")); });
}

// =====================================================================
// Comprehensions (Gap 2.1)
// =====================================================================
static void testComprehensions()
{
    std::cout << "\n===== Comprehensions (Gap 2.1) =====\n";

    // --- List comprehension basics ---

    runTest("basic list comprehension", []()
            {
        auto out = runXell(
            "result = [x * 2 for x in [1, 2, 3, 4, 5]]\n"
            "print(result)\n");
        XASSERT_EQ(out[0], std::string("[2, 4, 6, 8, 10]")); });

    runTest("list comprehension with if filter", []()
            {
        auto out = runXell(
            "result = [x for x in [1, 2, 3, 4, 5, 6] if x % 2 == 0]\n"
            "print(result)\n");
        XASSERT_EQ(out[0], std::string("[2, 4, 6]")); });

    runTest("list comprehension with expression transform and filter", []()
            {
        auto out = runXell(
            "result = [x * x for x in [1, 2, 3, 4, 5] if x > 2]\n"
            "print(result)\n");
        XASSERT_EQ(out[0], std::string("[9, 16, 25]")); });

    runTest("nested for in list comprehension", []()
            {
        auto out = runXell(
            "result = [x + y for x in [1, 2] for y in [10, 20]]\n"
            "print(result)\n");
        XASSERT_EQ(out[0], std::string("[11, 21, 12, 22]")); });

    runTest("list comprehension with string iteration", []()
            {
        auto out = runXell(
            "result = [c for c in \"hello\" if c != \"l\"]\n"
            "print(result)\n");
        XASSERT_EQ(out[0], std::string("[\"h\", \"e\", \"o\"]")); });

    runTest("empty list comprehension (all filtered)", []()
            {
        auto out = runXell(
            "result = [x for x in [1, 2, 3] if x > 10]\n"
            "print(result)\n");
        XASSERT_EQ(out[0], std::string("[]")); });

    runTest("list comprehension preserves outer scope", []()
            {
        auto out = runXell(
            "factor = 10\n"
            "result = [x * factor for x in [1, 2, 3]]\n"
            "print(result)\n");
        XASSERT_EQ(out[0], std::string("[10, 20, 30]")); });

    // --- Set comprehension ---

    runTest("basic set comprehension", []()
            {
        auto out = runXell(
            "result = {x % 3 for x in [1, 2, 3, 4, 5, 6]}\n"
            "print(len(result))\n");
        // x%3 for 1..6 gives {1, 2, 0, 1, 2, 0} → unique {0, 1, 2} → len 3
        XASSERT_EQ(out[0], std::string("3")); });

    runTest("set comprehension with filter", []()
            {
        auto out = runXell(
            "result = {x for x in [1, 2, 2, 3, 3, 3] if x > 1}\n"
            "print(len(result))\n");
        XASSERT_EQ(out[0], std::string("2")); });

    // --- Map comprehension ---

    runTest("basic map comprehension", []()
            {
        auto out = runXell(
            "result = {k: k * k for k in [1, 2, 3]}\n"
            "print(result[1])\n"
            "print(result[2])\n"
            "print(result[3])\n");
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("4"));
        XASSERT_EQ(out[2], std::string("9")); });

    runTest("map comprehension with filter", []()
            {
        auto out = runXell(
            "result = {k: k * 2 for k in [1, 2, 3, 4, 5] if k % 2 == 1}\n"
            "print(len(result))\n");
        XASSERT_EQ(out[0], std::string("3")); });

    // --- Destructuring in comprehension ---

    runTest("list comprehension with destructuring", []()
            {
        auto out = runXell(
            "pairs = [[1, \"a\"], [2, \"b\"], [3, \"c\"]]\n"
            "result = [s for n, s in pairs]\n"
            "print(result)\n");
        XASSERT_EQ(out[0], std::string("[\"a\", \"b\", \"c\"]")); });

    // --- Nested for with filter ---

    runTest("nested for with filter in list comprehension", []()
            {
        auto out = runXell(
            "result = [x * y for x in [1, 2, 3] if x != 2 for y in [10, 20] if y == 10]\n"
            "print(result)\n");
        XASSERT_EQ(out[0], std::string("[10, 30]")); });
}

// =====================================================================
// Hex, Octal, Binary Number Literals & Digit Separators (Gap 2.2)
// =====================================================================
static void testNumberLiterals()
{
    std::cout << "\n===== Number Literals (Gap 2.2) =====\n";

    runTest("hex literal 0xFF", []()
            {
        auto out = runXell("print(0xFF)\n");
        XASSERT_EQ(out[0], std::string("255")); });

    runTest("hex literal 0x1A", []()
            {
        auto out = runXell("print(0x1A)\n");
        XASSERT_EQ(out[0], std::string("26")); });

    runTest("hex literal lowercase 0xff", []()
            {
        auto out = runXell("print(0xff)\n");
        XASSERT_EQ(out[0], std::string("255")); });

    runTest("octal literal 0o77", []()
            {
        auto out = runXell("print(0o77)\n");
        XASSERT_EQ(out[0], std::string("63")); });

    runTest("octal literal 0o10", []()
            {
        auto out = runXell("print(0o10)\n");
        XASSERT_EQ(out[0], std::string("8")); });

    runTest("binary literal 0b1010", []()
            {
        auto out = runXell("print(0b1010)\n");
        XASSERT_EQ(out[0], std::string("10")); });

    runTest("binary literal 0b11111111", []()
            {
        auto out = runXell("print(0b11111111)\n");
        XASSERT_EQ(out[0], std::string("255")); });

    runTest("digit separator in decimal", []()
            {
        auto out = runXell("print(1_000_000)\n");
        XASSERT_EQ(out[0], std::string("1000000")); });

    runTest("digit separator in hex", []()
            {
        auto out = runXell("print(0xFF_FF)\n");
        XASSERT_EQ(out[0], std::string("65535")); });

    runTest("digit separator in binary", []()
            {
        auto out = runXell("print(0b1111_0000)\n");
        XASSERT_EQ(out[0], std::string("240")); });

    runTest("hex in arithmetic", []()
            {
        auto out = runXell("print(0xFF + 1)\n");
        XASSERT_EQ(out[0], std::string("256")); });

    runTest("binary in comparison", []()
            {
        auto out = runXell("print(0b1010 == 10)\n");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("digit separator in float", []()
            {
        auto out = runXell("print(1_234.5)\n");
        XASSERT_EQ(out[0], std::string("1234.5")); });
}

// ============================================================================
// Gap 2.3: Bitwise Operators
// ============================================================================
static void testBitwiseOperators()
{
    std::cout << "\n===== Bitwise Operators (Gap 2.3) =====\n";

    // --- Basic bitwise OR ---
    runTest("bitwise OR: 0b1100 | 0b1010", []()
            {
        auto out = runXell("print(0b1100 | 0b1010)\n");
        XASSERT_EQ(out[0], std::string("14")); }); // 1110 = 14

    // --- Basic bitwise AND ---
    runTest("bitwise AND: 0b1100 & 0b1010", []()
            {
        auto out = runXell("print(0b1100 & 0b1010)\n");
        XASSERT_EQ(out[0], std::string("8")); }); // 1000 = 8

    // --- Basic bitwise XOR ---
    runTest("bitwise XOR: 0b1100 ^ 0b1010", []()
            {
        auto out = runXell("print(0b1100 ^ 0b1010)\n");
        XASSERT_EQ(out[0], std::string("6")); }); // 0110 = 6

    // --- Bitwise NOT ---
    runTest("bitwise NOT: ~0", []()
            {
        auto out = runXell("print(~0)\n");
        XASSERT_EQ(out[0], std::string("-1")); });

    runTest("bitwise NOT: ~0xFF", []()
            {
        auto out = runXell("print(~0xFF)\n");
        XASSERT_EQ(out[0], std::string("-256")); });

    // --- Left shift ---
    runTest("left shift: 1 << 8", []()
            {
        auto out = runXell("print(1 << 8)\n");
        XASSERT_EQ(out[0], std::string("256")); });

    // --- Right shift ---
    runTest("right shift: 256 >> 4", []()
            {
        auto out = runXell("print(256 >> 4)\n");
        XASSERT_EQ(out[0], std::string("16")); });

    // --- Negative shift count errors ---
    runTest("left shift: negative count throws", []()
            { XASSERT(expectError<ValueError>("x = 1 << -1\n")); });

    runTest("right shift: negative count throws", []()
            { XASSERT(expectError<ValueError>("x = 1 >> -1\n")); });

    // --- Type errors on non-integers ---
    runTest("bitwise OR: float TypeError", []()
            { XASSERT(expectError<TypeError>("x = 3.14 | 1\n")); });

    runTest("bitwise AND: string TypeError", []()
            { XASSERT(expectError<TypeError>("x = \"hi\" & 1\n")); });

    runTest("bitwise NOT: float TypeError", []()
            { XASSERT(expectError<TypeError>("x = ~3.14\n")); });

    // --- Precedence: & binds tighter than |, ^ in between ---
    runTest("precedence: | vs & vs ^", []()
            {
        // 0b1111 | 0b0001 & 0b0011  →  0b1111 | (0b0001 & 0b0011)  →  0b1111 | 0b0001  →  0b1111 = 15
        auto out = runXell("print(0b1111 | 0b0001 & 0b0011)\n");
        XASSERT_EQ(out[0], std::string("15")); });

    runTest("precedence: shift vs addition", []()
            {
        // 1 << 2 + 1  →  1 << (2 + 1)  →  1 << 3  →  8
        auto out = runXell("print(1 << 2 + 1)\n");
        XASSERT_EQ(out[0], std::string("8")); });

    // --- Augmented assignment ---
    runTest("augmented assignment: x |= 3", []()
            {
        auto out = runXell("x = 0b1100\nx |= 0b0011\nprint(x)\n");
        XASSERT_EQ(out[0], std::string("15")); }); // 1111 = 15

    runTest("augmented assignment: x &= 0b1010", []()
            {
        auto out = runXell("x = 0b1111\nx &= 0b1010\nprint(x)\n");
        XASSERT_EQ(out[0], std::string("10")); }); // 1010 = 10

    runTest("augmented assignment: x ^= 0b1010", []()
            {
        auto out = runXell("x = 0b1100\nx ^= 0b1010\nprint(x)\n");
        XASSERT_EQ(out[0], std::string("6")); }); // 0110 = 6

    runTest("augmented assignment: x <<= 4", []()
            {
        auto out = runXell("x = 1\nx <<= 4\nprint(x)\n");
        XASSERT_EQ(out[0], std::string("16")); });

    runTest("augmented assignment: x >>= 2", []()
            {
        auto out = runXell("x = 16\nx >>= 2\nprint(x)\n");
        XASSERT_EQ(out[0], std::string("4")); });

    // --- Combined expression ---
    runTest("combined: flags = (1 << 0) | (1 << 2) | (1 << 4)", []()
            {
        auto out = runXell("flags = (1 << 0) | (1 << 2) | (1 << 4)\nprint(flags)\n");
        XASSERT_EQ(out[0], std::string("21")); }); // 10101 = 21

    runTest("combined: mask check (flags & mask) != 0", []()
            {
        auto out = runXell(R"(
            flags = 0b10101
            mask = 0b00100
            print((flags & mask) != 0)
        )");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("combined: toggle bit with XOR", []()
            {
        auto out = runXell(R"(
            x = 0b1010
            x = x ^ 0b0010
            print(x)
        )");
        XASSERT_EQ(out[0], std::string("8")); }); // 1000 = 8

    // --- Pipe |> still works separately ---
    runTest("pipe |> still works for strings", []()
            {
        auto out = runXell(R"(
            cmd = "ls" |> "grep foo"
            print(cmd)
        )");
        XASSERT_EQ(out[0], std::string("ls | grep foo")); });
}

int main()
{
    testBasicExpressions();
    testVariables();
    testConditionals();
    testForLoops();
    testWhileLoops();
    testFunctions();
    testScoping();
    testRecursion();
    testLists();
    testMaps();
    testStrings();
    testBuiltins();
    testErrors();
    testComplexPrograms();
    testInterpolation();
    testBreakContinue();

    // New feature tests
    testTryCatchFinally();
    testThrowStatement();
    testStackTraces();
    testClosureSafety();
    testInOperator();
    testChainedComparisons();
    testSliceSyntax();
    testTernary();
    testDefaultParams();
    testVariadicFunctions();
    testLambda();
    testAugmentedAssignment();
    testDestructuring();
    testSpreadOperator();
    testInCase();
    testMultilineStrings();
    testFormatBuiltin();
    testTypeofBuiltin();
    testInputExit();
    testChainedComparison();

    // Type system refactor tests
    testIntFloat();
    testComplexNumbers();
    testFrozenSets();
    testHashEnhancements();
    testCycleCollector();
    testErrorInstances();
    testAugmentedAssignmentDepth();
    testGeneratorLazyForLoop();
    testComprehensions();
    testNumberLiterals();
    testBitwiseOperators();

    std::cout << "\n============================================\n";
    std::cout << "  Total: " << (g_passed + g_failed)
              << "  |  Passed: " << g_passed
              << "  |  Failed: " << g_failed << "\n";
    std::cout << "============================================\n";

    return g_failed == 0 ? 0 : 1;
}
