// =============================================================================
// XObject Tests
// =============================================================================
// Verifies the entire XObject value system:
//   - Construction of all types (none, number, bool, string, list, map, function)
//   - Reference counting (copy, assign, move, destroy)
//   - Deep clone independence
//   - Truthiness rules
//   - Equality comparisons
//   - toString representation
//   - XMap ordered operations
//   - Leak detection via liveAllocations()
//   - Edge cases (self-assignment, nested structures, large ref counts)
// =============================================================================

#include "../src/interpreter/xobject.hpp"
#include "../src/parser/ast.hpp"
#include <iostream>
#include <string>
#include <sstream>
#include <functional>
#include <cmath>

using namespace xell;

// ---- Minimal test framework ------------------------------------------------

static int g_passed = 0;
static int g_failed = 0;

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

#define XASSERT_NEAR(a, b, eps)                               \
    do                                                        \
    {                                                         \
        if (std::fabs((a) - (b)) > (eps))                     \
        {                                                     \
            std::ostringstream os;                            \
            os << "Expected [" << (a) << "] ~= [" << (b)      \
               << "] (eps=" << (eps) << ", line " << __LINE__ \
               << ")";                                        \
            throw std::runtime_error(os.str());               \
        }                                                     \
    } while (0)

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
// Helper: scoped leak checker
// ============================================================================

struct LeakGuard
{
    int64_t baseline;
    LeakGuard()
    {
        XObject::resetAllocationCounter();
        baseline = XObject::liveAllocations();
    }
    void check()
    {
        int64_t live = XObject::liveAllocations();
        if (live != baseline)
        {
            std::ostringstream os;
            os << "Memory leak detected: " << (live - baseline) << " XData block(s) still alive";
            throw std::runtime_error(os.str());
        }
    }
};

// ============================================================================
// Section 1: Construction & Type Queries
// ============================================================================

