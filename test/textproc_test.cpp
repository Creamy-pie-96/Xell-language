// =============================================================================
// Text Processing Tests — head, tail, grep, sed, awk, cut, sort_file, uniq,
// wc, tee, tr, patch, less, more, xargs, grep_recursive, grep_regex
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

static std::vector<std::string> runXell(const std::string &source)
{
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();
    Interpreter interp;
    interp.loadModule("textproc");
    interp.loadModule("fs");
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

// Test sandbox
static std::string g_testDir;

static void setupTestDir()
{
    g_testDir = fs::temp_directory_path().string() + "/xell_textproc_test_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(g_testDir);
}

static void cleanupTestDir()
{
    if (!g_testDir.empty() && fs::exists(g_testDir))
        fs::remove_all(g_testDir);
}

// Helper: create a test file with known content
static void createFile(const std::string &name, const std::string &content)
{
    std::ofstream ofs(g_testDir + "/" + name);
    ofs << content;
}

// ============================================================================
// Section 1: head
// ============================================================================

static void testHead()
{
    std::cout << "\n[head]\n";
    createFile("lines.txt", "line1\nline2\nline3\nline4\nline5\nline6\nline7\nline8\nline9\nline10\nline11\nline12");

    runTest("head default 10 lines", [&]()
            {
        auto out = runXell("print(size(head(\"" + g_testDir + "/lines.txt\")))");
        XASSERT_EQ(out[0], "10"); });

    runTest("head 3 lines", [&]()
            {
        auto out = runXell("h = head(\"" + g_testDir + "/lines.txt\", 3)\nprint(h)");
        XASSERT_EQ(out[0], "[\"line1\", \"line2\", \"line3\"]"); });

    runTest("head more lines than file", [&]()
            {
        auto out = runXell("print(size(head(\"" + g_testDir + "/lines.txt\", 100)))");
        XASSERT_EQ(out[0], "12"); });

    runTest("head nonexistent throws", [&]()
            { XASSERT(expectError<std::exception>("head(\"" + g_testDir + "/nonexistent\")")); });

    runTest("head function style", [&]()
            {
        auto out = runXell("h = head(\"" + g_testDir + "/lines.txt\", 2)\nprint(size(h))");
        XASSERT_EQ(out[0], "2"); });
}

// ============================================================================
// Section 2: tail
// ============================================================================

static void testTail()
{
    std::cout << "\n[tail]\n";
    createFile("tail.txt", "a\nb\nc\nd\ne\nf\ng\nh\ni\nj\nk\nl");

    runTest("tail default 10 lines", [&]()
            {
        auto out = runXell("print(size(tail(\"" + g_testDir + "/tail.txt\")))");
        XASSERT_EQ(out[0], "10"); });

    runTest("tail 3 lines", [&]()
            {
        auto out = runXell("t = tail(\"" + g_testDir + "/tail.txt\", 3)\nprint(t)");
        XASSERT_EQ(out[0], "[\"j\", \"k\", \"l\"]"); });

    runTest("tail 1 line", [&]()
            {
        auto out = runXell("print(tail(\"" + g_testDir + "/tail.txt\", 1))");
        XASSERT_EQ(out[0], "[\"l\"]"); });

    runTest("tail nonexistent throws", [&]()
            { XASSERT(expectError<std::exception>("tail(\"" + g_testDir + "/nofile\")")); });
}

// ============================================================================
// Section 3: grep
// ============================================================================

static void testGrep()
{
    std::cout << "\n[grep, grep_regex, grep_recursive]\n";
    createFile("log.txt", "INFO: started\nERROR: disk full\nINFO: running\nERROR: timeout\nDEBUG: trace");

    runTest("grep finds matching lines", [&]()
            {
        auto out = runXell("print(size(grep(\"ERROR\", \"" + g_testDir + "/log.txt\")))");
        XASSERT_EQ(out[0], "2"); });

    runTest("grep returns correct lines", [&]()
            {
        auto out = runXell("g = grep(\"ERROR\", \"" + g_testDir + "/log.txt\")\nprint(g[0])");
        XASSERT_EQ(out[0], "ERROR: disk full"); });

    runTest("grep no matches returns empty", [&]()
            {
        auto out = runXell("print(size(grep(\"FATAL\", \"" + g_testDir + "/log.txt\")))");
        XASSERT_EQ(out[0], "0"); });

    runTest("grep function style", [&]()
            {
        auto out = runXell("g = grep(\"INFO\", \"" + g_testDir + "/log.txt\")\nprint(size(g))");
        XASSERT_EQ(out[0], "2"); });

    runTest("grep_regex with pattern", [&]()
            {
        auto out = runXell("print(size(grep_regex(\"^ERROR\", \"" + g_testDir + "/log.txt\")))");
        XASSERT_EQ(out[0], "2"); });

    runTest("grep_regex complex pattern", [&]()
            {
        auto out = runXell("print(size(grep_regex(\"INFO|DEBUG\", \"" + g_testDir + "/log.txt\")))");
        XASSERT_EQ(out[0], "3"); });

    // grep_recursive
    fs::create_directories(g_testDir + "/grepdir/sub");
    createFile("grepdir/a.txt", "hello world\nfoo bar");
    createFile("grepdir/sub/b.txt", "hello again\nbaz hello");

    runTest("grep_recursive finds in subdirs", [&]()
            {
        auto out = runXell("print(size(grep_recursive(\"hello\", \"" + g_testDir + "/grepdir\")))");
        XASSERT_EQ(out[0], "3"); }); // "hello world", "hello again", "baz hello"

    runTest("grep_recursive returns file info", [&]()
            {
        auto out = runXell(
            "r = grep_recursive(\"hello\", \"" + g_testDir + "/grepdir\")\n"
            "m = r[0]\n"
            "print(has(m, \"file\"))\n"
            "print(has(m, \"line_number\"))\n"
            "print(has(m, \"text\"))");
        XASSERT_EQ(out[0], "true");
        XASSERT_EQ(out[1], "true");
        XASSERT_EQ(out[2], "true"); });
}

// ============================================================================
// Section 4: sed
// ============================================================================

static void testSed()
{
    std::cout << "\n[sed]\n";
    createFile("sed.txt", "hello world\nfoo bar\nhello there");

    runTest("sed replaces pattern", [&]()
            {
        auto out = runXell("print(sed(\"hello\", \"hi\", \"" + g_testDir + "/sed.txt\"))");
        XASSERT_EQ(out[0], "hi world\nfoo bar\nhi there"); });

    runTest("sed in-place modifies file", [&]()
            {
        createFile("sed_ip.txt", "aaa bbb\nccc ddd");
        runXell("sed(\"aaa\", \"xxx\", \"" + g_testDir + "/sed_ip.txt\", true)");
        auto out = runXell("print(cat(\"" + g_testDir + "/sed_ip.txt\"))");
        XASSERT_EQ(out[0], "xxx bbb\nccc ddd"); });

    runTest("sed regex pattern", [&]()
            {
        createFile("sed_re.txt", "abc 123\ndef 456");
        auto out = runXell("print(sed(\"[0-9]+\", \"NUM\", \"" + g_testDir + "/sed_re.txt\"))");
        XASSERT_EQ(out[0], "abc NUM\ndef NUM"); });

    runTest("sed nonexistent throws", [&]()
            { XASSERT(expectError<std::exception>("sed(\"a\", \"b\", \"" + g_testDir + "/nofile\")")); });
}

// ============================================================================
// Section 5: awk
// ============================================================================

static void testAwk()
{
    std::cout << "\n[awk]\n";
    createFile("data.txt", "alice 25 NYC\nbob 30 LA\ncharlie 35 SF");

    runTest("awk $1 extracts first field", [&]()
            {
        auto out = runXell("r = awk(\"$1\", \"" + g_testDir + "/data.txt\")\nprint(r)");
        XASSERT_EQ(out[0], "[\"alice\", \"bob\", \"charlie\"]"); });

    runTest("awk $2 extracts second field", [&]()
            {
        auto out = runXell("r = awk(\"$2\", \"" + g_testDir + "/data.txt\")\nprint(r)");
        XASSERT_EQ(out[0], "[\"25\", \"30\", \"35\"]"); });

    runTest("awk $1 $3 extracts multiple fields", [&]()
            {
        auto out = runXell("r = awk(\"$1 $3\", \"" + g_testDir + "/data.txt\")\nprint(r[0])");
        XASSERT_EQ(out[0], "alice NYC"); });

    runTest("awk $0 returns whole line", [&]()
            {
        auto out = runXell("r = awk(\"$0\", \"" + g_testDir + "/data.txt\")\nprint(r[0])");
        XASSERT_EQ(out[0], "alice 25 NYC"); });

    runTest("awk $NF returns last field", [&]()
            {
        auto out = runXell("r = awk(\"$NF\", \"" + g_testDir + "/data.txt\")\nprint(r)");
        XASSERT_EQ(out[0], "[\"NYC\", \"LA\", \"SF\"]"); });

    runTest("awk with custom delimiter", [&]()
            {
        createFile("csv.txt", "a,b,c\n1,2,3");
        auto out = runXell("r = awk(\"$2\", \"" + g_testDir + "/csv.txt\", \",\")\nprint(r)");
        XASSERT_EQ(out[0], "[\"b\", \"2\"]"); });
}

// ============================================================================
// Section 6: cut
// ============================================================================

static void testCut()
{
    std::cout << "\n[cut]\n";
    createFile("cut.csv", "alice,25,NYC\nbob,30,LA\ncharlie,35,SF");

    runTest("cut extracts columns", [&]()
            {
        auto out = runXell("r = cut(\"" + g_testDir + "/cut.csv\", \",\", [1])\nprint(r)");
        XASSERT_EQ(out[0], "[\"alice\", \"bob\", \"charlie\"]"); });

    runTest("cut multiple columns", [&]()
            {
        auto out = runXell("r = cut(\"" + g_testDir + "/cut.csv\", \",\", [1, 3])\nprint(r[0])");
        XASSERT_EQ(out[0], "alice,NYC"); });

    runTest("cut column 2", [&]()
            {
        auto out = runXell("r = cut(\"" + g_testDir + "/cut.csv\", \",\", [2])\nprint(r)");
        XASSERT_EQ(out[0], "[\"25\", \"30\", \"35\"]"); });
}

// ============================================================================
// Section 7: sort_file, uniq
// ============================================================================

static void testSortUniq()
{
    std::cout << "\n[sort_file, uniq]\n";
    createFile("sort.txt", "banana\napple\ncherry\napple\nbanana");
    createFile("uniq.txt", "a\na\nb\nb\nb\nc\na");

    runTest("sort_file ascending", [&]()
            {
        auto out = runXell("r = sort_file(\"" + g_testDir + "/sort.txt\")\nprint(r[0])\nprint(r[1])");
        XASSERT_EQ(out[0], "apple");
        XASSERT_EQ(out[1], "apple"); });

    runTest("sort_file descending", [&]()
            {
        auto out = runXell("r = sort_file(\"" + g_testDir + "/sort.txt\", true)\nprint(r[0])");
        XASSERT_EQ(out[0], "cherry"); });

    runTest("uniq removes consecutive dupes", [&]()
            {
        auto out = runXell("r = uniq(\"" + g_testDir + "/uniq.txt\")\nprint(r)");
        XASSERT_EQ(out[0], "[\"a\", \"b\", \"c\", \"a\"]"); }); // a, b, c, a (last a is non-consecutive)

    runTest("uniq on already unique", [&]()
            {
        createFile("u2.txt", "x\ny\nz");
        auto out = runXell("print(size(uniq(\"" + g_testDir + "/u2.txt\")))");
        XASSERT_EQ(out[0], "3"); });
}

// ============================================================================
// Section 8: wc
// ============================================================================

static void testWc()
{
    std::cout << "\n[wc]\n";
    createFile("wc.txt", "hello world\nfoo bar baz\n");

    runTest("wc returns map with lines/words/bytes", [&]()
            {
        auto out = runXell(
            "w = wc(\"" + g_testDir + "/wc.txt\")\n"
            "print(w[\"lines\"])\n"
            "print(w[\"words\"])");
        XASSERT_EQ(out[0], "2");
        XASSERT_EQ(out[1], "5"); });

    runTest("wc tee command as statement", [&]()
            {
        // Command-style works as statement (side effects only)
        runXell("tee \"wc test content\" \"" + g_testDir + "/wc_cmd.txt\"");
        auto out = runXell("print(cat(\"" + g_testDir + "/wc_cmd.txt\"))");
        XASSERT_EQ(out[0], "wc test content"); });

    runTest("wc nonexistent throws", [&]()
            { XASSERT(expectError<std::exception>("wc(\"" + g_testDir + "/nofile\")")); });
}

// ============================================================================
// Section 9: tee
// ============================================================================

static void testTee()
{
    std::cout << "\n[tee]\n";

    runTest("tee writes and returns content", [&]()
            {
        auto out = runXell("r = tee(\"hello tee\", \"" + g_testDir + "/tee.txt\")\nprint(r)");
        XASSERT_EQ(out[0], "hello tee"); });

    runTest("tee file has content", [&]()
            {
        auto out = runXell("print(cat(\"" + g_testDir + "/tee.txt\"))");
        XASSERT_EQ(out[0], "hello tee"); });
}

// ============================================================================
// Section 10: tr
// ============================================================================

static void testTr()
{
    std::cout << "\n[tr]\n";

    runTest("tr basic translate", []()
            {
        auto out = runXell("print(tr(\"abc\", \"xyz\", \"aabbcc\"))");
        XASSERT_EQ(out[0], "xxyyzz"); });

    runTest("tr range expansion a-z", []()
            {
        auto out = runXell("print(tr(\"a-z\", \"A-Z\", \"hello\"))");
        XASSERT_EQ(out[0], "HELLO"); });

    runTest("tr partial match", []()
            {
        auto out = runXell("print(tr(\"aeiou\", \"AEIOU\", \"hello world\"))");
        XASSERT_EQ(out[0], "hEllO wOrld"); });

    runTest("tr no match passes through", []()
            {
        auto out = runXell("print(tr(\"xyz\", \"XYZ\", \"hello\"))");
        XASSERT_EQ(out[0], "hello"); });
}

// ============================================================================
// Section 11: patch
// ============================================================================

static void testPatch()
{
    std::cout << "\n[patch]\n";

    runTest("patch modifies file from diff", [&]()
            {
        createFile("patch_orig.txt", "alpha\nbeta\ngamma");
        createFile("patch_diff.txt", "~2: -beta +BETA");
        runXell("patch(\"" + g_testDir + "/patch_orig.txt\", \"" + g_testDir + "/patch_diff.txt\")");
        auto out = runXell("print(cat(\"" + g_testDir + "/patch_orig.txt\"))");
        XASSERT_EQ(out[0], "alpha\nBETA\ngamma"); });

    runTest("patch add line", [&]()
            {
        createFile("patch2.txt", "a\nb");
        createFile("diff2.txt", "+3: c");
        runXell("patch(\"" + g_testDir + "/patch2.txt\", \"" + g_testDir + "/diff2.txt\")");
        auto out = runXell("print(read_lines(\"" + g_testDir + "/patch2.txt\"))");
        XASSERT_EQ(out[0], "[\"a\", \"b\", \"c\"]"); });
}

// ============================================================================
// Section 12: less, more
// ============================================================================

static void testLessMore()
{
    std::cout << "\n[less, more]\n";
    // Create a file with 25 lines
    {
        std::ofstream ofs(g_testDir + "/bigfile.txt");
        for (int i = 1; i <= 25; i++)
            ofs << "line" << i << "\n";
    }

    runTest("less paginates into pages", [&]()
            {
        auto out = runXell("pages = less(\"" + g_testDir + "/bigfile.txt\", 10)\nprint(size(pages))");
        XASSERT_EQ(out[0], "3"); }); // 10+10+5

    runTest("less first page has 10 lines", [&]()
            {
        auto out = runXell("pages = less(\"" + g_testDir + "/bigfile.txt\", 10)\nprint(size(pages[0]))");
        XASSERT_EQ(out[0], "10"); });

    runTest("more works same as less", [&]()
            {
        auto out = runXell("pages = more(\"" + g_testDir + "/bigfile.txt\", 10)\nprint(size(pages))");
        XASSERT_EQ(out[0], "3"); });
}

// ============================================================================
// Section 13: Error handling
// ============================================================================

static void testErrors()
{
    std::cout << "\n[Error handling]\n";

    runTest("head arity error", []()
            { XASSERT(expectError<std::exception>("head()")); });

    runTest("tail type error", []()
            { XASSERT(expectError<std::exception>("tail(123)")); });

    runTest("grep arity error", []()
            { XASSERT(expectError<std::exception>("grep(\"x\")")); });

    runTest("sed arity error", []()
            { XASSERT(expectError<std::exception>("sed(\"a\", \"b\")")); });

    runTest("awk type error", []()
            { XASSERT(expectError<std::exception>("awk(123, 456)")); });

    runTest("cut type error", []()
            { XASSERT(expectError<std::exception>("cut(123, \",\", [1])")); });

    runTest("wc type error", []()
            { XASSERT(expectError<std::exception>("wc(123)")); });

    runTest("tee arity error", []()
            { XASSERT(expectError<std::exception>("tee(\"x\")")); });

    runTest("tr arity error", []()
            { XASSERT(expectError<std::exception>("tr(\"a\", \"b\")")); });

    runTest("sort_file nonexistent", [&]()
            { XASSERT(expectError<std::exception>("sort_file(\"" + g_testDir + "/nofile\")")); });

    runTest("uniq nonexistent", [&]()
            { XASSERT(expectError<std::exception>("uniq(\"" + g_testDir + "/nofile\")")); });

    runTest("less nonexistent", [&]()
            { XASSERT(expectError<std::exception>("less(\"" + g_testDir + "/nofile\")")); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===== Text Processing Builtin Tests =====\n";

    setupTestDir();

    testHead();
    testTail();
    testGrep();
    testSed();
    testAwk();
    testCut();
    testSortUniq();
    testWc();
    testTee();
    testTr();
    testPatch();
    testLessMore();
    testErrors();

    cleanupTestDir();

    std::cout << "\n===== Results: " << g_passed << " passed, "
              << g_failed << " failed =====\n";
    return g_failed > 0 ? 1 : 0;
}
