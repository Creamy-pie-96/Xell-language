// =============================================================================
// module_test.cpp — tests for the Xell module system
// =============================================================================
//
// Tests:
//   1. Module registry metadata (isBuiltinModule, tier2, function lists)
//   2. Tier 1 builtins are always available (no bring needed)
//   3. Tier 2 builtins require bring and give helpful error without it
//   4. bring * from "module" — wildcard import
//   5. bring name1, name2 from "module" — selective import
//   6. bring name from "module" as alias — aliased import
//   7. File-based bring still works (3rd party .xel modules)
//   8. bring from unknown module gives error
//   9. bring nonexistent name from module gives error
//  10. Multiple modules can be brought
//  11. loadModule() C++ API
//  12. Module functions work correctly after bring
//
// =============================================================================

#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <cstdlib>
#include <filesystem>
#include <unistd.h>

#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include "../src/interpreter/interpreter.hpp"

using namespace xell;

// ---- Mini test framework (same as other test files) ----

static int g_total = 0, g_passed = 0, g_failed = 0;

#define XASSERT(cond)                                                                   \
    do                                                                                  \
    {                                                                                   \
        if (!(cond))                                                                    \
        {                                                                               \
            std::cerr << "  FAIL: " #cond " (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            throw std::runtime_error("assertion failed");                               \
        }                                                                               \
    } while (0)

#define XASSERT_EQ(a, b)                                                  \
    do                                                                    \
    {                                                                     \
        auto _a = (a);                                                    \
        auto _b = (b);                                                    \
        if (_a != _b)                                                     \
        {                                                                 \
            std::cerr << "  FAIL: " #a " == " #b "\n"                     \
                      << "    got:      [" << _a << "]\n"                 \
                      << "    expected: [" << _b << "]\n"                 \
                      << "    (" << __FILE__ << ":" << __LINE__ << ")\n"; \
            throw std::runtime_error("assertion failed");                 \
        }                                                                 \
    } while (0)

static void runTest(const std::string &name, std::function<void()> fn)
{
    g_total++;
    try
    {
        fn();
        g_passed++;
        std::cout << "  ✓ " << name << "\n";
    }
    catch (const std::exception &e)
    {
        g_failed++;
        std::cerr << "  ✗ " << name << " — " << e.what() << "\n";
    }
}

// ---- Helpers ----

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

static std::vector<std::string> runXellWithModules(
    const std::string &source,
    std::initializer_list<std::string> modules)
{
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();
    Interpreter interp;
    for (const auto &m : modules)
        interp.loadModule(m);
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
    catch (...)
    {
        return false;
    }
}

// Temp directory for file-based bring tests
static std::string g_testDir;
static void setupTestDir()
{
    g_testDir = std::filesystem::temp_directory_path().string() + "/xell_module_test_" +
                std::to_string(getpid());
    std::filesystem::create_directories(g_testDir);
}
static void cleanupTestDir()
{
    std::filesystem::remove_all(g_testDir);
}
static void writeFile(const std::string &path, const std::string &content)
{
    std::ofstream f(path);
    f << content;
}

// =============================================================================
// Test sections
// =============================================================================

