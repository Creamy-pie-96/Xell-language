// ============================================================================
// Xell Phase 10 OOP Tests — Class Decorators (@dataclass, @immutable, @singleton)
// ============================================================================
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

// ---- Minimal test harness (same pattern as other OOP test files) ----

static int g_passed = 0;
static int g_failed = 0;

#define XASSERT_EQ(a, b)                                                          \
    do                                                                             \
    {                                                                              \
        auto _a = (a);                                                             \
        auto _b = (b);                                                             \
        if (_a != _b)                                                              \
        {                                                                          \
            std::ostringstream os;                                                 \
            os << "  ASSERTION FAILED: " << #a << " == " << #b << "\n"             \
               << "        got: [" << _a << "] vs [" << _b << "]";                 \
            throw std::runtime_error(os.str());                                    \
        }                                                                          \
    } while (0)

#define XASSERT_CONTAINS(haystack, needle)                                         \
    do                                                                             \
    {                                                                              \
        if ((haystack).find(needle) == std::string::npos)                          \
        {                                                                          \
            std::ostringstream os;                                                 \
            os << "  ASSERTION FAILED: output should contain \"" << (needle)       \
               << "\"\n        got: [" << (haystack) << "]";                       \
            throw std::runtime_error(os.str());                                    \
        }                                                                          \
    } while (0)

#define XASSERT_THROWS(expr)                                                       \
    do                                                                             \
    {                                                                              \
        bool caught = false;                                                       \
        try { expr; } catch (...) { caught = true; }                               \
        if (!caught) throw std::runtime_error("  Expected exception but none thrown"); \
    } while (0)

// ---- Include the interpreter directly (single-header style) ----
#include "../src/lexer/lexer.cpp"
#include "../src/parser/parser.cpp"
#include "../src/interpreter/xobject.cpp"
#include "../src/interpreter/interpreter.cpp"

using namespace xell;

static std::vector<std::string> runXell(const std::string &code)
{
    Lexer lexer(code);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();
    Interpreter interp;
    interp.run(program);
    return interp.output();
}

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

// ============================================================================
// @dataclass tests
// ============================================================================

