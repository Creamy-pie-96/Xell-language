#pragma once

// =============================================================================
// Interpreter — Xell's tree-walking evaluator
// =============================================================================
//
// Walks the AST produced by the Parser and executes it.
//
// Design choices:
//   - Lexical scoping: closures capture their definition environment.
//   - Block scoping: every `:` ... `;` block creates a child Environment.
//   - `give` (return) uses an exception (GiveSignal) for stack unwinding.
//   - Built-in functions are modular — see src/builtins/*.hpp.
//   - Output is captured into a vector<string> for easy testing.
//
// =============================================================================

#include "environment.hpp"
#include "xobject.hpp"
#include "shell_state.hpp"
#include "trace_collector.hpp"
#include "../builtins/builtin_registry.hpp"
#include "../builtins/module_registry.hpp"
#include "../module/module_resolver.hpp"
#include "../parser/ast.hpp"
#include "../lib/errors/error.hpp"
#include "../common/dialect_convert.hpp"
#include <string>
#include <vector>
#include <functional>
#include <unordered_map>
#include <unordered_set>

namespace xell
{

    // ---- Control-flow signal for give (return) ------------------------------

    struct GiveSignal
    {
        XObject value;
    };

    // ---- Control-flow signals for break/continue ---------------------------

    struct BreakSignal
    {
        XObject value;         // value carried out of expression-mode loops
        bool hasValue = false; // true when break VALUE was used
    };
    struct ContinueSignal
    {
    };

    // ---- Control-flow signal for yield (generators) -------------------------

    struct YieldSignal
    {
        XObject value;
    };

    // ========================================================================
    // Interpreter
    // ========================================================================

    class Interpreter
    {
    public:
        Interpreter();
        ~Interpreter();

        /// Execute a complete program
        void run(const Program &program);

        /// Get captured output lines (one entry per print() call)
        const std::vector<std::string> &output() const { return output_; }

        /// Clear output buffer (useful for REPL between evaluations)
        void clearOutput() { output_.clear(); }

        /// Clear all state between runs
        void reset();

        /// Set the source file path (for resolving relative bring paths)
        void setSourceFile(const std::string &path)
        {
            sourceFile_ = path;
            moduleResolver_.setSourceFile(path);
        }

        /// Set CLI arguments (for __args__ dunder)
        void setCliArgs(const std::vector<std::string> &args) { cliArgs_ = args; }

        /// Mark this interpreter as running the main file (for __name__ == "__main__")
        void setIsMainFile(bool val) { isMainFile_ = val; }

        /// Access to global environment (testing / REPL)
        Environment &globals() { return globalEnv_; }

        /// Access to shell state (for builtins)
        ShellState &shellState() { return shellState_; }

        /// Access to module registry (for testing / introspection)
        const ModuleRegistry &moduleRegistry() const { return moduleRegistry_; }

        /// Set the trace collector (shared across modules for cross-module debug)
        void setTraceCollector(TraceCollector *tc) { trace_ = tc; }

        /// Get the trace collector (may be nullptr when debug is off)
        TraceCollector *traceCollector() const { return trace_; }

        /// Programmatically load a built-in module (same as `bring * from "mod"`)
        /// Useful for C++ tests and embedding — avoids modifying Xell source strings.
        void loadModule(const std::string &moduleName);

        /// Enable streaming output: print() writes to stdout immediately.
        /// Essential when running as a subprocess so output appears in real time
        /// (e.g. print() before input() prompts).
        void setStreamOutput(bool val) { streamOutput_ = val; }
        bool streamOutput() const { return streamOutput_; }

        /// Set a custom input reader.  When set, the input() builtin calls
        /// this instead of std::getline.  The REPL uses this to temporarily
        /// restore canonical terminal mode around the actual read so that
        /// echo and line-editing work even though the REPL itself runs in
        /// raw mode.
        using InputHook = std::function<std::string(const std::string & /*prompt*/)>;
        void setInputHook(InputHook hook) { inputHook_ = std::move(hook); }
        const InputHook &inputHook() const { return inputHook_; }

        /// Check if a function is available in the active builtin table
        bool hasActiveBuiltin(const std::string &name) const { return builtins_.count(name) > 0; }

        /// Check if a function exists in any builtin (active or module)
        bool hasAnyBuiltin(const std::string &name) const { return allBuiltins_.count(name) > 0; }

