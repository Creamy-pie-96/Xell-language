#pragma once

// =============================================================================
// SymbolCollector — AST-based symbol extraction for IDE autocomplete
// =============================================================================
// Walks the parsed AST and extracts all defined symbols (functions, variables,
// classes, structs, enums, interfaces, modules, parameters, etc.) with rich
// metadata suitable for IDE features like autocomplete, hover, go-to-definition.
//
// This is a pure reader — it does not modify the AST or execute anything.
// It runs after parsing, alongside or instead of the static analyzer.
// =============================================================================

#include "../parser/ast.hpp"
#include <string>
#include <vector>
#include <sstream>
#include <unordered_set>

namespace xell
{

    enum class SymbolKind
    {
        Variable,
        Function,
        Class,
        Struct,
        Enum,
        Interface,
        Module,
        Parameter,
        Field,
        Method,
        Property,
        Import
    };

    struct SymbolInfo
    {
        std::string name;
        SymbolKind kind;
        int line = 0;
        std::string detail;                  // e.g. "fn greet(name, age)"
        std::string parentScope;             // e.g. "MyClass" for methods
        std::vector<std::string> params;     // for functions/methods
        std::vector<std::string> paramTypes; // type annotations
        std::string returnType;
    };

    static inline const char *symbolKindStr(SymbolKind k)
    {
        switch (k)
        {
        case SymbolKind::Variable:
            return "variable";
        case SymbolKind::Function:
            return "function";
        case SymbolKind::Class:
            return "class";
        case SymbolKind::Struct:
            return "struct";
        case SymbolKind::Enum:
            return "enum";
        case SymbolKind::Interface:
            return "interface";
        case SymbolKind::Module:
            return "module";
        case SymbolKind::Parameter:
            return "parameter";
        case SymbolKind::Field:
            return "field";
        case SymbolKind::Method:
            return "method";
        case SymbolKind::Property:
            return "property";
        case SymbolKind::Import:
            return "import";
        }
        return "unknown";
    }

    class SymbolCollector
    {
    public:
        std::vector<SymbolInfo> collect(const Program &program)
        {
            symbols_.clear();
            seen_.clear();
            for (auto &stmt : program.statements)
                visitStmt(stmt.get(), "");
            return symbols_;
        }

        // Serialize symbols to JSON string
        static std::string toJSON(const std::vector<SymbolInfo> &symbols)
        {
            std::ostringstream out;
            out << "[";
            for (size_t i = 0; i < symbols.size(); i++)
            {
                auto &s = symbols[i];
                if (i > 0)
                    out << ",";
                out << "{\"name\":\"" << escapeJSON(s.name)
                    << "\",\"kind\":\"" << symbolKindStr(s.kind)
                    << "\",\"line\":" << s.line
                    << ",\"detail\":\"" << escapeJSON(s.detail) << "\"";

                if (!s.parentScope.empty())
                    out << ",\"scope\":\"" << escapeJSON(s.parentScope) << "\"";

                if (!s.params.empty())
                {
                    out << ",\"params\":[";
                    for (size_t j = 0; j < s.params.size(); j++)
                    {
                        if (j > 0)
                            out << ",";
                        out << "\"" << escapeJSON(s.params[j]) << "\"";
                    }
                    out << "]";
                }

                if (!s.returnType.empty())
                    out << ",\"returnType\":\"" << escapeJSON(s.returnType) << "\"";

                out << "}";
            }
            out << "]";
            return out.str();
        }

    private:
        std::vector<SymbolInfo> symbols_;
        std::unordered_set<std::string> seen_; // avoid duplicates

        void addSymbol(const std::string &name, SymbolKind kind, int line,
                       const std::string &detail = "", const std::string &scope = "",
                       const std::vector<std::string> &params = {},
                       const std::vector<std::string> &paramTypes = {},
                       const std::string &returnType = "")
        {
            // Avoid duplicate top-level names (same name + same scope)
            std::string key = name + "::" + scope;
            if (seen_.count(key))
                return;
            seen_.insert(key);

            SymbolInfo sym;
            sym.name = name;
            sym.kind = kind;
            sym.line = line;
            sym.detail = detail;
            sym.parentScope = scope;
            sym.params = params;
            sym.paramTypes = paramTypes;
            sym.returnType = returnType;
            symbols_.push_back(sym);
        }

