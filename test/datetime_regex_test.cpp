// =============================================================================
// DateTime & Regex Tests — Comprehensive coverage for both modules
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
#include <chrono>
#include <ctime>

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
    interp.loadModule("datetime");
    interp.loadModule("regex");
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
// Section 1: DateTime — now()
// ============================================================================

static void testDateTimeNow()
{
    std::cout << "\n[DateTime — now]\n";

    runTest("now returns map with year", []()
            {
        auto out = runXell(
            "d = now()\n"
            "print(d[\"year\"] >= 2024)");
        XASSERT_EQ(out[0], "true"); });

    runTest("now returns map with month", []()
            {
        auto out = runXell(
            "d = now()\n"
            "print(d[\"month\"] >= 1 and d[\"month\"] <= 12)");
        XASSERT_EQ(out[0], "true"); });

    runTest("now returns map with day", []()
            {
        auto out = runXell(
            "d = now()\n"
            "print(d[\"day\"] >= 1 and d[\"day\"] <= 31)");
        XASSERT_EQ(out[0], "true"); });

    runTest("now returns map with hour", []()
            {
        auto out = runXell(
            "d = now()\n"
            "print(d[\"hour\"] >= 0 and d[\"hour\"] <= 23)");
        XASSERT_EQ(out[0], "true"); });

    runTest("now returns map with minute", []()
            {
        auto out = runXell(
            "d = now()\n"
            "print(d[\"minute\"] >= 0 and d[\"minute\"] <= 59)");
        XASSERT_EQ(out[0], "true"); });

    runTest("now returns map with second", []()
            {
        auto out = runXell(
            "d = now()\n"
            "print(d[\"second\"] >= 0 and d[\"second\"] <= 59)");
        XASSERT_EQ(out[0], "true"); });

    runTest("now returns map with weekday 0-6", []()
            {
        auto out = runXell(
            "d = now()\n"
            "print(d[\"weekday\"] >= 0 and d[\"weekday\"] <= 6)");
        XASSERT_EQ(out[0], "true"); });

    runTest("now returns map with yearday 1-366", []()
            {
        auto out = runXell(
            "d = now()\n"
            "print(d[\"yearday\"] >= 1 and d[\"yearday\"] <= 366)");
        XASSERT_EQ(out[0], "true"); });

    runTest("now arity error", []()
            { XASSERT(expectError<std::exception>("now(1)")); });
}

// ============================================================================
// Section 2: DateTime — timestamp / timestamp_ms
// ============================================================================

