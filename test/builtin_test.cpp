// =============================================================================
// Builtin Tests — Comprehensive coverage for all new builtins
// =============================================================================
// Tests string, list, map, math (trig/hyperbolic), and higher-order function
// builtins. Each test runs Xell source and checks the captured print() output.
// =============================================================================

#include "../src/interpreter/interpreter.hpp"
#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include <iostream>
#include <sstream>
#include <functional>
#include <string>
#include <vector>
#include <cmath>

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

// Helper: run and expect error
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
// Section 1: String Builtins
// ============================================================================

static void testStringBuiltins()
{
    std::cout << "\n===== String Builtins =====\n";

    // split
    runTest("split basic", []()
            {
        auto out = runXell("print(split(\"a,b,c\", \",\"))");
        XASSERT_EQ(out[0], std::string("[\"a\", \"b\", \"c\"]")); });

    runTest("split with space", []()
            {
        auto out = runXell("print(split(\"hello world\", \" \"))");
        XASSERT_EQ(out[0], std::string("[\"hello\", \"world\"]")); });

    // join
    runTest("join basic", []()
            {
        auto out = runXell("print(join([\"a\", \"b\", \"c\"], \"-\"))");
        XASSERT_EQ(out[0], std::string("a-b-c")); });

    runTest("join empty separator", []()
            {
        auto out = runXell("print(join([\"x\", \"y\"], \"\"))");
        XASSERT_EQ(out[0], std::string("xy")); });

    // trim
    runTest("trim whitespace", []()
            {
        auto out = runXell("print(trim(\"  hello  \"))");
        XASSERT_EQ(out[0], std::string("hello")); });

    runTest("trim_start", []()
            {
        auto out = runXell("print(trim_start(\"  hello  \"))");
        XASSERT_EQ(out[0], std::string("hello  ")); });

    runTest("trim_end", []()
            {
        auto out = runXell("print(trim_end(\"  hello  \"))");
        XASSERT_EQ(out[0], std::string("  hello")); });

    // upper / lower
    runTest("upper", []()
            {
        auto out = runXell("print(upper(\"hello\"))");
        XASSERT_EQ(out[0], std::string("HELLO")); });

    runTest("lower", []()
            {
        auto out = runXell("print(lower(\"HELLO\"))");
        XASSERT_EQ(out[0], std::string("hello")); });

    // replace / replace_first
    runTest("replace all", []()
            {
        auto out = runXell("print(replace(\"aabbcc\", \"b\", \"X\"))");
        XASSERT_EQ(out[0], std::string("aaXXcc")); });

    runTest("replace_first", []()
            {
        auto out = runXell("print(replace_first(\"aabbcc\", \"b\", \"X\"))");
        XASSERT_EQ(out[0], std::string("aaXbcc")); });

    // starts_with / ends_with
    runTest("starts_with true", []()
            {
        auto out = runXell("print(starts_with(\"hello world\", \"hello\"))");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("starts_with false", []()
            {
        auto out = runXell("print(starts_with(\"hello world\", \"world\"))");
        XASSERT_EQ(out[0], std::string("false")); });

    runTest("ends_with true", []()
            {
        auto out = runXell("print(ends_with(\"hello world\", \"world\"))");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("ends_with false", []()
            {
        auto out = runXell("print(ends_with(\"hello world\", \"hello\"))");
        XASSERT_EQ(out[0], std::string("false")); });

    // index_of (string)
    runTest("index_of string found", []()
            {
        auto out = runXell("print(index_of(\"hello\", \"ll\"))");
        XASSERT_EQ(out[0], std::string("2")); });

    runTest("index_of string not found", []()
            {
        auto out = runXell("print(index_of(\"hello\", \"xyz\"))");
        XASSERT_EQ(out[0], std::string("-1")); });

    // index_of (list — polymorphic)
    runTest("index_of list found", []()
            {
        auto out = runXell("print(index_of([10, 20, 30], 20))");
        XASSERT_EQ(out[0], std::string("1")); });

    runTest("index_of list not found", []()
            {
        auto out = runXell("print(index_of([10, 20, 30], 99))");
        XASSERT_EQ(out[0], std::string("-1")); });

    // substr
    runTest("substr", []()
            {
        auto out = runXell("print(substr(\"hello world\", 6, 5))");
        XASSERT_EQ(out[0], std::string("world")); });

    // char_at
    runTest("char_at", []()
            {
        auto out = runXell("print(char_at(\"hello\", 1))");
        XASSERT_EQ(out[0], std::string("e")); });

    // repeat
    runTest("repeat", []()
            {
        auto out = runXell("print(repeat(\"ab\", 3))");
        XASSERT_EQ(out[0], std::string("ababab")); });

    // pad_start / pad_end
    runTest("pad_start", []()
            {
        auto out = runXell("print(pad_start(\"5\", 3, \"0\"))");
        XASSERT_EQ(out[0], std::string("005")); });

    runTest("pad_end", []()
            {
        auto out = runXell("print(pad_end(\"5\", 3, \"0\"))");
        XASSERT_EQ(out[0], std::string("500")); });

    // reverse (string)
    runTest("reverse string", []()
            {
        auto out = runXell("print(reverse(\"hello\"))");
        XASSERT_EQ(out[0], std::string("olleh")); });

    // reverse (list — polymorphic)
    runTest("reverse list", []()
            {
        auto out = runXell("print(reverse([1, 2, 3]))");
        XASSERT_EQ(out[0], std::string("[3, 2, 1]")); });

    // count (string)
    runTest("count string", []()
            {
        auto out = runXell("print(count(\"banana\", \"a\"))");
        XASSERT_EQ(out[0], std::string("3")); });

    // count (list — polymorphic)
    runTest("count list", []()
            {
        auto out = runXell("print(count([1, 2, 2, 3, 2], 2))");
        XASSERT_EQ(out[0], std::string("3")); });

    // is_empty
    runTest("is_empty empty string", []()
            {
        auto out = runXell("print(is_empty(\"\"))");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("is_empty non-empty string", []()
            {
        auto out = runXell("print(is_empty(\"hi\"))");
        XASSERT_EQ(out[0], std::string("false")); });

    runTest("is_empty empty list", []()
            {
        auto out = runXell("print(is_empty([]))");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("is_empty non-empty list", []()
            {
        auto out = runXell("print(is_empty([1]))");
        XASSERT_EQ(out[0], std::string("false")); });

    // is_numeric / is_alpha
    runTest("is_numeric true", []()
            {
        auto out = runXell("print(is_numeric(\"12345\"))");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("is_numeric false", []()
            {
        auto out = runXell("print(is_numeric(\"12a45\"))");
        XASSERT_EQ(out[0], std::string("false")); });

    runTest("is_alpha true", []()
            {
        auto out = runXell("print(is_alpha(\"hello\"))");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("is_alpha false", []()
            {
        auto out = runXell("print(is_alpha(\"he1lo\"))");
        XASSERT_EQ(out[0], std::string("false")); });

    // lines
    runTest("lines", []()
            {
        auto out = runXell("print(len(lines(\"a\\nb\\nc\")))");
        XASSERT_EQ(out[0], std::string("3")); });

    // to_chars
    runTest("to_chars", []()
            {
        auto out = runXell("print(to_chars(\"abc\"))");
        XASSERT_EQ(out[0], std::string("[\"a\", \"b\", \"c\"]")); });
}

// ============================================================================
// Section 2: List Builtins
// ============================================================================

static void testListBuiltins()
{
    std::cout << "\n===== List Builtins =====\n";

    // shift / unshift
    runTest("shift removes first", []()
            {
        auto out = runXell(
            "lst = [1, 2, 3]\n"
            "v = shift(lst)\n"
            "print(v)\n"
            "print(lst)");
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("[2, 3]")); });

    runTest("unshift adds to front", []()
            {
        auto out = runXell(
            "lst = [2, 3]\n"
            "unshift(lst, 1)\n"
            "print(lst)");
        XASSERT_EQ(out[0], std::string("[1, 2, 3]")); });

    // insert
    runTest("insert at index", []()
            {
        auto out = runXell(
            "lst = [1, 3, 4]\n"
            "insert(lst, 1, 2)\n"
            "print(lst)");
        XASSERT_EQ(out[0], std::string("[1, 2, 3, 4]")); });

    // remove_val
    runTest("remove_val", []()
            {
        auto out = runXell(
            "lst = [1, 2, 3, 2]\n"
            "remove_val(lst, 2)\n"
            "print(lst)");
        XASSERT_EQ(out[0], std::string("[1, 3, 2]")); });

    // sort / sort_desc
    runTest("sort ascending", []()
            {
        auto out = runXell(
            "lst = [3, 1, 2]\n"
            "sort(lst)\n"
            "print(lst)");
        XASSERT_EQ(out[0], std::string("[1, 2, 3]")); });

    runTest("sort_desc descending", []()
            {
        auto out = runXell(
            "lst = [1, 3, 2]\n"
            "sort_desc(lst)\n"
            "print(lst)");
        XASSERT_EQ(out[0], std::string("[3, 2, 1]")); });

    // slice
    runTest("slice", []()
            {
        auto out = runXell("print(slice([10, 20, 30, 40, 50], 1, 4))");
        XASSERT_EQ(out[0], std::string("[20, 30, 40]")); });

    // flatten
    runTest("flatten nested", []()
            {
        auto out = runXell("print(flatten([[1, 2], [3, 4], [5]]))");
        XASSERT_EQ(out[0], std::string("[1, 2, 3, 4, 5]")); });

    // unique
    runTest("unique", []()
            {
        auto out = runXell(
            "lst = unique([1, 2, 2, 3, 1])\n"
            "print(len(lst))");
        XASSERT_EQ(out[0], std::string("3")); });

    // first / last
    runTest("first", []()
            {
        auto out = runXell("print(first([10, 20, 30]))");
        XASSERT_EQ(out[0], std::string("10")); });

    runTest("last", []()
            {
        auto out = runXell("print(last([10, 20, 30]))");
        XASSERT_EQ(out[0], std::string("30")); });

    // zip
    runTest("zip two lists", []()
            {
        auto out = runXell("print(zip([1, 2], [\"a\", \"b\"]))");
        XASSERT_EQ(out[0], std::string("[[1, \"a\"], [2, \"b\"]]")); });

    // sum
    runTest("sum", []()
            {
        auto out = runXell("print(sum([1, 2, 3, 4]))");
        XASSERT_EQ(out[0], std::string("10")); });

    // min / max (list mode)
    runTest("min of list", []()
            {
        auto out = runXell("print(min([5, 2, 8, 1]))");
        XASSERT_EQ(out[0], std::string("1")); });

    runTest("max of list", []()
            {
        auto out = runXell("print(max([5, 2, 8, 1]))");
        XASSERT_EQ(out[0], std::string("8")); });

    // min / max (pair mode)
    runTest("min of two", []()
            {
        auto out = runXell("print(min(3, 7))");
        XASSERT_EQ(out[0], std::string("3")); });

    runTest("max of two", []()
            {
        auto out = runXell("print(max(3, 7))");
        XASSERT_EQ(out[0], std::string("7")); });

    // avg
    runTest("avg", []()
            {
        auto out = runXell("print(avg([2, 4, 6]))");
        XASSERT_EQ(out[0], std::string("4")); });

    // size (polymorphic)
    runTest("size of list", []()
            {
        auto out = runXell("print(size([1, 2, 3]))");
        XASSERT_EQ(out[0], std::string("3")); });

    runTest("size of string", []()
            {
        auto out = runXell("print(size(\"hello\"))");
        XASSERT_EQ(out[0], std::string("5")); });
}

// ============================================================================
// Section 3: Map Builtins
// ============================================================================

static void testMapBuiltins()
{
    std::cout << "\n===== Map Builtins =====\n";

    // delete_key
    runTest("delete_key", []()
            {
        auto out = runXell(
            "m = {a: 1, b: 2, c: 3}\n"
            "delete_key(m, \"b\")\n"
            "print(has(m, \"b\"))");
        XASSERT_EQ(out[0], std::string("false")); });

    // get with default
    runTest("get existing key", []()
            {
        auto out = runXell("print(get({a: 1, b: 2}, \"a\", 0))");
        XASSERT_EQ(out[0], std::string("1")); });

    runTest("get missing key returns default", []()
            {
        auto out = runXell("print(get({a: 1}, \"z\", 99))");
        XASSERT_EQ(out[0], std::string("99")); });

    // merge
    runTest("merge two maps", []()
            {
        auto out = runXell(
            "m = merge({a: 1}, {b: 2})\n"
            "print(has(m, \"a\"))\n"
            "print(has(m, \"b\"))");
        XASSERT_EQ(out[0], std::string("true"));
        XASSERT_EQ(out[1], std::string("true")); });

    // entries
    runTest("entries", []()
            {
        auto out = runXell(
            "m = {x: 10}\n"
            "e = entries(m)\n"
            "print(len(e))\n"
            "print(e[0][0])\n"
            "print(e[0][1])");
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("x"));
        XASSERT_EQ(out[2], std::string("10")); });

    // from_entries
    runTest("from_entries", []()
            {
        auto out = runXell(
            "m = from_entries([[\"a\", 1], [\"b\", 2]])\n"
            "print(m[\"a\"])\n"
            "print(m[\"b\"])");
        XASSERT_EQ(out[0], std::string("1"));
        XASSERT_EQ(out[1], std::string("2")); });
}

// ============================================================================
// Section 4: Math Builtins — Basic
// ============================================================================

static void testMathBuiltinsBasic()
{
    std::cout << "\n===== Math Builtins (Basic) =====\n";

    // log / log10
    runTest("log(1) == 0", []()
            {
        auto out = runXell("print(log(1))");
        XASSERT_EQ(out[0], std::string("0")); });

    runTest("log10(100) == 2", []()
            {
        auto out = runXell("print(log10(100))");
        XASSERT_EQ(out[0], std::string("2")); });

    // clamp
    runTest("clamp within range", []()
            {
        auto out = runXell("print(clamp(5, 0, 10))");
        XASSERT_EQ(out[0], std::string("5")); });

    runTest("clamp below low", []()
            {
        auto out = runXell("print(clamp(-5, 0, 10))");
        XASSERT_EQ(out[0], std::string("0")); });

    runTest("clamp above high", []()
            {
        auto out = runXell("print(clamp(15, 0, 10))");
        XASSERT_EQ(out[0], std::string("10")); });

    // is_nan / is_inf
    runTest("is_nan false", []()
            {
        auto out = runXell("print(is_nan(42))");
        XASSERT_EQ(out[0], std::string("false")); });

    runTest("is_inf false", []()
            {
        auto out = runXell("print(is_inf(42))");
        XASSERT_EQ(out[0], std::string("false")); });

    // to_int / to_float
    runTest("to_int from float", []()
            {
        auto out = runXell("print(to_int(3.9))");
        XASSERT_EQ(out[0], std::string("3")); });

    runTest("to_int from string", []()
            {
        auto out = runXell("print(to_int(\"42\"))");
        XASSERT_EQ(out[0], std::string("42")); });

    runTest("to_float from int", []()
            {
        auto out = runXell("print(to_float(5) + 0.1)");
        XASSERT_EQ(out[0], std::string("5.1")); });

    // hex / bin
    runTest("hex", []()
            {
        auto out = runXell("print(hex(255))");
        XASSERT_EQ(out[0], std::string("ff")); });

    runTest("bin", []()
            {
        auto out = runXell("print(bin(10))");
        XASSERT_EQ(out[0], std::string("1010")); });

    // random (just check it doesn't crash)
    runTest("random returns number", []()
            {
        auto out = runXell(
            "r = random()\n"
            "print(r >= 0 and r < 1)");
        XASSERT_EQ(out[0], std::string("true")); });

    // random_int
    runTest("random_int in range", []()
            {
        auto out = runXell(
            "r = random_int(5, 5)\n"
            "print(r)");
        XASSERT_EQ(out[0], std::string("5")); });

    // Constants
    runTest("PI is defined", []()
            {
        auto out = runXell("print(PI > 3.14 and PI < 3.15)");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("E is defined", []()
            {
        auto out = runXell("print(E > 2.71 and E < 2.72)");
        XASSERT_EQ(out[0], std::string("true")); });
}

// ============================================================================
// Section 5: Math Builtins — Trigonometric
// ============================================================================

static void testMathTrig()
{
    std::cout << "\n===== Math Builtins (Trigonometric) =====\n";

    // sin / cos / tan at 0
    runTest("sin(0) == 0", []()
            {
        auto out = runXell("print(sin(0))");
        XASSERT_EQ(out[0], std::string("0")); });

    runTest("cos(0) == 1", []()
            {
        auto out = runXell("print(cos(0))");
        XASSERT_EQ(out[0], std::string("1")); });

    runTest("tan(0) == 0", []()
            {
        auto out = runXell("print(tan(0))");
        XASSERT_EQ(out[0], std::string("0")); });

    // sin(PI/2) ≈ 1
    runTest("sin(PI/2) approx 1", []()
            {
        auto out = runXell("print(abs(sin(PI / 2) - 1) < 0.0001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // cos(PI) ≈ -1
    runTest("cos(PI) approx -1", []()
            {
        auto out = runXell("print(abs(cos(PI) + 1) < 0.0001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // cot(PI/4) ≈ 1
    runTest("cot(PI/4) approx 1", []()
            {
        auto out = runXell("print(abs(cot(PI / 4) - 1) < 0.001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // sec(0) == 1 (1/cos(0) = 1)
    runTest("sec(0) == 1", []()
            {
        auto out = runXell("print(sec(0))");
        XASSERT_EQ(out[0], std::string("1")); });

    // csc(PI/2) ≈ 1
    runTest("csc(PI/2) approx 1", []()
            {
        auto out = runXell("print(abs(csc(PI / 2) - 1) < 0.0001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // ---- Inverse trig ----

    // asin(0) == 0
    runTest("asin(0) == 0", []()
            {
        auto out = runXell("print(asin(0))");
        XASSERT_EQ(out[0], std::string("0")); });

    // acos(1) == 0
    runTest("acos(1) == 0", []()
            {
        auto out = runXell("print(acos(1))");
        XASSERT_EQ(out[0], std::string("0")); });

    // atan(0) == 0
    runTest("atan(0) == 0", []()
            {
        auto out = runXell("print(atan(0))");
        XASSERT_EQ(out[0], std::string("0")); });

    // atan2(0, 1) == 0
    runTest("atan2(0, 1) == 0", []()
            {
        auto out = runXell("print(atan2(0, 1))");
        XASSERT_EQ(out[0], std::string("0")); });

    // atan2(1, 0) ≈ PI/2
    runTest("atan2(1, 0) approx PI/2", []()
            {
        auto out = runXell("print(abs(atan2(1, 0) - PI / 2) < 0.0001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // asin(1) ≈ PI/2
    runTest("asin(1) approx PI/2", []()
            {
        auto out = runXell("print(abs(asin(1) - PI / 2) < 0.0001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // acot(1) ≈ PI/4
    runTest("acot(1) approx PI/4", []()
            {
        auto out = runXell("print(abs(acot(1) - PI / 4) < 0.001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // asec(1) == 0  (acos(1) = 0)
    runTest("asec(1) == 0", []()
            {
        auto out = runXell("print(asec(1))");
        XASSERT_EQ(out[0], std::string("0")); });

    // acsc(1) ≈ PI/2 (asin(1) = PI/2)
    runTest("acsc(1) approx PI/2", []()
            {
        auto out = runXell("print(abs(acsc(1) - PI / 2) < 0.0001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // Domain errors
    runTest("asin domain error", []()
            { XASSERT(expectError<std::exception>("asin(2)")); });

    runTest("acos domain error", []()
            { XASSERT(expectError<std::exception>("acos(2)")); });

    runTest("asec domain error |x| < 1", []()
            { XASSERT(expectError<std::exception>("asec(0.5)")); });

    runTest("acsc domain error |x| < 1", []()
            { XASSERT(expectError<std::exception>("acsc(0.5)")); });
}

// ============================================================================
// Section 6: Math Builtins — Hyperbolic
// ============================================================================

static void testMathHyperbolic()
{
    std::cout << "\n===== Math Builtins (Hyperbolic) =====\n";

    // sinh(0) == 0
    runTest("sinh(0) == 0", []()
            {
        auto out = runXell("print(sinh(0))");
        XASSERT_EQ(out[0], std::string("0")); });

    // cosh(0) == 1
    runTest("cosh(0) == 1", []()
            {
        auto out = runXell("print(cosh(0))");
        XASSERT_EQ(out[0], std::string("1")); });

    // tanh(0) == 0
    runTest("tanh(0) == 0", []()
            {
        auto out = runXell("print(tanh(0))");
        XASSERT_EQ(out[0], std::string("0")); });

    // sinh(1) ≈ 1.1752
    runTest("sinh(1) approx 1.1752", []()
            {
        auto out = runXell("print(abs(sinh(1) - 1.1752) < 0.001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // cosh(1) ≈ 1.5431
    runTest("cosh(1) approx 1.5431", []()
            {
        auto out = runXell("print(abs(cosh(1) - 1.5431) < 0.001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // tanh(1) ≈ 0.7616
    runTest("tanh(1) approx 0.7616", []()
            {
        auto out = runXell("print(abs(tanh(1) - 0.7616) < 0.001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // coth(1) ≈ 1.3130
    runTest("coth(1) approx 1.3130", []()
            {
        auto out = runXell("print(abs(coth(1) - 1.3130) < 0.001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // sech(0) == 1  (1/cosh(0) = 1)
    runTest("sech(0) == 1", []()
            {
        auto out = runXell("print(sech(0))");
        XASSERT_EQ(out[0], std::string("1")); });

    // csch(1) ≈ 0.8509
    runTest("csch(1) approx 0.8509", []()
            {
        auto out = runXell("print(abs(csch(1) - 0.8509) < 0.001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // csch(0) → division by zero
    runTest("csch(0) division by zero", []()
            { XASSERT(expectError<std::exception>("csch(0)")); });

    // coth(0) → division by zero
    runTest("coth(0) division by zero", []()
            { XASSERT(expectError<std::exception>("coth(0)")); });

    // ---- Inverse hyperbolic ----

    // asinh(0) == 0
    runTest("asinh(0) == 0", []()
            {
        auto out = runXell("print(asinh(0))");
        XASSERT_EQ(out[0], std::string("0")); });

    // acosh(1) == 0
    runTest("acosh(1) == 0", []()
            {
        auto out = runXell("print(acosh(1))");
        XASSERT_EQ(out[0], std::string("0")); });

    // atanh(0) == 0
    runTest("atanh(0) == 0", []()
            {
        auto out = runXell("print(atanh(0))");
        XASSERT_EQ(out[0], std::string("0")); });

    // asinh(1) ≈ 0.8814
    runTest("asinh(1) approx 0.8814", []()
            {
        auto out = runXell("print(abs(asinh(1) - 0.8814) < 0.001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // acosh(2) ≈ 1.3170
    runTest("acosh(2) approx 1.3170", []()
            {
        auto out = runXell("print(abs(acosh(2) - 1.3170) < 0.001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // acoth(2) ≈ 0.5493
    runTest("acoth(2) approx 0.5493", []()
            {
        auto out = runXell("print(abs(acoth(2) - 0.5493) < 0.001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // asech(0.5) ≈ 1.3170
    runTest("asech(0.5) approx 1.3170", []()
            {
        auto out = runXell("print(abs(asech(0.5) - 1.3170) < 0.001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // acsch(1) ≈ 0.8814
    runTest("acsch(1) approx 0.8814", []()
            {
        auto out = runXell("print(abs(acsch(1) - 0.8814) < 0.001)");
        XASSERT_EQ(out[0], std::string("true")); });

    // Domain errors
    runTest("acosh domain error x < 1", []()
            { XASSERT(expectError<std::exception>("acosh(0.5)")); });

    runTest("atanh domain error x == 1", []()
            { XASSERT(expectError<std::exception>("atanh(1)")); });

    runTest("acoth domain error |x| <= 1", []()
            { XASSERT(expectError<std::exception>("acoth(0.5)")); });

    runTest("asech domain error x > 1", []()
            { XASSERT(expectError<std::exception>("asech(1.5)")); });

    runTest("acsch domain error x == 0", []()
            { XASSERT(expectError<std::exception>("acsch(0)")); });
}

// ============================================================================
// Section 7: Higher-Order Function Builtins
// ============================================================================

static void testHOFBuiltins()
{
    std::cout << "\n===== Higher-Order Function Builtins =====\n";

    // map
    runTest("map with lambda", []()
            {
        auto out = runXell("print(map([1, 2, 3], x => x * 2))");
        XASSERT_EQ(out[0], std::string("[2, 4, 6]")); });

    runTest("map with named fn", []()
            {
        auto out = runXell(
            "fn square(x):\n"
            "  give x * x\n"
            ";\n"
            "print(map([1, 2, 3], square))");
        XASSERT_EQ(out[0], std::string("[1, 4, 9]")); });

    // filter
    runTest("filter with lambda", []()
            {
        auto out = runXell("print(filter([1, 2, 3, 4, 5], x => x > 3))");
        XASSERT_EQ(out[0], std::string("[4, 5]")); });

    runTest("filter even numbers", []()
            {
        auto out = runXell("print(filter([1, 2, 3, 4, 6], x => x % 2 == 0))");
        XASSERT_EQ(out[0], std::string("[2, 4, 6]")); });

    // reduce
    runTest("reduce sum", []()
            {
        auto out = runXell("print(reduce([1, 2, 3, 4], (acc, x) => acc + x, 0))");
        XASSERT_EQ(out[0], std::string("10")); });

    runTest("reduce product", []()
            {
        auto out = runXell("print(reduce([1, 2, 3, 4], (acc, x) => acc * x, 1))");
        XASSERT_EQ(out[0], std::string("24")); });

    // any
    runTest("any true", []()
            {
        auto out = runXell("print(any([1, 2, 3], x => x > 2))");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("any false", []()
            {
        auto out = runXell("print(any([1, 2, 3], x => x > 5))");
        XASSERT_EQ(out[0], std::string("false")); });

    // all
    runTest("all true", []()
            {
        auto out = runXell("print(all([2, 4, 6], x => x % 2 == 0))");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("all false", []()
            {
        auto out = runXell("print(all([2, 3, 6], x => x % 2 == 0))");
        XASSERT_EQ(out[0], std::string("false")); });
}

// ============================================================================
// Section 8: Collection Builtins — polymorphic contains
// ============================================================================

static void testContainsPolymorphic()
{
    std::cout << "\n===== Contains Polymorphic =====\n";

    runTest("contains in string", []()
            {
        auto out = runXell("print(contains(\"hello world\", \"world\"))");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("contains not in string", []()
            {
        auto out = runXell("print(contains(\"hello\", \"xyz\"))");
        XASSERT_EQ(out[0], std::string("false")); });

    runTest("contains in list", []()
            {
        auto out = runXell("print(contains([1, 2, 3], 2))");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("contains not in list", []()
            {
        auto out = runXell("print(contains([1, 2, 3], 9))");
        XASSERT_EQ(out[0], std::string("false")); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "=======================================\n";
    std::cout << "   Xell Builtin Tests\n";
    std::cout << "=======================================\n";

    testStringBuiltins();
    testListBuiltins();
    testMapBuiltins();
    testMathBuiltinsBasic();
    testMathTrig();
    testMathHyperbolic();
    testHOFBuiltins();
    testContainsPolymorphic();

    std::cout << "\n=======================================\n";
    std::cout << "  Results: " << g_passed << " passed, " << g_failed << " failed\n";
    std::cout << "=======================================\n";

    return g_failed == 0 ? 0 : 1;
}
