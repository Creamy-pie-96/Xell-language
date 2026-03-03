// ============================================================================
// Xell — let ... be (RAII / Context Manager) Tests
// ============================================================================
#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <functional>

static int g_passed = 0;
static int g_failed = 0;

#define XASSERT_EQ(a, b)                                                          \
    do                                                                             \
    {                                                                              \
        auto _a = (a);                                                             \
        auto _b = (b);                                                             \
        if (_a != _b)                                                              \
        {                                                                          \
            std::ostringstream os;                                                 \
            os << "  ASSERTION FAILED: " << #a << " == " << #b << "\n"             \
               << "        got: [" << _a << "] vs [" << _b << "]";                 \
            throw std::runtime_error(os.str());                                    \
        }                                                                          \
    } while (0)

#define XASSERT_THROWS(expr)                                                       \
    do                                                                             \
    {                                                                              \
        bool caught = false;                                                       \
        try { expr; } catch (...) { caught = true; }                               \
        if (!caught) throw std::runtime_error("  Expected exception but none thrown"); \
    } while (0)

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
        std::cout << "  FAIL: " << name << "\n"
                  << e.what() << "\n";
        g_failed++;
    }
}

// A reusable resource class definition used in many tests
static const std::string RES_CLASS = R"XEL(
class Res :
    name = ""
    fn __init__(self, name):
        self->name = name
    ;
    fn __enter__(self):
        print("enter:" + self->name)
        give self
    ;
    fn __exit__(self):
        print("exit:" + self->name)
    ;
;
)XEL";

