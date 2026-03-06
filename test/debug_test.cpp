// =============================================================================
// Debug System Tests — TraceCollector + Interpreter Hooks
// =============================================================================
// Verifies Phase 4 of the Debug System Plan:
//   - TraceCollector event emission
//   - TrackFilter whitelist/blacklist logic
//   - SamplingState head/tail selection
//   - Interpreter hooks: VAR_BORN, VAR_CHANGED, FN_CALLED, FN_RETURNED,
//     LOOP_STARTED, LOOP_ITERATION, LOOP_COMPLETED, LOOP_BROKE,
//     BRANCH_IF, BRANCH_ELIF, BRANCH_ELSE, ERROR_CAUGHT
//   - Snapshot value-copy correctness
//   - Zero-cost when disabled (trace_ null or enabled=false)
//   - Watch dirty-flag dependency tracking
// =============================================================================

#include "../src/interpreter/interpreter.hpp"
#include "../src/interpreter/trace_collector.hpp"
#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include <iostream>
#include <sstream>
#include <functional>
#include <string>
#include <vector>
#include <algorithm>

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

#define XASSERT_GE(a, b)                                 \
    do                                                   \
    {                                                    \
        if ((a) < (b))                                   \
        {                                                \
            std::ostringstream os;                       \
            os << "Expected [" << (a) << "] >= [" << (b) \
               << "] (line " << __LINE__ << ")";         \
            throw std::runtime_error(os.str());          \
        }                                                \
    } while (0)

// Helper: run Xell source with tracing enabled, return the trace entries
static std::pair<std::vector<std::string>, std::vector<TraceEntry>>
runXellTraced(const std::string &source, TraceCollector &tc)
{
    Lexer lexer(source);
    auto tokens = lexer.tokenize();
    Parser parser(tokens);
    auto program = parser.parse();
    Interpreter interp;
    tc.enabled = true;
    interp.setTraceCollector(&tc);
    interp.run(program);
    return {interp.output(), {tc.entries().begin(), tc.entries().end()}};
}

// Helper: count events of a specific type
static int countEvents(const std::vector<TraceEntry> &entries, TraceEvent ev)
{
    int count = 0;
    for (const auto &e : entries)
        if (e.event == ev)
            count++;
    return count;
}

// Helper: find first event of a type with a given name
static const TraceEntry *findEvent(const std::vector<TraceEntry> &entries,
                                   TraceEvent ev, const std::string &name = "")
{
    for (const auto &e : entries)
    {
        if (e.event == ev && (name.empty() || e.name == name))
            return &e;
    }
    return nullptr;
}

// ============================================================================
// Section 1: TrackFilter Unit Tests
// ============================================================================

static void testTrackFilter()
{
    std::cout << "\n===== TrackFilter Unit Tests =====\n";

    runTest("trackFilter: default allows all vars", []()
            {
        TrackFilter f;
        XASSERT(f.shouldTrackVar("x"));
        XASSERT(f.shouldTrackVar("anything")); });

    runTest("trackFilter: whitelist restricts vars", []()
            {
        TrackFilter f;
        f.trackVars.insert("x");
        f.trackVars.insert("y");
        XASSERT(f.shouldTrackVar("x"));
        XASSERT(f.shouldTrackVar("y"));
        XASSERT(!f.shouldTrackVar("z")); });

    runTest("trackFilter: blacklist overrides whitelist", []()
            {
        TrackFilter f;
        f.trackVars.insert("x");
        f.notrackVars.insert("x"); // blacklist wins
        XASSERT(!f.shouldTrackVar("x")); });

    runTest("trackFilter: fn whitelist", []()
            {
        TrackFilter f;
        f.trackFns.insert("process");
        XASSERT(f.shouldTrackFn("process"));
        XASSERT(!f.shouldTrackFn("helper")); });

    runTest("trackFilter: fn blacklist", []()
            {
        TrackFilter f;
        f.notrackFns.insert("_internal");
        XASSERT(!f.shouldTrackFn("_internal"));
        XASSERT(f.shouldTrackFn("process")); });

    runTest("trackFilter: category flags", []()
            {
        TrackFilter f;
        XASSERT(f.shouldTrackEvent(TraceEvent::BRANCH_IF));
        f.trackConditions = false;
        XASSERT(!f.shouldTrackEvent(TraceEvent::BRANCH_IF));
        XASSERT(!f.shouldTrackEvent(TraceEvent::BRANCH_ELIF));
        XASSERT(!f.shouldTrackEvent(TraceEvent::BRANCH_ELSE)); });

    runTest("trackFilter: errors always tracked", []()
            {
        TrackFilter f;
        f.trackConditions = false;
        f.trackLoopFor = false;
        f.trackCalls = false;
        // Errors should still be tracked
        XASSERT(f.shouldTrackEvent(TraceEvent::ERROR_THROWN));
        XASSERT(f.shouldTrackEvent(TraceEvent::ERROR_CAUGHT)); });

    runTest("trackFilter: reset restores defaults", []()
            {
        TrackFilter f;
        f.trackVars.insert("x");
        f.notrackFns.insert("helper");
        f.trackConditions = false;
        f.reset();
        XASSERT(f.trackVars.empty());
        XASSERT(f.notrackFns.empty());
        XASSERT(f.trackConditions); });
}

