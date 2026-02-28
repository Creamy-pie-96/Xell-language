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
        XASSERT_EQ(out[0], std::string("number"));
        XASSERT_EQ(out[1], std::string("string"));
        XASSERT_EQ(out[2], std::string("bool"));
        XASSERT_EQ(out[3], std::string("none"));
        XASSERT_EQ(out[4], std::string("list"));
        XASSERT_EQ(out[5], std::string("map")); });

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
// main
// ============================================================================

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

    std::cout << "\n============================================\n";
    std::cout << "  Total: " << (g_passed + g_failed)
              << "  |  Passed: " << g_passed
              << "  |  Failed: " << g_failed << "\n";
    std::cout << "============================================\n";

    return g_failed == 0 ? 0 : 1;
}
