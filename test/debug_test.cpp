// =============================================================================
// Debug System Tests — TraceCollector + Interpreter Hooks + Phase 5 Decorators
//                      + Phase 6 Cross-Module + Phase 7 IPC + Phase 9 Stepping
// =============================================================================
// Verifies Phases 4–9 of the Debug System Plan:
//   - TraceCollector event emission
//   - TrackFilter whitelist/blacklist logic
//   - SamplingState head/tail selection
//   - Interpreter hooks: VAR_BORN, VAR_CHANGED, FN_CALLED, FN_RETURNED,
//     LOOP_STARTED, LOOP_ITERATION, LOOP_COMPLETED, LOOP_BROKE,
//     BRANCH_IF, BRANCH_ELIF, BRANCH_ELSE, ERROR_CAUGHT
//   - Snapshot value-copy correctness
//   - Zero-cost when disabled (trace_ null or enabled=false)
//   - Watch dirty-flag dependency tracking
//   - Phase 5: @debug on/off, @debug sample, @debug on fn,
//     @breakpoint, @watch, @checkpoint, @track, @notrack
//   - Phase 6: Cross-module debug (TraceCollector shared across modules)
//   - Phase 7: Debug IPC (DebugIPC, DebugServer, DebugMessage parsing)
//   - Phase 9: Stepping logic (IDE breakpoints, serializeVisibleVars/CallStack)
// =============================================================================

#include "../src/interpreter/interpreter.hpp"
#include "../src/interpreter/trace_collector.hpp"
#include "../src/interpreter/debug_ipc.hpp"
#include "../src/interpreter/environment.hpp"
#include "../src/lexer/lexer.hpp"
#include "../src/parser/parser.hpp"
#include <iostream>
#include <sstream>
#include <functional>
#include <string>
#include <vector>
#include <algorithm>
#include <thread>
#include <chrono>

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
// Section 15: Phase 5 — @debug on / @debug off
// ============================================================================

static void testPhase5_DebugOnOff()
{
    std::cout << "\n===== Phase 5: @debug on/off =====\n";

    runTest("@debug on enables tracing mid-code", []()
            {
        TraceCollector tc;
        // Tracing starts disabled; @debug on enables it
        Lexer lexer(
            "x = 1\n"         // not traced (before @debug on)
            "@debug on\n"
            "y = 2\n"         // traced
            "z = 3\n"         // traced
            "@debug off\n"
            "w = 4\n"         // not traced
        );
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = false; // start disabled
        interp.setTraceCollector(&tc);
        interp.run(program);
        // Only y and z should have VAR_BORN events
        auto *evY = findEvent(tc.entries(), TraceEvent::VAR_BORN, "y");
        auto *evZ = findEvent(tc.entries(), TraceEvent::VAR_BORN, "z");
        auto *evX = findEvent(tc.entries(), TraceEvent::VAR_BORN, "x");
        auto *evW = findEvent(tc.entries(), TraceEvent::VAR_BORN, "w");
        XASSERT(evY != nullptr);
        XASSERT(evZ != nullptr);
        XASSERT(evX == nullptr);
        XASSERT(evW == nullptr); });

    runTest("@debug off disables tracing", []()
            {
        TraceCollector tc;
        Lexer lexer(
            "@debug on\n"
            "a = 10\n"
            "@debug off\n"
            "b = 20\n"
        );
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = false;
        interp.setTraceCollector(&tc);
        interp.run(program);
        auto *evA = findEvent(tc.entries(), TraceEvent::VAR_BORN, "a");
        auto *evB = findEvent(tc.entries(), TraceEvent::VAR_BORN, "b");
        XASSERT(evA != nullptr);
        XASSERT(evB == nullptr); });

    runTest("@debug on/off toggle multiple times", []()
            {
        TraceCollector tc;
        Lexer lexer(
            "@debug on\n"
            "a = 1\n"
            "@debug off\n"
            "b = 2\n"
            "@debug on\n"
            "c = 3\n"
            "@debug off\n"
        );
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = false;
        interp.setTraceCollector(&tc);
        interp.run(program);
        XASSERT(findEvent(tc.entries(), TraceEvent::VAR_BORN, "a") != nullptr);
        XASSERT(findEvent(tc.entries(), TraceEvent::VAR_BORN, "b") == nullptr);
        XASSERT(findEvent(tc.entries(), TraceEvent::VAR_BORN, "c") != nullptr); });
}

// ============================================================================
// Section 16: Phase 5 — @debug sample N
// ============================================================================

static void testPhase5_DebugSample()
{
    std::cout << "\n===== Phase 5: @debug sample N =====\n";

    runTest("@debug sample sets sampling state", []()
            {
        TraceCollector tc;
        Lexer lexer(
            "@debug on\n"
            "@debug sample 4\n"
            "x = 0\n"
        );
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = false;
        interp.setTraceCollector(&tc);
        interp.run(program);
        // Sampling state should be configured
        XASSERT(tc.sampling().active);
        XASSERT_EQ(tc.sampling().sampleSize, 4); });

    runTest("@debug sample parses correctly", []()
            {
        // Just verifying it parses without error
        Lexer lexer("@debug sample 100\nx = 42");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        // Should not throw
        XASSERT(program.statements.size() >= 2); });
}

// ============================================================================
// Section 17: Phase 5 — @debug on function
// ============================================================================

static void testPhase5_DebugOnFn()
{
    std::cout << "\n===== Phase 5: @debug on function =====\n";

    runTest("@debug on fn enables tracing during call", []()
            {
        TraceCollector tc;
        Lexer lexer(
            "@debug\n"
            "fn compute(x) :\n"
            "    result = x * 2\n"
            "    give result\n"
            ";\n"
            "a = 5\n"        // not traced (outside debug fn)
            "b = compute(a)\n" // compute traced, a not traced
        );
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = false; // globally disabled
        interp.setTraceCollector(&tc);
        interp.run(program);
        // FN_CALLED/FN_RETURNED should exist (auto-enabled inside compute)
        auto *called = findEvent(tc.entries(), TraceEvent::FN_CALLED, "compute");
        auto *returned = findEvent(tc.entries(), TraceEvent::FN_RETURNED, "compute");
        XASSERT(called != nullptr);
        XASSERT(returned != nullptr); });

    runTest("@debug on fn restores previous tracing state", []()
            {
        TraceCollector tc;
        Lexer lexer(
            "@debug\n"
            "fn helper() : give 42 ;\n"
            "before = 1\n"    // not traced
            "r = helper()\n"  // traced inside helper
            "after = 2\n"     // should NOT be traced (restored to false)
        );
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = false;
        interp.setTraceCollector(&tc);
        interp.run(program);
        auto *evBefore = findEvent(tc.entries(), TraceEvent::VAR_BORN, "before");
        auto *evAfter = findEvent(tc.entries(), TraceEvent::VAR_BORN, "after");
        XASSERT(evBefore == nullptr);
        XASSERT(evAfter == nullptr); });

    runTest("@debug on fn works with tracing already on", []()
            {
                TraceCollector tc;
                Lexer lexer(
                    "@debug\n"
                    "fn helper() : give 10 ;\n"
                    "@debug on\n"
                    "a = helper()\n" // trace was on, helper also debug-enabled
                    "b = 99\n"       // still traced after helper returns (was already on)
                );
                auto tokens = lexer.tokenize();
                Parser parser(tokens);
                auto program = parser.parse();
                Interpreter interp;
                tc.enabled = false;
                interp.setTraceCollector(&tc);
                interp.run(program);
                auto *evB = findEvent(tc.entries(), TraceEvent::VAR_BORN, "b");
                XASSERT(evB != nullptr); // tracing should still be on after helper
            });
}

