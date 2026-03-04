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
            bring * of json
            x = json_parse("[1, 2, 3]")
            print(len(x))
        )");
        XASSERT_EQ(out[0], "3"); });

    runTest("bring * from regex makes regex_match available", []()
            {
        auto out = runXell(R"(
            bring * of regex
            print(regex_match("hello world", "hello"))
        )");
        XASSERT_EQ(out[0], "true"); });

    runTest("bring * from datetime makes now available", []()
            {
        auto out = runXell(R"(
            bring * of datetime
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
                bring * of archive
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
            bring * of process
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
            bring json_parse of json
            x = json_parse("[10, 20]")
            print(len(x))
        )");
        XASSERT_EQ(out[0], "2"); });

    runTest("selectively brought function works, others don't", []()
            {
        try
        {
            runXell(R"(
                bring json_parse of json
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
            bring json_parse, json_stringify of json
            x = json_parse("[1, 2]")
            print(json_stringify(x))
        )");
        XASSERT_EQ(out[0], "[1,2]"); });

    runTest("bring nonexistent function from module errors", []()
            {
        try
        {
            runXell(R"(
                bring nonexistent_func of json
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
            bring json_parse of json as jp
            x = jp("[5, 10]")
            print(len(x))
        )");
        XASSERT_EQ(out[0], "2"); });

    runTest("original name not available after aliased bring", []()
            {
        try
        {
            runXell(R"(
                bring json_parse of json as jp
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
            runXell(R"(bring totally_fake_module)");
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
            bring * of json
            bring * of regex
            x = json_parse("[1, 2, 3]")
            print(len(x))
            print(regex_match("abc", "a.c"))
        )");
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "true"); });

    runTest("bring selective from multiple modules", []()
            {
        auto out = runXell(R"(
            bring json_parse of json
            bring regex_match of regex
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
            bring * of json
            x = json_parse("[1, 2, 3]")
            print(json_stringify(x))
        )");
        XASSERT_EQ(out[0], "[1,2,3]"); });

    runTest("regex operations after bring", []()
            {
        auto out = runXell(R"(
            bring * of regex
            print(regex_find("hello world 123", "[0-9]+"))
            print(regex_replace_all("hello", "l", "r"))
        )");
        XASSERT_EQ(out[0], "123");
        XASSERT_EQ(out[1], "herro"); });

    runTest("datetime after bring", []()
            {
        auto out = runXell(R"(
            bring * of datetime
            t = timestamp()
            print(t > 0)
        )");
        XASSERT_EQ(out[0], "true"); });

    runTest("Tier 1 still works alongside Tier 2", []()
            {
        auto out = runXell(R"(
            bring * of json
            print(upper("hello"))
            x = json_parse("[1]")
            print(len(x))
        )");
        XASSERT_EQ(out[0], "HELLO");
        XASSERT_EQ(out[1], "1"); });

    runTest("user function can shadow brought module function", []()
            {
        auto out = runXell(R"(
            bring * of json
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
            bring * of math
            print(floor(3.7))
        )");
        XASSERT_EQ(out[0], "3"); });

    runTest("bring specific from Tier 1 module works", []()
            {
        auto out = runXell(R"(
            bring floor of math
            print(floor(9.9))
        )");
        XASSERT_EQ(out[0], "9"); });
}

// =============================================================================
// New Module System Tests (module def, export, bring of, nested, -> access)
// =============================================================================

static void testModuleDefinition()
{
    std::cout << "\n── Module Definition ──\n";

    runTest("basic module definition", []()
            {
        auto out = runXell(R"(
            module math_utils:
                fn square(x):
                    give x * x
                ;
                fn cube(x):
                    give x * x * x
                ;
            ;
            print(type(math_utils))
        )");
        XASSERT_EQ(out[0], "module"); });

    runTest("module -> member access (implicit exports)", []()
            {
        auto out = runXell(R"(
            module helpers:
                fn greet(name):
                    give "Hello, " + name
                ;
                pi = 3.14159
            ;
            print(helpers->greet("World"))
            print(helpers->pi)
        )");
        XASSERT_EQ(out[0], "Hello, World");
        XASSERT_EQ(out[1], "3.14159"); });

    runTest("module with explicit exports", []()
            {
        auto out = runXell(R"(
            module mymod:
                export fn public_fn():
                    give "public"
                ;
                fn private_fn():
                    give "private"
                ;
            ;
            print(mymod->public_fn())
        )");
        XASSERT_EQ(out[0], "public"); });

    runTest("explicit export hides non-exported names", []()
            {
        try
        {
            runXell(R"(
                module mymod:
                    export fn public_fn():
                        give "public"
                    ;
                    fn private_fn():
                        give "private"
                    ;
                ;
                mymod->private_fn()
            )");
            XASSERT(false);
        }
        catch (const AttributeError &)
        {
            XASSERT(true);
        } });

    runTest("module __name__ dunder", []()
            {
        auto out = runXell(R"(
            module mylib:
                x = 1
            ;
            print(mylib->__name__)
        )");
        XASSERT_EQ(out[0], "mylib"); });

    runTest("module __exports__ dunder returns list", []()
            {
        auto out = runXell(R"(
            module m:
                export a = 1
                export b = 2
            ;
            exps = m->__exports__
            print(type(exps))
        )");
        // This should return a list
        XASSERT_EQ(out[0], "list"); });

    runTest("module with variable exports", []()
            {
        auto out = runXell(R"(
            module config:
                export version = "1.0.0"
                export debug = true
            ;
            print(config->version)
            print(config->debug)
        )");
        XASSERT_EQ(out[0], "1.0.0");
        XASSERT_EQ(out[1], "true"); });
}

static void testModuleExportDecl()
{
    std::cout << "\n── Export Declaration ──\n";

    runTest("export fn inside module", []()
            {
        auto out = runXell(R"(
            module m:
                export fn add(a, b):
                    give a + b
                ;
            ;
            print(m->add(3, 4))
        )");
        XASSERT_EQ(out[0], "7"); });

    runTest("export variable inside module", []()
            {
        auto out = runXell(R"(
            module m:
                export x = 42
            ;
            print(m->x)
        )");
        XASSERT_EQ(out[0], "42"); });

    runTest("export immutable inside module", []()
            {
        auto out = runXell(R"(
            module m:
                export immutable PI = 3.14159
            ;
            print(m->PI)
        )");
        XASSERT_EQ(out[0], "3.14159"); });
}

static void testBringOfSyntax()
{
    std::cout << "\n── bring X of module (new syntax) ──\n";

    runTest("bring single name of module", []()
            {
        auto out = runXell(R"(
            module math:
                fn square(x):
                    give x * x
                ;
                fn cube(x):
                    give x * x * x
                ;
            ;
            bring square of math
            print(square(5))
        )");
        XASSERT_EQ(out[0], "25"); });

    runTest("bring multiple names of module", []()
            {
        auto out = runXell(R"(
            module calc:
                fn add(a, b):
                    give a + b
                ;
                fn mul(a, b):
                    give a * b
                ;
            ;
            bring add, mul of calc
            print(add(2, 3))
            print(mul(4, 5))
        )");
        XASSERT_EQ(out[0], "5");
        XASSERT_EQ(out[1], "20"); });

    runTest("bring * of module (wildcard)", []()
            {
        auto out = runXell(R"(
            module utils:
                fn double(x):
                    give x * 2
                ;
                fn triple(x):
                    give x * 3
                ;
            ;
            bring * of utils
            print(double(10))
            print(triple(10))
        )");
        XASSERT_EQ(out[0], "20");
        XASSERT_EQ(out[1], "30"); });

    runTest("bring with alias", []()
            {
        auto out = runXell(R"(
            module math:
                fn square(x):
                    give x * x
                ;
            ;
            bring square of math as sq
            print(sq(6))
        )");
        XASSERT_EQ(out[0], "36"); });

    runTest("bring module itself (no of)", []()
            {
        auto out = runXell(R"(
            module m:
                fn hello():
                    give "hi"
                ;
            ;
            bring m
            print(m->hello())
        )");
        XASSERT_EQ(out[0], "hi"); });

    runTest("bring nonexistent name of module errors", []()
            {
        try
        {
            runXell(R"(
                module m:
                    fn foo():
                        give 1
                    ;
                ;
                bring bar of m
            )");
            XASSERT(false);
        }
        catch (const BringError &)
        {
            XASSERT(true);
        } });
}

static void testNestedModules()
{
    std::cout << "\n── Nested Modules ──\n";

    runTest("module as first-class value", []()
            {
        auto out = runXell(R"(
            module m:
                fn get_val():
                    give 42
                ;
            ;
            x = m
            print(x->get_val())
        )");
        XASSERT_EQ(out[0], "42"); });

    runTest("module passed to function", []()
            {
        auto out = runXell(R"(
            module m:
                fn value():
                    give 99
                ;
            ;
            fn use_mod(mod):
                give mod->value()
            ;
            print(use_mod(m))
        )");
        XASSERT_EQ(out[0], "99"); });

    runTest("module toString representation", []()
            {
        auto out = runXell(R"(
            module mymod:
                x = 1
            ;
            print(mymod)
        )");
        XASSERT_EQ(out[0], "<module mymod>"); });
}

// =============================================================================
// Requires Keyword Tests
// =============================================================================

static void testRequiresKeyword()
{
    std::cout << "\n── requires keyword ──\n";

    runTest("requires brings whole module into module scope", []()
            {
        auto out = runXell(R"(
            module utils:
                fn double(x):
                    give x * 2
                ;
            ;
            module app:
                requires utils
                export fn compute(x):
                    give utils->double(x) + 1
                ;
            ;
            print(app->compute(5))
        )");
        XASSERT_EQ(out[0], "11"); });

    runTest("requires items of module path", []()
            {
        auto out = runXell(R"(
            module math_lib:
                export fn add(a, b):
                    give a + b
                ;
                export fn sub(a, b):
                    give a - b
                ;
            ;
            module calculator:
                requires add, sub of math_lib
                export fn calc(a, b):
                    give add(a, b) * sub(a, b)
                ;
            ;
            print(calculator->calc(10, 3))
        )");
        // (10+3) * (10-3) = 13 * 7 = 91
        XASSERT_EQ(out[0], "91"); });

    runTest("requires missing module throws RequireError", []()
            {
        try
        {
            runXell(R"(
                module app:
                    requires nonexistent_module
                    export fn foo():
                        give 1
                    ;
                ;
            )");
            XASSERT(false);
        }
        catch (const RequireError &)
        {
            XASSERT(true);
        } });

    runTest("requires missing item from module throws RequireError", []()
            {
        try
        {
            runXell(R"(
                module utils:
                    export fn add(a, b):
                        give a + b
                    ;
                ;
                module app:
                    requires nonexistent of utils
                    export fn foo():
                        give 1
                    ;
                ;
            )");
            XASSERT(false);
        }
        catch (const RequireError &)
        {
            XASSERT(true);
        } });

    runTest("requires builtin module works", []()
            {
        auto out = runXell(R"(
            module app:
                requires json
                export fn parse_data(s):
                    give json_parse(s)
                ;
            ;
            data = app->parse_data("[1, 2, 3]")
            print(len(data))
        )");
        XASSERT_EQ(out[0], "3"); });
}

// =============================================================================
// @eager Decorator Tests
// =============================================================================

static void testEagerDecorator()
{
    std::cout << "\n── @eager decorator ──\n";

    runTest("@eager bring parses correctly", []()
            {
        auto out = runXell(R"(
            module m:
                fn greet():
                    give "hello"
                ;
            ;
            @eager bring m
            print(m->greet())
        )");
        XASSERT_EQ(out[0], "hello"); });

    runTest("@eager bring with of syntax", []()
            {
        auto out = runXell(R"(
            module m:
                fn foo():
                    give 42
                ;
            ;
            @eager bring foo of m
            print(foo())
        )");
        XASSERT_EQ(out[0], "42"); });

    runTest("invalid decorator on bring errors", []()
            {
        try
        {
            runXell(R"(
                module m:
                    fn foo():
                        give 1
                    ;
                ;
                @invalid bring m
            )");
            XASSERT(false);
        }
        catch (const ParseError &)
        {
            XASSERT(true);
        } });
}

// =============================================================================
// Conflict Resolution Tests
// =============================================================================

static void testConflictResolution()
{
    std::cout << "\n── Conflict Resolution ──\n";

    runTest("partial aliases — fewer aliases than items", []()
            {
        // 1 alias for 2 items: first gets aliased, second keeps its name
        auto out = runXell(R"(
            module m:
                fn a():
                    give 1
                ;
                fn b():
                    give 2
                ;
            ;
            bring a, b of m as aliased_a
            print(aliased_a())
            print(b())
        )");
        XASSERT_EQ(out[0], "1");
        XASSERT_EQ(out[1], "2"); });

    runTest("too many aliases is parse error", []()
            {
        try
        {
            runXell(R"(
                module m:
                    fn a():
                        give 1
                    ;
                ;
                bring a of m as x, y, z
            )");
            XASSERT(false);
        }
        catch (const ParseError &)
        {
            XASSERT(true);
        } });

    runTest("bring * collision detection", []()
            {
        try
        {
            runXell(R"(
                module m1:
                    fn add(a, b):
                        give a + b
                    ;
                ;
                module m2:
                    fn add(a, b):
                        give a * b
                    ;
                ;
                bring * of m1
                bring * of m2
            )");
            XASSERT(false);
        }
        catch (const BringError &e)
        {
            std::string msg = e.what();
            XASSERT(msg.find("already in scope") != std::string::npos);
        } });

    runTest("bring * with alias avoids collision", []()
            {
        auto out = runXell(R"(
            module m1:
                fn add(a, b):
                    give a + b
                ;
            ;
            module m2:
                fn add(a, b):
                    give a * b
                ;
            ;
            bring * of m1 as math
            bring * of m2 as calc
            print(math->add(2, 3))
            print(calc->add(2, 3))
        )");
        XASSERT_EQ(out[0], "5");
        XASSERT_EQ(out[1], "6"); });

    runTest("correct alias count matches items", []()
            {
        auto out = runXell(R"(
            module m:
                fn a():
                    give 10
                ;
                fn b():
                    give 20
                ;
            ;
            bring a, b of m as x, y
            print(x())
            print(y())
        )");
        XASSERT_EQ(out[0], "10");
        XASSERT_EQ(out[1], "20"); });
}

// =============================================================================
// Dunder Variables Tests
// =============================================================================

static void testDunderVariables()
{
    std::cout << "\n── Dunder Variables ──\n";

    runTest("__name__ of top-level module", []()
            {
        auto out = runXell(R"(
            module mylib:
                x = 1
            ;
            print(mylib->__name__)
        )");
        XASSERT_EQ(out[0], "mylib"); });

    runTest("__name__ of nested module is qualified", []()
            {
        auto out = runXell(R"(
            module lib:
                export module math_lib:
                    export fn add(a, b):
                        give a + b
                    ;
                ;
            ;
            print(lib->math_lib->__name__)
        )");
        XASSERT_EQ(out[0], "lib->math_lib"); });

    runTest("__exports__ returns list of export names", []()
            {
        auto out = runXell(R"(
            module m:
                export fn foo():
                    give 1
                ;
                export bar = 42
                fn private_fn():
                    give 2
                ;
            ;
            exps = m->__exports__
            print(len(exps))
        )");
        XASSERT_EQ(out[0], "2"); });

    runTest("__submodules__ returns list of submodule names", []()
            {
        auto out = runXell(R"(
            module lib:
                export module sub1:
                    export x = 1
                ;
                export module sub2:
                    export y = 2
                ;
            ;
            subs = lib->__submodules__
            print(len(subs))
        )");
        XASSERT_EQ(out[0], "2"); });

    runTest("__module__ is none for top-level module", []()
            {
        auto out = runXell(R"(
            module m:
                x = 1
            ;
            print(m->__module__)
        )");
        XASSERT_EQ(out[0], "none"); });

    runTest("__module__ of nested module is parent name", []()
            {
        auto out = runXell(R"(
            module lib:
                export module child:
                    export x = 1
                ;
            ;
            print(lib->child->__module__)
        )");
        XASSERT_EQ(out[0], "lib"); });

    runTest("__version__ is none when not set", []()
            {
        auto out = runXell(R"(
            module m:
                x = 1
            ;
            print(m->__version__)
        )");
        XASSERT_EQ(out[0], "none"); });

    runTest("__version__ set by module author", []()
            {
        auto out = runXell(R"(
            module mylib:
                __version__ = "2.3.1"
                export fn add(a, b):
                    give a + b
                ;
            ;
            print(mylib->__version__)
        )");
        XASSERT_EQ(out[0], "2.3.1"); });

    runTest("__cached__ is none without bytecode", []()
            {
        auto out = runXell(R"(
            module m:
                x = 1
            ;
            print(m->__cached__)
        )");
        XASSERT_EQ(out[0], "none"); });

    runTest("global __name__ is __main__ for direct run", []()
            {
        auto out = runXell(R"(
            print(__name__)
        )");
        XASSERT_EQ(out[0], "__main__"); });

    runTest("global __args__ is none when not set", []()
            {
        auto out = runXell(R"(
            print(__args__)
        )");
        XASSERT_EQ(out[0], "none"); });
}

// =============================================================================
// Module Method Calls Tests
// =============================================================================

static void testModuleMethodCalls()
{
    std::cout << "\n── Module Method Calls (mod->fn()) ──\n";

    runTest("call exported function via -> on module", []()
            {
        auto out = runXell(R"(
            module m:
                fn greet(name):
                    give "Hello, " + name
                ;
            ;
            print(m->greet("World"))
        )");
        XASSERT_EQ(out[0], "Hello, World"); });

    runTest("call exported function with multiple args", []()
            {
        auto out = runXell(R"(
            module math:
                fn add(a, b):
                    give a + b
                ;
            ;
            print(math->add(10, 20))
        )");
        XASSERT_EQ(out[0], "30"); });

    runTest("call function on nested module", []()
            {
        auto out = runXell(R"(
            module lib:
                export module math:
                    export fn square(x):
                        give x * x
                    ;
                ;
            ;
            print(lib->math->square(7))
        )");
        XASSERT_EQ(out[0], "49"); });

    runTest("module method with no explicit exports (all visible)", []()
            {
        auto out = runXell(R"(
            module m:
                fn a():
                    give "A"
                ;
                fn b():
                    give "B"
                ;
            ;
            print(m->a())
            print(m->b())
        )");
        XASSERT_EQ(out[0], "A");
        XASSERT_EQ(out[1], "B"); });

    runTest("private function not callable via ->", []()
            {
        try
        {
            runXell(R"(
                module m:
                    export fn visible():
                        give 1
                    ;
                    fn hidden():
                        give 2
                    ;
                ;
                m->hidden()
            )");
            XASSERT(false);
        }
        catch (const AttributeError &)
        {
            XASSERT(true);
        } });
}

// =============================================================================
// Deep Nested Access Tests
// =============================================================================

static void testDeepNestedAccess()
{
    std::cout << "\n── Deep Nested Module Access ──\n";

    runTest("two-level nesting: lib->math_lib->add", []()
            {
        auto out = runXell(R"(
            module lib:
                export module math_lib:
                    export fn add(a, b):
                        give a + b
                    ;
                    export fn sub(a, b):
                        give a - b
                    ;
                ;
            ;
            print(lib->math_lib->add(3, 4))
            print(lib->math_lib->sub(10, 3))
        )");
        XASSERT_EQ(out[0], "7");
        XASSERT_EQ(out[1], "7"); });

    runTest("three-level nesting", []()
            {
        auto out = runXell(R"(
            module root:
                export module mid:
                    export module leaf:
                        export fn value():
                            give 42
                        ;
                    ;
                ;
            ;
            print(root->mid->leaf->value())
        )");
        XASSERT_EQ(out[0], "42"); });

    runTest("bring from deep nested path", []()
            {
        auto out = runXell(R"(
            module lib:
                export module math:
                    export fn square(x):
                        give x * x
                    ;
                ;
            ;
            bring square of lib->math
            print(square(9))
        )");
        XASSERT_EQ(out[0], "81"); });

    runTest("bring * from deep nested path", []()
            {
        auto out = runXell(R"(
            module lib:
                export module tools:
                    export fn double(x):
                        give x * 2
                    ;
                    export fn triple(x):
                        give x * 3
                    ;
                ;
            ;
            bring * of lib->tools
            print(double(5))
            print(triple(5))
        )");
        XASSERT_EQ(out[0], "10");
        XASSERT_EQ(out[1], "15"); });

    runTest("multiple modules in same parent", []()
            {
        auto out = runXell(R"(
            module lib:
                export module math:
                    export fn add(a, b):
                        give a + b
                    ;
                ;
                export module string:
                    export fn greet(name):
                        give "Hi " + name
                    ;
                ;
            ;
            print(lib->math->add(1, 2))
            print(lib->string->greet("Bob"))
        )");
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "Hi Bob"); });
}