// ============================================================================
// Section 2: SamplingState Unit Tests
// ============================================================================

static void testSamplingState()
{
    std::cout << "\n===== SamplingState Unit Tests =====\n";

    runTest("sampling: disabled traces everything", []()
            {
        SamplingState s;
        XASSERT(s.shouldTrace(0, 100));
        XASSERT(s.shouldTrace(50, 100));
        XASSERT(s.shouldTrace(99, 100)); });

    runTest("sampling: head/tail split", []()
            {
        SamplingState s;
        s.configure(10); // sample 10 → head=5, tail=5
        // Total of 100 iterations
        // Head: 0-4 traced, tail: 95-99 traced, middle skipped
        XASSERT(s.shouldTrace(0, 100));
        XASSERT(s.shouldTrace(4, 100));
        XASSERT(!s.shouldTrace(5, 100));
        XASSERT(!s.shouldTrace(50, 100));
        XASSERT(!s.shouldTrace(94, 100));
        XASSERT(s.shouldTrace(95, 100));
        XASSERT(s.shouldTrace(99, 100)); });

    runTest("sampling: configure and disable", []()
            {
                SamplingState s;
                s.configure(20);
                XASSERT(s.active);
                XASSERT_EQ(s.sampleSize, 20);
                XASSERT_EQ(s.headCount, 10);
                s.disable();
                XASSERT(!s.active);
                XASSERT(s.shouldTrace(50, 100)); // disabled → always true
            });
}

// ============================================================================
// Section 3: TraceCollector Unit Tests
// ============================================================================