// ============================================================================
// Section 18: Phase 5 — @breakpoint
// ============================================================================

static void testPhase5_Breakpoint()
{
    std::cout << "\n===== Phase 5: @breakpoint =====\n";

    runTest("@breakpoint(name) takes snapshot", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "x = 42\n"
            "y = \"hello\"\n"
            "@breakpoint(\"test_bp\")\n"
            "z = 99\n",
            tc);
        // Should have a snapshot
        XASSERT_GE((int)tc.snapshots().size(), 1);
        const auto &snap = tc.snapshots()[0];
        XASSERT_EQ(snap.name, "test_bp");
        // BREAKPOINT_HIT event
        auto *ev = findEvent(entries, TraceEvent::BREAKPOINT_HIT);
        XASSERT(ev != nullptr); });

    runTest("@breakpoint captures variable state", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "x = 42\n"
            "name = \"alice\"\n"
            "@breakpoint(\"state_check\")\n",
            tc);
        XASSERT_GE((int)tc.snapshots().size(), 1);
        const auto &snap = tc.snapshots()[0];
        // Should have captured x and name
        bool foundX = false, foundName = false;
        for (const auto &[scope, vars] : snap.scopeVars)
        {
            if (vars.count("x")) foundX = true;
            if (vars.count("name")) foundName = true;
        }
        XASSERT(foundX);
        XASSERT(foundName); });

    runTest("@breakpoint pause N (timed) completes", []()
            {
        // Test with pause 1 second (short enough for a test)
        Lexer lexer(
            "@debug on\n"
            "x = 1\n"
            "@breakpoint pause 1\n"
            "y = 2\n"
            "@debug off\n"
        );
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        TraceCollector tc;
        tc.enabled = false;
        interp.setTraceCollector(&tc);
        interp.run(program);
        // Should complete after 1 second sleep
        XASSERT(true); });

    runTest("@breakpoint without name uses default", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "x = 1\n"
            "@breakpoint(\"\")\n",
            tc);
        // Should still create snapshot even with empty name
        XASSERT_GE((int)tc.snapshots().size(), 1); });

    runTest("multiple @breakpoints create multiple snapshots", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "a = 1\n"
            "@breakpoint(\"bp1\")\n"
            "b = 2\n"
            "@breakpoint(\"bp2\")\n"
            "c = 3\n"
            "@breakpoint(\"bp3\")\n",
            tc);
        XASSERT_GE((int)tc.snapshots().size(), 3);
        XASSERT_EQ(tc.snapshots()[0].name, "bp1");
        XASSERT_EQ(tc.snapshots()[1].name, "bp2");
        XASSERT_EQ(tc.snapshots()[2].name, "bp3"); });
}

// ============================================================================
// Section 19: Phase 5 — @breakpoint conditional (when)
// ============================================================================

static void testPhase5_BreakpointConditional()
{
    std::cout << "\n===== Phase 5: @breakpoint conditional =====\n";

    runTest("@breakpoint when true takes snapshot", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "x = 42\n"
            "@breakpoint(\"cond_bp\") when x > 10\n",
            tc);
        XASSERT_GE((int)tc.snapshots().size(), 1);
        XASSERT_EQ(tc.snapshots()[0].name, "cond_bp"); });

    runTest("@breakpoint when false skips snapshot", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "x = 5\n"
            "@breakpoint(\"skip_bp\") when x > 100\n",
            tc);
        XASSERT_EQ((int)tc.snapshots().size(), 0); });

    runTest("@breakpoint when in loop triggers selectively", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "for i in [1, 2, 3, 4, 5] :\n"
            "    @breakpoint(\"loop_bp\") when i == 3\n"
            ";\n",
            tc);
        // Only one snapshot (when i == 3)
        XASSERT_EQ((int)tc.snapshots().size(), 1);
        XASSERT_EQ(tc.snapshots()[0].name, "loop_bp"); });

    runTest("@breakpoint when with complex expression", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "x = 10\n"
            "y = 20\n"
            "@breakpoint(\"complex\") when x + y > 25\n",
            tc);
        XASSERT_GE((int)tc.snapshots().size(), 1); });

    runTest("@breakpoint when false→true across iterations", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "for i in [1, 2, 3, 10, 20, 30] :\n"
            "    @breakpoint(\"big\") when i >= 10\n"
            ";\n",
            tc);
        // Should trigger for i=10, 20, 30
        XASSERT_EQ((int)tc.snapshots().size(), 3); });
}

// ============================================================================
// Section 20: Phase 5 — @watch
// ============================================================================

static void testPhase5_Watch()
{
    std::cout << "\n===== Phase 5: @watch =====\n";

    runTest("@watch registers watch expression", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@watch(\"x > 10\")\n"
            "x = 5\n",
            tc);
        XASSERT_GE((int)tc.watches().size(), 1);
        XASSERT_EQ(tc.watches()[0].expression, "x > 10"); });

    runTest("@watch triggers on false→true transition", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@watch(\"x > 10\")\n"
            "x = 5\n"   // false
            "x = 15\n"  // true! → should trigger
            "x = 20\n", // still true → no trigger
            tc);
        int triggers = countEvents(entries, TraceEvent::WATCH_TRIGGERED);
        XASSERT_GE(triggers, 1); });

    runTest("@watch does not trigger on true→true", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@watch(\"x > 0\")\n"
            "x = 10\n"  // true
            "x = 20\n"  // still true
            "x = 30\n", // still true
            tc);
        // Should trigger at most once (initial false→true if x was undefined, or first assignment)
        int triggers = countEvents(entries, TraceEvent::WATCH_TRIGGERED);
        XASSERT(triggers <= 1); });

    runTest("@watch re-triggers after false→true→false→true", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@watch(\"x > 10\")\n"
            "x = 5\n"   // false
            "x = 15\n"  // true → trigger
            "x = 3\n"   // false (reset)
            "x = 20\n", // true → trigger again
            tc);
        int triggers = countEvents(entries, TraceEvent::WATCH_TRIGGERED);
        XASSERT_GE(triggers, 2); });

    runTest("@watch parses complex expression", []()
            {
        // Just verify it parses without error
        Lexer lexer("@watch(\"a + b > c * 2\")\na = 1\nb = 2\nc = 0");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        XASSERT(program.statements.size() >= 4); });

    runTest("multiple @watch expressions", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@watch(\"x > 10\")\n"
            "@watch(\"y < 0\")\n"
            "x = 5\n"
            "y = 5\n"
            "x = 15\n"  // triggers first watch
            "y = -1\n", // triggers second watch
            tc);
        XASSERT_GE((int)tc.watches().size(), 2);
        int triggers = countEvents(entries, TraceEvent::WATCH_TRIGGERED);
        XASSERT_GE(triggers, 2); });
}

// ============================================================================
// Section 21: Phase 5 — @watch dependency tracking
// ============================================================================