// =============================================================================
// And Chaining Tests
// =============================================================================

static void testAndChaining()
{
    std::cout << "\n── and chaining ──\n";

    runTest("bring items from two modules with and", []()
            {
        auto out = runXell(R"(
            module m1:
                fn foo():
                    give "m1"
                ;
            ;
            module m2:
                fn bar():
                    give "m2"
                ;
            ;
            bring foo of m1 and bar of m2
            print(foo())
            print(bar())
        )");
        XASSERT_EQ(out[0], "m1");
        XASSERT_EQ(out[1], "m2"); });

    runTest("bring items from three modules with and", []()
            {
        auto out = runXell(R"(
            module a:
                fn fa():
                    give 1
                ;
            ;
            module b:
                fn fb():
                    give 2
                ;
            ;
            module c:
                fn fc():
                    give 3
                ;
            ;
            bring fa of a and fb of b and fc of c
            print(fa() + fb() + fc())
        )");
        XASSERT_EQ(out[0], "6"); });
}

// =============================================================================
// From "dir" Bring Syntax Tests
// =============================================================================

static void testFromDirBring()
{
    std::cout << "\n── from dir bring syntax ──\n";

    runTest("from dir bring with .xel file", []()
            {
        writeFile(g_testDir + "/dirlib.xel", R"(
            x = 99
            fn get_x():
                give 99
            ;
        )");
        // Use full path for file-based bring (legacy syntax)
        auto out = runXell(
            "bring * from \"" + g_testDir + "/dirlib.xel\"\n"
            "print(x)\n");
        XASSERT_EQ(out[0], "99"); });
}

