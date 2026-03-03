// =============================================================================
// Properties (get/set) Tests
// =============================================================================
// Tests Phase 6 OOP: property getters and setters for classes.
// get name(self) : ... ; intercepts field reads
// set name(self, val) : ... ; intercepts field writes
// Read-only (get without set) and write-only (set without get) enforcement.
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
// Section 1: Basic Getter
// ============================================================================

static void testBasicGetter()
{
    std::cout << "\n===== Basic Getter =====\n";

    runTest("Simple property getter", []()
            {
        auto out = runXell(R"XEL(
class Circle :
    _radius = 5

    get radius(self) :
        give self->_radius
    ;
;
c = Circle()
print(c->radius)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "5"); });

    runTest("Getter with __init__", []()
            {
        auto out = runXell(R"XEL(
class Circle :
    _radius = 0

    fn __init__(self, r) :
        self->_radius = r
    ;

    get radius(self) :
        give self->_radius
    ;
;
c = Circle(10)
print(c->radius)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "10"); });

    runTest("Computed property getter", []()
            {
        auto out = runXell(R"XEL(
class Rect :
    _w = 0
    _h = 0

    fn __init__(self, w, h) :
        self->_w = w
        self->_h = h
    ;

    get area(self) :
        give self->_w * self->_h
    ;
;
r = Rect(3, 4)
print(r->area)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "12"); });
}

// ============================================================================
// Section 2: Basic Setter
// ============================================================================

static void testBasicSetter()
{
    std::cout << "\n===== Basic Setter =====\n";

    runTest("Simple property setter", []()
            {
        auto out = runXell(R"XEL(
class Box :
    _size = 0

    get size(self) :
        give self->_size
    ;

    set size(self, val) :
        self->_size = val
    ;
;
b = Box()
b->size = 42
print(b->size)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "42"); });

    runTest("Setter with validation", []()
            {
        auto out = runXell(R"XEL(
class Circle :
    _radius = 0

    fn __init__(self, r) :
        self->_radius = r
    ;

    get radius(self) :
        give self->_radius
    ;

    set radius(self, val) :
        if val < 0 :
            error("negative radius!")
        ;
        self->_radius = val
    ;
;
c = Circle(5)
print(c->radius)
c->radius = 10
print(c->radius)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "5");
        XASSERT_EQ(out[1], "10"); });

    runTest("Setter validation prevents bad values", []()
            {
        auto out = runXell(R"XEL(
class Circle :
    _radius = 0

    get radius(self) :
        give self->_radius
    ;

    set radius(self, val) :
        if val >= 0 :
            self->_radius = val
        ;
    ;
;
c = Circle()
c->radius = 5
print(c->radius)
c->radius = -1
print(c->radius)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "5");
        XASSERT_EQ(out[1], "5"); }); // -1 was ignored, still 5
}

// ============================================================================
// Section 3: Read-only and Write-only Properties
// ============================================================================

static void testReadWriteOnly()
{
    std::cout << "\n===== Read-only / Write-only Properties =====\n";

    runTest("Read-only property (get without set)", []()
            {
        auto out = runXell(R"XEL(
class Immutable :
    _val = 42

    get val(self) :
        give self->_val
    ;
;
o = Immutable()
print(o->val)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "42"); });

    runTest("Read-only property rejects write", []()
            { XASSERT(expectError<AttributeError>(R"XEL(
class Immutable :
    _val = 42

    get val(self) :
        give self->_val
    ;
;
o = Immutable()
o->val = 99
)XEL")); });

    runTest("Write-only property rejects read", []()
            { XASSERT(expectError<AttributeError>(R"XEL(
class Logger :
    _log = ""

    set message(self, msg) :
        self->_log = self->_log + msg
    ;
;
l = Logger()
l->message = "hello"
print(l->message)
)XEL")); });
}

// ============================================================================
// Section 4: Properties with Inheritance
// ============================================================================

static void testPropertyInheritance()
{
    std::cout << "\n===== Properties with Inheritance =====\n";

    runTest("Property inherited from parent class", []()
            {
        auto out = runXell(R"XEL(
class Base :
    _x = 100

    get x(self) :
        give self->_x
    ;
;
class Child inherits Base :
;
c = Child()
print(c->x)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "100"); });

    runTest("Property setter inherited from parent", []()
            {
        auto out = runXell(R"XEL(
class Base :
    _x = 0

    get x(self) :
        give self->_x
    ;

    set x(self, val) :
        self->_x = val
    ;
;
class Child inherits Base :
;
c = Child()
c->x = 55
print(c->x)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "55"); });
}

// ============================================================================
// Section 5: Multiple Properties
// ============================================================================

static void testMultipleProperties()
{
    std::cout << "\n===== Multiple Properties =====\n";

    runTest("Class with multiple get/set properties", []()
            {
        auto out = runXell(R"XEL(
class Person :
    _name = ""
    _age = 0

    fn __init__(self, n, a) :
        self->_name = n
        self->_age = a
    ;

    get name(self) :
        give self->_name
    ;

    get age(self) :
        give self->_age
    ;

    set age(self, val) :
        if val < 0 :
            error("age cannot be negative")
        ;
        self->_age = val
    ;
;
p = Person("Alice", 30)
print(p->name)
print(p->age)
p->age = 31
print(p->age)
)XEL");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], "Alice");
        XASSERT_EQ(out[1], "30");
        XASSERT_EQ(out[2], "31"); });
}

// ============================================================================
// Section 6: Properties alongside regular fields and methods
// ============================================================================

static void testPropertiesWithMembers()
{
    std::cout << "\n===== Properties with Regular Members =====\n";

    runTest("Properties coexist with regular fields and methods", []()
            {
        auto out = runXell(R"XEL(
class Car :
    _speed = 0
    color = "red"

    fn __init__(self, c) :
        self->color = c
    ;

    get speed(self) :
        give self->_speed
    ;

    set speed(self, val) :
        self->_speed = val
    ;

    fn describe(self) :
        give self->color + " car at speed " + str(self->speed)
    ;
;
car = Car("blue")
car->speed = 60
print(car->describe())
print(car->color)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "blue car at speed 60");
        XASSERT_EQ(out[1], "blue"); });
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout << "===== PROPERTY TESTS (Phase 6 OOP) =====\n";

    testBasicGetter();
    testBasicSetter();
    testReadWriteOnly();
    testPropertyInheritance();
    testMultipleProperties();
    testPropertiesWithMembers();

    std::cout << "\n========================================\n";
    std::cout << "Total: " << (g_passed + g_failed)
              << "  Passed: " << g_passed
              << "  Failed: " << g_failed << "\n";

    return g_failed == 0 ? 0 : 1;
}
