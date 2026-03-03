// ============================================================================
// Xell Phase 11 OOP Tests — Magic Methods & Operator Overloading
// ============================================================================
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <functional>
#include <algorithm>

// ---- Minimal test harness ----

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

#define XASSERT_CONTAINS(haystack, needle)                                   \
    do                                                                       \
    {                                                                        \
        if ((haystack).find(needle) == std::string::npos)                    \
        {                                                                    \
            std::ostringstream os;                                           \
            os << "  ASSERTION FAILED: output should contain \"" << (needle) \
               << "\"\n        got: [" << (haystack) << "]";                 \
            throw std::runtime_error(os.str());                              \
        }                                                                    \
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

// ---- Include the interpreter directly ----
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
// Arithmetic operator overloading: __add__, __sub__, __mul__, __div__, __mod__
// ============================================================================

void testArithmeticOps()
{
    std::cout << "\n===== Arithmetic Operator Overloading =====\n";

    runTest("__add__: a + b", []()
            {
        auto out = runXell(R"XEL(
class Vector :
    x = 0
    y = 0

    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;

    fn __add__(self, other) :
        give Vector(self->x + other->x, self->y + other->y)
    ;
;

a = Vector(1, 2)
b = Vector(3, 4)
c = a + b
print(c->x)
print(c->y)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "4");
        XASSERT_EQ(out[1], "6"); });

    runTest("__sub__: a - b", []()
            {
        auto out = runXell(R"XEL(
class Vec :
    x = 0
    y = 0

    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;

    fn __sub__(self, other) :
        give Vec(self->x - other->x, self->y - other->y)
    ;
;

a = Vec(5, 8)
b = Vec(2, 3)
c = a - b
print(c->x)
print(c->y)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "5"); });

    runTest("__mul__: a * scalar", []()
            {
        auto out = runXell(R"XEL(
class Vec :
    x = 0
    y = 0

    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;

    fn __mul__(self, scalar) :
        give Vec(self->x * scalar, self->y * scalar)
    ;
;

v = Vec(2, 3)
r = v * 4
print(r->x)
print(r->y)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "8");
        XASSERT_EQ(out[1], "12"); });

    runTest("__div__: a / scalar", []()
            {
        auto out = runXell(R"XEL(
class Vec :
    x = 0
    y = 0

    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;

    fn __div__(self, scalar) :
        give Vec(self->x / scalar, self->y / scalar)
    ;
;

v = Vec(10, 20)
r = v / 5
print(r->x)
print(r->y)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "2");
        XASSERT_EQ(out[1], "4"); });

    runTest("__mod__: a % b", []()
            {
        auto out = runXell(R"XEL(
class ModVal :
    val = 0

    fn __init__(self, v) :
        self->val = v
    ;

    fn __mod__(self, other) :
        give ModVal(self->val % other->val)
    ;
;

a = ModVal(10)
b = ModVal(3)
c = a % b
print(c->val)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "1"); });
}

// ============================================================================
// Comparison operator overloading: __eq__, __ne__, __lt__, __gt__, __le__, __ge__
// ============================================================================

void testComparisonOps()
{
    std::cout << "\n===== Comparison Operator Overloading =====\n";

    runTest("__eq__: a == b", []()
            {
        auto out = runXell(R"XEL(
class Point :
    x = 0
    y = 0

    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;

    fn __eq__(self, other) :
        give self->x == other->x and self->y == other->y
    ;
;

a = Point(1, 2)
b = Point(1, 2)
c = Point(3, 4)
print(a == b)
print(a == c)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "true");
        XASSERT_EQ(out[1], "false"); });

    runTest("__ne__: a != b (derived from __eq__)", []()
            {
        auto out = runXell(R"XEL(
class Point :
    x = 0
    y = 0

    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;

    fn __eq__(self, other) :
        give self->x == other->x and self->y == other->y
    ;
;

a = Point(1, 2)
b = Point(3, 4)
print(a != b)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "true"); });

    runTest("__lt__: a < b", []()
            {
        auto out = runXell(R"XEL(
class Num :
    val = 0

    fn __init__(self, v) :
        self->val = v
    ;

    fn __lt__(self, other) :
        give self->val < other->val
    ;
;

a = Num(3)
b = Num(7)
print(a < b)
print(b < a)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "true");
        XASSERT_EQ(out[1], "false"); });

    runTest("__gt__: a > b", []()
            {
        auto out = runXell(R"XEL(
class Num :
    val = 0

    fn __init__(self, v) :
        self->val = v
    ;

    fn __gt__(self, other) :
        give self->val > other->val
    ;
;

a = Num(9)
b = Num(5)
print(a > b)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "true"); });

    runTest("__le__ and __ge__", []()
            {
        auto out = runXell(R"XEL(
class Num :
    val = 0

    fn __init__(self, v) :
        self->val = v
    ;

    fn __le__(self, other) :
        give self->val <= other->val
    ;

    fn __ge__(self, other) :
        give self->val >= other->val
    ;
;

a = Num(5)
b = Num(5)
c = Num(3)
print(a <= b)
print(a >= c)
print(c >= a)
)XEL");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], "true");
        XASSERT_EQ(out[1], "true");
        XASSERT_EQ(out[2], "false"); });
}

// ============================================================================
// __print__ and __str__
// ============================================================================

void testPrintStr()
{
    std::cout << "\n===== __print__ and __str__ =====\n";

    runTest("__print__ controls print output", []()
            {
        auto out = runXell(R"XEL(
class Point :
    x = 0
    y = 0

    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;

    fn __print__(self) :
        give "({self->x}, {self->y})"
    ;
;

p = Point(3, 4)
print(p)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "(3, 4)"); });

    runTest("__str__ controls string interpolation", []()
            {
        auto out = runXell(R"XEL(
class Name :
    val = ""

    fn __init__(self, v) :
        self->val = v
    ;

    fn __str__(self) :
        give self->val
    ;
;

n = Name("Alice")
print("Hello, {n}!")
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "Hello, Alice!"); });

    runTest("__str__ fallback for print when no __print__", []()
            {
        auto out = runXell(R"XEL(
class Tag :
    label = ""

    fn __init__(self, lbl) :
        self->label = lbl
    ;

    fn __str__(self) :
        give self->label
    ;
;

t = Tag("important")
print(t)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "important"); });

    runTest("__print__ fallback for interpolation when no __str__", []()
            {
        auto out = runXell(R"XEL(
class Wrap :
    val = 0

    fn __init__(self, v) :
        self->val = v
    ;

    fn __print__(self) :
        give "W({self->val})"
    ;
;

w = Wrap(42)
print("value: {w}")
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "value: W(42)"); });
}

// ============================================================================
// __neg__ (unary negation)
// ============================================================================

void testUnaryNeg()
{
    std::cout << "\n===== __neg__ Unary Negation =====\n";

    runTest("__neg__: -obj", []()
            {
        auto out = runXell(R"XEL(
class Vec :
    x = 0
    y = 0

    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;

    fn __neg__(self) :
        give Vec(0 - self->x, 0 - self->y)
    ;
;

v = Vec(3, 4)
n = -v
print(n->x)
print(n->y)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "-3");
        XASSERT_EQ(out[1], "-4"); });
}

// ============================================================================
// __len__
// ============================================================================

void testLen()
{
    std::cout << "\n===== __len__ =====\n";

    runTest("__len__: len(obj)", []()
            {
        auto out = runXell(R"XEL(
class MyList :
    items = []

    fn __init__(self, items) :
        self->items = items
    ;

    fn __len__(self) :
        give len(self->items)
    ;
;

ml = MyList([1, 2, 3, 4, 5])
print(len(ml))
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "5"); });
}

// ============================================================================
// __get__ and __set__ (index access/assignment)
// ============================================================================

void testGetSet()
{
    std::cout << "\n===== __get__ and __set__ =====\n";

    runTest("__get__: obj[key]", []()
            {
        auto out = runXell(R"XEL(
class Matrix :
    data = []

    fn __init__(self, data) :
        self->data = data
    ;

    fn __get__(self, idx) :
        give self->data[idx]
    ;
;

m = Matrix([10, 20, 30])
print(m[0])
print(m[2])
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "10");
        XASSERT_EQ(out[1], "30"); });

    runTest("__set__: obj[key] = val", []()
            {
        auto out = runXell(R"XEL(
class Box :
    items = {}

    fn __init__(self) :
        self->items = {}
    ;

    fn __set__(self, key, val) :
        self->items[key] = val
    ;

    fn __get__(self, key) :
        give self->items[key]
    ;
;

b = Box()
b["x"] = 42
b["y"] = 99
print(b["x"])
print(b["y"])
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "42");
        XASSERT_EQ(out[1], "99"); });
}

// ============================================================================
// __call__
// ============================================================================

void testCall()
{
    std::cout << "\n===== __call__ =====\n";

    runTest("__call__: obj(args)", []()
            {
        auto out = runXell(R"XEL(
class Adder :
    base = 0

    fn __init__(self, base) :
        self->base = base
    ;

    fn __call__(self, x) :
        give self->base + x
    ;
;

add5 = Adder(5)
print(add5(10))
print(add5(20))
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "15");
        XASSERT_EQ(out[1], "25"); });
}

// ============================================================================
// __contains__
// ============================================================================

void testContains()
{
    std::cout << "\n===== __contains__ =====\n";

    runTest("__contains__: contains(obj, val)", []()
            {
        auto out = runXell(R"XEL(
class OddSet :
    fn __contains__(self, val) :
        give val % 2 != 0
    ;
;

s = OddSet()
print(contains(s, 3))
print(contains(s, 4))
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "true");
        XASSERT_EQ(out[1], "false"); });
}

// ============================================================================
// Full Vector example from the plan
// ============================================================================

void testVectorExample()
{
    std::cout << "\n===== Full Vector Example =====\n";

    runTest("Vector class with multiple magic methods", []()
            {
        auto out = runXell(R"XEL(
class Vector :
    x = 0
    y = 0

    fn __init__(self, x, y) :
        self->x = x
        self->y = y
    ;

    fn __add__(self, other) :
        give Vector(self->x + other->x, self->y + other->y)
    ;

    fn __sub__(self, other) :
        give Vector(self->x - other->x, self->y - other->y)
    ;

    fn __mul__(self, scalar) :
        give Vector(self->x * scalar, self->y * scalar)
    ;

    fn __eq__(self, other) :
        give self->x == other->x and self->y == other->y
    ;

    fn __print__(self) :
        give "({self->x}, {self->y})"
    ;
;

a = Vector(1, 2)
b = Vector(3, 4)

c = a + b
d = a * 3
print(c)
print(d)
print(a == b)
print(a == Vector(1, 2))
)XEL");
        XASSERT_EQ(out.size(), 4u);
        XASSERT_EQ(out[0], "(4, 6)");
        XASSERT_EQ(out[1], "(3, 6)");
        XASSERT_EQ(out[2], "false");
        XASSERT_EQ(out[3], "true"); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "===== MAGIC METHODS TESTS (Phase 11 OOP) =====\n";

    testArithmeticOps();
    testComparisonOps();
    testPrintStr();
    testUnaryNeg();
    testLen();
    testGetSet();
    testCall();
    testContains();
    testVectorExample();

    std::cout << "\n========================================\n";
    std::cout << "Total: " << (g_passed + g_failed)
              << "  Passed: " << g_passed
              << "  Failed: " << g_failed << "\n";
    return g_failed > 0 ? 1 : 0;
}
