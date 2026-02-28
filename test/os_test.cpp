// =============================================================================
// os_test.cpp — Standalone tests for the Xell OS abstraction layer
// =============================================================================
//
// Tests filesystem, environment, and process operations.
// Uses a temp directory for all FS tests to avoid polluting the workspace.
//
// =============================================================================

#include "../src/os/os.hpp"
#include "../src/lib/errors/error.hpp"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <functional>
#include <cstdlib>
#include <filesystem>

namespace fs = std::filesystem;

// ---- Test harness (same pattern as other Xell tests) -----------------------

static int g_total = 0, g_passed = 0, g_failed = 0;

#define XASSERT(cond)                                                       \
    do                                                                      \
    {                                                                       \
        if (!(cond))                                                        \
        {                                                                   \
            std::cerr << "  ASSERT FAILED: " #cond << " (line " << __LINE__ \
                      << ")" << std::endl;                                  \
            throw std::runtime_error("assertion failed");                   \
        }                                                                   \
    } while (0)

#define XASSERT_EQ(a, b)                                               \
    do                                                                 \
    {                                                                  \
        auto _a = (a);                                                 \
        auto _b = (b);                                                 \
        if (_a != _b)                                                  \
        {                                                              \
            std::cerr << "  ASSERT FAILED: " << #a << " == " << #b     \
                      << "\n    Got: [" << _a << "] Expected: [" << _b \
                      << "] (line " << __LINE__ << ")" << std::endl;   \
            throw std::runtime_error("assertion failed");              \
        }                                                              \
    } while (0)

#define XASSERT_THROWS(expr, ExceptionType)                             \
    do                                                                  \
    {                                                                   \
        bool caught = false;                                            \
        try                                                             \
        {                                                               \
            expr;                                                       \
        }                                                               \
        catch (const ExceptionType &)                                   \
        {                                                               \
            caught = true;                                              \
        }                                                               \
        if (!caught)                                                    \
        {                                                               \
            std::cerr << "  ASSERT FAILED: expected " #ExceptionType    \
                      << " from " #expr << " (line " << __LINE__ << ")" \
                      << std::endl;                                     \
            throw std::runtime_error("assertion failed");               \
        }                                                               \
    } while (0)

static void runTest(const std::string &name, std::function<void()> fn)
{
    g_total++;
    try
    {
        fn();
        g_passed++;
        std::cout << "  PASS: " << name << std::endl;
    }
    catch (const std::exception &e)
    {
        g_failed++;
        std::cout << "  FAIL: " << name << " — " << e.what() << std::endl;
    }
}

// ---- Temp directory helper --------------------------------------------------

static std::string g_tmpDir;

static void createTmpDir()
{
    g_tmpDir = (fs::temp_directory_path() / "xell_os_test").string();
    fs::remove_all(g_tmpDir);
    fs::create_directories(g_tmpDir);
}

static void cleanTmpDir()
{
    std::error_code ec;
    fs::remove_all(g_tmpDir, ec);
}

static std::string tmp(const std::string &rel)
{
    return g_tmpDir + "/" + rel;
}

using namespace xell;

// =============================================================================
// Section 1: Filesystem — Directory Operations
// =============================================================================

static void testFilesystemDirs()
{
    std::cout << "\n--- Section 1: Filesystem — Directories ---\n";

    runTest("make_dir creates single dir", []()
            {
        os::make_dir(tmp("dir_a"));
        XASSERT(os::path_exists(tmp("dir_a")));
        XASSERT(os::is_dir(tmp("dir_a"))); });

    runTest("make_dir creates nested dirs", []()
            {
        os::make_dir(tmp("deep/nested/dir"));
        XASSERT(os::is_dir(tmp("deep/nested/dir"))); });

    runTest("make_dir is no-op if already exists", []()
            {
        os::make_dir(tmp("dir_a")); // already exists — should not throw
        XASSERT(os::is_dir(tmp("dir_a"))); });

    runTest("remove_path removes empty dir", []()
            {
        os::make_dir(tmp("to_remove"));
        XASSERT(os::path_exists(tmp("to_remove")));
        os::remove_path(tmp("to_remove"));
        XASSERT(!os::path_exists(tmp("to_remove"))); });

    runTest("remove_path removes dir recursively", []()
            {
        os::make_dir(tmp("tree/sub/deep"));
        os::write_file(tmp("tree/sub/deep/f.txt"), "data");
        os::remove_path(tmp("tree"));
        XASSERT(!os::path_exists(tmp("tree"))); });

    runTest("remove_path throws on non-existent", []()
            { XASSERT_THROWS(os::remove_path(tmp("no_such_path")), FileNotFoundError); });

    runTest("list_dir lists children names", []()
            {
        os::make_dir(tmp("parent"));
        os::write_file(tmp("parent/a.txt"), "a");
        os::write_file(tmp("parent/b.txt"), "b");
        os::make_dir(tmp("parent/subdir"));

        auto entries = os::list_dir(tmp("parent"));
        XASSERT_EQ(entries.size(), (size_t)3);
        // Check they are names, not full paths
        std::sort(entries.begin(), entries.end());
        XASSERT_EQ(entries[0], std::string("a.txt"));
        XASSERT_EQ(entries[1], std::string("b.txt"));
        XASSERT_EQ(entries[2], std::string("subdir")); });

    runTest("list_dir_full lists full paths", []()
            {
        auto entries = os::list_dir_full(tmp("parent"));
        XASSERT_EQ(entries.size(), (size_t)3);
        // Each entry should start with the parent path
        for (const auto &e : entries)
            XASSERT(e.find(g_tmpDir) == 0); });

    runTest("list_dir throws on non-existent", []()
            { XASSERT_THROWS(os::list_dir(tmp("nope")), FileNotFoundError); });

    runTest("list_dir throws on file", []()
            {
        os::write_file(tmp("a_file.txt"), "x");
        XASSERT_THROWS(os::list_dir(tmp("a_file.txt")), IOError); });
}

