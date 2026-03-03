// =============================================================================
// Access Control Tests
// =============================================================================
// Tests Phase 4 OOP: private/protected/public access modifiers for class
// fields and methods. Covers access from outside, inside methods, inheritance
// chains (private not accessible in subclass, protected is), default public
// behavior, multiple access blocks, and error messages.
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
// Section 1: Public Access (default)
// ============================================================================

static void testPublicAccess()
{
    std::cout << "\n===== Public Access (Default) =====\n";

    runTest("Fields before any access block are public by default", []()
            {
        auto out = runXell(R"XEL(
class Foo :
    x = 10
    y = 20
;
f = Foo()
print(f->x)
print(f->y)
)XEL");
        XASSERT_EQ(out.size(), 2u);
        XASSERT_EQ(out[0], "10");
        XASSERT_EQ(out[1], "20"); });

    runTest("Methods before any access block are public by default", []()
            {
        auto out = runXell(R"XEL(
class Foo :
    fn greet(self) :
        give "hello"
    ;
;
f = Foo()
print(f->greet())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "hello"); });

    runTest("Explicit public: block fields are accessible", []()
            {
        auto out = runXell(R"XEL(
class Foo :
    public:
        x = 42
;
f = Foo()
print(f->x)
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "42"); });

    runTest("Explicit public: block methods are accessible", []()
            {
        auto out = runXell(R"XEL(
class Foo :
    public:
        fn hello(self) :
            give "world"
        ;
;
f = Foo()
print(f->hello())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "world"); });
}

// ============================================================================
// Section 2: Private Access
// ============================================================================

static void testPrivateAccess()
{
    std::cout << "\n===== Private Access =====\n";

    runTest("Private field read from outside throws AccessError", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class Secret :
    private:
        code = 1234
;
s = Secret()
print(s->code)
)XEL")); });

    runTest("Private field write from outside throws AccessError", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class Secret :
    private:
        code = 1234
;
s = Secret()
s->code = 9999
)XEL")); });

    runTest("Private method call from outside throws AccessError", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class Secret :
    private:
        fn internal(self) :
            give "secret"
        ;
;
s = Secret()
s->internal()
)XEL")); });

    runTest("Private field accessible from within own class method", []()
            {
        auto out = runXell(R"XEL(
class BankAccount :
    private:
        balance = 0

    public:
        fn deposit(self, amount) :
            self->balance += amount
        ;
        fn get_balance(self) :
            give self->balance
        ;
;
acc = BankAccount()
acc->deposit(500)
print(acc->get_balance())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "500"); });

    runTest("Private method callable from within own class method", []()
            {
        auto out = runXell(R"XEL(
class Processor :
    private:
        fn compute(self, x) :
            give x * 2
        ;

    public:
        fn run(self, x) :
            give self->compute(x)
        ;
;
p = Processor()
print(p->run(21))
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "42"); });

    runTest("Private field write from within own class method works", []()
            {
        auto out = runXell(R"XEL(
class Counter :
    private:
        count = 0

    public:
        fn increment(self) :
            self->count += 1
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
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "3"); });
}

// ============================================================================
// Section 3: Protected Access
// ============================================================================