static void testDateTimeTimestamp()
{
    std::cout << "\n[DateTime — timestamp / timestamp_ms]\n";

    runTest("timestamp returns positive integer", []()
            {
        auto out = runXell("print(timestamp() > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("timestamp is reasonable (after 2024-01-01)", []()
            {
        // 2024-01-01 00:00:00 UTC ~ 1704067200
        auto out = runXell("print(timestamp() > 1704067200)");
        XASSERT_EQ(out[0], "true"); });

    runTest("timestamp_ms returns positive integer", []()
            {
        auto out = runXell("print(timestamp_ms() > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("timestamp_ms >= timestamp * 1000 approx", []()
            {
        auto out = runXell(
            "ts = timestamp()\n"
            "tms = timestamp_ms()\n"
            "print(tms >= ts * 1000 - 1000)");
        XASSERT_EQ(out[0], "true"); });

    runTest("timestamp arity error", []()
            { XASSERT(expectError<std::exception>("timestamp(1)")); });

    runTest("timestamp_ms arity error", []()
            { XASSERT(expectError<std::exception>("timestamp_ms(1)")); });
}

// ============================================================================
// Section 3: DateTime — format_date
// ============================================================================

static void testDateTimeFormat()
{
    std::cout << "\n[DateTime — format_date]\n";

    runTest("format_date with %Y-%m-%d", []()
            {
        auto out = runXell(
            "d = {\"year\": 2025, \"month\": 6, \"day\": 15}\n"
            "print(format_date(d, \"%Y-%m-%d\"))");
        XASSERT_EQ(out[0], "2025-06-15"); });

    runTest("format_date with %H:%M:%S", []()
            {
        auto out = runXell(
            "d = {\"year\": 2025, \"month\": 1, \"day\": 1, \"hour\": 14, \"minute\": 30, \"second\": 45}\n"
            "print(format_date(d, \"%H:%M:%S\"))");
        XASSERT_EQ(out[0], "14:30:45"); });

    runTest("format_date full datetime", []()
            {
        auto out = runXell(
            "d = {\"year\": 2025, \"month\": 3, \"day\": 20, \"hour\": 9, \"minute\": 5, \"second\": 3}\n"
            "print(format_date(d, \"%Y/%m/%d %H:%M:%S\"))");
        XASSERT_EQ(out[0], "2025/03/20 09:05:03"); });

    runTest("format_date defaults missing fields to zero", []()
            {
        auto out = runXell(
            "d = {\"year\": 2025, \"month\": 12, \"day\": 25}\n"
            "print(format_date(d, \"%H:%M\"))");
        XASSERT_EQ(out[0], "00:00"); });

    runTest("format_date type error (non-map)", []()
            { XASSERT(expectError<std::exception>("format_date(\"hello\", \"%Y\")")); });

    runTest("format_date arity error", []()
            { XASSERT(expectError<std::exception>("format_date({\"year\": 2025})")); });
}

// ============================================================================
// Section 4: DateTime — parse_date
// ============================================================================

static void testDateTimeParse()
{
    std::cout << "\n[DateTime — parse_date]\n";

    runTest("parse_date %Y-%m-%d", []()
            {
        auto out = runXell(
            "d = parse_date(\"2025-06-15\", \"%Y-%m-%d\")\n"
            "print(d[\"year\"])\n"
            "print(d[\"month\"])\n"
            "print(d[\"day\"])");
        XASSERT_EQ(out[0], "2025");
        XASSERT_EQ(out[1], "6");
        XASSERT_EQ(out[2], "15"); });

    runTest("parse_date with time", []()
            {
        auto out = runXell(
            "d = parse_date(\"2025-01-20 14:30:45\", \"%Y-%m-%d %H:%M:%S\")\n"
            "print(d[\"hour\"])\n"
            "print(d[\"minute\"])\n"
            "print(d[\"second\"])");
        XASSERT_EQ(out[0], "14");
        XASSERT_EQ(out[1], "30");
        XASSERT_EQ(out[2], "45"); });

    runTest("parse_date invalid string throws", []()
            { XASSERT(expectError<std::exception>("parse_date(\"not-a-date\", \"%Y-%m-%d\")")); });

    runTest("parse_date type error", []()
            { XASSERT(expectError<std::exception>("parse_date(123, \"%Y\")")); });

    runTest("parse_date arity error", []()
            { XASSERT(expectError<std::exception>("parse_date(\"2025\")")); });
}

// ============================================================================
// Section 5: DateTime — sleep / sleep_sec
// ============================================================================

static void testDateTimeSleep()
{
    std::cout << "\n[DateTime — sleep / sleep_sec]\n";

    runTest("sleep(1) completes quickly", []()
            {
        auto start = std::chrono::steady_clock::now();
        runXell("sleep(1)");
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        XASSERT(elapsed >= 1 && elapsed < 200); });

    runTest("sleep_sec(0.001) completes quickly", []()
            {
        auto start = std::chrono::steady_clock::now();
        runXell("sleep_sec(0.001)");
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        XASSERT(elapsed >= 1 && elapsed < 200); });

    runTest("sleep(0) is a no-op", []()
            {
        auto start = std::chrono::steady_clock::now();
        runXell("sleep(0)");
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start).count();
        XASSERT(elapsed < 100); });

    runTest("sleep type error", []()
            { XASSERT(expectError<std::exception>("sleep(\"abc\")")); });

    runTest("sleep_sec type error", []()
            { XASSERT(expectError<std::exception>("sleep_sec(\"abc\")")); });
}

// ============================================================================
// Section 6: DateTime — time_since
// ============================================================================

static void testDateTimeTimeSince()
{
    std::cout << "\n[DateTime — time_since]\n";

    runTest("time_since returns positive value", []()
            {
        auto out = runXell(
            "ts = timestamp()\n"
            "sleep(5)\n"
            "elapsed = time_since(ts)\n"
            "print(elapsed >= 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("time_since with old timestamp gives large value", []()
            {
        auto out = runXell("print(time_since(0) > 1000000)");
        XASSERT_EQ(out[0], "true"); });

    runTest("time_since type error", []()
            { XASSERT(expectError<std::exception>("time_since(\"abc\")")); });

    runTest("time_since arity error", []()
            { XASSERT(expectError<std::exception>("time_since()")); });
}

// ============================================================================
// Section 7: Regex — regex_match / regex_match_full
// ============================================================================

static void testRegexMatch()
{
    std::cout << "\n[Regex — regex_match / regex_match_full]\n";

    runTest("regex_match partial match", []()
            {
        auto out = runXell("print(regex_match(\"hello world\", \"wor\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("regex_match no match", []()
            {
        auto out = runXell("print(regex_match(\"hello world\", \"xyz\"))");
        XASSERT_EQ(out[0], "false"); });

    runTest("regex_match with digit pattern", []()
            {
        auto out = runXell("print(regex_match(\"abc123\", \"[0-9]+\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("regex_match_full exact match", []()
            {
        auto out = runXell("print(regex_match_full(\"hello\", \"hello\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("regex_match_full partial fails", []()
            {
        auto out = runXell("print(regex_match_full(\"hello world\", \"hello\"))");
        XASSERT_EQ(out[0], "false"); });

    runTest("regex_match_full full pattern", []()
            {
        auto out = runXell("print(regex_match_full(\"abc123\", \"[a-z]+[0-9]+\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("regex_match type error", []()
            { XASSERT(expectError<std::exception>("regex_match(123, \"abc\")")); });

    runTest("regex_match_full type error", []()
            { XASSERT(expectError<std::exception>("regex_match_full(\"abc\", 123)")); });

    runTest("regex_match invalid pattern", []()
            { XASSERT(expectError<std::exception>("regex_match(\"abc\", \"[invalid\")")); });
}

// ============================================================================
// Section 8: Regex — regex_find / regex_find_all
// ============================================================================

static void testRegexFind()
{
    std::cout << "\n[Regex — regex_find / regex_find_all]\n";

    runTest("regex_find first match", []()
            {
        auto out = runXell("print(regex_find(\"abc 123 def 456\", \"[0-9]+\"))");
        XASSERT_EQ(out[0], "123"); });

    runTest("regex_find no match returns empty", []()
            {
        auto out = runXell("print(regex_find(\"hello\", \"[0-9]+\"))");
        XASSERT_EQ(out[0], ""); });

    runTest("regex_find_all all matches", []()
            {
        auto out = runXell("print(regex_find_all(\"abc 123 def 456 ghi 789\", \"[0-9]+\"))");
        XASSERT_EQ(out[0], "[\"123\", \"456\", \"789\"]"); });

    runTest("regex_find_all no matches returns empty list", []()
            {
        auto out = runXell("print(regex_find_all(\"hello world\", \"[0-9]+\"))");
        XASSERT_EQ(out[0], "[]"); });

    runTest("regex_find_all words", []()
            {
        auto out = runXell("print(regex_find_all(\"cat bat hat\", \"[cbh]at\"))");
        XASSERT_EQ(out[0], "[\"cat\", \"bat\", \"hat\"]"); });

    runTest("regex_find type error", []()
            { XASSERT(expectError<std::exception>("regex_find(123, \"abc\")")); });

    runTest("regex_find_all arity error", []()
            { XASSERT(expectError<std::exception>("regex_find_all(\"abc\")")); });
}

// ============================================================================
// Section 9: Regex — regex_replace / regex_replace_all
// ============================================================================

static void testRegexReplace()
{
    std::cout << "\n[Regex — regex_replace / regex_replace_all]\n";

    runTest("regex_replace first only", []()
            {
        auto out = runXell("print(regex_replace(\"aaa bbb aaa\", \"aaa\", \"xxx\"))");
        XASSERT_EQ(out[0], "xxx bbb aaa"); });

    runTest("regex_replace_all all occurrences", []()
            {
        auto out = runXell("print(regex_replace_all(\"aaa bbb aaa\", \"aaa\", \"xxx\"))");
        XASSERT_EQ(out[0], "xxx bbb xxx"); });

    runTest("regex_replace with pattern", []()
            {
        auto out = runXell("print(regex_replace(\"abc 123 def\", \"[0-9]+\", \"NUM\"))");
        XASSERT_EQ(out[0], "abc NUM def"); });

    runTest("regex_replace_all with pattern", []()
            {
        auto out = runXell("print(regex_replace_all(\"abc 123 def 456\", \"[0-9]+\", \"N\"))");
        XASSERT_EQ(out[0], "abc N def N"); });

    runTest("regex_replace no match returns original", []()
            {
        auto out = runXell("print(regex_replace(\"hello\", \"[0-9]+\", \"X\"))");
        XASSERT_EQ(out[0], "hello"); });

    runTest("regex_replace_all type error", []()
            { XASSERT(expectError<std::exception>("regex_replace_all(123, \"a\", \"b\")")); });

    runTest("regex_replace arity error", []()
            { XASSERT(expectError<std::exception>("regex_replace(\"abc\", \"a\")")); });
}

// ============================================================================
// Section 10: Regex — regex_split
// ============================================================================

static void testRegexSplit()
{
    std::cout << "\n[Regex — regex_split]\n";

    runTest("regex_split by whitespace", []()
            {
        auto out = runXell("print(regex_split(\"hello world foo\", \"\\\\s+\"))");
        XASSERT_EQ(out[0], "[\"hello\", \"world\", \"foo\"]"); });

    runTest("regex_split by comma-space", []()
            {
        auto out = runXell("print(regex_split(\"a, b, c\", \",\\\\s*\"))");
        XASSERT_EQ(out[0], "[\"a\", \"b\", \"c\"]"); });

    runTest("regex_split by digits", []()
            {
        auto out = runXell("print(regex_split(\"abc123def456ghi\", \"[0-9]+\"))");
        XASSERT_EQ(out[0], "[\"abc\", \"def\", \"ghi\"]"); });

    runTest("regex_split no match returns whole string", []()
            {
        auto out = runXell("print(regex_split(\"hello\", \"[0-9]+\"))");
        XASSERT_EQ(out[0], "[\"hello\"]"); });

    runTest("regex_split type error", []()
            { XASSERT(expectError<std::exception>("regex_split(123, \"a\")")); });
}

// ============================================================================
// Section 11: Regex — regex_groups
// ============================================================================

static void testRegexGroups()
{
    std::cout << "\n[Regex — regex_groups]\n";

    runTest("regex_groups date captures", []()
            {
        auto out = runXell(
            "print(regex_groups(\"2025-06-15\", \"([0-9]+)-([0-9]+)-([0-9]+)\"))");
        XASSERT_EQ(out[0], "[\"2025\", \"06\", \"15\"]"); });

    runTest("regex_groups single group", []()
            {
        auto out = runXell(
            "print(regex_groups(\"hello123\", \"([0-9]+)\"))");
        XASSERT_EQ(out[0], "[\"123\"]"); });

    runTest("regex_groups no groups returns empty", []()
            {
        auto out = runXell("print(regex_groups(\"hello\", \"[a-z]+\"))");
        XASSERT_EQ(out[0], "[]"); });

    runTest("regex_groups no match returns empty", []()
            {
        auto out = runXell(
            "print(regex_groups(\"hello\", \"([0-9]+)\"))");
        XASSERT_EQ(out[0], "[]"); });

    runTest("regex_groups multiple groups", []()
            {
        auto out = runXell(
            "print(regex_groups(\"John Doe 30\", \"([A-Za-z]+) ([A-Za-z]+) ([0-9]+)\"))");
        XASSERT_EQ(out[0], "[\"John\", \"Doe\", \"30\"]"); });

    runTest("regex_groups type error", []()
            { XASSERT(expectError<std::exception>("regex_groups(123, \"abc\")")); });

    runTest("regex_groups arity error", []()
            { XASSERT(expectError<std::exception>("regex_groups(\"abc\")")); });
}

// ============================================================================
// Section 12: Integration — DateTime + Regex combined
// ============================================================================

static void testIntegration()
{
    std::cout << "\n[Integration — DateTime + Regex]\n";

    runTest("format then parse round-trip", []()
            {
        auto out = runXell(
            "d = {\"year\": 2025, \"month\": 3, \"day\": 20, \"hour\": 14, \"minute\": 30, \"second\": 0}\n"
            "s = format_date(d, \"%Y-%m-%d %H:%M:%S\")\n"
            "d2 = parse_date(s, \"%Y-%m-%d %H:%M:%S\")\n"
            "print(d2[\"year\"])\n"
            "print(d2[\"month\"])\n"
            "print(d2[\"day\"])\n"
            "print(d2[\"hour\"])\n"
            "print(d2[\"minute\"])");
        XASSERT_EQ(out[0], "2025");
        XASSERT_EQ(out[1], "3");
        XASSERT_EQ(out[2], "20");
        XASSERT_EQ(out[3], "14");
        XASSERT_EQ(out[4], "30"); });

    runTest("regex on formatted date", []()
            {
        auto out = runXell(
            "d = {\"year\": 2025, \"month\": 12, \"day\": 25}\n"
            "s = format_date(d, \"%Y-%m-%d\")\n"
            "groups = regex_groups(s, \"([0-9]+)-([0-9]+)-([0-9]+)\")\n"
            "print(groups)");
        XASSERT_EQ(out[0], "[\"2025\", \"12\", \"25\"]"); });

    runTest("timestamp consistency check", []()
            {
        auto out = runXell(
            "ts1 = timestamp()\n"
            "ts2 = timestamp()\n"
            "print(ts2 >= ts1)");
        XASSERT_EQ(out[0], "true"); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===== DateTime & Regex Builtin Tests =====\n";

    // DateTime
    testDateTimeNow();
    testDateTimeTimestamp();
    testDateTimeFormat();
    testDateTimeParse();
    testDateTimeSleep();
    testDateTimeTimeSince();

    // Regex
    testRegexMatch();
    testRegexFind();
    testRegexReplace();
    testRegexSplit();
    testRegexGroups();

    // Integration
    testIntegration();

    std::cout << "\n===== Results: " << g_passed << " passed, "
              << g_failed << " failed =====\n";
    return g_failed > 0 ? 1 : 0;
}