// =============================================================================
// Section 2: Filesystem — File Operations
// =============================================================================

static void testFilesystemFiles()
{
    std::cout << "\n--- Section 2: Filesystem — Files ---\n";

    runTest("write_file + read_file roundtrip", []()
            {
        os::write_file(tmp("hello.txt"), "Hello, Xell!");
        XASSERT_EQ(os::read_file(tmp("hello.txt")), std::string("Hello, Xell!")); });

    runTest("write_file creates parent dirs", []()
            {
        os::write_file(tmp("auto/parent/file.txt"), "auto");
        XASSERT_EQ(os::read_file(tmp("auto/parent/file.txt")), std::string("auto")); });

    runTest("write_file overwrites existing", []()
            {
        os::write_file(tmp("overwrite.txt"), "old");
        os::write_file(tmp("overwrite.txt"), "new");
        XASSERT_EQ(os::read_file(tmp("overwrite.txt")), std::string("new")); });

    runTest("append_file appends to existing", []()
            {
        os::write_file(tmp("append.txt"), "Hello");
        os::append_file(tmp("append.txt"), " World");
        XASSERT_EQ(os::read_file(tmp("append.txt")), std::string("Hello World")); });

    runTest("append_file creates new file", []()
            {
        os::append_file(tmp("new_append.txt"), "fresh");
        XASSERT_EQ(os::read_file(tmp("new_append.txt")), std::string("fresh")); });

    runTest("read_file throws on non-existent", []()
            { XASSERT_THROWS(os::read_file(tmp("no_file.txt")), FileNotFoundError); });

    runTest("read_file throws on directory", []()
            {
        os::make_dir(tmp("a_dir_for_read"));
        XASSERT_THROWS(os::read_file(tmp("a_dir_for_read")), IOError); });

    runTest("file_size returns correct size", []()
            {
        os::write_file(tmp("sized.txt"), "12345");
        XASSERT_EQ(os::file_size(tmp("sized.txt")), (std::uint64_t)5); });

    runTest("file_size throws on non-existent", []()
            { XASSERT_THROWS(os::file_size(tmp("nope.txt")), FileNotFoundError); });

    runTest("is_file returns true for file", []()
            {
        os::write_file(tmp("check_file.txt"), "x");
        XASSERT(os::is_file(tmp("check_file.txt")));
        XASSERT(!os::is_dir(tmp("check_file.txt"))); });

    runTest("is_dir returns true for directory", []()
            {
        os::make_dir(tmp("check_dir"));
        XASSERT(os::is_dir(tmp("check_dir")));
        XASSERT(!os::is_file(tmp("check_dir"))); });

    runTest("path_exists returns false for non-existent", []()
            { XASSERT(!os::path_exists(tmp("phantom.txt"))); });
}

// =============================================================================
// Section 3: Filesystem — Copy, Move, Paths
// =============================================================================