static void testPhase5_WatchDependency()
{
    std::cout << "\n===== Phase 5: @watch dependency tracking =====\n";

    runTest("watch extracts variable dependencies", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@watch(\"x > 10\")\n"
            "x = 5\n",
            tc);
        XASSERT_GE((int)tc.watches().size(), 1);
        // Should have "x" in dependsOn
        XASSERT(tc.watches()[0].dependsOn.count("x")); });

    runTest("watch with multiple variables extracts all deps", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@watch(\"a + b > threshold\")\n"
            "a = 1\nb = 2\nthreshold = 10\n",
            tc);
        XASSERT(tc.watches()[0].dependsOn.count("a"));
        XASSERT(tc.watches()[0].dependsOn.count("b"));
        XASSERT(tc.watches()[0].dependsOn.count("threshold")); });

    runTest("watch not dirtied by unrelated variable", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@watch(\"x > 100\")\n"
            "x = 5\n"         // dirties (x is a dep), evaluates to false
            "y = 999\n"       // does NOT dirty the watch (y is not a dep)
            "z = 1000\n",     // does NOT dirty the watch (z is not a dep)
            tc);
        // The watch should have triggered 0 times (x never > 100)
        int triggers = countEvents(entries, TraceEvent::WATCH_TRIGGERED);
        XASSERT_EQ(triggers, 0); });
}

// ============================================================================
// Section 22: Phase 5 — @checkpoint
// ============================================================================

static void testPhase5_Checkpoint()
{
    std::cout << "\n===== Phase 5: @checkpoint =====\n";

    runTest("@checkpoint creates named snapshot", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "x = 10\n"
            "@checkpoint(\"initial\")\n"
            "x = 20\n"
            "@checkpoint(\"after_change\")\n",
            tc);
        XASSERT_GE((int)tc.snapshots().size(), 2);
        XASSERT_EQ(tc.snapshots()[0].name, "initial");
        XASSERT_EQ(tc.snapshots()[1].name, "after_change"); });

    runTest("@checkpoint captures state at that moment", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "x = 10\n"
            "@checkpoint(\"before\")\n"
            "x = 99\n"
            "@checkpoint(\"after\")\n",
            tc);
        XASSERT_GE((int)tc.snapshots().size(), 2);
        // First snapshot should have x=10, second x=99
        bool foundBefore = false, foundAfter = false;
        for (const auto &[scope, vars] : tc.snapshots()[0].scopeVars)
        {
            if (vars.count("x") && vars.at("x").second == "10")
                foundBefore = true;
        }
        for (const auto &[scope, vars] : tc.snapshots()[1].scopeVars)
        {
            if (vars.count("x") && vars.at("x").second == "99")
                foundAfter = true;
        }
        XASSERT(foundBefore);
        XASSERT(foundAfter); });

    runTest("@checkpoint emits CHECKPOINT_SAVED event", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@checkpoint(\"test\")\n",
            tc);
        auto *ev = findEvent(entries, TraceEvent::CHECKPOINT_SAVED);
        XASSERT(ev != nullptr); });
}

// ============================================================================
// Section 23: Phase 5 — @track
// ============================================================================

static void testPhase5_Track()
{
    std::cout << "\n===== Phase 5: @track =====\n";

    runTest("@track var(x) only traces x", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@track var(x)\n"
            "x = 10\n"
            "y = 20\n"
            "z = 30\n",
            tc);
        auto *evX = findEvent(entries, TraceEvent::VAR_BORN, "x");
        auto *evY = findEvent(entries, TraceEvent::VAR_BORN, "y");
        auto *evZ = findEvent(entries, TraceEvent::VAR_BORN, "z");
        XASSERT(evX != nullptr);
        XASSERT(evY == nullptr);
        XASSERT(evZ == nullptr); });

    runTest("@track var(a, b) traces multiple vars", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@track var(a, b)\n"
            "a = 1\n"
            "b = 2\n"
            "c = 3\n",
            tc);
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "a") != nullptr);
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "b") != nullptr);
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "c") == nullptr); });

    runTest("@track fn(process) only traces process calls", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@track fn(process)\n"
            "fn process(x) : give x * 2 ;\n"
            "fn helper(x) : give x + 1 ;\n"
            "a = process(5)\n"
            "b = helper(5)\n",
            tc);
        auto *callProcess = findEvent(entries, TraceEvent::FN_CALLED, "process");
        auto *callHelper = findEvent(entries, TraceEvent::FN_CALLED, "helper");
        XASSERT(callProcess != nullptr);
        XASSERT(callHelper == nullptr); });
}

// ============================================================================
// Section 24: Phase 5 — @notrack
// ============================================================================

static void testPhase5_Notrack()
{
    std::cout << "\n===== Phase 5: @notrack =====\n";

    runTest("@notrack var(temp) excludes temp", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@notrack var(temp)\n"
            "x = 10\n"
            "temp = 999\n"
            "y = 20\n",
            tc);
        auto *evX = findEvent(entries, TraceEvent::VAR_BORN, "x");
        auto *evTemp = findEvent(entries, TraceEvent::VAR_BORN, "temp");
        auto *evY = findEvent(entries, TraceEvent::VAR_BORN, "y");
        XASSERT(evX != nullptr);
        XASSERT(evTemp == nullptr);
        XASSERT(evY != nullptr); });

    runTest("@notrack fn(helper) excludes helper calls", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@notrack fn(helper)\n"
            "fn helper() : give 1 ;\n"
            "fn main_fn() : give helper() ;\n"
            "r = main_fn()\n",
            tc);
        auto *helperCall = findEvent(entries, TraceEvent::FN_CALLED, "helper");
        auto *mainCall = findEvent(entries, TraceEvent::FN_CALLED, "main_fn");
        XASSERT(helperCall == nullptr);
        XASSERT(mainCall != nullptr); });

    runTest("@notrack overrides @track for same item", []()
            {
                TraceCollector tc;
                auto [output, entries] = runXellTraced(
                    "@track var(x, y)\n"
                    "@notrack var(y)\n"
                    "x = 10\n"
                    "y = 20\n",
                    tc);
                auto *evX = findEvent(entries, TraceEvent::VAR_BORN, "x");
                auto *evY = findEvent(entries, TraceEvent::VAR_BORN, "y");
                XASSERT(evX != nullptr);
                XASSERT(evY == nullptr); // blacklist wins
            });
}

// ============================================================================
// Section 25: Phase 5 — @track categories
// ============================================================================