static void testConstruction()
{
    std::cout << "\n===== XObject Construction =====\n";

    runTest("none: default constructor", []()
            {
        LeakGuard lg;
        {
            XObject obj;
            XASSERT(obj.isNone());
            XASSERT_EQ((int)obj.type(), (int)XType::NONE);
        }
        lg.check(); });

    runTest("none: makeNone", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeNone();
            XASSERT(obj.isNone());
            XASSERT(!obj.isNumber());
            XASSERT(!obj.isBool());
            XASSERT(!obj.isString());
        }
        lg.check(); });

    runTest("number: integer", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeNumber(42);
            XASSERT(obj.isNumber());
            XASSERT_NEAR(obj.asNumber(), 42.0, 0.0001);
        }
        lg.check(); });

    runTest("number: float", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeNumber(3.14);
            XASSERT(obj.isNumber());
            XASSERT_NEAR(obj.asNumber(), 3.14, 0.0001);
        }
        lg.check(); });

    runTest("number: zero", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeNumber(0);
            XASSERT(obj.isNumber());
            XASSERT_NEAR(obj.asNumber(), 0.0, 0.0001);
        }
        lg.check(); });

    runTest("number: negative", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeNumber(-99.5);
            XASSERT(obj.isNumber());
            XASSERT_NEAR(obj.asNumber(), -99.5, 0.0001);
        }
        lg.check(); });

    runTest("bool: true", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeBool(true);
            XASSERT(obj.isBool());
            XASSERT(obj.asBool() == true);
        }
        lg.check(); });

    runTest("bool: false", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeBool(false);
            XASSERT(obj.isBool());
            XASSERT(obj.asBool() == false);
        }
        lg.check(); });

    runTest("string: lvalue", []()
            {
        LeakGuard lg;
        {
            std::string s = "hello xell";
            auto obj = XObject::makeString(s);
            XASSERT(obj.isString());
            XASSERT_EQ(obj.asString(), std::string("hello xell"));
        }
        lg.check(); });

    runTest("string: rvalue", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeString(std::string("moved"));
            XASSERT(obj.isString());
            XASSERT_EQ(obj.asString(), std::string("moved"));
        }
        lg.check(); });

    runTest("string: empty", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeString("");
            XASSERT(obj.isString());
            XASSERT_EQ(obj.asString(), std::string(""));
        }
        lg.check(); });

    runTest("list: empty", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeList();
            XASSERT(obj.isList());
            XASSERT_EQ(obj.asList().size(), (size_t)0);
        }
        lg.check(); });

    runTest("list: with elements", []()
            {
        LeakGuard lg;
        {
            XList elems;
            elems.push_back(XObject::makeNumber(1));
            elems.push_back(XObject::makeString("two"));
            elems.push_back(XObject::makeBool(true));
            auto obj = XObject::makeList(std::move(elems));
            XASSERT(obj.isList());
            XASSERT_EQ(obj.asList().size(), (size_t)3);
            XASSERT(obj.asList()[0].isNumber());
            XASSERT(obj.asList()[1].isString());
            XASSERT(obj.asList()[2].isBool());
        }
        lg.check(); });

    runTest("map: empty", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeMap();
            XASSERT(obj.isMap());
            XASSERT_EQ(obj.asMap().size(), (size_t)0);
        }
        lg.check(); });

    runTest("map: with entries", []()
            {
        LeakGuard lg;
        {
            XMap m;
            m.set("host", XObject::makeString("localhost"));
            m.set("port", XObject::makeNumber(3000));
            auto obj = XObject::makeMap(std::move(m));
            XASSERT(obj.isMap());
            XASSERT_EQ(obj.asMap().size(), (size_t)2);
            XASSERT(obj.asMap().has("host"));
            XASSERT(obj.asMap().has("port"));
            XASSERT_EQ(obj.asMap().get("host")->asString(), std::string("localhost"));
            XASSERT_NEAR(obj.asMap().get("port")->asNumber(), 3000.0, 0.001);
        }
        lg.check(); });

    runTest("function", []()
            {
        LeakGuard lg;
        {
            // We don't need a real AST body for construction testing
            auto obj = XObject::makeFunction("greet", {"name", "msg"}, nullptr);
            XASSERT(obj.isFunction());
            XASSERT_EQ(obj.asFunction().name, std::string("greet"));
            XASSERT_EQ(obj.asFunction().params.size(), (size_t)2);
            XASSERT_EQ(obj.asFunction().params[0], std::string("name"));
            XASSERT_EQ(obj.asFunction().params[1], std::string("msg"));
            XASSERT(obj.asFunction().body == nullptr);
        }
        lg.check(); });

    runTest("xtype_name strings", []()
            {
        XASSERT_EQ(std::string(xtype_name(XType::NONE)), std::string("none"));
        XASSERT_EQ(std::string(xtype_name(XType::NUMBER)), std::string("number"));
        XASSERT_EQ(std::string(xtype_name(XType::BOOL)), std::string("bool"));
        XASSERT_EQ(std::string(xtype_name(XType::STRING)), std::string("string"));
        XASSERT_EQ(std::string(xtype_name(XType::LIST)), std::string("list"));
        XASSERT_EQ(std::string(xtype_name(XType::MAP)), std::string("map"));
        XASSERT_EQ(std::string(xtype_name(XType::FUNCTION)), std::string("function")); });
}

// ============================================================================
// Section 2: Reference Counting
// ============================================================================

