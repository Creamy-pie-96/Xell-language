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
        int lineEnd = 0;                     // end line of scope (computed from body)
        std::string detail;                  // e.g. "fn greet(name, age)"
        std::string parentScope;             // e.g. "MyClass" for methods
        std::string scopeType;               // "global", "function", "class", "module", etc.
        std::vector<std::string> params;     // for functions/methods
        std::vector<std::string> paramTypes; // type annotations
        std::string returnType;
        std::string inferredType; // static type inference: "int", "str", "list", etc.
        std::string sourceFile;   // originating file for imported symbols (empty = current file)
        bool isExported = false;
        bool isImmutable = false;
        std::vector<std::string> children; // names of child symbols (methods, exports)
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

                // Extended fields
                if (s.lineEnd > 0 && s.lineEnd != s.line)
                    out << ",\"lineEnd\":" << s.lineEnd;

                if (!s.scopeType.empty())
                    out << ",\"scopeType\":\"" << escapeJSON(s.scopeType) << "\"";

                if (!s.inferredType.empty())
                    out << ",\"inferredType\":\"" << escapeJSON(s.inferredType) << "\"";

                if (s.isExported)
                    out << ",\"isExported\":true";

                if (s.isImmutable)
                    out << ",\"isImmutable\":true";

                if (!s.sourceFile.empty())
                    out << ",\"sourceFile\":\"" << escapeJSON(s.sourceFile) << "\"";

                if (!s.children.empty())
                {
                    out << ",\"children\":[";
                    for (size_t j = 0; j < s.children.size(); j++)
                    {
                        if (j > 0)
                            out << ",";
                        out << "\"" << escapeJSON(s.children[j]) << "\"";
                    }
                    out << "]";
                }

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
                       const std::string &returnType = "",
                       int lineEnd = 0, const std::string &scopeType = "",
                       const std::string &inferredType = "",
                       bool isExported = false, bool isImmutable = false,
                       const std::string &sourceFile = "")
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
            sym.lineEnd = (lineEnd > 0) ? lineEnd : line;
            sym.detail = detail;
            sym.parentScope = scope;
            sym.scopeType = scopeType.empty() ? (scope.empty() ? "global" : "scoped") : scopeType;
            sym.params = params;
            sym.paramTypes = paramTypes;
            sym.returnType = returnType;
            sym.inferredType = inferredType;
            sym.isExported = isExported;
            sym.isImmutable = isImmutable;
            sym.sourceFile = sourceFile;
            symbols_.push_back(sym);
        }

        // Add a child name to an already-registered parent symbol
        void addChild(const std::string &parentName, const std::string &parentScope,
                      const std::string &childName)
        {
            std::string key = parentName + "::" + parentScope;
            for (auto &sym : symbols_)
            {
                std::string sk = sym.name + "::" + sym.parentScope;
                if (sk == key)
                {
                    sym.children.push_back(childName);
                    return;
                }
            }
        }

        // Compute end line from a vector of statements
        static int computeEndLine(const std::vector<StmtPtr> &body, int startLine)
        {
            int maxLine = startLine;
            for (auto &s : body)
            {
                if (s && s->line > maxLine)
                    maxLine = s->line;
                // Recurse into nested blocks for accurate end line
                if (auto *fn = dynamic_cast<const FnDef *>(s.get()))
                {
                    int e = computeEndLine(fn->body, fn->line);
                    if (e > maxLine)
                        maxLine = e;
                }
                else if (auto *ifS = dynamic_cast<const IfStmt *>(s.get()))
                {
                    int e = computeEndLine(ifS->body, ifS->line);
                    if (e > maxLine)
                        maxLine = e;
                    for (auto &elif : ifS->elifs)
                    {
                        int ee = computeEndLine(elif.body, elif.line);
                        if (ee > maxLine)
                            maxLine = ee;
                    }
                    int ee = computeEndLine(ifS->elseBody, ifS->line);
                    if (ee > maxLine)
                        maxLine = ee;
                }
                else if (auto *forS = dynamic_cast<const ForStmt *>(s.get()))
                {
                    int e = computeEndLine(forS->body, forS->line);
                    if (e > maxLine)
                        maxLine = e;
                }
                else if (auto *whS = dynamic_cast<const WhileStmt *>(s.get()))
                {
                    int e = computeEndLine(whS->body, whS->line);
                    if (e > maxLine)
                        maxLine = e;
                }
                else if (auto *loS = dynamic_cast<const LoopStmt *>(s.get()))
                {
                    int e = computeEndLine(loS->body, loS->line);
                    if (e > maxLine)
                        maxLine = e;
                }
                else if (auto *tryS = dynamic_cast<const TryCatchStmt *>(s.get()))
                {
                    int e = computeEndLine(tryS->tryBody, tryS->line);
                    if (e > maxLine)
                        maxLine = e;
                    for (const auto &clause : tryS->catchClauses)
                    {
                        e = computeEndLine(clause.body, tryS->line);
                        if (e > maxLine)
                            maxLine = e;
                    }
                    e = computeEndLine(tryS->finallyBody, tryS->line);
                    if (e > maxLine)
                        maxLine = e;
                }
            }
            return maxLine;
        }

        // Compute end line for a class from its fields and methods
        static int computeClassEndLine(const ClassDef *cls)
        {
            int maxLine = cls->line;
            for (auto &f : cls->fields)
                if (f.line > maxLine)
                    maxLine = f.line;
            for (auto &m : cls->methods)
            {
                int e = computeEndLine(m->body, m->line);
                if (e > maxLine)
                    maxLine = e;
            }
            for (auto &p : cls->properties)
                if (p.line > maxLine)
                    maxLine = p.line;
            return maxLine;
        }

        // Compute end line for a struct
        static int computeStructEndLine(const StructDef *st)
        {
            int maxLine = st->line;
            for (auto &f : st->fields)
                if (f.line > maxLine)
                    maxLine = f.line;
            for (auto &m : st->methods)
            {
                int e = computeEndLine(m->body, m->line);
                if (e > maxLine)
                    maxLine = e;
            }
            return maxLine;
        }

        // Infer type from an expression (static, no execution)
        static std::string inferTypeFromExpr(const Expr *expr)
        {
            if (!expr)
                return "";
            if (dynamic_cast<const IntLiteral *>(expr))
                return "int";
            if (dynamic_cast<const FloatLiteral *>(expr))
                return "float";
            if (dynamic_cast<const NumberLiteral *>(expr))
                return "float";
            if (dynamic_cast<const StringLiteral *>(expr))
                return "str";
            if (dynamic_cast<const BoolLiteral *>(expr))
                return "bool";
            if (dynamic_cast<const NoneLiteral *>(expr))
                return "none";
            if (dynamic_cast<const ListLiteral *>(expr))
                return "list";
            if (dynamic_cast<const TupleLiteral *>(expr))
                return "tuple";
            if (dynamic_cast<const SetLiteral *>(expr))
                return "set";
            if (dynamic_cast<const MapLiteral *>(expr))
                return "map";
            return ""; // unknown / can't infer statically
        }

        void visitStmt(const Stmt *stmt, const std::string &scope)
        {
            if (!stmt)
                return;

            if (auto *a = dynamic_cast<const Assignment *>(stmt))
            {
                std::string itype = inferTypeFromExpr(a->value.get());
                addSymbol(a->name, SymbolKind::Variable, a->line,
                          a->name + " = ...", scope, {}, {}, "",
                          0, "", itype, false, false);
            }
            else if (auto *imm = dynamic_cast<const ImmutableBinding *>(stmt))
            {
                std::string itype = inferTypeFromExpr(imm->value.get());
                addSymbol(imm->name, SymbolKind::Variable, imm->line,
                          "be " + imm->name + " = ...", scope, {}, {}, "",
                          0, "", itype, false, true);
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
                int endLine = computeEndLine(mod->body, mod->line);
                addSymbol(mod->name, SymbolKind::Module, mod->line,
                          "module " + mod->name, scope, {}, {}, "",
                          endLine, "module", "", mod->isExported, false);
                for (auto &s : mod->body)
                {
                    visitStmt(s.get(), mod->name);
                    // Track children names
                    if (auto *childFn = dynamic_cast<const FnDef *>(s.get()))
                        addChild(mod->name, scope, childFn->name);
                    else if (auto *childA = dynamic_cast<const Assignment *>(s.get()))
                        addChild(mod->name, scope, childA->name);
                    else if (auto *childExp = dynamic_cast<const ExportDecl *>(s.get()))
                    {
                        if (auto *ef = dynamic_cast<const FnDef *>(childExp->declaration.get()))
                            addChild(mod->name, scope, ef->name);
                        else if (auto *ea = dynamic_cast<const Assignment *>(childExp->declaration.get()))
                            addChild(mod->name, scope, ea->name);
                        else if (auto *em = dynamic_cast<const ModuleDef *>(childExp->declaration.get()))
                            addChild(mod->name, scope, em->name);
                    }
                }
            }
            else if (auto *exp = dynamic_cast<const ExportDecl *>(stmt))
            {
                // Visit the inner declaration, then mark it exported
                size_t before = symbols_.size();
                visitStmt(exp->declaration.get(), scope);
                // Mark any newly added symbols as exported
                for (size_t idx = before; idx < symbols_.size(); idx++)
                    symbols_[idx].isExported = true;
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
                // Record imported names, carrying source file path when available
                if (!bring->aliases.empty())
                {
                    // For aliased imports, try to derive source file from parts
                    std::string srcFile;
                    if (!bring->parts.empty())
                        srcFile = bring->parts[0].filePath; // from "file.xel"
                    for (auto &alias : bring->aliases)
                        addSymbol(alias, SymbolKind::Import, bring->line,
                                  "import " + alias, scope,
                                  {}, {}, "", 0, "", "", false, false, srcFile);
                }
                else
                {
                    for (auto &part : bring->parts)
                    {
                        for (auto &item : part.items)
                            addSymbol(item, SymbolKind::Import, bring->line,
                                      "import " + item, scope,
                                      {}, {}, "", 0, "", "", false, false, part.filePath);
                    }
                }
            }
            else if (auto *forStmt = dynamic_cast<const ForStmt *>(stmt))
            {
                int forEnd = computeEndLine(forStmt->body, forStmt->line);
                for (auto &vn : forStmt->varNames)
                    addSymbol(vn, SymbolKind::Variable, forStmt->line,
                              "for " + vn + " in ...", scope,
                              {}, {}, "", forEnd, "for");
                if (forStmt->hasRest)
                    addSymbol(forStmt->restName, SymbolKind::Variable, forStmt->line,
                              "for ..." + forStmt->restName, scope,
                              {}, {}, "", forEnd, "for");
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
                for (const auto &clause : tryStmt->catchClauses)
                {
                    if (!clause.varName.empty())
                        addSymbol(clause.varName, SymbolKind::Variable, tryStmt->line,
                                  "catch " + clause.varName, scope);
                    for (auto &s : clause.body)
                        visitStmt(s.get(), scope);
                }
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

            int endLine = computeEndLine(fn->body, fn->line);
            SymbolKind kind = scope.empty() ? SymbolKind::Function : SymbolKind::Method;
            addSymbol(fn->name, kind, fn->line, detail, scope,
                      fn->params, fn->paramTypes, fn->returnType,
                      endLine, "function", "fn");

            // Register parameters as symbols too
            for (auto &p : fn->params)
                addSymbol(p, SymbolKind::Parameter, fn->line,
                          "param of " + fn->name, fn->name);
            if (fn->isVariadic && !fn->variadicName.empty())
                addSymbol(fn->variadicName, SymbolKind::Parameter, fn->line,
                          "variadic param of " + fn->name, fn->name);

            // Visit body for inner definitions
            std::string innerScope = scope.empty() ? fn->name : scope;
            for (auto &s : fn->body)
                visitStmt(s.get(), innerScope);
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

            int endLine = computeClassEndLine(cls);
            addSymbol(cls->name, SymbolKind::Class, cls->line, detail, scope,
                      {}, {}, "", endLine, "class");

            // Fields
            for (auto &f : cls->fields)
            {
                addSymbol(f.name, SymbolKind::Field, f.line,
                          cls->name + "." + f.name, cls->name);
                addChild(cls->name, scope, f.name);
            }

            // Methods
            for (auto &m : cls->methods)
            {
                visitFnDef(m.get(), cls->name);
                addChild(cls->name, scope, m->name);
            }

            // Properties
            for (auto &p : cls->properties)
            {
                addSymbol(p.name, SymbolKind::Property, p.line,
                          "property " + cls->name + "." + p.name, cls->name);
                addChild(cls->name, scope, p.name);
            }
        }

        void visitStructDef(const StructDef *st, const std::string &scope)
        {
            std::string detail = "struct " + st->name;
            int endLine = computeStructEndLine(st);
            addSymbol(st->name, SymbolKind::Struct, st->line, detail, scope,
                      {}, {}, "", endLine, "struct");

            for (auto &f : st->fields)
            {
                addSymbol(f.name, SymbolKind::Field, f.line,
                          st->name + "." + f.name, st->name);
                addChild(st->name, scope, f.name);
            }

            for (auto &m : st->methods)
            {
                visitFnDef(m.get(), st->name);
                addChild(st->name, scope, m->name);
            }
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
