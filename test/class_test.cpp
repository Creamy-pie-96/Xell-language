// =============================================================================
// Class & Inheritance Tests
// =============================================================================
// Tests Phase 2+3 OOP: class definitions, __init__ constructors, inheritance
// via `inherits`, method resolution order (left-to-right DFS), parent->method()
// calls, `is` operator for type checking, typeof for classes, multiple
// inheritance, frozen class instances (~), and error cases.
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

// Helper: run Xell source and return the output lines
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

// Helper: run Xell source and expect a specific exception type
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
// Section 1: Basic Class Definition & Construction
// ============================================================================

static void testBasicClass()
{
    std::cout << "\n===== Basic Class Definition & Construction =====\n";

    runTest("Empty class definition", []()
            {
        auto out = runXell(R"XEL(
class Empty :
;
e = Empty()
print(typeof(e))
)XEL");
        XASSERT_EQ(out[0], "Empty"); });

    runTest("Class with fields no __init__", []()
            {
        auto out = runXell(R"XEL(
class Point :
    x = 0
    y = 0
;
p = Point(10, 20)
print(p->x)
print(p->y)
)XEL");
        XASSERT_EQ(out[0], "10");
        XASSERT_EQ(out[1], "20"); });

    runTest("Class with named args no __init__", []()
            {
        auto out = runXell(R"XEL(
class Point :
    x = 0
    y = 0
;
p = Point(y: 42, x: 7)
print(p->x)
print(p->y)
)XEL");
        XASSERT_EQ(out[0], "7");
        XASSERT_EQ(out[1], "42"); });

    runTest("typeof class definition returns class", []()
            {
        auto out = runXell(R"XEL(
class Foo :
;
print(typeof(Foo))
)XEL");
        XASSERT_EQ(out[0], "class"); });

    runTest("typeof class instance returns class name", []()
            {
        auto out = runXell(R"XEL(
class Foo :
    x = 1
;
f = Foo()
print(typeof(f))
)XEL");
        XASSERT_EQ(out[0], "Foo"); });
}

// ============================================================================
// Section 2: __init__ Constructor
// ============================================================================

static void testInit()
{
    std::cout << "\n===== __init__ Constructor =====\n";

    runTest("Basic __init__", []()
            {
        auto out = runXell(R"XEL(
class Person :
    name = ""
    age = 0

    fn __init__(self, name, age) :
        self->name = name
        self->age = age
    ;
;
p = Person("Alice", 30)
print(p->name)
print(p->age)
)XEL");
        XASSERT_EQ(out[0], "Alice");
        XASSERT_EQ(out[1], "30"); });

    runTest("__init__ with default values", []()
            {
        auto out = runXell(R"XEL(
class Config :
    host = ""
    port = 0

    fn __init__(self, host = "localhost", port = 8080) :
        self->host = host
        self->port = port
    ;
;
c = Config()
print(c->host)
print(c->port)
)XEL");
        XASSERT_EQ(out[0], "localhost");
        XASSERT_EQ(out[1], "8080"); });

    runTest("__init__ with partial args", []()
            {
        auto out = runXell(R"XEL(
class Config :
    host = ""
    port = 0

    fn __init__(self, host = "localhost", port = 8080) :
        self->host = host
        self->port = port
    ;
;
c = Config("example.com")
print(c->host)
print(c->port)
)XEL");
        XASSERT_EQ(out[0], "example.com");
        XASSERT_EQ(out[1], "8080"); });

    runTest("__init__ mutation propagates", []()
            {
        auto out = runXell(R"XEL(
class Counter :
    count = 0

    fn __init__(self, start) :
        self->count = start * 2
    ;
;
c = Counter(5)
print(c->count)
)XEL");
        XASSERT_EQ(out[0], "10"); });
}

// ============================================================================
// Section 3: Class Methods
// ============================================================================