int main()
{
    std::cout << "===== LET...BE (RAII) TESTS =====\n\n";

    // ========== Basic Binding ==========
    std::cout << "===== Basic Binding =====\n";

    runTest("single binding", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
let Res("file") be f:
    print("use:" + f->name)
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], "enter:file");
        XASSERT_EQ(out[1], "use:file");
        XASSERT_EQ(out[2], "exit:file");
    });

    runTest("multiple bindings — reverse teardown", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
let Res("a") be r1, Res("b") be r2:
    print("body")
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)5);
        XASSERT_EQ(out[0], "enter:a");
        XASSERT_EQ(out[1], "enter:b");
        XASSERT_EQ(out[2], "body");
        XASSERT_EQ(out[3], "exit:b");
        XASSERT_EQ(out[4], "exit:a");
    });

    runTest("three bindings — LIFO teardown", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
let Res("x") be a, Res("y") be b, Res("z") be c:
    print("all")
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)7);
        XASSERT_EQ(out[0], "enter:x");
        XASSERT_EQ(out[1], "enter:y");
        XASSERT_EQ(out[2], "enter:z");
        XASSERT_EQ(out[3], "all");
        XASSERT_EQ(out[4], "exit:z");
        XASSERT_EQ(out[5], "exit:y");
        XASSERT_EQ(out[6], "exit:x");
    });

    runTest("_ discard binding", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
let Res("lock") be _:
    print("critical")
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], "enter:lock");
        XASSERT_EQ(out[1], "critical");
        XASSERT_EQ(out[2], "exit:lock");
    });

    // ========== Proxy Pattern ==========
    std::cout << "\n===== Proxy Pattern =====\n";

    runTest("__enter__ returns proxy", []()
            {
        auto out = runXell(R"XEL(
class Conn :
    fn __enter__(self):
        give "cursor_proxy"
    ;
    fn __exit__(self):
        print("conn_closed")
    ;
;
let Conn() be c:
    print(c)
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)2);
        XASSERT_EQ(out[0], "cursor_proxy");
        XASSERT_EQ(out[1], "conn_closed");
    });

    // ========== Error Handling ==========
    std::cout << "\n===== Error Handling =====\n";

    runTest("error in block — __exit__ called before propagation", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
try:
    let Res("h") be h:
        print("before_error")
        1 / 0
    ;
; catch e:
    print("caught:" + e->message)
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)4);
        XASSERT_EQ(out[0], "enter:h");
        XASSERT_EQ(out[1], "before_error");
        XASSERT_EQ(out[2], "exit:h");
        XASSERT_EQ(out[3], "caught:division by zero");
    });

    runTest("error with multiple bindings — reverse cleanup", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
try:
    let Res("1") be a, Res("2") be b, Res("3") be c:
        1 / 0
    ;
; catch e:
    print("caught")
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)7);
        XASSERT_EQ(out[0], "enter:1");
        XASSERT_EQ(out[1], "enter:2");
        XASSERT_EQ(out[2], "enter:3");
        XASSERT_EQ(out[3], "exit:3");
        XASSERT_EQ(out[4], "exit:2");
        XASSERT_EQ(out[5], "exit:1");
        XASSERT_EQ(out[6], "caught");
    });

    runTest("__exit__ itself throws — error propagates", []()
            {
        auto out = runXell(R"XEL(
class BadExit :
    fn __enter__(self):
        print("enter")
        give self
    ;
    fn __exit__(self):
        print("exit_throws")
        1 / 0
    ;
;
try:
    let BadExit() be b:
        print("body")
    ;
; catch e:
    print("caught:" + e->message)
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)4);
        XASSERT_EQ(out[0], "enter");
        XASSERT_EQ(out[1], "body");
        XASSERT_EQ(out[2], "exit_throws");
        XASSERT_EQ(out[3], "caught:division by zero");
    });

    // ========== Partial Init Failure ==========
    std::cout << "\n===== Partial Init Failure =====\n";

    runTest("__enter__ of second binding fails — first cleaned up", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
class Bad :
    fn __enter__(self):
        print("bad_enter")
        1 / 0
    ;
    fn __exit__(self):
        print("bad_exit")
    ;
;
try:
    let Res("ok") be r1, Bad() be r2:
        print("never")
    ;
; catch e:
    print("caught:" + e->message)
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)4);
        XASSERT_EQ(out[0], "enter:ok");
        XASSERT_EQ(out[1], "bad_enter");
        XASSERT_EQ(out[2], "exit:ok");
        XASSERT_EQ(out[3], "caught:division by zero");
    });

    // ========== give / break / continue ==========
    std::cout << "\n===== give / break / continue =====\n";

    runTest("give inside let block — __exit__ called", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
fn process():
    let Res("h") be h:
        give "result"
    ;
;
r = process()
print(r)
)XEL");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], "enter:h");
        XASSERT_EQ(out[1], "exit:h");
        XASSERT_EQ(out[2], "result");
    });

    runTest("break inside let inside for — __exit__ called each iteration", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
results = []
for i in [1, 2, 3]:
    let Res(str(i)) be c:
        if i eq 2:
            break
        ;
        push(results, i)
    ;
;
print(str(results))
)XEL");
        // enter:1, exit:1, enter:2, exit:2, [1]
        XASSERT_EQ(out.size(), (size_t)5);
        XASSERT_EQ(out[0], "enter:1");
        XASSERT_EQ(out[1], "exit:1");
        XASSERT_EQ(out[2], "enter:2");
        XASSERT_EQ(out[3], "exit:2");
        XASSERT_EQ(out[4], "[1]");
    });

    runTest("continue inside let inside for — __exit__ called", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
results = []
for i in [1, 2, 3]:
    let Res(str(i)) be c:
        if i eq 2:
            continue
        ;
        push(results, i)
    ;
;
print(str(results))
)XEL");
        // enter:1, exit:1, enter:2, exit:2, enter:3, exit:3, [1, 3]
        XASSERT_EQ(out.size(), (size_t)7);
        XASSERT_EQ(out[0], "enter:1");
        XASSERT_EQ(out[1], "exit:1");
        XASSERT_EQ(out[2], "enter:2");
        XASSERT_EQ(out[3], "exit:2");
        XASSERT_EQ(out[4], "enter:3");
        XASSERT_EQ(out[5], "exit:3");
        XASSERT_EQ(out[6], "[1, 3]");
    });

    runTest("give nested in incase inside let", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
fn choose(mode):
    let Res("ctx") be c:
        incase mode:
            is "a":
                give "alpha"
            ;
            is "b":
                give "beta"
            ;
        ;
        give "default"
    ;
;
print(choose("a"))
print(choose("b"))
print(choose("x"))
)XEL");
        XASSERT_EQ(out.size(), (size_t)9);
        XASSERT_EQ(out[0], "enter:ctx"); XASSERT_EQ(out[1], "exit:ctx"); XASSERT_EQ(out[2], "alpha");
        XASSERT_EQ(out[3], "enter:ctx"); XASSERT_EQ(out[4], "exit:ctx"); XASSERT_EQ(out[5], "beta");
        XASSERT_EQ(out[6], "enter:ctx"); XASSERT_EQ(out[7], "exit:ctx"); XASSERT_EQ(out[8], "default");
    });

    runTest("give nested in if inside let", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
fn check(x):
    let Res("chk") be c:
        if x > 10:
            give "big"
        ;
        give "small"
    ;
;
print(check(20))
print(check(5))
)XEL");
        XASSERT_EQ(out.size(), (size_t)6);
        XASSERT_EQ(out[0], "enter:chk"); XASSERT_EQ(out[1], "exit:chk"); XASSERT_EQ(out[2], "big");
        XASSERT_EQ(out[3], "enter:chk"); XASSERT_EQ(out[4], "exit:chk"); XASSERT_EQ(out[5], "small");
    });

    // ========== Nested let blocks ==========
    std::cout << "\n===== Nested Let Blocks =====\n";

    runTest("nested let blocks — inner exits before outer", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
let Res("outer") be o:
    let Res("inner") be i:
        print("both:" + o->name + "+" + i->name)
    ;
    print("after_inner:" + o->name)
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)6);
        XASSERT_EQ(out[0], "enter:outer");
        XASSERT_EQ(out[1], "enter:inner");
        XASSERT_EQ(out[2], "both:outer+inner");
        XASSERT_EQ(out[3], "exit:inner");
        XASSERT_EQ(out[4], "after_inner:outer");
        XASSERT_EQ(out[5], "exit:outer");
    });

    // ========== let inside try/catch ==========
    std::cout << "\n===== Let inside Try/Catch =====\n";

    runTest("__enter__ throws — caught by try/catch", []()
            {
        auto out = runXell(R"XEL(
class FailEnter :
    fn __enter__(self):
        1 / 0
    ;
    fn __exit__(self):
        print("nope")
    ;
;
try:
    let FailEnter() be x:
        print("never")
    ;
; catch e:
    print("caught:" + e->message)
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)1);
        XASSERT_EQ(out[0], "caught:division by zero");
    });

    runTest("block throws — __exit__ then catch", []()
            {
        auto out = runXell(RES_CLASS + R"XEL(
try:
    let Res("r") be r:
        1 / 0
    ;
; catch e:
    print("caught")
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)3);
        XASSERT_EQ(out[0], "enter:r");
        XASSERT_EQ(out[1], "exit:r");
        XASSERT_EQ(out[2], "caught");
    });

    runTest("__exit__ throws — replaces body error", []()
            {
        auto out = runXell(R"XEL(
class BothFail :
    fn __enter__(self):
        print("enter")
        give self
    ;
    fn __exit__(self):
        print("exit_fail")
        1 / 0
    ;
;
try:
    let BothFail() be b:
        print("body_fail")
        0 - none
    ;
; catch e:
    print("caught:" + e->message)
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)4);
        XASSERT_EQ(out[0], "enter");
        XASSERT_EQ(out[1], "body_fail");
        XASSERT_EQ(out[2], "exit_fail");
        XASSERT_EQ(out[3], "caught:division by zero");
    });

    // ========== Error Cases ==========
    std::cout << "\n===== Error Cases =====\n";

    runTest("missing __enter__ — TypeError", []()
            {
        XASSERT_THROWS(runXell(R"XEL(
class NoEnter :
    fn __exit__(self):
        print("x")
    ;
;
let NoEnter() be x:
    print("never")
;
)XEL"));
    });

    runTest("missing __exit__ — TypeError", []()
            {
        XASSERT_THROWS(runXell(R"XEL(
class NoExit :
    fn __enter__(self):
        give self
    ;
;
let NoExit() be x:
    print("never")
;
)XEL"));
    });

    runTest("non-instance — TypeError", []()
            {
        XASSERT_THROWS(runXell(R"XEL(
let 42 be x:
    print("never")
;
)XEL"));
    });

    // ========== Dependent Resources ==========
    std::cout << "\n===== Dependent Resources =====\n";

    runTest("second binding depends on first", []()
            {
        auto out = runXell(R"XEL(
class Pool :
    fn __enter__(self):
        print("pool_enter")
        give self
    ;
    fn __exit__(self):
        print("pool_exit")
    ;
    fn connect(self):
        give Conn()
    ;
;
class Conn :
    fn __enter__(self):
        print("conn_enter")
        give self
    ;
    fn __exit__(self):
        print("conn_exit")
    ;
;
let Pool() be pool, pool->connect() be conn:
    print("working")
;
)XEL");
        XASSERT_EQ(out.size(), (size_t)5);
        XASSERT_EQ(out[0], "pool_enter");
        XASSERT_EQ(out[1], "conn_enter");
        XASSERT_EQ(out[2], "working");
        XASSERT_EQ(out[3], "conn_exit");
        XASSERT_EQ(out[4], "pool_exit");
    });

    // ========== Summary ==========
    std::cout << "\n========================================\n";
    std::cout << "Total: " << (g_passed + g_failed)
              << "  Passed: " << g_passed
              << "  Failed: " << g_failed << "\n";

    return g_failed == 0 ? 0 : 1;
}