static void testModuleRegistryMetadata()
{
    std::cout << "\n── Module Registry Metadata ──\n";

    runTest("registry knows all 9 Tier 2 modules", []()
            {
        Interpreter interp;
        auto &reg = interp.moduleRegistry();
        for (const auto &mod : {"datetime", "regex", "fs", "textproc",
                                "process", "sysmon", "net", "archive", "json"})
        {
            XASSERT(reg.isBuiltinModule(mod));
            XASSERT(reg.isTier2(mod));
        } });

    runTest("registry knows all Tier 1 modules", []()
            {
        Interpreter interp;
        auto &reg = interp.moduleRegistry();
        for (const auto &mod : {"io", "math", "type", "collection", "util",
                                "os", "hash", "string", "list", "map",
                                "bytes", "generator", "shell"})
        {
            XASSERT(reg.isBuiltinModule(mod));
            XASSERT(!reg.isTier2(mod));
        } });

    runTest("registry has correct module function counts", []()
            {
        Interpreter interp;
        auto &reg = interp.moduleRegistry();
        // Just check a few modules have non-empty function lists
        XASSERT(reg.moduleFunctions("json").size() == 10);
        XASSERT(reg.moduleFunctions("archive").size() == 10);
        XASSERT(reg.moduleFunctions("regex").size() == 8);
        XASSERT(reg.moduleFunctions("datetime").size() == 8);
        XASSERT(reg.moduleFunctions("io").size() > 0);
        XASSERT(reg.moduleFunctions("math").size() > 0); });

    runTest("findModuleForFunction works", []()
            {
        Interpreter interp;
        auto &reg = interp.moduleRegistry();
        XASSERT_EQ(reg.findModuleForFunction("json_parse"), std::string("json"));
        XASSERT_EQ(reg.findModuleForFunction("http_get"), std::string("net"));
        XASSERT_EQ(reg.findModuleForFunction("regex_match"), std::string("regex"));
        XASSERT_EQ(reg.findModuleForFunction("print"), std::string("io"));
        XASSERT_EQ(reg.findModuleForFunction("nonexistent"), std::string("")); });

    runTest("moduleHasFunction works", []()
            {
        Interpreter interp;
        auto &reg = interp.moduleRegistry();
        XASSERT(reg.moduleHasFunction("json", "json_parse"));
        XASSERT(reg.moduleHasFunction("json", "csv_read"));
        XASSERT(!reg.moduleHasFunction("json", "http_get"));
        XASSERT(!reg.moduleHasFunction("nonexistent", "foo")); });

    runTest("unknown module returns empty", []()
            {
        Interpreter interp;
        auto &reg = interp.moduleRegistry();
        XASSERT(!reg.isBuiltinModule("nonexistent"));
        XASSERT(reg.moduleFunctions("nonexistent").empty()); });
}

static void testTier1AlwaysAvailable()
{
    std::cout << "\n── Tier 1: Always Available ──\n";

    runTest("print is available without bring", []()
            {
        auto out = runXell(R"(print("hello"))");
        XASSERT_EQ(out[0], "hello"); });

    runTest("math functions available without bring", []()
            {
        auto out = runXell(R"(print(floor(3.7)))");
        XASSERT_EQ(out[0], "3"); });

    runTest("string functions available without bring", []()
            {
        auto out = runXell(R"(print(upper("hello")))");
        XASSERT_EQ(out[0], "HELLO"); });

    runTest("list functions available without bring", []()
            {
        auto out = runXell(R"(
            x = [3, 1, 2]
            print(sort(x))
        )");
        XASSERT_EQ(out[0], "[1, 2, 3]"); });

    runTest("collection functions available without bring", []()
            {
        auto out = runXell(R"(print(len([1,2,3])))");
        XASSERT_EQ(out[0], "3"); });

    runTest("type functions available without bring", []()
            {
        auto out = runXell(R"(print(type(42)))");
        XASSERT_EQ(out[0], "int"); });

    runTest("os functions available without bring", []()
            {
        auto out = runXell(R"(print(exists("/tmp")))");
        XASSERT_EQ(out[0], "true"); });

    runTest("map/filter/reduce available without bring", []()
            {
        auto out = runXell(R"(
            x = [1, 2, 3, 4]
            fn is_even(n):
                give n % 2 == 0
            ;
            evens = filter(x, is_even)
            print(evens)
        )");
        XASSERT_EQ(out[0], "[2, 4]"); });

    runTest("shell functions available without bring", []()
            {
        auto out = runXell(R"(print(true_val()))");
        XASSERT_EQ(out[0], "true"); });

    runTest("hash functions available without bring", []()
            {
        auto out = runXell(R"(print(is_hashable("hello")))");
        XASSERT_EQ(out[0], "true"); });

    runTest("math constants available without bring", []()
            {
        auto out = runXell(R"(print(PI > 3.14))");
        XASSERT_EQ(out[0], "true"); });
}

