// =============================================================================
// Type Casting & Function Overloading Tests
// =============================================================================
// Tests all new type-casting builtins (Int, Float, String, Complex, Bool,
// List, Tuple, Set, iSet, ~List, ~Tuple, ~Set, ~iSet, number, auto) and
// function overloading (count-based + type-based).
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
// Section 1: Int() Conversion
// ============================================================================

static void testIntConversion()
{
    std::cout << "\n===== Int() Conversion =====\n";

    runTest("Int from float", []()
            {
        auto out = runXell("print(Int(3.7))");
        XASSERT_EQ(out[0], "3"); });

    runTest("Int from string", []()
            {
        auto out = runXell("print(Int(\"42\"))");
        XASSERT_EQ(out[0], "42"); });

    runTest("Int from bool true", []()
            {
        auto out = runXell("print(Int(true))");
        XASSERT_EQ(out[0], "1"); });

    runTest("Int from bool false", []()
            {
        auto out = runXell("print(Int(false))");
        XASSERT_EQ(out[0], "0"); });

    runTest("Int from int (passthrough)", []()
            {
        auto out = runXell("print(Int(42))");
        XASSERT_EQ(out[0], "42"); });

    runTest("Int from complex (real only)", []()
            {
        auto out = runXell("print(Int(Complex(5, 0)))");
        XASSERT_EQ(out[0], "5"); });

    runTest("Int from invalid string → error", []()
            { XASSERT(expectError<ConversionError>("Int(\"hello\")")); });

    runTest("Int from complex with imag → error", []()
            { XASSERT(expectError<ConversionError>("Int(Complex(1, 2))")); });
}

// ============================================================================
// Section 2: Float() Conversion
// ============================================================================

static void testFloatConversion()
{
    std::cout << "\n===== Float() Conversion =====\n";

    runTest("Float from int", []()
            {
        auto out = runXell("print(Float(42))");
        XASSERT_EQ(out[0], "42"); });

    runTest("Float from string", []()
            {
        auto out = runXell("print(Float(\"3.14\"))");
        XASSERT_EQ(out[0], "3.14"); });

    runTest("Float from bool", []()
            {
        auto out = runXell("print(Float(true))");
        XASSERT_EQ(out[0], "1"); });

    runTest("Float from float (passthrough)", []()
            {
        auto out = runXell("print(Float(2.5))");
        XASSERT_EQ(out[0], "2.5"); });

    runTest("Float from invalid string → error", []()
            { XASSERT(expectError<ConversionError>("Float(\"abc\")")); });
}

// ============================================================================
// Section 3: String() Conversion
// ============================================================================

static void testStringConversion()
{
    std::cout << "\n===== String() Conversion =====\n";

    runTest("String from int", []()
            {
        auto out = runXell("print(String(42))");
        XASSERT_EQ(out[0], "42"); });

    runTest("String from float", []()
            {
        auto out = runXell("print(String(3.14))");
        XASSERT_EQ(out[0], "3.14"); });

    runTest("String from bool", []()
            {
        auto out = runXell("print(String(true))");
        XASSERT_EQ(out[0], "true"); });

    runTest("String from list", []()
            {
        auto out = runXell("print(String([1, 2, 3]))");
        XASSERT_EQ(out[0], "[1, 2, 3]"); });

    runTest("String from none", []()
            {
        auto out = runXell("print(String(none))");
        XASSERT_EQ(out[0], "none"); });
}

// ============================================================================
// Section 4: Complex() Conversion
// ============================================================================

static void testComplexConversion()
{
    std::cout << "\n===== Complex() Conversion =====\n";

    runTest("Complex from int", []()
            {
        auto out = runXell("print(Complex(5))");
        XASSERT_EQ(out[0], "(5+0i)"); });

    runTest("Complex from two args", []()
            {
        auto out = runXell("print(Complex(3, 4))");
        XASSERT_EQ(out[0], "(3+4i)"); });

    runTest("Complex from float", []()
            {
        auto out = runXell("print(Complex(2.5))");
        XASSERT_EQ(out[0], "(2.5+0i)"); });
}

// ============================================================================
// Section 5: Bool() Conversion
// ============================================================================

