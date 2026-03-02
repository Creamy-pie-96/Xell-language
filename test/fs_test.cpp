// =============================================================================
// File System Tests — Comprehensive coverage for FS builtins
// =============================================================================
// Tests both existing OS builtins and new FS builtins.
// Uses a temp directory for all file operations.
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

// Test sandbox directory
static std::string g_testDir;

static void setupTestDir()
{
    g_testDir = fs::temp_directory_path().string() + "/xell_fs_test_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(g_testDir);
}

static void cleanupTestDir()
{
    if (!g_testDir.empty() && fs::exists(g_testDir))
        fs::remove_all(g_testDir);
}

// ============================================================================
// Section 1: touch, cat, read_lines, write_lines
// ============================================================================

static void testFileOps()
{
    std::cout << "\n[File Ops — touch, cat, read_lines, write_lines]\n";

    runTest("touch creates file", [&]()
            {
        auto out = runXell(
            "touch(\"" + g_testDir + "/touched.txt\")\n"
            "print(exists(\"" + g_testDir + "/touched.txt\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("touch existing file no error", [&]()
            {
        // Write something first
        runXell("write(\"" + g_testDir + "/touched.txt\", \"hello\")");
        // touch should not fail
        runXell("touch(\"" + g_testDir + "/touched.txt\")");
        // File should still have content
        auto out = runXell("print(read(\"" + g_testDir + "/touched.txt\"))");
        XASSERT_EQ(out[0], "hello"); });

    runTest("cat reads file content", [&]()
            {
        runXell("write(\"" + g_testDir + "/cattest.txt\", \"cat content\")");
        auto out = runXell("print(cat(\"" + g_testDir + "/cattest.txt\"))");
        XASSERT_EQ(out[0], "cat content"); });

    runTest("cat nonexistent throws", [&]()
            { XASSERT(expectError<std::exception>("cat(\"" + g_testDir + "/nonexistent.txt\")")); });

    runTest("read_lines reads lines", [&]()
            {
        // Write multi-line file directly
        {
            std::ofstream ofs(g_testDir + "/lines.txt");
            ofs << "line1\nline2\nline3";
        }
        auto out = runXell("print(read_lines(\"" + g_testDir + "/lines.txt\"))");
        XASSERT_EQ(out[0], "[\"line1\", \"line2\", \"line3\"]"); });

    runTest("write_lines writes list", [&]()
            {
        runXell("write_lines(\"" + g_testDir + "/wlines.txt\", [\"a\", \"b\", \"c\"])");
        auto out = runXell("print(cat(\"" + g_testDir + "/wlines.txt\"))");
        XASSERT_EQ(out[0], "a\nb\nc"); });

    runTest("write_lines + read_lines round-trip", [&]()
            {
        runXell("write_lines(\"" + g_testDir + "/rt.txt\", [\"x\", \"y\"])");
        auto out = runXell("print(read_lines(\"" + g_testDir + "/rt.txt\"))");
        XASSERT_EQ(out[0], "[\"x\", \"y\"]"); });
}

// ============================================================================
// Section 2: ls_all, pwd
// ============================================================================

static void testListAndPwd()
{
    std::cout << "\n[ls_all, pwd]\n";

    runTest("ls_all lists files", [&]()
            {
        // Create a few files in a subdir
        fs::create_directories(g_testDir + "/lsdir");
        std::ofstream(g_testDir + "/lsdir/a.txt");
        std::ofstream(g_testDir + "/lsdir/.hidden");
        auto out = runXell("print(size(ls_all(\"" + g_testDir + "/lsdir\")))");
        // Should contain both a.txt and .hidden
        XASSERT_EQ(out[0], "2"); });

    runTest("pwd returns path", []()
            {
        auto out = runXell("print(pwd())");
        XASSERT(out[0].size() > 0); });
}

// ============================================================================
// Section 3: Symlinks
// ============================================================================

static void testSymlinks()
{
    std::cout << "\n[Symlinks — symlink, hardlink, ln, readlink, is_symlink]\n";

    runTest("symlink creates symlink", [&]()
            {
        std::ofstream(g_testDir + "/symtarget.txt") << "target content";
        runXell("symlink(\"" + g_testDir + "/symtarget.txt\", \"" + g_testDir + "/sym.link\")");
        auto out = runXell("print(is_symlink(\"" + g_testDir + "/sym.link\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("readlink resolves symlink", [&]()
            {
        auto out = runXell("print(readlink(\"" + g_testDir + "/sym.link\"))");
        XASSERT(out[0].find("symtarget.txt") != std::string::npos); });

    runTest("hardlink creates hard link", [&]()
            {
        runXell("hardlink(\"" + g_testDir + "/symtarget.txt\", \"" + g_testDir + "/hard.link\")");
        auto out = runXell("print(exists(\"" + g_testDir + "/hard.link\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("ln creates soft link by default", [&]()
            {
        runXell("ln(\"" + g_testDir + "/symtarget.txt\", \"" + g_testDir + "/ln_soft.link\")");
        auto out = runXell("print(is_symlink(\"" + g_testDir + "/ln_soft.link\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("ln hard link with false", [&]()
            {
        runXell("ln(\"" + g_testDir + "/symtarget.txt\", \"" + g_testDir + "/ln_hard.link\", false)");
        auto out = runXell("print(exists(\"" + g_testDir + "/ln_hard.link\"))");
        XASSERT_EQ(out[0], "true");
        auto out2 = runXell("print(is_symlink(\"" + g_testDir + "/ln_hard.link\"))");
        XASSERT_EQ(out2[0], "false"); });

    runTest("is_symlink false for regular file", [&]()
            {
        auto out = runXell("print(is_symlink(\"" + g_testDir + "/symtarget.txt\"))");
        XASSERT_EQ(out[0], "false"); });
}

// ============================================================================
// Section 4: stat, file metadata
// ============================================================================

static void testStat()
{
    std::cout << "\n[stat, modified_time, created_time, chmod]\n";

    runTest("stat returns map with fields", [&]()
            {
        std::ofstream(g_testDir + "/statfile.txt") << "stat test";
        auto out = runXell(
            "s = stat(\"" + g_testDir + "/statfile.txt\")\n"
            "print(s[\"is_file\"])\n"
            "print(s[\"is_dir\"])\n"
            "print(s[\"type\"])");
        XASSERT_EQ(out[0], "true");
        XASSERT_EQ(out[1], "false");
        XASSERT_EQ(out[2], "file"); });

    runTest("stat size is correct", [&]()
            {
        auto out = runXell(
            "s = stat(\"" + g_testDir + "/statfile.txt\")\n"
            "print(s[\"size\"])");
        XASSERT_EQ(out[0], "9"); }); // "stat test" = 9 bytes

    runTest("stat for directory", [&]()
            {
        auto out = runXell(
            "s = stat(\"" + g_testDir + "\")\n"
            "print(s[\"is_dir\"])\n"
            "print(s[\"type\"])");
        XASSERT_EQ(out[0], "true");
        XASSERT_EQ(out[1], "directory"); });

    runTest("modified_time returns number", [&]()
            {
        auto out = runXell("print(modified_time(\"" + g_testDir + "/statfile.txt\") > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("created_time returns number", [&]()
            {
        auto out = runXell("print(created_time(\"" + g_testDir + "/statfile.txt\") > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("chmod sets permissions", [&]()
            {
        std::ofstream(g_testDir + "/chmodtest.txt") << "perm test";
        runXell("chmod(\"" + g_testDir + "/chmodtest.txt\", 755)");
        // Check it's still readable
        auto out = runXell("print(read(\"" + g_testDir + "/chmodtest.txt\"))");
        XASSERT_EQ(out[0], "perm test"); });
}

// ============================================================================
// Section 5: find, find_regex, locate, glob
// ============================================================================

static void testSearch()
{
    std::cout << "\n[find, find_regex, locate, glob]\n";

    // Setup test structure
    fs::create_directories(g_testDir + "/search/sub1");
    fs::create_directories(g_testDir + "/search/sub2");
    std::ofstream(g_testDir + "/search/a.txt");
    std::ofstream(g_testDir + "/search/b.xel");
    std::ofstream(g_testDir + "/search/sub1/c.txt");
    std::ofstream(g_testDir + "/search/sub1/d.xel");
    std::ofstream(g_testDir + "/search/sub2/e.txt");

    runTest("find *.txt recursively", [&]()
            {
        auto out = runXell("print(size(find(\"" + g_testDir + "/search\", \"*.txt\")))");
        XASSERT_EQ(out[0], "3"); }); // a.txt, c.txt, e.txt

    runTest("find *.xel recursively", [&]()
            {
        auto out = runXell("print(size(find(\"" + g_testDir + "/search\", \"*.xel\")))");
        XASSERT_EQ(out[0], "2"); }); // b.xel, d.xel

    runTest("find_regex pattern", [&]()
            {
        auto out = runXell("print(size(find_regex(\"" + g_testDir + "/search\", \"^[a-c]\\\\.txt$\")))");
        XASSERT_EQ(out[0], "2"); }); // a.txt, c.txt

    runTest("glob in directory", [&]()
            {
        auto out = runXell("print(size(glob(\"" + g_testDir + "/search/*.txt\")))");
        XASSERT_EQ(out[0], "1"); }); // just a.txt (not recursive)

    runTest("locate finds file", [&]()
            {
        // cd to search dir first, locate searches from cwd
        auto savedCwd = fs::current_path();
        fs::current_path(g_testDir + "/search");
        auto out = runXell("print(size(locate(\"d.xel\")))");
        XASSERT_EQ(out[0], "1");
        fs::current_path(savedCwd); });
}

// ============================================================================
// Section 6: diff, tree
// ============================================================================

static void testDiffTree()
{
    std::cout << "\n[diff, tree]\n";

    runTest("file_diff identical files", [&]()
            {
        std::ofstream(g_testDir + "/diff1.txt") << "line1\nline2";
        std::ofstream(g_testDir + "/diff2.txt") << "line1\nline2";
        auto out = runXell("print(size(file_diff(\"" + g_testDir + "/diff1.txt\", \"" + g_testDir + "/diff2.txt\")))");
        XASSERT_EQ(out[0], "0"); });

    runTest("file_diff different files", [&]()
            {
        std::ofstream(g_testDir + "/diffa.txt") << "alpha\nbeta";
        std::ofstream(g_testDir + "/diffb.txt") << "alpha\ngamma";
        auto out = runXell("print(size(file_diff(\"" + g_testDir + "/diffa.txt\", \"" + g_testDir + "/diffb.txt\")))");
        XASSERT_EQ(out[0], "1"); }); // line 2 differs

    runTest("tree returns string", [&]()
            {
        fs::create_directories(g_testDir + "/treedir/sub");
        std::ofstream(g_testDir + "/treedir/file.txt");
        std::ofstream(g_testDir + "/treedir/sub/inner.txt");
        auto out = runXell("t = tree(\"" + g_testDir + "/treedir\")\nprint(size(t) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("tree contains filenames", [&]()
            {
        auto out = runXell("t = tree(\"" + g_testDir + "/treedir\")\nprint(regex_match(t, \"file.txt\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("tree with depth limit", [&]()
            {
        auto out = runXell("t = tree(\"" + g_testDir + "/treedir\", 0)\nprint(regex_match(t, \"inner.txt\"))");
        XASSERT_EQ(out[0], "false"); }); // depth 0 should not show inner.txt
}

// ============================================================================
// Section 7: Path operations
// ============================================================================

static void testPathOps()
{
    std::cout << "\n[Path ops — extension, stem, realpath, join_path, normalize, is_absolute, relative_path]\n";

    runTest("extension returns .ext", []()
            {
        auto out = runXell("print(extension(\"file.txt\"))");
        XASSERT_EQ(out[0], ".txt"); });

    runTest("extension no ext returns empty", []()
            {
        auto out = runXell("print(extension(\"Makefile\"))");
        XASSERT_EQ(out[0], ""); });

    runTest("stem returns name without ext", []()
            {
        auto out = runXell("print(stem(\"file.txt\"))");
        XASSERT_EQ(out[0], "file"); });

    runTest("stem with dotfiles", []()
            {
        auto out = runXell("print(stem(\".gitignore\"))");
        XASSERT_EQ(out[0], ".gitignore"); });

    runTest("realpath resolves path", [&]()
            {
        auto out = runXell("print(size(realpath(\"" + g_testDir + "\")) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("join_path joins", []()
            {
        auto out = runXell("print(join_path(\"a\", \"b\", \"c.txt\"))");
        XASSERT_EQ(out[0], "a/b/c.txt"); });

    runTest("normalize resolves dots", []()
            {
        auto out = runXell("print(normalize(\"a/b/../c\"))");
        XASSERT_EQ(out[0], "a/c"); });

    runTest("is_absolute true for /path", []()
            {
        auto out = runXell("print(is_absolute(\"/usr/bin\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("is_absolute false for relative", []()
            {
        auto out = runXell("print(is_absolute(\"src/main.cpp\"))");
        XASSERT_EQ(out[0], "false"); });

    runTest("relative_path computes relative", []()
            {
        auto out = runXell("print(relative_path(\"/a/b/c\", \"/a\"))");
        XASSERT_EQ(out[0], "b/c"); });

    runTest("basename returns filename", []()
            {
        auto out = runXell("print(basename(\"/a/b/c.txt\"))");
        XASSERT_EQ(out[0], "c.txt"); });

    runTest("dirname returns parent", []()
            {
        auto out = runXell("print(dirname(\"/a/b/c.txt\"))");
        XASSERT_EQ(out[0], "/a/b"); });
}

// ============================================================================
// Section 8: home_dir, temp_dir, cwd
// ============================================================================

static void testDirHelpers()
{
    std::cout << "\n[home_dir, temp_dir, cwd]\n";

    runTest("home_dir returns non-empty", []()
            {
        auto out = runXell("print(size(home_dir()) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("temp_dir returns non-empty", []()
            {
        auto out = runXell("print(size(temp_dir()) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("cwd matches pwd", []()
            {
        auto out = runXell("print(cwd() == pwd())");
        XASSERT_EQ(out[0], "true"); });
}

// ============================================================================
// Section 9: disk_usage, disk_free
// ============================================================================

static void testDiskOps()
{
    std::cout << "\n[disk_usage, disk_free]\n";

    runTest("disk_usage on file", [&]()
            {
        std::ofstream(g_testDir + "/du_file.txt") << "twelve chars"; // 12 bytes
        auto out = runXell("print(disk_usage(\"" + g_testDir + "/du_file.txt\"))");
        XASSERT_EQ(out[0], "12"); });

    runTest("disk_usage on directory", [&]()
            {
        auto out = runXell("print(disk_usage(\"" + g_testDir + "\") > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("disk_free returns positive", [&]()
            {
        auto out = runXell("print(disk_free(\"" + g_testDir + "\") > 0)");
        XASSERT_EQ(out[0], "true"); });
}

// ============================================================================
// Section 10: xxd, strings
// ============================================================================

static void testBinaryOps()
{
    std::cout << "\n[xxd, strings]\n";

    runTest("xxd returns hex dump", [&]()
            {
        std::ofstream(g_testDir + "/xxdtest.bin", std::ios::binary) << "ABC";
        auto out = runXell("x = xxd(\"" + g_testDir + "/xxdtest.bin\")\nprint(size(x) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("xxd contains hex values", [&]()
            {
        auto out = runXell("x = xxd(\"" + g_testDir + "/xxdtest.bin\")\nprint(regex_match(x, \"41 42 43\"))");
        XASSERT_EQ(out[0], "true"); }); // 0x41=A, 0x42=B, 0x43=C

    runTest("strings extracts printable strings", [&]()
            {
        {
            std::ofstream ofs(g_testDir + "/strtest.bin", std::ios::binary);
            ofs << '\0' << '\0' << "hello world" << '\0' << '\x01' << "test";
        }
        auto out = runXell("print(strings(\"" + g_testDir + "/strtest.bin\"))");
        // "hello world" (11 chars >= 4 min), "test" (4 chars >= 4 min)
        XASSERT_EQ(out[0], "[\"hello world\", \"test\"]"); });

    runTest("strings with custom min length", [&]()
            {
        auto out = runXell("print(strings(\"" + g_testDir + "/strtest.bin\", 6))");
        // Only "hello world" (11 chars >= 6)
        XASSERT_EQ(out[0], "[\"hello world\"]"); });
}

// ============================================================================
// Section 11: Existing OS builtins (regression)
// ============================================================================

static void testExistingOSBuiltins()
{
    std::cout << "\n[Regression — existing OS builtins]\n";

    runTest("mkdir + exists", [&]()
            {
        runXell("mkdir(\"" + g_testDir + "/newdir\")");
        auto out = runXell("print(exists(\"" + g_testDir + "/newdir\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("write + read", [&]()
            {
        runXell("write(\"" + g_testDir + "/rw.txt\", \"hello\")");
        auto out = runXell("print(read(\"" + g_testDir + "/rw.txt\"))");
        XASSERT_EQ(out[0], "hello"); });

    runTest("append adds content", [&]()
            {
        runXell("write(\"" + g_testDir + "/app.txt\", \"a\")");
        runXell("append(\"" + g_testDir + "/app.txt\", \"b\")");
        auto out = runXell("print(read(\"" + g_testDir + "/app.txt\"))");
        XASSERT_EQ(out[0], "ab"); });

    runTest("is_file true for file", [&]()
            {
        auto out = runXell("print(is_file(\"" + g_testDir + "/rw.txt\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("is_dir true for directory", [&]()
            {
        auto out = runXell("print(is_dir(\"" + g_testDir + "/newdir\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("file_size returns correct size", [&]()
            {
        runXell("write(\"" + g_testDir + "/sz.txt\", \"12345\")");
        auto out = runXell("print(file_size(\"" + g_testDir + "/sz.txt\"))");
        XASSERT_EQ(out[0], "5"); });

    runTest("cp copies file", [&]()
            {
        runXell("cp(\"" + g_testDir + "/rw.txt\", \"" + g_testDir + "/rw_copy.txt\")");
        auto out = runXell("print(read(\"" + g_testDir + "/rw_copy.txt\"))");
        XASSERT_EQ(out[0], "hello"); });

    runTest("mv renames file", [&]()
            {
        runXell("write(\"" + g_testDir + "/mvold.txt\", \"moved\")");
        runXell("mv(\"" + g_testDir + "/mvold.txt\", \"" + g_testDir + "/mvnew.txt\")");
        auto out = runXell("print(exists(\"" + g_testDir + "/mvold.txt\"))");
        XASSERT_EQ(out[0], "false");
        auto out2 = runXell("print(read(\"" + g_testDir + "/mvnew.txt\"))");
        XASSERT_EQ(out2[0], "moved"); });

    runTest("rm removes file", [&]()
            {
        runXell("write(\"" + g_testDir + "/todel.txt\", \"x\")");
        runXell("rm(\"" + g_testDir + "/todel.txt\")");
        auto out = runXell("print(exists(\"" + g_testDir + "/todel.txt\"))");
        XASSERT_EQ(out[0], "false"); });

    runTest("ls lists directory contents", [&]()
            {
        auto out = runXell("print(size(ls(\"" + g_testDir + "\")) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("abspath returns absolute", [&]()
            {
        auto out = runXell("print(is_absolute(abspath(\".\")) )");
        XASSERT_EQ(out[0], "true"); });

    runTest("ext returns extension", []()
            {
        auto out = runXell("print(ext(\"file.cpp\"))");
        XASSERT_EQ(out[0], ".cpp"); });
}

// ============================================================================
// Section 12: Error handling
// ============================================================================

static void testErrors()
{
    std::cout << "\n[Error handling]\n";

    runTest("touch arity error", []()
            { XASSERT(expectError<std::exception>("touch()")); });

    runTest("cat type error", []()
            { XASSERT(expectError<std::exception>("cat(123)")); });

    runTest("read_lines type error", []()
            { XASSERT(expectError<std::exception>("read_lines(123)")); });

    runTest("write_lines arity error", []()
            { XASSERT(expectError<std::exception>("write_lines(\"a\")")); });

    runTest("symlink arity error", []()
            { XASSERT(expectError<std::exception>("symlink(\"a\")")); });

    runTest("stat nonexistent throws", []()
            { XASSERT(expectError<std::exception>("stat(\"/nonexistent_path_xyz\")")); });

    runTest("find nonexistent dir throws", []()
            { XASSERT(expectError<std::exception>("find(\"/nonexistent_dir_xyz\", \"*\")")); });

    runTest("file_diff missing file throws", []()
            { XASSERT(expectError<std::exception>("file_diff(\"/nonexistent1\", \"/nonexistent2\")")); });

    runTest("xxd missing file throws", []()
            { XASSERT(expectError<std::exception>("xxd(\"/nonexistent_xyz\")")); });

    runTest("chmod arity error", []()
            { XASSERT(expectError<std::exception>("chmod(\"file\")")); });

    runTest("join_path no args throws", []()
            { XASSERT(expectError<std::exception>("join_path()")); });

    runTest("disk_free nonexistent throws", []()
            { XASSERT(expectError<std::exception>("disk_free(\"/nonexistent_mount_xyz\")")); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===== File System Builtin Tests =====\n";

    setupTestDir();

    testFileOps();
    testListAndPwd();
    testSymlinks();
    testStat();
    testSearch();
    testDiffTree();
    testPathOps();
    testDirHelpers();
    testDiskOps();
    testBinaryOps();
    testExistingOSBuiltins();
    testErrors();

    cleanupTestDir();

    std::cout << "\n===== Results: " << g_passed << " passed, "
              << g_failed << " failed =====\n";
    return g_failed > 0 ? 1 : 0;
}
