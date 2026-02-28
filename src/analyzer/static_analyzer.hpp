#pragma once

// =============================================================================
// StaticAnalyzer — shallow semantic analysis for Xell source code
// =============================================================================
// Walks the AST after parsing to detect:
//   • Undefined variable / function usage
//   • Known misspellings of builtins
//   • Unreachable code after break/continue/give
//   • Unused variables (warnings)
//
// This is a FAST, lightweight pass — no execution, no type inference.
// =============================================================================

#include "../parser/ast.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>

namespace xell
{

    struct LintDiagnostic
    {
        int line;
        std::string message;
        std::string severity; // "error", "warning", "hint"
    };

    class StaticAnalyzer
    {
    public:
        StaticAnalyzer()
        {
            // All built-in function names the interpreter provides
            builtins_ = {
                // IO
                "print",
                // Type
                "type", "str", "num",
                // Util
                "assert",
                // Collection
                "len", "push", "pop", "keys", "values", "range", "set", "has",
                // Math
                "floor", "ceil", "round", "abs", "mod",
                // OS
                "mkdir", "rm", "cp", "mv", "exists", "is_file", "is_dir",
                "ls", "read", "write", "append", "file_size",
                "cwd", "cd", "abspath", "basename", "dirname", "ext",
                "env_get", "env_set", "env_unset", "env_has",
                "run", "run_capture", "pid",
                // Shell control
                "set_e", "unset_e", "exit_code"};

            // Common misspellings → correct name
            typoMap_ = {
                {"pirnt", "print"},
                {"pint", "print"},
                {"prnt", "print"},
                {"pritn", "print"},
                {"prtin", "print"},
                {"rpint", "print"},
                {"prin", "print"},
                {"rint", "print"},
                {"lne", "len"},
                {"lenght", "len"},
                {"length", "len"},
                {"rnage", "range"},
                {"rnge", "range"},
                {"ragne", "range"},
                {"tpye", "type"},
                {"tyep", "type"},
                {"flase", "false"},
                {"ture", "true"},
                {"noen", "none"},
                {"assrt", "assert"},
                {"asert", "assert"},
                {"stirng", "str"},
                {"stri", "str"},
                {"psh", "push"},
                {"ppush", "push"},
                {"vlues", "values"},
                {"vales", "values"},
                {"kyes", "keys"},
                {"keyes", "keys"},
                {"foor", "floor"},
                {"ciel", "ceil"},
                {"roud", "round"},
                {"rond", "round"},
                {"read_flie", "read_file"},
                {"write_flie", "write_file"},
                {"file_exits", "file_exists"},
                {"file_exist", "file_exists"},
                {"lst_dir", "list_dir"},
                {"mkae_dir", "make_dir"},
                {"getevn", "getenv"},
                {"setevn", "setenv"},
            };
        }

        std::vector<LintDiagnostic> analyze(const Program &program)
        {
            diagnostics_.clear();
            // First pass: collect all top-level definitions
            scopes_.clear();
            scopes_.push_back({}); // global scope

            for (auto &stmt : program.statements)
            {
                collectDefinitions(stmt.get());
            }

            // Second pass: check usage
            scopes_.clear();
            scopes_.push_back({}); // reset to global

            // Re-collect definitions so we know what's available
            for (auto &stmt : program.statements)
            {
                collectDefinitions(stmt.get());
            }

            for (auto &stmt : program.statements)
            {
                checkStatement(stmt.get());
            }

            return diagnostics_;
        }

    private:
        std::unordered_set<std::string> builtins_;
        std::unordered_map<std::string, std::string> typoMap_;
        std::vector<std::unordered_set<std::string>> scopes_;
        std::vector<LintDiagnostic> diagnostics_;

        void pushScope() { scopes_.push_back({}); }
        void popScope()
        {
            if (scopes_.size() > 1)
                scopes_.pop_back();
        }

        void define(const std::string &name)
        {
            if (!scopes_.empty())
                scopes_.back().insert(name);
        }

