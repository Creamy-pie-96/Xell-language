// =============================================================================
// Struct & Immutability Tests
// =============================================================================
// Tests Phase 1 OOP: struct definitions, instance construction (positional
// and named args), field access/mutation via ->, method calls with self,
// typeof support, equality, frozen instances (~), immutable bindings,
// index assignment, and error cases.
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
// Section 1: Basic Struct Definition & Construction
// ============================================================================

static void testBasicStruct()
{
    std::cout << "\n===== Basic Struct Definition & Construction =====\n";

    runTest("Empty struct definition", []()
            {
        auto out = runXell("struct Empty : ; \n e = Empty() \n print(typeof(e))");
        XASSERT_EQ(out[0], "Empty"); });

    runTest("Struct with defaults", []()
            {
        auto out = runXell("struct Point : \n x = 0 \n y = 0 \n ; \n p = Point() \n print(p)");
        XASSERT_EQ(out[0], "Point(x=0, y=0)"); });

    runTest("Positional construction", []()
            {
        auto out = runXell("struct Point : \n x = 0 \n y = 0 \n ; \n p = Point(3, 7) \n print(p)");
        XASSERT_EQ(out[0], "Point(x=3, y=7)"); });

    runTest("Partial positional (rest defaults)", []()
            {
        auto out = runXell("struct Point : \n x = 0 \n y = 0 \n ; \n p = Point(5) \n print(p)");
        XASSERT_EQ(out[0], "Point(x=5, y=0)"); });

    runTest("Named construction", []()
            {
        auto out = runXell("struct Point : \n x = 0 \n y = 0 \n ; \n p = Point(x: 10, y: 20) \n print(p)");
        XASSERT_EQ(out[0], "Point(x=10, y=20)"); });

    runTest("Named construction (partial)", []()
            {
        auto out = runXell("struct Point : \n x = 0 \n y = 0 \n ; \n p = Point(y: 99) \n print(p)");
        XASSERT_EQ(out[0], "Point(x=0, y=99)"); });

    runTest("Named construction (reversed order)", []()
            {
        auto out = runXell("struct Point : \n x = 0 \n y = 0 \n ; \n p = Point(y: 5, x: 3) \n print(p)");
        XASSERT_EQ(out[0], "Point(x=3, y=5)"); });

    runTest("Struct with different field types", []()
            {
        auto out = runXell(R"(
struct Person :
    name = "unknown"
    age = 0
    active = true
;
p = Person("Alice", 30, false)
print(p)
)");
        XASSERT_EQ(out[0], "Person(name=\"Alice\", age=30, active=false)"); });

    runTest("Multiple struct definitions", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
struct Color : r = 0  g = 0  b = 0 ;
p = Point(1, 2)
c = Color(255, 128, 0)
print(p)
print(c)
)");
        XASSERT_EQ(out[0], "Point(x=1, y=2)");
        XASSERT_EQ(out[1], "Color(r=255, g=128, b=0)"); });
}

// ============================================================================
// Section 2: Field Access & Mutation
// ============================================================================

