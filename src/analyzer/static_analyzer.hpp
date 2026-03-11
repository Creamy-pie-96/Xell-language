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
#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include <string>
#include <vector>
#include <algorithm>
#include <unordered_set>
#include <unordered_map>
#include <fstream>
#include <filesystem>
#include <regex>

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

        // Set source directory for resolving relative import paths
        void setSourceDir(const std::string &dir)
        {
            sourceDir_ = dir;
        }

        // ─── Module export info (name + line + kind + sourceFile) ───
        struct ModuleExportInfo
        {
            std::string name;
            int line = 0;
            std::string kind;       // "function", "class", "variable", "struct", "enum", "interface", "module"
            std::string sourceFile; // resolved file path this export comes from
        };

        // Expose module exports for IDE (module name → list of exports with line/kind info)
        const std::unordered_map<std::string, std::vector<ModuleExportInfo>> &getModuleExports() const
        {
            return moduleExportsMap_;
        }

        // Expose module name → resolved file path mapping
        const std::unordered_map<std::string, std::string> &getModuleFileMap() const
        {
            return moduleFileMap_;
        }

    private:
        // ─── Arity info for function call validation ───
        struct FnArity
        {
            int minParams = 0; // number of required params (no default)
            int maxParams = 0; // total params (including those with defaults)
            bool isVariadic = false;
        };

        std::unordered_set<std::string> builtins_;
        std::unordered_map<std::string, std::string> typoMap_;
        std::vector<std::unordered_set<std::string>> scopes_;
        std::vector<LintDiagnostic> diagnostics_;
        bool hasWildcardBring_ = false;                   // suppress "undefined" when wildcard bring is in scope
        std::string sourceDir_;                           // directory of the source file being analyzed
        std::unordered_set<std::string> resolvedImports_; // avoid re-parsing same import file

        // Function name → arity info (for argument count validation)
        std::unordered_map<std::string, FnArity> fnArityMap_;

        // Module name → exported members with line/kind info
        std::unordered_map<std::string, std::vector<ModuleExportInfo>> moduleExportsMap_;

        // Module name → resolved source file path
        std::unordered_map<std::string, std::string> moduleFileMap_;

        // "module.member" → arity info for module-exported functions
        std::unordered_map<std::string, FnArity> moduleExportArityMap_;

        // Helper: check if a module exports a given name
        bool hasModuleExport(const std::vector<ModuleExportInfo> &exports, const std::string &name) const
        {
            for (auto &e : exports)
                if (e.name == name)
                    return true;
            return false;
        }

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

        // Register a function's arity for argument count validation
        void defineFn(const FnDef *fn)
        {
            if (!fn || fn->name.empty())
                return;
            define(fn->name);
            FnArity arity;
            arity.maxParams = (int)fn->params.size();
            arity.isVariadic = fn->isVariadic;
            // Count required params (those without defaults)
            int required = 0;
            for (size_t i = 0; i < fn->params.size(); i++)
            {
                bool hasDefault = (i < fn->defaults.size() && fn->defaults[i] != nullptr);
                if (!hasDefault)
                    required++;
            }
            arity.minParams = required;
            fnArityMap_[fn->name] = arity;
        }

        // Register a function as a module export with arity info
        void defineModuleExportFn(const std::string &moduleName, const FnDef *fn,
                                  const std::string &srcFile = "")
        {
            if (!fn || fn->name.empty())
                return;
            // Store export info with line number and kind
            auto &exports = moduleExportsMap_[moduleName];
            // Avoid duplicates
            bool exists = false;
            for (auto &e : exports)
                if (e.name == fn->name)
                {
                    exists = true;
                    break;
                }
            if (!exists)
                exports.push_back({fn->name, fn->line, "function", srcFile});
            FnArity arity;
            arity.maxParams = (int)fn->params.size();
            arity.isVariadic = fn->isVariadic;
            int required = 0;
            for (size_t i = 0; i < fn->params.size(); i++)
            {
                bool hasDefault = (i < fn->defaults.size() && fn->defaults[i] != nullptr);
                if (!hasDefault)
                    required++;
            }
            arity.minParams = required;
            moduleExportArityMap_[moduleName + "." + fn->name] = arity;
        }

        // Register a non-function module export (variable, class, etc.)
        void defineModuleExport(const std::string &moduleName, const std::string &memberName,
                                int memberLine = 0, const std::string &memberKind = "variable",
                                const std::string &srcFile = "")
        {
            auto &exports = moduleExportsMap_[moduleName];
            // Avoid duplicates
            bool exists = false;
            for (auto &e : exports)
                if (e.name == memberName)
                {
                    exists = true;
                    break;
                }
            if (!exists)
                exports.push_back({memberName, memberLine, memberKind, srcFile});
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
                defineFn(fn);
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
            {
                define(mod->name);
                collectModuleExports(mod);
            }
            else if (auto *exp = dynamic_cast<const ExportDecl *>(stmt))
                collectDefinitions(exp->declaration.get());
            else if (auto *decFn = dynamic_cast<const DecoratedFnDef *>(stmt))
                collectDefinitions(decFn->fnDef.get());
            else if (auto *decCls = dynamic_cast<const DecoratedClassDef *>(stmt))
                collectDefinitions(decCls->classDef.get());
            else if (auto *bring = dynamic_cast<const BringStmt *>(stmt))
            {
                processBringStmt(bring);
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
                defineFn(fn);
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
                collectModuleExports(mod);
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
            else if (auto *throwStmt = dynamic_cast<const ThrowStmt *>(stmt))
            {
                if (throwStmt->value)
                    checkExpr(throwStmt->value.get());
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
                processBringStmt(bring);
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

        // ─── processBringStmt ────────────────────────────────────
        // Shared logic for both collectDefinitions and checkStatement
        void processBringStmt(const BringStmt *bring)
        {
            if (!bring)
                return;

            // Collect all original item names across all parts (for alias mapping)
            std::vector<std::string> originalNames;
            for (const auto &part : bring->parts)
            {
                if (part.bringAll)
                    originalNames.push_back("*");
                else
                    for (const auto &item : part.items)
                        originalNames.push_back(item);
            }

            if (!bring->aliases.empty())
            {
                // Define aliases + unaliased items
                for (size_t i = 0; i < originalNames.size(); i++)
                {
                    std::string bindName = (i < bring->aliases.size() && !bring->aliases[i].empty())
                                               ? bring->aliases[i]
                                               : originalNames[i];
                    define(bindName);
                }

                // Resolve imports for arity info
                for (const auto &part : bring->parts)
                {
                    if (part.bringAll)
                    {
                        if (!part.filePath.empty())
                            resolveImportedFile(part.filePath, bring->fromDir);
                        else if (part.hasModulePath && !part.modulePath.empty())
                            resolveModuleImport(part.modulePath, bring->fromDir);
                        else
                            hasWildcardBring_ = true;
                    }
                    else if (part.hasModulePath && !part.modulePath.empty())
                    {
                        resolveModuleImportForArity(part.modulePath, part.items, bring->fromDir);
                    }
                    else if (!part.filePath.empty())
                    {
                        resolveImportedFileForArity(part.filePath, part.items, bring->fromDir);
                    }
                    else
                    {
                        // Bare bring with alias: bring module_name as m
                        for (const auto &item : part.items)
                            resolveBareBring(item, bring->fromDir);
                    }
                }

                // Map alias names → original function arity
                for (size_t i = 0; i < originalNames.size(); i++)
                {
                    if (i < bring->aliases.size() && !bring->aliases[i].empty())
                    {
                        auto arityIt = fnArityMap_.find(originalNames[i]);
                        if (arityIt != fnArityMap_.end())
                            fnArityMap_[bring->aliases[i]] = arityIt->second;
                    }
                }
            }
            else
            {
                // No aliases
                for (const auto &part : bring->parts)
                {
                    if (part.bringAll)
                    {
                        if (!part.filePath.empty())
                            resolveImportedFile(part.filePath, bring->fromDir);
                        else if (part.hasModulePath && !part.modulePath.empty())
                            resolveModuleImport(part.modulePath, bring->fromDir);
                        else
                            hasWildcardBring_ = true;
                    }
                    else if (part.hasModulePath && !part.modulePath.empty())
                    {
                        for (const auto &item : part.items)
                            define(item);
                        resolveModuleImportForArity(part.modulePath, part.items, bring->fromDir);
                    }
                    else if (!part.filePath.empty())
                    {
                        for (const auto &item : part.items)
                            define(item);
                        resolveImportedFileForArity(part.filePath, part.items, bring->fromDir);
                    }
                    else
                    {
                        // Bare bring: bring module_name → namespace
                        for (const auto &item : part.items)
                        {
                            define(item);
                            resolveBareBring(item, bring->fromDir);
                        }
                    }
                }
            }
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
                if (call->isMethodCall)
                {
                    // Method call: obj->method(args)
                    // args[0] is the object, args[1..] are the actual arguments
                    // Check if the object is a known module and the method exists
                    if (!call->args.empty())
                    {
                        if (auto *objIdent = dynamic_cast<const Identifier *>(call->args[0].get()))
                        {
                            const std::string &modName = objIdent->name;
                            auto exportIt = moduleExportsMap_.find(modName);
                            if (exportIt != moduleExportsMap_.end())
                            {
                                // We know this module's exports
                                const auto &exports = exportIt->second;
                                if (!hasModuleExport(exports, call->callee))
                                {
                                    // Find closest export name for suggestion
                                    std::string closest;
                                    int bestDist = 3;
                                    for (const auto &exp : exports)
                                    {
                                        int d = editDistance(call->callee, exp.name);
                                        if (d < bestDist)
                                        {
                                            bestDist = d;
                                            closest = exp.name;
                                        }
                                    }
                                    if (!closest.empty())
                                    {
                                        diagnostics_.push_back({call->line,
                                                                "Module '" + modName + "' has no export '" + call->callee +
                                                                    "'. Did you mean '" + closest + "'?",
                                                                "error"});
                                    }
                                    else
                                    {
                                        diagnostics_.push_back({call->line,
                                                                "Module '" + modName + "' has no export '" + call->callee + "'",
                                                                "error"});
                                    }
                                }
                                else
                                {
                                    // Method exists — check arity (actual args exclude the object itself)
                                    int actualArgs = (int)call->args.size() - 1;
                                    std::string key = modName + "." + call->callee;
                                    auto arityIt = moduleExportArityMap_.find(key);
                                    if (arityIt != moduleExportArityMap_.end())
                                    {
                                        checkCallArity(call->callee, actualArgs, arityIt->second, call->line);
                                    }
                                }
                            }
                        }
                    }
                }
                else if (!isDefined(call->callee) && !hasWildcardBring_)
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
                else if (isDefined(call->callee) && !call->isMethodCall)
                {
                    // Known function — validate argument count
                    auto arityIt = fnArityMap_.find(call->callee);
                    if (arityIt != fnArityMap_.end())
                    {
                        int actualArgs = (int)call->args.size();
                        checkCallArity(call->callee, actualArgs, arityIt->second, call->line);
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
            else if (auto *chain = dynamic_cast<const ChainedComparisonExpr *>(expr))
            {
                for (const auto &operand : chain->operands)
                    checkExpr(operand.get());
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
            else if (auto *slice = dynamic_cast<const SliceExpr *>(expr))
            {
                checkExpr(slice->object.get());
                if (slice->start)
                    checkExpr(slice->start.get());
                if (slice->end)
                    checkExpr(slice->end.get());
                if (slice->step)
                    checkExpr(slice->step.get());
            }
            else if (auto *mapAcc = dynamic_cast<const MemberAccess *>(expr))
            {
                checkExpr(mapAcc->object.get());
                // Validate module member access: module->member
                if (auto *objIdent = dynamic_cast<const Identifier *>(mapAcc->object.get()))
                {
                    const std::string &modName = objIdent->name;
                    auto exportIt = moduleExportsMap_.find(modName);
                    if (exportIt != moduleExportsMap_.end())
                    {
                        const auto &exports = exportIt->second;
                        if (!hasModuleExport(exports, mapAcc->member))
                        {
                            std::string closest;
                            int bestDist = 3;
                            for (const auto &exp : exports)
                            {
                                int d = editDistance(mapAcc->member, exp.name);
                                if (d < bestDist)
                                {
                                    bestDist = d;
                                    closest = exp.name;
                                }
                            }
                            if (!closest.empty())
                            {
                                diagnostics_.push_back({mapAcc->line,
                                                        "Module '" + modName + "' has no export '" + mapAcc->member +
                                                            "'. Did you mean '" + closest + "'?",
                                                        "error"});
                            }
                            else
                            {
                                diagnostics_.push_back({mapAcc->line,
                                                        "Module '" + modName + "' has no export '" + mapAcc->member + "'",
                                                        "error"});
                            }
                        }
                    }
                }
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

        // ─── Module file finder ──────────────────────────────────
        // Searches for a module's .xel file by checking:
        //   1. Direct path: sourceDir/moduleName.xel
        //   2. fromDir/moduleName.xel
        //   3. .xell_meta in sourceDir and parent directories
        std::string findModuleFile(const std::string &moduleName, const std::string &fromDir)
        {
            namespace fs = std::filesystem;

            // 1. Try direct path in sourceDir
            if (!sourceDir_.empty())
            {
                std::string candidate = sourceDir_ + "/" + moduleName + ".xel";
                if (fs::exists(candidate))
                    return fs::canonical(candidate).string();
            }

            // 2. Try fromDir
            if (!fromDir.empty())
            {
                std::string base = fromDir;
                if (!sourceDir_.empty() && !fs::path(fromDir).is_absolute())
                    base = sourceDir_ + "/" + fromDir;
                std::string candidate = base + "/" + moduleName + ".xel";
                if (fs::exists(candidate))
                    return fs::canonical(candidate).string();
            }

            // 3. Search .xell_meta files in sourceDir subdirectories (where modules are registered)
            if (!sourceDir_.empty())
            {
                try
                {
                    for (auto &entry : fs::directory_iterator(sourceDir_))
                    {
                        if (!entry.is_directory())
                            continue;
                        std::string metaPath = entry.path().string() + "/.xell_meta";
                        if (fs::exists(metaPath))
                        {
                            std::string filePath = lookupInMeta(metaPath, moduleName);
                            if (!filePath.empty())
                            {
                                std::string fullPath = entry.path().string() + "/" + filePath;
                                if (fs::exists(fullPath))
                                    return fs::canonical(fullPath).string();
                            }
                        }
                    }
                }
                catch (...)
                {
                }
            }

            // 4. Search .xell_meta files starting from sourceDir upward
            if (!sourceDir_.empty())
            {
                std::string searchDir = sourceDir_;
                for (int depth = 0; depth < 10; depth++) // limit depth
                {
                    std::string metaPath = searchDir + "/.xell_meta";
                    if (fs::exists(metaPath))
                    {
                        std::string filePath = lookupInMeta(metaPath, moduleName);
                        if (!filePath.empty())
                        {
                            std::string fullPath = searchDir + "/" + filePath;
                            if (fs::exists(fullPath))
                                return fs::canonical(fullPath).string();
                        }
                    }
                    // Go up one directory
                    fs::path parent = fs::path(searchDir).parent_path();
                    if (parent == searchDir || parent.empty())
                        break;
                    searchDir = parent.string();
                }
            }

            return ""; // not found
        }

        // Read .xell_meta JSON and look up a module name → file path
        std::string lookupInMeta(const std::string &metaPath, const std::string &moduleName)
        {
            std::ifstream f(metaPath);
            if (!f.is_open())
                return "";

            std::string content((std::istreambuf_iterator<char>(f)),
                                std::istreambuf_iterator<char>());

            // Quick regex search: "module_name" : { "file": "filename.xel"
            // This avoids a full JSON parser dependency
            std::regex pat("\"" + moduleName + "\"\\s*:\\s*\\{\\s*\"file\"\\s*:\\s*\"([^\"]+)\"");
            std::smatch match;
            if (std::regex_search(content, match, pat))
                return match[1].str();

            return "";
        }

        // ─── Arity validation helper ────────────────────────────
        void checkCallArity(const std::string &fnName, int actualArgs,
                            const FnArity &arity, int line)
        {
            if (arity.isVariadic)
            {
                // Variadic: at least minParams required
                if (actualArgs < arity.minParams)
                {
                    diagnostics_.push_back({line,
                                            "'" + fnName + "' expects at least " +
                                                std::to_string(arity.minParams) +
                                                " argument(s), got " + std::to_string(actualArgs),
                                            "error"});
                }
            }
            else
            {
                if (actualArgs < arity.minParams)
                {
                    diagnostics_.push_back({line,
                                            "'" + fnName + "' expects " +
                                                (arity.minParams == arity.maxParams
                                                     ? std::to_string(arity.minParams)
                                                     : std::to_string(arity.minParams) + "-" + std::to_string(arity.maxParams)) +
                                                " argument(s), got " + std::to_string(actualArgs),
                                            "error"});
                }
                else if (actualArgs > arity.maxParams)
                {
                    diagnostics_.push_back({line,
                                            "'" + fnName + "' expects " +
                                                (arity.minParams == arity.maxParams
                                                     ? std::to_string(arity.maxParams)
                                                     : std::to_string(arity.minParams) + "-" + std::to_string(arity.maxParams)) +
                                                " argument(s), got " + std::to_string(actualArgs),
                                            "error"});
                }
            }
        }

        // ─── Collect exported members of a local module definition ──
        void collectModuleExports(const ModuleDef *mod)
        {
            if (!mod)
                return;
            for (auto &s : mod->body)
            {
                if (auto *exp = dynamic_cast<const ExportDecl *>(s.get()))
                {
                    if (auto *fn = dynamic_cast<const FnDef *>(exp->declaration.get()))
                        defineModuleExportFn(mod->name, fn);
                    else if (auto *decFn = dynamic_cast<const DecoratedFnDef *>(exp->declaration.get()))
                    {
                        if (auto *fn2 = dynamic_cast<const FnDef *>(decFn->fnDef.get()))
                            defineModuleExportFn(mod->name, fn2);
                    }
                    else if (auto *a = dynamic_cast<const Assignment *>(exp->declaration.get()))
                        defineModuleExport(mod->name, a->name);
                    else if (auto *imm = dynamic_cast<const ImmutableBinding *>(exp->declaration.get()))
                        defineModuleExport(mod->name, imm->name);
                    else if (auto *cls = dynamic_cast<const ClassDef *>(exp->declaration.get()))
                        defineModuleExport(mod->name, cls->name);
                    else if (auto *st = dynamic_cast<const StructDef *>(exp->declaration.get()))
                        defineModuleExport(mod->name, st->name);
                    else if (auto *en = dynamic_cast<const EnumDef *>(exp->declaration.get()))
                        defineModuleExport(mod->name, en->name);
                    else if (auto *iface = dynamic_cast<const InterfaceDef *>(exp->declaration.get()))
                        defineModuleExport(mod->name, iface->name);
                    else if (auto *nestedMod = dynamic_cast<const ModuleDef *>(exp->declaration.get()))
                    {
                        defineModuleExport(mod->name, nestedMod->name);
                        // Recurse: populate the nested module's own export map
                        collectModuleExports(nestedMod);
                    }
                }
            }
        }

        // ─── Resolve bare bring (bring module_name) ──────────────
        // Tries to find module_name.xel, parse it, and populate moduleExportsMap_
        void resolveBareBring(const std::string &moduleName, const std::string &fromDir)
        {
            namespace fs = std::filesystem;

            // Use centralized module resolution
            std::string resolvedPath = findModuleFile(moduleName, fromDir);

            if (resolvedImports_.count(resolvedPath))
                return;
            resolvedImports_.insert(resolvedPath);

            // Store the resolved path so IDE can open the source file
            if (!resolvedPath.empty())
                moduleFileMap_[moduleName] = resolvedPath;

            std::ifstream file(resolvedPath);
            if (!file.is_open())
                return;

            std::string source((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

            try
            {
                Lexer lexer(source);
                auto tokens = lexer.tokenize();
                std::vector<CollectedParseError> errors;
                Parser parser(tokens);
                auto program = parser.parseLint(errors);

                // Look for module definitions and collect their exports
                for (auto &stmt : program.statements)
                {
                    if (auto *mod = dynamic_cast<const ModuleDef *>(stmt.get()))
                    {
                        // Register exports under the module name used in the bring
                        collectModuleExportsFromImported(moduleName, mod, resolvedPath);
                    }
                    else if (auto *exp = dynamic_cast<const ExportDecl *>(stmt.get()))
                    {
                        if (auto *mod2 = dynamic_cast<const ModuleDef *>(exp->declaration.get()))
                            collectModuleExportsFromImported(moduleName, mod2, resolvedPath);
                    }
                }
            }
            catch (...)
            {
            }
        }

        // Collect exports from an imported module definition
        void collectModuleExportsFromImported(const std::string &bindName, const ModuleDef *mod,
                                              const std::string &srcFile = "")
        {
            if (!mod)
                return;
            for (auto &s : mod->body)
            {
                if (auto *exp = dynamic_cast<const ExportDecl *>(s.get()))
                {
                    if (auto *fn = dynamic_cast<const FnDef *>(exp->declaration.get()))
                        defineModuleExportFn(bindName, fn, srcFile);
                    else if (auto *decFn = dynamic_cast<const DecoratedFnDef *>(exp->declaration.get()))
                    {
                        if (auto *fn2 = dynamic_cast<const FnDef *>(decFn->fnDef.get()))
                            defineModuleExportFn(bindName, fn2, srcFile);
                    }
                    else if (auto *a = dynamic_cast<const Assignment *>(exp->declaration.get()))
                        defineModuleExport(bindName, a->name, a->line, "variable", srcFile);
                    else if (auto *imm = dynamic_cast<const ImmutableBinding *>(exp->declaration.get()))
                        defineModuleExport(bindName, imm->name, imm->line, "variable", srcFile);
                    else if (auto *cls = dynamic_cast<const ClassDef *>(exp->declaration.get()))
                        defineModuleExport(bindName, cls->name, cls->line, "class", srcFile);
                    else if (auto *st = dynamic_cast<const StructDef *>(exp->declaration.get()))
                        defineModuleExport(bindName, st->name, st->line, "struct", srcFile);
                    else if (auto *en = dynamic_cast<const EnumDef *>(exp->declaration.get()))
                        defineModuleExport(bindName, en->name, en->line, "enum", srcFile);
                    else if (auto *iface = dynamic_cast<const InterfaceDef *>(exp->declaration.get()))
                        defineModuleExport(bindName, iface->name, iface->line, "interface", srcFile);
                    else if (auto *nestedMod = dynamic_cast<const ModuleDef *>(exp->declaration.get()))
                    {
                        defineModuleExport(bindName, nestedMod->name, nestedMod->line, "module", srcFile);
                        // Also map the nested module name to the same source file
                        if (!srcFile.empty())
                            moduleFileMap_[nestedMod->name] = srcFile;
                        // Recurse: populate the nested module's own export map
                        collectModuleExportsFromImported(nestedMod->name, nestedMod, srcFile);
                    }
                }
            }
        }

        // ─── Resolve named imports with arity info ───────────────
        // For "bring X of module" — resolve the module file to get arity info for X
        void resolveModuleImportForArity(const std::vector<std::string> &modulePath,
                                         const std::vector<std::string> &items,
                                         const std::string &fromDir)
        {
            namespace fs = std::filesystem;

            // Build the module name for lookup (last component or joined path)
            std::string moduleName = modulePath.empty() ? "" : modulePath.back();

            // Use centralized module resolution
            std::string resolvedPath = findModuleFile(moduleName, fromDir);
            if (resolvedPath.empty())
            {
                // Try with full module path joined (e.g. "lib/math_lib")
                std::string fullName;
                for (size_t i = 0; i < modulePath.size(); i++)
                {
                    if (i > 0)
                        fullName += "/";
                    fullName += modulePath[i];
                }
                if (fullName != moduleName)
                    resolvedPath = findModuleFile(fullName, fromDir);
            }

            if (resolvedPath.empty())
                return;

            // Key: use "arity:" prefix to avoid conflict with wildcard resolution
            // Include items in key so different bring statements can resolve different items
            std::string arityKey = "arity:" + resolvedPath + ":";
            for (const auto &item : items)
                arityKey += item + ",";
            if (resolvedImports_.count(arityKey))
                return;
            resolvedImports_.insert(arityKey);

            std::ifstream file(resolvedPath);
            if (!file.is_open())
                return;

            std::string source((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

            try
            {
                Lexer lexer(source);
                auto tokens = lexer.tokenize();
                std::vector<CollectedParseError> errors;
                Parser parser(tokens);
                auto program = parser.parseLint(errors);

                // Find exported functions matching the imported items
                std::unordered_set<std::string> itemSet(items.begin(), items.end());
                for (auto &stmt : program.statements)
                    collectArityFromImported(stmt.get(), itemSet, resolvedPath);
            }
            catch (...)
            {
            }
        }

        void resolveImportedFileForArity(const std::string &filePath,
                                         const std::vector<std::string> &items,
                                         const std::string &fromDir)
        {
            namespace fs = std::filesystem;

            std::string resolvedPath;
            if (!fromDir.empty())
                resolvedPath = fromDir + "/" + filePath;
            else
                resolvedPath = filePath;

            if (!sourceDir_.empty() && !fs::path(resolvedPath).is_absolute())
            {
                std::string candidate = sourceDir_ + "/" + resolvedPath;
                if (fs::exists(candidate))
                    resolvedPath = candidate;
            }

            if (fs::path(resolvedPath).extension().empty())
                resolvedPath += ".xel";

            try
            {
                if (fs::exists(resolvedPath))
                    resolvedPath = fs::canonical(resolvedPath).string();
            }
            catch (...)
            {
            }

            std::string arityKey = "arity:" + resolvedPath + ":";
            for (const auto &item : items)
                arityKey += item + ",";
            if (resolvedImports_.count(arityKey))
                return;
            resolvedImports_.insert(arityKey);

            std::ifstream file(resolvedPath);
            if (!file.is_open())
                return;

            std::string source((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

            try
            {
                Lexer lexer(source);
                auto tokens = lexer.tokenize();
                std::vector<CollectedParseError> errors;
                Parser parser(tokens);
                auto program = parser.parseLint(errors);

                std::unordered_set<std::string> itemSet(items.begin(), items.end());
                for (auto &stmt : program.statements)
                    collectArityFromImported(stmt.get(), itemSet, resolvedPath);
            }
            catch (...)
            {
            }
        }

        // Collect arity info for specific imported names from an imported file
        void collectArityFromImported(const Stmt *stmt, const std::unordered_set<std::string> &items,
                                      const std::string &srcFile = "")
        {
            if (!stmt)
                return;
            if (auto *fn = dynamic_cast<const FnDef *>(stmt))
            {
                if (items.count(fn->name))
                    defineFn(fn); // re-registers with arity info
            }
            else if (auto *exp = dynamic_cast<const ExportDecl *>(stmt))
                collectArityFromImported(exp->declaration.get(), items, srcFile);
            else if (auto *decFn = dynamic_cast<const DecoratedFnDef *>(stmt))
                collectArityFromImported(decFn->fnDef.get(), items, srcFile);
            else if (auto *mod = dynamic_cast<const ModuleDef *>(stmt))
            {
                // If the module name is in the imported items, collect its exports
                // so member access validation works (e.g., bring mod from "file.xel" → mod->fn())
                if (items.count(mod->name))
                    collectModuleExportsFromImported(mod->name, mod, srcFile);
                // Also look inside module body for exported functions
                for (auto &s : mod->body)
                    collectArityFromImported(s.get(), items, srcFile);
            }
        }

        // ─── Import resolution ───────────────────────────────────
        // Parse imported files to discover their exported names.
        // This is a lightweight parse-only pass (no execution).

        void resolveImportedFile(const std::string &filePath, const std::string &fromDir)
        {
            namespace fs = std::filesystem;

            // Build the full path
            std::string resolvedPath;
            if (!fromDir.empty())
                resolvedPath = fromDir + "/" + filePath;
            else
                resolvedPath = filePath;

            // Try to resolve relative to source directory
            if (!sourceDir_.empty() && !fs::path(resolvedPath).is_absolute())
            {
                std::string candidate = sourceDir_ + "/" + resolvedPath;
                if (fs::exists(candidate))
                    resolvedPath = candidate;
            }

            // Add .xel extension if missing
            if (fs::path(resolvedPath).extension().empty())
                resolvedPath += ".xel";

            // Normalize path to avoid re-parsing
            try
            {
                if (fs::exists(resolvedPath))
                    resolvedPath = fs::canonical(resolvedPath).string();
            }
            catch (...)
            {
            }

            // Skip if already resolved
            if (resolvedImports_.count(resolvedPath))
                return;
            resolvedImports_.insert(resolvedPath);

            // Read and parse the file
            std::ifstream file(resolvedPath);
            if (!file.is_open())
                return; // File not found — silently skip (don't false-positive on missing files)

            std::string source((std::istreambuf_iterator<char>(file)),
                               std::istreambuf_iterator<char>());

            try
            {
                Lexer lexer(source);
                auto tokens = lexer.tokenize();

                std::vector<CollectedParseError> errors;
                Parser parser(tokens);
                auto program = parser.parseLint(errors);

                // Collect all top-level definitions from the imported file
                for (auto &stmt : program.statements)
                    collectImportedDefinitions(stmt.get());
            }
            catch (...)
            {
                // Parse/lex error in imported file — silently skip
            }
        }

        void resolveModuleImport(const std::vector<std::string> &modulePath,
                                 const std::string &fromDir)
        {
            namespace fs = std::filesystem;

            // Convert module path to file path: ["lib", "math_lib"] → "lib/math_lib.xel"
            std::string filePath;
            for (size_t i = 0; i < modulePath.size(); i++)
            {
                if (i > 0)
                    filePath += "/";
                filePath += modulePath[i];
            }

            // Try as a file first
            resolveImportedFile(filePath, fromDir);

            // Also try via .xell_meta for cross-directory modules
            std::string moduleName = modulePath.empty() ? "" : modulePath.back();
            std::string metaResolved = findModuleFile(moduleName, fromDir);
            if (!metaResolved.empty() && !resolvedImports_.count(metaResolved))
                resolveImportedFile(metaResolved, "");
        }

        // Collect only top-level definitions from an imported file
        // (no recursive descent into blocks — just the surface-level names)
        void collectImportedDefinitions(const Stmt *stmt)
        {
            if (!stmt)
                return;

            if (auto *a = dynamic_cast<const Assignment *>(stmt))
                define(a->name);
            else if (auto *imm = dynamic_cast<const ImmutableBinding *>(stmt))
                define(imm->name);
            else if (auto *fn = dynamic_cast<const FnDef *>(stmt))
                defineFn(fn);
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
                collectImportedDefinitions(exp->declaration.get());
            else if (auto *decFn = dynamic_cast<const DecoratedFnDef *>(stmt))
                collectImportedDefinitions(decFn->fnDef.get());
            else if (auto *decCls = dynamic_cast<const DecoratedClassDef *>(stmt))
                collectImportedDefinitions(decCls->classDef.get());
        }
    };

} // namespace xell