    private:
        ShellState shellState_;
        Environment globalEnv_;
        Environment *currentEnv_;
        std::vector<std::string> output_;
        BuiltinTable builtins_;    // Tier 1: always-available builtins
        BuiltinTable allBuiltins_; // ALL builtins (Tier 1 + Tier 2)
        ModuleRegistry moduleRegistry_;
        ModuleResolver moduleResolver_;
        int callDepth_ = 0;
        static constexpr int MAX_CALL_DEPTH = 512;
        std::string sourceFile_;                        // current file path (for bring resolution)
        std::unordered_set<std::string> importedFiles_; // circular-import guard
        std::vector<std::string> cliArgs_;              // CLI arguments (for __args__ dunder)
        bool isMainFile_ = true;                        // true when running the entry-point file
        bool streamOutput_ = false;                     // when true, print() writes to stdout immediately
        InputHook inputHook_;                           // custom stdin reader (used by REPL)

        // ---- Debug / Trace ----
        TraceCollector *trace_ = nullptr; // Non-owning. Null when debug is off.

        // ---- Call stack for error tracebacks ----
        struct CallFrame
        {
            std::string functionName; // e.g. "validate", "<module>", "MyClass.__init__"
            int callLine;             // line where the function was called from
        };
        std::vector<CallFrame> callStack_;     // Live call stack (function frames)
        std::vector<CallFrame> lastTraceback_; // Snapshot at point of error (before unwind)
        bool tracebackCaptured_ = false;       // True once snapshot taken for current error

    public:
        /// Get a formatted traceback string from the last captured traceback.
        /// Used by the REPL / main to display Python-style tracebacks.
        std::string formatTraceback(int errorLine) const;

        /// Get the raw call stack (for testing / introspection)
        const std::vector<CallFrame> &callFrames() const { return callStack_; }

        /// Get the last captured traceback (for testing)
        const std::vector<CallFrame> &lastTraceback() const { return lastTraceback_; }

        /// Access the current environment (used by comprehension helpers)
        Environment *currentEnv() const { return currentEnv_; }

    private:
        // Module system: tracks names that have been export-declared in current scope.
        // Used by execModuleDef to collect exports when building an XModule.
        std::unordered_set<std::string> exportedNames_;

        // Access control: tracks the class that owns the currently executing method.
        // Set by callUserFn when calling a method (has "self" param). nullptr when
        // executing top-level code or non-method functions.
        const XStructDef *executingMethodClass_ = nullptr;

        // Imported modules — kept alive so their AST + Env don't dangle
        struct ImportedModule
        {
            Program program;
            std::unique_ptr<Interpreter> interp;
        };
        std::vector<std::unique_ptr<ImportedModule>> importedModules_;

        void registerBuiltins();

        // Built-in Error class hierarchy (Gap 1.8)
        // Maps C++ error category names (e.g. "TypeError", "IndexError")
        // to their XStructDef so caught errors become proper instances.
        std::unordered_map<std::string, std::shared_ptr<XStructDef>> errorClasses_;
        void registerErrorClasses();

        struct ThreadTask
        {
            std::thread worker;
            std::mutex mtx;
            std::condition_variable cv;
            bool done = false;
            bool joined = false;
            XObject result = XObject::makeNone();
            std::vector<std::string> output;
            std::exception_ptr error;
        };

        std::unordered_map<int, std::shared_ptr<ThreadTask>> threadTasks_;
        std::unordered_map<int, std::shared_ptr<std::mutex>> mutexHandles_;
        std::mutex threadStateMutex_;
        int nextThreadTaskId_ = 1;
        int nextMutexHandleId_ = 1;

        void cleanupThreadTasks();

        // ---- Statement execution -------------------------------------------

        void exec(const Stmt *stmt);
        void execBlock(const std::vector<StmtPtr> &stmts, Environment &env);
        void execAssignment(const Assignment *node);
        void execIf(const IfStmt *node);
        void execFor(const ForStmt *node);
        void execWhile(const WhileStmt *node);
        void execLoop(const LoopStmt *node);
        void execDoWhile(const DoWhileStmt *node);
        void execFnDef(const FnDef *node);
        void execGive(const GiveStmt *node);
        void execExprStmt(const ExprStmt *node);

        /// Convert any iterable source to a materialized vector of XObjects.
        /// Handles lists, tuples, maps, sets, strings, instances with __iter__.
        /// Lazy iterators are drained into a vector here; for-loops should prefer
        /// `normalizeIterableSource()` + `nextLazyIterableValue()` when possible.
        std::vector<XObject> materializeIterable(XObject &src, int line);

        /// Normalize an iteration source. Instance values may be replaced by the
        /// result of `__iter__()` when present.
        XObject normalizeIterableSource(XObject src, int line);

        /// True when a source can be consumed lazily (`generator` or iterator
        /// object exposing `__next__`).
        bool isLazyIterableSource(const XObject &src) const;

        /// Pull the next value from any lazy iterable source.
        std::pair<bool, XObject> nextLazyIterableValue(XObject &src, int line);