static void testMethods()
{
    std::cout << "\n===== Class Methods =====\n";

    runTest("Simple method call", []()
            {
        auto out = runXell(R"XEL(
class Greeter :
    name = ""

    fn __init__(self, name) :
        self->name = name
    ;

    fn greet(self) :
        give "Hello, " + self->name + "!"
    ;
;
g = Greeter("World")
print(g->greet())
)XEL");
        XASSERT_EQ(out[0], "Hello, World!"); });

    runTest("Method with args", []()
            {
        auto out = runXell(R"XEL(
class Calc :
    base = 0

    fn __init__(self, base) :
        self->base = base
    ;

    fn add(self, x) :
        give self->base + x
    ;
;
c = Calc(10)
print(c->add(5))
)XEL");
        XASSERT_EQ(out[0], "15"); });

    runTest("Method modifying fields", []()
            {
        auto out = runXell(R"XEL(
class Counter :
    count = 0

    fn increment(self) :
        self->count = self->count + 1
    ;

    fn get(self) :
        give self->count
    ;
;
c = Counter()
c->increment()
c->increment()
c->increment()
print(c->get())
)XEL");
        XASSERT_EQ(out[0], "3"); });
}

// ============================================================================
// Section 4: Single Inheritance
// ============================================================================

static void testSingleInheritance()
{
    std::cout << "\n===== Single Inheritance =====\n";

    runTest("Child inherits parent fields", []()
            {
        auto out = runXell(R"XEL(
class Animal :
    name = ""
    sound = ""

    fn __init__(self, name, sound) :
        self->name = name
        self->sound = sound
    ;
;

class Dog inherits Animal :
    breed = ""

    fn __init__(self, name, breed) :
        parent->__init__(name, "Woof")
        self->breed = breed
    ;
;

d = Dog("Rex", "Labrador")
print(d->name)
print(d->sound)
print(d->breed)
)XEL");
        XASSERT_EQ(out[0], "Rex");
        XASSERT_EQ(out[1], "Woof");
        XASSERT_EQ(out[2], "Labrador"); });

    runTest("Child inherits parent methods", []()
            {
        auto out = runXell(R"XEL(
class Animal :
    name = ""

    fn __init__(self, name) :
        self->name = name
    ;

    fn speak(self) :
        give self->name + " makes a sound"
    ;
;

class Cat inherits Animal :
    fn __init__(self, name) :
        parent->__init__(name)
    ;
;

c = Cat("Whiskers")
print(c->speak())
)XEL");
        XASSERT_EQ(out[0], "Whiskers makes a sound"); });

    runTest("Child overrides parent method", []()
            {
        auto out = runXell(R"XEL(
class Animal :
    name = ""

    fn __init__(self, name) :
        self->name = name
    ;

    fn speak(self) :
        give self->name + " makes a sound"
    ;
;

class Dog inherits Animal :
    fn __init__(self, name) :
        parent->__init__(name)
    ;

    fn speak(self) :
        give self->name + " says Woof!"
    ;
;

d = Dog("Rex")
print(d->speak())
)XEL");
        XASSERT_EQ(out[0], "Rex says Woof!"); });

    runTest("Child adds new methods", []()
            {
        auto out = runXell(R"XEL(
class Animal :
    name = ""

    fn __init__(self, name) :
        self->name = name
    ;
;

class Dog inherits Animal :
    fn __init__(self, name) :
        parent->__init__(name)
    ;

    fn fetch(self) :
        give self->name + " fetches!"
    ;
;

d = Dog("Buddy")
print(d->fetch())
)XEL");
        XASSERT_EQ(out[0], "Buddy fetches!"); });

    runTest("Three-level inheritance", []()
            {
        auto out = runXell(R"XEL(
class A :
    x = 0
    fn __init__(self, x) :
        self->x = x
    ;
    fn who(self) :
        give "A"
    ;
;

class B inherits A :
    y = 0
    fn __init__(self, x, y) :
        parent->__init__(x)
        self->y = y
    ;
;

class C inherits B :
    z = 0
    fn __init__(self, x, y, z) :
        parent->__init__(x, y)
        self->z = z
    ;
;

c = C(1, 2, 3)
print(c->x)
print(c->y)
print(c->z)
print(c->who())
)XEL");
        XASSERT_EQ(out[0], "1");
        XASSERT_EQ(out[1], "2");
        XASSERT_EQ(out[2], "3");
        XASSERT_EQ(out[3], "A"); });
}