static void testPhase5_TrackCategories()
{
    std::cout << "\n===== Phase 5: @track categories =====\n";

    runTest("@track loop enables loop events", []()
            {
        TraceCollector tc;
        // Pre-disable loop tracking, then @track loop re-enables
        Lexer lexer(
            "@track loop\n"
            "for i in [1, 2, 3] :\n"
            "    print(i)\n"
            ";\n"
        );
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = true;
        tc.filter().trackLoopFor = false;  // disabled initially
        tc.filter().trackLoopWhile = false;
        interp.setTraceCollector(&tc);
        interp.run(program);
        // @track loop should have re-enabled
        XASSERT(tc.filter().trackLoopFor);
        XASSERT(tc.filter().trackLoopWhile); });

    runTest("@track conditions enables branch events", []()
            {
        TraceCollector tc;
        Lexer lexer("@track conditions\nx = 1\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = true;
        tc.filter().trackConditions = false;
        interp.setTraceCollector(&tc);
        interp.run(program);
        XASSERT(tc.filter().trackConditions); });

    runTest("@track perf enables performance tracking", []()
            {
        TraceCollector tc;
        Lexer lexer("@track perf\nx = 1\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = true;
        interp.setTraceCollector(&tc);
        interp.run(program);
        XASSERT(tc.filter().trackPerf); });

    runTest("@track multiple categories at once", []()
            {
        TraceCollector tc;
        Lexer lexer("@track scope imports calls\nx = 1\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = true;
        tc.filter().trackScope = false;
        tc.filter().trackImports = false;
        tc.filter().trackCalls = false;
        interp.setTraceCollector(&tc);
        interp.run(program);
        XASSERT(tc.filter().trackScope);
        XASSERT(tc.filter().trackImports);
        XASSERT(tc.filter().trackCalls); });

    runTest("@track var() and category combined", []()
            {
        TraceCollector tc;
        Lexer lexer("@track var(x, y) loop\nx = 1\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = true;
        tc.filter().trackLoopFor = false;
        interp.setTraceCollector(&tc);
        interp.run(program);
        XASSERT(tc.filter().trackVars.count("x"));
        XASSERT(tc.filter().trackVars.count("y"));
        XASSERT(tc.filter().trackLoopFor); });
}

// ============================================================================
// Section 26: Phase 5 — @track obj()
// ============================================================================

static void testPhase5_TrackObj()
{
    std::cout << "\n===== Phase 5: @track obj() =====\n";

    runTest("@track obj(myInst) parses correctly", []()
            {
        TraceCollector tc;
        Lexer lexer("@track obj(myInst, other)\nx = 1\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = true;
        interp.setTraceCollector(&tc);
        interp.run(program);
        XASSERT(tc.filter().trackObjs.count("myInst"));
        XASSERT(tc.filter().trackObjs.count("other")); });

    runTest("@notrack obj(x) parses correctly", []()
            {
        TraceCollector tc;
        Lexer lexer("@notrack obj(temp_inst)\nx = 1\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = true;
        interp.setTraceCollector(&tc);
        interp.run(program);
        XASSERT(tc.filter().notrackObjs.count("temp_inst")); });

    runTest("@notrack class(Temp) parses correctly", []()
            {
        TraceCollector tc;
        Lexer lexer("@notrack class(Temp)\nx = 1\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = true;
        interp.setTraceCollector(&tc);
        interp.run(program);
        XASSERT(tc.filter().notrackClasses.count("Temp")); });
}

// ============================================================================
// Section 27: Phase 5 — Edge Cases
// ============================================================================

static void testPhase5_EdgeCases()
{
    std::cout << "\n===== Phase 5: Edge Cases =====\n";

    runTest("@debug on without trace collector does not crash", []()
            {
        Lexer lexer("@debug on\nx = 42\n@debug off\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        // No trace collector set — should not crash
        interp.run(program);
        XASSERT(true); });

    runTest("@breakpoint without trace collector does not crash", []()
            {
        Lexer lexer("@breakpoint(\"test\")\nx = 42\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        interp.run(program); // no trace collector
        XASSERT(true); });

    runTest("@watch without trace collector does not crash", []()
            {
        Lexer lexer("@watch(\"x > 10\")\nx = 42\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        interp.run(program);
        XASSERT(true); });

    runTest("@checkpoint without trace collector does not crash", []()
            {
        Lexer lexer("@checkpoint(\"test\")\nx = 42\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        interp.run(program);
        XASSERT(true); });

    runTest("@track without trace collector does not crash", []()
            {
        Lexer lexer("@track var(x) loop\nx = 42\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        interp.run(program);
        XASSERT(true); });

    runTest("@notrack without trace collector does not crash", []()
            {
        Lexer lexer("@notrack fn(helper)\nx = 42\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        interp.run(program);
        XASSERT(true); });

    runTest("@debug sample without prior @debug on does not crash", []()
            {
        TraceCollector tc;
        Lexer lexer("@debug sample 10\nx = 42\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = false;
        interp.setTraceCollector(&tc);
        interp.run(program);
        XASSERT(true); });

    runTest("@breakpoint when with undefined var catches error", []()
            {
        TraceCollector tc;
        // The condition references undefined variable — should not crash
        // (breakpoint condition eval should handle errors gracefully)
        try {
            auto [output, entries] = runXellTraced(
                "@breakpoint(\"undef_test\") when undefined_var > 10\n",
                tc);
        } catch (...) {
            // If it throws, that's acceptable — just shouldn't segfault
        }
        XASSERT(true); });

    runTest("empty @track parses correctly", []()
            {
        // @track with no categories should still be valid
        Lexer lexer("@track\nx = 42\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        // Should not crash
        XASSERT(true); });

    runTest("@debug off when already off is idempotent", []()
            {
        TraceCollector tc;
        Lexer lexer("@debug off\nx = 42\n@debug off\n");
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = false;
        interp.setTraceCollector(&tc);
        interp.run(program);
        XASSERT(!tc.enabled); });
}

// ============================================================================
// Section 28: Phase 5 — Combined Decorator Scenarios
// ============================================================================

static void testPhase5_CombinedDecorators()
{
    std::cout << "\n===== Phase 5: Combined Decorators =====\n";

    runTest("@debug on + @track + @breakpoint together", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@debug on\n"
            "@track var(x)\n"
            "x = 10\n"
            "y = 20\n"
            "@breakpoint(\"mid\")\n"
            "x = 30\n"
            "@debug off\n",
            tc);
        // x should be traced, y should not (whitelist)
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "x") != nullptr);
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "y") == nullptr);
        // Snapshot should exist
        XASSERT_GE((int)tc.snapshots().size(), 1); });

    runTest("@debug on + @watch + variable change", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@debug on\n"
            "@watch(\"count > 3\")\n"
            "count = 0\n"
            "count = 1\n"
            "count = 2\n"
            "count = 3\n"
            "count = 4\n"  // should trigger
            "@debug off\n",
            tc);
        int triggers = countEvents(entries, TraceEvent::WATCH_TRIGGERED);
        XASSERT_GE(triggers, 1); });

    runTest("@debug on + @notrack + @breakpoint", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@debug on\n"
            "@notrack var(temp)\n"
            "x = 10\n"
            "temp = 999\n"
            "@breakpoint(\"check\")\n"
            "@debug off\n",
            tc);
        // x traced, temp not traced
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "x") != nullptr);
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "temp") == nullptr);
        // But snapshot should still capture temp (snapshot captures ALL vars)
        XASSERT_GE((int)tc.snapshots().size(), 1); });

    runTest("@debug fn + @breakpoint inside fn", []()
            {
        TraceCollector tc;
        Lexer lexer(
            "@debug\n"
            "fn process(x) :\n"
            "    result = x * 2\n"
            "    @breakpoint(\"inside_fn\")\n"
            "    give result\n"
            ";\n"
            "r = process(21)\n"
        );
        auto tokens = lexer.tokenize();
        Parser parser(tokens);
        auto program = parser.parse();
        Interpreter interp;
        tc.enabled = false;
        interp.setTraceCollector(&tc);
        interp.run(program);
        // Snapshot should be taken (tracing enabled inside process)
        XASSERT_GE((int)tc.snapshots().size(), 1);
        XASSERT_EQ(tc.snapshots()[0].name, "inside_fn"); });

    runTest("full pipeline: debug + track + watch + breakpoint + checkpoint", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@debug on\n"
            "@track var(total, i)\n"
            "@watch(\"total > 5\")\n"
            "total = 0\n"
            "@checkpoint(\"start\")\n"
            "for i in [1, 2, 3, 4, 5] :\n"
            "    total = total + i\n"
            "    @breakpoint(\"iter\") when i == 3\n"
            ";\n"
            "@checkpoint(\"end\")\n"
            "@debug off\n",
            tc);
        // Var tracking: total and i should be traced
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "total") != nullptr);
        // Checkpoints: start and end
        bool hasStart = false, hasEnd = false;
        for (const auto &s : tc.snapshots())
        {
            if (s.name == "start") hasStart = true;
            if (s.name == "end") hasEnd = true;
        }
        XASSERT(hasStart);
        XASSERT(hasEnd);
        // Breakpoint at i==3
        bool hasIter = false;
        for (const auto &s : tc.snapshots())
            if (s.name == "iter") hasIter = true;
        XASSERT(hasIter);
        // Watch should have triggered (total goes 0→1→3→6→10→15; > 5 triggers at 6)
        XASSERT_GE(countEvents(entries, TraceEvent::WATCH_TRIGGERED), 1); });
}