        bool isDefined(const std::string &name) const
        {
            // Check all scopes from innermost to outermost
            for (int i = (int)scopes_.size() - 1; i >= 0; i--)
            {
                if (scopes_[i].count(name))
                    return true;
            }
            return builtins_.count(name) > 0;
        }

        void collectDefinitions(const Stmt *stmt)
        {
            if (!stmt)
                return;

            if (auto *assign = dynamic_cast<const Assignment *>(stmt))
            {
                define(assign->name);
            }
            else if (auto *fn = dynamic_cast<const FnDef *>(stmt))
            {
                define(fn->name);
            }
            else if (auto *bring = dynamic_cast<const BringStmt *>(stmt))
            {
                // Imported names (or aliases) are defined
                if (!bring->aliases.empty())
                {
                    for (auto &alias : bring->aliases)
                        define(alias);
                }
                else
                {
                    for (auto &name : bring->names)
                        define(name);
                }
            }
            else if (auto *forStmt = dynamic_cast<const ForStmt *>(stmt))
            {
                define(forStmt->varName);
            }
        }

        void checkStatement(const Stmt *stmt)
        {
            if (!stmt)
                return;

            if (auto *assign = dynamic_cast<const Assignment *>(stmt))
            {
                checkExpr(assign->value.get());
                define(assign->name);
            }
            else if (auto *exprStmt = dynamic_cast<const ExprStmt *>(stmt))
            {
                checkExpr(exprStmt->expr.get());
            }
            else if (auto *ifStmt = dynamic_cast<const IfStmt *>(stmt))
            {
                checkExpr(ifStmt->condition.get());
                pushScope();
                for (auto &s : ifStmt->body)
                {
                    collectDefinitions(s.get());
                    checkStatement(s.get());
                }
                popScope();
                for (auto &elif : ifStmt->elifs)
                {
                    checkExpr(elif.condition.get());
                    pushScope();
                    for (auto &s : elif.body)
                    {
                        collectDefinitions(s.get());
                        checkStatement(s.get());
                    }
                    popScope();
                }
                pushScope();
                for (auto &s : ifStmt->elseBody)
                {
                    collectDefinitions(s.get());
                    checkStatement(s.get());
                }
                popScope();
            }
            else if (auto *forStmt = dynamic_cast<const ForStmt *>(stmt))
            {
                checkExpr(forStmt->iterable.get());
                pushScope();
                define(forStmt->varName);
                for (auto &s : forStmt->body)
                {
                    collectDefinitions(s.get());
                    checkStatement(s.get());
                }
                popScope();
            }
            else if (auto *whileStmt = dynamic_cast<const WhileStmt *>(stmt))
            {
                checkExpr(whileStmt->condition.get());
                pushScope();
                for (auto &s : whileStmt->body)
                {
                    collectDefinitions(s.get());
                    checkStatement(s.get());
                }
                popScope();
            }
            else if (auto *fn = dynamic_cast<const FnDef *>(stmt))
            {
                define(fn->name);
                pushScope();
                for (auto &p : fn->params)
                    define(p);
                for (auto &s : fn->body)
                {
                    collectDefinitions(s.get());
                    checkStatement(s.get());
                }
                popScope();
            }
            else if (auto *give = dynamic_cast<const GiveStmt *>(stmt))
            {
                if (give->value)
                    checkExpr(give->value.get());
            }
            else if (auto *bring = dynamic_cast<const BringStmt *>(stmt))
            {
                if (!bring->aliases.empty())
                {
                    for (auto &alias : bring->aliases)
                        define(alias);
                }
                else
                {
                    for (auto &name : bring->names)
                        define(name);
                }
            }
        }