// ============================================================================
// Section 5: `is` Operator
// ============================================================================

static void testIsOperator()
{
    std::cout << "\n===== is Operator =====\n";

    runTest("Instance is its own class", []()
            {
        auto out = runXell(R"XEL(
class Foo :
;
f = Foo()
print(f is Foo)
)XEL");
        XASSERT_EQ(out[0], "true"); });

    runTest("Instance is parent class", []()
            {
        auto out = runXell(R"XEL(
class Animal :
;
class Dog inherits Animal :
;
d = Dog()
print(d is Animal)
)XEL");
        XASSERT_EQ(out[0], "true"); });

    runTest("Instance is not unrelated class", []()
            {
        auto out = runXell(R"XEL(
class Cat :
;
class Dog :
;
d = Dog()
print(d is Cat)
)XEL");
        XASSERT_EQ(out[0], "false"); });

    runTest("Parent instance is not child class", []()
            {
        auto out = runXell(R"XEL(
class Animal :
;
class Dog inherits Animal :
;
a = Animal()
print(a is Dog)
)XEL");
        XASSERT_EQ(out[0], "false"); });

    runTest("is works through multiple levels", []()
            {
        auto out = runXell(R"XEL(
class A :
;
class B inherits A :
;
class C inherits B :
;
c = C()
print(c is A)
print(c is B)
print(c is C)
)XEL");
        XASSERT_EQ(out[0], "true");
        XASSERT_EQ(out[1], "true");
        XASSERT_EQ(out[2], "true"); });

    runTest("is with non-instance falls back to equality", []()
            {
        auto out = runXell(R"XEL(
print(5 is 5)
print("a" is "b")
)XEL");
        XASSERT_EQ(out[0], "true");
        XASSERT_EQ(out[1], "false"); });
}

// ============================================================================
// Section 6: Multiple Inheritance
// ============================================================================

static void testMultipleInheritance()
{
    std::cout << "\n===== Multiple Inheritance =====\n";

    runTest("Fields from multiple parents", []()
            {
        auto out = runXell(R"XEL(
class Flyer :
    altitude = 0
    fn __init__(self, alt) :
        self->altitude = alt
    ;
;

class Swimmer :
    depth = 0
    fn __init__(self, d) :
        self->depth = d
    ;
;

class Duck inherits Flyer, Swimmer :
    name = ""
    fn __init__(self, name, alt, d) :
        self->name = name
        self->altitude = alt
        self->depth = d
    ;
;

duck = Duck("Donald", 100, 5)
print(duck->name)
print(duck->altitude)
print(duck->depth)
)XEL");
        XASSERT_EQ(out[0], "Donald");
        XASSERT_EQ(out[1], "100");
        XASSERT_EQ(out[2], "5"); });

    runTest("Method from left parent wins (MRO)", []()
            {
        auto out = runXell(R"XEL(
class A :
    fn who(self) :
        give "A"
    ;
;

class B :
    fn who(self) :
        give "B"
    ;
;

class C inherits A, B :
;

c = C()
print(c->who())
)XEL");
        XASSERT_EQ(out[0], "A"); });

    runTest("is works with multiple parents", []()
            {
        auto out = runXell(R"XEL(
class A :
;
class B :
;
class C inherits A, B :
;
c = C()
print(c is A)
print(c is B)
print(c is C)
)XEL");
        XASSERT_EQ(out[0], "true");
        XASSERT_EQ(out[1], "true");
        XASSERT_EQ(out[2], "true"); });

    runTest("Method from second parent if first lacks it", []()
            {
        auto out = runXell(R"XEL(
class A :
    fn greet(self) :
        give "Hi from A"
    ;
;

class B :
    fn farewell(self) :
        give "Bye from B"
    ;
;

class C inherits A, B :
;

c = C()
print(c->greet())
print(c->farewell())
)XEL");
        XASSERT_EQ(out[0], "Hi from A");
        XASSERT_EQ(out[1], "Bye from B"); });
}

