// ============================================================================
// Xell — Keyword Arguments & Default Parameters Tests
// ============================================================================
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <functional>

static int g_passed = 0;
static int g_failed = 0;

#define XASSERT_EQ(a, b)                                               \
    do                                                                 \
    {                                                                  \
        auto _a = (a);                                                 \
        auto _b = (b);                                                 \
        if (_a != _b)                                                  \
        {                                                              \
            std::ostringstream os;                                     \
            os << "  ASSERTION FAILED: " << #a << " == " << #b << "\n" \
               << "        got: [" << _a << "] vs [" << _b << "]";     \
            throw std::runtime_error(os.str());                        \
        }                                                              \
    } while (0)

#define XASSERT_THROWS(expr)                                                  \
    do                                                                        \
    {                                                                         \
        bool caught = false;                                                  \
        try                                                                   \
        {                                                                     \
            expr;                                                             \
        }                                                                     \
        catch (...)                                                           \
        {                                                                     \
            caught = true;                                                    \
        }                                                                     \
        if (!caught)                                                          \
            throw std::runtime_error("  Expected exception but none thrown"); \
    } while (0)

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
// Default Parameters
// ============================================================================

void testDefaults()
{
    std::cout << "\n===== Default Parameters =====\n";

    runTest("default param: basic", []()
            {
        auto out = runXell(R"XEL(
fn greet(name, greeting="Hello") :
    print("{greeting}, {name}!")
;
greet("Alice")
greet("Bob", "Hi")
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "Hello, Alice!");
        XASSERT_EQ(out[1], "Hi, Bob!"); });

    runTest("default param: multiple defaults", []()
            {
        auto out = runXell(R"XEL(
fn config(host, port=8080, debug=false) :
    print("{host} {port} {debug}")
;
config("0.0.0.0")
config("0.0.0.0", 9090)
config("0.0.0.0", 9090, true)
)XEL");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], "0.0.0.0 8080 false");
        XASSERT_EQ(out[1], "0.0.0.0 9090 false");
        XASSERT_EQ(out[2], "0.0.0.0 9090 true"); });
}

// ============================================================================
// Keyword Arguments — basic
// ============================================================================

void testKeywordArgs()
{
    std::cout << "\n===== Keyword Arguments =====\n";

    runTest("kwargs: reorder args by name", []()
            {
        auto out = runXell(R"XEL(
fn sub(a, b) :
    give a - b
;
print(sub(b: 3, a: 10))
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "7"); });

    runTest("kwargs: mixed positional + keyword", []()
            {
        auto out = runXell(R"XEL(
fn point(x, y, z) :
    print("{x} {y} {z}")
;
point(1, z: 3, y: 2)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "1 2 3"); });

    runTest("kwargs: all keyword", []()
            {
        auto out = runXell(R"XEL(
fn add(a, b) :
    give a + b
;
print(add(a: 10, b: 20))
print(add(b: 100, a: 5))
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "30");
        XASSERT_EQ(out[1], "105"); });

    runTest("kwargs: with defaults", []()
            {
        auto out = runXell(R"XEL(
fn greet(name, greeting="Hello") :
    print("{greeting}, {name}!")
;
greet(greeting: "Hey", name: "Charlie")
greet("Bob", greeting: "Yo")
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "Hey, Charlie!");
        XASSERT_EQ(out[1], "Yo, Bob!"); });

    runTest("kwargs: skip middle param with default", []()
            {
        auto out = runXell(R"XEL(
fn config(host, port=8080, debug=false) :
    print("{host} {port} {debug}")
;
config("0.0.0.0", debug: true)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "0.0.0.0 8080 true"); });
}

// ============================================================================
// Kwargs with class __init__
// ============================================================================

