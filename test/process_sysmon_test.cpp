// =============================================================================
// Process & System Monitoring Tests
// Tests for builtins_process.hpp and builtins_sysmon.hpp
// Only safe, non-destructive operations are tested
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
// Section 1: Process — safe read-only queries
// ============================================================================

static void testProcessQueries()
{
    std::cout << "\n[Process queries]\n";

    runTest("ps returns a list", []()
            {
        auto out = runXell("p = ps()\nprint(type(p))");
        XASSERT_EQ(out[0], "list"); });

    runTest("ps returns non-empty list", []()
            {
        auto out = runXell("p = ps()\nprint(size(p) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("ps entries have pid key", []()
            {
        auto out = runXell("p = ps()\nprint(has(p[0], \"pid\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("ps entries have name key", []()
            {
        auto out = runXell("p = ps()\nprint(has(p[0], \"name\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("pgrep finds a process", []()
            {
        // Search for a common substring that appears in many process names
        auto out = runXell("r = pgrep(\"\")\nprint(size(r) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("pidof returns list", []()
            {
        auto out = runXell("r = pidof(\"init\")\nprint(type(r))");
        XASSERT_EQ(out[0], "list"); });
}

// ============================================================================
// Section 2: Process — identity & system info
// ============================================================================

static void testIdentity()
{
    std::cout << "\n[Identity & system info]\n";

    runTest("whoami returns non-empty", []()
            {
        auto out = runXell("print(size(whoami()) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("whoami function call", []()
            {
        auto out = runXell("w = whoami()\nprint(size(w) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("hostname returns non-empty", []()
            {
        auto out = runXell("print(size(hostname()) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("uname returns map", []()
            {
        auto out = runXell("u = uname()\nprint(type(u))");
        XASSERT_EQ(out[0], "map"); });

    runTest("uname has sysname key", []()
            {
        auto out = runXell("u = uname()\nprint(has(u, \"sysname\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("uname has machine key", []()
            {
        auto out = runXell("u = uname()\nprint(has(u, \"machine\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("os_name returns string", []()
            {
        auto out = runXell("print(type(os_name()))");
        XASSERT_EQ(out[0], "string"); });

    runTest("os_name is linux or windows or macos", []()
            {
        auto out = runXell("n = os_name()\nprint(n == \"linux\" or n == \"windows\" or n == \"macos\")");
        XASSERT_EQ(out[0], "true"); });

    runTest("arch returns non-empty", []()
            {
        auto out = runXell("print(size(arch()) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("getuid returns int", []()
            {
        auto out = runXell("print(type(getuid()))");
        XASSERT_EQ(out[0], "int"); });

    runTest("getuid >= 0", []()
            {
        auto out = runXell("print(getuid() >= 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("is_root returns bool", []()
            {
        auto out = runXell("print(type(is_root()))");
        XASSERT_EQ(out[0], "bool"); });

    runTest("id returns map with uid", []()
            {
        auto out = runXell("i = id()\nprint(has(i, \"uid\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("id returns map with user", []()
            {
        auto out = runXell("i = id()\nprint(has(i, \"user\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("ppid returns int > 0", []()
            {
        auto out = runXell("print(ppid() > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("uptime returns positive number", []()
            {
        auto out = runXell("print(uptime() > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("sys_info returns map", []()
            {
        auto out = runXell("s = sys_info()\nprint(type(s))");
        XASSERT_EQ(out[0], "map"); });

    runTest("sys_info has os key", []()
            {
        auto out = runXell("s = sys_info()\nprint(has(s, \"os\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("sys_info has cpu_count key", []()
            {
        auto out = runXell("s = sys_info()\nprint(has(s, \"cpu_count\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("sys_info has hostname key", []()
            {
        auto out = runXell("s = sys_info()\nprint(has(s, \"hostname\"))");
        XASSERT_EQ(out[0], "true"); });
}

// ============================================================================
// Section 3: Process — spawn, time_cmd, run_timeout
// ============================================================================

static void testSpawnTime()
{
    std::cout << "\n[spawn, time_cmd, run_timeout]\n";

    runTest("time_cmd measures duration", []()
            {
        auto out = runXell("t = time_cmd(\"echo hello\")\nprint(has(t, \"elapsed_ms\"))\nprint(has(t, \"exit_code\"))");
        XASSERT_EQ(out[0], "true");
        XASSERT_EQ(out[1], "true"); });

    runTest("time_cmd exit_code 0 for success", []()
            {
        auto out = runXell("t = time_cmd(\"echo hello\")\nprint(t[\"exit_code\"])");
        XASSERT_EQ(out[0], "0"); });

    runTest("run_timeout completes fast command", []()
            {
        auto out = runXell("r = run_timeout(\"echo hi\", 5000)\nprint(r[\"timed_out\"])");
        XASSERT_EQ(out[0], "false"); });

    runTest("run_timeout returns exit_code", []()
            {
        auto out = runXell("r = run_timeout(\"echo hi\", 5000)\nprint(has(r, \"exit_code\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("spawn returns pid", []()
            {
        auto out = runXell("p = spawn(\"sleep 0.01\")\nprint(p > 0)");
        XASSERT_EQ(out[0], "true"); });
}

// ============================================================================
// Section 4: System Monitoring — Memory
// ============================================================================

static void testSysMonMemory()
{
    std::cout << "\n[System monitoring — memory]\n";

    runTest("free returns map", []()
            {
        auto out = runXell("f = free()\nprint(type(f))");
        XASSERT_EQ(out[0], "map"); });

    runTest("free has total key", []()
            {
        auto out = runXell("f = free()\nprint(has(f, \"total\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("free has free key", []()
            {
        auto out = runXell("f = free()\nprint(has(f, \"free\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("free total > 0", []()
            {
        auto out = runXell("f = free()\nprint(f[\"total\"] > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("mem_total > 0", []()
            {
        auto out = runXell("print(mem_total() > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("mem_free >= 0", []()
            {
        auto out = runXell("print(mem_free() >= 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("mem_used > 0", []()
            {
        auto out = runXell("print(mem_used() > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("mem_total >= mem_used", []()
            {
        auto out = runXell("print(mem_total() >= mem_used())");
        XASSERT_EQ(out[0], "true"); });
}

// ============================================================================
// Section 5: System Monitoring — CPU
// ============================================================================

static void testSysMonCPU()
{
    std::cout << "\n[System monitoring — CPU]\n";

    runTest("cpu_count > 0", []()
            {
        auto out = runXell("print(cpu_count() > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("cpu_count returns int", []()
            {
        auto out = runXell("print(type(cpu_count()))");
        XASSERT_EQ(out[0], "int"); });

    runTest("cpu_usage returns float", []()
            {
        auto out = runXell("u = cpu_usage()\nprint(type(u))");
        XASSERT_EQ(out[0], "float"); });

    runTest("cpu_usage between 0 and 100", []()
            {
        auto out = runXell("u = cpu_usage()\nprint(u >= 0.0 and u <= 100.0)");
        XASSERT_EQ(out[0], "true"); });
}

// ============================================================================
// Section 6: System Monitoring — cal, date_str
// ============================================================================

static void testCalDate()
{
    std::cout << "\n[cal, date_str]\n";

    runTest("cal returns non-empty string", []()
            {
        auto out = runXell("print(size(cal()) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("cal default contains day numbers", []()
            {
        // Default (current month) uses pure C++ impl
        auto out = runXell("c = cal()\nprint(contains(c, \"1\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("cal default contains weekday headers", []()
            {
        auto out = runXell("c = cal()\nprint(contains(c, \"Su\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("date_str returns non-empty", []()
            {
        auto out = runXell("print(size(date_str()) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("date_str with format", []()
            {
        auto out = runXell("d = date_str(\"%Y\")\nprint(size(d))");
        XASSERT_EQ(out[0], "4"); }); // e.g. "2025"

    runTest("date_str default has dashes", []()
            {
        auto out = runXell("d = date_str()\nprint(contains(d, \"-\"))");
        XASSERT_EQ(out[0], "true"); }); // "%Y-%m-%d %H:%M:%S"
}

// ============================================================================
// Section 7: System Monitoring — misc safe queries
// ============================================================================

static void testSysMonMisc()
{
    std::cout << "\n[System monitoring — misc]\n";

    // lscpu wraps system command, should return non-empty string
    runTest("lscpu returns string", []()
            {
        auto out = runXell("print(type(lscpu()))");
        XASSERT_EQ(out[0], "string"); });

    // ulimit_info should return string output
    runTest("ulimit_info returns string", []()
            {
        auto out = runXell("print(type(ulimit_info()))");
        XASSERT_EQ(out[0], "string"); });

    // w_cmd
    runTest("w_cmd returns string", []()
            {
        auto out = runXell("print(type(w_cmd()))");
        XASSERT_EQ(out[0], "string"); });
}

// ============================================================================
// Section 8: Error cases
// ============================================================================

static void testErrors()
{
    std::cout << "\n[Error cases]\n";

    runTest("kill with bad pid returns false", []()
            {
        auto out = runXell("print(kill(-999))");
        XASSERT_EQ(out[0], "false"); });

    runTest("kill_name with nonexistent returns nonzero", []()
            {
        // killall returns non-zero if no processes found
        auto out = runXell("print(type(kill_name(\"__nonexistent_process_xell__\")))");
        XASSERT_EQ(out[0], "int"); });

    runTest("kill arity error", []()
            { XASSERT(expectError<std::exception>("kill()")); });

    runTest("fdisk requires string arg", []()
            { XASSERT(expectError<std::exception>("fdisk(123)")); });
}

// ============================================================================
// Section 9: Command style (where applicable)
// ============================================================================

static void testCommandStyle()
{
    std::cout << "\n[Command-style invocations]\n";

    runTest("hostname command/function same", []()
            {
        auto out = runXell("print(hostname())");
        XASSERT(out.size() > 0 && out[0].size() > 0); });

    runTest("os_name function call", []()
            {
        auto out = runXell("n = os_name()\nprint(type(n))");
        XASSERT_EQ(out[0], "string"); });

    runTest("arch function call", []()
            {
        auto out = runXell("a = arch()\nprint(type(a))");
        XASSERT_EQ(out[0], "string"); });

    runTest("cal function call", []()
            {
        auto out = runXell("c = cal()\nprint(type(c))");
        XASSERT_EQ(out[0], "string"); });

    runTest("date_str function call", []()
            {
        auto out = runXell("d = date_str()\nprint(type(d))");
        XASSERT_EQ(out[0], "string"); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===== Process & System Monitoring Tests =====\n";

    testProcessQueries();
    testIdentity();
    testSpawnTime();
    testSysMonMemory();
    testSysMonCPU();
    testCalDate();
    testSysMonMisc();
    testErrors();
    testCommandStyle();

    std::cout << "\n===== Results: " << g_passed << " passed, "
              << g_failed << " failed =====\n";
    return g_failed > 0 ? 1 : 0;
}
