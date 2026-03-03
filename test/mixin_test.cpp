// =============================================================================
// Mixin Tests
// =============================================================================
// Tests Phase 9 OOP: mixin definitions, 'with' keyword for mixing in methods,
// conflict resolution, and combining with inherits/implements.
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
// Section 1: Basic Mixin Definition
// ============================================================================

static void testBasicMixin()
{
    std::cout << "\n===== Basic Mixin Definition =====\n";

    runTest("Define a simple mixin", []()
            { runXell(R"XEL(
mixin Loggable :
    fn log(self, msg) :
        print("[LOG] " + msg)
    ;
;
)XEL"); });

    runTest("Mixin cannot be instantiated", []()
            { XASSERT(expectError<TypeError>(R"XEL(
mixin Loggable :
    fn log(self, msg) :
        print(msg)
    ;
;
x = Loggable()
)XEL")); });

    runTest("Mixin cannot have __init__", []()
            { XASSERT(expectError<TypeError>(R"XEL(
mixin Bad :
    fn __init__(self) :
        print("bad")
    ;
;
)XEL")); });
}

// ============================================================================
// Section 2: Using Mixins with 'with'
// ============================================================================

static void testUsingMixins()
{
    std::cout << "\n===== Using Mixins with 'with' =====\n";

    runTest("Class uses mixin method", []()
            {
        auto out = runXell(R"XEL(
mixin Loggable :
    fn log(self, msg) :
        print("[LOG] " + msg)
    ;
;

class User with Loggable :
    name = ""
    fn __init__(self, name) :
        self->name = name
    ;
;

u = User("Alice")
u->log("created")
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "[LOG] created"); });

    runTest("Multiple mixins", []()
            {
        auto out = runXell(R"XEL(
mixin Loggable :
    fn log(self, msg) :
        print("[LOG] " + msg)
    ;
;

mixin Taggable :
    fn tag(self) :
        give "tagged"
    ;
;

class Item with Loggable, Taggable :
    name = ""
    fn __init__(self, name) :
        self->name = name
    ;
;

i = Item("widget")
i->log("hello")
print(i->tag())
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "[LOG] hello");
        XASSERT_EQ(out[1], "tagged"); });

    runTest("Non-mixin in 'with' clause throws error", []()
            { XASSERT(expectError<TypeError>(R"XEL(
class Foo :
    fn bar(self) :
        print("bar")
    ;
;

class Baz with Foo :
    fn qux(self) :
        print("qux")
    ;
;
)XEL")); });
}

// ============================================================================
// Section 3: Conflict Resolution
// ============================================================================

static void testConflictResolution()
{
    std::cout << "\n===== Conflict Resolution =====\n";

    runTest("Class method overrides mixin method", []()
            {
        auto out = runXell(R"XEL(
mixin Logger :
    fn describe(self) :
        give "from mixin"
    ;
;

class Widget with Logger :
    fn describe(self) :
        give "from class"
    ;
;

w = Widget()
print(w->describe())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "from class"); });

    runTest("First mixin wins in case of conflict between mixins", []()
            {
        auto out = runXell(R"XEL(
mixin A :
    fn greet(self) :
        give "hello from A"
    ;
;

mixin B :
    fn greet(self) :
        give "hello from B"
    ;
;

class C with A, B :
    name = ""
;

c = C()
print(c->greet())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "hello from A"); });
}

// ============================================================================
// Section 4: Mixins with Inheritance
// ============================================================================

static void testMixinsWithInheritance()
{
    std::cout << "\n===== Mixins with Inheritance =====\n";

    runTest("Class inherits and uses mixin", []()
            {
        auto out = runXell(R"XEL(
mixin Loggable :
    fn log(self, msg) :
        print("[LOG] " + msg)
    ;
;

class Animal :
    name = ""
    fn __init__(self, name) :
        self->name = name
    ;
;

class Dog inherits Animal with Loggable :
    breed = ""
    fn __init__(self, name, breed) :
        parent->__init__(name)
        self->breed = breed
    ;
;

d = Dog("Rex", "Lab")
d->log("bark!")
print(d->name)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "[LOG] bark!");
        XASSERT_EQ(out[1], "Rex"); });

    runTest("Parent method takes priority over mixin", []()
            {
        auto out = runXell(R"XEL(
mixin Logger :
    fn speak(self) :
        give "from mixin"
    ;
;

class Base :
    fn speak(self) :
        give "from parent"
    ;
;

class Child inherits Base with Logger :
    name = ""
;

c = Child()
print(c->speak())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "from parent"); });
}

// ============================================================================
// Section 5: Mixins with Interfaces
// ============================================================================

static void testMixinsWithInterfaces()
{
    std::cout << "\n===== Mixins with Interfaces =====\n";

    runTest("Mixin satisfies interface requirement", []()
            {
        auto out = runXell(R"XEL(
interface Printable :
    fn to_string(self) ;
;

mixin DefaultPrintable :
    fn to_string(self) :
        give "printable object"
    ;
;

class Widget with DefaultPrintable implements Printable :
    name = ""
;

w = Widget()
print(w->to_string())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "printable object"); });
}

// ============================================================================
// Section 6: Mixin with Fields
// ============================================================================

static void testMixinWithFields()
{
    std::cout << "\n===== Mixin with Fields =====\n";

    runTest("Mixin provides default field values", []()
            {
        auto out = runXell(R"XEL(
mixin Colorable :
    color = "white"

    fn set_color(self, c) :
        self->color = c
    ;
;

class Box with Colorable :
    size = 10
;

b = Box()
print(b->color)
b->set_color("red")
print(b->color)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "white");
        XASSERT_EQ(out[1], "red"); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===== MIXIN TESTS (Phase 9 OOP) =====\n";

    testBasicMixin();
    testUsingMixins();
    testConflictResolution();
    testMixinsWithInheritance();
    testMixinsWithInterfaces();
    testMixinWithFields();

    std::cout << "\n========================================\n";
    std::cout << "Total: " << (g_passed + g_failed)
              << "  Passed: " << g_passed
              << "  Failed: " << g_failed << "\n";
    return g_failed > 0 ? 1 : 0;
}