static void testRefCounting()
{
    std::cout << "\n===== Reference Counting =====\n";

    runTest("initial ref count is 1", []()
            {
        auto obj = XObject::makeNumber(42);
        XASSERT_EQ(obj.refCount(), (uint32_t)1); });

    runTest("copy increments ref count", []()
            {
        LeakGuard lg;
        {
            auto a = XObject::makeString("shared");
            XASSERT_EQ(a.refCount(), (uint32_t)1);
            auto b = a; // copy
            XASSERT_EQ(a.refCount(), (uint32_t)2);
            XASSERT_EQ(b.refCount(), (uint32_t)2);
            // Both point to same data
            XASSERT_EQ(&a.asString(), &b.asString());
        }
        lg.check(); });

    runTest("copy assignment increments ref count", []()
            {
        LeakGuard lg;
        {
            auto a = XObject::makeNumber(1);
            auto b = XObject::makeNumber(2);
            XASSERT_EQ(a.refCount(), (uint32_t)1);
            b = a; // copy assign — old b is released
            XASSERT_EQ(a.refCount(), (uint32_t)2);
            XASSERT_EQ(b.refCount(), (uint32_t)2);
        }
        lg.check(); });

    runTest("move leaves source null (no leak)", []()
            {
        LeakGuard lg;
        {
            auto a = XObject::makeString("moveme");
            auto b = std::move(a);
            XASSERT_EQ(b.refCount(), (uint32_t)1);
            XASSERT(b.isString());
            // a is now in a moved-from state (data_ is nullptr)
            XASSERT_EQ(a.refCount(), (uint32_t)0);
        }
        lg.check(); });

    runTest("move assignment", []()
            {
        LeakGuard lg;
        {
            auto a = XObject::makeNumber(10);
            auto b = XObject::makeNumber(20);
            b = std::move(a);
            XASSERT_EQ(b.refCount(), (uint32_t)1);
            XASSERT_NEAR(b.asNumber(), 10.0, 0.001);
        }
        lg.check(); });

    runTest("ref count drops to zero → freed", []()
            {
        XObject::resetAllocationCounter();
        {
            auto a = XObject::makeNumber(42);
            XASSERT_EQ(XObject::liveAllocations(), (int64_t)1);
            {
                auto b = a; // ref = 2
                XASSERT_EQ(XObject::liveAllocations(), (int64_t)1); // still 1 block
            }
            // b destroyed, ref back to 1
            XASSERT_EQ(a.refCount(), (uint32_t)1);
            XASSERT_EQ(XObject::liveAllocations(), (int64_t)1);
        }
        // a destroyed, ref = 0, block freed
        XASSERT_EQ(XObject::liveAllocations(), (int64_t)0); });

    runTest("many copies then destroy", []()
            {
        LeakGuard lg;
        {
            auto original = XObject::makeString("shared string");
            std::vector<XObject> copies;
            for (int i = 0; i < 100; i++)
            {
                copies.push_back(original);
            }
            XASSERT_EQ(original.refCount(), (uint32_t)101); // 1 + 100 copies
            copies.clear();
            XASSERT_EQ(original.refCount(), (uint32_t)1);
        }
        lg.check(); });

    runTest("self-assignment is safe", []()
            {
        LeakGuard lg;
        {
            auto a = XObject::makeNumber(7);
            a = a; // self-assign
            XASSERT_EQ(a.refCount(), (uint32_t)1);
            XASSERT_NEAR(a.asNumber(), 7.0, 0.001);
        }
        lg.check(); });

    runTest("re-assign releases old value", []()
            {
        XObject::resetAllocationCounter();
        auto a = XObject::makeNumber(1);
        XASSERT_EQ(XObject::liveAllocations(), (int64_t)1);
        a = XObject::makeString("replaced");
        // Old number block freed, new string block allocated
        XASSERT_EQ(XObject::liveAllocations(), (int64_t)1);
        XASSERT(a.isString()); });
}

// ============================================================================
// Section 3: Truthiness
// ============================================================================

static void testTruthiness()
{
    std::cout << "\n===== Truthiness =====\n";

    runTest("none is falsy", []()
            { XASSERT(!XObject::makeNone().truthy()); });

    runTest("false is falsy", []()
            { XASSERT(!XObject::makeBool(false).truthy()); });

    runTest("true is truthy", []()
            { XASSERT(XObject::makeBool(true).truthy()); });

    runTest("0 is falsy", []()
            { XASSERT(!XObject::makeNumber(0).truthy()); });

    runTest("non-zero number is truthy", []()
            { XASSERT(XObject::makeNumber(1).truthy()); });

    runTest("negative number is truthy", []()
            { XASSERT(XObject::makeNumber(-5).truthy()); });

    runTest("empty string is falsy", []()
            { XASSERT(!XObject::makeString("").truthy()); });

    runTest("non-empty string is truthy", []()
            { XASSERT(XObject::makeString("hello").truthy()); });

    runTest("empty list is falsy", []()
            { XASSERT(!XObject::makeList().truthy()); });

    runTest("non-empty list is truthy", []()
            {
        XList elems;
        elems.push_back(XObject::makeNumber(1));
        XASSERT(XObject::makeList(std::move(elems)).truthy()); });

    runTest("empty map is falsy", []()
            { XASSERT(!XObject::makeMap().truthy()); });

    runTest("non-empty map is truthy", []()
            {
        XMap m;
        m.set("key", XObject::makeNone());
        XASSERT(XObject::makeMap(std::move(m)).truthy()); });

    runTest("function is always truthy", []()
            {
        auto fn = XObject::makeFunction("f", {}, nullptr);
        XASSERT(fn.truthy()); });
}

