// =============================================================================
// For-Loop Tests — Advanced iteration features
// =============================================================================
// Tests: multi-variable destructuring, parallel iteration, rest capture,
//        map/set/string/generator iteration, enumerate, range variants,
//        zip_longest, and edge cases.
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
// Section 1: Basic for-loop (backward compatibility)
// ============================================================================

static void testBasicFor()
{
    std::cout << "\n===== Basic For Loop =====\n";

    runTest("for over list", []()
            {
        auto out = runXell(
            "result = \"\"\n"
            "for x in [1, 2, 3] :\n"
            "  result = result + str(x)\n"
            ";\n"
            "print(result)");
        XASSERT_EQ(out[0], std::string("123")); });

    runTest("for with break", []()
            {
        auto out = runXell(
            "for x in [1, 2, 3, 4, 5] :\n"
            "  if x == 3 :\n"
            "    break\n"
            "  ;\n"
            "  print(x)\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("2")); });

    runTest("for with continue", []()
            {
        auto out = runXell(
            "for x in [1, 2, 3, 4] :\n"
            "  if x == 2 :\n"
            "    continue\n"
            "  ;\n"
            "  print(x)\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("3"));
        XASSERT_EQ(out[2], std::string("4")); });
}

// ============================================================================
// Section 2: Destructuring for-loop (single source, multiple targets)
// ============================================================================

static void testDestructuringFor()
{
    std::cout << "\n===== Destructuring For Loop =====\n";

    runTest("for name, age in people list", []()
            {
        auto out = runXell(
            "people = [[\"Prithu\", 22], [\"Nasif\", 23], [\"Sadik\", 21]]\n"
            "for name, age in people :\n"
            "  print(name + \" is \" + str(age))\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], std::string("Prithu is 22"));
        XASSERT_EQ(out[1], std::string("Nasif is 23"));
        XASSERT_EQ(out[2], std::string("Sadik is 21")); });

    runTest("for x, y in coords tuples", []()
            {
        auto out = runXell(
            "coords = [(10, 20), (30, 40), (50, 60)]\n"
            "for x, y in coords :\n"
            "  print(\"x=\" + str(x) + \" y=\" + str(y))\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], std::string("x=10 y=20"));
        XASSERT_EQ(out[1], std::string("x=30 y=40"));
        XASSERT_EQ(out[2], std::string("x=50 y=60")); });

    runTest("for a, b, c in 3-element lists", []()
            {
        auto out = runXell(
            "data = [[1, 2, 3], [4, 5, 6]]\n"
            "for a, b, c in data :\n"
            "  print(a + b + c)\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("6"));
        XASSERT_EQ(out[1], std::string("15")); });
}

// ============================================================================
// Section 3: Parallel iteration (multiple sources)
// ============================================================================

static void testParallelFor()
{
    std::cout << "\n===== Parallel For Loop =====\n";

    runTest("for name, score in names, scores", []()
            {
        auto out = runXell(
            "names = [\"Prithu\", \"Nasif\", \"Sadik\"]\n"
            "scores = [95, 87, 92]\n"
            "for name, score in names, scores :\n"
            "  print(name + \" scored \" + str(score))\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], std::string("Prithu scored 95"));
        XASSERT_EQ(out[1], std::string("Nasif scored 87"));
        XASSERT_EQ(out[2], std::string("Sadik scored 92")); });

    runTest("parallel for with unequal lengths (zip semantics)", []()
            {
        auto out = runXell(
            "a = [1, 2, 3, 4]\n"
            "b = [\"x\", \"y\"]\n"
            "for n, s in a, b :\n"
            "  print(str(n) + s)\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("1x"));
        XASSERT_EQ(out[1], std::string("2y")); });

    runTest("for nums, names in groups, labels", []()
            {
        auto out = runXell(
            "groups = [[1,2,3], [4,5,6]]\n"
            "labels = [[\"a\",\"b\",\"c\"], [\"d\",\"e\",\"f\"]]\n"
            "for nums, names in groups, labels :\n"
            "  print(str(len(nums)) + \" \" + str(len(names)))\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("3 3"));
        XASSERT_EQ(out[1], std::string("3 3")); });
}

// ============================================================================
// Section 4: Rest capture (...rest)
// ============================================================================

static void testRestCapture()
{
    std::cout << "\n===== Rest Capture For Loop =====\n";

    runTest("for first, ...rest in data", []()
            {
        auto out = runXell(
            "data = [[1, 2, 3, 4], [5, 6, 7, 8]]\n"
            "for first, ...rest in data :\n"
            "  print(\"first: \" + str(first) + \" rest: \" + str(rest))\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("first: 1 rest: [2, 3, 4]"));
        XASSERT_EQ(out[1], std::string("first: 5 rest: [6, 7, 8]")); });

    runTest("for a, b, ...rest in data", []()
            {
        auto out = runXell(
            "data = [[10, 20, 30, 40, 50]]\n"
            "for a, b, ...rest in data :\n"
            "  print(a)\n"
            "  print(b)\n"
            "  print(rest)\n"
            ";");
        XASSERT_EQ(out[0], std::string("10"));
        XASSERT_EQ(out[1], std::string("20"));
        XASSERT_EQ(out[2], std::string("[30, 40, 50]")); });
}

// ============================================================================
// Section 5: Map iteration
// ============================================================================

static void testMapIteration()
{
    std::cout << "\n===== Map Iteration =====\n";

    runTest("for key, value in map", []()
            {
        auto out = runXell(
            "m = {x: 10, y: 20, z: 30}\n"
            "for key, value in m :\n"
            "  print(key + \"=\" + str(value))\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)3);
        // Map preserves insertion order
        XASSERT_EQ(out[0], std::string("x=10"));
        XASSERT_EQ(out[1], std::string("y=20"));
        XASSERT_EQ(out[2], std::string("z=30")); });

    runTest("for single var in map gets pair", []()
            {
        auto out = runXell(
            "m = {a: 1}\n"
            "for pair in m :\n"
            "  print(pair)\n"
            ";");
        XASSERT_EQ(out[0], std::string("[\"a\", 1]")); });
}

// ============================================================================
// Section 6: String iteration
// ============================================================================

static void testStringIteration()
{
    std::cout << "\n===== String Iteration =====\n";

    runTest("for char in string", []()
            {
        auto out = runXell(
            "result = \"\"\n"
            "for c in \"abc\" :\n"
            "  result = result + c + \"-\"\n"
            ";\n"
            "print(result)");
        XASSERT_EQ(out[0], std::string("a-b-c-")); });
}

// ============================================================================
// Section 7: Set iteration
// ============================================================================

static void testSetIteration()
{
    std::cout << "\n===== Set Iteration =====\n";

    runTest("for elem in set", []()
            {
        auto out = runXell(
            "s = to_set([3, 1, 2])\n"
            "total = 0\n"
            "for x in s :\n"
            "  total = total + x\n"
            ";\n"
            "print(total)");
        XASSERT_EQ(out[0], std::string("6")); });
}

// ============================================================================
// Section 8: Enumerate
// ============================================================================

static void testEnumerate()
{
    std::cout << "\n===== Enumerate =====\n";

    runTest("for i, name in enumerate(names)", []()
            {
        auto out = runXell(
            "names = [\"Prithu\", \"Nasif\", \"Sadik\"]\n"
            "for i, name in enumerate(names) :\n"
            "  print(str(i) + \": \" + name)\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], std::string("0: Prithu"));
        XASSERT_EQ(out[1], std::string("1: Nasif"));
        XASSERT_EQ(out[2], std::string("2: Sadik")); });

    runTest("enumerate with start offset", []()
            {
        auto out = runXell(
            "for i, v in enumerate([\"a\", \"b\"], 1) :\n"
            "  print(str(i) + v)\n"
            ";");
        XASSERT_EQ(out[0], std::string("1a"));
        XASSERT_EQ(out[1], std::string("2b")); });

    runTest("enumerate returns correct structure", []()
            {
        auto out = runXell(
            "e = enumerate([10, 20, 30])\n"
            "print(e[0])\n"
            "print(e[1])\n"
            "print(e[2])");
        XASSERT_EQ(out[0], std::string("[0, 10]"));
        XASSERT_EQ(out[1], std::string("[1, 20]"));
        XASSERT_EQ(out[2], std::string("[2, 30]")); });
}

// ============================================================================
// Section 9: Range variants
// ============================================================================

static void testRangeVariants()
{
    std::cout << "\n===== Range Variants =====\n";

    // range(to)
    runTest("range(5) = [0,1,2,3,4]", []()
            {
        auto out = runXell("print(range(5))");
        XASSERT_EQ(out[0], std::string("[0, 1, 2, 3, 4]")); });

    // range(from, to)
    runTest("range(2, 5) = [2,3,4]", []()
            {
        auto out = runXell("print(range(2, 5))");
        XASSERT_EQ(out[0], std::string("[2, 3, 4]")); });

    // range(from, to) descending — auto-detect step
    runTest("range(5, 2) = [5,4,3] descending", []()
            {
        auto out = runXell("print(range(5, 2))");
        XASSERT_EQ(out[0], std::string("[5, 4, 3]")); });

    runTest("range(10, 0) descending", []()
            {
        auto out = runXell("print(range(10, 7))");
        XASSERT_EQ(out[0], std::string("[10, 9, 8]")); });

    // range(from, to, step)
    runTest("range(0, 10, 2) = [0,2,4,6,8]", []()
            {
        auto out = runXell("print(range(0, 10, 2))");
        XASSERT_EQ(out[0], std::string("[0, 2, 4, 6, 8]")); });

    runTest("range(10, 0, -2) = [10,8,6,4,2]", []()
            {
        auto out = runXell("print(range(10, 0, -2))");
        XASSERT_EQ(out[0], std::string("[10, 8, 6, 4, 2]")); });

    // range(same, same) = empty
    runTest("range(5, 5) = []", []()
            {
        auto out = runXell("print(range(5, 5))");
        XASSERT_EQ(out[0], std::string("[]")); });

    // range(0) = empty
    runTest("range(0) = []", []()
            {
        auto out = runXell("print(range(0))");
        XASSERT_EQ(out[0], std::string("[]")); });

    // range with step 0 → error
    runTest("range with step 0 throws error", []()
            { XASSERT(expectError<std::exception>("range(0, 5, 0)")); });

    // for in range
    runTest("for i in range(3)", []()
            {
        auto out = runXell(
            "for i in range(3) :\n"
            "  print(i)\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], std::string("0"));
        XASSERT_EQ(out[1], std::string("1"));
        XASSERT_EQ(out[2], std::string("2")); });

    runTest("for i in range(5, 0)", []()
            {
        auto out = runXell(
            "for i in range(5, 3) :\n"
            "  print(i)\n"
            ";");
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("5"));
        XASSERT_EQ(out[1], std::string("4")); });
}

// ============================================================================
// Section 10: zip_longest
// ============================================================================

static void testZipLongest()
{
    std::cout << "\n===== zip_longest =====\n";

    runTest("zip_longest with default none", []()
            {
        auto out = runXell(
            "r = zip_longest([1, 2, 3], [\"a\"])\n"
            "print(len(r))\n"
            "print(r[2])");
        XASSERT_EQ(out[0], std::string("3"));
        XASSERT_EQ(out[1], std::string("[3, none]")); });

    runTest("zip_longest with fill value", []()
            {
        auto out = runXell(
            "r = zip_longest([1], [\"a\", \"b\"], 0)\n"
            "print(r[1])");
        XASSERT_EQ(out[0], std::string("[0, \"b\"]")); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "=======================================\n";
    std::cout << "   Xell For-Loop Tests\n";
    std::cout << "=======================================\n";

    testBasicFor();
    testDestructuringFor();
    testParallelFor();
    testRestCapture();
    testMapIteration();
    testStringIteration();
    testSetIteration();
    testEnumerate();
    testRangeVariants();
    testZipLongest();

    std::cout << "\n=======================================\n";
    std::cout << "  Results: " << g_passed << " passed, " << g_failed << " failed\n";
    std::cout << "=======================================\n";

    return g_failed == 0 ? 0 : 1;
}