static void testProtectedAccess()
{
    std::cout << "\n===== Protected Access =====\n";

    runTest("Protected field read from outside throws AccessError", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class Base :
    protected:
        secret = 42
;
b = Base()
print(b->secret)
)XEL")); });

    runTest("Protected field write from outside throws AccessError", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class Base :
    protected:
        secret = 42
;
b = Base()
b->secret = 99
)XEL")); });

    runTest("Protected method call from outside throws AccessError", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class Base :
    protected:
        fn helper(self) :
            give "internal"
        ;
;
b = Base()
b->helper()
)XEL")); });

    runTest("Protected field accessible from within own class", []()
            {
        auto out = runXell(R"XEL(
class Base :
    protected:
        data = 100

    public:
        fn get_data(self) :
            give self->data
        ;
;
b = Base()
print(b->get_data())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "100"); });

    runTest("Protected field accessible from subclass", []()
            {
        auto out = runXell(R"XEL(
class Base :
    protected:
        data = 100
;
class Child inherits Base :
    public:
        fn get_data(self) :
            give self->data
        ;
;
c = Child()
print(c->get_data())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "100"); });

    runTest("Protected method accessible from subclass", []()
            {
        auto out = runXell(R"XEL(
class Base :
    protected:
        fn helper(self) :
            give "from base"
        ;
;
class Child inherits Base :
    public:
        fn run(self) :
            give self->helper()
        ;
;
c = Child()
print(c->run())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "from base"); });

    runTest("Protected field writable from subclass", []()
            {
        auto out = runXell(R"XEL(
class Base :
    protected:
        val = 0
;
class Child inherits Base :
    public:
        fn set_val(self, v) :
            self->val = v
        ;
        fn get_val(self) :
            give self->val
        ;
;
c = Child()
c->set_val(77)
print(c->get_val())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "77"); });

    runTest("Protected accessible from grandchild (3-level chain)", []()
            {
        auto out = runXell(R"XEL(
class A :
    protected:
        x = 10
;
class B inherits A :
;
class C inherits B :
    public:
        fn get_x(self) :
            give self->x
        ;
;
c = C()
print(c->get_x())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "10"); });
}

// ============================================================================
// Section 4: Private Not Accessible from Subclass
// ============================================================================

static void testPrivateInheritance()
{
    std::cout << "\n===== Private Not Accessible from Subclass =====\n";

    runTest("Private field of parent NOT accessible from subclass method", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class Base :
    private:
        secret = 42
;
class Child inherits Base :
    public:
        fn reveal(self) :
            give self->secret
        ;
;
c = Child()
c->reveal()
)XEL")); });

    runTest("Private method of parent NOT callable from subclass method", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class Base :
    private:
        fn hidden(self) :
            give "nope"
        ;
;
class Child inherits Base :
    public:
        fn try_call(self) :
            give self->hidden()
        ;
;
c = Child()
c->try_call()
)XEL")); });

    runTest("Private field of parent NOT writable from subclass method", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class Base :
    private:
        val = 0
;
class Child inherits Base :
    public:
        fn set_it(self) :
            self->val = 99
        ;
;
c = Child()
c->set_it()
)XEL")); });
}

// ============================================================================
// Section 5: Mixed Access Levels
// ============================================================================

static void testMixedAccess()
{
    std::cout << "\n===== Mixed Access Levels =====\n";

    runTest("Class with public, private, and protected sections", []()
            {
        auto out = runXell(R"XEL(
class Account :
    name = "default"

    private:
        balance = 0

    protected:
        internal_id = 42

    public:
        fn get_name(self) :
            give self->name
        ;
        fn get_balance(self) :
            give self->balance
        ;
        fn get_id(self) :
            give self->internal_id
        ;
;
a = Account()
print(a->name)
print(a->get_balance())
print(a->get_id())
)XEL");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], "default");
        XASSERT_EQ(out[1], "0");
        XASSERT_EQ(out[2], "42"); });

    runTest("Private balance not accessible from outside", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class Account :
    private:
        balance = 0
;
a = Account()
print(a->balance)
)XEL")); });

    runTest("Protected internal_id not accessible from outside", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class Account :
    protected:
        internal_id = 42
;
a = Account()
print(a->internal_id)
)XEL")); });

    runTest("Multiple access blocks in one class", []()
            {
        auto out = runXell(R"XEL(
class Multi :
    private:
        a = 1
    public:
        b = 2
    private:
        c = 3
    public:
        fn get_a(self) :
            give self->a
        ;
        fn get_c(self) :
            give self->c
        ;
;
m = Multi()
print(m->b)
print(m->get_a())
print(m->get_c())
)XEL");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], "2");
        XASSERT_EQ(out[1], "1");
        XASSERT_EQ(out[2], "3"); });
}

// ============================================================================
// Section 6: Access Control with __init__
// ============================================================================