        void visitStmt(const Stmt *stmt, const std::string &scope)
        {
            if (!stmt)
                return;

            if (auto *a = dynamic_cast<const Assignment *>(stmt))
            {
                addSymbol(a->name, SymbolKind::Variable, a->line,
                          a->name + " = ...", scope);
            }
            else if (auto *imm = dynamic_cast<const ImmutableBinding *>(stmt))
            {
                addSymbol(imm->name, SymbolKind::Variable, imm->line,
                          "be " + imm->name + " = ...", scope);
            }
            else if (auto *fn = dynamic_cast<const FnDef *>(stmt))
            {
                visitFnDef(fn, scope);
            }
            else if (auto *cls = dynamic_cast<const ClassDef *>(stmt))
            {
                visitClassDef(cls, scope);
            }
            else if (auto *st = dynamic_cast<const StructDef *>(stmt))
            {
                visitStructDef(st, scope);
            }
            else if (auto *en = dynamic_cast<const EnumDef *>(stmt))
            {
                addSymbol(en->name, SymbolKind::Enum, en->line,
                          "enum " + en->name, scope);
                for (auto &m : en->members)
                    addSymbol(m, SymbolKind::Field, en->line,
                              en->name + "." + m, en->name);
            }
            else if (auto *iface = dynamic_cast<const InterfaceDef *>(stmt))
            {
                addSymbol(iface->name, SymbolKind::Interface, iface->line,
                          "interface " + iface->name, scope);
            }
            else if (auto *mod = dynamic_cast<const ModuleDef *>(stmt))
            {
                addSymbol(mod->name, SymbolKind::Module, mod->line,
                          "module " + mod->name, scope);
                for (auto &s : mod->body)
                    visitStmt(s.get(), mod->name);
            }
            else if (auto *exp = dynamic_cast<const ExportDecl *>(stmt))
            {
                visitStmt(exp->declaration.get(), scope);
            }
            else if (auto *decFn = dynamic_cast<const DecoratedFnDef *>(stmt))
            {
                visitStmt(decFn->fnDef.get(), scope);
            }
            else if (auto *decCls = dynamic_cast<const DecoratedClassDef *>(stmt))
            {
                visitStmt(decCls->classDef.get(), scope);
            }
            else if (auto *bring = dynamic_cast<const BringStmt *>(stmt))
            {
                // Record imported names
                if (!bring->aliases.empty())
                {
                    for (auto &alias : bring->aliases)
                        addSymbol(alias, SymbolKind::Import, bring->line,
                                  "import " + alias, scope);
                }
                else
                {
                    for (auto &part : bring->parts)
                    {
                        for (auto &item : part.items)
                            addSymbol(item, SymbolKind::Import, bring->line,
                                      "import " + item, scope);
                    }
                }
            }
            else if (auto *forStmt = dynamic_cast<const ForStmt *>(stmt))
            {
                for (auto &vn : forStmt->varNames)
                    addSymbol(vn, SymbolKind::Variable, forStmt->line,
                              "for " + vn + " in ...", scope);
                if (forStmt->hasRest)
                    addSymbol(forStmt->restName, SymbolKind::Variable, forStmt->line,
                              "for ..." + forStmt->restName, scope);
                // Also visit body for inner definitions
                for (auto &s : forStmt->body)
                    visitStmt(s.get(), scope);
            }
            else if (auto *destr = dynamic_cast<const DestructuringAssignment *>(stmt))
            {
                for (auto &n : destr->names)
                    addSymbol(n, SymbolKind::Variable, destr->line,
                              n + " = ... (destructured)", scope);
            }
            else if (auto *letStmt = dynamic_cast<const LetStmt *>(stmt))
            {
                for (auto &b : letStmt->bindings)
                    addSymbol(b.name, SymbolKind::Variable, letStmt->line,
                              "let " + b.name + " = ...", scope);
                for (auto &s : letStmt->body)
                    visitStmt(s.get(), scope);
            }
            else if (auto *ifStmt = dynamic_cast<const IfStmt *>(stmt))
            {
                for (auto &s : ifStmt->body)
                    visitStmt(s.get(), scope);
                for (auto &elif : ifStmt->elifs)
                    for (auto &s : elif.body)
                        visitStmt(s.get(), scope);
                for (auto &s : ifStmt->elseBody)
                    visitStmt(s.get(), scope);
            }
            else if (auto *whileStmt = dynamic_cast<const WhileStmt *>(stmt))
            {
                for (auto &s : whileStmt->body)
                    visitStmt(s.get(), scope);
            }
            else if (auto *loopStmt = dynamic_cast<const LoopStmt *>(stmt))
            {
                for (auto &s : loopStmt->body)
                    visitStmt(s.get(), scope);
            }
            else if (auto *tryStmt = dynamic_cast<const TryCatchStmt *>(stmt))
            {
                for (auto &s : tryStmt->tryBody)
                    visitStmt(s.get(), scope);
                if (!tryStmt->catchVarName.empty())
                    addSymbol(tryStmt->catchVarName, SymbolKind::Variable, tryStmt->line,
                              "catch " + tryStmt->catchVarName, scope);
                for (auto &s : tryStmt->catchBody)
                    visitStmt(s.get(), scope);
                for (auto &s : tryStmt->finallyBody)
                    visitStmt(s.get(), scope);
            }
            else if (auto *incase = dynamic_cast<const InCaseStmt *>(stmt))
            {
                for (auto &c : incase->clauses)
                    for (auto &s : c.body)
                        visitStmt(s.get(), scope);
                for (auto &s : incase->elseBody)
                    visitStmt(s.get(), scope);
            }
            // Also handle expression statements that might contain assignments
            // (ExprStmt with lambda assignments etc.) — handled by the above patterns
        }