static void testTraceCollector()
{
    std::cout << "\n===== TraceCollector Unit Tests =====\n";

    runTest("collector: no events when disabled", []()
            {
        TraceCollector tc;
        tc.emit(TraceEvent::VAR_BORN, 1, "x", "int", "10");
        XASSERT_EQ((int)tc.entries().size(), 0); });

    runTest("collector: events emitted when enabled", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.emit(TraceEvent::VAR_BORN, 1, "x", "int", "10");
        XASSERT_EQ((int)tc.entries().size(), 1);
        XASSERT_EQ(tc.entries()[0].name, "x");
        XASSERT(std::string(traceEventName(tc.entries()[0].event)) == "VAR_BORN"); });

    runTest("collector: sequence increments", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.emit(TraceEvent::VAR_BORN, 1, "x");
        tc.emit(TraceEvent::VAR_CHANGED, 2, "x");
        tc.emit(TraceEvent::FN_CALLED, 3, "foo");
        XASSERT_EQ(tc.entries()[0].sequence, 0);
        XASSERT_EQ(tc.entries()[1].sequence, 1);
        XASSERT_EQ(tc.entries()[2].sequence, 2); });

    runTest("collector: emitVar respects var filter", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.filter().notrackVars.insert("temp");
        tc.emitVar(TraceEvent::VAR_BORN, 1, "x", "int", "10");
        tc.emitVar(TraceEvent::VAR_BORN, 2, "temp", "int", "20");
        XASSERT_EQ((int)tc.entries().size(), 1);
        XASSERT_EQ(tc.entries()[0].name, "x"); });

    runTest("collector: emitFn respects fn filter", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.filter().notrackFns.insert("_helper");
        tc.emitFn(TraceEvent::FN_CALLED, 1, "process");
        tc.emitFn(TraceEvent::FN_CALLED, 2, "_helper");
        XASSERT_EQ((int)tc.entries().size(), 1);
        XASSERT_EQ(tc.entries()[0].name, "process"); });

    runTest("collector: value truncation at 200 chars", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        std::string longValue(500, 'a');
        tc.emit(TraceEvent::VAR_BORN, 1, "big", "str", longValue);
        XASSERT(tc.entries()[0].value.size() <= 200);
        XASSERT(tc.entries()[0].value.find("...") != std::string::npos); });

    runTest("collector: clear resets everything", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.emit(TraceEvent::VAR_BORN, 1, "x");
        tc.pushCallStack("main");
        tc.addWatch("x > 5", {"x"});
        tc.clear();
        XASSERT(tc.entries().empty());
        XASSERT(tc.callStack().empty());
        XASSERT(tc.watches().empty());
        XASSERT(!tc.enabled); });

    runTest("collector: scope tracking", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.pushScope("fn:factorial", 5);
        tc.emit(TraceEvent::VAR_BORN, 6, "n");
        XASSERT_EQ(tc.entries().back().scope, "fn:factorial");
        XASSERT_EQ(tc.depth(), 1);
        tc.popScope(10);
        XASSERT_EQ(tc.depth(), 0); });

    runTest("collector: call stack", []()
            {
        TraceCollector tc;
        tc.pushCallStack("main");
        tc.pushCallStack("factorial");
        XASSERT_EQ((int)tc.callStack().size(), 2);
        tc.popCallStack();
        XASSERT_EQ((int)tc.callStack().size(), 1);
        XASSERT_EQ(tc.callStack()[0], "main"); });

    runTest("collector: JSON serialization", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.emit(TraceEvent::VAR_BORN, 1, "x", "int", "42");
        std::string json = tc.toJSON();
        XASSERT(json.find("\"trace\":[") != std::string::npos);
        XASSERT(json.find("VAR_BORN") != std::string::npos);
        XASSERT(json.find("\"name\":\"x\"") != std::string::npos);
        XASSERT(json.find("\"totalEvents\":1") != std::string::npos); });
}

// ============================================================================
// Section 4: Watch Dirty-Flag Tests
// ============================================================================

static void testWatchDirtyFlag()
{
    std::cout << "\n===== Watch Dirty-Flag Tests =====\n";

    runTest("watch: dirty flag set on dependency", []()
            {
                TraceCollector tc;
                tc.enabled = true;
                tc.addWatch("x > 10", {"x"});
                XASSERT(tc.watches()[0].dirty); // initially dirty
                tc.recordWatchResult(0, false, 1);
                XASSERT(!tc.watches()[0].dirty); // cleared after eval
                tc.markDirty("x");
                XASSERT(tc.watches()[0].dirty); // dirty again
            });

    runTest("watch: unrelated var does not dirty", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.addWatch("x > 10", {"x"});
        tc.recordWatchResult(0, false, 1);
        tc.markDirty("y"); // unrelated
        XASSERT(!tc.watches()[0].dirty); });

    runTest("watch: trigger on false→true transition", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.addWatch("x > 10", {"x"});
        tc.recordWatchResult(0, false, 1); // false→false: no trigger
        XASSERT_EQ(countEvents(tc.entries(), TraceEvent::WATCH_TRIGGERED), 0);
        tc.markDirty("x");
        tc.recordWatchResult(0, true, 2); // false→true: TRIGGER!
        XASSERT_EQ(countEvents(tc.entries(), TraceEvent::WATCH_TRIGGERED), 1); });

    runTest("watch: no trigger on true→true", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.addWatch("x > 10", {"x"});
        tc.recordWatchResult(0, true, 1); // initially true
        int prevCount = countEvents(tc.entries(), TraceEvent::WATCH_TRIGGERED);
        tc.markDirty("x");
        tc.recordWatchResult(0, true, 2); // true→true: no new trigger
        XASSERT_EQ(countEvents(tc.entries(), TraceEvent::WATCH_TRIGGERED), prevCount); });

    runTest("watch: hasDirtyWatches", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.addWatch("x > 10", {"x"});
        tc.addWatch("y < 5", {"y"});
        // Both initially dirty
        XASSERT(tc.hasDirtyWatches());
        tc.recordWatchResult(0, false, 1);
        tc.recordWatchResult(1, false, 1);
        XASSERT(!tc.hasDirtyWatches()); });
}