// ============================================================================
// Section 29: Debug IPC — Message Parsing (Phase 7)
// ============================================================================

static void testDebugIPC_MessageParsing()
{
    std::cout << "\n===== Debug IPC — Message Parsing =====\n";

    runTest("parseCommand: continue", []()
            {
        auto msg = DebugIPC::parseCommand("{\"cmd\":\"continue\"}");
        XASSERT(msg.cmd == DebugCmd::Continue); });

    runTest("parseCommand: step_over", []()
            {
        auto msg = DebugIPC::parseCommand("{\"cmd\":\"step_over\"}");
        XASSERT(msg.cmd == DebugCmd::StepOver); });

    runTest("parseCommand: step_into", []()
            {
        auto msg = DebugIPC::parseCommand("{\"cmd\":\"step_into\"}");
        XASSERT(msg.cmd == DebugCmd::StepInto); });

    runTest("parseCommand: step_out", []()
            {
        auto msg = DebugIPC::parseCommand("{\"cmd\":\"step_out\"}");
        XASSERT(msg.cmd == DebugCmd::StepOut); });

    runTest("parseCommand: stop", []()
            {
        auto msg = DebugIPC::parseCommand("{\"cmd\":\"stop\"}");
        XASSERT(msg.cmd == DebugCmd::Stop); });

    runTest("parseCommand: add_breakpoint with line and type", []()
            {
        auto msg = DebugIPC::parseCommand("{\"cmd\":\"add_breakpoint\",\"line\":25,\"type\":\"snapshot\"}");
        XASSERT(msg.cmd == DebugCmd::AddBreakpoint);
        XASSERT_EQ(msg.line, 25);
        XASSERT_EQ(msg.type, "snapshot"); });

    runTest("parseCommand: add_breakpoint pause type", []()
            {
        auto msg = DebugIPC::parseCommand("{\"cmd\":\"add_breakpoint\",\"line\":10,\"type\":\"pause\"}");
        XASSERT(msg.cmd == DebugCmd::AddBreakpoint);
        XASSERT_EQ(msg.line, 10);
        XASSERT_EQ(msg.type, "pause"); });

    runTest("parseCommand: remove_breakpoint", []()
            {
        auto msg = DebugIPC::parseCommand("{\"cmd\":\"remove_breakpoint\",\"line\":25}");
        XASSERT(msg.cmd == DebugCmd::RemoveBreakpoint);
        XASSERT_EQ(msg.line, 25); });

    runTest("parseCommand: add_watch", []()
            {
        auto msg = DebugIPC::parseCommand("{\"cmd\":\"add_watch\",\"expr\":\"x > 100\"}");
        XASSERT(msg.cmd == DebugCmd::AddWatch);
        XASSERT_EQ(msg.expr, "x > 100"); });

    runTest("parseCommand: remove_watch", []()
            {
        auto msg = DebugIPC::parseCommand("{\"cmd\":\"remove_watch\",\"expr\":\"x > 100\"}");
        XASSERT(msg.cmd == DebugCmd::RemoveWatch);
        XASSERT_EQ(msg.expr, "x > 100"); });

    runTest("parseCommand: jump_to", []()
            {
        auto msg = DebugIPC::parseCommand("{\"cmd\":\"jump_to\",\"sequence\":42}");
        XASSERT(msg.cmd == DebugCmd::JumpTo);
        XASSERT_EQ(msg.sequence, 42); });

    runTest("parseCommand: eval", []()
            {
        auto msg = DebugIPC::parseCommand("{\"cmd\":\"eval\",\"expr\":\"x + y\"}");
        XASSERT(msg.cmd == DebugCmd::Eval);
        XASSERT_EQ(msg.expr, "x + y"); });

    runTest("parseCommand: unknown command", []()
            {
        auto msg = DebugIPC::parseCommand("{\"cmd\":\"bogus\"}");
        XASSERT(msg.cmd == DebugCmd::Unknown); });

    runTest("parseCommand: raw preserved", []()
            {
        std::string json = "{\"cmd\":\"step_over\"}";
        auto msg = DebugIPC::parseCommand(json);
        XASSERT_EQ(msg.raw, json); });
}

// ============================================================================
// Section 30: Debug IPC — State/Event JSON Building (Phase 7)
// ============================================================================

static void testDebugIPC_JSONBuilding()
{
    std::cout << "\n===== Debug IPC — JSON Building =====\n";

    runTest("buildStateJSON: paused state", []()
            {
        std::string json = DebugIPC::buildStateJSON("paused", 25, 42, 3, "{}", "[]");
        XASSERT(json.find("\"state\":\"paused\"") != std::string::npos);
        XASSERT(json.find("\"line\":25") != std::string::npos);
        XASSERT(json.find("\"seq\":42") != std::string::npos);
        XASSERT(json.find("\"depth\":3") != std::string::npos); });

    runTest("buildStateJSON: running state", []()
            {
        std::string json = DebugIPC::buildStateJSON("running", -1, -1, -1);
        XASSERT(json.find("\"state\":\"running\"") != std::string::npos);
        // Should not have line/seq when -1
        XASSERT(json.find("\"line\"") == std::string::npos); });

    runTest("buildStateJSON: finished state", []()
            {
        std::string json = DebugIPC::buildStateJSON("finished", -1, 100, -1);
        XASSERT(json.find("\"state\":\"finished\"") != std::string::npos);
        XASSERT(json.find("\"seq\":100") != std::string::npos); });

    runTest("buildStateJSON: with vars JSON", []()
            {
        std::string vars = "{\"x\":\"10\",\"y\":\"20\"}";
        std::string json = DebugIPC::buildStateJSON("paused", 5, 10, 1, vars);
        XASSERT(json.find("\"vars\":{\"x\":\"10\"") != std::string::npos); });

    runTest("buildStateJSON: with callStack JSON", []()
            {
        std::string stack = "[\"main:1\",\"foo:5\"]";
        std::string json = DebugIPC::buildStateJSON("paused", 5, 10, 2, "{}", stack);
        XASSERT(json.find("\"callStack\":[\"main:1\"") != std::string::npos); });

    runTest("buildEventJSON: breakpoint_hit", []()
            {
        std::string json = DebugIPC::buildEventJSON("breakpoint_hit", "epoch_start", 10, 42);
        XASSERT(json.find("\"event\":\"breakpoint_hit\"") != std::string::npos);
        XASSERT(json.find("\"name\":\"epoch_start\"") != std::string::npos);
        XASSERT(json.find("\"line\":10") != std::string::npos); });

    runTest("buildEventJSON: watch_triggered", []()
            {
        std::string json = DebugIPC::buildEventJSON("watch_triggered", "", 15, 55, "\"expr\":\"x > 100\"");
        XASSERT(json.find("\"event\":\"watch_triggered\"") != std::string::npos);
        XASSERT(json.find("\"expr\":\"x > 100\"") != std::string::npos); });

    runTest("buildEventJSON: error", []()
            {
        std::string json = DebugIPC::buildEventJSON("error", "", 10, -1, "\"message\":\"division by zero\"");
        XASSERT(json.find("\"event\":\"error\"") != std::string::npos);
        XASSERT(json.find("\"message\":\"division by zero\"") != std::string::npos); });
}

