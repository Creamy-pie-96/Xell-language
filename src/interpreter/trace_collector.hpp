#pragma once

// =============================================================================
// TraceCollector — Xell's language-native debugging / tracing engine
// =============================================================================
//
// Phase 4 of the Debug System Plan (readme/debug_ide_plan.md).
//
// Zero-cost when disabled: every hook checks `if (!trace_ || !trace_->enabled)`
// before doing any work. TrackFilter further gates each event category.
//
// Snapshots store VALUE COPIES (serialised strings), never raw Environment*
// pointers — the scopes will be long gone by the time the IDE reads them.
//
// Cross-module: the same TraceCollector* is shared across child interpreters.
//
// =============================================================================

#include "environment.hpp"
#include "xobject.hpp"
#include <chrono>
#include <cstdint>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace xell
{

    // =========================================================================
    // TraceEvent — every observable event in the interpreter
    // =========================================================================

    enum class TraceEvent
    {
        // Variables
        VAR_BORN,          // First assignment (new variable)
        VAR_CHANGED,       // Re-assignment of existing variable
        VAR_ENTERED_SCOPE, // Variable becomes visible in a new scope
        VAR_EXITED_SCOPE,  // Variable goes out of scope
        VAR_DIED,          // Scope destroyed, variable unreachable

        // Functions
        FN_CALLED,   // Function call begins
        FN_RETURNED, // Function returns
        FN_ERRORED,  // Unhandled error inside function

        // Loops
        LOOP_STARTED,   // Loop begins
        LOOP_ITERATION, // Each iteration
        LOOP_BROKE,     // break hit
        LOOP_COMPLETED, // Natural end

        // Branches
        BRANCH_IF,      // if evaluated true
        BRANCH_ELIF,    // elif evaluated true
        BRANCH_ELSE,    // else taken
        BRANCH_SKIPPED, // Branch not taken (condition false)

        // Scope
        SCOPE_ENTER, // New scope created
        SCOPE_EXIT,  // Scope destroyed

        // Imports
        MODULE_LOADED, // bring resolved and executed
        MODULE_CACHED, // bring hit cache (already imported)

        // Errors
        ERROR_THROWN,     // Runtime error raised
        ERROR_CAUGHT,     // catch block activated
        ERROR_PROPAGATED, // Error bubbled up uncaught

        // Objects
        OBJ_CREATED, // Object/instance constructed

        // Debug Decorators
        BREAKPOINT_HIT,   // @breakpoint snapshot taken
        WATCH_TRIGGERED,  // @watch expression became true
        CHECKPOINT_SAVED, // @checkpoint saved state

        // Sampling
        SAMPLE_GAP, // Skipped iterations marker (sampling mode)
    };

    // Human-readable name for each event (used in JSON / timeline)
    inline const char *traceEventName(TraceEvent e)
    {
        switch (e)
        {
        case TraceEvent::VAR_BORN:
            return "VAR_BORN";
        case TraceEvent::VAR_CHANGED:
            return "VAR_CHANGED";
        case TraceEvent::VAR_ENTERED_SCOPE:
            return "VAR_ENTERED_SCOPE";
        case TraceEvent::VAR_EXITED_SCOPE:
            return "VAR_EXITED_SCOPE";
        case TraceEvent::VAR_DIED:
            return "VAR_DIED";
        case TraceEvent::FN_CALLED:
            return "FN_CALLED";
        case TraceEvent::FN_RETURNED:
            return "FN_RETURNED";
        case TraceEvent::FN_ERRORED:
            return "FN_ERRORED";
        case TraceEvent::LOOP_STARTED:
            return "LOOP_STARTED";
        case TraceEvent::LOOP_ITERATION:
            return "LOOP_ITERATION";
        case TraceEvent::LOOP_BROKE:
            return "LOOP_BROKE";
        case TraceEvent::LOOP_COMPLETED:
            return "LOOP_COMPLETED";
        case TraceEvent::BRANCH_IF:
            return "BRANCH_IF";
        case TraceEvent::BRANCH_ELIF:
            return "BRANCH_ELIF";
        case TraceEvent::BRANCH_ELSE:
            return "BRANCH_ELSE";
        case TraceEvent::BRANCH_SKIPPED:
            return "BRANCH_SKIPPED";
        case TraceEvent::SCOPE_ENTER:
            return "SCOPE_ENTER";
        case TraceEvent::SCOPE_EXIT:
            return "SCOPE_EXIT";
        case TraceEvent::MODULE_LOADED:
            return "MODULE_LOADED";
        case TraceEvent::MODULE_CACHED:
            return "MODULE_CACHED";
        case TraceEvent::ERROR_THROWN:
            return "ERROR_THROWN";
        case TraceEvent::ERROR_CAUGHT:
            return "ERROR_CAUGHT";
        case TraceEvent::ERROR_PROPAGATED:
            return "ERROR_PROPAGATED";
        case TraceEvent::OBJ_CREATED:
            return "OBJ_CREATED";
        case TraceEvent::BREAKPOINT_HIT:
            return "BREAKPOINT_HIT";
        case TraceEvent::WATCH_TRIGGERED:
            return "WATCH_TRIGGERED";
        case TraceEvent::CHECKPOINT_SAVED:
            return "CHECKPOINT_SAVED";
        case TraceEvent::SAMPLE_GAP:
            return "SAMPLE_GAP";
        default:
            return "UNKNOWN";
        }
    }

    // =========================================================================
    // TraceEntry — one recorded event
    // =========================================================================

    struct TraceEntry
    {
        int64_t timestamp_ns = 0; // Nanosecond timestamp (monotonic clock)
        int sequence = 0;         // Monotonic counter across entire trace
        TraceEvent event;
        int line = 0;       // Source line number
        std::string name;   // Variable / function / module name
        std::string type;   // XObject type string ("int", "str", ...)
        std::string value;  // toString() truncated at 200 chars
        std::string scope;  // "global", "fn:factorial", "class:Dog"
        std::string module; // Module name ("" = main file)
        std::string detail; // Extra context
        int depth = 0;      // Call / scope depth

        // Causation tracking — WHO caused this event
        std::string byWhom; // "assignment", "fn:forward", "for:i"
        int byWhomLine = 0; // Line of the causing statement

        // Serialise a single entry to JSON
        std::string toJSON() const
        {
            std::ostringstream os;
            os << "{";
            os << "\"timestamp\":" << timestamp_ns;
            os << ",\"seq\":" << sequence;
            os << ",\"event\":\"" << traceEventName(event) << "\"";
            os << ",\"line\":" << line;
            os << ",\"name\":\"" << jsonEscape(name) << "\"";
            os << ",\"type\":\"" << jsonEscape(type) << "\"";
            os << ",\"value\":\"" << jsonEscape(value) << "\"";
            os << ",\"scope\":\"" << jsonEscape(scope) << "\"";
            os << ",\"module\":\"" << jsonEscape(module) << "\"";
            os << ",\"detail\":\"" << jsonEscape(detail) << "\"";
            os << ",\"depth\":" << depth;
            os << ",\"byWhom\":\"" << jsonEscape(byWhom) << "\"";
            os << ",\"byWhomLine\":" << byWhomLine;
            os << "}";
            return os.str();
        }

    private:
        static std::string jsonEscape(const std::string &s)
        {
            std::string out;
            out.reserve(s.size() + 8);
            for (char c : s)
            {
                switch (c)
                {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    out += c;
                    break;
                }
            }
            return out;
        }
    };

    // =========================================================================
    // Snapshot — value-copy state at a @breakpoint
    // =========================================================================

    struct Snapshot
    {
        std::string name; // Breakpoint label
        int line = 0;
        int sequence = 0;
        std::string module;

        // Deep copy of all visible variables at snapshot time.
        // scope_name → { var_name → (type_str, value_str) }
        std::map<std::string,
                 std::map<std::string, std::pair<std::string, std::string>>>
            scopeVars;

        // Call stack at snapshot time (value copies, not pointers)
        std::vector<std::string> callStack;
    };

    // =========================================================================
    // TrackFilter — selective tracking (@track / @notrack)
    // =========================================================================

    struct TrackFilter
    {
        // ---- Whitelists (if non-empty, ONLY these are tracked) ----
        std::unordered_set<std::string> trackVars;
        std::unordered_set<std::string> trackFns;
        std::unordered_set<std::string> trackClasses;
        std::unordered_set<std::string> trackObjs;

        // ---- Category flags (default: all true when @debug on) ----
        bool trackConditions = true;
        bool trackLoopFor = true;
        bool trackLoopWhile = true;
        bool trackScope = true;
        bool trackImports = true;
        bool trackReturns = true;
        bool trackCalls = true;
        bool trackMutations = true;
        bool trackTypes = true;
        bool trackPerf = false;      // off by default (overhead)
        bool trackRecursion = false; // off by default

        // ---- Blacklists (always excluded) ----
        std::unordered_set<std::string> notrackVars;
        std::unordered_set<std::string> notrackFns;

        // Should this variable be tracked?
        bool shouldTrackVar(const std::string &name) const
        {
            // Blacklist always wins
            if (notrackVars.count(name))
                return false;
            // If whitelist is active, must be in it
            if (!trackVars.empty())
                return trackVars.count(name) > 0;
            return true;
        }

        // Should this function be tracked?
        bool shouldTrackFn(const std::string &name) const
        {
            if (notrackFns.count(name))
                return false;
            if (!trackFns.empty())
                return trackFns.count(name) > 0;
            return true;
        }

        // Should this event category be tracked?
        bool shouldTrackEvent(TraceEvent event) const
        {
            switch (event)
            {
            case TraceEvent::VAR_BORN:
            case TraceEvent::VAR_CHANGED:
            case TraceEvent::VAR_ENTERED_SCOPE:
            case TraceEvent::VAR_EXITED_SCOPE:
            case TraceEvent::VAR_DIED:
                return trackMutations;

            case TraceEvent::FN_CALLED:
                return trackCalls;
            case TraceEvent::FN_RETURNED:
                return trackReturns;
            case TraceEvent::FN_ERRORED:
                return true; // always track errors

            case TraceEvent::LOOP_STARTED:
            case TraceEvent::LOOP_ITERATION:
            case TraceEvent::LOOP_BROKE:
            case TraceEvent::LOOP_COMPLETED:
                return trackLoopFor || trackLoopWhile; // refined per-hook

            case TraceEvent::BRANCH_IF:
            case TraceEvent::BRANCH_ELIF:
            case TraceEvent::BRANCH_ELSE:
            case TraceEvent::BRANCH_SKIPPED:
                return trackConditions;

            case TraceEvent::SCOPE_ENTER:
            case TraceEvent::SCOPE_EXIT:
                return trackScope;

            case TraceEvent::MODULE_LOADED:
            case TraceEvent::MODULE_CACHED:
                return trackImports;

            case TraceEvent::ERROR_THROWN:
            case TraceEvent::ERROR_CAUGHT:
            case TraceEvent::ERROR_PROPAGATED:
                return true; // always track errors

            case TraceEvent::OBJ_CREATED:
                return true; // gated by class/obj whitelist at emit site

            case TraceEvent::BREAKPOINT_HIT:
            case TraceEvent::WATCH_TRIGGERED:
            case TraceEvent::CHECKPOINT_SAVED:
                return true; // debug decorators always recorded

            case TraceEvent::SAMPLE_GAP:
                return true; // always record gaps

            default:
                return true;
            }
        }

        // Reset all filters to default (track everything)
        void reset()
        {
            trackVars.clear();
            trackFns.clear();
            trackClasses.clear();
            trackObjs.clear();
            notrackVars.clear();
            notrackFns.clear();
            trackConditions = true;
            trackLoopFor = true;
            trackLoopWhile = true;
            trackScope = true;
            trackImports = true;
            trackReturns = true;
            trackCalls = true;
            trackMutations = true;
            trackTypes = true;
            trackPerf = false;
            trackRecursion = false;
        }
    };

    // =========================================================================
    // WatchExpr — dependency-aware watch with dirty-flag optimisation
    // =========================================================================

    struct WatchExpr
    {
        std::string expression; // Raw expression string
        // ExprPtr parsed;                             // Parsed AST (set later by Phase 5)
        bool lastValue = false;                    // Last evaluated boolean result
        std::unordered_set<std::string> dependsOn; // Variables this watch references
        bool dirty = false;                        // Set when a dependency changes
    };

    // =========================================================================
    // SamplingState — for @debug sample N
    // =========================================================================

    struct SamplingState
    {
        bool active = false;
        int sampleSize = 0; // N from @debug sample N
        int totalCount = 0; // Total invocations / iterations counted
        int headCount = 0;  // sampleSize / 2 (first half to trace)

        // Should this iteration / invocation be traced?
        // Traces first headCount and last (sampleSize - headCount).
        // Returns true if this invocation should be recorded.
        bool shouldTrace(int currentIndex, int totalExpected) const
        {
            if (!active)
                return true;
            int tail = sampleSize - headCount;
            if (currentIndex < headCount)
                return true;
            if (totalExpected > 0 && currentIndex >= totalExpected - tail)
                return true;
            return false;
        }

        void configure(int n)
        {
            active = true;
            sampleSize = n;
            headCount = n / 2;
            totalCount = 0;
        }

        void disable()
        {
            active = false;
            sampleSize = 0;
            totalCount = 0;
            headCount = 0;
        }
    };

    // =========================================================================
    // TraceCollector — the main tracing engine
    // =========================================================================

    class TraceCollector
    {
    public:
        // Master enable/disable. All hooks check this first.
        bool enabled = false;

        // Step-through execution state (Phase 9)
        bool stepping = false;
        bool stepInto = false;
        int stepOutDepth = -1;

        // Access the filter for selective tracking
        TrackFilter &filter() { return filter_; }
        const TrackFilter &filter() const { return filter_; }

        // Access sampling state
        SamplingState &sampling() { return sampling_; }
        const SamplingState &sampling() const { return sampling_; }

        // ------------------------------------------------------------------
        // Emit a trace event
        // ------------------------------------------------------------------

        void emit(TraceEvent event, int line, const std::string &name = "",
                  const std::string &type = "", const std::string &value = "",
                  const std::string &detail = "", const std::string &byWhom = "",
                  int byWhomLine = 0)
        {
            if (!enabled)
                return;
            if (!filter_.shouldTrackEvent(event))
                return;

            TraceEntry entry;
            entry.timestamp_ns = nowNs();
            entry.sequence = nextSequence_++;
            entry.event = event;
            entry.line = line;
            entry.name = name;
            entry.type = type;
            entry.value = truncateValue(value);
            entry.scope = currentScope_;
            entry.module = currentModule_;
            entry.detail = detail;
            entry.depth = depth_;
            entry.byWhom = byWhom;
            entry.byWhomLine = byWhomLine;

            entries_.push_back(std::move(entry));
        }

        // Convenience: emit a variable event (checks var filter)
        void emitVar(TraceEvent event, int line, const std::string &name,
                     const std::string &type, const std::string &value,
                     const std::string &byWhom = "", int byWhomLine = 0)
        {
            if (!enabled)
                return;
            if (!filter_.shouldTrackVar(name))
                return;
            if (!filter_.shouldTrackEvent(event))
                return;

            emit(event, line, name, type, value, "", byWhom, byWhomLine);

            // Mark watches dirty if they depend on this variable
            markDirty(name);
        }

        // Convenience: emit a function event (checks fn filter)
        void emitFn(TraceEvent event, int line, const std::string &name,
                    const std::string &detail = "", const std::string &value = "",
                    const std::string &byWhom = "", int byWhomLine = 0)
        {
            if (!enabled)
                return;
            if (!filter_.shouldTrackFn(name))
                return;

            emit(event, line, name, "fn", value, detail, byWhom, byWhomLine);
        }

        // ------------------------------------------------------------------
        // Scope / depth tracking
        // ------------------------------------------------------------------

        void pushScope(const std::string &scopeName, int line)
        {
            currentScope_ = scopeName;
            ++depth_;
            if (filter_.trackScope)
                emit(TraceEvent::SCOPE_ENTER, line, scopeName, "", "", "", "", 0);
        }

        void popScope(int line)
        {
            if (filter_.trackScope)
                emit(TraceEvent::SCOPE_EXIT, line, currentScope_, "", "", "", "", 0);
            --depth_;
            if (depth_ < 0)
                depth_ = 0;
            // Scope name restored by caller (they know the parent scope name)
        }

        void setCurrentScope(const std::string &scope) { currentScope_ = scope; }
        const std::string &currentScope() const { return currentScope_; }

        // ------------------------------------------------------------------
        // Module tracking (cross-module debug)
        // ------------------------------------------------------------------

        void setCurrentModule(const std::string &mod) { currentModule_ = mod; }
        const std::string &currentModule() const { return currentModule_; }

        // ------------------------------------------------------------------
        // Depth
        // ------------------------------------------------------------------

        int depth() const { return depth_; }
        void setDepth(int d) { depth_ = d; }

        // ------------------------------------------------------------------
        // Watch expressions (dirty-flag dependency tracking)
        // ------------------------------------------------------------------

        void addWatch(const std::string &expr, const std::unordered_set<std::string> &deps)
        {
            WatchExpr w;
            w.expression = expr;
            w.dependsOn = deps;
            w.dirty = true; // evaluate on first check
            watches_.push_back(std::move(w));
        }

        void markDirty(const std::string &varName)
        {
            for (auto &w : watches_)
            {
                if (w.dependsOn.count(varName))
                    w.dirty = true;
            }
        }

        bool hasDirtyWatches() const
        {
            for (const auto &w : watches_)
            {
                if (w.dirty)
                    return true;
            }
            return false;
        }

        // Returns indices of watches that just became true.
        // The caller (interpreter) evaluates each dirty watch expression
        // and calls this to record the result.
        void recordWatchResult(size_t index, bool newValue, int line)
        {
            if (index >= watches_.size())
                return;
            auto &w = watches_[index];
            w.dirty = false;
            bool oldValue = w.lastValue;
            w.lastValue = newValue;
            // Trigger on false→true transition
            if (newValue && !oldValue)
            {
                emit(TraceEvent::WATCH_TRIGGERED, line, w.expression,
                     "bool", "true", "watch", "", 0);
            }
        }

        const std::vector<WatchExpr> &watches() const { return watches_; }
        std::vector<WatchExpr> &watches() { return watches_; }

        // ------------------------------------------------------------------
        // Snapshots (@breakpoint value-copy capture)
        // ------------------------------------------------------------------

        void takeSnapshot(const std::string &name, int line,
                          const Environment *env,
                          const std::vector<std::string> &callStack)
        {
            Snapshot snap;
            snap.name = name;
            snap.line = line;
            snap.sequence = nextSequence_ - 1; // current sequence
            snap.module = currentModule_;
            snap.callStack = callStack;

            // Walk the scope chain and deep-copy all variables
            captureScopes(env, snap.scopeVars);

            snapshots_.push_back(std::move(snap));

            // Record the event
            emit(TraceEvent::BREAKPOINT_HIT, line, name, "", "",
                 "snapshot", "", 0);
        }

        const std::vector<Snapshot> &snapshots() const { return snapshots_; }

        // ------------------------------------------------------------------
        // Sampling (@debug sample N)
        // ------------------------------------------------------------------

        void setSampling(int n)
        {
            sampling_.configure(n);
        }

        void disableSampling()
        {
            sampling_.disable();
        }

        void emitSampleGap(int fromSeq, int toSeq, int line, int gapSize)
        {
            TraceEntry entry;
            entry.timestamp_ns = nowNs();
            entry.sequence = nextSequence_++;
            entry.event = TraceEvent::SAMPLE_GAP;
            entry.line = line;
            entry.name = "";
            entry.type = "";
            entry.value = "";
            entry.scope = currentScope_;
            entry.module = currentModule_;
            entry.detail = std::to_string(gapSize) + " iterations skipped";
            entry.depth = depth_;
            entry.byWhom = "";
            entry.byWhomLine = 0;
            entries_.push_back(std::move(entry));
        }

        // ------------------------------------------------------------------
        // Enable for a specific function (@debug on a fn)
        // ------------------------------------------------------------------

        void enableForFunction(const std::string &fnName)
        {
            debugFunctions_.insert(fnName);
        }

        bool isFunctionDebugEnabled(const std::string &fnName) const
        {
            return debugFunctions_.count(fnName) > 0;
        }

        // ------------------------------------------------------------------
        // Call stack management (value-copy, not pointer)
        // ------------------------------------------------------------------

        void pushCallStack(const std::string &frame) { callStack_.push_back(frame); }
        void popCallStack()
        {
            if (!callStack_.empty())
                callStack_.pop_back();
        }
        const std::vector<std::string> &callStack() const { return callStack_; }

        // ------------------------------------------------------------------
        // Read-only access to the trace log
        // ------------------------------------------------------------------

        const std::vector<TraceEntry> &entries() const { return entries_; }
        int currentSequence() const { return nextSequence_; }

        // ------------------------------------------------------------------
        // Clear / reset all state
        // ------------------------------------------------------------------

        void clear()
        {
            entries_.clear();
            snapshots_.clear();
            watches_.clear();
            debugFunctions_.clear();
            callStack_.clear();
            nextSequence_ = 0;
            depth_ = 0;
            currentScope_ = "global";
            currentModule_ = "";
            filter_.reset();
            sampling_.disable();
            enabled = false;
            stepping = false;
            stepInto = false;
            stepOutDepth = -1;
        }

        // ------------------------------------------------------------------
        // JSON serialisation of the entire trace log
        // ------------------------------------------------------------------

        std::string toJSON() const
        {
            std::ostringstream os;
            os << "{\"trace\":[";
            for (size_t i = 0; i < entries_.size(); ++i)
            {
                if (i > 0)
                    os << ",";
                os << entries_[i].toJSON();
            }
            os << "],\"snapshots\":[";
            for (size_t i = 0; i < snapshots_.size(); ++i)
            {
                if (i > 0)
                    os << ",";
                os << snapshotToJSON(snapshots_[i]);
            }
            os << "],\"totalEvents\":" << entries_.size();
            os << ",\"totalSnapshots\":" << snapshots_.size();
            os << "}";
            return os.str();
        }

    private:
        std::vector<TraceEntry> entries_;
        std::vector<Snapshot> snapshots_;
        std::vector<WatchExpr> watches_;
        std::unordered_set<std::string> debugFunctions_;
        std::vector<std::string> callStack_;

        TrackFilter filter_;
        SamplingState sampling_;

        int nextSequence_ = 0;
        int depth_ = 0;
        std::string currentScope_ = "global";
        std::string currentModule_;

        // Nanosecond timestamp from monotonic clock
        static int64_t nowNs()
        {
            auto now = std::chrono::steady_clock::now();
            return std::chrono::duration_cast<std::chrono::nanoseconds>(
                       now.time_since_epoch())
                .count();
        }

        // Truncate value strings to 200 characters
        static std::string truncateValue(const std::string &s)
        {
            if (s.size() <= 200)
                return s;
            return s.substr(0, 197) + "...";
        }

        // Deep-copy all variables from the scope chain into a nested map.
        // Each scope level gets a label: "local", "parent", "global", etc.
        static void captureScopes(
            const Environment *env,
            std::map<std::string,
                     std::map<std::string, std::pair<std::string, std::string>>> &out)
        {
            int level = 0;
            const Environment *e = env;
            while (e)
            {
                std::string scopeLabel = (level == 0) ? "local" : (e->parent() ? "parent_" + std::to_string(level) : "global");
                auto names = e->allNames();
                // Filter to only names owned by this scope level
                // (allNames walks up, but we want per-scope)
                for (const auto &n : names)
                {
                    if (e->hasLocal(n))
                    {
                        try
                        {
                            XObject val = e->get(n, 0);
                            out[scopeLabel][n] = {std::string(xtype_name(val.type())), val.toString()};
                        }
                        catch (...)
                        {
                            out[scopeLabel][n] = {"?", "?"};
                        }
                    }
                }
                e = e->parent();
                ++level;
            }
        }

        // JSON-escape a string
        static std::string jsonEscape(const std::string &s)
        {
            std::string out;
            out.reserve(s.size() + 8);
            for (char c : s)
            {
                switch (c)
                {
                case '"':
                    out += "\\\"";
                    break;
                case '\\':
                    out += "\\\\";
                    break;
                case '\n':
                    out += "\\n";
                    break;
                case '\r':
                    out += "\\r";
                    break;
                case '\t':
                    out += "\\t";
                    break;
                default:
                    out += c;
                    break;
                }
            }
            return out;
        }

        // Serialise a Snapshot to JSON
        static std::string snapshotToJSON(const Snapshot &snap)
        {
            std::ostringstream os;
            os << "{\"name\":\"" << jsonEscape(snap.name) << "\"";
            os << ",\"line\":" << snap.line;
            os << ",\"seq\":" << snap.sequence;
            os << ",\"module\":\"" << jsonEscape(snap.module) << "\"";
            os << ",\"callStack\":[";
            for (size_t i = 0; i < snap.callStack.size(); ++i)
            {
                if (i > 0)
                    os << ",";
                os << "\"" << jsonEscape(snap.callStack[i]) << "\"";
            }
            os << "],\"scopes\":{";
            bool firstScope = true;
            for (const auto &[scopeName, vars] : snap.scopeVars)
            {
                if (!firstScope)
                    os << ",";
                firstScope = false;
                os << "\"" << jsonEscape(scopeName) << "\":{";
                bool firstVar = true;
                for (const auto &[varName, tv] : vars)
                {
                    if (!firstVar)
                        os << ",";
                    firstVar = false;
                    os << "\"" << jsonEscape(varName) << "\":{\"type\":\""
                       << jsonEscape(tv.first) << "\",\"value\":\""
                       << jsonEscape(tv.second) << "\"}";
                }
                os << "}";
            }
            os << "}}";
            return os.str();
        }
    };

} // namespace xell