// ============================================================================
// Section 7: Frozen Class Instances
// ============================================================================

static void testFrozenClass()
{
    std::cout << "\n===== Frozen Class Instances =====\n";

    runTest("Frozen class (no __init__) immutable", []()
            {
        XASSERT(expectError<ImmutabilityError>(R"XEL(
class Point :
    x = 0
    y = 0
;
p = ~Point(1, 2)
p->x = 99
)XEL")); });

    runTest("Frozen class with __init__", []()
            {
        auto out = runXell(R"XEL(
class Vec :
    x = 0
    y = 0

    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;
;
v = ~Vec(3, 4)
print(v->x)
print(v->y)
)XEL");
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "4"); });

    runTest("Frozen class with __init__ is immutable", []()
            {
        XASSERT(expectError<ImmutabilityError>(R"XEL(
class Vec :
    x = 0
    y = 0

    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;
;
v = ~Vec(3, 4)
v->x = 99
)XEL")); });
}

// ============================================================================
// Section 8: Field Access & Mutation
// ============================================================================

static void testFieldAccess()
{
    std::cout << "\n===== Field Access & Mutation =====\n";

    runTest("Set and get fields on class instance", []()
            {
        auto out = runXell(R"XEL(
class Box :
    width = 0
    height = 0
;
b = Box(10, 20)
print(b->width)
b->width = 50
print(b->width)
)XEL");
        XASSERT_EQ(out[0], "10");
        XASSERT_EQ(out[1], "50"); });

    runTest("Set inherited field", []()
            {
        auto out = runXell(R"XEL(
class Base :
    x = 0
;
class Child inherits Base :
    y = 0
;
c = Child()
c->x = 42
c->y = 99
print(c->x)
print(c->y)
)XEL");
        XASSERT_EQ(out[0], "42");
        XASSERT_EQ(out[1], "99"); });

    runTest("Error on nonexistent field assignment", []()
            {
        XASSERT(expectError<AttributeError>(R"XEL(
class Foo :
    x = 0
;
f = Foo()
f->missing = 1
)XEL")); });

    runTest("Error on nonexistent field access", []()
            {
        XASSERT(expectError<AttributeError>(R"XEL(
class Foo :
    x = 0
;
f = Foo()
print(f->missing)
)XEL")); });
}

// ============================================================================
// Section 9: parent->method() Calls
// ============================================================================

static void testParentCalls()
{
    std::cout << "\n===== parent->method() Calls =====\n";

    runTest("parent->__init__ basic", []()
            {
        auto out = runXell(R"XEL(
class Base :
    x = 0

    fn __init__(self, x) :
        self->x = x
    ;
;

class Child inherits Base :
    y = 0

    fn __init__(self, x, y) :
        parent->__init__(x)
        self->y = y
    ;
;

c = Child(10, 20)
print(c->x)
print(c->y)
)XEL");
        XASSERT_EQ(out[0], "10");
        XASSERT_EQ(out[1], "20"); });

    runTest("parent->method() for non-init", []()
            {
        auto out = runXell(R"XEL(
class Base :
    fn greet(self) :
        give "Hello from Base"
    ;
;

class Child inherits Base :
    fn greet(self) :
        result = parent->greet()
        give result + " and Child"
    ;
;

c = Child()
print(c->greet())
)XEL");
        XASSERT_EQ(out[0], "Hello from Base and Child"); });

    runTest("parent chain — three levels", []()
            {
        auto out = runXell(R"XEL(
class A :
    val = ""

    fn __init__(self, v) :
        self->val = v
    ;
;

class B inherits A :
    fn __init__(self, v) :
        parent->__init__(v + "-B")
    ;
;

class C inherits B :
    fn __init__(self, v) :
        parent->__init__(v + "-C")
    ;
;

c = C("start")
print(c->val)
)XEL");
        XASSERT_EQ(out[0], "start-C-B"); });
}

// ============================================================================
// Section 10: Edge Cases & Errors
// ============================================================================

static void testEdgeCases()
{
    std::cout << "\n===== Edge Cases & Errors =====\n";

    runTest("Cannot inherit from struct", []()
            {
        XASSERT(expectError<TypeError>(R"XEL(
struct S :
    x = 0
;
class C inherits S :
;
)XEL")); });

    runTest("Cannot inherit from undefined", []()
            {
        XASSERT(expectError<UndefinedVariableError>(R"XEL(
class C inherits NoSuchClass :
;
)XEL")); });

    runTest("Class method shadows builtin name", []()
            {
        auto out = runXell(R"XEL(
class MyList :
    items = []

    fn len(self) :
        give 42
    ;
;
m = MyList()
print(m->len())
)XEL");
        XASSERT_EQ(out[0], "42"); });

    runTest("Multiple instances are independent", []()
            {
        auto out = runXell(R"XEL(
class Counter :
    count = 0

    fn inc(self) :
        self->count = self->count + 1
    ;
;
a = Counter()
b = Counter()
a->inc()
a->inc()
b->inc()
print(a->count)
print(b->count)
)XEL");
        XASSERT_EQ(out[0], "2");
        XASSERT_EQ(out[1], "1"); });

    runTest("Class with only methods (no fields)", []()
            {
        auto out = runXell(R"XEL(
class Util :
    fn hello(self) :
        give "hello"
    ;
;
u = Util()
print(u->hello())
)XEL");
        XASSERT_EQ(out[0], "hello"); });

    runTest("Inherited method accesses child fields", []()
            {
        auto out = runXell(R"XEL(
class Shape :
    fn describe(self) :
        give "Shape: " + str(self->area())
    ;
;

class Square inherits Shape :
    side = 0

    fn __init__(self, s) :
        self->side = s
    ;

    fn area(self) :
        give self->side * self->side
    ;
;

sq = Square(5)
print(sq->describe())
)XEL");
        XASSERT_EQ(out[0], "Shape: 25"); });
}

// ============================================================================
// Section 11: Class with immutable keyword
// ============================================================================

static void testClassImmutable()
{
    std::cout << "\n===== Class with immutable keyword =====\n";

    runTest("Immutable binding to class instance", []()
            {
        XASSERT(expectError<ImmutabilityError>(R"XEL(
class Foo :
    x = 0
;
immutable f = Foo(5)
f = Foo(10)
)XEL")); });

    runTest("Immutable binding - can still mutate fields", []()
            {
        auto out = runXell(R"XEL(
class Foo :
    x = 0
;
immutable f = Foo(5)
f->x = 99
print(f->x)
)XEL");
        XASSERT_EQ(out[0], "99"); });
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout << "============================================\n";
    std::cout << " Class & Inheritance Test Suite\n";
    std::cout << "============================================\n";

    testBasicClass();
    testInit();
    testMethods();
    testSingleInheritance();
    testIsOperator();
    testMultipleInheritance();
    testFrozenClass();
    testFieldAccess();
    testParentCalls();
    testEdgeCases();
    testClassImmutable();

    std::cout << "\n============================================\n";
    std::cout << " Results: " << g_passed << " passed, " << g_failed << " failed\n";
    std::cout << "============================================\n";

    return g_failed > 0 ? 1 : 0;
}