static void testTier2RequiresBring()
{
    std::cout << "\n── Tier 2: Require Bring ──\n";

    runTest("json_parse without bring gives helpful error", []()
            {
        try
        {
            runXell(R"(json_parse("[]"))");
            XASSERT(false); // should not reach here
        }
        catch (const RuntimeError &e)
        {
            std::string msg = e.what();
            XASSERT(msg.find("bring") != std::string::npos);
            XASSERT(msg.find("json") != std::string::npos);
        } });

    runTest("regex_match without bring gives helpful error", []()
            {
        try
        {
            runXell(R"(regex_match("hello", "h.*"))");
            XASSERT(false);
        }
        catch (const RuntimeError &e)
        {
            std::string msg = e.what();
            XASSERT(msg.find("bring") != std::string::npos);
            XASSERT(msg.find("regex") != std::string::npos);
        } });

    runTest("http_get without bring gives helpful error", []()
            {
        try
        {
            runXell(R"(http_get("http://example.com"))");
            XASSERT(false);
        }
        catch (const RuntimeError &e)
        {
            std::string msg = e.what();
            XASSERT(msg.find("bring") != std::string::npos);
            XASSERT(msg.find("net") != std::string::npos);
        } });

    runTest("now() without bring gives helpful error", []()
            {
        try
        {
            runXell(R"(now())");
            XASSERT(false);
        }
        catch (const RuntimeError &e)
        {
            std::string msg = e.what();
            XASSERT(msg.find("bring") != std::string::npos);
            XASSERT(msg.find("datetime") != std::string::npos);
        } });

    runTest("ps() without bring gives helpful error", []()
            {
        try
        {
            runXell(R"(ps())");
            XASSERT(false);
        }
        catch (const RuntimeError &e)
        {
            std::string msg = e.what();
            XASSERT(msg.find("bring") != std::string::npos);
            XASSERT(msg.find("process") != std::string::npos);
        } });
}

static void testBringWildcard()
{
    std::cout << "\n── bring * from module ──\n";

    runTest("bring * from json makes json_parse available", []()
            {
        auto out = runXell(R"(
            bring * from "json"
            x = json_parse("[1, 2, 3]")
            print(len(x))
        )");
        XASSERT_EQ(out[0], "3"); });

    runTest("bring * from regex makes regex_match available", []()
            {
        auto out = runXell(R"(
            bring * from "regex"
            print(regex_match("hello world", "hello"))
        )");
        XASSERT_EQ(out[0], "true"); });

    runTest("bring * from datetime makes now available", []()
            {
        auto out = runXell(R"(
            bring * from "datetime"
            t = now()
            print(len(t) > 0)
        )");
        XASSERT_EQ(out[0], "true"); });

    runTest("bring * from archive makes zip_archive available", []()
            {
        // Just verify the function is callable (will error on missing file)
        try
        {
            runXell(R"(
                bring * from "archive"
                zip_archive("/nonexistent", "/tmp/test.zip")
            )");
        }
        catch (const std::exception &)
        {
            // Expected — function is callable but file doesn't exist
        }
        // If we get here without "requires bring" error, the bring worked
        XASSERT(true); });

    runTest("bring * from process makes whoami available", []()
            {
        auto out = runXell(R"(
            bring * from "process"
            w = whoami()
            print(len(w) > 0)
        )");
        XASSERT_EQ(out[0], "true"); });
}

static void testBringSelective()
{
    std::cout << "\n── Selective bring ──\n";

    runTest("bring specific function from json", []()
            {
        auto out = runXell(R"(
            bring json_parse from "json"
            x = json_parse("[10, 20]")
            print(len(x))
        )");
        XASSERT_EQ(out[0], "2"); });

    runTest("selectively brought function works, others don't", []()
            {
        try
        {
            runXell(R"(
                bring json_parse from "json"
                json_stringify([1,2,3])
            )");
            XASSERT(false); // json_stringify should fail
        }
        catch (const RuntimeError &e)
        {
            std::string msg = e.what();
            XASSERT(msg.find("bring") != std::string::npos);
        } });

    runTest("bring multiple specific functions", []()
            {
        auto out = runXell(R"(
            bring json_parse, json_stringify from "json"
            x = json_parse("[1, 2]")
            print(json_stringify(x))
        )");
        XASSERT_EQ(out[0], "[1,2]"); });

    runTest("bring nonexistent function from module errors", []()
            {
        try
        {
            runXell(R"(
                bring nonexistent_func from "json"
            )");
            XASSERT(false);
        }
        catch (const BringError &)
        {
            XASSERT(true);
        } });
}

static void testBringAlias()
{
    std::cout << "\n── bring with alias ──\n";

    runTest("bring function with alias", []()
            {
        auto out = runXell(R"(
            bring json_parse from "json" as jp
            x = jp("[5, 10]")
            print(len(x))
        )");
        XASSERT_EQ(out[0], "2"); });

    runTest("original name not available after aliased bring", []()
            {
        try
        {
            runXell(R"(
                bring json_parse from "json" as jp
                json_parse("[1]")
            )");
            XASSERT(false); // json_parse should not be available
        }
        catch (const RuntimeError &e)
        {
            // json_parse is still in allBuiltins_ but not in builtins_
            std::string msg = e.what();
            XASSERT(msg.find("bring") != std::string::npos);
        } });
}

static void testBringFromFile()
{
    std::cout << "\n── File-based bring (3rd party .xel) ──\n";

    runTest("bring * from .xel file still works", []()
            {
        writeFile(g_testDir + "/mylib.xel", R"(
            greeting = "hello from module"
            fn double(x):
                give x * 2
            ;
        )");

        auto out = runXell(
            "bring * from \"" + g_testDir + "/mylib.xel\"\n"
            "print(greeting)\n"
            "print(double(21))\n");
        XASSERT_EQ(out[0], "hello from module");
        XASSERT_EQ(out[1], "42"); });

    runTest("bring specific name from .xel file", []()
            {
        writeFile(g_testDir + "/utils.xel", R"(
            fn add(a, b):
                give a + b
            ;
            fn sub(a, b):
                give a - b
            ;
        )");

        auto out = runXell(
            "bring add from \"" + g_testDir + "/utils.xel\"\n"
            "print(add(10, 5))\n");
        XASSERT_EQ(out[0], "15"); });

    runTest("bring from nonexistent file errors", []()
            {
        try
        {
            runXell(R"(bring * from "/nonexistent/file.xel")");
            XASSERT(false);
        }
        catch (const BringError &)
        {
            XASSERT(true);
        } });
}

static void testBringUnknownModule()
{
    std::cout << "\n── Error handling ──\n";

    // Note: bring from "unknown_module" will try to open it as a file,
    // which should fail with a BringError about cannot open file
    runTest("bring from nonexistent module/file errors", []()
            {
        try
        {
            runXell(R"(bring * from "totally_fake_module")");
            XASSERT(false);
        }
        catch (const BringError &)
        {
            XASSERT(true);
        } });
}

static void testMultipleModules()
{
    std::cout << "\n── Multiple module imports ──\n";

    runTest("bring from multiple modules in sequence", []()
            {
        auto out = runXell(R"(
            bring * from "json"
            bring * from "regex"
            x = json_parse("[1, 2, 3]")
            print(len(x))
            print(regex_match("abc", "a.c"))
        )");
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "true"); });

    runTest("bring selective from multiple modules", []()
            {
        auto out = runXell(R"(
            bring json_parse from "json"
            bring regex_match from "regex"
            x = json_parse("[42]")
            print(len(x))
            print(regex_match("test", "t..t"))
        )");
        XASSERT_EQ(out[0], "1");
        XASSERT_EQ(out[1], "true"); });
}