static void testFilesystemCopyMove()
{
    std::cout << "\n--- Section 3: Filesystem — Copy/Move/Paths ---\n";

    runTest("copy_path copies a file", []()
            {
        os::write_file(tmp("orig.txt"), "original");
        os::copy_path(tmp("orig.txt"), tmp("copy.txt"));
        XASSERT_EQ(os::read_file(tmp("copy.txt")), std::string("original"));
        // Original still exists
        XASSERT(os::path_exists(tmp("orig.txt"))); });

    runTest("copy_path copies a directory recursively", []()
            {
        os::make_dir(tmp("src_dir/sub"));
        os::write_file(tmp("src_dir/a.txt"), "aaa");
        os::write_file(tmp("src_dir/sub/b.txt"), "bbb");
        os::copy_path(tmp("src_dir"), tmp("dst_dir"));
        XASSERT_EQ(os::read_file(tmp("dst_dir/a.txt")), std::string("aaa"));
        XASSERT_EQ(os::read_file(tmp("dst_dir/sub/b.txt")), std::string("bbb")); });

    runTest("copy_path throws on non-existent source", []()
            { XASSERT_THROWS(os::copy_path(tmp("nope"), tmp("dst")), FileNotFoundError); });

    runTest("move_path moves a file", []()
            {
        os::write_file(tmp("to_move.txt"), "moving");
        os::move_path(tmp("to_move.txt"), tmp("moved.txt"));
        XASSERT(!os::path_exists(tmp("to_move.txt")));
        XASSERT_EQ(os::read_file(tmp("moved.txt")), std::string("moving")); });

    runTest("move_path throws on non-existent source", []()
            { XASSERT_THROWS(os::move_path(tmp("nope"), tmp("dst")), FileNotFoundError); });

    runTest("absolute_path returns absolute", []()
            {
        auto abs = os::absolute_path(".");
        XASSERT(abs.size() > 0);
        XASSERT(abs[0] == '/'); /* Unix absolute starts with / */ });

    runTest("file_name extracts filename", []()
            {
        XASSERT_EQ(os::file_name("/foo/bar/baz.txt"), std::string("baz.txt"));
        XASSERT_EQ(os::file_name("hello.xel"), std::string("hello.xel")); });

    runTest("parent_path extracts parent", []()
            { XASSERT_EQ(os::parent_path("/foo/bar/baz.txt"), std::string("/foo/bar")); });

    runTest("extension extracts extension", []()
            {
        XASSERT_EQ(os::extension("script.xel"), std::string(".xel"));
        XASSERT_EQ(os::extension("noext"), std::string("")); });

    runTest("cwd returns non-empty", []()
            {
        auto c = os::cwd();
        XASSERT(c.size() > 0); });

    runTest("change_dir + cwd round-trip", []()
            {
        auto original = os::cwd();
        os::make_dir(tmp("cd_test"));
        os::change_dir(tmp("cd_test"));
        auto after = os::cwd();
        // Restore original directory
        os::change_dir(original);
        // The cwd after change should contain "cd_test"
        XASSERT(after.find("cd_test") != std::string::npos); });

    runTest("change_dir throws on non-existent", []()
            { XASSERT_THROWS(os::change_dir(tmp("no_such_dir")), FileNotFoundError); });

    runTest("change_dir throws on file", []()
            {
        os::write_file(tmp("not_a_dir.txt"), "x");
        XASSERT_THROWS(os::change_dir(tmp("not_a_dir.txt")), IOError); });
}

// =============================================================================
// Section 4: Environment Variables
// =============================================================================

static void testEnvironment()
{
    std::cout << "\n--- Section 4: Environment Variables ---\n";

    runTest("env_get returns empty for unset var", []()
            { XASSERT_EQ(os::env_get("XELL_TEST_NONEXISTENT_VAR_12345"), std::string("")); });

    runTest("env_has returns false for unset var", []()
            { XASSERT(!os::env_has("XELL_TEST_NONEXISTENT_VAR_12345")); });

    runTest("env_set + env_get round-trip", []()
            {
        os::env_set("XELL_TEST_VAR", "hello_xell");
        XASSERT_EQ(os::env_get("XELL_TEST_VAR"), std::string("hello_xell")); });

    runTest("env_has returns true after set", []()
            { XASSERT(os::env_has("XELL_TEST_VAR")); });

    runTest("env_set overwrites existing", []()
            {
        os::env_set("XELL_TEST_VAR", "first");
        os::env_set("XELL_TEST_VAR", "second");
        XASSERT_EQ(os::env_get("XELL_TEST_VAR"), std::string("second")); });

    runTest("env_unset removes variable", []()
            {
        os::env_set("XELL_TEST_VAR2", "temp");
        XASSERT(os::env_has("XELL_TEST_VAR2"));
        os::env_unset("XELL_TEST_VAR2");
        XASSERT(!os::env_has("XELL_TEST_VAR2")); });

    runTest("env_get reads system PATH", []()
            {
        auto path = os::env_get("PATH");
        XASSERT(path.size() > 0); });

    // Cleanup test vars
    os::env_unset("XELL_TEST_VAR");
}

// =============================================================================
// Section 5: Process Execution
// =============================================================================