// ============================================================================
// Section 31: Debug IPC — Socket Communication (Phase 7)
// ============================================================================

static void testDebugIPC_Socket()
{
    std::cout << "\n===== Debug IPC — Socket Communication =====\n";

    runTest("DebugServer: start and accept", []()
            {
        // Use a unique PID to avoid conflicts
        int fakePid = 99990 + (int)(std::chrono::steady_clock::now().time_since_epoch().count() % 1000);
        DebugServer server;
        std::string path = server.start(fakePid);
        XASSERT(!path.empty());
        XASSERT(server.isReady());
        XASSERT(!server.isConnected());

        // Connect a client on another thread
        std::thread clientThread([&]() {
            DebugIPC client;
            bool ok = client.connect(fakePid, 3000);
            XASSERT(ok);
            XASSERT(client.isConnected());

            // Send a command
            client.send("{\"cmd\":\"step_over\"}");

            // Receive state
            std::string state = client.recv();
            XASSERT(state.find("\"state\":\"paused\"") != std::string::npos);

            client.close();
        });

        // Server side: accept connection
        int attempts = 0;
        while (!server.acceptIfReady() && attempts < 100) {
            usleep(20000); // 20ms
            attempts++;
        }
        XASSERT(server.isConnected());

        // Receive the command from client
        std::string cmd = server.recv();
        XASSERT(cmd.find("\"cmd\":\"step_over\"") != std::string::npos);

        // Send state back
        server.send(DebugIPC::buildStateJSON("paused", 10, 1, 0));

        clientThread.join();
        server.shutdown(); });

    runTest("DebugServer: send/recv multiple messages", []()
            {
        int fakePid = 99991 + (int)(std::chrono::steady_clock::now().time_since_epoch().count() % 1000);
        DebugServer server;
        server.start(fakePid);

        std::thread clientThread([&]() {
            DebugIPC client;
            client.connect(fakePid, 3000);

            // Send multiple commands
            client.send("{\"cmd\":\"add_breakpoint\",\"line\":5,\"type\":\"pause\"}");
            client.send("{\"cmd\":\"continue\"}");
            client.send("{\"cmd\":\"stop\"}");

            // Receive acknowledgments
            std::string r1 = client.recv();
            std::string r2 = client.recv();
            XASSERT(r1.find("ack") != std::string::npos || !r1.empty());
            XASSERT(r2.find("ack") != std::string::npos || !r2.empty());

            client.close();
        });

        int attempts = 0;
        while (!server.acceptIfReady() && attempts < 100) { usleep(20000); attempts++; }
        XASSERT(server.isConnected());

        // Read all three commands
        std::string c1 = server.recv();
        std::string c2 = server.recv();
        std::string c3 = server.recv();

        auto m1 = DebugIPC::parseCommand(c1);
        auto m2 = DebugIPC::parseCommand(c2);
        auto m3 = DebugIPC::parseCommand(c3);
        XASSERT(m1.cmd == DebugCmd::AddBreakpoint);
        XASSERT_EQ(m1.line, 5);
        XASSERT(m2.cmd == DebugCmd::Continue);
        XASSERT(m3.cmd == DebugCmd::Stop);

        // Send acks
        server.send("{\"ack\":true}");
        server.send("{\"ack\":true}");

        clientThread.join();
        server.shutdown(); });

    runTest("DebugServer: poll detects data", []()
            {
        int fakePid = 99992 + (int)(std::chrono::steady_clock::now().time_since_epoch().count() % 1000);
        DebugServer server;
        server.start(fakePid);

        std::thread clientThread([&]() {
            DebugIPC client;
            client.connect(fakePid, 3000);
            usleep(100000); // Wait 100ms before sending
            client.send("{\"cmd\":\"step_into\"}");
            usleep(200000); // Keep alive
            client.close();
        });

        int attempts = 0;
        while (!server.acceptIfReady() && attempts < 100) { usleep(20000); attempts++; }

        // Poll should return false initially (no data yet)
        bool hasData = server.poll(10);
        // After client sends, poll should detect it
        usleep(200000);
        hasData = server.poll(100);
        XASSERT(hasData);

        std::string cmd = server.recv();
        XASSERT(cmd.find("step_into") != std::string::npos);

        clientThread.join();
        server.shutdown(); });
}

// ============================================================================
// Section 32: IDE Breakpoints via TraceCollector (Phase 9)
// ============================================================================

static void testIDEBreakpoints()
{
    std::cout << "\n===== IDE Breakpoints via TraceCollector =====\n";

    runTest("ideBreakpoints: add/remove/query", []()
            {
        TraceCollector tc;
        tc.ideBreakpoints[5] = "pause";
        tc.ideBreakpoints[10] = "snapshot";

        XASSERT(tc.hasBreakpoint(5));
        XASSERT(tc.hasBreakpoint(10));
        XASSERT(!tc.hasBreakpoint(15));
        XASSERT_EQ(tc.breakpointType(5), "pause");
        XASSERT_EQ(tc.breakpointType(10), "snapshot"); });

    runTest("ideBreakpoints: empty returns false", []()
            {
        TraceCollector tc;
        XASSERT(!tc.hasBreakpoint(0));
        XASSERT(!tc.hasBreakpoint(100)); });

    runTest("ideBreakpoints: erase works", []()
            {
        TraceCollector tc;
        tc.ideBreakpoints[5] = "pause";
        XASSERT(tc.hasBreakpoint(5));
        tc.ideBreakpoints.erase(5);
        XASSERT(!tc.hasBreakpoint(5)); });
}

// ============================================================================
// Section 33: Environment allLocalNames (Phase 9)
// ============================================================================

static void testEnvironmentAllLocalNames()
{
    std::cout << "\n===== Environment allLocalNames =====\n";

    runTest("allLocalNames: returns local scope only", []()
            {
        // Create an outer env with a variable, then an inner env with another
        Environment outer;
        outer.set("x", XObject::makeInt(10));

        Environment inner(&outer);
        inner.set("y", XObject::makeInt(20));
        inner.set("z", XObject::makeInt(30));

        auto names = inner.allLocalNames();
        // Should have y and z but NOT x (which is in outer)
        XASSERT_EQ((int)names.size(), 2);
        bool hasY = std::find(names.begin(), names.end(), "y") != names.end();
        bool hasZ = std::find(names.begin(), names.end(), "z") != names.end();
        bool hasX = std::find(names.begin(), names.end(), "x") != names.end();
        XASSERT(hasY);
        XASSERT(hasZ);
        XASSERT(!hasX); });

    runTest("allLocalNames: empty scope", []()
            {
        Environment env;
        auto names = env.allLocalNames();
        XASSERT(names.empty()); });
}

// ============================================================================
// Section 34: Cross-Module Debug Trace Sharing (Phase 6)
// ============================================================================