static void testFieldAccess()
{
    std::cout << "\n===== Field Access & Mutation =====\n";

    runTest("Read field via ->", []()
            {
        auto out = runXell("struct Point : x = 0 y = 0 ; \n p = Point(3, 7) \n print(p->x) \n print(p->y)");
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "7"); });

    runTest("Mutate field via ->", []()
            {
        auto out = runXell("struct Point : x = 0 y = 0 ; \n p = Point(3, 7) \n p->x = 42 \n print(p->x)");
        XASSERT_EQ(out[0], "42"); });

    runTest("Mutate multiple fields", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
p = Point(1, 2)
p->x = 10
p->y = 20
print(p)
)");
        XASSERT_EQ(out[0], "Point(x=10, y=20)"); });

    runTest("Field mutation persists", []()
            {
        auto out = runXell(R"(
struct Counter : val = 0 ;
c = Counter()
c->val = c->val + 1
c->val = c->val + 1
c->val = c->val + 1
print(c->val)
)");
        XASSERT_EQ(out[0], "3"); });

    runTest("Augmented member assignment +=", []()
            {
        auto out = runXell(R"(
struct Counter : val = 0 ;
c = Counter(10)
c->val += 5
print(c->val)
)");
        XASSERT_EQ(out[0], "15"); });

    runTest("Augmented member assignment -=", []()
            {
        auto out = runXell(R"(
struct Counter : val = 100 ;
c = Counter()
c->val -= 30
print(c->val)
)");
        XASSERT_EQ(out[0], "70"); });

    runTest("Access non-existent field throws AttributeError", []()
            { XASSERT(expectError<AttributeError>(
                  "struct Point : x = 0 y = 0 ; \n p = Point() \n print(p->z)")); });

    runTest("Set non-existent field throws AttributeError", []()
            { XASSERT(expectError<AttributeError>(
                  "struct Point : x = 0 y = 0 ; \n p = Point() \n p->z = 42")); });
}

// ============================================================================
// Section 3: Methods
// ============================================================================

static void testMethods()
{
    std::cout << "\n===== Methods =====\n";

    runTest("Simple method with self", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0
    fn get_x(self) : give self->x ;
;
p = Point(42, 99)
print(p->get_x())
)");
        XASSERT_EQ(out[0], "42"); });

    runTest("Method mutating self", []()
            {
        auto out = runXell(R"(
struct Counter : val = 0
    fn increment(self) : self->val = self->val + 1 ;
;
c = Counter()
c->increment()
c->increment()
c->increment()
print(c->val)
)");
        XASSERT_EQ(out[0], "3"); });

    runTest("Method with extra parameters", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0
    fn translate(self, dx, dy) :
        self->x = self->x + dx
        self->y = self->y + dy
    ;
;
p = Point(1, 2)
p->translate(10, 20)
print(p)
)");
        XASSERT_EQ(out[0], "Point(x=11, y=22)"); });

    runTest("Method returning a value", []()
            {
        auto out = runXell(R"(
struct Rect : w = 0  h = 0
    fn area(self) : give self->w * self->h ;
;
r = Rect(5, 3)
print(r->area())
)");
        XASSERT_EQ(out[0], "15"); });

    runTest("Method calling another method", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0
    fn mag_sq(self) : give self->x * self->x + self->y * self->y ;
    fn magnitude(self) : give sqrt(self->mag_sq()) ;
;
p = Point(3, 4)
print(p->magnitude())
)");
        XASSERT_EQ(out[0], "5"); });

    runTest("Method with two instances", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0
    fn add(self, other) :
        give Point(self->x + other->x, self->y + other->y)
    ;
;
p1 = Point(1, 2)
p2 = Point(3, 4)
p3 = p1->add(p2)
print(p3)
)");
        XASSERT_EQ(out[0], "Point(x=4, y=6)"); });

    runTest("Multiple methods", []()
            {
        auto out = runXell(R"XEL(
struct Vec2 : x = 0  y = 0
    fn dot(self, other) : give self->x * other->x + self->y * other->y ;
    fn scale(self, s) : give Vec2(self->x * s, self->y * s) ;
    fn to_str(self) : give "(" + str(self->x) + "," + str(self->y) + ")" ;
;
v = Vec2(3, 4)
print(v->dot(Vec2(1, 0)))
print(v->scale(2))
print(v->to_str())
)XEL");
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "Vec2(x=6, y=8)");
        XASSERT_EQ(out[2], "(3,4)"); });
}

// ============================================================================
// Section 4: typeof Support
// ============================================================================