// ============================================================================
// Section 5: Interpreter Trace Hooks — Variable Events
// ============================================================================

static void testVariableTracing()
{
    std::cout << "\n===== Variable Tracing =====\n";

    runTest("trace: VAR_BORN on first assignment", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced("x = 42", tc);
        auto *ev = findEvent(entries, TraceEvent::VAR_BORN, "x");
        XASSERT(ev != nullptr);
        XASSERT_EQ(ev->type, "int");
        XASSERT_EQ(ev->value, "42");
        XASSERT_EQ(ev->byWhom, "assignment"); });

    runTest("trace: VAR_CHANGED on reassignment", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced("x = 1\nx = 2", tc);
        XASSERT_GE(countEvents(entries, TraceEvent::VAR_BORN), 1);
        auto *changed = findEvent(entries, TraceEvent::VAR_CHANGED, "x");
        XASSERT(changed != nullptr);
        XASSERT_EQ(changed->value, "2"); });

    runTest("trace: immutable binding", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced("immutable PI = 3.14", tc);
        auto *ev = findEvent(entries, TraceEvent::VAR_BORN, "PI");
        XASSERT(ev != nullptr);
        XASSERT_EQ(ev->byWhom, "be"); });

    runTest("trace: destructuring assignment", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced("a, b = [10, 20]", tc);
        auto *evA = findEvent(entries, TraceEvent::VAR_BORN, "a");
        auto *evB = findEvent(entries, TraceEvent::VAR_BORN, "b");
        XASSERT(evA != nullptr);
        XASSERT(evB != nullptr);
        XASSERT_EQ(evA->value, "10");
        XASSERT_EQ(evB->value, "20");
        XASSERT_EQ(evA->byWhom, "destructuring"); });

    runTest("trace: fn definition as VAR_BORN", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced("fn greet() : print(\"hi\") ;", tc);
        auto *ev = findEvent(entries, TraceEvent::VAR_BORN, "greet");
        XASSERT(ev != nullptr);
        XASSERT_EQ(ev->type, "fn"); });
}

// ============================================================================
// Section 6: Interpreter Trace Hooks — Function Events
// ============================================================================

static void testFunctionTracing()
{
    std::cout << "\n===== Function Tracing =====\n";

    runTest("trace: FN_CALLED and FN_RETURNED", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "fn add(a, b) : give a + b ;\n"
            "result = add(3, 4)\n",
            tc);
        auto *called = findEvent(entries, TraceEvent::FN_CALLED, "add");
        auto *returned = findEvent(entries, TraceEvent::FN_RETURNED, "add");
        XASSERT(called != nullptr);
        XASSERT(returned != nullptr);
        XASSERT_EQ(returned->value, "7"); });

    runTest("trace: nested function calls", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "fn double(x) : give x * 2 ;\n"
            "fn quad(x) : give double(double(x)) ;\n"
            "r = quad(5)\n",
            tc);
        int callCount = countEvents(entries, TraceEvent::FN_CALLED);
        int returnCount = countEvents(entries, TraceEvent::FN_RETURNED);
        // quad calls double twice, so: quad(1) + double(2) = 3 calls
        XASSERT_GE(callCount, 3);
        XASSERT_EQ(callCount, returnCount); });

    runTest("trace: fn filter excludes blacklisted", []()
            {
                TraceCollector tc;
                tc.filter().notrackFns.insert("helper");
                auto [output, entries] = runXellTraced(
                    "fn helper() : give 1 ;\n"
                    "fn main_fn() : give helper() ;\n"
                    "r = main_fn()\n",
                    tc);
                auto *helperCall = findEvent(entries, TraceEvent::FN_CALLED, "helper");
                auto *mainCall = findEvent(entries, TraceEvent::FN_CALLED, "main_fn");
                XASSERT(helperCall == nullptr); // filtered out
                XASSERT(mainCall != nullptr);   // not filtered
            });
}

// ============================================================================
// Section 7: Interpreter Trace Hooks — Loop Events
// ============================================================================

