// =============================================================================
// threading_test.cpp — tests for the threading/concurrency module
// =============================================================================

#include "../src/interpreter/interpreter.hpp"
#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include <functional>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using namespace xell;

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

static std::vector<std::string> runXell(const std::string &source)
{
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();
    Interpreter interp;
    interp.loadModule("threading");
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

static void testThreadingModule()
{
    std::cout << "\n===== Threading Module =====\n";

    runTest("thread_spawn + thread_join returns result", []()
            {
        auto out = runXell(
            "fn work(x):\n"
            "  give x * 2\n"
            ";\n"
            "h = thread_spawn(work, 21)\n"
            "print(thread_join(h))");
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], std::string("42")); });

    runTest("thread_spawn snapshots closure values", []()
            {
        auto out = runXell(
            "base = 10\n"
            "fn work(x):\n"
            "  give x + base\n"
            ";\n"
            "h = thread_spawn(work, 5)\n"
            "base = 100\n"
            "print(thread_join(h))");
        XASSERT_EQ(out[0], std::string("15")); });

    runTest("thread_done reports completion", []()
            {
        auto out = runXell(
            "fn work():\n"
            "  give 1\n"
            ";\n"
            "h = thread_spawn(work)\n"
            "thread_join(h)\n"
            "print(thread_done(h))");
        XASSERT_EQ(out.back(), std::string("true")); });

    runTest("thread_join rethrows worker error", []()
            { XASSERT(expectError<XellError>(
                  "fn boom():\n"
                  "  throw \"boom\"\n"
                  ";\n"
                  "h = thread_spawn(boom)\n"
                  "thread_join(h)")); });

    runTest("thread_count is available", []()
            {
        auto out = runXell("print(thread_count() > 0)");
        XASSERT_EQ(out[0], std::string("true")); });

    runTest("thread_spawn supports multiple arguments", []()
            {
        auto out = runXell(
            "fn add3(a, b, c):\n"
            "  give a + b + c\n"
            ";\n"
            "h = thread_spawn(add3, 10, 20, 30)\n"
            "print(thread_join(h))");
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], std::string("60")); });

    runTest("multiple spawned threads can be joined independently", []()
            {
        auto out = runXell(
            "fn square(x):\n"
            "  give x * x\n"
            ";\n"
            "h1 = thread_spawn(square, 2)\n"
            "h2 = thread_spawn(square, 3)\n"
            "a = thread_join(h1)\n"
            "b = thread_join(h2)\n"
            "print(a + b)");
        XASSERT_EQ(out[0], std::string("13")); });

    runTest("mutex lock / try_lock / unlock behavior", []()
            {
        auto out = runXell(
            "m = mutex_create()\n"
            "mutex_lock(m)\n"
            "print(mutex_try_lock(m))\n"
            "mutex_unlock(m)\n"
            "ok = mutex_try_lock(m)\n"
            "print(ok)\n"
            "if ok:\n"
            "  mutex_unlock(m)\n"
            ";\n");
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], std::string("false"));
        XASSERT_EQ(out[1], std::string("true")); });

    runTest("mutex operations reject unknown handle", []()
            {
        XASSERT(expectError<ValueError>(
            "mutex_lock(999999)"));
        XASSERT(expectError<ValueError>(
            "mutex_unlock(999999)"));
        XASSERT(expectError<ValueError>(
            "mutex_try_lock(999999)")); });
}

int main()
{
    testThreadingModule();

    std::cout << "\n============================================\n";
    std::cout << "  Total: " << (g_passed + g_failed)
              << "  |  Passed: " << g_passed
              << "  |  Failed: " << g_failed << "\n";
    std::cout << "============================================\n";

    return g_failed == 0 ? 0 : 1;
}