static void testTypeof()
{
    std::cout << "\n===== typeof Support =====\n";

    runTest("typeof returns struct name", []()
            {
        auto out = runXell("struct Dog : name = \"\" ; \n d = Dog(\"Rex\") \n print(typeof(d))");
        XASSERT_EQ(out[0], "Dog"); });

    runTest("type also returns struct name", []()
            {
        auto out = runXell("struct Cat : ; \n c = Cat() \n print(type(c))");
        XASSERT_EQ(out[0], "Cat"); });

    runTest("typeof on struct definition returns 'struct'", []()
            {
        auto out = runXell("struct Foo : ; \n print(typeof(Foo))");
        XASSERT_EQ(out[0], "struct"); });

    runTest("Different structs have different type names", []()
            {
        auto out = runXell(R"(
struct A : ;
struct B : ;
a = A()
b = B()
print(typeof(a))
print(typeof(b))
print(typeof(a) == typeof(b))
)");
        XASSERT_EQ(out[0], "A");
        XASSERT_EQ(out[1], "B");
        XASSERT_EQ(out[2], "false"); });
}

// ============================================================================
// Section 5: Equality
// ============================================================================

static void testEquality()
{
    std::cout << "\n===== Equality =====\n";

    runTest("Same fields are equal", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
p1 = Point(3, 7)
p2 = Point(3, 7)
print(p1 == p2)
)");
        XASSERT_EQ(out[0], "true"); });

    runTest("Different fields are not equal", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
p1 = Point(3, 7)
p2 = Point(3, 8)
print(p1 == p2)
)");
        XASSERT_EQ(out[0], "false"); });

    runTest("Identity equality", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
p = Point(1, 2)
print(p == p)
)");
        XASSERT_EQ(out[0], "true"); });

    runTest("Inequality operator", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
p1 = Point(1, 2)
p2 = Point(3, 4)
print(p1 != p2)
)");
        XASSERT_EQ(out[0], "true"); });

    runTest("Default instances are equal", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
print(Point() == Point())
)");
        XASSERT_EQ(out[0], "true"); });
}

// ============================================================================
// Section 6: Frozen Instances (~)
// ============================================================================

static void testFrozenInstances()
{
    std::cout << "\n===== Frozen Instances (~) =====\n";

    runTest("Frozen instance creation", []()
            {
        auto out = runXell("struct Point : x = 0 y = 0 ; \n p = ~Point(3, 7) \n print(p)");
        XASSERT_EQ(out[0], "Point(x=3, y=7)"); });

    runTest("Frozen instance field read works", []()
            {
        auto out = runXell("struct Point : x = 0 y = 0 ; \n p = ~Point(3, 7) \n print(p->x)");
        XASSERT_EQ(out[0], "3"); });

    runTest("Frozen instance field mutation throws", []()
            { XASSERT(expectError<ImmutabilityError>(
                  "struct Point : x = 0 y = 0 ; \n p = ~Point(3, 7) \n p->x = 42")); });

    runTest("Frozen instance with named args", []()
            {
        auto out = runXell("struct Point : x = 0 y = 0 ; \n p = ~Point(y: 99) \n print(p)");
        XASSERT_EQ(out[0], "Point(x=0, y=99)"); });

    runTest("Frozen instance method read works", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0
    fn mag_sq(self) : give self->x * self->x + self->y * self->y ;
;
p = ~Point(3, 4)
print(p->mag_sq())
)");
        XASSERT_EQ(out[0], "25"); });

    runTest("Frozen instance method mutation throws", []()
            { XASSERT(expectError<ImmutabilityError>(R"(
struct Counter : val = 0
    fn inc(self) : self->val = self->val + 1 ;
;
c = ~Counter(5)
c->inc()
)")); });

    runTest("Frozen and mutable coexist", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
p1 = Point(1, 2)
p2 = ~Point(3, 4)
p1->x = 99
print(p1)
print(p2)
)");
        XASSERT_EQ(out[0], "Point(x=99, y=2)");
        XASSERT_EQ(out[1], "Point(x=3, y=4)"); });

    runTest("Frozen instance typeof", []()
            {
        auto out = runXell("struct Pt : x = 0 ; \n p = ~Pt(5) \n print(typeof(p))");
        XASSERT_EQ(out[0], "Pt"); });
}

