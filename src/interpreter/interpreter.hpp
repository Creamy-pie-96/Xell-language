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
#include "../builtins/builtin_registry.hpp"
#include "../parser/ast.hpp"
#include "../lib/errors/error.hpp"
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
    };
    struct ContinueSignal
    {
    };

    // ========================================================================
    // Interpreter
    // ========================================================================

    class Interpreter
    {
    public:
        Interpreter();

        /// Execute a complete program
        void run(const Program &program);

        /// Get captured output lines (one entry per print() call)
        const std::vector<std::string> &output() const { return output_; }

        /// Clear output buffer (useful for REPL between evaluations)
        void clearOutput() { output_.clear(); }

        /// Clear all state between runs
        void reset();

        /// Set the source file path (for resolving relative bring paths)
        void setSourceFile(const std::string &path) { sourceFile_ = path; }

        /// Access to global environment (testing / REPL)
        Environment &globals() { return globalEnv_; }

        /// Access to shell state (for builtins)
        ShellState &shellState() { return shellState_; }

    private:
        ShellState shellState_;
        Environment globalEnv_;
        Environment *currentEnv_;
        std::vector<std::string> output_;
        BuiltinTable builtins_;
        int callDepth_ = 0;
        static constexpr int MAX_CALL_DEPTH = 512;
        std::string sourceFile_;                        // current file path (for bring resolution)
        std::unordered_set<std::string> importedFiles_; // circular-import guard

        // Imported modules — kept alive so their AST + Env don't dangle
        struct ImportedModule
        {
            Program program;
            std::unique_ptr<Interpreter> interp;
        };
        std::vector<std::unique_ptr<ImportedModule>> importedModules_;

        void registerBuiltins();

        // ---- Statement execution -------------------------------------------

        void exec(const Stmt *stmt);
        void execBlock(const std::vector<StmtPtr> &stmts, Environment &env);
        void execAssignment(const Assignment *node);
        void execIf(const IfStmt *node);
        void execFor(const ForStmt *node);
        void execWhile(const WhileStmt *node);
        void execFnDef(const FnDef *node);
        void execGive(const GiveStmt *node);
        void execExprStmt(const ExprStmt *node);
        void execBring(const BringStmt *node);

        // ---- Expression evaluation -----------------------------------------

        XObject eval(const Expr *expr);
        XObject evalBinary(const BinaryExpr *node);
        XObject evalUnary(const UnaryExpr *node);
        XObject evalPostfix(const PostfixExpr *node);
        XObject evalCall(const CallExpr *node);
        XObject evalIndex(const IndexAccess *node);
        XObject evalMember(const MemberAccess *node);
        XObject evalList(const ListLiteral *node);
        XObject evalMap(const MapLiteral *node);

        // ---- Helpers -------------------------------------------------------

        XObject callUserFn(const XFunction &fn, std::vector<XObject> &args, int line);
        std::string interpolate(const std::string &raw, int line);
    };

} // namespace xell