static void testAccessWithInit()
{
    std::cout << "\n===== Access Control with __init__ =====\n";

    runTest("__init__ can set private fields", []()
            {
        auto out = runXell(R"XEL(
class Wallet :
    fn __init__(self, amount) :
        self->cash = amount
    ;

    private:
        cash = 0

    public:
        fn get_cash(self) :
            give self->cash
        ;
;
w = Wallet(100)
print(w->get_cash())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "100"); });

    runTest("Private field set in __init__ not readable outside", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class Wallet :
    fn __init__(self, amount) :
        self->cash = amount
    ;

    private:
        cash = 0
;
w = Wallet(100)
print(w->cash)
)XEL")); });
}

// ============================================================================
// Section 7: Access Control with Inheritance Chain
// ============================================================================

static void testAccessInheritanceChain()
{
    std::cout << "\n===== Access Control with Inheritance Chain =====\n";

    runTest("Full BankAccount example from plan", []()
            {
        auto out = runXell(R"XEL(
class BankAccount :
    owner = ""

    fn __init__(self, owner) :
        self->owner = owner
    ;

    private:
        balance = 0

    protected:
        internal_id = 42

    public:
        fn deposit(self, amount) :
            self->balance += amount
        ;
        fn get_balance(self) :
            give self->balance
        ;
;
acc = BankAccount("Alice")
acc->deposit(500)
print(acc->get_balance())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "500"); });

    runTest("BankAccount: balance is private", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class BankAccount :
    private:
        balance = 0
;
acc = BankAccount()
print(acc->balance)
)XEL")); });

    runTest("BankAccount: internal_id is protected", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class BankAccount :
    protected:
        internal_id = 42
;
acc = BankAccount()
print(acc->internal_id)
)XEL")); });

    runTest("Subclass can access protected parent field but not private", []()
            {
        auto out = runXell(R"XEL(
class Base :
    private:
        priv = 1

    protected:
        prot = 2

    public:
        fn get_priv(self) :
            give self->priv
        ;
;
class Child inherits Base :
    public:
        fn get_prot(self) :
            give self->prot
        ;
;
c = Child()
print(c->get_prot())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "2"); });

    runTest("Private via parent's public getter works from subclass", []()
            {
        auto out = runXell(R"XEL(
class Base :
    private:
        secret = 99

    public:
        fn get_secret(self) :
            give self->secret
        ;
;
class Child inherits Base :
    public:
        fn reveal(self) :
            give self->get_secret()
        ;
;
c = Child()
print(c->reveal())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "99"); });
}

// ============================================================================
// Section 8: Edge Cases
// ============================================================================

static void testAccessEdgeCases()
{
    std::cout << "\n===== Access Control Edge Cases =====\n";

    runTest("Class with only private fields — construction works", []()
            {
        auto out = runXell(R"XEL(
class AllPrivate :
    private:
        x = 1
        y = 2

    public:
        fn sum(self) :
            give self->x + self->y
        ;
;
a = AllPrivate()
print(a->sum())
)XEL");
        XASSERT_EQ(out.size(), 1u);
        XASSERT_EQ(out[0], "3"); });

    runTest("Two instances share access rules", []()
            {
        XASSERT(expectError<AccessError>(R"XEL(
class Foo :
    private:
        val = 42
;
a = Foo()
b = Foo()
print(a->val)
)XEL")); });

    runTest("Default access (no blocks) — all public", []()
            {
        auto out = runXell(R"XEL(
class Open :
    x = 1
    y = 2
    fn total(self) :
        give self->x + self->y
    ;
;
o = Open()
print(o->x)
print(o->y)
print(o->total())
)XEL");
        XASSERT_EQ(out.size(), 3u);
        XASSERT_EQ(out[0], "1");
        XASSERT_EQ(out[1], "2");
        XASSERT_EQ(out[2], "3"); });
}

// ============================================================================
// main
// ============================================================================

int main()
{
    std::cout << "===== ACCESS CONTROL TESTS (Phase 4 OOP) =====\n";

    testPublicAccess();
    testPrivateAccess();
    testProtectedAccess();
    testPrivateInheritance();
    testMixedAccess();
    testAccessWithInit();
    testAccessInheritanceChain();
    testAccessEdgeCases();

    std::cout << "\n========================================\n";
    std::cout << "Total: " << (g_passed + g_failed)
              << "  Passed: " << g_passed
              << "  Failed: " << g_failed << "\n";

    return g_failed == 0 ? 0 : 1;
}
