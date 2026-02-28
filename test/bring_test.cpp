// =============================================================================
// Bring (Import) System Tests
// =============================================================================
// Tests for: bring <names> from <file> [as <aliases>]
//            bring * from <file>
//            circular import detection
//            error handling (missing file, missing name)
//
// Strategy: write temporary .xel fixture files to /tmp, then bring from them.
// =============================================================================

#include "../src/interpreter/interpreter.hpp"
#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include <iostream>
#include <sstream>
#include <fstream>
#include <functional>
#include <string>
#include <vector>
#include <cstdio>

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

// ---- Helpers ---------------------------------------------------------------

static void writeFile(const std::string &path, const std::string &content)
{
    std::ofstream f(path);
    f << content;
}

static void removeFile(const std::string &path)
{
    std::remove(path.c_str());
}

// Run Xell source with a faux source file path (for relative bring resolution)
static std::vector<std::string> runXellFrom(const std::string &source,
                                            const std::string &sourceFile)
{
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();
    Interpreter interp;
    interp.setSourceFile(sourceFile);
    interp.run(program);
    return interp.output();
}

template <typename ExcType>
static bool expectError(const std::string &source, const std::string &sourceFile = "")
{
    try
    {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        if (!sourceFile.empty())
            interp.setSourceFile(sourceFile);
        interp.run(program);
        return false;
    }
    catch (const ExcType &)
    {
        return true;
    }
}

// ---- Fixture paths ---------------------------------------------------------

static const std::string TMPDIR = "/tmp/xell_bring_test";
static const std::string MAIN_FILE = TMPDIR + "/main.xel";
static const std::string HELPERS_FILE = TMPDIR + "/helpers.xel";
static const std::string MATH_FILE = TMPDIR + "/math_utils.xel";
static const std::string CIRCULAR_A = TMPDIR + "/circ_a.xel";
static const std::string CIRCULAR_B = TMPDIR + "/circ_b.xel";
static const std::string NESTED_DIR = TMPDIR + "/lib";
static const std::string NESTED_FILE = NESTED_DIR + "/utils.xel";

static void setupFixtures()
{
    // Create directories
    (void)std::system(("mkdir -p " + NESTED_DIR).c_str());

    // helpers.xel — functions + variables
    writeFile(HELPERS_FILE,
              "greeting = \"hello\"\n"
              "version = 42\n"
              "\n"
              "fn greet(name):\n"
              "    give \"Hi, {name}!\"\n"
              ";\n"
              "\n"
              "fn farewell(name):\n"
              "    give \"Bye, {name}!\"\n"
              ";\n");

    // math_utils.xel — pure functions
    writeFile(MATH_FILE,
              "fn double(n):\n"
              "    give n * 2\n"
              ";\n"
              "\n"
              "fn square(n):\n"
              "    give n * n\n"
              ";\n"
              "\n"
              "pi = 3\n");

    // lib/utils.xel — nested directory
    writeFile(NESTED_FILE,
              "fn add(a, b):\n"
              "    give a + b\n"
              ";\n"
              "\n"
              "fn sub(a, b):\n"
              "    give a - b\n"
              ";\n");

    // circ_a.xel — circular import A→B
    writeFile(CIRCULAR_A,
              "bring * from \"circ_b.xel\"\n"
              "x = 1\n");

    // circ_b.xel — circular import B→A
    writeFile(CIRCULAR_B,
              "bring * from \"circ_a.xel\"\n"
              "y = 2\n");
}

static void cleanupFixtures()
{
    removeFile(HELPERS_FILE);
    removeFile(MATH_FILE);
    removeFile(NESTED_FILE);
    removeFile(CIRCULAR_A);
    removeFile(CIRCULAR_B);
    (void)std::system(("rm -rf " + TMPDIR).c_str());
}

// ============================================================================
// Test sections
// ============================================================================

static void testBringNamed()
{
    std::cout << "\n===== Bring: Named Imports =====\n";

    runTest("bring single function", []()
            {
        auto out = runXellFrom(
            "bring greet from \"helpers.xel\"\n"
            "print(greet(\"Xell\"))",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], std::string("Hi, Xell!")); });

    runTest("bring single variable", []()
            {
        auto out = runXellFrom(
            "bring greeting from \"helpers.xel\"\n"
            "print(greeting)",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], std::string("hello")); });

    runTest("bring multiple names", []()
            {
        auto out = runXellFrom(
            "bring greet, farewell from \"helpers.xel\"\n"
            "print(greet(\"A\"))\n"
            "print(farewell(\"B\"))",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("Hi, A!"));
        XASSERT_EQ(out[1], std::string("Bye, B!")); });

    runTest("bring mixed functions and variables", []()
            {
        auto out = runXellFrom(
            "bring greet, version from \"helpers.xel\"\n"
            "print(greet(\"World\"))\n"
            "print(version)",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("Hi, World!"));
        XASSERT_EQ(out[1], std::string("42")); });

    runTest("bring from math_utils", []()
            {
        auto out = runXellFrom(
            "bring double, square from \"math_utils.xel\"\n"
            "print(double(5))\n"
            "print(square(4))",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("10"));
        XASSERT_EQ(out[1], std::string("16")); });
}