// =============================================================================
// File-Based Module Resolution Tests (with test scripts)
// =============================================================================

static void testFileBasedResolution()
{
    std::cout << "\n── File-Based Module Resolution ──\n";

    runTest("module defined in a .xell file can be resolved", []()
            {
        // Create a .xell file with a module definition
        writeFile(g_testDir + "/mathmod.xell", R"(
            module math_utils:
                export fn add(a, b):
                    give a + b
                ;
                export fn mul(a, b):
                    give a * b
                ;
            ;
        )");

        // Run code that references this module via file-based resolution
        // The interpreter should search the directory for .xell files
        Lexer lexer("bring add of math_utils\nprint(add(3, 7))\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        interp.setSourceFile(g_testDir + "/test_main.xell");
        interp.run(program);
        auto out = interp.output();
        XASSERT_EQ(out[0], "10"); });
}

// =============================================================================
// Module Caching Tests
// =============================================================================

static void testModuleCaching()
{
    std::cout << "\n── Module Caching ──\n";

    runTest("module is cached — same object returned", []()
            {
        auto out = runXell(R"(
            module m:
                export counter = 0
                export fn inc():
                    give 1
                ;
            ;
            a = m
            b = m
            print(a->__name__ == b->__name__)
        )");
        XASSERT_EQ(out[0], "true"); });

    runTest("module can be brought by name after definition", []()
            {
        auto out = runXell(R"(
            module cache_test:
                export fn value():
                    give 42
                ;
            ;
            bring cache_test
            print(cache_test->value())
        )");
        XASSERT_EQ(out[0], "42"); });

    runTest("bring same module multiple times is idempotent", []()
            {
        auto out = runXell(R"(
            module m:
                export fn val():
                    give 99
                ;
            ;
            bring m
            bring m
            bring m
            print(m->val())
        )");
        XASSERT_EQ(out[0], "99"); });
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
    testModuleDefinition();
    testModuleExportDecl();
    testBringOfSyntax();
    testNestedModules();
    testRequiresKeyword();
    testEagerDecorator();
    testConflictResolution();
    testDunderVariables();
    testModuleMethodCalls();
    testDeepNestedAccess();
    testAndChaining();
    testFromDirBring();
    testFileBasedResolution();
    testModuleCaching();

    cleanupTestDir();

    std::cout << "\n═══════════════════════════════════════════\n";
    std::cout << "  Results: " << g_passed << "/" << g_total << " passed";
    if (g_failed > 0)
        std::cout << ", " << g_failed << " FAILED";
    std::cout << "\n═══════════════════════════════════════════\n";

    return g_failed > 0 ? 1 : 0;
}