static void testBoolConversion()
{
    std::cout << "\n===== Bool() Conversion =====\n";

    runTest("Bool from int 0", []()
            {
        auto out = runXell("print(Bool(0))");
        XASSERT_EQ(out[0], "false"); });

    runTest("Bool from int 1", []()
            {
        auto out = runXell("print(Bool(1))");
        XASSERT_EQ(out[0], "true"); });

    runTest("Bool from empty string", []()
            {
        auto out = runXell("print(Bool(\"\"))");
        XASSERT_EQ(out[0], "false"); });

    runTest("Bool from non-empty string", []()
            {
        auto out = runXell("print(Bool(\"hello\"))");
        XASSERT_EQ(out[0], "true"); });

    runTest("Bool from empty list", []()
            {
        auto out = runXell("print(Bool([]))");
        XASSERT_EQ(out[0], "false"); });

    runTest("Bool from non-empty list", []()
            {
        auto out = runXell("print(Bool([1]))");
        XASSERT_EQ(out[0], "true"); });

    runTest("Bool from none", []()
            {
        auto out = runXell("print(Bool(none))");
        XASSERT_EQ(out[0], "false"); });
}

// ============================================================================
// Section 6: number() and auto() Conversion
// ============================================================================

static void testNumberAutoConversion()
{
    std::cout << "\n===== number() and auto() Conversion =====\n";

    runTest("number from string int", []()
            {
        auto out = runXell("print(number(\"42\"))");
        XASSERT_EQ(out[0], "42"); });

    runTest("number from string float", []()
            {
        auto out = runXell("print(number(\"3.14\"))");
        XASSERT_EQ(out[0], "3.14"); });

    runTest("number from bool", []()
            {
        auto out = runXell("print(number(true))");
        XASSERT_EQ(out[0], "1"); });

    runTest("number from invalid string → error", []()
            { XASSERT(expectError<ConversionError>("number(\"hello\")")); });

    runTest("auto from string int", []()
            {
        auto out = runXell("x = auto(\"42\")\nprint(type(x))");
        XASSERT_EQ(out[0], "int"); });

    runTest("auto from string float", []()
            {
        auto out = runXell("x = auto(\"3.14\")\nprint(type(x))");
        XASSERT_EQ(out[0], "float"); });

    runTest("auto from string bool", []()
            {
        auto out = runXell("x = auto(\"true\")\nprint(type(x))");
        XASSERT_EQ(out[0], "bool"); });

    runTest("auto from string none", []()
            {
        auto out = runXell("x = auto(\"none\")\nprint(type(x))");
        XASSERT_EQ(out[0], "none"); });

    runTest("auto from non-string passthrough", []()
            {
        auto out = runXell("x = auto(42)\nprint(type(x))");
        XASSERT_EQ(out[0], "int"); });

    runTest("auto from plain string stays string", []()
            {
        auto out = runXell("x = auto(\"hello\")\nprint(type(x))");
        XASSERT_EQ(out[0], "string"); });
}

// ============================================================================
// Section 7: List() Conversion
// ============================================================================

static void testListConversion()
{
    std::cout << "\n===== List() Conversion =====\n";

    runTest("List from tuple", []()
            {
        auto out = runXell("t = (1, 2, 3)\nprint(List(t))");
        XASSERT_EQ(out[0], "[1, 2, 3]"); });

    runTest("List from set", []()
            {
        auto out = runXell("s = {42}\nresult = List(s)\nprint(len(result))");
        XASSERT_EQ(out[0], "1"); });

    runTest("List from string (comma separated)", []()
            {
        auto out = runXell("print(List(\"a,b,c\"))");
        XASSERT_EQ(out[0], "[\"a\", \"b\", \"c\"]"); });

    runTest("List from number (wraps in list)", []()
            {
        auto out = runXell("print(List(42))");
        XASSERT_EQ(out[0], "[42]"); });

    runTest("List from list passthrough", []()
            {
        auto out = runXell("print(List([1, 2]))");
        XASSERT_EQ(out[0], "[1, 2]"); });
}

// ============================================================================
// Section 8: Tuple() Conversion
// ============================================================================

static void testTupleConversion()
{
    std::cout << "\n===== Tuple() Conversion =====\n";

    runTest("Tuple from list", []()
            {
        auto out = runXell("print(Tuple([1, 2, 3]))");
        XASSERT_EQ(out[0], "(1, 2, 3)"); });

    runTest("Tuple from string", []()
            {
        auto out = runXell("print(Tuple(\"x,y,z\"))");
        XASSERT_EQ(out[0], "(\"x\", \"y\", \"z\")"); });

    runTest("Tuple from number", []()
            {
        auto out = runXell("print(Tuple(99))");
        XASSERT_EQ(out[0], "(99,)"); });
}