// ============================================================================
// Section 7: Immutable Bindings
// ============================================================================

static void testImmutableBindings()
{
    std::cout << "\n===== Immutable Bindings =====\n";

    runTest("Immutable binding prevents reassignment", []()
            { XASSERT(expectError<ImmutabilityError>(
                  "immutable x = 42 \n x = 100")); });

    runTest("Immutable binding value is accessible", []()
            {
        auto out = runXell("immutable x = 42 \n print(x)");
        XASSERT_EQ(out[0], "42"); });

    runTest("Immutable struct binding allows field mutation", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
immutable p = Point(3, 7)
p->x = 42
print(p)
)");
        XASSERT_EQ(out[0], "Point(x=42, y=7)"); });

    runTest("Immutable struct binding prevents reassignment", []()
            { XASSERT(expectError<ImmutabilityError>(R"(
struct Point : x = 0  y = 0 ;
immutable p = Point(3, 7)
p = Point(10, 20)
)")); });

    runTest("Immutable + frozen: no field mutation, no reassignment", []()
            { XASSERT(expectError<ImmutabilityError>(R"(
struct Point : x = 0  y = 0 ;
immutable p = ~Point(3, 7)
p->x = 42
)")); });

    runTest("Immutable with string", []()
            {
        auto out = runXell("immutable msg = \"hello\" \n print(msg)");
        XASSERT_EQ(out[0], "hello"); });

    runTest("Immutable with list", []()
            { XASSERT(expectError<ImmutabilityError>(
                  "immutable nums = [1, 2, 3] \n nums = [4, 5, 6]")); });

    runTest("Multiple immutable variables", []()
            {
        auto out = runXell(R"(
immutable a = 10
immutable b = 20
print(a + b)
)");
        XASSERT_EQ(out[0], "30"); });
}

// ============================================================================
// Section 8: Index Assignment
// ============================================================================

static void testIndexAssignment()
{
    std::cout << "\n===== Index Assignment =====\n";

    runTest("List index assignment", []()
            {
        auto out = runXell("xs = [1, 2, 3] \n xs[0] = 10 \n print(xs)");
        XASSERT_EQ(out[0], "[10, 2, 3]"); });

    runTest("List negative index assignment", []()
            {
        auto out = runXell("xs = [1, 2, 3] \n xs[-1] = 99 \n print(xs)");
        XASSERT_EQ(out[0], "[1, 2, 99]"); });

    runTest("Map index assignment (new key)", []()
            {
        auto out = runXell(R"(
m = {"a": 1}
m["b"] = 2
print(m["b"])
)");
        XASSERT_EQ(out[0], "2"); });

    runTest("Map index assignment (overwrite)", []()
            {
        auto out = runXell(R"(
m = {"x": 10}
m["x"] = 42
print(m["x"])
)");
        XASSERT_EQ(out[0], "42"); });

    runTest("Index out of range throws", []()
            { XASSERT(expectError<IndexError>(
                  "xs = [1, 2, 3] \n xs[10] = 99")); });

    runTest("Index assignment on string throws", []()
            { XASSERT(expectError<TypeError>(
                  "s = \"hello\" \n s[0] = \"H\"")); });
}

// ============================================================================
// Section 9: Edge Cases & Error Handling
// ============================================================================

static void testEdgeCases()
{
    std::cout << "\n===== Edge Cases & Error Handling =====\n";

    runTest("Too many positional args throws ArityError", []()
            { XASSERT(expectError<ArityError>(
                  "struct Point : x = 0 y = 0 ; \n Point(1, 2, 3)")); });

    runTest("Named arg for non-existent field throws", []()
            { XASSERT(expectError<AttributeError>(
                  "struct Point : x = 0 y = 0 ; \n Point(z: 42)")); });

    runTest("Struct used in expressions", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
p = Point(3, 4)
result = p->x + p->y
print(result)
)");
        XASSERT_EQ(out[0], "7"); });

    runTest("Instance as function argument", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
fn show(p) : print("x=" + str(p->x) + " y=" + str(p->y)) ;
p = Point(3, 7)
show(p)
)");
        XASSERT_EQ(out[0], "x=3 y=7"); });

    runTest("Instance in list", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
pts = [Point(1, 2), Point(3, 4)]
print(pts[0]->x)
print(pts[1]->y)
)");
        XASSERT_EQ(out[0], "1");
        XASSERT_EQ(out[1], "4"); });

    runTest("Instance passed by reference semantics", []()
            {
        auto out = runXell(R"(
struct Box : val = 0 ;
fn set_val(b, v) : b->val = v ;
box = Box(10)
set_val(box, 99)
print(box->val)
)");
        XASSERT_EQ(out[0], "99"); });

    runTest("Struct truthiness", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
p = Point()
if p : print("truthy") ;
)");
        XASSERT_EQ(out[0], "truthy"); });

    runTest("Member access on struct def throws", []()
            { XASSERT(expectError<AttributeError>(
                  "struct Point : x = 0 y = 0 ; \n print(Point->x)")); });

    runTest("toString for struct definition", []()
            {
        auto out = runXell("struct Foo : ; \n print(Foo)");
        XASSERT_EQ(out[0], "<struct Foo>"); });

    runTest("Struct with complex default values", []()
            {
        auto out = runXell(R"(
struct Config :
    name = "default"
    values = [1, 2, 3]
    active = true
;
c = Config()
print(c->name)
print(c->values)
print(c->active)
)");
        XASSERT_EQ(out[0], "default");
        XASSERT_EQ(out[1], "[1, 2, 3]");
        XASSERT_EQ(out[2], "true"); });
}