static void testProcess()
{
    std::cout << "\n--- Section 5: Process Execution ---\n";

    runTest("run returns 0 for successful command", []()
            {
        int code = os::run("true");
        XASSERT_EQ(code, 0); });

    runTest("run returns non-zero for failing command", []()
            {
        int code = os::run("false");
        XASSERT(code != 0); });

    runTest("run executes complex command", []()
            {
        int code = os::run("echo hello > /dev/null");
        XASSERT_EQ(code, 0); });

    runTest("run_capture captures stdout", []()
            {
        auto result = os::run_capture("echo hello");
        XASSERT_EQ(result.exitCode, 0);
        // stdout should contain "hello\n"
        XASSERT(result.stdoutOutput.find("hello") != std::string::npos); });

    runTest("run_capture captures multi-line stdout", []()
            {
        auto result = os::run_capture("echo line1 && echo line2");
        XASSERT_EQ(result.exitCode, 0);
        XASSERT(result.stdoutOutput.find("line1") != std::string::npos);
        XASSERT(result.stdoutOutput.find("line2") != std::string::npos); });

    runTest("run_capture captures stderr", []()
            {
        auto result = os::run_capture("echo err_msg >&2");
        XASSERT(result.stderrOutput.find("err_msg") != std::string::npos); });

    runTest("run_capture returns exit code from failed command", []()
            {
        auto result = os::run_capture("exit 42");
        XASSERT_EQ(result.exitCode, 42); });

    runTest("run_capture with command that produces no output", []()
            {
        auto result = os::run_capture("true");
        XASSERT_EQ(result.exitCode, 0);
        XASSERT_EQ(result.stdoutOutput, std::string("")); });

    runTest("run_capture works with pipes", []()
            {
        auto result = os::run_capture("echo 'aaa bbb ccc' | wc -w");
        XASSERT_EQ(result.exitCode, 0);
        // Output should contain "3"
        XASSERT(result.stdoutOutput.find("3") != std::string::npos); });

    runTest("run with file creation side effect", []()
            {
        std::string f = tmp("run_created.txt");
        os::run("echo hello > " + f);
        XASSERT(os::path_exists(f));
        auto content = os::read_file(f);
        XASSERT(content.find("hello") != std::string::npos); });

    runTest("run_capture with env var", []()
            {
        os::env_set("XELL_PROC_TEST", "xell_rocks");
        auto result = os::run_capture("echo $XELL_PROC_TEST");
        XASSERT(result.stdoutOutput.find("xell_rocks") != std::string::npos);
        os::env_unset("XELL_PROC_TEST"); });

    runTest("get_pid returns positive number", []()
            {
        int pid = os::get_pid();
        XASSERT(pid > 0); });
}

// =============================================================================
// Section 6: Edge Cases & Robustness
// =============================================================================

static void testEdgeCases()
{
    std::cout << "\n--- Section 6: Edge Cases ---\n";

    runTest("write + read empty file", []()
            {
        os::write_file(tmp("empty.txt"), "");
        XASSERT_EQ(os::read_file(tmp("empty.txt")), std::string(""));
        XASSERT_EQ(os::file_size(tmp("empty.txt")), (std::uint64_t)0); });

    runTest("write + read file with special chars", []()
            {
        std::string special = "line1\nline2\ttab\r\nwindows";
        os::write_file(tmp("special.txt"), special);
        XASSERT_EQ(os::read_file(tmp("special.txt")), special); });

    runTest("write + read binary-ish content", []()
            {
        std::string bin;
        for (int i = 0; i < 256; i++)
            bin += static_cast<char>(i);
        os::write_file(tmp("binary.dat"), bin);
        XASSERT_EQ(os::read_file(tmp("binary.dat")), bin); });

    runTest("list_dir on empty directory", []()
            {
        os::make_dir(tmp("empty_dir"));
        auto entries = os::list_dir(tmp("empty_dir"));
        XASSERT_EQ(entries.size(), (size_t)0); });

    runTest("file_name of root-like path", []()
            {
        // On Linux, std::filesystem::path("/").filename() returns ""
        XASSERT_EQ(os::file_name("/"), std::string("")); });

    runTest("extension of dotfile", []()
            { XASSERT_EQ(os::extension(".gitignore"), std::string("")); });

    runTest("make_dir with trailing slash", []()
            {
        os::make_dir(tmp("trailing_slash/"));
        XASSERT(os::is_dir(tmp("trailing_slash"))); });
}

// =============================================================================
// Main
// =============================================================================

int main()
{
    std::cout << "============================================" << std::endl;
    std::cout << "  Xell OS Abstraction Layer Tests" << std::endl;
    std::cout << "============================================" << std::endl;

    createTmpDir();

    testFilesystemDirs();
    testFilesystemFiles();
    testFilesystemCopyMove();
    testEnvironment();
    testProcess();
    testEdgeCases();

    cleanTmpDir();

    std::cout << "\n============================================" << std::endl;
    std::cout << "  Total: " << g_total << "  |  Passed: " << g_passed
              << "  |  Failed: " << g_failed << std::endl;
    std::cout << "============================================" << std::endl;

    return g_failed > 0 ? 1 : 0;
}