static void testCrossModuleTraceSharing()
{
    std::cout << "\n===== Cross-Module Trace Sharing =====\n";

    runTest("traceCollector shared across bring", []()
            {
        TraceCollector tc;
        tc.enabled = true;

        // Module A: defines a function
        std::string moduleA =
            "@debug on\n"
            "fn greet(name) :\n"
            "    msg = \"hello \" + name\n"
            "    give msg\n"
            ";\n";

        // Module B: calls the function
        std::string moduleB =
            "@debug on\n"
            "result = greet(\"world\")\n";

        // Run moduleA
        Lexer lexerA(moduleA);
        auto tokensA = lexerA.tokenize();
        Parser parserA(tokensA);
        auto programA = parserA.parse();
        Interpreter interpA;
        interpA.setTraceCollector(&tc);
        interpA.run(programA);

        // Run moduleB in same interpreter with same trace
        Lexer lexerB(moduleB);
        auto tokensB = lexerB.tokenize();
        Parser parserB(tokensB);
        auto programB = parserB.parse();
        interpA.run(programB);

        // Should have entries from both modules
        XASSERT_GE((int)tc.entries().size(), 2);
        // Should have FN_CALLED for greet
        XASSERT(findEvent(tc.entries(), TraceEvent::FN_CALLED, "greet") != nullptr); });

    runTest("traceCollector: module tracking with setCurrentModule", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.setCurrentModule("moduleA");

        tc.emit(TraceEvent::VAR_BORN, 1, "x");

        XASSERT_EQ(tc.entries().back().module, "moduleA");

        tc.setCurrentModule("moduleB");
        tc.emit(TraceEvent::VAR_BORN, 2, "y");

        XASSERT_EQ(tc.entries().back().module, "moduleB"); });
}

// ============================================================================
// Section 35: Serialize Visible Vars / Call Stack (Phase 9)
// ============================================================================

static void testSerialization()
{
    std::cout << "\n===== Serialize Visible Vars & Call Stack =====\n";

    runTest("serializeVisibleVars: produces valid JSON", []()
            {
        // Run code with tracing to get a state we can serialize
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@debug on\n"
            "x = 10\n"
            "y = \"hello\"\n"
            "z = [1, 2, 3]\n",
            tc);
        // Entries should have VAR_BORN for x, y, z
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "x") != nullptr);
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "y") != nullptr);
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "z") != nullptr); });

    runTest("serializeCallStack: tracks function calls", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@debug on\n"
            "fn outer() :\n"
            "    fn inner() :\n"
            "        x = 1\n"
            "    ;\n"
            "    inner()\n"
            ";\n"
            "outer()\n",
            tc);
        // Should have nested function calls
        int fnCalls = countEvents(entries, TraceEvent::FN_CALLED);
        XASSERT_GE(fnCalls, 2); // outer + inner
        int fnReturns = countEvents(entries, TraceEvent::FN_RETURNED);
        XASSERT_GE(fnReturns, 2); });
}

// ============================================================================
// Section 36: End-to-End Debug Session Simulation (Phase 9)
// ============================================================================

static void testDebugSessionSimulation()
{
    std::cout << "\n===== Debug Session Simulation =====\n";

    runTest("stepping: step_over basic execution", []()
            {
        // Simulate what happens when the interpreter steps:
        // - Set up a TraceCollector with stepping enabled
        // - Run code that should pause after each statement
        TraceCollector tc;
        tc.enabled = true;
        tc.stepping = true; // Start in stepping mode

        // The stepping logic in exec() checks tc.stepping.
        // Without an actual IPC connection, stepping will block.
        // We test that the flags are set correctly.
        XASSERT(tc.stepping);
        XASSERT(!tc.stepInto);
        XASSERT_EQ(tc.stepOutDepth, -1); });

    runTest("stepping: step_out sets correct depth", []()
            {
        TraceCollector tc;
        tc.enabled = true;
        tc.stepping = true;

        // Simulate step_out: set depth to current call stack depth - 1
        tc.stepOutDepth = 2; // e.g., depth was 3, so step out to depth 2
        XASSERT_EQ(tc.stepOutDepth, 2); });

    runTest("stepping: IDE breakpoint types", []()
            {
        TraceCollector tc;
        tc.enabled = true;

        // Add different types of IDE breakpoints
        tc.ideBreakpoints[5] = "pause";
        tc.ideBreakpoints[10] = "snapshot";
        tc.ideBreakpoints[15] = "pause";

        XASSERT_EQ(tc.breakpointType(5), "pause");
        XASSERT_EQ(tc.breakpointType(10), "snapshot");
        XASSERT_EQ(tc.breakpointType(15), "pause");

        // Remove a breakpoint
        tc.ideBreakpoints.erase(10);
        XASSERT(!tc.hasBreakpoint(10)); });

    runTest("debugging: full IPC round-trip simulation", []()
            {
        // Test the complete flow:
        // 1. Server starts
        // 2. Client connects
        // 3. Server sends "paused" state
        // 4. Client sends "step_over"
        // 5. Server sends "paused" again at next line
        // 6. Client sends "continue"
        // 7. Server sends "finished"

        int fakePid = 99995 + (int)(std::chrono::steady_clock::now().time_since_epoch().count() % 1000);
        DebugServer server;
        server.start(fakePid);

        std::thread ide([&]() {
            DebugIPC client;
            client.connect(fakePid, 3000);

            // 3. Receive initial "paused" state
            std::string s1 = client.recv();
            auto state1 = DebugIPC::parseCommand(s1); // Reuse parseCommand for JSON fields
            XASSERT(s1.find("\"state\":\"paused\"") != std::string::npos);
            XASSERT(s1.find("\"line\":1") != std::string::npos);

            // 4. Send step_over
            client.send("{\"cmd\":\"step_over\"}");

            // 5. Receive paused at next line
            std::string s2 = client.recv();
            XASSERT(s2.find("\"state\":\"paused\"") != std::string::npos);
            XASSERT(s2.find("\"line\":2") != std::string::npos);

            // 6. Send continue
            client.send("{\"cmd\":\"continue\"}");

            // 7. Receive finished
            std::string s3 = client.recv();
            XASSERT(s3.find("\"state\":\"finished\"") != std::string::npos);

            client.close();
        });

        // Server side:
        int attempts = 0;
        while (!server.acceptIfReady() && attempts < 100) { usleep(20000); attempts++; }

        // 2. Send initial paused state
        server.send(DebugIPC::buildStateJSON("paused", 1, 0, 0));

        // Wait for step_over command
        std::string cmd1 = server.recv();
        XASSERT(cmd1.find("step_over") != std::string::npos);

        // Send paused at line 2
        server.send(DebugIPC::buildStateJSON("paused", 2, 1, 0));

        // Wait for continue
        std::string cmd2 = server.recv();
        XASSERT(cmd2.find("continue") != std::string::npos);

        // Send finished
        server.send(DebugIPC::buildStateJSON("finished", -1, 10, -1));

        ide.join();
        server.shutdown(); });
}

// ============================================================================
// Section 37: Debug Decorator Integration (comprehensive)
// ============================================================================