void testKwargsClasses()
{
    std::cout << "\n===== Kwargs with Classes =====\n";

    runTest("kwargs: class __init__", []()
            {
        auto out = runXell(R"XEL(
class Dog :
    name = ""
    age = 0
    fn __init__(self, name, age) :
        self->name = name
        self->age = age
    ;
;
d = Dog(age: 5, name: "Rex")
print(d->name)
print(d->age)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "Rex");
        XASSERT_EQ(out[1], "5"); });

    runTest("kwargs: class __init__ with defaults", []()
            {
        auto out = runXell(R"XEL(
class Server :
    host = ""
    port = 0
    debug = false
    fn __init__(self, host, port=8080, debug=false) :
        self->host = host
        self->port = port
        self->debug = debug
    ;
;
s = Server("0.0.0.0", debug: true)
print(s->host)
print(s->port)
print(s->debug)
)XEL");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], "0.0.0.0");
        XASSERT_EQ(out[1], "8080");
        XASSERT_EQ(out[2], "true"); });
}

// ============================================================================
// Kwargs with lambdas
// ============================================================================

void testKwargsLambda()
{
    std::cout << "\n===== Kwargs with Lambdas =====\n";

    runTest("kwargs: lambda", []()
            {
        auto out = runXell(R"XEL(
f = (a, b) => a * 10 + b
print(f(b: 3, a: 7))
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "73"); });
}

// ============================================================================
// Kwargs with generators
// ============================================================================

void testKwargsGenerator()
{
    std::cout << "\n===== Kwargs with Generators =====\n";

    runTest("kwargs: generator function", []()
            {
        auto out = runXell(R"XEL(
fn gen_range(start, stop, step=1) :
    i = start
    while i < stop :
        yield i
        i = i + step
    ;
;
for x in gen_range(0, 10, step: 2) :
    print(x)
;
)XEL");
        XASSERT_EQ(out.size(), 5u);
        XASSERT_EQ(out[0], "0");
        XASSERT_EQ(out[1], "2");
        XASSERT_EQ(out[2], "4");
        XASSERT_EQ(out[3], "6");
        XASSERT_EQ(out[4], "8"); });
}

// ============================================================================
// Kwargs error cases
// ============================================================================

void testKwargsErrors()
{
    std::cout << "\n===== Kwargs Error Cases =====\n";

    runTest("kwargs: unexpected keyword argument", []()
            { XASSERT_THROWS(runXell(R"XEL(
fn foo(x) :
    give x
;
foo(z: 5)
)XEL")); });

    runTest("kwargs: duplicate argument", []()
            { XASSERT_THROWS(runXell(R"XEL(
fn bar(x) :
    give x
;
bar(1, x: 2)
)XEL")); });

    runTest("kwargs: positional after keyword", []()
            { XASSERT_THROWS(runXell(R"XEL(
fn baz(a, b) :
    give a + b
;
baz(a: 1, 2)
)XEL")); });
}

// ============================================================================
// Struct named args still work
// ============================================================================

void testStructNamedArgs()
{
    std::cout << "\n===== Struct Named Args =====\n";

    runTest("struct: named construction", []()
            {
        auto out = runXell(R"XEL(
struct Color :
    r = 0
    g = 0
    b = 0
;
c = Color(r: 255, g: 128, b: 0)
print(c->r)
print(c->g)
print(c->b)
)XEL");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], "255");
        XASSERT_EQ(out[1], "128");
        XASSERT_EQ(out[2], "0"); });

    runTest("struct: frozen named construction", []()
            {
        auto out = runXell(R"XEL(
struct Point :
    x = 0
    y = 0
;
p = ~Point(x: 3, y: 7)
print(p->x)
print(p->y)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "7"); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===== KWARGS & DEFAULT PARAMS TESTS =====\n";

    testDefaults();
    testKeywordArgs();
    testKwargsClasses();
    testKwargsLambda();
    testKwargsGenerator();
    testKwargsErrors();
    testStructNamedArgs();

    std::cout << "\n========================================\n";
    std::cout << "Total: " << (g_passed + g_failed)
              << "  Passed: " << g_passed
              << "  Failed: " << g_failed << "\n";
    return g_failed > 0 ? 1 : 0;
}