// ============================================================================
// Section 4: toString
// ============================================================================

static void testToString()
{
    std::cout << "\n===== toString =====\n";

    runTest("none → \"none\"", []()
            { XASSERT_EQ(XObject::makeNone().toString(), std::string("none")); });

    runTest("integer number → no decimal", []()
            { XASSERT_EQ(XObject::makeNumber(42).toString(), std::string("42")); });

    runTest("zero → \"0\"", []()
            { XASSERT_EQ(XObject::makeNumber(0).toString(), std::string("0")); });

    runTest("negative integer → \"-5\"", []()
            { XASSERT_EQ(XObject::makeNumber(-5).toString(), std::string("-5")); });

    runTest("float number → has decimal", []()
            {
        std::string s = XObject::makeNumber(3.14).toString();
        XASSERT(s.find("3.14") != std::string::npos); });

    runTest("bool true → \"true\"", []()
            { XASSERT_EQ(XObject::makeBool(true).toString(), std::string("true")); });

    runTest("bool false → \"false\"", []()
            { XASSERT_EQ(XObject::makeBool(false).toString(), std::string("false")); });

    runTest("string → itself", []()
            { XASSERT_EQ(XObject::makeString("hello").toString(), std::string("hello")); });

    runTest("empty list → \"[]\"", []()
            { XASSERT_EQ(XObject::makeList().toString(), std::string("[]")); });

    runTest("list of numbers → \"[1, 2, 3]\"", []()
            {
        XList elems;
        elems.push_back(XObject::makeNumber(1));
        elems.push_back(XObject::makeNumber(2));
        elems.push_back(XObject::makeNumber(3));
        XASSERT_EQ(XObject::makeList(std::move(elems)).toString(), std::string("[1, 2, 3]")); });

    runTest("list with strings → quoted strings", []()
            {
        XList elems;
        elems.push_back(XObject::makeString("a"));
        elems.push_back(XObject::makeString("b"));
        auto s = XObject::makeList(std::move(elems)).toString();
        XASSERT(s.find("\"a\"") != std::string::npos);
        XASSERT(s.find("\"b\"") != std::string::npos); });

    runTest("empty map → \"{}\"", []()
            { XASSERT_EQ(XObject::makeMap().toString(), std::string("{}")); });

    runTest("map → key: value format", []()
            {
        XMap m;
        m.set("host", XObject::makeString("localhost"));
        m.set("port", XObject::makeNumber(3000));
        auto s = XObject::makeMap(std::move(m)).toString();
        XASSERT(s.find("host: \"localhost\"") != std::string::npos);
        XASSERT(s.find("port: 3000") != std::string::npos); });

    runTest("function → \"<fn name>\"", []()
            {
        auto fn = XObject::makeFunction("greet", {"name"}, nullptr);
        XASSERT_EQ(fn.toString(), std::string("<fn greet>")); });
}

// ============================================================================
// Section 5: Equality
// ============================================================================