static void testBringAliases()
{
    std::cout << "\n===== Bring: Aliases =====\n";

    runTest("bring with single alias", []()
            {
        auto out = runXellFrom(
            "bring greet from \"helpers.xel\" as hi\n"
            "print(hi(\"World\"))",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], std::string("Hi, World!")); });

    runTest("bring with multiple aliases", []()
            {
        auto out = runXellFrom(
            "bring greet, farewell from \"helpers.xel\" as hi, bye\n"
            "print(hi(\"X\"))\n"
            "print(bye(\"Y\"))",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("Hi, X!"));
        XASSERT_EQ(out[1], std::string("Bye, Y!")); });

    runTest("bring variable with alias", []()
            {
        auto out = runXellFrom(
            "bring version from \"helpers.xel\" as ver\n"
            "print(ver)",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], std::string("42")); });

    runTest("partial aliases (fewer aliases than names)", []()
            {
        auto out = runXellFrom(
            "bring greet, farewell from \"helpers.xel\" as hi\n"
            "print(hi(\"X\"))\n"
            "print(farewell(\"Y\"))",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("Hi, X!"));
        XASSERT_EQ(out[1], std::string("Bye, Y!")); });
}

static void testBringAll()
{
    std::cout << "\n===== Bring: Wildcard (*) =====\n";

    runTest("bring * imports all names", []()
            {
        auto out = runXellFrom(
            "bring * from \"helpers.xel\"\n"
            "print(greet(\"All\"))\n"
            "print(farewell(\"All\"))\n"
            "print(greeting)\n"
            "print(version)",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)4);
        XASSERT_EQ(out[0], std::string("Hi, All!"));
        XASSERT_EQ(out[1], std::string("Bye, All!"));
        XASSERT_EQ(out[2], std::string("hello"));
        XASSERT_EQ(out[3], std::string("42")); });

    runTest("bring * from math_utils", []()
            {
        auto out = runXellFrom(
            "bring * from \"math_utils.xel\"\n"
            "print(double(7))\n"
            "print(square(3))\n"
            "print(pi)",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], std::string("14"));
        XASSERT_EQ(out[1], std::string("9"));
        XASSERT_EQ(out[2], std::string("3")); });
}

static void testBringNested()
{
    std::cout << "\n===== Bring: Nested Paths =====\n";

    runTest("bring from subdirectory", []()
            {
        auto out = runXellFrom(
            "bring add, sub from \"lib/utils.xel\"\n"
            "print(add(10, 3))\n"
            "print(sub(10, 3))",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("13"));
        XASSERT_EQ(out[1], std::string("7")); });

    runTest("bring * from subdirectory", []()
            {
        auto out = runXellFrom(
            "bring * from \"lib/utils.xel\"\n"
            "print(add(1, 2))\n"
            "print(sub(5, 1))",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("3"));
        XASSERT_EQ(out[1], std::string("4")); });
}

static void testBringUsage()
{
    std::cout << "\n===== Bring: Usage Patterns =====\n";

    runTest("use brought function in expression", []()
            {
        auto out = runXellFrom(
            "bring double from \"math_utils.xel\"\n"
            "x = double(5) + 3\n"
            "print(x)",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], std::string("13")); });

    runTest("brought function as argument to another", []()
            {
        auto out = runXellFrom(
            "bring double, square from \"math_utils.xel\"\n"
            "print(double(square(3)))",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], std::string("18")); });

    runTest("bring into function scope", []()
            {
        auto out = runXellFrom(
            "bring double from \"math_utils.xel\"\n"
            "fn quad(n):\n"
            "    give double(double(n))\n"
            ";\n"
            "print(quad(3))",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], std::string("12")); });

    runTest("multiple bring statements", []()
            {
        auto out = runXellFrom(
            "bring greet from \"helpers.xel\"\n"
            "bring double from \"math_utils.xel\"\n"
            "print(greet(\"X\"))\n"
            "print(double(10))",
            MAIN_FILE);
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("Hi, X!"));
        XASSERT_EQ(out[1], std::string("20")); });

    runTest("bring does not leak unimported names", []()
            {
        // Only import 'greet', but 'farewell' should NOT be available
        XASSERT(expectError<UndefinedVariableError>(
            "bring greet from \"helpers.xel\"\n"
            "farewell(\"X\")",
            MAIN_FILE)); });
}

static void testBringErrors()
{
    std::cout << "\n===== Bring: Error Cases =====\n";

    runTest("bring from nonexistent file", []()
            { XASSERT(expectError<BringError>(
                  "bring foo from \"nonexistent.xel\"",
                  MAIN_FILE)); });

    runTest("bring nonexistent name", []()
            { XASSERT(expectError<BringError>(
                  "bring nonexistent_fn from \"helpers.xel\"",
                  MAIN_FILE)); });

    runTest("circular import detection", []()
            { XASSERT(expectError<BringError>(
                  "bring * from \"circ_a.xel\"",
                  MAIN_FILE)); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "============================================\n";
    std::cout << "  Bring (Import) System Tests\n";
    std::cout << "============================================\n";

    setupFixtures();

    testBringNamed();
    testBringAliases();
    testBringAll();
    testBringNested();
    testBringUsage();
    testBringErrors();

    cleanupFixtures();

    std::cout << "\n============================================\n";
    std::cout << "  Total: " << (g_passed + g_failed)
              << "  |  Passed: " << g_passed
              << "  |  Failed: " << g_failed << "\n";
    std::cout << "============================================\n";

    return g_failed > 0 ? 1 : 0;
}