        void visitFnDef(const FnDef *fn, const std::string &scope)
        {
            // Build detail string: "fn name(param1, param2)"
            std::string detail = "fn " + fn->name + "(";
            for (size_t i = 0; i < fn->params.size(); i++)
            {
                if (i > 0)
                    detail += ", ";
                detail += fn->params[i];
                if (i < fn->paramTypes.size() && !fn->paramTypes[i].empty())
                    detail += ": " + fn->paramTypes[i];
            }
            if (fn->isVariadic && !fn->variadicName.empty())
            {
                if (!fn->params.empty())
                    detail += ", ";
                detail += "..." + fn->variadicName;
            }
            detail += ")";
            if (!fn->returnType.empty())
                detail += " -> " + fn->returnType;

            SymbolKind kind = scope.empty() ? SymbolKind::Function : SymbolKind::Method;
            addSymbol(fn->name, kind, fn->line, detail, scope,
                      fn->params, fn->paramTypes, fn->returnType);

            // Register parameters as symbols too
            for (auto &p : fn->params)
                addSymbol(p, SymbolKind::Parameter, fn->line,
                          "param of " + fn->name, fn->name);
            if (fn->isVariadic && !fn->variadicName.empty())
                addSymbol(fn->variadicName, SymbolKind::Parameter, fn->line,
                          "variadic param of " + fn->name, fn->name);

            // Visit body for inner definitions
            for (auto &s : fn->body)
                visitStmt(s.get(), scope.empty() ? fn->name : scope);
        }

        void visitClassDef(const ClassDef *cls, const std::string &scope)
        {
            std::string detail = "class " + cls->name;
            if (!cls->parents.empty())
            {
                detail += " inherits ";
                for (size_t i = 0; i < cls->parents.size(); i++)
                {
                    if (i > 0)
                        detail += ", ";
                    detail += cls->parents[i];
                }
            }

            addSymbol(cls->name, SymbolKind::Class, cls->line, detail, scope);

            // Fields
            for (auto &f : cls->fields)
                addSymbol(f.name, SymbolKind::Field, f.line,
                          cls->name + "." + f.name, cls->name);

            // Methods
            for (auto &m : cls->methods)
                visitFnDef(m.get(), cls->name);

            // Properties
            for (auto &p : cls->properties)
                addSymbol(p.name, SymbolKind::Property, p.line,
                          "property " + cls->name + "." + p.name, cls->name);
        }

        void visitStructDef(const StructDef *st, const std::string &scope)
        {
            std::string detail = "struct " + st->name;
            addSymbol(st->name, SymbolKind::Struct, st->line, detail, scope);

            for (auto &f : st->fields)
                addSymbol(f.name, SymbolKind::Field, f.line,
                          st->name + "." + f.name, st->name);

            for (auto &m : st->methods)
                visitFnDef(m.get(), st->name);
        }

        // JSON string escaping
        static std::string escapeJSON(const std::string &s)
        {
            std::string result;
            result.reserve(s.size());
            for (char c : s)
            {
                switch (c)
                {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    result += c;
                    break;
                }
            }
            return result;
        }
    };

} // namespace xell