// ============================================================================
// Section 10: Struct Instances in Control Flow
// ============================================================================

static void testControlFlow()
{
    std::cout << "\n===== Struct Instances in Control Flow =====\n";

    runTest("Struct in for loop", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
pts = [Point(1, 1), Point(2, 2), Point(3, 3)]
total = 0
for p in pts :
    total = total + p->x + p->y
;
print(total)
)");
        XASSERT_EQ(out[0], "12"); });

    runTest("Struct in if condition field check", []()
            {
        auto out = runXell(R"(
struct User : name = ""  admin = false ;
u = User("Alice", true)
if u->admin :
    print("admin")
else :
    print("user")
;
)");
        XASSERT_EQ(out[0], "admin"); });

    runTest("Struct method in while loop", []()
            {
        auto out = runXell(R"(
struct Counter : val = 0
    fn inc(self) : self->val = self->val + 1 ;
    fn get(self) : give self->val ;
;
c = Counter()
while c->get() < 5 :
    c->inc()
;
print(c->val)
)");
        XASSERT_EQ(out[0], "5"); });

    runTest("Factory function returning struct", []()
            {
        auto out = runXell(R"(
struct Point : x = 0  y = 0 ;
fn origin() : give Point(0, 0) ;
fn unit_x() : give Point(1, 0) ;
print(origin())
print(unit_x())
)");
        XASSERT_EQ(out[0], "Point(x=0, y=0)");
        XASSERT_EQ(out[1], "Point(x=1, y=0)"); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "========================================\n";
    std::cout << "   Struct & Immutability Test Suite\n";
    std::cout << "========================================\n";

    testBasicStruct();
    testFieldAccess();
    testMethods();
    testTypeof();
    testEquality();
    testFrozenInstances();
    testImmutableBindings();
    testIndexAssignment();
    testEdgeCases();
    testControlFlow();

    std::cout << "\n========================================\n";
    std::cout << "   Results: " << g_passed << " passed, " << g_failed << " failed\n";
    std::cout << "========================================\n";

    return g_failed > 0 ? 1 : 0;
}