static void testLoopTracing()
{
    std::cout << "\n===== Loop Tracing =====\n";

    runTest("trace: for loop lifecycle", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "for i in [1, 2, 3] :\n"
            "    print(i)\n"
            ";\n",
            tc);
        XASSERT_GE(countEvents(entries, TraceEvent::LOOP_STARTED), 1);
        XASSERT_EQ(countEvents(entries, TraceEvent::LOOP_ITERATION), 3);
        XASSERT_GE(countEvents(entries, TraceEvent::LOOP_COMPLETED), 1); });

    runTest("trace: for loop break", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "for i in [1, 2, 3, 4, 5] :\n"
            "    if i == 3 : break ;\n"
            ";\n",
            tc);
        auto *broke = findEvent(entries, TraceEvent::LOOP_BROKE);
        XASSERT(broke != nullptr);
        // Should have iterated fewer than 5 times
        XASSERT(countEvents(entries, TraceEvent::LOOP_ITERATION) < 5); });

    runTest("trace: while loop", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "x = 0\n"
            "while x < 3 :\n"
            "    x = x + 1\n"
            ";\n",
            tc);
        XASSERT_GE(countEvents(entries, TraceEvent::LOOP_STARTED), 1);
        XASSERT_EQ(countEvents(entries, TraceEvent::LOOP_ITERATION), 3);
        XASSERT_GE(countEvents(entries, TraceEvent::LOOP_COMPLETED), 1); });

    runTest("trace: loop filter disabled", []()
            {
        TraceCollector tc;
        tc.filter().trackLoopFor = false;
        auto [output, entries] = runXellTraced(
            "for i in [1, 2, 3] :\n"
            "    print(i)\n"
            ";\n",
            tc);
        // Loop events should be absent
        XASSERT_EQ(countEvents(entries, TraceEvent::LOOP_STARTED), 0);
        XASSERT_EQ(countEvents(entries, TraceEvent::LOOP_ITERATION), 0); });
}

// ============================================================================
// Section 8: Interpreter Trace Hooks — Branch Events
// ============================================================================

static void testBranchTracing()
{
    std::cout << "\n===== Branch Tracing =====\n";

    runTest("trace: if true branch", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "x = 10\n"
            "if x > 5 :\n"
            "    print(\"yes\")\n"
            ";\n",
            tc);
        XASSERT_GE(countEvents(entries, TraceEvent::BRANCH_IF), 1); });

    runTest("trace: elif branch", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "x = 3\n"
            "if x > 10 :\n"
            "    print(\"big\")\n"
            "elif x > 2 :\n"
            "    print(\"medium\")\n"
            ";\n",
            tc);
        // The if should be skipped, elif should be taken
        XASSERT_GE(countEvents(entries, TraceEvent::BRANCH_SKIPPED), 1);
        XASSERT_GE(countEvents(entries, TraceEvent::BRANCH_ELIF), 1); });

    runTest("trace: else branch", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "x = 0\n"
            "if x > 10 :\n"
            "    print(\"big\")\n"
            "else :\n"
            "    print(\"small\")\n"
            ";\n",
            tc);
        XASSERT_GE(countEvents(entries, TraceEvent::BRANCH_ELSE), 1); });

    runTest("trace: branch filter disabled", []()
            {
        TraceCollector tc;
        tc.filter().trackConditions = false;
        auto [output, entries] = runXellTraced(
            "if true : print(\"yes\") ;\n",
            tc);
        XASSERT_EQ(countEvents(entries, TraceEvent::BRANCH_IF), 0); });
}

// ============================================================================
// Section 9: Interpreter Trace Hooks — Error Events
// ============================================================================

static void testErrorTracing()
{
    std::cout << "\n===== Error Tracing =====\n";

    runTest("trace: ERROR_CAUGHT in try/catch", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "try:\n"
            "  x = 1 / 0\n"
            ";\n"
            "catch e:\n"
            "  print(e->message)\n"
            ";\n",
            tc);
        auto *ev = findEvent(entries, TraceEvent::ERROR_CAUGHT);
        XASSERT(ev != nullptr); });
}

// ============================================================================
// Section 10: Snapshot Value-Copy Tests
// ============================================================================