static void testEquality()
{
    std::cout << "\n===== Equality =====\n";

    runTest("none == none", []()
            { XASSERT(XObject::makeNone().equals(XObject::makeNone())); });

    runTest("number == same number", []()
            { XASSERT(XObject::makeNumber(42).equals(XObject::makeNumber(42))); });

    runTest("number != different number", []()
            { XASSERT(!XObject::makeNumber(42).equals(XObject::makeNumber(43))); });

    runTest("bool == same bool", []()
            { XASSERT(XObject::makeBool(true).equals(XObject::makeBool(true))); });

    runTest("bool != different bool", []()
            { XASSERT(!XObject::makeBool(true).equals(XObject::makeBool(false))); });

    runTest("string == same string", []()
            { XASSERT(XObject::makeString("hello").equals(XObject::makeString("hello"))); });

    runTest("string != different string", []()
            { XASSERT(!XObject::makeString("hello").equals(XObject::makeString("world"))); });

    runTest("different types are never equal", []()
            {
        XASSERT(!XObject::makeNumber(1).equals(XObject::makeBool(true)));
        XASSERT(!XObject::makeNumber(0).equals(XObject::makeNone()));
        XASSERT(!XObject::makeString("1").equals(XObject::makeNumber(1)));
        XASSERT(!XObject::makeString("").equals(XObject::makeNone())); });

    runTest("same XData pointer → equal", []()
            {
        auto a = XObject::makeNumber(42);
        auto b = a; // copy, same XData
        XASSERT(a.equals(b)); });

    runTest("list == same elements", []()
            {
        XList a, b;
        a.push_back(XObject::makeNumber(1));
        a.push_back(XObject::makeString("two"));
        b.push_back(XObject::makeNumber(1));
        b.push_back(XObject::makeString("two"));
        XASSERT(XObject::makeList(std::move(a)).equals(XObject::makeList(std::move(b)))); });

    runTest("list != different elements", []()
            {
        XList a, b;
        a.push_back(XObject::makeNumber(1));
        b.push_back(XObject::makeNumber(2));
        XASSERT(!XObject::makeList(std::move(a)).equals(XObject::makeList(std::move(b)))); });

    runTest("list != different size", []()
            {
        XList a, b;
        a.push_back(XObject::makeNumber(1));
        XASSERT(!XObject::makeList(std::move(a)).equals(XObject::makeList(std::move(b)))); });

    runTest("map == same entries", []()
            {
        XMap a, b;
        a.set("x", XObject::makeNumber(1));
        b.set("x", XObject::makeNumber(1));
        XASSERT(XObject::makeMap(std::move(a)).equals(XObject::makeMap(std::move(b)))); });

    runTest("map != different values", []()
            {
        XMap a, b;
        a.set("x", XObject::makeNumber(1));
        b.set("x", XObject::makeNumber(2));
        XASSERT(!XObject::makeMap(std::move(a)).equals(XObject::makeMap(std::move(b)))); });

    runTest("map != different keys", []()
            {
        XMap a, b;
        a.set("x", XObject::makeNumber(1));
        b.set("y", XObject::makeNumber(1));
        XASSERT(!XObject::makeMap(std::move(a)).equals(XObject::makeMap(std::move(b)))); });

    runTest("function equality is identity", []()
            {
        auto a = XObject::makeFunction("f", {}, nullptr);
        auto b = XObject::makeFunction("f", {}, nullptr);
        XASSERT(!a.equals(b)); // different objects
        auto c = a; // copy → same object
        XASSERT(a.equals(c)); });
}

// ============================================================================
// Section 6: Deep Clone
// ============================================================================

static void testClone()
{
    std::cout << "\n===== Deep Clone =====\n";

    runTest("clone none", []()
            {
        LeakGuard lg;
        {
            auto a = XObject::makeNone();
            auto b = a.clone();
            XASSERT(b.isNone());
            XASSERT_EQ(b.refCount(), (uint32_t)1); // independent
            XASSERT_EQ(a.refCount(), (uint32_t)1);
        }
        lg.check(); });

    runTest("clone number is independent", []()
            {
        LeakGuard lg;
        {
            auto a = XObject::makeNumber(42);
            auto b = a.clone();
            XASSERT(b.isNumber());
            XASSERT_NEAR(b.asNumber(), 42.0, 0.001);
            XASSERT_EQ(a.refCount(), (uint32_t)1);
            XASSERT_EQ(b.refCount(), (uint32_t)1);
            // They should be different XData (independent allocations)
        }
        lg.check(); });

    runTest("clone string is independent", []()
            {
        LeakGuard lg;
        {
            auto a = XObject::makeString("original");
            auto b = a.clone();
            XASSERT_EQ(b.asString(), std::string("original"));
            XASSERT(&a.asString() != &b.asString()); // different memory
        }
        lg.check(); });

    runTest("clone list is deep", []()
            {
        LeakGuard lg;
        {
            XList elems;
            elems.push_back(XObject::makeNumber(1));
            elems.push_back(XObject::makeNumber(2));
            auto a = XObject::makeList(std::move(elems));
            auto b = a.clone();
            XASSERT_EQ(b.asList().size(), (size_t)2);
            // Modifying clone's list shouldn't affect original
            b.asListMut().push_back(XObject::makeNumber(3));
            XASSERT_EQ(a.asList().size(), (size_t)2);
            XASSERT_EQ(b.asList().size(), (size_t)3);
        }
        lg.check(); });

    runTest("clone map is deep", []()
            {
        LeakGuard lg;
        {
            XMap m;
            m.set("a", XObject::makeNumber(1));
            auto a = XObject::makeMap(std::move(m));
            auto b = a.clone();
            b.asMapMut().set("b", XObject::makeNumber(2));
            XASSERT_EQ(a.asMap().size(), (size_t)1);
            XASSERT_EQ(b.asMap().size(), (size_t)2);
        }
        lg.check(); });

    runTest("clone preserves equality", []()
            {
        XList elems;
        elems.push_back(XObject::makeNumber(1));
        elems.push_back(XObject::makeString("two"));
        auto a = XObject::makeList(std::move(elems));
        auto b = a.clone();
        XASSERT(a.equals(b)); });
}