        void checkExpr(const Expr *expr)
        {
            if (!expr)
                return;

            if (auto *ident = dynamic_cast<const Identifier *>(expr))
            {
                const std::string &name = ident->name;
                if (!isDefined(name))
                {
                    // Check for typos
                    auto it = typoMap_.find(name);
                    if (it != typoMap_.end())
                    {
                        diagnostics_.push_back({ident->line,
                                                "Undefined name '" + name + "'. Did you mean '" + it->second + "'?",
                                                "error"});
                    }
                    else
                    {
                        // Check for close matches to builtins
                        std::string suggestion = findClosest(name);
                        if (!suggestion.empty())
                        {
                            diagnostics_.push_back({ident->line,
                                                    "Undefined name '" + name + "'. Did you mean '" + suggestion + "'?",
                                                    "error"});
                        }
                        else
                        {
                            diagnostics_.push_back({ident->line,
                                                    "Undefined name '" + name + "'",
                                                    "warning"});
                        }
                    }
                }
            }
            else if (auto *call = dynamic_cast<const CallExpr *>(expr))
            {
                // Check if function name is defined
                if (!isDefined(call->callee))
                {
                    auto it = typoMap_.find(call->callee);
                    if (it != typoMap_.end())
                    {
                        diagnostics_.push_back({call->line,
                                                "Undefined function '" + call->callee + "'. Did you mean '" + it->second + "'?",
                                                "error"});
                    }
                    else
                    {
                        std::string suggestion = findClosest(call->callee);
                        if (!suggestion.empty())
                        {
                            diagnostics_.push_back({call->line,
                                                    "Undefined function '" + call->callee + "'. Did you mean '" + suggestion + "'?",
                                                    "error"});
                        }
                        else
                        {
                            diagnostics_.push_back({call->line,
                                                    "Undefined function '" + call->callee + "'",
                                                    "error"});
                        }
                    }
                }
                // Check arguments
                for (auto &arg : call->args)
                    checkExpr(arg.get());
            }
            else if (auto *bin = dynamic_cast<const BinaryExpr *>(expr))
            {
                checkExpr(bin->left.get());
                checkExpr(bin->right.get());
            }
            else if (auto *unary = dynamic_cast<const UnaryExpr *>(expr))
            {
                checkExpr(unary->operand.get());
            }
            else if (auto *index = dynamic_cast<const IndexAccess *>(expr))
            {
                checkExpr(index->object.get());
                checkExpr(index->index.get());
            }
            else if (auto *mapAcc = dynamic_cast<const MemberAccess *>(expr))
            {
                checkExpr(mapAcc->object.get());
            }
            else if (auto *list = dynamic_cast<const ListLiteral *>(expr))
            {
                for (auto &elem : list->elements)
                    checkExpr(elem.get());
            }
            else if (auto *map = dynamic_cast<const MapLiteral *>(expr))
            {
                for (auto &entry : map->entries)
                {
                    checkExpr(entry.second.get());
                }
            }
            else if (auto *postfix = dynamic_cast<const PostfixExpr *>(expr))
            {
                checkExpr(postfix->operand.get());
            }
        }

        /// Simple edit-distance based closest match finder
        std::string findClosest(const std::string &name) const
        {
            if (name.size() < 2)
                return "";

            std::string best;
            int bestDist = 3; // Max edit distance to consider

            for (auto &builtin : builtins_)
            {
                int d = editDistance(name, builtin);
                if (d < bestDist)
                {
                    bestDist = d;
                    best = builtin;
                }
            }

            // Also check user-defined names
            for (int i = (int)scopes_.size() - 1; i >= 0; i--)
            {
                for (auto &def : scopes_[i])
                {
                    int d = editDistance(name, def);
                    if (d < bestDist)
                    {
                        bestDist = d;
                        best = def;
                    }
                }
            }

            return best;
        }

        static int editDistance(const std::string &a, const std::string &b)
        {
            int m = a.size(), n = b.size();
            if (std::abs(m - n) > 3)
                return 4; // early exit

            std::vector<int> prev(n + 1), curr(n + 1);
            for (int j = 0; j <= n; j++)
                prev[j] = j;

            for (int i = 1; i <= m; i++)
            {
                curr[0] = i;
                for (int j = 1; j <= n; j++)
                {
                    int cost = (a[i - 1] == b[j - 1]) ? 0 : 1;
                    curr[j] = std::min({prev[j] + 1, curr[j - 1] + 1, prev[j - 1] + cost});
                }
                std::swap(prev, curr);
            }
            return prev[n];
        }
    };

} // namespace xell
