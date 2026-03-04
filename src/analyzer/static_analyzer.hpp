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
#include <filesystem>

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
            // ─── All built-in function names the interpreter provides ───
            // Kept comprehensive so --check doesn't produce false positives.
            // IO
            builtins_.insert({"print", "input", "exit"});
            // Type
            builtins_.insert({"type", "typeof", "str", "int", "float", "num", "complex",
                              "real", "imag", "conjugate", "magnitude",
                              "Int", "Float", "String", "Complex", "Bool", "number", "auto",
                              "List", "Tuple", "Set", "iSet"});
            // Util
            builtins_.insert({"assert", "format"});
            // Collection
            builtins_.insert({"len", "push", "pop", "keys", "values", "range", "set",
                              "has", "add", "remove", "contains", "to_set", "to_tuple",
                              "to_list", "union_set", "intersect", "diff"});
            // Math
            builtins_.insert({"floor", "ceil", "round", "abs", "mod", "pow", "sqrt",
                              "log", "log10", "sin", "cos", "tan", "cot", "sec", "csc",
                              "asin", "acos", "atan", "atan2", "acot", "asec", "acsc",
                              "sinh", "cosh", "tanh", "coth", "sech", "csch",
                              "asinh", "acosh", "atanh", "acoth", "asech", "acsch",
                              "clamp", "random", "random_int", "random_choice",
                              "is_nan", "is_inf", "to_int", "to_float", "hex", "bin"});
            // String
            builtins_.insert({"split", "join", "trim", "trim_start", "trim_end",
                              "upper", "lower", "replace", "replace_first",
                              "starts_with", "ends_with", "index_of", "substr",
                              "char_at", "repeat", "pad_start", "pad_end", "reverse",
                              "count", "is_empty", "is_numeric", "is_alpha", "lines", "to_chars"});
            // List
            builtins_.insert({"shift", "unshift", "insert", "remove_val", "sort", "sort_desc",
                              "slice", "flatten", "unique", "first", "last",
                              "zip", "sum", "min", "max", "avg", "size",
                              "enumerate", "zip_longest"});
            // Map
            builtins_.insert({"delete_key", "get", "merge", "entries", "from_entries"});
            // OS
            builtins_.insert({"mkdir", "rm", "cp", "mv", "exists", "is_file", "is_dir",
                              "ls", "read", "write", "append", "file_size",
                              "cwd", "cd", "abspath", "basename", "dirname", "ext",
                              "env_get", "env_set", "env_unset", "env_has",
                              "run", "run_capture", "pid",
                              "set_e", "unset_e", "exit_code"});
            // Hash
            builtins_.insert({"hash", "is_hashable", "hash_seed"});
            // Bytes
            builtins_.insert({"bytes", "encode", "decode", "byte_at", "byte_len",
                              "bytes_concat", "bytes_slice", "to_bytes"});
            // Generator
            builtins_.insert({"next", "is_exhausted", "gen_collect"});
            // Datetime
            builtins_.insert({"now", "timestamp", "timestamp_ms", "format_date",
                              "parse_date", "sleep", "sleep_sec", "time_since"});
            // Regex
            builtins_.insert({"regex_match", "regex_match_full", "regex_find",
                              "regex_find_all", "regex_replace", "regex_replace_all",
                              "regex_split", "regex_groups"});
            // JSON/CSV/TOML/YAML
            builtins_.insert({"json_parse", "json_stringify", "json_pretty",
                              "json_read", "json_write", "csv_parse", "csv_read",
                              "csv_write", "toml_read", "yaml_read"});
            // FS
            builtins_.insert({"ls_all", "pwd", "touch", "cat", "read_lines", "write_lines",
                              "is_symlink", "symlink", "hardlink", "ln", "readlink",
                              "chmod", "chown", "chgrp", "stat", "modified_time", "created_time",
                              "find", "find_regex", "locate", "glob", "file_diff", "tree",
                              "extension", "stem", "realpath", "join_path", "normalize",
                              "is_absolute", "relative_path", "home_dir", "temp_dir",
                              "disk_usage", "disk_free", "xxd", "strings"});
            // Shell
            builtins_.insert({"error", "clear", "reset", "logout", "alias", "unalias",
                              "export_env", "env_list", "printenv", "set_env", "which",
                              "whereis", "type_cmd", "man", "history", "history_add",
                              "yes_cmd", "true_val", "false_val", "source_file"});
            // Textproc
            builtins_.insert({"head", "tail", "tail_follow", "grep", "grep_regex",
                              "grep_recursive", "sed", "awk", "cut", "sort_file",
                              "uniq", "wc", "tee", "tr", "patch", "less", "more", "xargs"});
            // Process
            builtins_.insert({"ps", "kill", "kill_name", "pkill", "pgrep", "pidof",
                              "pstree", "jobs", "bg", "fg", "nohup", "nice", "wait_pid",
                              "ppid", "spawn", "run_timeout", "getuid", "is_root", "id",
                              "whoami", "hostname", "uname", "uptime", "time_cmd",
                              "watch", "strace", "lsof", "sys_info", "os_name", "arch",
                              "exec_proc", "signal_send"});
            // Sysmon
            builtins_.insert({"free", "vmstat", "iostat", "mpstat", "sar",
                              "cpu_count", "cpu_usage", "mem_total", "mem_free", "mem_used",
                              "lscpu", "lsmem", "lspci", "lsusb", "lsblk", "fdisk",
                              "mount_fs", "umount_fs", "dmesg", "journalctl",
                              "w_cmd", "last_logins", "ulimit_info", "cal", "date_str"});
            // Network
            builtins_.insert({"ping", "http_get", "http_post", "http_put", "http_delete",
                              "download", "dns_lookup", "nslookup", "host_lookup",
                              "whois", "traceroute", "netstat", "ss", "ifconfig", "ip_cmd",
                              "route", "iptables", "ufw", "nc", "telnet_connect",
                              "rsync", "local_ip", "public_ip"});
            // Archive
            builtins_.insert({"zip_archive", "unzip_archive", "tar_create", "tar_extract",
                              "gzip_compress", "gunzip_decompress", "bzip2_compress",
                              "bunzip2_decompress", "xz_compress", "xz_decompress"});

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
            };
        }

        std::vector<LintDiagnostic> analyze(const Program &program)
        {
            diagnostics_.clear();
            scopes_.clear();
            scopes_.push_back({}); // global scope

            // First pass: collect all top-level definitions
            for (auto &stmt : program.statements)
                collectDefinitions(stmt.get());

            // Second pass: check usage
            for (auto &stmt : program.statements)
                checkStatement(stmt.get());

            return diagnostics_;
        }

    private:
        std::unordered_set<std::string> builtins_;
        std::unordered_map<std::string, std::string> typoMap_;
        std::vector<std::unordered_set<std::string>> scopes_;
        std::vector<LintDiagnostic> diagnostics_;
        bool hasWildcardBring_ = false; // suppress "undefined" when wildcard bring is in scope

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
            for (int i = (int)scopes_.size() - 1; i >= 0; i--)
            {
                if (scopes_[i].count(name))
                    return true;
            }
            return builtins_.count(name) > 0;
        }

        // ─── collectDefinitions ──────────────────────────────────
        // First pass: register all names defined at this scope level.

        void collectDefinitions(const Stmt *stmt)
        {
            if (!stmt)
                return;

            if (auto *a = dynamic_cast<const Assignment *>(stmt))
                define(a->name);
            else if (auto *imm = dynamic_cast<const ImmutableBinding *>(stmt))
                define(imm->name);
            else if (auto *fn = dynamic_cast<const FnDef *>(stmt))
                define(fn->name);
            else if (auto *cls = dynamic_cast<const ClassDef *>(stmt))
                define(cls->name);
            else if (auto *st = dynamic_cast<const StructDef *>(stmt))
                define(st->name);
            else if (auto *en = dynamic_cast<const EnumDef *>(stmt))
            {
                define(en->name);
                for (auto &m : en->members)
                    define(m);
            }
            else if (auto *iface = dynamic_cast<const InterfaceDef *>(stmt))
                define(iface->name);
            else if (auto *mod = dynamic_cast<const ModuleDef *>(stmt))
                define(mod->name);
            else if (auto *exp = dynamic_cast<const ExportDecl *>(stmt))
                collectDefinitions(exp->declaration.get());
            else if (auto *decFn = dynamic_cast<const DecoratedFnDef *>(stmt))
                collectDefinitions(decFn->fnDef.get());
            else if (auto *decCls = dynamic_cast<const DecoratedClassDef *>(stmt))
                collectDefinitions(decCls->classDef.get());
            else if (auto *bring = dynamic_cast<const BringStmt *>(stmt))
            {
                if (!bring->aliases.empty())
                {
                    for (const auto &alias : bring->aliases)
                        define(alias);
                }
                else
                {
                    for (const auto &part : bring->parts)
                    {
                        if (part.bringAll)
                            hasWildcardBring_ = true;
                        else
                        {
                            for (const auto &item : part.items)
                                define(item);
                        }
                    }
                }
            }
            else if (auto *forStmt = dynamic_cast<const ForStmt *>(stmt))
            {
                for (auto &vn : forStmt->varNames)
                    define(vn);
                if (forStmt->hasRest)
                    define(forStmt->restName);
            }
            else if (auto *destr = dynamic_cast<const DestructuringAssignment *>(stmt))
            {
                for (auto &n : destr->names)
                    define(n);
            }
            else if (auto *letStmt = dynamic_cast<const LetStmt *>(stmt))
            {
                for (auto &b : letStmt->bindings)
                    define(b.name);
            }
        }

        // ─── checkStatement ──────────────────────────────────────
        // Second pass: walk every statement and check for errors.

        void checkStatement(const Stmt *stmt)
        {
            if (!stmt)
                return;

            if (auto *assign = dynamic_cast<const Assignment *>(stmt))
            {
                checkExpr(assign->value.get());
                define(assign->name);
            }
            else if (auto *imm = dynamic_cast<const ImmutableBinding *>(stmt))
            {
                checkExpr(imm->value.get());
                define(imm->name);
            }
            else if (auto *exprStmt = dynamic_cast<const ExprStmt *>(stmt))
            {
                checkExpr(exprStmt->expr.get());
            }
            else if (auto *ifStmt = dynamic_cast<const IfStmt *>(stmt))
            {
                checkExpr(ifStmt->condition.get());
                checkBlock(ifStmt->body);
                for (auto &elif : ifStmt->elifs)
                {
                    checkExpr(elif.condition.get());
                    checkBlock(elif.body);
                }
                checkBlock(ifStmt->elseBody);
            }
            else if (auto *forStmt = dynamic_cast<const ForStmt *>(stmt))
            {
                for (auto &iterExpr : forStmt->iterables)
                    checkExpr(iterExpr.get());
                pushScope();
                for (auto &vn : forStmt->varNames)
                    define(vn);
                if (forStmt->hasRest)
                    define(forStmt->restName);
                checkBlockContents(forStmt->body);
                popScope();
            }
            else if (auto *whileStmt = dynamic_cast<const WhileStmt *>(stmt))
            {
                checkExpr(whileStmt->condition.get());
                checkBlock(whileStmt->body);
            }
            else if (auto *loopStmt = dynamic_cast<const LoopStmt *>(stmt))
            {
                checkBlock(loopStmt->body);
            }
            else if (auto *fn = dynamic_cast<const FnDef *>(stmt))
            {
                define(fn->name);
                pushScope();
                for (auto &p : fn->params)
                    define(p);
                // Variadic param
                if (fn->isVariadic && !fn->variadicName.empty())
                    define(fn->variadicName);
                checkBlockContents(fn->body);
                popScope();
            }
            else if (auto *give = dynamic_cast<const GiveStmt *>(stmt))
            {
                if (give->value)
                    checkExpr(give->value.get());
            }
            else if (auto *cls = dynamic_cast<const ClassDef *>(stmt))
            {
                define(cls->name);
                pushScope();
                define("self"); // self is always available in class body
                // Fields
                for (auto &f : cls->fields)
                {
                    define(f.name);
                    if (f.defaultValue)
                        checkExpr(f.defaultValue.get());
                }
                // Methods
                for (auto &m : cls->methods)
                    checkStatement(m.get());
                // Properties
                for (auto &p : cls->properties)
                {
                    if (p.getter)
                        checkStatement(p.getter.get());
                    if (p.setter)
                        checkStatement(p.setter.get());
                }
                popScope();
            }
            else if (auto *st = dynamic_cast<const StructDef *>(stmt))
            {
                define(st->name);
                pushScope();
                define("self");
                for (auto &f : st->fields)
                {
                    define(f.name);
                    if (f.defaultValue)
                        checkExpr(f.defaultValue.get());
                }
                for (auto &m : st->methods)
                    checkStatement(m.get());
                popScope();
            }
            else if (auto *en = dynamic_cast<const EnumDef *>(stmt))
            {
                define(en->name);
                for (auto &m : en->members)
                    define(m);
                for (auto &v : en->memberValues)
                    if (v)
                        checkExpr(v.get());
            }
            else if (auto *iface = dynamic_cast<const InterfaceDef *>(stmt))
            {
                define(iface->name);
            }
            else if (auto *mod = dynamic_cast<const ModuleDef *>(stmt))
            {
                define(mod->name);
                pushScope();
                // First collect all definitions in module body
                for (auto &s : mod->body)
                    collectDefinitions(s.get());
                // Then check them
                for (auto &s : mod->body)
                    checkStatement(s.get());
                popScope();
            }
            else if (auto *exp = dynamic_cast<const ExportDecl *>(stmt))
            {
                checkStatement(exp->declaration.get());
            }
            else if (auto *decFn = dynamic_cast<const DecoratedFnDef *>(stmt))
            {
                // Decorators should be defined
                for (auto &d : decFn->decorators)
                {
                    if (!isDefined(d))
                        diagnostics_.push_back({decFn->line,
                                                "Undefined decorator '" + d + "'", "warning"});
                }
                checkStatement(decFn->fnDef.get());
            }
            else if (auto *decCls = dynamic_cast<const DecoratedClassDef *>(stmt))
            {
                for (auto &d : decCls->decorators)
                {
                    if (!isDefined(d))
                        diagnostics_.push_back({decCls->line,
                                                "Undefined decorator '" + d + "'", "warning"});
                }
                checkStatement(decCls->classDef.get());
            }
            else if (auto *tryStmt = dynamic_cast<const TryCatchStmt *>(stmt))
            {
                checkBlock(tryStmt->tryBody);
                if (!tryStmt->catchBody.empty())
                {
                    pushScope();
                    if (!tryStmt->catchVarName.empty())
                        define(tryStmt->catchVarName);
                    checkBlockContents(tryStmt->catchBody);
                    popScope();
                }
                checkBlock(tryStmt->finallyBody);
            }
            else if (auto *incase = dynamic_cast<const InCaseStmt *>(stmt))
            {
                checkExpr(incase->subject.get());
                for (auto &c : incase->clauses)
                {
                    for (auto &v : c.values)
                        checkExpr(v.get());
                    checkBlock(c.body);
                }
                checkBlock(incase->elseBody);
            }
            else if (auto *bring = dynamic_cast<const BringStmt *>(stmt))
            {
                if (!bring->aliases.empty())
                {
                    for (const auto &alias : bring->aliases)
                        define(alias);
                }
                else
                {
                    for (const auto &part : bring->parts)
                    {
                        if (part.bringAll)
                            hasWildcardBring_ = true;
                        else
                        {
                            for (const auto &item : part.items)
                                define(item);
                        }
                    }
                }
            }
            else if (auto *destr = dynamic_cast<const DestructuringAssignment *>(stmt))
            {
                checkExpr(destr->value.get());
                for (auto &n : destr->names)
                    define(n);
            }
            else if (auto *memAssign = dynamic_cast<const MemberAssignment *>(stmt))
            {
                checkExpr(memAssign->object.get());
                checkExpr(memAssign->value.get());
            }
            else if (auto *idxAssign = dynamic_cast<const IndexAssignment *>(stmt))
            {
                checkExpr(idxAssign->object.get());
                checkExpr(idxAssign->index.get());
                checkExpr(idxAssign->value.get());
            }
            else if (auto *letStmt = dynamic_cast<const LetStmt *>(stmt))
            {
                pushScope();
                for (auto &b : letStmt->bindings)
                {
                    checkExpr(b.expr.get());
                    define(b.name);
                }
                checkBlockContents(letStmt->body);
                popScope();
            }
            // BreakStmt, ContinueStmt — nothing to check
        }

        // ─── Helper: check a block (push/pop scope) ─────────────
        void checkBlock(const std::vector<StmtPtr> &block)
        {
            if (block.empty())
                return;
            pushScope();
            checkBlockContents(block);
            popScope();
        }

        void checkBlockContents(const std::vector<StmtPtr> &block)
        {
            for (auto &s : block)
                collectDefinitions(s.get());
            for (auto &s : block)
                checkStatement(s.get());
        }

        // ─── checkExpr ──────────────────────────────────────────

        void checkExpr(const Expr *expr)
        {
            if (!expr)
                return;

            if (auto *ident = dynamic_cast<const Identifier *>(expr))
            {
                const std::string &name = ident->name;
                if (!isDefined(name) && !hasWildcardBring_)
                {
                    auto it = typoMap_.find(name);
                    if (it != typoMap_.end())
                    {
                        diagnostics_.push_back({ident->line,
                                                "Undefined name '" + name + "'. Did you mean '" + it->second + "'?",
                                                "error"});
                    }
                    else
                    {
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
                if (!isDefined(call->callee) && !call->isMethodCall && !hasWildcardBring_)
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
            else if (auto *tup = dynamic_cast<const TupleLiteral *>(expr))
            {
                for (auto &elem : tup->elements)
                    checkExpr(elem.get());
            }
            else if (auto *setLit = dynamic_cast<const SetLiteral *>(expr))
            {
                for (auto &elem : setLit->elements)
                    checkExpr(elem.get());
            }
            else if (auto *map = dynamic_cast<const MapLiteral *>(expr))
            {
                for (auto &entry : map->entries)
                    checkExpr(entry.second.get());
            }
            else if (auto *postfix = dynamic_cast<const PostfixExpr *>(expr))
            {
                checkExpr(postfix->operand.get());
            }
            else if (auto *ternary = dynamic_cast<const TernaryExpr *>(expr))
            {
                checkExpr(ternary->value.get());
                checkExpr(ternary->condition.get());
                checkExpr(ternary->alternative.get());
            }
            else if (auto *ifExpr = dynamic_cast<const IfExpr *>(expr))
            {
                for (auto &branch : ifExpr->branches)
                {
                    if (branch.condition)
                        checkExpr(branch.condition.get());
                    if (branch.value)
                        checkExpr(branch.value.get());
                }
            }
            else if (auto *lambda = dynamic_cast<const LambdaExpr *>(expr))
            {
                pushScope();
                for (auto &p : lambda->params)
                    define(p);
                if (lambda->singleExpr)
                    checkExpr(lambda->singleExpr.get());
                checkBlockContents(lambda->body);
                popScope();
            }
            else if (auto *spread = dynamic_cast<const SpreadExpr *>(expr))
            {
                checkExpr(spread->operand.get());
            }
        }

        /// Simple edit-distance based closest match finder
        std::string findClosest(const std::string &name) const
        {
            if (name.size() < 2)
                return "";

            std::string best;
            int bestDist = 3;

            for (auto &builtin : builtins_)
            {
                int d = editDistance(name, builtin);
                if (d < bestDist)
                {
                    bestDist = d;
                    best = builtin;
                }
            }

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
                return 4;

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
