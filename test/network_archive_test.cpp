// =============================================================================
// Network & Archive Tests
// Tests for builtins_network.hpp and builtins_archive.hpp
// Network tests use only safe local operations (dns_lookup localhost, local_ip)
// Archive tests create temp files and compress/decompress them
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
    interp.loadModule("net");
    interp.loadModule("archive");
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
    g_testDir = fs::temp_directory_path().string() + "/xell_netarch_test_" +
                std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    fs::create_directories(g_testDir);
}

static void cleanupTestDir()
{
    if (!g_testDir.empty() && fs::exists(g_testDir))
        fs::remove_all(g_testDir);
}

static void createFile(const std::string &name, const std::string &content)
{
    std::ofstream ofs(g_testDir + "/" + name);
    ofs << content;
}

// ============================================================================
// Section 1: Network — DNS (native, no external deps)
// ============================================================================

static void testDNS()
{
    std::cout << "\n[DNS]\n";

    runTest("dns_lookup localhost", []()
            {
        auto out = runXell("r = dns_lookup(\"localhost\")\nprint(type(r))");
        XASSERT_EQ(out[0], "list"); });

    runTest("dns_lookup localhost returns IPs", []()
            {
        auto out = runXell("r = dns_lookup(\"localhost\")\nprint(size(r) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("dns_lookup localhost contains 127.0.0.1 or ::1", []()
            {
        auto out = runXell(
            "r = dns_lookup(\"localhost\")\n"
            "found = false\n"
            "for ip in r:\n"
            "  if ip == \"127.0.0.1\" or ip == \"::1\":\n"
            "    found = true\n"
            "  ;\n"
            ";\n"
            "print(found)");
        XASSERT_EQ(out[0], "true"); });

    runTest("dns_lookup invalid throws", []()
            { XASSERT(expectError<std::exception>("dns_lookup(\"this.host.does.not.exist.xell\")")); });

    runTest("dns_lookup arity error", []()
            { XASSERT(expectError<std::exception>("dns_lookup()")); });

    runTest("dns_lookup type error", []()
            { XASSERT(expectError<std::exception>("dns_lookup(123)")); });
}

// ============================================================================
// Section 2: Network — local_ip
// ============================================================================

static void testLocalIP()
{
    std::cout << "\n[local_ip]\n";

    runTest("local_ip returns string", []()
            {
        auto out = runXell("print(type(local_ip()))");
        XASSERT_EQ(out[0], "string"); });

    runTest("local_ip non-empty", []()
            {
        auto out = runXell("print(size(local_ip()) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("local_ip contains dots (IPv4)", []()
            {
        auto out = runXell("print(contains(local_ip(), \".\"))");
        XASSERT_EQ(out[0], "true"); });
}

// ============================================================================
// Section 3: Network — nc (connect test to localhost)
// ============================================================================

static void testNC()
{
    std::cout << "\n[nc]\n";

    runTest("nc returns map", []()
            {
        // Port 1 is usually closed/refused but nc won't throw
        auto out = runXell("r = nc(\"127.0.0.1\", 1)\nprint(type(r))");
        XASSERT_EQ(out[0], "map"); });

    runTest("nc has connected key", []()
            {
        auto out = runXell("r = nc(\"127.0.0.1\", 1)\nprint(has(r, \"connected\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("nc has host key", []()
            {
        auto out = runXell("r = nc(\"127.0.0.1\", 1)\nprint(has(r, \"host\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("nc has port key", []()
            {
        auto out = runXell("r = nc(\"127.0.0.1\", 1)\nprint(has(r, \"port\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("nc arity error", []()
            { XASSERT(expectError<std::exception>("nc(\"localhost\")")); });

    runTest("nc type error", []()
            { XASSERT(expectError<std::exception>("nc(123, 80)")); });
}

// ============================================================================
// Section 4: Network — ping (limited)
// ============================================================================

static void testPing()
{
    std::cout << "\n[ping]\n";

    runTest("ping localhost returns string", []()
            {
        auto out = runXell("r = ping(\"127.0.0.1\", 1)\nprint(type(r))");
        XASSERT_EQ(out[0], "string"); });

    runTest("ping localhost returns non-empty", []()
            {
        auto out = runXell("r = ping(\"127.0.0.1\", 1)\nprint(size(r) > 0)");
        XASSERT_EQ(out[0], "true"); });

    runTest("ping arity error", []()
            { XASSERT(expectError<std::exception>("ping()")); });

    runTest("ping type error", []()
            { XASSERT(expectError<std::exception>("ping(123)")); });
}

// ============================================================================
// Section 5: Network — nslookup, host_lookup, telnet_connect
// ============================================================================

static void testNetworkMisc()
{
    std::cout << "\n[Network misc]\n";

    runTest("nslookup returns string", []()
            {
        auto out = runXell("print(type(nslookup(\"localhost\")))");
        XASSERT_EQ(out[0], "string"); });

    runTest("host_lookup returns string", []()
            {
        auto out = runXell("print(type(host_lookup(\"localhost\")))");
        XASSERT_EQ(out[0], "string"); });

    runTest("telnet_connect returns bool", []()
            {
        auto out = runXell("print(type(telnet_connect(\"127.0.0.1\", 1)))");
        XASSERT_EQ(out[0], "bool"); });

    runTest("netstat returns string", []()
            {
        auto out = runXell("print(type(netstat()))");
        XASSERT_EQ(out[0], "string"); });

    runTest("ss returns string", []()
            {
        auto out = runXell("print(type(ss()))");
        XASSERT_EQ(out[0], "string"); });

    runTest("ifconfig returns string", []()
            {
        auto out = runXell("print(type(ifconfig()))");
        XASSERT_EQ(out[0], "string"); });

    runTest("ip_cmd addr returns string", []()
            {
        auto out = runXell("print(type(ip_cmd(\"addr\")))");
        XASSERT_EQ(out[0], "string"); });

    runTest("route returns string", []()
            {
        auto out = runXell("print(type(route()))");
        XASSERT_EQ(out[0], "string"); });
}

// ============================================================================
// Section 6: Network — HTTP (requires curl)
// ============================================================================

static void testHTTP()
{
    std::cout << "\n[HTTP]\n";

    runTest("http_get returns map", []()
            {
        auto out = runXell("r = http_get(\"http://localhost:1\")\nprint(type(r))");
        XASSERT_EQ(out[0], "map"); });

    runTest("http_get result has status key", []()
            {
        auto out = runXell("r = http_get(\"http://localhost:1\")\nprint(has(r, \"status\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("http_get result has body key", []()
            {
        auto out = runXell("r = http_get(\"http://localhost:1\")\nprint(has(r, \"body\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("http_get arity error", []()
            { XASSERT(expectError<std::exception>("http_get()")); });

    runTest("http_get type error", []()
            { XASSERT(expectError<std::exception>("http_get(123)")); });

    runTest("http_post returns map", []()
            {
        auto out = runXell("r = http_post(\"http://localhost:1\", \"{}\")\nprint(type(r))");
        XASSERT_EQ(out[0], "map"); });

    runTest("http_post arity error", []()
            { XASSERT(expectError<std::exception>("http_post(\"url\")")); });

    runTest("http_put returns map", []()
            {
        auto out = runXell("r = http_put(\"http://localhost:1\", \"{}\")\nprint(type(r))");
        XASSERT_EQ(out[0], "map"); });

    runTest("http_delete returns map", []()
            {
        auto out = runXell("r = http_delete(\"http://localhost:1\")\nprint(type(r))");
        XASSERT_EQ(out[0], "map"); });
}

// ============================================================================
// Section 7: Network — error cases
// ============================================================================

static void testNetworkErrors()
{
    std::cout << "\n[Network errors]\n";

    runTest("download arity error", []()
            { XASSERT(expectError<std::exception>("download(\"url\")")); });

    runTest("download type error", []()
            { XASSERT(expectError<std::exception>("download(123, \"file\")")); });

    runTest("nslookup type error", []()
            { XASSERT(expectError<std::exception>("nslookup(123)")); });

    runTest("whois type error", []()
            { XASSERT(expectError<std::exception>("whois(123)")); });

    runTest("traceroute arity error", []()
            { XASSERT(expectError<std::exception>("traceroute()")); });

    runTest("rsync arity error", []()
            { XASSERT(expectError<std::exception>("rsync(\"src\")")); });

    runTest("iptables type error", []()
            { XASSERT(expectError<std::exception>("iptables(123)")); });

    runTest("ufw type error", []()
            { XASSERT(expectError<std::exception>("ufw(123)")); });
}

// ============================================================================
// Section 8: Archive — zip/unzip
// ============================================================================

static void testZipUnzip()
{
    std::cout << "\n[zip/unzip]\n";
    createFile("ziptest.txt", "hello zip world");

    runTest("zip_archive creates archive", [&]()
            {
        auto out = runXell("print(zip_archive(\"" + g_testDir + "/ziptest.txt\", \"" + g_testDir + "/test.zip\"))");
        XASSERT_EQ(out[0], "true");
        XASSERT(fs::exists(g_testDir + "/test.zip")); });

    runTest("unzip_archive extracts", [&]()
            {
        std::string extractDir = g_testDir + "/zip_extract";
        auto out = runXell("print(unzip_archive(\"" + g_testDir + "/test.zip\", \"" + extractDir + "\"))");
        XASSERT_EQ(out[0], "true");
        XASSERT(fs::exists(extractDir + "/ziptest.txt")); });

    runTest("unzip_archive extracted content matches", [&]()
            {
        auto out = runXell("print(cat(\"" + g_testDir + "/zip_extract/ziptest.txt\"))");
        XASSERT_EQ(out[0], "hello zip world"); });

    runTest("zip_archive nonexistent throws", [&]()
            { XASSERT(expectError<std::exception>("zip_archive(\"" + g_testDir + "/nofile\", \"" + g_testDir + "/out.zip\")")); });

    runTest("unzip_archive nonexistent throws", [&]()
            { XASSERT(expectError<std::exception>("unzip_archive(\"" + g_testDir + "/nofile.zip\")")); });

    runTest("zip_archive arity error", []()
            { XASSERT(expectError<std::exception>("zip_archive(\"src\")")); });

    runTest("zip_archive type error", []()
            { XASSERT(expectError<std::exception>("zip_archive(123, \"out\")")); });
}

// ============================================================================
// Section 9: Archive — tar create/extract
// ============================================================================

static void testTar()
{
    std::cout << "\n[tar]\n";
    createFile("tartest.txt", "hello tar world");

    runTest("tar_create .tar.gz", [&]()
            {
        auto out = runXell("print(tar_create(\"" + g_testDir + "/tartest.txt\", \"" + g_testDir + "/test.tar.gz\"))");
        XASSERT_EQ(out[0], "true");
        XASSERT(fs::exists(g_testDir + "/test.tar.gz")); });

    runTest("tar_extract .tar.gz", [&]()
            {
        std::string extractDir = g_testDir + "/tar_extract";
        auto out = runXell("print(tar_extract(\"" + g_testDir + "/test.tar.gz\", \"" + extractDir + "\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("tar_create nonexistent throws", [&]()
            { XASSERT(expectError<std::exception>("tar_create(\"" + g_testDir + "/nofile\", \"" + g_testDir + "/out.tar\")")); });

    runTest("tar_extract nonexistent throws", [&]()
            { XASSERT(expectError<std::exception>("tar_extract(\"" + g_testDir + "/nofile.tar\")")); });

    runTest("tar_create arity error", []()
            { XASSERT(expectError<std::exception>("tar_create(\"src\")")); });

    runTest("tar_create type error", []()
            { XASSERT(expectError<std::exception>("tar_create(123, \"out\")")); });
}

// ============================================================================
// Section 10: Archive — gzip
// ============================================================================

static void testGzip()
{
    std::cout << "\n[gzip]\n";
    createFile("gztest.txt", "hello gzip world");

    runTest("gzip_compress creates .gz", [&]()
            {
        auto out = runXell("print(gzip_compress(\"" + g_testDir + "/gztest.txt\"))");
        XASSERT_EQ(out[0], g_testDir + "/gztest.txt.gz");
        XASSERT(fs::exists(g_testDir + "/gztest.txt.gz")); });

    runTest("gunzip_decompress restores", [&]()
            {
        auto out = runXell("print(gunzip_decompress(\"" + g_testDir + "/gztest.txt.gz\"))");
        XASSERT_EQ(out[0], g_testDir + "/gztest.txt");
        XASSERT(fs::exists(g_testDir + "/gztest.txt")); });

    runTest("gunzip_decompress content matches", [&]()
            {
        auto out = runXell("print(cat(\"" + g_testDir + "/gztest.txt\"))");
        XASSERT_EQ(out[0], "hello gzip world"); });

    runTest("gzip_compress nonexistent throws", [&]()
            { XASSERT(expectError<std::exception>("gzip_compress(\"" + g_testDir + "/nofile\")")); });

    runTest("gzip_compress type error", []()
            { XASSERT(expectError<std::exception>("gzip_compress(123)")); });

    runTest("gunzip_decompress nonexistent throws", [&]()
            { XASSERT(expectError<std::exception>("gunzip_decompress(\"" + g_testDir + "/nofile\")")); });
}

// ============================================================================
// Section 11: Archive — bzip2
// ============================================================================

static void testBzip2()
{
    std::cout << "\n[bzip2]\n";
    createFile("bz2test.txt", "hello bzip2 world");

    runTest("bzip2_compress creates .bz2", [&]()
            {
        auto out = runXell("print(bzip2_compress(\"" + g_testDir + "/bz2test.txt\"))");
        XASSERT_EQ(out[0], g_testDir + "/bz2test.txt.bz2");
        XASSERT(fs::exists(g_testDir + "/bz2test.txt.bz2")); });

    runTest("bunzip2_decompress restores", [&]()
            {
        auto out = runXell("print(bunzip2_decompress(\"" + g_testDir + "/bz2test.txt.bz2\"))");
        XASSERT_EQ(out[0], g_testDir + "/bz2test.txt");
        XASSERT(fs::exists(g_testDir + "/bz2test.txt")); });

    runTest("bunzip2_decompress content matches", [&]()
            {
        auto out = runXell("print(cat(\"" + g_testDir + "/bz2test.txt\"))");
        XASSERT_EQ(out[0], "hello bzip2 world"); });

    runTest("bzip2_compress nonexistent throws", [&]()
            { XASSERT(expectError<std::exception>("bzip2_compress(\"" + g_testDir + "/nofile\")")); });

    runTest("bzip2_compress type error", []()
            { XASSERT(expectError<std::exception>("bzip2_compress(123)")); });
}

// ============================================================================
// Section 12: Archive — xz
// ============================================================================

static void testXz()
{
    std::cout << "\n[xz]\n";
    createFile("xztest.txt", "hello xz world");

    runTest("xz_compress creates .xz", [&]()
            {
        auto out = runXell("print(xz_compress(\"" + g_testDir + "/xztest.txt\"))");
        XASSERT_EQ(out[0], g_testDir + "/xztest.txt.xz");
        XASSERT(fs::exists(g_testDir + "/xztest.txt.xz")); });

    runTest("xz_decompress restores", [&]()
            {
        auto out = runXell("print(xz_decompress(\"" + g_testDir + "/xztest.txt.xz\"))");
        XASSERT_EQ(out[0], g_testDir + "/xztest.txt");
        XASSERT(fs::exists(g_testDir + "/xztest.txt")); });

    runTest("xz_decompress content matches", [&]()
            {
        auto out = runXell("print(cat(\"" + g_testDir + "/xztest.txt\"))");
        XASSERT_EQ(out[0], "hello xz world"); });

    runTest("xz_compress nonexistent throws", [&]()
            { XASSERT(expectError<std::exception>("xz_compress(\"" + g_testDir + "/nofile\")")); });

    runTest("xz_compress type error", []()
            { XASSERT(expectError<std::exception>("xz_compress(123)")); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===== Network & Archive Builtin Tests =====\n";

    setupTestDir();

    // Network
    testDNS();
    testLocalIP();
    testNC();
    testPing();
    testNetworkMisc();
    testHTTP();
    testNetworkErrors();

    // Archive
    testZipUnzip();
    testTar();
    testGzip();
    testBzip2();
    testXz();

    cleanupTestDir();

    std::cout << "\n===== Results: " << g_passed << " passed, "
              << g_failed << " failed =====\n";
    return g_failed > 0 ? 1 : 0;
}