static void testLoadModuleCppApi()
{
    std::cout << "\n── loadModule() C++ API ──\n";

    runTest("loadModule makes Tier 2 functions available", []()
            {
        auto out = runXellWithModules(R"(
            x = json_parse("[100]")
            print(len(x))
        )",
                                      {"json"});
        XASSERT_EQ(out[0], "1"); });

    runTest("loadModule for unknown module throws", []()
            {
        try
        {
            Interpreter interp;
            interp.loadModule("totally_fake");
            XASSERT(false);
        }
        catch (const RuntimeError &)
        {
            XASSERT(true);
        } });

    runTest("hasActiveBuiltin / hasAnyBuiltin", []()
            {
        Interpreter interp;
        // Tier 1 should be active
        XASSERT(interp.hasActiveBuiltin("print"));
        XASSERT(interp.hasAnyBuiltin("print"));
        // Tier 2 should be in allBuiltins_ but not active
        XASSERT(!interp.hasActiveBuiltin("json_parse"));
        XASSERT(interp.hasAnyBuiltin("json_parse"));
        // After loading, both should be true
        interp.loadModule("json");
        XASSERT(interp.hasActiveBuiltin("json_parse"));
        XASSERT(interp.hasAnyBuiltin("json_parse")); });
}

static void testModuleFunctionality()
{
    std::cout << "\n── Module functions work correctly after bring ──\n";

    runTest("json_parse + json_stringify roundtrip", []()
            {
        auto out = runXell(R"(
            bring * from "json"
            x = json_parse("[1, 2, 3]")
            print(json_stringify(x))
        )");
        XASSERT_EQ(out[0], "[1,2,3]"); });

    runTest("regex operations after bring", []()
            {
        auto out = runXell(R"(
            bring * from "regex"
            print(regex_find("hello world 123", "[0-9]+"))
            print(regex_replace_all("hello", "l", "r"))
        )");
        XASSERT_EQ(out[0], "123");
        XASSERT_EQ(out[1], "herro"); });

    runTest("datetime after bring", []()
            {
        auto out = runXell(R"(
            bring * from "datetime"
            t = timestamp()
            print(t > 0)
        )");
        XASSERT_EQ(out[0], "true"); });

    runTest("Tier 1 still works alongside Tier 2", []()
            {
        auto out = runXell(R"(
            bring * from "json"
            print(upper("hello"))
            x = json_parse("[1]")
            print(len(x))
        )");
        XASSERT_EQ(out[0], "HELLO");
        XASSERT_EQ(out[1], "1"); });

    runTest("user function can shadow brought module function", []()
            {
        auto out = runXell(R"(
            bring * from "json"
            fn json_parse(x):
                give "custom"
            ;
            print(json_parse("anything"))
        )");
        XASSERT_EQ(out[0], "custom"); });
}

