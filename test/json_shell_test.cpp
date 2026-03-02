// =============================================================================
// JSON/Data & Shell Utility Tests
// Tests for builtins_json.hpp and builtins_shell.hpp
// =============================================================================

#include "../src/interpreter/interpreter.hpp"
#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include <iostream>
#include <sstream>
#include <functional>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <unistd.h>

namespace fs = std::filesystem;
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
    catch (...)
    {
        return false;
    }
}

// Create a temp directory for file-based tests
static std::string g_tmpDir;

static void setupTmpDir()
{
    g_tmpDir = fs::temp_directory_path().string() + "/xell_json_test_" +
               std::to_string(getpid());
    fs::create_directories(g_tmpDir);
}

static void cleanupTmpDir()
{
    if (!g_tmpDir.empty())
        fs::remove_all(g_tmpDir);
}

// =============================================================================
//  JSON Tests
// =============================================================================

int main()
{
    setupTmpDir();

    std::cout << "=== JSON Parse Tests ===\n";

    // ---- json_parse: primitives ----
    runTest("json_parse: integer", []()
            {
        auto out = runXell("print(json_parse(\"42\"))");
        XASSERT_EQ(out[0], "42"); });

    runTest("json_parse: negative integer", []()
            {
        auto out = runXell("print(json_parse(\"-100\"))");
        XASSERT_EQ(out[0], "-100"); });

    runTest("json_parse: float", []()
            {
        auto out = runXell("print(json_parse(\"3.14\"))");
        XASSERT_EQ(out[0], "3.14"); });

    runTest("json_parse: scientific notation", []()
            {
        auto out = runXell("print(json_parse(\"1e10\"))");
        XASSERT_EQ(out[0], "10000000000"); });

    runTest("json_parse: string", []()
            {
        auto out = runXell("x = json_parse(\"\\\"hello\\\"\")\nprint(x)");
        XASSERT_EQ(out[0], "hello"); });

    runTest("json_parse: true", []()
            {
        auto out = runXell("print(json_parse(\"true\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("json_parse: false", []()
            {
        auto out = runXell("print(json_parse(\"false\"))");
        XASSERT_EQ(out[0], "false"); });

    runTest("json_parse: null", []()
            {
        auto out = runXell("print(json_parse(\"null\"))");
        XASSERT_EQ(out[0], "none"); });

    // ---- json_parse: arrays ----
    runTest("json_parse: empty array", []()
            {
        auto out = runXell("x = json_parse(\"[]\")\nprint(type(x))");
        XASSERT_EQ(out[0], "list"); });

    runTest("json_parse: array of ints", []()
            {
        auto out = runXell("x = json_parse(\"[1, 2, 3]\")\nprint(len(x))");
        XASSERT_EQ(out[0], "3"); });

    runTest("json_parse: array values", []()
            {
        auto out = runXell("x = json_parse(\"[10, 20, 30]\")\nprint(x[0])\nprint(x[2])");
        XASSERT_EQ(out[0], "10");
        XASSERT_EQ(out[1], "30"); });

    // ---- json_parse: objects ----
    runTest("json_parse: empty object", []()
            {
        auto out = runXell("x = json_parse(r\"{}\")\nprint(type(x))");
        XASSERT_EQ(out[0], "map"); });

    runTest("json_parse: object with values", [&]()
            {
        std::string path = g_tmpDir + "/obj_values.json";
        { std::ofstream f(path); f << R"({"name": "xell", "version": 1})"; }
        auto out = runXell(
            "x = json_read(\"" + path + "\")\n"
            "print(x[\"name\"])\n"
            "print(x[\"version\"])\n"
        );
        XASSERT_EQ(out[0], "xell");
        XASSERT_EQ(out[1], "1"); });

    // ---- json_parse: nested ----
    runTest("json_parse: nested object", [&]()
            {
        std::string path = g_tmpDir + "/nested_obj.json";
        { std::ofstream f(path); f << R"({"a": {"b": 42}})"; }
        auto out = runXell(
            "x = json_read(\"" + path + "\")\n"
            "print(x[\"a\"][\"b\"])\n"
        );
        XASSERT_EQ(out[0], "42"); });

    runTest("json_parse: array of objects", [&]()
            {
        std::string path = g_tmpDir + "/arr_obj.json";
        { std::ofstream f(path); f << R"([{"id": 1}, {"id": 2}])"; }
        auto out = runXell(
            "x = json_read(\"" + path + "\")\n"
            "print(x[0][\"id\"])\n"
            "print(x[1][\"id\"])\n"
        );
        XASSERT_EQ(out[0], "1");
        XASSERT_EQ(out[1], "2"); });

    // ---- json_parse: escape sequences ----
    runTest("json_parse: string escapes", [&]()
            {
        std::string path = g_tmpDir + "/escapes.json";
        { std::ofstream f(path); f << "{\"msg\": \"hello\\nworld\"}"; }
        auto out = runXell(
            "x = json_read(\"" + path + "\")\n"
            "print(x[\"msg\"])\n"
        );
        XASSERT(out[0].find("hello") != std::string::npos); });

    runTest("json_parse: invalid JSON throws", []()
            { XASSERT(expectError<std::exception>("json_parse(r\"{invalid}\")")); });

    runTest("json_parse: arity error", []()
            { XASSERT(expectError<std::exception>("json_parse()")); });

    runTest("json_parse: type error", []()
            { XASSERT(expectError<std::exception>("json_parse(42)")); });

    // ---- json_stringify ----
    std::cout << "\n=== JSON Stringify Tests ===\n";

    runTest("json_stringify: integer", []()
            {
        auto out = runXell("print(json_stringify(42))");
        XASSERT_EQ(out[0], "42"); });

    runTest("json_stringify: string", []()
            {
        auto out = runXell(R"(print(json_stringify("hello")))");
        XASSERT_EQ(out[0], "\"hello\""); });

    runTest("json_stringify: bool", []()
            {
        auto out = runXell("print(json_stringify(true))");
        XASSERT_EQ(out[0], "true"); });

    runTest("json_stringify: none → null", []()
            {
        auto out = runXell("x = none\nprint(json_stringify(x))");
        XASSERT_EQ(out[0], "null"); });

    runTest("json_stringify: list", []()
            {
        auto out = runXell("print(json_stringify([1, 2, 3]))");
        XASSERT_EQ(out[0], "[1,2,3]"); });

    runTest("json_stringify: map", []()
            {
        auto out = runXell(R"(
m = {"a": 1}
s = json_stringify(m)
print(s)
)");
        XASSERT(out[0].find("\"a\"") != std::string::npos);
        XASSERT(out[0].find("1") != std::string::npos); });

    runTest("json_stringify: arity error", []()
            { XASSERT(expectError<std::exception>("json_stringify()")); });

    // ---- json_pretty ----
    runTest("json_pretty: produces indented output", []()
            {
                auto out = runXell(R"(
m = {"key": "val"}
s = json_pretty(m)
print(s)
)");
                XASSERT(out[0].find("\n") != std::string::npos); // must be multi-line
                XASSERT(out[0].find("  ") != std::string::npos); // must have indent
            });

    runTest("json_pretty: custom indent", []()
            {
                auto out = runXell(R"(
m = {"k": 1}
s = json_pretty(m, 4)
print(s)
)");
                XASSERT(out[0].find("    ") != std::string::npos); // 4-space indent
            });

    // ---- json_parse roundtrip ----
    runTest("json_stringify → json_parse roundtrip", []()
            {
        auto out = runXell(R"(
original = [1, 2, 3]
s = json_stringify(original)
parsed = json_parse(s)
print(parsed[0])
print(parsed[2])
)");
        XASSERT_EQ(out[0], "1");
        XASSERT_EQ(out[1], "3"); });

    // ---- json_read / json_write ----
    std::cout << "\n=== JSON File I/O Tests ===\n";

    runTest("json_write + json_read roundtrip", [&]()
            {
        std::string path = g_tmpDir + "/test.json";
        auto out = runXell(
            "m = {\"name\": \"xell\", \"version\": 2}\n"
            "json_write(\"" + path + "\", m)\n"
            "r = json_read(\"" + path + "\")\n"
            "print(r[\"name\"])\n"
            "print(r[\"version\"])\n"
        );
        XASSERT_EQ(out[0], "xell");
        XASSERT_EQ(out[1], "2"); });

    runTest("json_read: nonexistent file throws", [&]()
            { XASSERT(expectError<std::exception>(
                  "json_read(\"/nonexistent/path/file.json\")")); });

    runTest("json_write: arity error", []()
            { XASSERT(expectError<std::exception>("json_write()")); });

    runTest("json_read: type error", []()
            { XASSERT(expectError<std::exception>("json_read(42)")); });

    // ---- CSV Tests ----
    std::cout << "\n=== CSV Tests ===\n";

    runTest("csv_parse: basic CSV", []()
            {
        auto out = runXell(R"(
data = csv_parse("name,age\nAlice,30\nBob,25")
print(len(data))
print(data[0]["name"])
print(data[0]["age"])
print(data[1]["name"])
)");
        XASSERT_EQ(out[0], "2");
        XASSERT_EQ(out[1], "Alice");
        XASSERT_EQ(out[2], "30");
        XASSERT_EQ(out[3], "Bob"); });

    runTest("csv_parse: custom separator", []()
            {
        auto out = runXell(R"(
data = csv_parse("name;age\nAlice;30", ";")
print(data[0]["name"])
print(data[0]["age"])
)");
        XASSERT_EQ(out[0], "Alice");
        XASSERT_EQ(out[1], "30"); });

    runTest("csv_parse: empty CSV", []()
            {
        auto out = runXell(R"(
data = csv_parse("")
print(len(data))
)");
        XASSERT_EQ(out[0], "0"); });

    runTest("csv_parse: quoted fields with commas", []()
            {
        auto out = runXell(R"(
data = csv_parse("name,city\n\"Smith, John\",NYC")
print(data[0]["name"])
print(data[0]["city"])
)");
        XASSERT_EQ(out[0], "Smith, John");
        XASSERT_EQ(out[1], "NYC"); });

    runTest("csv_parse: arity error", []()
            { XASSERT(expectError<std::exception>("csv_parse()")); });

    runTest("csv_parse: type error", []()
            { XASSERT(expectError<std::exception>("csv_parse(42)")); });

    // ---- csv_write + csv_read ----
    runTest("csv_write + csv_read roundtrip", [&]()
            {
        std::string path = g_tmpDir + "/test.csv";
        auto out = runXell(
            "data = [{\"name\": \"Alice\", \"age\": \"30\"}, {\"name\": \"Bob\", \"age\": \"25\"}]\n"
            "csv_write(\"" + path + "\", data)\n"
            "r = csv_read(\"" + path + "\")\n"
            "print(len(r))\n"
            "print(r[0][\"name\"])\n"
            "print(r[1][\"age\"])\n"
        );
        XASSERT_EQ(out[0], "2");
        XASSERT_EQ(out[1], "Alice");
        XASSERT_EQ(out[2], "25"); });

    runTest("csv_read: nonexistent file throws", [&]()
            { XASSERT(expectError<std::exception>(
                  "csv_read(\"/nonexistent/file.csv\")")); });

    runTest("csv_write: arity error", []()
            { XASSERT(expectError<std::exception>("csv_write()")); });

    runTest("csv_write: type error on path", []()
            { XASSERT(expectError<std::exception>("csv_write(42, [])")); });

    runTest("csv_write: type error on data", []()
            { XASSERT(expectError<std::exception>("csv_write(\"f.csv\", 42)")); });

    // ---- TOML Tests ----
    std::cout << "\n=== TOML Tests ===\n";

    runTest("toml_read: basic key-value", [&]()
            {
        std::string path = g_tmpDir + "/test.toml";
        {
            std::ofstream f(path);
            f << "title = \"TOML Example\"\nversion = 2\nenabled = true\n";
        }
        auto out = runXell(
            "t = toml_read(\"" + path + "\")\n"
            "print(t[\"title\"])\n"
            "print(t[\"version\"])\n"
            "print(t[\"enabled\"])\n"
        );
        XASSERT_EQ(out[0], "TOML Example");
        XASSERT_EQ(out[1], "2");
        XASSERT_EQ(out[2], "true"); });

    runTest("toml_read: sections", [&]()
            {
        std::string path = g_tmpDir + "/sections.toml";
        {
            std::ofstream f(path);
            f << "name = \"app\"\n\n[server]\nhost = \"localhost\"\nport = 8080\n";
        }
        auto out = runXell(
            "t = toml_read(\"" + path + "\")\n"
            "print(t[\"name\"])\n"
            "print(t[\"server\"][\"host\"])\n"
            "print(t[\"server\"][\"port\"])\n"
        );
        XASSERT_EQ(out[0], "app");
        XASSERT_EQ(out[1], "localhost");
        XASSERT_EQ(out[2], "8080"); });

    runTest("toml_read: inline array", [&]()
            {
        std::string path = g_tmpDir + "/arr.toml";
        {
            std::ofstream f(path);
            f << "colors = [\"red\", \"green\", \"blue\"]\n";
        }
        auto out = runXell(
            "t = toml_read(\"" + path + "\")\n"
            "print(len(t[\"colors\"]))\n"
            "print(t[\"colors\"][0])\n"
        );
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "red"); });

    runTest("toml_read: comments ignored", [&]()
            {
        std::string path = g_tmpDir + "/comments.toml";
        {
            std::ofstream f(path);
            f << "# This is a comment\nkey = \"value\" # inline comment\n";
        }
        auto out = runXell(
            "t = toml_read(\"" + path + "\")\n"
            "print(t[\"key\"])\n"
        );
        XASSERT_EQ(out[0], "value"); });

    runTest("toml_read: nonexistent file throws", []()
            { XASSERT(expectError<std::exception>(
                  "toml_read(\"/nonexistent/file.toml\")")); });

    runTest("toml_read: arity error", []()
            { XASSERT(expectError<std::exception>("toml_read()")); });

    runTest("toml_read: type error", []()
            { XASSERT(expectError<std::exception>("toml_read(42)")); });

    // ---- YAML Tests ----
    std::cout << "\n=== YAML Tests ===\n";

    runTest("yaml_read: basic key-value", [&]()
            {
        std::string path = g_tmpDir + "/test.yaml";
        {
            std::ofstream f(path);
            f << "name: myapp\nversion: 3\nenabled: true\n";
        }
        auto out = runXell(
            "y = yaml_read(\"" + path + "\")\n"
            "print(y[\"name\"])\n"
            "print(y[\"version\"])\n"
            "print(y[\"enabled\"])\n"
        );
        XASSERT_EQ(out[0], "myapp");
        XASSERT_EQ(out[1], "3");
        XASSERT_EQ(out[2], "true"); });

    runTest("yaml_read: nested maps", [&]()
            {
        std::string path = g_tmpDir + "/nested.yaml";
        {
            std::ofstream f(path);
            f << "server:\n  host: localhost\n  port: 9090\n";
        }
        auto out = runXell(
            "y = yaml_read(\"" + path + "\")\n"
            "print(y[\"server\"][\"host\"])\n"
            "print(y[\"server\"][\"port\"])\n"
        );
        XASSERT_EQ(out[0], "localhost");
        XASSERT_EQ(out[1], "9090"); });

    runTest("yaml_read: list", [&]()
            {
        std::string path = g_tmpDir + "/list.yaml";
        {
            std::ofstream f(path);
            f << "items:\n  - apple\n  - banana\n  - cherry\n";
        }
        auto out = runXell(
            "y = yaml_read(\"" + path + "\")\n"
            "print(len(y[\"items\"]))\n"
            "print(y[\"items\"][0])\n"
            "print(y[\"items\"][2])\n"
        );
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "apple");
        XASSERT_EQ(out[2], "cherry"); });

    runTest("yaml_read: document separator ---", [&]()
            {
        std::string path = g_tmpDir + "/doc.yaml";
        {
            std::ofstream f(path);
            f << "---\nkey: value\n";
        }
        auto out = runXell(
            "y = yaml_read(\"" + path + "\")\n"
            "print(y[\"key\"])\n"
        );
        XASSERT_EQ(out[0], "value"); });

    runTest("yaml_read: booleans", [&]()
            {
        std::string path = g_tmpDir + "/bools.yaml";
        {
            std::ofstream f(path);
            f << "a: true\nb: false\nc: yes\nd: no\n";
        }
        auto out = runXell(
            "y = yaml_read(\"" + path + "\")\n"
            "print(y[\"a\"])\n"
            "print(y[\"b\"])\n"
            "print(y[\"c\"])\n"
            "print(y[\"d\"])\n"
        );
        XASSERT_EQ(out[0], "true");
        XASSERT_EQ(out[1], "false");
        XASSERT_EQ(out[2], "true");
        XASSERT_EQ(out[3], "false"); });

    runTest("yaml_read: null values", [&]()
            {
        std::string path = g_tmpDir + "/nulls.yaml";
        {
            std::ofstream f(path);
            f << "a: null\nb: ~\n";
        }
        auto out = runXell(
            "y = yaml_read(\"" + path + "\")\n"
            "print(y[\"a\"])\n"
            "print(y[\"b\"])\n"
        );
        XASSERT_EQ(out[0], "none");
        XASSERT_EQ(out[1], "none"); });

    runTest("yaml_read: nonexistent file throws", []()
            { XASSERT(expectError<std::exception>(
                  "yaml_read(\"/nonexistent/file.yaml\")")); });

    runTest("yaml_read: arity error", []()
            { XASSERT(expectError<std::exception>("yaml_read()")); });

    runTest("yaml_read: type error", []()
            { XASSERT(expectError<std::exception>("yaml_read(42)")); });

    // =================================================================
    //  Shell Utility Tests
    // =================================================================

    std::cout << "\n=== Shell Utility Tests ===\n";

    // ---- error() ----
    runTest("error: returns 0 (success code)", []()
            {
        auto out = runXell("x = error(\"test\")\nprint(x)");
        XASSERT_EQ(out[0], "0"); });

    // ---- alias / unalias ----
    runTest("alias: define and retrieve", []()
            {
        auto out = runXell(R"(
alias("ll", "ls -la")
m = alias()
print(m["ll"])
)");
        XASSERT_EQ(out[0], "ls -la"); });

    runTest("unalias: remove alias", []()
            {
        auto out = runXell(R"(
alias("ll", "ls -la")
unalias("ll")
m = alias()
print(type(m))
)");
        XASSERT_EQ(out[0], "map"); });

    runTest("unalias: nonexistent throws", []()
            { XASSERT(expectError<std::exception>("unalias(\"doesntexist\")")); });

    runTest("alias: arity error", []()
            { XASSERT(expectError<std::exception>("alias(\"a\")")); });

    runTest("alias: type error name", []()
            { XASSERT(expectError<std::exception>("alias(42, \"cmd\")")); });

    // ---- export_env / set_env / printenv / env_list ----
    runTest("export_env + printenv roundtrip", []()
            {
        auto out = runXell(R"(
export_env("XELL_TEST_VAR_123", "hello_xell")
v = printenv("XELL_TEST_VAR_123")
print(v)
)");
        XASSERT_EQ(out[0], "hello_xell"); });

    runTest("set_env: alias for export_env", []()
            {
        auto out = runXell(R"(
set_env("XELL_TEST_SE_456", "world")
v = printenv("XELL_TEST_SE_456")
print(v)
)");
        XASSERT_EQ(out[0], "world"); });

    runTest("printenv: nonexistent returns none", []()
            {
        auto out = runXell(R"(
v = printenv("XELL_NONEXISTENT_VAR_999")
print(v)
)");
        XASSERT_EQ(out[0], "none"); });

    runTest("env_list: returns map", []()
            {
        auto out = runXell("e = env_list()\nprint(type(e))");
        XASSERT_EQ(out[0], "map"); });

    runTest("env_list: contains PATH", []()
            {
        auto out = runXell(R"(
e = env_list()
print(e["PATH"] != none)
)");
        XASSERT_EQ(out[0], "true"); });

    runTest("printenv: no args returns map", []()
            {
        auto out = runXell("e = printenv()\nprint(type(e))");
        XASSERT_EQ(out[0], "map"); });

    runTest("export_env: arity error", []()
            { XASSERT(expectError<std::exception>("export_env(\"a\")")); });

    runTest("export_env: type error", []()
            { XASSERT(expectError<std::exception>("export_env(42, \"b\")")); });

    // ---- which / whereis / type_cmd ----
    runTest("which: finds ls", []()
            {
        auto out = runXell(R"(
w = which("ls")
print(w != none)
)");
        XASSERT_EQ(out[0], "true"); });

    runTest("which: nonexistent returns none", []()
            {
        auto out = runXell(R"(
w = which("xell_nonexistent_cmd_999")
print(w)
)");
        XASSERT_EQ(out[0], "none"); });

    runTest("which: arity error", []()
            { XASSERT(expectError<std::exception>("which()")); });

    runTest("which: type error", []()
            { XASSERT(expectError<std::exception>("which(42)")); });

    runTest("whereis: returns string", []()
            {
        auto out = runXell(R"(
w = whereis("ls")
print(type(w))
)");
        XASSERT_EQ(out[0], "string"); });

    runTest("type_cmd: returns string", []()
            {
        auto out = runXell(R"(
t = type_cmd("ls")
print(type(t))
)");
        XASSERT_EQ(out[0], "string"); });

    runTest("type_cmd: arity error", []()
            { XASSERT(expectError<std::exception>("type_cmd()")); });

    // ---- man ----
    runTest("man: returns string", []()
            {
        auto out = runXell(R"(
m = man("nonexistent_cmd_xyz")
print(type(m))
)");
        XASSERT_EQ(out[0], "string"); });

    runTest("man: arity error", []()
            { XASSERT(expectError<std::exception>("man()")); });

    // ---- history / history_add ----
    runTest("history: initially empty", []()
            {
        auto out = runXell(R"(
h = history()
print(len(h))
)");
        XASSERT_EQ(out[0], "0"); });

    runTest("history_add + history: records entries", []()
            {
        auto out = runXell(R"(
history_add("ls -la")
history_add("cd /tmp")
h = history()
print(len(h))
print(h[0]["command"])
print(h[1]["command"])
print(h[0]["index"])
)");
        XASSERT_EQ(out[0], "2");
        XASSERT_EQ(out[1], "ls -la");
        XASSERT_EQ(out[2], "cd /tmp");
        XASSERT_EQ(out[3], "1"); });

    // ---- yes_cmd ----
    runTest("yes_cmd: default", []()
            {
        auto out = runXell(R"(
y = yes_cmd("y", 3)
print(y)
)");
        XASSERT_EQ(out[0], "y\ny\ny"); });

    runTest("yes_cmd: custom text", []()
            {
        auto out = runXell(R"(
y = yes_cmd("ok", 2)
print(y)
)");
        XASSERT_EQ(out[0], "ok\nok"); });

    runTest("yes_cmd: zero count", []()
            {
        auto out = runXell(R"(
y = yes_cmd("x", 0)
print(y)
)");
        XASSERT_EQ(out[0], ""); });

    // ---- true_val / false_val ----
    runTest("true_val: returns true", []()
            {
        auto out = runXell("print(true_val())");
        XASSERT_EQ(out[0], "true"); });

    runTest("false_val: returns false", []()
            {
        auto out = runXell("print(false_val())");
        XASSERT_EQ(out[0], "false"); });

    runTest("true_val: arity error", []()
            { XASSERT(expectError<std::exception>("true_val(1)")); });

    runTest("false_val: arity error", []()
            { XASSERT(expectError<std::exception>("false_val(1)")); });

    // ---- source_file ----
    runTest("source_file: reads file content", [&]()
            {
        std::string path = g_tmpDir + "/src_test.txt";
        {
            std::ofstream f(path);
            f << "hello from file";
        }
        auto out = runXell(
            "s = source_file(\"" + path + "\")\n"
            "print(s)\n"
        );
        XASSERT_EQ(out[0], "hello from file"); });

    runTest("source_file: nonexistent throws", []()
            { XASSERT(expectError<std::exception>(
                  "source_file(\"/nonexistent/path\")")); });

    runTest("source_file: arity error", []()
            { XASSERT(expectError<std::exception>("source_file()")); });

    runTest("source_file: type error", []()
            { XASSERT(expectError<std::exception>("source_file(42)")); });

    // ---- clear / reset: just check they don't crash ----
    runTest("clear: no error on call", []()
            {
        auto out = runXell("clear()");
        XASSERT(out.empty()); });

    runTest("reset: no error on call", []()
            {
        auto out = runXell("reset()");
        XASSERT(out.empty()); });

    runTest("clear: arity error", []()
            { XASSERT(expectError<std::exception>("clear(1)")); });

    runTest("reset: arity error", []()
            { XASSERT(expectError<std::exception>("reset(1)")); });

    // ---- Complex integration tests ----
    std::cout << "\n=== Integration Tests ===\n";

    runTest("json_parse → csv_write → csv_read roundtrip", [&]()
            {
        std::string jsonPath = g_tmpDir + "/integration.json";
        std::string csvPath  = g_tmpDir + "/integration.csv";
        { std::ofstream f(jsonPath); f << R"([{"name": "Alice", "score": "95"}, {"name": "Bob", "score": "87"}])"; }
        auto out = runXell(
            "data = json_read(\"" + jsonPath + "\")\n"
            "csv_write(\"" + csvPath + "\", data)\n"
            "r = csv_read(\"" + csvPath + "\")\n"
            "print(len(r))\n"
            "print(r[0][\"name\"])\n"
            "print(r[1][\"score\"])\n"
        );
        XASSERT_EQ(out[0], "2");
        XASSERT_EQ(out[1], "Alice");
        XASSERT_EQ(out[2], "87"); });

    runTest("json_parse: complex nested structure", [&]()
            {
        std::string path = g_tmpDir + "/complex.json";
        { std::ofstream f(path); f << R"({"users": [{"name": "Alice", "tags": ["admin", "user"]}, {"name": "Bob", "tags": ["user"]}], "count": 2})"; }
        auto out = runXell(
            "data = json_read(\"" + path + "\")\n"
            "print(data[\"count\"])\n"
            "print(data[\"users\"][0][\"name\"])\n"
            "print(data[\"users\"][0][\"tags\"][1])\n"
            "print(len(data[\"users\"][1][\"tags\"]))\n"
        );
        XASSERT_EQ(out[0], "2");
        XASSERT_EQ(out[1], "Alice");
        XASSERT_EQ(out[2], "user");
        XASSERT_EQ(out[3], "1"); });

    runTest("json_stringify: nested map+list", []()
            {
        auto out = runXell(R"(
m = {"items": [1, 2, 3], "nested": {"a": true}}
s = json_stringify(m)
r = json_parse(s)
print(r["items"][1])
print(r["nested"]["a"])
)");
        XASSERT_EQ(out[0], "2");
        XASSERT_EQ(out[1], "true"); });

    // ---- Cleanup ----
    cleanupTmpDir();

    // ---- Summary ----
    std::cout << "\n============================================\n";
    std::cout << "  Total: " << (g_passed + g_failed)
              << "  Passed: " << g_passed
              << "  Failed: " << g_failed << "\n";
    std::cout << "============================================\n";
    return g_failed > 0 ? 1 : 0;
}
