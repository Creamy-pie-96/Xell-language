// =============================================================================
// Abstract Class Tests
// =============================================================================
// Tests Phase 8 OOP: abstract class definitions with abstract and default
// methods, prevention of direct instantiation, and enforcement of abstract
// method implementation in subclasses.
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
// Section 1: Basic Abstract Class Definition
// ============================================================================

static void testBasicAbstract()
{
    std::cout << "\n===== Basic Abstract Class =====\n";

    runTest("Define abstract class", []()
            {
        // Just defining an abstract class should not error
        runXell(R"XEL(
abstract Shape :
    fn area(self) ;
    fn perimeter(self) ;
;
)XEL"); });

    runTest("Abstract class cannot be instantiated", []()
            { XASSERT(expectError<TypeError>(R"XEL(
abstract Shape :
    fn area(self) ;
;
s = Shape()
)XEL")); });

    runTest("Abstract class with default methods", []()
            {
        // Should define fine — has both abstract and default methods
        runXell(R"XEL(
abstract Shape :
    fn area(self) ;

    fn color(self) :
        give "white"
    ;
;
)XEL"); });
}

// ============================================================================
// Section 2: Implementing Abstract Methods
// ============================================================================

static void testImplementAbstract()
{
    std::cout << "\n===== Implementing Abstract Methods =====\n";

    runTest("Subclass implements all abstract methods", []()
            {
        auto out = runXell(R"XEL(
abstract Shape :
    fn area(self) ;
    fn perimeter(self) ;
;

class Rectangle inherits Shape :
    w = 0
    h = 0
    fn __init__(self, w, h) :
        self->w = w
        self->h = h
    ;
    fn area(self) :
        give self->w * self->h
    ;
    fn perimeter(self) :
        give 2 * (self->w + self->h)
    ;
;

r = Rectangle(4, 5)
print(r->area())
print(r->perimeter())
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "20");
        XASSERT_EQ(out[1], "18"); });

    runTest("Missing abstract method throws error", []()
            { XASSERT(expectError<TypeError>(R"XEL(
abstract Shape :
    fn area(self) ;
    fn perimeter(self) ;
;

class Rectangle inherits Shape :
    w = 0
    h = 0
    fn __init__(self, w, h) :
        self->w = w
        self->h = h
    ;
    fn area(self) :
        give self->w * self->h
    ;
;
)XEL")); });

    runTest("Subclass inherits default methods", []()
            {
        auto out = runXell(R"XEL(
abstract Shape :
    fn area(self) ;

    fn color(self) :
        give "white"
    ;
;

class Square inherits Shape :
    s = 0
    fn __init__(self, s) :
        self->s = s
    ;
    fn area(self) :
        give self->s * self->s
    ;
;

sq = Square(5)
print(sq->area())
print(sq->color())
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "25");
        XASSERT_EQ(out[1], "white"); });

    runTest("Subclass overrides default method", []()
            {
        auto out = runXell(R"XEL(
abstract Shape :
    fn area(self) ;

    fn color(self) :
        give "white"
    ;
;

class Circle inherits Shape :
    r = 0
    fn __init__(self, r) :
        self->r = r
    ;
    fn area(self) :
        give 3.14 * self->r * self->r
    ;
    fn color(self) :
        give "red"
    ;
;

c = Circle(10)
print(c->color())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "red"); });
}

// ============================================================================
// Section 3: Abstract with Fields
// ============================================================================

static void testAbstractWithFields()
{
    std::cout << "\n===== Abstract with Fields =====\n";

    runTest("Abstract class with fields and abstract methods", []()
            {
        auto out = runXell(R"XEL(
abstract Vehicle :
    speed = 0

    fn describe(self) ;
;

class Car inherits Vehicle :
    brand = ""
    fn __init__(self, brand, speed) :
        self->brand = brand
        self->speed = speed
    ;
    fn describe(self) :
        give "{self->brand} at {self->speed}mph"
    ;
;

c = Car("Toyota", 60)
print(c->describe())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "Toyota at 60mph"); });
}

// ============================================================================
// Section 4: Abstract with Interfaces
// ============================================================================

static void testAbstractWithInterfaces()
{
    std::cout << "\n===== Abstract with Interfaces =====\n";

    runTest("Abstract class implements interface", []()
            {
        auto out = runXell(R"XEL(
interface Printable :
    fn to_string(self) ;
;

abstract Shape implements Printable :
    fn area(self) ;

    fn to_string(self) :
        give "Shape"
    ;
;

class Rect inherits Shape :
    w = 0
    h = 0
    fn __init__(self, w, h) :
        self->w = w
        self->h = h
    ;
    fn area(self) :
        give self->w * self->h
    ;
;

r = Rect(3, 4)
print(r->area())
print(r->to_string())
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "12");
        XASSERT_EQ(out[1], "Shape"); });
}

// ============================================================================
// Section 5: Abstract inheriting Abstract
// ============================================================================

static void testAbstractInheritance()
{
    std::cout << "\n===== Abstract Inheriting Abstract =====\n";

    runTest("Abstract subclass of abstract does not need to implement", []()
            {
        // An abstract class inheriting another abstract doesn't need to implement
        runXell(R"XEL(
abstract Base :
    fn foo(self) ;
;

abstract Mid inherits Base :
    fn bar(self) ;
;
)XEL"); });

    runTest("Concrete subclass must implement all inherited abstract methods", []()
            {
        auto out = runXell(R"XEL(
abstract Base :
    fn foo(self) ;
;

abstract Mid inherits Base :
    fn bar(self) ;
;

class Concrete inherits Mid :
    fn foo(self) :
        print("foo")
    ;
    fn bar(self) :
        print("bar")
    ;
;

c = Concrete()
c->foo()
c->bar()
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "foo");
        XASSERT_EQ(out[1], "bar"); });

    runTest("Missing inherited abstract method throws error", []()
            { XASSERT(expectError<TypeError>(R"XEL(
abstract Base :
    fn foo(self) ;
;

abstract Mid inherits Base :
    fn bar(self) ;
;

class Concrete inherits Mid :
    fn bar(self) :
        print("bar")
    ;
;
)XEL")); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===== ABSTRACT CLASS TESTS (Phase 8 OOP) =====\n";

    testBasicAbstract();
    testImplementAbstract();
    testAbstractWithFields();
    testAbstractWithInterfaces();
    testAbstractInheritance();

    std::cout << "\n========================================\n";
    std::cout << "Total: " << (g_passed + g_failed)
              << "  Passed: " << g_passed
              << "  Failed: " << g_failed << "\n";
    return g_failed > 0 ? 1 : 0;
}
