// =============================================================================
// Static Members Tests
// =============================================================================
// Tests Phase 5 OOP: static fields and static methods on classes.
// Static members are stored on the class definition, not on instances.
// Access via ClassName->member. Static methods have no `self` parameter.
// =============================================================================

#include "../src/interpreter/interpreter.hpp"
#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include <iostream>
#include <sstream>
#include <functional>
#include <string>
#include <vector>

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
}

// ============================================================================
// Section 1: Static Fields
// ============================================================================

static void testStaticFields()
{
    std::cout << "\n===== Static Fields =====\n";

    runTest("Static field accessible via class name", []()
            {
        auto out = runXell(R"XEL(
class MathHelper :
    static PI = 3.14159
;
print(MathHelper->PI)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "3.14159"); });

    runTest("Multiple static fields", []()
            {
        auto out = runXell(R"XEL(
class Config :
    static VERSION = "1.0"
    static DEBUG = true
    static MAX_RETRIES = 3
;
print(Config->VERSION)
print(Config->DEBUG)
print(Config->MAX_RETRIES)
)XEL");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], "1.0");
        XASSERT_EQ(out[1], "true");
        XASSERT_EQ(out[2], "3"); });

    runTest("Static field assignment modifies class-level value", []()
            {
        auto out = runXell(R"XEL(
class Counter :
    static count = 0
;
Counter->count = 10
print(Counter->count)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "10"); });

    runTest("Static fields are separate from instance fields", []()
            {
        auto out = runXell(R"XEL(
class Foo :
    x = 1
    static y = 2
;
f = Foo()
print(f->x)
print(Foo->y)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "1");
        XASSERT_EQ(out[1], "2"); });
}

// ============================================================================
// Section 2: Static Methods
// ============================================================================

static void testStaticMethods()
{
    std::cout << "\n===== Static Methods =====\n";

    runTest("Static method callable via class name", []()
            {
        auto out = runXell(R"XEL(
class MathHelper :
    static fn square(x) :
        give x * x
    ;
;
print(MathHelper->square(5))
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "25"); });

    runTest("Static method with no args", []()
            {
        auto out = runXell(R"XEL(
class Greeter :
    static fn hello() :
        give "hello world"
    ;
;
print(Greeter->hello())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "hello world"); });

    runTest("Static method accessing static field via class name", []()
            {
        auto out = runXell(R"XEL(
class MathHelper :
    static PI = 3.14159
    static fn circle_area(r) :
        give MathHelper->PI * r * r
    ;
;
print(MathHelper->circle_area(5))
)XEL");
        XASSERT_EQ(out.size(), 1u);
        // 3.14159 * 25 = 78.53975 (may be display-truncated)
        XASSERT(out[0].find("78.5397") == 0); });

    runTest("Multiple static methods", []()
            {
        auto out = runXell(R"XEL(
class Math :
    static fn add(a, b) :
        give a + b
    ;
    static fn mul(a, b) :
        give a * b
    ;
;
print(Math->add(3, 4))
print(Math->mul(3, 4))
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "7");
        XASSERT_EQ(out[1], "12"); });
}

// ============================================================================
// Section 3: Static with Instance Members
// ============================================================================

static void testStaticWithInstance()
{
    std::cout << "\n===== Static with Instance Members =====\n";

    runTest("Class with both static and instance members", []()
            {
        auto out = runXell(R"XEL(
class Dog :
    static species = "Canis lupus familiaris"
    name = "unknown"

    fn __init__(self, n) :
        self->name = n
    ;

    fn bark(self) :
        give self->name + " says woof"
    ;

    static fn info() :
        give "Dogs are " + Dog->species
    ;
;
d = Dog("Rex")
print(d->bark())
print(Dog->species)
print(Dog->info())
)XEL");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], "Rex says woof");
        XASSERT_EQ(out[1], "Canis lupus familiaris");
        XASSERT_EQ(out[2], "Dogs are Canis lupus familiaris"); });

    runTest("Static field not accessible on instances (via instance fields)", []()
            {
        // Static fields exist on the class, not instances
        // Accessing via instance should fail (no such instance field)
        XASSERT(expectError<AttributeError>(R"XEL(
class Foo :
    static x = 42
;
f = Foo()
print(f->x)
)XEL")); });
}

// ============================================================================
// Section 4: Static with Access Control
// ============================================================================

static void testStaticWithAccessControl()
{
    std::cout << "\n===== Static with Access Control =====\n";

    runTest("Static under private: accessible via class methods", []()
            {
        auto out = runXell(R"XEL(
class Secret :
    private:
        static key = "abc123"

    public:
        fn reveal(self) :
            give Secret->key
        ;
;
s = Secret()
print(s->reveal())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "abc123"); });

    runTest("Static fields with default (public) access work from outside", []()
            {
        auto out = runXell(R"XEL(
class Pub :
    static val = 99
;
print(Pub->val)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "99"); });
}

// ============================================================================
// Section 5: Static Inheritance
// ============================================================================

static void testStaticInheritance()
{
    std::cout << "\n===== Static Inheritance =====\n";

    runTest("Static field inherited from parent", []()
            {
        auto out = runXell(R"XEL(
class Base :
    static version = 1
;
class Child inherits Base :
;
print(Child->version)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "1"); });

    runTest("Static method inherited from parent", []()
            {
        auto out = runXell(R"XEL(
class Base :
    static fn greet() :
        give "hello from base"
    ;
;
class Child inherits Base :
;
print(Child->greet())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "hello from base"); });
}

// ============================================================================
// Section 6: Edge Cases
// ============================================================================

static void testStaticEdgeCases()
{
    std::cout << "\n===== Static Edge Cases =====\n";

    runTest("Static method with multiple args and defaults", []()
            {
        auto out = runXell(R"XEL(
class Utils :
    static fn clamp(val, lo = 0, hi = 100) :
        if val < lo :
            give lo
        ;
        if val > hi :
            give hi
        ;
        give val
    ;
;
print(Utils->clamp(150))
print(Utils->clamp(-5))
print(Utils->clamp(50))
)XEL");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], "100");
        XASSERT_EQ(out[1], "0");
        XASSERT_EQ(out[2], "50"); });

    runTest("Class with only static members, no instance", []()
            {
        auto out = runXell(R"XEL(
class Constants :
    static PI = 3.14
    static E = 2.71
    static fn sum() :
        give Constants->PI + Constants->E
    ;
;
print(Constants->sum())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "5.85"); });
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout << "===== STATIC MEMBERS TESTS (Phase 5 OOP) =====\n";

    testStaticFields();
    testStaticMethods();
    testStaticWithInstance();
    testStaticWithAccessControl();
    testStaticInheritance();
    testStaticEdgeCases();

    std::cout << "\n========================================\n";
    std::cout << "Total: " << (g_passed + g_failed)
              << "  Passed: " << g_passed
              << "  Failed: " << g_failed << "\n";

    return g_failed == 0 ? 0 : 1;
}