void testDataclass()
{
    std::cout << "\n===== @dataclass Decorator =====\n";

    runTest("@dataclass basic positional construction", []()
            {
        auto out = runXell(R"XEL(
@dataclass
class Point :
    x = 0
    y = 0
;

p = Point(3, 4)
print(p->x)
print(p->y)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "4"); });

    runTest("@dataclass named argument construction", []()
            {
        auto out = runXell(R"XEL(
@dataclass
class Point :
    x = 0
    y = 0
;

p = Point(y: 10, x: 5)
print(p->x)
print(p->y)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "5");
        XASSERT_EQ(out[1], "10"); });

    runTest("@dataclass default values", []()
            {
        auto out = runXell(R"XEL(
@dataclass
class Config :
    host = "localhost"
    port = 8080
;

c = Config()
print(c->host)
print(c->port)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "localhost");
        XASSERT_EQ(out[1], "8080"); });

    runTest("@dataclass equality comparison", []()
            {
        auto out = runXell(R"XEL(
@dataclass
class Point :
    x = 0
    y = 0
;

p1 = Point(1, 2)
p2 = Point(1, 2)
p3 = Point(3, 4)
print(p1 == p2)
print(p1 == p3)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "true");
        XASSERT_EQ(out[1], "false"); });

    runTest("@dataclass print representation", []()
            {
        auto out = runXell(R"XEL(
@dataclass
class Point :
    x = 0
    y = 0
;

p = Point(3, 4)
print(p)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_CONTAINS(out[0], "Point");
        XASSERT_CONTAINS(out[0], "x=3");
        XASSERT_CONTAINS(out[0], "y=4"); });

    runTest("@dataclass with methods", []()
            {
        auto out = runXell(R"XEL(
@dataclass
class Point :
    x = 0
    y = 0

    fn magnitude(self) :
        give (self->x * self->x + self->y * self->y)
    ;
;

p = Point(3, 4)
print(p->magnitude())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "25"); });
}

// ============================================================================
// @immutable tests
// ============================================================================

void testImmutable()
{
    std::cout << "\n===== @immutable Decorator =====\n";

    runTest("@immutable prevents field modification after construction", []()
            {
        XASSERT_THROWS(runXell(R"XEL(
@immutable
class Config :
    host = ""
    port = 0

    fn __init__(self, host, port) :
        self->host = host
        self->port = port
    ;
;

cfg = Config("localhost", 8080)
cfg->host = "other"
)XEL")); });

    runTest("@immutable allows reading fields", []()
            {
        auto out = runXell(R"XEL(
@immutable
class Config :
    host = ""
    port = 0

    fn __init__(self, host, port) :
        self->host = host
        self->port = port
    ;
;

cfg = Config("localhost", 8080)
print(cfg->host)
print(cfg->port)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "localhost");
        XASSERT_EQ(out[1], "8080"); });

    runTest("@immutable without __init__ (field defaults)", []()
            {
        XASSERT_THROWS(runXell(R"XEL(
@immutable
class Frozen :
    x = 42
    y = 99
;

f = Frozen()
f->x = 10
)XEL")); });

    runTest("@immutable allows __init__ to set fields", []()
            {
        auto out = runXell(R"XEL(
@immutable
class Point :
    x = 0
    y = 0

    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;
;

p = Point(5, 10)
print(p->x)
print(p->y)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "5");
        XASSERT_EQ(out[1], "10"); });

    runTest("@immutable with methods (read-only methods work)", []()
            {
        auto out = runXell(R"XEL(
@immutable
class Config :
    host = ""
    port = 0

    fn __init__(self, host, port) :
        self->host = host
        self->port = port
    ;

    fn url(self) :
        give self->host
    ;
;

cfg = Config("localhost", 8080)
print(cfg->url())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "localhost"); });
}

// ============================================================================
// @singleton tests
// ============================================================================

void testSingleton()
{
    std::cout << "\n===== @singleton Decorator =====\n";

    runTest("@singleton returns same instance", []()
            {
        auto out = runXell(R"XEL(
@singleton
class Database :
    connection = "none"

    fn __init__(self) :
        self->connection = "none"
    ;
;

db1 = Database()
db2 = Database()
db1->connection = "postgres"
print(db2->connection)
print(db1 == db2)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "postgres");
        XASSERT_EQ(out[1], "true"); });

    runTest("@singleton without __init__", []()
            {
        auto out = runXell(R"XEL(
@singleton
class Registry :
    items = 0
;

r1 = Registry()
r2 = Registry()
r1->items = 42
print(r2->items)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "42"); });

    runTest("@singleton preserves state across calls", []()
            {
        auto out = runXell(R"XEL(
@singleton
class Counter :
    count = 0

    fn __init__(self) :
        self->count = 0
    ;

    fn increment(self) :
        self->count = self->count + 1
    ;
;

c1 = Counter()
c1->increment()
c1->increment()
c2 = Counter()
print(c2->count)
c2->increment()
print(c1->count)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "2");
        XASSERT_EQ(out[1], "3"); });
}

// ============================================================================
// Decorator combination tests
// ============================================================================

void testDecoratorCombinations()
{
    std::cout << "\n===== Decorator Combinations =====\n";

    runTest("@dataclass + @immutable", []()
            {
        XASSERT_THROWS(runXell(R"XEL(
@immutable
@dataclass
class FrozenPoint :
    x = 0
    y = 0
;

p = FrozenPoint(3, 4)
p->x = 10
)XEL")); });

    runTest("@dataclass + @immutable allows reading", []()
            {
        auto out = runXell(R"XEL(
@immutable
@dataclass
class FrozenPoint :
    x = 0
    y = 0
;

p = FrozenPoint(3, 4)
print(p->x)
print(p->y)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "4"); });

    runTest("@dataclass + @singleton", []()
            {
        auto out = runXell(R"XEL(
@singleton
@dataclass
class AppConfig :
    debug = false
    port = 3000
;

c1 = AppConfig()
c2 = AppConfig()
c1->debug = true
print(c2->debug)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "true"); });

    runTest("Custom class decorator", []()
            {
        auto out = runXell(R"XEL(
fn my_decorator(cls) :
    give cls
;

@my_decorator
class Foo :
    x = 10
;

f = Foo()
print(f->x)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "10"); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===== DECORATOR TESTS (Phase 10 OOP) =====\n";

    testDataclass();
    testImmutable();
    testSingleton();
    testDecoratorCombinations();

    std::cout << "\n========================================\n";
    std::cout << "Total: " << (g_passed + g_failed)
              << "  Passed: " << g_passed
              << "  Failed: " << g_failed << "\n";
    return g_failed > 0 ? 1 : 0;
}