static void testSnapshots()
{
    std::cout << "\n===== Snapshot Value-Copy Tests =====\n";

    runTest("snapshot: captures current state", []()
            {
        TraceCollector tc;
        tc.enabled = true;

        // Create a mock environment
        Environment env;
        env.define("x", XObject::makeInt(42));
        env.define("name", XObject::makeString("test"));

        std::vector<std::string> callStack = {"main", "process"};
        tc.takeSnapshot("checkpoint1", 10, &env, callStack);

        XASSERT_EQ((int)tc.snapshots().size(), 1);
        const auto &snap = tc.snapshots()[0];
        XASSERT_EQ(snap.name, "checkpoint1");
        XASSERT_EQ(snap.line, 10);
        XASSERT_EQ((int)snap.callStack.size(), 2);
        XASSERT_EQ(snap.callStack[0], "main");

        // Verify variable values are copies
        XASSERT(snap.scopeVars.count("local"));
        const auto &localVars = snap.scopeVars.at("local");
        XASSERT(localVars.count("x"));
        XASSERT_EQ(localVars.at("x").first, "int");
        XASSERT_EQ(localVars.at("x").second, "42"); });

    runTest("snapshot: scope chain captured", []()
            {
        TraceCollector tc;
        tc.enabled = true;

        Environment globalEnv;
        globalEnv.define("g", XObject::makeString("global_val"));

        Environment localEnv(&globalEnv);
        localEnv.define("l", XObject::makeInt(99));

        std::vector<std::string> callStack;
        tc.takeSnapshot("nested", 5, &localEnv, callStack);

        const auto &snap = tc.snapshots()[0];
        // Should have both local and global scopes
        XASSERT(snap.scopeVars.count("local"));
        XASSERT(snap.scopeVars.count("global"));
        XASSERT(snap.scopeVars.at("local").count("l"));
        XASSERT(snap.scopeVars.at("global").count("g")); });

    runTest("snapshot: JSON serialization", []()
            {
        TraceCollector tc;
        tc.enabled = true;

        Environment env;
        env.define("val", XObject::makeInt(7));
        tc.takeSnapshot("test_snap", 1, &env, {});

        std::string json = tc.toJSON();
        XASSERT(json.find("\"snapshots\":[") != std::string::npos);
        XASSERT(json.find("test_snap") != std::string::npos);
        XASSERT(json.find("\"totalSnapshots\":1") != std::string::npos); });
}

// ============================================================================
// Section 11: Zero-Cost When Disabled
// ============================================================================

static void testZeroCost()
{
    std::cout << "\n===== Zero-Cost When Disabled =====\n";

    runTest("zero-cost: no trace when trace_ is null", []()
            {
        // Run without any TraceCollector — should produce no events and no crashes
        Lexer lexer("x = 42\nfor i in [1,2,3] : print(i) ;");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        // trace_ is null by default
        interp.run(program);
        auto out = interp.output();
        XASSERT_EQ((int)out.size(), 3); });

    runTest("zero-cost: no trace when enabled=false", []()
            {
        TraceCollector tc;
        tc.enabled = false;
        Lexer lexer("x = 42\ny = x + 1");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        interp.setTraceCollector(&tc);
        interp.run(program);
        XASSERT(tc.entries().empty()); });
}

// ============================================================================
// Section 12: TraceEvent Name Strings
// ============================================================================

static void testEventNames()
{
    std::cout << "\n===== TraceEvent Names =====\n";

    runTest("event names: all have non-UNKNOWN names", []()
            {
        // Test a sampling of event names
        XASSERT_EQ(std::string(traceEventName(TraceEvent::VAR_BORN)), "VAR_BORN");
        XASSERT_EQ(std::string(traceEventName(TraceEvent::FN_CALLED)), "FN_CALLED");
        XASSERT_EQ(std::string(traceEventName(TraceEvent::LOOP_STARTED)), "LOOP_STARTED");
        XASSERT_EQ(std::string(traceEventName(TraceEvent::BRANCH_IF)), "BRANCH_IF");
        XASSERT_EQ(std::string(traceEventName(TraceEvent::ERROR_CAUGHT)), "ERROR_CAUGHT");
        XASSERT_EQ(std::string(traceEventName(TraceEvent::BREAKPOINT_HIT)), "BREAKPOINT_HIT");
        XASSERT_EQ(std::string(traceEventName(TraceEvent::SAMPLE_GAP)), "SAMPLE_GAP"); });
}