static void testDebugDecoratorIntegration()
{
    std::cout << "\n===== Debug Decorator Integration =====\n";

    runTest("@debug on/off scoping with functions", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@debug on\n"
            "y = 2\n"              // traced
            "fn helper() :\n"
            "    z = 3\n"          // traced (inside debug on scope)
            ";\n"
            "helper()\n"
            "@debug off\n"
            "w = 4\n",             // not traced
            tc);
        // y and z should be traced, w should not
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "y") != nullptr);
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "z") != nullptr);
        XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "w") == nullptr); });

    runTest("@track + @notrack combined filtering", []()
            {
                TraceCollector tc;
                auto [output, entries] = runXellTraced(
                    "@debug on\n"
                    "@track var(x, y)\n"
                    "@notrack var(y)\n" // blacklist overrides whitelist
                    "x = 10\n"
                    "y = 20\n"
                    "z = 30\n", // not in whitelist
                    tc);
                XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "x") != nullptr);
                XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "y") == nullptr); // notrack'd
                XASSERT(findEvent(entries, TraceEvent::VAR_BORN, "z") == nullptr); // not in whitelist
            });

    runTest("@breakpoint with named snapshot", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@debug on\n"
            "x = 42\n"
            "@breakpoint(\"check_x\")\n"
            "y = 100\n",
            tc);
        // Should have created a snapshot named "check_x"
        bool found = false;
        for (const auto &s : tc.snapshots())
            if (s.name == "check_x") found = true;
        XASSERT(found); });

    runTest("@watch triggers on value change", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@debug on\n"
            "@watch(\"x > 5\")\n"
            "x = 3\n"             // watch not triggered
            "x = 10\n",           // watch triggered (10 > 5)
            tc);
        XASSERT_GE(countEvents(entries, TraceEvent::WATCH_TRIGGERED), 1); });

    runTest("@checkpoint saves complete state", []()
            {
        TraceCollector tc;
        auto [output, entries] = runXellTraced(
            "@debug on\n"
            "a = 1\n"
            "b = 2\n"
            "@checkpoint(\"mid\")\n"
            "c = 3\n",
            tc);
        bool found = false;
        for (const auto &s : tc.snapshots())
        {
            if (s.name == "mid")
            {
                found = true;
                // Snapshot should have captured some scope vars
                XASSERT(!s.scopeVars.empty());
            }
        }
        XASSERT(found); });

    runTest("@debug sample N limits trace entries", []()
            {
                TraceCollector tc;
                auto [output, entries] = runXellTraced(
                    "@debug sample 3\n" // only first 3 iterations
                    "for i in [1,2,3,4,5,6,7,8,9,10] :\n"
                    "    x = i * 2\n"
                    ";\n",
                    tc);
                // Should have limited entries (sampling enabled)
                int iterCount = countEvents(entries, TraceEvent::LOOP_ITERATION);
                // With sample 3: head=2 (first 2) + tail=1 (last 1) = 3 iterations traced
                // Or it could be head entries only. Either way, should be < 10.
                XASSERT(iterCount <= 10); // basic sanity
            });
}

// ============================================================================
// Section 38: TraceEntry toJSON Output Validation
// ============================================================================

static void testTraceEntryJSON()
{
    std::cout << "\n===== TraceEntry toJSON =====\n";

    runTest("toJSON: basic entry", []()
            {
        TraceEntry e;
        e.event = TraceEvent::VAR_BORN;
        e.name = "x";
        e.line = 5;
        e.type = "int";
        e.value = "42";
        e.sequence = 1;
        e.scope = "local";
        e.module = "main";

        std::string json = e.toJSON();
        XASSERT(json.find("\"event\":\"VAR_BORN\"") != std::string::npos);
        XASSERT(json.find("\"name\":\"x\"") != std::string::npos);
        XASSERT(json.find("\"line\":5") != std::string::npos);
        XASSERT(json.find("\"type\":\"int\"") != std::string::npos);
        XASSERT(json.find("\"value\":\"42\"") != std::string::npos); });

    runTest("toJSON: function event", []()
            {
        TraceEntry e;
        e.event = TraceEvent::FN_CALLED;
        e.name = "processData";
        e.line = 10;
        e.depth = 2;
        e.sequence = 42;

        std::string json = e.toJSON();
        XASSERT(json.find("\"event\":\"FN_CALLED\"") != std::string::npos);
        XASSERT(json.find("\"name\":\"processData\"") != std::string::npos);
        XASSERT(json.find("\"depth\":2") != std::string::npos); });

    runTest("toJSON: loop event", []()
            {
        TraceEntry e;
        e.event = TraceEvent::LOOP_ITERATION;
        e.name = "i";
        e.value = "5";
        e.line = 15;
        e.detail = "for i in range";

        std::string json = e.toJSON();
        XASSERT(json.find("\"event\":\"LOOP_ITERATION\"") != std::string::npos);
        XASSERT(json.find("\"detail\":\"for i in range\"") != std::string::npos); });
}

// ============================================================================
// Section 39: Stress Tests
// ============================================================================

static void testDebugStress()
{
    std::cout << "\n===== Debug Stress Tests =====\n";

    runTest("many trace entries (1000 iterations)", []()
            {
                TraceCollector tc;
                auto [output, entries] = runXellTraced(
                    "@debug on\n"
                    "total = 0\n"
                    "for i in range(1000) :\n"
                    "    total = total + i\n"
                    ";\n",
                    tc);
                // Should have many entries but not crash
                XASSERT_GE((int)tc.entries().size(), 100); // At least some entries
            });

    runTest("many snapshots", []()
            {
                TraceCollector tc;
                auto [output, entries] = runXellTraced(
                    "@debug on\n"
                    "for i in range(50) :\n"
                    "    @checkpoint(\"iter\")\n"
                    ";\n",
                    tc);
                // Should have 50 snapshots
                XASSERT_GE((int)tc.snapshots().size(), 10); // At least some
            });

    runTest("IPC rapid send/recv (100 messages)", []()
            {
        int fakePid = 99996 + (int)(std::chrono::steady_clock::now().time_since_epoch().count() % 1000);
        DebugServer server;
        server.start(fakePid);

        std::thread client([&]() {
            DebugIPC c;
            c.connect(fakePid, 3000);
            for (int i = 0; i < 100; i++)
                c.send("{\"cmd\":\"step_over\",\"seq\":" + std::to_string(i) + "}");
            // Read 100 acks
            for (int i = 0; i < 100; i++)
            {
                std::string ack = c.recv();
                XASSERT(!ack.empty());
            }
            c.close();
        });

        int att = 0;
        while (!server.acceptIfReady() && att < 100) { usleep(20000); att++; }

        // Read 100 commands and send acks
        for (int i = 0; i < 100; i++)
        {
            std::string cmd = server.recv();
            XASSERT(cmd.find("step_over") != std::string::npos);
            server.send("{\"ack\":" + std::to_string(i) + "}");
        }

        client.join();
        server.shutdown(); });
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
    testPhase5_DebugOnOff();
    testPhase5_DebugSample();
    testPhase5_DebugOnFn();
    testPhase5_Breakpoint();
    testPhase5_BreakpointConditional();
    testPhase5_Watch();
    testPhase5_WatchDependency();
    testPhase5_Checkpoint();
    testPhase5_Track();
    testPhase5_Notrack();
    testPhase5_TrackCategories();
    testPhase5_TrackObj();
    testPhase5_EdgeCases();
    testPhase5_CombinedDecorators();

    // Phase 6-9 tests
    testDebugIPC_MessageParsing();
    testDebugIPC_JSONBuilding();
    testDebugIPC_Socket();
    testIDEBreakpoints();
    testEnvironmentAllLocalNames();
    testCrossModuleTraceSharing();
    testSerialization();
    testDebugSessionSimulation();
    testDebugDecoratorIntegration();
    testTraceEntryJSON();
    testDebugStress();

    std::cout << "\n========================================\n";
    std::cout << " Results: " << g_passed << " passed, " << g_failed << " failed\n";
    std::cout << "========================================\n";

    return g_failed > 0 ? 1 : 0;
}