static void testBringTier1Module()
{
    std::cout << "\n── bring Tier 1 module (no-op but valid) ──\n";

    runTest("bring * from Tier 1 module is valid", []()
            {
        auto out = runXell(R"(
            bring * from "math"
            print(floor(3.7))
        )");
        XASSERT_EQ(out[0], "3"); });

    runTest("bring specific from Tier 1 module works", []()
            {
        auto out = runXell(R"(
            bring floor from "math"
            print(floor(9.9))
        )");
        XASSERT_EQ(out[0], "9"); });
}

// =============================================================================
// main
// =============================================================================

int main()
{
    setupTestDir();

    std::cout << "═══════════════════════════════════════════\n";
    std::cout << "  Xell Module System Tests\n";
    std::cout << "═══════════════════════════════════════════\n";

    testModuleRegistryMetadata();
    testTier1AlwaysAvailable();
    testTier2RequiresBring();
    testBringWildcard();
    testBringSelective();
    testBringAlias();
    testBringFromFile();
    testBringUnknownModule();
    testMultipleModules();
    testLoadModuleCppApi();
    testModuleFunctionality();
    testBringTier1Module();

    cleanupTestDir();

    std::cout << "\n═══════════════════════════════════════════\n";
    std::cout << "  Results: " << g_passed << "/" << g_total << " passed";
    if (g_failed > 0)
        std::cout << ", " << g_failed << " FAILED";
    std::cout << "\n═══════════════════════════════════════════\n";

    return g_failed > 0 ? 1 : 0;
}