// ============================================================================
// Section 13: Cross-Module Debug Setup
// ============================================================================

static void testCrossModuleSetup()
{
    std::cout << "\n===== Cross-Module Debug Setup =====\n";

    runTest("cross-module: module name tracking", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.setCurrentModule("math_lib");
        tc.emit(TraceEvent::VAR_BORN, 1, "PI", "float", "3.14159");
        XASSERT_EQ(tc.entries()[0].module, "math_lib");

        tc.setCurrentModule("");
        tc.emit(TraceEvent::VAR_BORN, 2, "x", "int", "1");
        XASSERT_EQ(tc.entries()[1].module, ""); });

    runTest("cross-module: shared collector between interpreters", []()
            {
        TraceCollector tc;
        tc.enabled = true;

        Interpreter interp1;
        Interpreter interp2;
        interp1.setTraceCollector(&tc);
        interp2.setTraceCollector(&tc);

        // Both should share the same collector
        XASSERT_EQ(interp1.traceCollector(), interp2.traceCollector()); });
}

// ============================================================================
// Section 14: Integrated Tracing with Complex Code
// ============================================================================

static void testIntegratedTracing()
{
    std::cout << "\n===== Integrated Tracing =====\n";

    runTest("integrated: factorial function trace", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "fn factorial(n) :\n"
            "    if n <= 1 : give 1 ;\n"
            "    give n * factorial(n - 1)\n"
            ";\n"
            "result = factorial(5)\n"
            "print(result)\n",
            tc);
        XASSERT_EQ(output[0], "120");

        // Should have multiple FN_CALLED events (recursive)
        int calls = countEvents(entries, TraceEvent::FN_CALLED);
        XASSERT_GE(calls, 5); // factorial(5) → factorial(1): 5 calls

        // Each call should have a matching return
        int returns = countEvents(entries, TraceEvent::FN_RETURNED);
        XASSERT_EQ(calls, returns); });

    runTest("integrated: loop + branch + variable trace", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "total = 0\n"
            "for i in [1, 2, 3, 4, 5] :\n"
            "    if i % 2 == 0 :\n"
            "        total = total + i\n"
            "    ;\n"
            ";\n"
            "print(total)\n",
            tc);
        XASSERT_EQ(output[0], "6"); // 2+4=6

        // Check we have all event types
        XASSERT_GE(countEvents(entries, TraceEvent::VAR_BORN), 1);
        XASSERT_GE(countEvents(entries, TraceEvent::VAR_CHANGED), 1);
        XASSERT_GE(countEvents(entries, TraceEvent::LOOP_STARTED), 1);
        XASSERT_GE(countEvents(entries, TraceEvent::LOOP_ITERATION), 5);
        XASSERT_GE(countEvents(entries, TraceEvent::BRANCH_IF), 1); });

    runTest("integrated: try/catch trace", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "fn risky(x) :\n"
            "    give x / 0\n"
            ";\n"
            "try:\n"
            "  r = risky(10)\n"
            ";\n"
            "catch err:\n"
            "  print(\"caught\")\n"
            ";\n",
            tc);
        XASSERT_EQ(output[0], "caught");
        XASSERT_GE(countEvents(entries, TraceEvent::FN_CALLED), 1);
        XASSERT_GE(countEvents(entries, TraceEvent::ERROR_CAUGHT), 1); });
}

// ============================================================================
// Main
// ============================================================================

int main()
{
    std::cout << "========================================\n";
    std::cout << " Xell Debug System Test Suite\n";
    std::cout << "========================================\n";

    testTrackFilter();
    testSamplingState();
    testTraceCollector();
    testWatchDirtyFlag();
    testVariableTracing();
    testFunctionTracing();
    testLoopTracing();
    testBranchTracing();
    testErrorTracing();
    testSnapshots();
    testZeroCost();
    testEventNames();
    testCrossModuleSetup();
    testIntegratedTracing();

    std::cout << "\n========================================\n";
    std::cout << " Results: " << g_passed << " passed, " << g_failed << " failed\n";
    std::cout << "========================================\n";

    return g_failed > 0 ? 1 : 0;
}