// ============================================================================
// Section 9: Set() and iSet() Conversion
// ============================================================================

static void testSetConversion()
{
    std::cout << "\n===== Set() and iSet() Conversion =====\n";

    runTest("Set from list", []()
            {
        auto out = runXell("s = Set([1, 2, 2, 3])\nprint(len(s))");
        XASSERT_EQ(out[0], "3"); });

    runTest("Set from tuple", []()
            {
        auto out = runXell("s = Set((1, 2, 3))\nprint(len(s))");
        XASSERT_EQ(out[0], "3"); });

    runTest("Set from string", []()
            {
        auto out = runXell("s = Set(\"a,b,c\")\nprint(len(s))");
        XASSERT_EQ(out[0], "3"); });

    runTest("iSet from list (frozen)", []()
            {
        auto out = runXell("s = iSet([1, 2, 3])\nprint(type(s))");
        XASSERT_EQ(out[0], "frozen_set"); });

    runTest("iSet from list (immutable)", []()
            { XASSERT(expectError<ImmutabilityError>(
                  "s = iSet([1, 2, 3])\nadd(s, 4)")); });
}

// ============================================================================
// Section 10: ~List() Smart-Cast Conversion
// ============================================================================

static void testSmartListConversion()
{
    std::cout << "\n===== ~List() Smart-Cast =====\n";

    runTest("~List from string with mixed types", []()
            {
        auto out = runXell(R"(
            result = ~List("42,3.14,true,hello")
            print(type(result[0]))
            print(type(result[1]))
            print(type(result[2]))
            print(type(result[3]))
        )");
        XASSERT_EQ(out[0], "int");
        XASSERT_EQ(out[1], "float");
        XASSERT_EQ(out[2], "bool");
        XASSERT_EQ(out[3], "string"); });

    runTest("~List values are correct", []()
            {
        auto out = runXell(R"(
            result = ~List("42,3.14,true,hello")
            print(result[0])
            print(result[1])
            print(result[2])
            print(result[3])
        )");
        XASSERT_EQ(out[0], "42");
        XASSERT_EQ(out[1], "3.14");
        XASSERT_EQ(out[2], "true");
        XASSERT_EQ(out[3], "hello"); });

    runTest("~List with none detection", []()
            {
        auto out = runXell(R"(
            result = ~List("none,false,0")
            print(type(result[0]))
            print(type(result[1]))
            print(type(result[2]))
        )");
        XASSERT_EQ(out[0], "none");
        XASSERT_EQ(out[1], "bool");
        XASSERT_EQ(out[2], "int"); });

    runTest("~List with nested list in string", []()
            {
        auto out = runXell(R"(
            result = ~List("[1,2,3],hello")
            print(type(result[0]))
            print(result[0])
            print(result[1])
        )");
        XASSERT_EQ(out[0], "list");
        XASSERT_EQ(out[1], "[1, 2, 3]");
        XASSERT_EQ(out[2], "hello"); });

    runTest("~List from existing list auto-casts string elements", []()
            {
        auto out = runXell(R"(
            original = ["42", "true", "hello"]
            result = ~List(original)
            print(type(result[0]))
            print(type(result[1]))
            print(type(result[2]))
        )");
        XASSERT_EQ(out[0], "int");
        XASSERT_EQ(out[1], "bool");
        XASSERT_EQ(out[2], "string"); });

    runTest("regular List from same string keeps strings", []()
            {
        auto out = runXell(R"(
            result = List("42,true,hello")
            print(type(result[0]))
            print(type(result[1]))
            print(type(result[2]))
        )");
        XASSERT_EQ(out[0], "string");
        XASSERT_EQ(out[1], "string");
        XASSERT_EQ(out[2], "string"); });
}

// ============================================================================
// Section 11: ~Tuple(), ~Set(), ~iSet() Smart-Cast
// ============================================================================