// ============================================================================
// Section 7: XMap Operations
// ============================================================================

static void testXMap()
{
    std::cout << "\n===== XMap =====\n";

    runTest("map: set and get", []()
            {
        LeakGuard lg;
        {
            XMap m;
            m.set("name", XObject::makeString("xell"));
            XASSERT(m.has("name"));
            XASSERT(!m.has("missing"));
            XASSERT_EQ(m.get("name")->asString(), std::string("xell"));
            XASSERT(m.get("missing") == nullptr);
        }
        lg.check(); });

    runTest("map: overwrite existing key", []()
            {
        LeakGuard lg;
        {
            XMap m;
            m.set("x", XObject::makeNumber(1));
            m.set("x", XObject::makeNumber(2));
            XASSERT_EQ(m.size(), (size_t)1);
            XASSERT_NEAR(m.get("x")->asNumber(), 2.0, 0.001);
        }
        lg.check(); });

    runTest("map: preserves insertion order", []()
            {
        LeakGuard lg;
        {
            XMap m;
            m.set("c", XObject::makeNumber(3));
            m.set("a", XObject::makeNumber(1));
            m.set("b", XObject::makeNumber(2));
            XASSERT_EQ(m.entries[0].first, std::string("c"));
            XASSERT_EQ(m.entries[1].first, std::string("a"));
            XASSERT_EQ(m.entries[2].first, std::string("b"));
        }
        lg.check(); });

    runTest("map: const get", []()
            {
        LeakGuard lg;
        {
            XMap m;
            m.set("key", XObject::makeString("val"));
            const XMap& cm = m;
            XASSERT(cm.get("key") != nullptr);
            XASSERT(cm.get("nope") == nullptr);
        }
        lg.check(); });
}

// ============================================================================
// Section 8: Mutable Access
// ============================================================================

static void testMutableAccess()
{
    std::cout << "\n===== Mutable Access =====\n";

    runTest("asStringMut: modify in place", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeString("hello");
            obj.asStringMut() += " world";
            XASSERT_EQ(obj.asString(), std::string("hello world"));
        }
        lg.check(); });

    runTest("asListMut: push_back", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeList();
            obj.asListMut().push_back(XObject::makeNumber(1));
            obj.asListMut().push_back(XObject::makeNumber(2));
            XASSERT_EQ(obj.asList().size(), (size_t)2);
        }
        lg.check(); });

    runTest("asMapMut: add entries", []()
            {
        LeakGuard lg;
        {
            auto obj = XObject::makeMap();
            obj.asMapMut().set("key", XObject::makeString("val"));
            XASSERT_EQ(obj.asMap().size(), (size_t)1);
        }
        lg.check(); });
}

// ============================================================================
// Section 9: Edge Cases & Stress
// ============================================================================