        /// Pull the next value from a generator. Returns {false, value} on yield,
        /// {true, none} when the generator is exhausted.
        std::pair<bool, XObject> nextGeneratorValue(XObject &gen, int line);
        void execBring(const BringStmt *node);
        void execModuleDef(const ModuleDef *node);
        void execExportDecl(const ExportDecl *node);
        void execTryCatch(const TryCatchStmt *node);
        void execThrow(const ThrowStmt *node);
        void execInCase(const InCaseStmt *node);
        void execLet(const LetStmt *node);
        void execDestructuring(const DestructuringAssignment *node);
        void execEnumDef(const EnumDef *node);
        void execDecoratedFnDef(const DecoratedFnDef *node);
        void execDecoratedClassDef(const DecoratedClassDef *node);
        void execStructDef(const StructDef *node);
        void execClassDef(const ClassDef *node);
        void execInterfaceDef(const InterfaceDef *node);
        void execMemberAssignment(const MemberAssignment *node);
        void execIndexAssignment(const IndexAssignment *node);

        /// Apply a binary arithmetic/string operation to two values.
        /// Used by augmented assignment (+=, -=, etc.) to avoid duplicating
        /// the full evalBinary() logic.
        XObject applyBinaryOp(const std::string &op, XObject left, XObject right, int line);

        /// Execute a .xell file and return all module objects defined within.
        /// Used by ModuleResolver for file-based module resolution.
        std::unordered_map<std::string, std::shared_ptr<XModule>>
        executeModuleFile(const std::string &filePath);

        // ---- Expression evaluation -----------------------------------------

        XObject eval(const Expr *expr);
        XObject evalBinary(const BinaryExpr *node);
        XObject evalChainedComparison(const ChainedComparisonExpr *node);
        XObject evalUnary(const UnaryExpr *node);
        XObject evalPostfix(const PostfixExpr *node);
        XObject evalCall(const CallExpr *node);
        XObject evalIndex(const IndexAccess *node);
        XObject evalSlice(const SliceExpr *node);
        XObject evalMember(const MemberAccess *node);
        XObject evalList(const ListLiteral *node);
        XObject evalTuple(const TupleLiteral *node);
        XObject evalSet(const SetLiteral *node);
        XObject evalFrozenSet(const FrozenSetLiteral *node);
        XObject evalShellCmd(const ShellCmdExpr *node);
        XObject evalMap(const MapLiteral *node);
        XObject evalListComprehension(const ListComprehension *node);
        XObject evalSetComprehension(const SetComprehension *node);
        XObject evalMapComprehension(const MapComprehension *node);
        void runCompClauses(const std::vector<CompClause> &clauses,
                            size_t idx, const std::function<void()> &emitFn);
        XObject evalTernary(const TernaryExpr *node);
        XObject evalIfExpr(const IfExpr *node);
        XObject evalForExpr(const ForExpr *node);
        XObject evalWhileExpr(const WhileExpr *node);
        XObject evalLoopExpr(const LoopExpr *node);
        XObject evalInCaseExpr(const InCaseExpr *node);
        XObject evalLambda(const LambdaExpr *node);
        XObject evalSpread(const SpreadExpr *node);
        XObject evalYield(const YieldExpr *node);
        XObject evalAwait(const AwaitExpr *node);
        XObject evalBytes(const BytesLiteral *node);

        // ---- Helpers -------------------------------------------------------

        XObject callUserFn(const XFunction &fn, std::vector<XObject> &args, int line,
                           std::shared_ptr<XStructDef> parentClassDef = nullptr,
                           const std::vector<std::pair<std::string, XObject>> *namedArgs = nullptr);
        XObject createGenerator(const XFunction &fn, std::vector<XObject> &args, int line);
        std::string interpolate(const std::string &raw, int line);

        // Debug serialization helpers (Phase 9 — step-through execution)
        std::string serializeVisibleVars() const;
        std::string serializeCallStack() const;

        // Call a magic method (__dunder__) on an instance if it exists.
        // Returns true and sets result if the method was found and called.
        bool callMagicMethod(const XObject &instance, const std::string &methodName,
                             std::vector<XObject> &args, int line, XObject &result);

        // Access control: check if the current scope can access a member
        // with the given access level on the given class. Throws AccessError if not.
        void checkAccess(AccessLevel access, const std::string &memberName,
                         const XStructDef &owningClass, int line);

        // Generator yield context — when non-null, yield is valid
        GeneratorState *activeGeneratorState_ = nullptr;

        // Static callback for __hash__ — called from xobject.cpp hash system
        static bool instanceHashCallback(const XObject &instance, int64_t &result);
        static Interpreter *currentInterpreter_;
    };

} // namespace xell