static void testSmartOtherContainers()
{
    std::cout << "\n===== ~Tuple(), ~Set(), ~iSet() Smart-Cast =====\n";

    runTest("~Tuple from string", []()
            {
        auto out = runXell(R"(
            result = ~Tuple("42,hello,true")
            print(type(result[0]))
            print(type(result[1]))
            print(type(result[2]))
        )");
        XASSERT_EQ(out[0], "int");
        XASSERT_EQ(out[1], "string");
        XASSERT_EQ(out[2], "bool"); });

    runTest("~Set from string", []()
            {
        auto out = runXell(R"(
            result = ~Set("1,2,3")
            print(len(result))
            print(type(result))
        )");
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "set"); });

    runTest("~iSet from string", []()
            {
        auto out = runXell(R"(
            result = ~iSet("1,2,3")
            print(len(result))
            print(type(result))
        )");
        XASSERT_EQ(out[0], "3");
        XASSERT_EQ(out[1], "frozen_set"); });
}

// ============================================================================
// Section 12: Count-Based Function Overloading
// ============================================================================

static void testCountOverloading()
{
    std::cout << "\n===== Count-Based Overloading =====\n";

    runTest("overload: 0, 1, 2 params", []()
            {
        auto out = runXell(R"(
            fn greet() :
                print("Hello stranger")
            ;
            fn greet(name) :
                print("Hello {name}")
            ;
            fn greet(name, title) :
                print("Hello {title} {name}")
            ;
            greet()
            greet("Prithu")
            greet("Prithu", "Mr")
        )");
        XASSERT_EQ(out[0], "Hello stranger");
        XASSERT_EQ(out[1], "Hello Prithu");
        XASSERT_EQ(out[2], "Hello Mr Prithu"); });

    runTest("overload: different return values", []()
            {
        auto out = runXell(R"(
            fn compute() :
                give 0
            ;
            fn compute(x) :
                give x * 2
            ;
            fn compute(x, y) :
                give x + y
            ;
            print(compute())
            print(compute(5))
            print(compute(3, 4))
        )");
        XASSERT_EQ(out[0], "0");
        XASSERT_EQ(out[1], "10");
        XASSERT_EQ(out[2], "7"); });

    runTest("overload: wrong arity → error", []()
            { XASSERT(expectError<TypeError>(R"(
            fn foo() : print("zero") ;
            fn foo(x) : print("one") ;
            foo(1, 2, 3)
        )")); });
}

// ============================================================================
// Section 13: Type-Based Function Overloading
// ============================================================================

static void testTypeOverloading()
{
    std::cout << "\n===== Type-Based Overloading =====\n";

    runTest("type overload: str vs int", []()
            {
        auto out = runXell(R"(
            fn process(str(name)) :
                print("Name: {name}")
            ;
            fn process(int(age)) :
                print("Age: {age}")
            ;
            process("Prithu")
            process(22)
        )");
        XASSERT_EQ(out[0], "Name: Prithu");
        XASSERT_EQ(out[1], "Age: 22"); });

    runTest("type overload: str vs int vs bool", []()
            {
        auto out = runXell(R"(
            fn check(str(x)) :
                print("string")
            ;
            fn check(int(x)) :
                print("int")
            ;
            fn check(bool(x)) :
                print("bool")
            ;
            check("hello")
            check(42)
            check(true)
        )");
        XASSERT_EQ(out[0], "string");
        XASSERT_EQ(out[1], "int");
        XASSERT_EQ(out[2], "bool"); });

    runTest("type overload: no match → error", []()
            { XASSERT(expectError<TypeError>(R"(
            fn handler(str(x)) :
                print("string")
            ;
            fn handler(int(x)) :
                print("int")
            ;
            handler([1, 2, 3])
        )")); });
}

// ============================================================================
// Section 14: Mixed Count + Type Overloading
// ============================================================================

static void testMixedOverloading()
{
    std::cout << "\n===== Mixed Count + Type Overloading =====\n";

    runTest("mixed: count + type", []()
            {
        auto out = runXell(R"(
            fn describe(str(name)) :
                print("Name: {name}")
            ;
            fn describe(int(age)) :
                print("Age: {age}")
            ;
            fn describe(str(name), int(age)) :
                print("{name} is {age}")
            ;
            describe("Alice")
            describe(30)
            describe("Bob", 25)
        )");
        XASSERT_EQ(out[0], "Name: Alice");
        XASSERT_EQ(out[1], "Age: 30");
        XASSERT_EQ(out[2], "Bob is 25"); });

    runTest("mixed: type-specific + dynamic 3-param", []()
            {
        auto out = runXell(R"(
            fn info(str(name)) :
                print("name={name}")
            ;
            fn info(int(id)) :
                print("id={id}")
            ;
            fn info(name, age, city) :
                print("{name},{age},{city}")
            ;
            info("Alice")
            info(42)
            info("Bob", 30, "NYC")
        )");
        XASSERT_EQ(out[0], "name=Alice");
        XASSERT_EQ(out[1], "id=42");
        XASSERT_EQ(out[2], "Bob,30,NYC"); });

    runTest("type overload resolution: type match beats dynamic", []()
            {
        auto out = runXell(R"(
            fn test(str(x)) :
                print("typed-str")
            ;
            fn test(int(x)) :
                print("typed-int")
            ;
            test("hello")
            test(42)
        )");
        XASSERT_EQ(out[0], "typed-str");
        XASSERT_EQ(out[1], "typed-int"); });
}

// ============================================================================
// Section 15: Overloading Edge Cases
// ============================================================================

static void testOverloadEdgeCases()
{
    std::cout << "\n===== Overloading Edge Cases =====\n";

    runTest("redefine function (same arity, no types) → overwrite", []()
            {
        auto out = runXell(R"(
            fn greet(name) :
                print("v1: {name}")
            ;
            fn greet(name) :
                print("v2: {name}")
            ;
            greet("test")
        )");
        XASSERT_EQ(out[0], "v2: test"); });

    runTest("conflict: dynamic + typed at same arity → error", []()
            { XASSERT(expectError<ParseError>(R"(
            fn handler(name) :
                print("dynamic")
            ;
            fn handler(str(name)) :
                print("typed")
            ;
        )")); });

    runTest("overload with two type params", []()
            {
        auto out = runXell(R"(
            fn add(int(a), int(b)) :
                give a + b
            ;
            fn add(str(a), str(b)) :
                give "{a}{b}"
            ;
            print(add(3, 4))
            print(add("foo", "bar"))
        )");
        XASSERT_EQ(out[0], "7");
        XASSERT_EQ(out[1], "foobar"); });

    runTest("overload with list type", []()
            {
        auto out = runXell(R"(
            fn process(list(items)) :
                print("list of {len(items)}")
            ;
            fn process(str(text)) :
                print("text: {text}")
            ;
            process([1, 2, 3])
            process("hello")
        )");
        XASSERT_EQ(out[0], "list of 3");
        XASSERT_EQ(out[1], "text: hello"); });

    runTest("overload with float type", []()
            {
        auto out = runXell(R"(
            fn show(int(x)) :
                print("int: {x}")
            ;
            fn show(float(x)) :
                print("float: {x}")
            ;
            show(42)
            show(3.14)
        )");
        XASSERT_EQ(out[0], "int: 42");
        XASSERT_EQ(out[1], "float: 3.14"); });
}

// ============================================================================
// Section 16: Colon-syntax type annotations (backward compat)
// ============================================================================

static void testColonTypeAnnotations()
{
    std::cout << "\n===== Colon Type Annotations (backward compat) =====\n";

    runTest("colon annotation still works", []()
            {
        auto out = runXell(R"(
            fn greet(name: str) :
                print("Hello {name}")
            ;
            greet("World")
        )");
        XASSERT_EQ(out[0], "Hello World"); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "============================================\n";
    std::cout << "  Type Casting & Overloading Tests\n";
    std::cout << "============================================\n";

    // Type casting
    testIntConversion();
    testFloatConversion();
    testStringConversion();
    testComplexConversion();
    testBoolConversion();
    testNumberAutoConversion();
    testListConversion();
    testTupleConversion();
    testSetConversion();
    testSmartListConversion();
    testSmartOtherContainers();

    // Function overloading
    testCountOverloading();
    testTypeOverloading();
    testMixedOverloading();
    testOverloadEdgeCases();
    testColonTypeAnnotations();

    std::cout << "\n============================================\n";
    std::cout << "  Total: " << (g_passed + g_failed)
              << "  |  Passed: " << g_passed
              << "  |  Failed: " << g_failed << "\n";
    std::cout << "============================================\n";

    return g_failed == 0 ? 0 : 1;
}
