// =============================================================================
// Interface Tests
// =============================================================================
// Tests Phase 7 OOP: interface definitions, implements keyword, and
// interface contract enforcement.
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
// Section 1: Basic Interface Definition
// ============================================================================

static void testBasicInterface()
{
    std::cout << "\n===== Basic Interface Definition =====\n";

    runTest("Define a simple interface", []()
            {
        // Just defining an interface should not error
        runXell(R"XEL(
interface Drawable :
    fn draw(self) ;
    fn resize(self, factor) ;
;
)XEL"); });

    runTest("Interface with single method", []()
            { runXell(R"XEL(
interface Printable :
    fn to_string(self) ;
;
)XEL"); });

    runTest("Interface cannot be instantiated", []()
            { XASSERT(expectError<TypeError>(R"XEL(
interface Drawable :
    fn draw(self) ;
;
d = Drawable()
)XEL")); });
}

// ============================================================================
// Section 2: Implementing Interfaces
// ============================================================================

static void testImplementsBasic()
{
    std::cout << "\n===== Implementing Interfaces =====\n";

    runTest("Class implements interface with all methods", []()
            {
        auto out = runXell(R"XEL(
interface Drawable :
    fn draw(self) ;
;

class Circle implements Drawable :
    r = 0
    fn __init__(self, r) :
        self->r = r
    ;
    fn draw(self) :
        print("drawing circle r={self->r}")
    ;
;

c = Circle(5)
c->draw()
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "drawing circle r=5"); });

    runTest("Class implements interface with multiple methods", []()
            {
        auto out = runXell(R"XEL(
interface Serializable :
    fn to_json(self) ;
    fn from_json(self, data) ;
;

class User implements Serializable :
    name = ""
    fn __init__(self, name) :
        self->name = name
    ;
    fn to_json(self) :
        give "json:" + self->name
    ;
    fn from_json(self, data) :
        self->name = data
    ;
;

u = User("Alice")
print(u->to_json())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "json:Alice"); });

    runTest("Missing interface method throws error", []()
            { XASSERT(expectError<TypeError>(R"XEL(
interface Drawable :
    fn draw(self) ;
    fn resize(self, factor) ;
;

class Box implements Drawable :
    fn draw(self) :
        print("box")
    ;
;
)XEL")); });

    runTest("Wrong parameter count throws error", []()
            { XASSERT(expectError<TypeError>(R"XEL(
interface Drawable :
    fn draw(self) ;
;

class Box implements Drawable :
    fn draw(self, extra) :
        print("box")
    ;
;
)XEL")); });
}

// ============================================================================
// Section 3: Multiple Interfaces
// ============================================================================

static void testMultipleInterfaces()
{
    std::cout << "\n===== Multiple Interfaces =====\n";

    runTest("Class implements multiple interfaces", []()
            {
        auto out = runXell(R"XEL(
interface Drawable :
    fn draw(self) ;
;

interface Printable :
    fn to_string(self) ;
;

class Widget implements Drawable, Printable :
    name = ""
    fn __init__(self, name) :
        self->name = name
    ;
    fn draw(self) :
        print("draw {self->name}")
    ;
    fn to_string(self) :
        give "Widget({self->name})"
    ;
;

w = Widget("btn")
w->draw()
print(w->to_string())
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "draw btn");
        XASSERT_EQ(out[1], "Widget(btn)"); });

    runTest("Missing method from second interface throws error", []()
            { XASSERT(expectError<TypeError>(R"XEL(
interface Drawable :
    fn draw(self) ;
;

interface Printable :
    fn to_string(self) ;
;

class Widget implements Drawable, Printable :
    fn draw(self) :
        print("draw")
    ;
;
)XEL")); });
}

// ============================================================================
// Section 4: Interfaces with Inheritance
// ============================================================================

static void testInterfacesWithInheritance()
{
    std::cout << "\n===== Interfaces with Inheritance =====\n";

    runTest("Class inherits and implements", []()
            {
        auto out = runXell(R"XEL(
interface Drawable :
    fn draw(self) ;
;

class Shape :
    color = ""
    fn __init__(self, color) :
        self->color = color
    ;
;

class Circle inherits Shape implements Drawable :
    r = 0
    fn __init__(self, color, r) :
        parent->__init__(color)
        self->r = r
    ;
    fn draw(self) :
        print("draw {self->color} circle r={self->r}")
    ;
;

c = Circle("red", 10)
c->draw()
print(c->color)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "draw red circle r=10");
        XASSERT_EQ(out[1], "red"); });

    runTest("Inherited method satisfies interface", []()
            {
        auto out = runXell(R"XEL(
interface Drawable :
    fn draw(self) ;
;

class Base :
    fn draw(self) :
        print("base draw")
    ;
;

class Child inherits Base implements Drawable :
    fn other(self) :
        print("other")
    ;
;

c = Child()
c->draw()
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "base draw"); });

    runTest("Non-interface in implements clause throws error", []()
            { XASSERT(expectError<TypeError>(R"XEL(
class Foo :
    fn bar(self) :
        print("bar")
    ;
;

class Baz implements Foo :
    fn bar(self) :
        print("baz bar")
    ;
;
)XEL")); });
}

// ============================================================================
// Section 5: Edge Cases
// ============================================================================

static void testEdgeCases()
{
    std::cout << "\n===== Edge Cases =====\n";

    runTest("Empty interface (no methods)", []()
            {
        auto out = runXell(R"XEL(
interface Marker :
;

class Foo implements Marker :
    fn hello(self) :
        print("hello")
    ;
;

f = Foo()
f->hello()
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "hello"); });

    runTest("Interface method with multiple params", []()
            {
        auto out = runXell(R"XEL(
interface Calculator :
    fn add(self, a, b) ;
    fn sub(self, a, b) ;
;

class SimpleCalc implements Calculator :
    fn add(self, a, b) :
        give a + b
    ;
    fn sub(self, a, b) :
        give a - b
    ;
;

c = SimpleCalc()
print(c->add(3, 4))
print(c->sub(10, 3))
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "7");
        XASSERT_EQ(out[1], "7"); });

    runTest("Same method in multiple interfaces satisfied once", []()
            {
        auto out = runXell(R"XEL(
interface A :
    fn foo(self) ;
;

interface B :
    fn foo(self) ;
;

class C implements A, B :
    fn foo(self) :
        print("foo from C")
    ;
;

c = C()
c->foo()
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "foo from C"); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===== INTERFACE TESTS (Phase 7 OOP) =====\n";

    testBasicInterface();
    testImplementsBasic();
    testMultipleInterfaces();
    testInterfacesWithInheritance();
    testEdgeCases();

    std::cout << "\n========================================\n";
    std::cout << "Total: " << (g_passed + g_failed)
              << "  Passed: " << g_passed
              << "  Failed: " << g_failed << "\n";
    return g_failed > 0 ? 1 : 0;
}