static void testEdgeCases()
{
    std::cout << "\n===== Edge Cases =====\n";

    runTest("nested list of lists", []()
            {
        LeakGuard lg;
        {
            XList inner1, inner2;
            inner1.push_back(XObject::makeNumber(1));
            inner1.push_back(XObject::makeNumber(2));
            inner2.push_back(XObject::makeString("a"));
            inner2.push_back(XObject::makeString("b"));
            XList outer;
            outer.push_back(XObject::makeList(std::move(inner1)));
            outer.push_back(XObject::makeList(std::move(inner2)));
            auto obj = XObject::makeList(std::move(outer));
            XASSERT_EQ(obj.asList().size(), (size_t)2);
            XASSERT(obj.asList()[0].isList());
            XASSERT_EQ(obj.asList()[0].asList().size(), (size_t)2);
            auto s = obj.toString();
            XASSERT(s.find("[1, 2]") != std::string::npos);
        }
        lg.check(); });

    runTest("map containing list", []()
            {
        LeakGuard lg;
        {
            XList tags;
            tags.push_back(XObject::makeString("web"));
            tags.push_back(XObject::makeString("api"));
            XMap m;
            m.set("tags", XObject::makeList(std::move(tags)));
            m.set("port", XObject::makeNumber(8080));
            auto obj = XObject::makeMap(std::move(m));
            XASSERT(obj.asMap().get("tags")->isList());
            XASSERT_EQ(obj.asMap().get("tags")->asList().size(), (size_t)2);
        }
        lg.check(); });

    runTest("list containing map", []()
            {
        LeakGuard lg;
        {
            XMap m;
            m.set("x", XObject::makeNumber(1));
            XList elems;
            elems.push_back(XObject::makeMap(std::move(m)));
            auto obj = XObject::makeList(std::move(elems));
            XASSERT(obj.asList()[0].isMap());
        }
        lg.check(); });

    runTest("shared list: copy shares data, clone is independent", []()
            {
        LeakGuard lg;
        {
            XList elems;
            elems.push_back(XObject::makeNumber(1));
            auto a = XObject::makeList(std::move(elems));
            auto b = a; // shared
            XASSERT_EQ(a.refCount(), (uint32_t)2);
            // Mutating through b also changes a (same data)
            b.asListMut().push_back(XObject::makeNumber(2));
            XASSERT_EQ(a.asList().size(), (size_t)2);
            // Clone is independent
            auto c = a.clone();
            c.asListMut().push_back(XObject::makeNumber(3));
            XASSERT_EQ(a.asList().size(), (size_t)2);
            XASSERT_EQ(c.asList().size(), (size_t)3);
        }
        lg.check(); });

    runTest("large number of allocations and frees", []()
            {
        LeakGuard lg;
        {
            XList big;
            for (int i = 0; i < 10000; i++)
            {
                big.push_back(XObject::makeNumber(i));
            }
            auto obj = XObject::makeList(std::move(big));
            XASSERT_EQ(obj.asList().size(), (size_t)10000);
        }
        lg.check(); });

    runTest("rapid create-destroy cycle (no leak)", []()
            {
        LeakGuard lg;
        for (int i = 0; i < 1000; i++)
        {
            auto obj = XObject::makeString("temp");
            // immediately destroyed
        }
        lg.check(); });

    runTest("moved-from XObject is safe to destroy", []()
            {
        LeakGuard lg;
        {
            auto a = XObject::makeNumber(42);
            auto b = std::move(a);
            // a is now moved-from, but it's safe to let it go out of scope
        }
        lg.check(); });

    runTest("moved-from XObject is none-like", []()
            {
        auto a = XObject::makeNumber(42);
        auto b = std::move(a);
        XASSERT(a.isNone()); // moved-from → type() returns NONE (data_ is null)
        XASSERT_EQ(a.refCount(), (uint32_t)0); });
}

// ============================================================================
// Section 10: Leak Detection Sanity
// ============================================================================

static void testLeakDetection()
{
    std::cout << "\n===== Leak Detection =====\n";

    runTest("liveAllocations starts at 0 after reset", []()
            {
        XObject::resetAllocationCounter();
        XASSERT_EQ(XObject::liveAllocations(), (int64_t)0); });

    runTest("liveAllocations tracks correctly", []()
            {
        XObject::resetAllocationCounter();
        auto a = XObject::makeNumber(1);
        auto b = XObject::makeString("two");
        auto c = XObject::makeBool(true);
        XASSERT_EQ(XObject::liveAllocations(), (int64_t)3);
        { auto d = a; } // copy, no new alloc; d destroyed, but ref > 0, no free
        XASSERT_EQ(XObject::liveAllocations(), (int64_t)3); });
}

// ============================================================================
// main
// ============================================================================

int main()
{
    testConstruction();
    testRefCounting();
    testTruthiness();
    testToString();
    testEquality();
    testClone();
    testXMap();
    testMutableAccess();
    testEdgeCases();
    testLeakDetection();

    std::cout << "\n============================================\n";
    std::cout << "  Total: " << (g_passed + g_failed)
              << "  |  Passed: " << g_passed
              << "  |  Failed: " << g_failed << "\n";
    std::cout << "============================================\n";

    return g_failed == 0 ? 0 : 1;
}
