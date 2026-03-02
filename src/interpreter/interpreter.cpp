#include "interpreter.hpp"
#include "../builtins/register_all.hpp"
#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <fstream>
#include <limits>

// Portable dirname — returns directory part of a path (no <filesystem> needed)
static std::string parentDir(const std::string &path)
{
    auto pos = path.find_last_of("/\\");
    if (pos == std::string::npos)
        return ".";
    return path.substr(0, pos);
}

// Portable canonical-ish path resolution (no <filesystem> needed)
static std::string resolvePath(const std::string &base, const std::string &relative)
{
    if (!relative.empty() && (relative[0] == '/' || (relative.size() > 1 && relative[1] == ':')))
        return relative; // absolute path
    return parentDir(base) + "/" + relative;
}

// Portable realpath (POSIX realpath, Windows _fullpath)
static std::string canonicalPath(const std::string &path)
{
#ifdef _WIN32
    char buf[_MAX_PATH];
    if (_fullpath(buf, path.c_str(), _MAX_PATH))
        return buf;
    return path;
#else
    char *resolved = ::realpath(path.c_str(), nullptr);
    if (resolved)
    {
        std::string result(resolved);
        ::free(resolved);
        return result;
    }
    return path; // fallback if file doesn't exist yet
#endif
}

namespace xell
{

    // ========================================================================
    // AST helper: detect yield expressions in a function body
    // ========================================================================

    static bool containsYieldExpr(const Expr *expr);
    static bool containsYieldStmt(const Stmt *stmt);

    static bool containsYieldExpr(const Expr *expr)
    {
        if (!expr)
            return false;
        if (dynamic_cast<const YieldExpr *>(expr))
            return true;
        if (auto *p = dynamic_cast<const BinaryExpr *>(expr))
            return containsYieldExpr(p->left.get()) || containsYieldExpr(p->right.get());
        if (auto *p = dynamic_cast<const UnaryExpr *>(expr))
            return containsYieldExpr(p->operand.get());
        if (auto *p = dynamic_cast<const CallExpr *>(expr))
        {
            for (const auto &arg : p->args)
                if (containsYieldExpr(arg.get()))
                    return true;
            return false;
        }
        if (auto *p = dynamic_cast<const TernaryExpr *>(expr))
            return containsYieldExpr(p->value.get()) || containsYieldExpr(p->condition.get()) || containsYieldExpr(p->alternative.get());
        return false;
    }

    static bool containsYieldStmt(const Stmt *stmt)
    {
        if (!stmt)
            return false;
        if (auto *p = dynamic_cast<const ExprStmt *>(stmt))
            return containsYieldExpr(p->expr.get());
        if (auto *p = dynamic_cast<const Assignment *>(stmt))
            return containsYieldExpr(p->value.get());
        if (auto *p = dynamic_cast<const IfStmt *>(stmt))
        {
            for (const auto &s : p->body)
                if (containsYieldStmt(s.get()))
                    return true;
            for (const auto &elif : p->elifs)
                for (const auto &s : elif.body)
                    if (containsYieldStmt(s.get()))
                        return true;
            for (const auto &s : p->elseBody)
                if (containsYieldStmt(s.get()))
                    return true;
            return false;
        }
        if (auto *p = dynamic_cast<const ForStmt *>(stmt))
        {
            for (const auto &s : p->body)
                if (containsYieldStmt(s.get()))
                    return true;
            return false;
        }
        if (auto *p = dynamic_cast<const WhileStmt *>(stmt))
        {
            for (const auto &s : p->body)
                if (containsYieldStmt(s.get()))
                    return true;
            return false;
        }
        if (auto *p = dynamic_cast<const GiveStmt *>(stmt))
            return p->value ? containsYieldExpr(p->value.get()) : false;
        if (auto *p = dynamic_cast<const TryCatchStmt *>(stmt))
        {
            for (const auto &s : p->tryBody)
                if (containsYieldStmt(s.get()))
                    return true;
            for (const auto &s : p->catchBody)
                if (containsYieldStmt(s.get()))
                    return true;
            return false;
        }
        return false;
    }

    static bool containsYield(const std::vector<std::unique_ptr<Stmt>> &stmts)
    {
        for (const auto &stmt : stmts)
        {
            if (containsYieldStmt(stmt.get()))
                return true;
        }
        return false;
    }

    // ========================================================================
    // Constructor / reset
    // ========================================================================

    Interpreter::Interpreter()
        : currentEnv_(&globalEnv_)
    {
        registerBuiltins();
    }

    void Interpreter::reset()
    {
        globalEnv_ = Environment();
        currentEnv_ = &globalEnv_;
        output_.clear();
        callDepth_ = 0;
        importedFiles_.clear();
        importedModules_.clear();
        registerBuiltins();
    }

    // ========================================================================
    // run — top-level entry point
    // ========================================================================

    void Interpreter::run(const Program &program)
    {
        currentEnv_ = &globalEnv_;
        for (const auto &stmt : program.statements)
        {
            exec(stmt.get());
        }
    }

    // ========================================================================
    // Built-in functions
    // ========================================================================

    void Interpreter::registerBuiltins()
    {
        builtins_.clear();
        registerAllBuiltins(builtins_, output_, shellState_);

        // ---- Math constants (injected into global environment) ----
        globalEnv_.define("PI", XObject::makeFloat(3.14159265358979323846));
        globalEnv_.define("E", XObject::makeFloat(2.71828182845904523536));
        globalEnv_.define("INF", XObject::makeFloat(std::numeric_limits<double>::infinity()));

        // ---- Higher-order function builtins (need interpreter access) ----

        // map(list, fn) → apply fn to each element
        builtins_["map"] = [this](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("map", 2, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("map() expects a list as first argument", line);
            if (!args[1].isFunction())
                throw TypeError("map() expects a function as second argument", line);
            const auto &list = args[0].asList();
            const auto &fn = args[1].asFunction();
            XList result;
            for (const auto &item : list)
            {
                std::vector<XObject> callArgs = {item};
                result.push_back(callUserFn(fn, callArgs, line));
            }
            return XObject::makeList(std::move(result));
        };

        // filter(list, fn) → keep elements where fn returns true
        builtins_["filter"] = [this](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("filter", 2, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("filter() expects a list as first argument", line);
            if (!args[1].isFunction())
                throw TypeError("filter() expects a function as second argument", line);
            const auto &list = args[0].asList();
            const auto &fn = args[1].asFunction();
            XList result;
            for (const auto &item : list)
            {
                std::vector<XObject> callArgs = {item};
                XObject res = callUserFn(fn, callArgs, line);
                if (res.truthy())
                    result.push_back(item);
            }
            return XObject::makeList(std::move(result));
        };

        // reduce(list, fn, init) → reduce to single value
        builtins_["reduce"] = [this](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("reduce", 3, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("reduce() expects a list as first argument", line);
            if (!args[1].isFunction())
                throw TypeError("reduce() expects a function as second argument", line);
            const auto &list = args[0].asList();
            const auto &fn = args[1].asFunction();
            XObject acc = args[2];
            for (const auto &item : list)
            {
                std::vector<XObject> callArgs = {acc, item};
                acc = callUserFn(fn, callArgs, line);
            }
            return acc;
        };

        // any(list, fn) → true if at least one element matches
        builtins_["any"] = [this](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("any", 2, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("any() expects a list as first argument", line);
            if (!args[1].isFunction())
                throw TypeError("any() expects a function as second argument", line);
            const auto &list = args[0].asList();
            const auto &fn = args[1].asFunction();
            for (const auto &item : list)
            {
                std::vector<XObject> callArgs = {item};
                if (callUserFn(fn, callArgs, line).truthy())
                    return XObject::makeBool(true);
            }
            return XObject::makeBool(false);
        };

        // all(list, fn) → true if all elements match
        builtins_["all"] = [this](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("all", 2, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("all() expects a list as first argument", line);
            if (!args[1].isFunction())
                throw TypeError("all() expects a function as second argument", line);
            const auto &list = args[0].asList();
            const auto &fn = args[1].asFunction();
            for (const auto &item : list)
            {
                std::vector<XObject> callArgs = {item};
                if (!callUserFn(fn, callArgs, line).truthy())
                    return XObject::makeBool(false);
            }
            return XObject::makeBool(true);
        };
    }

    // ========================================================================
    // Statement execution
    // ========================================================================

    void Interpreter::exec(const Stmt *stmt)
    {
        if (auto *p = dynamic_cast<const Assignment *>(stmt))
            return execAssignment(p);
        if (auto *p = dynamic_cast<const IfStmt *>(stmt))
            return execIf(p);
        if (auto *p = dynamic_cast<const ForStmt *>(stmt))
            return execFor(p);
        if (auto *p = dynamic_cast<const WhileStmt *>(stmt))
            return execWhile(p);
        if (auto *p = dynamic_cast<const FnDef *>(stmt))
            return execFnDef(p);
        if (auto *p = dynamic_cast<const GiveStmt *>(stmt))
            return execGive(p);
        if (auto *p = dynamic_cast<const BreakStmt *>(stmt))
            throw BreakSignal{};
        if (auto *p = dynamic_cast<const ContinueStmt *>(stmt))
            throw ContinueSignal{};
        if (auto *p = dynamic_cast<const ExprStmt *>(stmt))
            return execExprStmt(p);
        if (auto *p = dynamic_cast<const BringStmt *>(stmt))
            return execBring(p);
        if (auto *p = dynamic_cast<const TryCatchStmt *>(stmt))
            return execTryCatch(p);
        if (auto *p = dynamic_cast<const InCaseStmt *>(stmt))
            return execInCase(p);
        if (auto *p = dynamic_cast<const DestructuringAssignment *>(stmt))
            return execDestructuring(p);
        if (auto *p = dynamic_cast<const EnumDef *>(stmt))
            return execEnumDef(p);
        if (auto *p = dynamic_cast<const DecoratedFnDef *>(stmt))
            return execDecoratedFnDef(p);
    }

    void Interpreter::execBlock(const std::vector<StmtPtr> &stmts, Environment &env)
    {
        auto *savedEnv = currentEnv_;
        currentEnv_ = &env;
        try
        {
            for (const auto &stmt : stmts)
            {
                exec(stmt.get());
            }
        }
        catch (...)
        {
            currentEnv_ = savedEnv;
            throw;
        }
        currentEnv_ = savedEnv;
    }

    void Interpreter::execAssignment(const Assignment *node)
    {
        XObject value = eval(node->value.get());
        currentEnv_->set(node->name, std::move(value));
    }

    void Interpreter::execIf(const IfStmt *node)
    {
        // Check main if condition
        if (eval(node->condition.get()).truthy())
        {
            Environment blockEnv(currentEnv_);
            execBlock(node->body, blockEnv);
            return;
        }

        // Check elif clauses
        for (const auto &elif : node->elifs)
        {
            if (eval(elif.condition.get()).truthy())
            {
                Environment blockEnv(currentEnv_);
                execBlock(elif.body, blockEnv);
                return;
            }
        }

        // Else clause
        if (!node->elseBody.empty())
        {
            Environment blockEnv(currentEnv_);
            execBlock(node->elseBody, blockEnv);
        }
    }

    void Interpreter::execFor(const ForStmt *node)
    {
        XObject iterable = eval(node->iterable.get());
        if (!iterable.isList())
            throw TypeError("for..in requires a list, got " +
                                std::string(xtype_name(iterable.type())),
                            node->line);

        Environment loopEnv(currentEnv_);
        auto *savedEnv = currentEnv_;
        currentEnv_ = &loopEnv;

        try
        {
            const auto &list = iterable.asList();
            for (size_t i = 0; i < list.size(); i++)
            {
                loopEnv.define(node->varName, list[i]);
                try
                {
                    for (const auto &stmt : node->body)
                    {
                        exec(stmt.get());
                    }
                }
                catch (const BreakSignal &)
                {
                    break;
                }
                catch (const ContinueSignal &)
                {
                    continue;
                }
            }
        }
        catch (...)
        {
            currentEnv_ = savedEnv;
            throw;
        }

        currentEnv_ = savedEnv;
    }

    void Interpreter::execWhile(const WhileStmt *node)
    {
        Environment loopEnv(currentEnv_);
        auto *savedEnv = currentEnv_;
        currentEnv_ = &loopEnv;

        try
        {
            while (eval(node->condition.get()).truthy())
            {
                try
                {
                    for (const auto &stmt : node->body)
                    {
                        exec(stmt.get());
                    }
                }
                catch (const BreakSignal &)
                {
                    break;
                }
                catch (const ContinueSignal &)
                {
                    continue;
                }
            }
        }
        catch (...)
        {
            currentEnv_ = savedEnv;
            throw;
        }

        currentEnv_ = savedEnv;
    }

    void Interpreter::execFnDef(const FnDef *node)
    {
        // Capture the current environment as the lexical closure scope
        auto fn = XObject::makeFunction(node->name, node->params, &node->body, currentEnv_);

        // Store default parameter AST pointers (non-owning) and variadic info
        XFunction &fnRef = const_cast<XFunction &>(fn.asFunction());
        fnRef.defaults.clear();
        for (const auto &d : node->defaults)
        {
            fnRef.defaults.push_back(d.get()); // raw non-owning pointer
        }
        fnRef.isVariadic = node->isVariadic;
        fnRef.variadicName = node->variadicName;

        // Check if this function is a generator (contains yield statements)
        // We do a simple AST scan
        fnRef.isGenerator = containsYield(node->body);

        // Async flag
        fnRef.isAsync = node->isAsync;

        // Store type annotations for runtime checking
        // (paramTypes and returnType are stored in the FnDef AST node and
        //  can be accessed via the AST pointer in the function)

        currentEnv_->set(node->name, std::move(fn));
    }

    void Interpreter::execGive(const GiveStmt *node)
    {
        XObject value = node->value ? eval(node->value.get()) : XObject::makeNone();
        throw GiveSignal{std::move(value)};
    }

    void Interpreter::execExprStmt(const ExprStmt *node)
    {
        eval(node->expr.get());
    }

    // ========================================================================
    // Bring (import)
    // ========================================================================

    void Interpreter::execBring(const BringStmt *node)
    {
        // 1. Resolve the file path relative to the current source file
        std::string rawPath = node->path;
        std::string resolvedPath;
        if (sourceFile_.empty())
            resolvedPath = canonicalPath(rawPath);
        else
            resolvedPath = canonicalPath(resolvePath(sourceFile_, rawPath));

        // 2. Check for circular imports
        if (importedFiles_.count(resolvedPath))
            throw BringError("Circular import detected: '" + rawPath + "'", node->line);

        // 3. Read the source file
        std::ifstream f(resolvedPath);
        if (!f.is_open())
            throw BringError("Cannot open file '" + rawPath + "' (resolved to '" + resolvedPath + "')", node->line);
        std::ostringstream ss;
        ss << f.rdbuf();
        std::string source = ss.str();

        // 4. Lex → Parse into a module that we keep alive
        auto mod = std::make_unique<ImportedModule>();
        mod->interp = std::make_unique<Interpreter>();
        mod->interp->sourceFile_ = resolvedPath;
        // Share the circular-import guard (pass current set + this file)
        mod->interp->importedFiles_ = importedFiles_;
        mod->interp->importedFiles_.insert(resolvedPath);

        try
        {
            Lexer lexer(source);
            auto tokens = lexer.tokenize();
            Parser parser(tokens);
            mod->program = parser.parse();
            mod->interp->run(mod->program);
        }
        catch (const XellError &e)
        {
            throw BringError("Error in '" + rawPath + "': " + e.what(), node->line);
        }

        // 5. Extract names from the child's global environment
        Environment &childEnv = mod->interp->globals();

        if (node->bringAll)
        {
            // bring * from "file" — bring everything
            // User-defined imports can shadow builtins (evalCall checks user scope first)
            auto names = childEnv.allNames();
            for (const auto &name : names)
            {
                try
                {
                    XObject val = childEnv.get(name, node->line);
                    currentEnv_->define(name, std::move(val));
                }
                catch (...)
                {
                    // skip inaccessible names
                }
            }
        }
        else
        {
            // bring name1, name2 from "file" [as alias1, alias2]
            for (size_t i = 0; i < node->names.size(); ++i)
            {
                const std::string &name = node->names[i];
                std::string alias = (i < node->aliases.size()) ? node->aliases[i] : name;

                try
                {
                    XObject val = childEnv.get(name, node->line);
                    currentEnv_->define(alias, std::move(val));
                }
                catch (const UndefinedVariableError &)
                {
                    throw BringError("Name '" + name + "' not found in '" + rawPath + "'", node->line);
                }
            }
        }

        // 6. Keep the module alive so AST pointers & closureEnvs stay valid
        importedModules_.push_back(std::move(mod));
    }

    // ========================================================================
    // Expression evaluation
    // ========================================================================

    XObject Interpreter::eval(const Expr *expr)
    {
        // Literals
        if (auto *p = dynamic_cast<const NumberLiteral *>(expr))
            return XObject::makeFloat(p->value);

        if (auto *p = dynamic_cast<const IntLiteral *>(expr))
            return XObject::makeInt(p->value);

        if (auto *p = dynamic_cast<const FloatLiteral *>(expr))
            return XObject::makeFloat(p->value);

        if (auto *p = dynamic_cast<const ImaginaryLiteral *>(expr))
            return XObject::makeComplex(0.0, p->value);

        if (auto *p = dynamic_cast<const BoolLiteral *>(expr))
            return XObject::makeBool(p->value);

        if (dynamic_cast<const NoneLiteral *>(expr))
            return XObject::makeNone();

        if (auto *p = dynamic_cast<const StringLiteral *>(expr))
        {
            // Raw strings skip interpolation
            if (p->isRaw)
                return XObject::makeString(p->value);
            // Check for interpolation markers
            if (p->value.find('{') != std::string::npos)
                return XObject::makeString(interpolate(p->value, p->line));
            return XObject::makeString(p->value);
        }

        // Identifier
        if (auto *p = dynamic_cast<const Identifier *>(expr))
            return currentEnv_->get(p->name, p->line);

        // Compound literals
        if (auto *p = dynamic_cast<const ListLiteral *>(expr))
            return evalList(p);
        if (auto *p = dynamic_cast<const TupleLiteral *>(expr))
            return evalTuple(p);
        if (auto *p = dynamic_cast<const SetLiteral *>(expr))
            return evalSet(p);
        if (auto *p = dynamic_cast<const FrozenSetLiteral *>(expr))
            return evalFrozenSet(p);
        if (auto *p = dynamic_cast<const MapLiteral *>(expr))
            return evalMap(p);

        // Operators
        if (auto *p = dynamic_cast<const BinaryExpr *>(expr))
            return evalBinary(p);
        if (auto *p = dynamic_cast<const UnaryExpr *>(expr))
            return evalUnary(p);
        if (auto *p = dynamic_cast<const PostfixExpr *>(expr))
            return evalPostfix(p);

        // Calls & access
        if (auto *p = dynamic_cast<const CallExpr *>(expr))
            return evalCall(p);
        if (auto *p = dynamic_cast<const IndexAccess *>(expr))
            return evalIndex(p);
        if (auto *p = dynamic_cast<const MemberAccess *>(expr))
            return evalMember(p);

        // New expression types
        if (auto *p = dynamic_cast<const TernaryExpr *>(expr))
            return evalTernary(p);
        if (auto *p = dynamic_cast<const LambdaExpr *>(expr))
            return evalLambda(p);
        if (auto *p = dynamic_cast<const SpreadExpr *>(expr))
            return evalSpread(p);
        if (auto *p = dynamic_cast<const YieldExpr *>(expr))
            return evalYield(p);
        if (auto *p = dynamic_cast<const AwaitExpr *>(expr))
            return evalAwait(p);
        if (auto *p = dynamic_cast<const BytesLiteral *>(expr))
            return evalBytes(p);

        throw NotImplementedError("unknown expression node", expr->line);
    }

    // ---- Binary expressions ------------------------------------------------

    XObject Interpreter::evalBinary(const BinaryExpr *node)
    {
        const std::string &op = node->op;

        // ================================================================
        // Shell pipe operator: "cmd1" | "cmd2"  →  "cmd1 | cmd2"
        // Builds a pipeline string by concatenating with " | "
        // ================================================================
        if (op == "|")
        {
            XObject left = eval(node->left.get());
            XObject right = eval(node->right.get());
            if (!left.isString() || !right.isString())
                throw TypeError("pipe operator '|' requires string operands (command strings), got " +
                                    std::string(xtype_name(left.type())) + " and " +
                                    std::string(xtype_name(right.type())),
                                node->line);
            return XObject::makeString(left.asString() + " | " + right.asString());
        }

        // ================================================================
        // Shell AND: expr1 && expr2
        //   - If left is a number: 0 = success → eval right; non-0 = fail → return left
        //   - If left is a map with "exit_code": use that exit code
        //   - Otherwise: truthy → eval right; falsy → return left
        // ================================================================
        if (op == "&&")
        {
            XObject left = eval(node->left.get());

            bool isSuccess = false;
            if (left.isNumber())
                isSuccess = (left.asNumber() == 0.0);
            else if (left.isMap() && left.asMap().get("exit_code"))
                isSuccess = (left.asMap().get("exit_code")->asNumber() == 0.0);
            else
                isSuccess = left.truthy();

            if (isSuccess)
                return eval(node->right.get());
            return left;
        }

        // ================================================================
        // Shell OR: expr1 || expr2
        //   - If left is a number: 0 = success → return left; non-0 = fail → eval right
        //   - If left is a map with "exit_code": use that exit code
        //   - Otherwise: truthy → return left; falsy → eval right
        // ================================================================
        if (op == "||")
        {
            XObject left = eval(node->left.get());

            bool isSuccess = false;
            if (left.isNumber())
                isSuccess = (left.asNumber() == 0.0);
            else if (left.isMap() && left.asMap().get("exit_code"))
                isSuccess = (left.asMap().get("exit_code")->asNumber() == 0.0);
            else
                isSuccess = left.truthy();

            if (!isSuccess)
                return eval(node->right.get());
            return left;
        }

        // Short-circuit logical operators
        if (op == "and")
        {
            XObject left = eval(node->left.get());
            if (!left.truthy())
                return left;
            return eval(node->right.get());
        }
        if (op == "or")
        {
            XObject left = eval(node->left.get());
            if (left.truthy())
                return left;
            return eval(node->right.get());
        }

        // Evaluate both sides
        XObject left = eval(node->left.get());
        XObject right = eval(node->right.get());

        // Equality / inequality (any types)
        if (op == "==")
            return XObject::makeBool(left.equals(right));
        if (op == "!=")
            return XObject::makeBool(!left.equals(right));

        // Arithmetic / string concatenation
        if (op == "+")
        {
            // Complex + anything numeric
            if (left.isNumeric() && right.isNumeric())
            {
                if (left.isComplex() || right.isComplex())
                {
                    XComplex a = left.isComplex() ? left.asComplex() : XComplex(left.asNumber(), 0.0);
                    XComplex b = right.isComplex() ? right.asComplex() : XComplex(right.asNumber(), 0.0);
                    XComplex result = a + b;
                    // If result has no imaginary part and came from int+imaginary, stay as complex
                    return XObject::makeComplex(result);
                }
                // Both are int/float
                if (left.isInt() && right.isInt())
                    return XObject::makeInt(left.asInt() + right.asInt());
                return XObject::makeFloat(left.asNumber() + right.asNumber());
            }
            if (left.isString() || right.isString())
            {
                // Auto-convert to string for concatenation
                return XObject::makeString(left.toString() + right.toString());
            }
            // List concatenation
            if (left.isList() && right.isList())
            {
                XList result = left.asList();
                const auto &rhs = right.asList();
                result.insert(result.end(), rhs.begin(), rhs.end());
                return XObject::makeList(std::move(result));
            }
            throw TypeError("unsupported operand types for +: " +
                                std::string(xtype_name(left.type())) + " and " +
                                std::string(xtype_name(right.type())),
                            node->line);
        }

        // Numeric-only arithmetic
        if (op == "-" || op == "*" || op == "/" || op == "%")
        {
            if (!left.isNumeric() || !right.isNumeric())
                throw TypeError("unsupported operand types for " + op + ": " +
                                    std::string(xtype_name(left.type())) + " and " +
                                    std::string(xtype_name(right.type())),
                                node->line);

            // Complex arithmetic
            if (left.isComplex() || right.isComplex())
            {
                XComplex a = left.isComplex() ? left.asComplex() : XComplex(left.asNumber(), 0.0);
                XComplex b = right.isComplex() ? right.asComplex() : XComplex(right.asNumber(), 0.0);
                if (op == "-")
                    return XObject::makeComplex(a - b);
                if (op == "*")
                    return XObject::makeComplex(a * b);
                if (op == "%")
                    throw TypeError("modulo (%) not supported for complex numbers", node->line);
                // op == "/"
                if (b.real == 0.0 && b.imag == 0.0)
                    throw DivisionByZeroError(node->line);
                return XObject::makeComplex(a / b);
            }

            // Integer arithmetic (preserves int type when both are int)
            if (left.isInt() && right.isInt())
            {
                int64_t l = left.asInt(), r = right.asInt();
                if (op == "-")
                    return XObject::makeInt(l - r);
                if (op == "*")
                    return XObject::makeInt(l * r);
                if (op == "%")
                {
                    if (r == 0)
                        throw DivisionByZeroError(node->line);
                    return XObject::makeInt(l % r);
                }
                // op == "/" → integer division only if it divides evenly
                if (r == 0)
                    throw DivisionByZeroError(node->line);
                if (l % r == 0)
                    return XObject::makeInt(l / r);
                return XObject::makeFloat(static_cast<double>(l) / static_cast<double>(r));
            }

            // Float arithmetic (at least one is float)
            double l = left.asNumber(), r = right.asNumber();
            if (op == "-")
                return XObject::makeFloat(l - r);
            if (op == "*")
                return XObject::makeFloat(l * r);
            if (op == "%")
            {
                if (r == 0.0)
                    throw DivisionByZeroError(node->line);
                return XObject::makeFloat(std::fmod(l, r));
            }
            // op == "/"
            if (r == 0.0)
                throw DivisionByZeroError(node->line);
            return XObject::makeFloat(l / r);
        }

        // Comparison (numbers and strings)
        if (op == ">" || op == "<" || op == ">=" || op == "<=")
        {
            if (left.isNumeric() && right.isNumeric())
            {
                if (left.isComplex() || right.isComplex())
                    throw TypeError("comparison not supported for complex numbers", node->line);
                double l = left.asNumber(), r = right.asNumber();
                if (op == ">")
                    return XObject::makeBool(l > r);
                if (op == "<")
                    return XObject::makeBool(l < r);
                if (op == ">=")
                    return XObject::makeBool(l >= r);
                return XObject::makeBool(l <= r);
            }
            if (left.isString() && right.isString())
            {
                const auto &l = left.asString();
                const auto &r = right.asString();
                if (op == ">")
                    return XObject::makeBool(l > r);
                if (op == "<")
                    return XObject::makeBool(l < r);
                if (op == ">=")
                    return XObject::makeBool(l >= r);
                return XObject::makeBool(l <= r);
            }
            throw TypeError("unsupported operand types for " + op + ": " +
                                std::string(xtype_name(left.type())) + " and " +
                                std::string(xtype_name(right.type())),
                            node->line);
        }

        throw NotImplementedError("binary operator '" + op + "'", node->line);
    }

    // ---- Unary expressions -------------------------------------------------

    XObject Interpreter::evalUnary(const UnaryExpr *node)
    {
        const std::string &op = node->op;

        if (op == "not" || op == "!")
        {
            return XObject::makeBool(!eval(node->operand.get()).truthy());
        }

        if (op == "-")
        {
            XObject val = eval(node->operand.get());
            if (val.isInt())
                return XObject::makeInt(-val.asInt());
            if (val.isFloat())
                return XObject::makeFloat(-val.asFloat());
            if (val.isComplex())
                return XObject::makeComplex(-val.asComplex());
            throw TypeError("unary '-' requires a number, got " +
                                std::string(xtype_name(val.type())),
                            node->line);
        }

        // Prefix ++ and --
        if (op == "++" || op == "--")
        {
            auto *ident = dynamic_cast<const Identifier *>(node->operand.get());
            if (!ident)
                throw TypeError("prefix " + op + " requires a variable", node->line);

            XObject val = currentEnv_->get(ident->name, node->line);
            if (!val.isNumeric())
                throw TypeError("prefix " + op + " requires a number", node->line);

            if (val.isInt())
            {
                int64_t newVal = val.asInt() + (op == "++" ? 1 : -1);
                XObject result = XObject::makeInt(newVal);
                currentEnv_->set(ident->name, XObject::makeInt(newVal));
                return result;
            }
            double newVal = val.asNumber() + (op == "++" ? 1.0 : -1.0);
            XObject result = XObject::makeFloat(newVal);
            currentEnv_->set(ident->name, XObject::makeFloat(newVal));
            return result; // prefix returns new value
        }

        throw NotImplementedError("unary operator '" + op + "'", node->line);
    }

    // ---- Postfix expressions -----------------------------------------------

    XObject Interpreter::evalPostfix(const PostfixExpr *node)
    {
        auto *ident = dynamic_cast<const Identifier *>(node->operand.get());
        if (!ident)
            throw TypeError("postfix " + node->op + " requires a variable", node->line);

        XObject val = currentEnv_->get(ident->name, node->line);
        if (!val.isNumeric())
            throw TypeError("postfix " + node->op + " requires a number", node->line);

        if (val.isInt())
        {
            int64_t oldVal = val.asInt();
            int64_t newVal = oldVal + (node->op == "++" ? 1 : -1);
            currentEnv_->set(ident->name, XObject::makeInt(newVal));
            return XObject::makeInt(oldVal);
        }
        double oldVal = val.asNumber();
        double newVal = oldVal + (node->op == "++" ? 1.0 : -1.0);
        currentEnv_->set(ident->name, XObject::makeFloat(newVal));
        return XObject::makeFloat(oldVal); // postfix returns old value
    }

    // ---- Function calls ----------------------------------------------------

    XObject Interpreter::evalCall(const CallExpr *node)
    {
        // Evaluate arguments, handling spread
        std::vector<XObject> args;
        for (const auto &arg : node->args)
        {
            if (auto *spread = dynamic_cast<const SpreadExpr *>(arg.get()))
            {
                XObject val = eval(spread->operand.get());
                if (!val.isList())
                    throw TypeError("spread operator in function call requires a list, got " +
                                        std::string(xtype_name(val.type())),
                                    spread->line);
                for (const auto &item : val.asList())
                    args.push_back(item);
            }
            else
            {
                args.push_back(eval(arg.get()));
            }
        }

        // Check builtins first, BUT user-defined functions take precedence
        // (allows shadowing builtins, like Python)
        if (currentEnv_->has(node->callee))
        {
            XObject fnObj = currentEnv_->get(node->callee, node->line);
            if (fnObj.isFunction())
                return callUserFn(fnObj.asFunction(), args, node->line);
        }

        auto bit = builtins_.find(node->callee);
        if (bit != builtins_.end())
        {
            return bit->second(args, node->line);
        }

        // Look up user-defined function (throws if not found)
        XObject fnObj = currentEnv_->get(node->callee, node->line);
        if (!fnObj.isFunction())
            throw TypeError("'" + node->callee + "' is not a function", node->line);

        return callUserFn(fnObj.asFunction(), args, node->line);
    }

    XObject Interpreter::callUserFn(const XFunction &fn, std::vector<XObject> &args, int line)
    {
        // If this is a generator function, create a generator instead of executing
        if (fn.isGenerator)
            return createGenerator(fn, args, line);

        size_t minRequired = 0;
        // Count required params (those without defaults)
        for (size_t i = 0; i < fn.params.size(); i++)
        {
            if (i >= fn.defaults.size() || fn.defaults[i] == nullptr)
                minRequired = i + 1;
        }

        // Arity check with defaults and variadic
        if (fn.isVariadic)
        {
            // Variadic: need at least minRequired args
            if (args.size() < minRequired)
                throw ArityError(fn.name, (int)minRequired, (int)args.size(), line);
        }
        else
        {
            // Non-variadic: need at least minRequired, at most fn.params.size()
            if (args.size() < minRequired)
                throw ArityError(fn.name, (int)minRequired, (int)args.size(), line);
            if (args.size() > fn.params.size())
                throw ArityError(fn.name, (int)fn.params.size(), (int)args.size(), line);
        }

        // Recursion guard
        if (callDepth_ >= MAX_CALL_DEPTH)
            throw RecursionError(MAX_CALL_DEPTH, line);

        // Lexical scoping: parent = the environment where the function was *defined*
        Environment fnEnv(fn.closureEnv ? fn.closureEnv : currentEnv_);

        // Bind parameters
        for (size_t i = 0; i < fn.params.size(); i++)
        {
            if (i < args.size())
            {
                fnEnv.define(fn.params[i], std::move(args[i]));
            }
            else if (i < fn.defaults.size() && fn.defaults[i] != nullptr)
            {
                // Evaluate default value in the closure environment
                fnEnv.define(fn.params[i], eval(fn.defaults[i]));
            }
            else
            {
                fnEnv.define(fn.params[i], XObject::makeNone());
            }
        }

        // Bind variadic parameter: collect remaining args into a list
        if (fn.isVariadic && !fn.variadicName.empty())
        {
            XList varArgs;
            for (size_t i = fn.params.size(); i < args.size(); i++)
            {
                varArgs.push_back(std::move(args[i]));
            }
            fnEnv.define(fn.variadicName, XObject::makeList(std::move(varArgs)));
        }

        // Execute body, catching GiveSignal for return values
        callDepth_++;
        auto *savedEnv = currentEnv_;
        currentEnv_ = &fnEnv;

        XObject result = XObject::makeNone();
        try
        {
            // Lambda with single expression — evaluate and return it
            if (fn.lambdaSingleExpr)
            {
                result = eval(fn.lambdaSingleExpr);
            }
            else if (fn.body)
            {
                for (const auto &stmt : *fn.body)
                {
                    exec(stmt.get());
                }
            }
        }
        catch (GiveSignal &sig)
        {
            result = std::move(sig.value);
        }

        currentEnv_ = savedEnv;
        callDepth_--;
        return result;
    }

    // ---- Index access: obj[index] ------------------------------------------

    XObject Interpreter::evalIndex(const IndexAccess *node)
    {
        XObject obj = eval(node->object.get());
        XObject idx = eval(node->index.get());

        if (obj.isList())
        {
            if (!idx.isNumber())
                throw TypeError("list index must be a number", node->line);
            int index = (int)idx.asNumber();
            const auto &list = obj.asList();
            if (index < 0)
                index += (int)list.size();
            if (index < 0 || index >= (int)list.size())
                throw IndexError("list index " + std::to_string(index) + " out of range (size " +
                                     std::to_string(list.size()) + ")",
                                 node->line);
            return list[index];
        }

        if (obj.isTuple())
        {
            if (!idx.isNumber())
                throw TypeError("tuple index must be a number", node->line);
            int index = (int)idx.asNumber();
            const auto &tup = obj.asTuple();
            if (index < 0)
                index += (int)tup.size();
            if (index < 0 || index >= (int)tup.size())
                throw IndexError("tuple index " + std::to_string(index) + " out of range (size " +
                                     std::to_string(tup.size()) + ")",
                                 node->line);
            return tup[index];
        }

        if (obj.isMap())
        {
            // Map keys can be any hashable type — look up by XObject key
            const XObject *val = obj.asMap().get(idx);
            if (!val)
            {
                if (idx.isString())
                    throw KeyError(idx.asString(), node->line);
                else
                    throw KeyError(idx.toString(), node->line);
            }
            return *val;
        }

        if (obj.isString())
        {
            if (!idx.isNumber())
                throw TypeError("string index must be a number", node->line);
            int index = (int)idx.asNumber();
            const auto &str = obj.asString();
            if (index < 0)
                index += (int)str.size();
            if (index < 0 || index >= (int)str.size())
                throw IndexError("string index " + std::to_string(index) + " out of range (length " +
                                     std::to_string(str.size()) + ")",
                                 node->line);
            return XObject::makeString(std::string(1, str[index]));
        }

        throw TypeError("indexing not supported on " + std::string(xtype_name(obj.type())), node->line);
    }

    // ---- Member access: obj->member ----------------------------------------

    XObject Interpreter::evalMember(const MemberAccess *node)
    {
        XObject obj = eval(node->object.get());

        if (obj.isMap())
        {
            const XObject *val = obj.asMap().get(node->member);
            if (!val)
                throw KeyError(node->member, node->line);
            return *val;
        }

        // Enum member access: Color->Red
        if (obj.isEnum())
        {
            const auto &e = obj.asEnum();
            auto it = e.members.find(node->member);
            if (it == e.members.end())
                throw KeyError(node->member + " is not a member of enum " + e.name, node->line);
            return it->second;
        }

        // Bytes member access for properties
        if (obj.isBytes())
        {
            if (node->member == "length" || node->member == "len")
                return XObject::makeInt(static_cast<int64_t>(obj.asBytes().data.size()));
        }

        throw TypeError("member access (->) not supported on " +
                            std::string(xtype_name(obj.type())),
                        node->line);
    }

    // ---- List literal ------------------------------------------------------

    XObject Interpreter::evalList(const ListLiteral *node)
    {
        XList elements;
        for (const auto &elem : node->elements)
        {
            // Handle spread expressions within list literals
            if (auto *spread = dynamic_cast<const SpreadExpr *>(elem.get()))
            {
                XObject val = eval(spread->operand.get());
                if (!val.isList())
                    throw TypeError("spread operator requires a list, got " +
                                        std::string(xtype_name(val.type())),
                                    spread->line);
                for (const auto &item : val.asList())
                    elements.push_back(item);
            }
            else
            {
                elements.push_back(eval(elem.get()));
            }
        }
        return XObject::makeList(std::move(elements));
    }

    // ---- Map literal -------------------------------------------------------

    XObject Interpreter::evalMap(const MapLiteral *node)
    {
        XMap map;
        for (const auto &entry : node->entries)
        {
            // Map literal keys are strings (from parser: identifier or string literal)
            XObject key = XObject::makeString(entry.first);
            map.set(key, eval(entry.second.get()));
        }
        return XObject::makeMap(std::move(map));
    }

    // ---- Tuple literal -----------------------------------------------------

    XObject Interpreter::evalTuple(const TupleLiteral *node)
    {
        XTuple elements;
        for (const auto &elem : node->elements)
        {
            elements.push_back(eval(elem.get()));
        }
        return XObject::makeTuple(std::move(elements));
    }

    // ---- Set literal -------------------------------------------------------

    XObject Interpreter::evalSet(const SetLiteral *node)
    {
        XSet set;
        for (const auto &elem : node->elements)
        {
            XObject val = eval(elem.get());
            if (!isHashable(val))
                throw HashError("set elements must be hashable (immutable), got " +
                                    std::string(xtype_name(val.type())),
                                node->line);
            set.add(val);
        }
        return XObject::makeSet(std::move(set));
    }

    // ---- Frozen set literal ------------------------------------------------

    XObject Interpreter::evalFrozenSet(const FrozenSetLiteral *node)
    {
        XSet set;
        for (const auto &elem : node->elements)
        {
            XObject val = eval(elem.get());
            if (!isHashable(val))
                throw HashError("frozen set elements must be hashable (immutable), got " +
                                    std::string(xtype_name(val.type())),
                                node->line);
            set.add(val);
        }
        return XObject::makeFrozenSet(std::move(set));
    }

    // ---- Ternary expression: value if condition else alternative -----------

    XObject Interpreter::evalTernary(const TernaryExpr *node)
    {
        XObject condition = eval(node->condition.get());
        if (condition.truthy())
            return eval(node->value.get());
        return eval(node->alternative.get());
    }

    // ---- Lambda expression: creates an anonymous function ------------------

    XObject Interpreter::evalLambda(const LambdaExpr *node)
    {
        // Create a heap-allocated snapshot of the current environment
        // so the closure survives even after the enclosing function returns.
        auto ownedEnv = std::make_shared<Environment>(currentEnv_);
        // Copy all visible variables into the owned environment
        auto names = currentEnv_->allNames();
        for (const auto &n : names)
        {
            try
            {
                ownedEnv->define(n, currentEnv_->get(n, 0));
            }
            catch (...)
            {
            }
        }

        if (node->singleExpr)
        {
            auto fn = XObject::makeFunction("<lambda>", node->params, &node->body, ownedEnv.get());
            XFunction &fnRef = const_cast<XFunction &>(fn.asFunction());
            fnRef.lambdaSingleExpr = node->singleExpr.get();
            fnRef.ownedEnv = ownedEnv;
            return fn;
        }
        else
        {
            auto fn = XObject::makeFunction("<lambda>", node->params, &node->body, ownedEnv.get());
            XFunction &fnRef = const_cast<XFunction &>(fn.asFunction());
            fnRef.ownedEnv = ownedEnv;
            return fn;
        }
    }

    // ---- Spread expression (standalone) ------------------------------------

    XObject Interpreter::evalSpread(const SpreadExpr *node)
    {
        // When spread appears in a standalone context, just evaluate the operand
        // The actual spreading happens in evalList/evalCall
        return eval(node->operand.get());
    }

    // ---- Bytes literal: b"..." -------------------------------------------

    XObject Interpreter::evalBytes(const BytesLiteral *node)
    {
        // The lexer stores the raw byte content (with escape sequences processed)
        return XObject::makeBytes(node->bytes);
    }

    // ---- Yield expression (for generators) --------------------------------

    XObject Interpreter::evalYield(const YieldExpr *node)
    {
        if (!activeGeneratorState_)
            throw TypeError("yield can only be used inside a generator function", node->line);

        XObject value = node->value ? eval(node->value.get()) : XObject::makeNone();

        // Signal the yield to the generator orchestrator
        auto *gs = activeGeneratorState_;
        {
            std::lock_guard<std::mutex> lk(gs->mtx);
            delete gs->yieldedValue;
            gs->yieldedValue = new XObject(std::move(value));
            gs->phase = GeneratorState::YIELDED;
        }
        gs->cv.notify_all();

        // Wait for next() to resume us
        {
            std::unique_lock<std::mutex> lk(gs->mtx);
            gs->cv.wait(lk, [gs]
                        { return gs->phase == GeneratorState::RUNNING || gs->phase == GeneratorState::DONE; });
        }

        // If DONE was signaled, the generator is being abandoned
        if (gs->phase == GeneratorState::DONE)
            throw GiveSignal{XObject::makeNone()}; // unwind the generator

        return XObject::makeNone(); // yield always "returns" none to the generator body
    }

    // ---- Await expression (for async) ------------------------------------

    XObject Interpreter::evalAwait(const AwaitExpr *node)
    {
        XObject obj = eval(node->operand.get());

        // If the operand is a generator, exhaust it and return the last value
        if (obj.isGenerator())
        {
            auto &gen = const_cast<XGenerator &>(obj.asGenerator());
            auto &gs = gen.state;
            XObject lastValue = XObject::makeNone();

            while (true)
            {
                {
                    std::unique_lock<std::mutex> lk(gs->mtx);
                    if (gs->phase == GeneratorState::DONE)
                        break;

                    // Resume the generator
                    gs->phase = GeneratorState::RUNNING;
                }
                gs->cv.notify_all();

                // Wait for yield or completion
                {
                    std::unique_lock<std::mutex> lk(gs->mtx);
                    gs->cv.wait(lk, [&gs]
                                { return gs->phase == GeneratorState::YIELDED || gs->phase == GeneratorState::DONE; });
                }

                if (gs->phase == GeneratorState::YIELDED && gs->yieldedValue)
                    lastValue = gs->yieldedValue->clone();

                if (gs->phase == GeneratorState::DONE)
                {
                    if (gs->error)
                        std::rethrow_exception(gs->error);
                    break;
                }
            }

            if (gs->worker.joinable())
                gs->worker.join();

            return lastValue;
        }

        // For non-generator values, await is a no-op (returns the value as-is)
        // This allows simple async compatibility
        return obj;
    }

    // ---- Enum definition ---------------------------------------------------

    void Interpreter::execEnumDef(const EnumDef *node)
    {
        XEnum enumDef(node->name);
        int64_t autoValue = 0;

        for (size_t i = 0; i < node->members.size(); i++)
        {
            const std::string &memberName = node->members[i];
            enumDef.memberNames.push_back(memberName);

            if (i < node->memberValues.size() && node->memberValues[i])
            {
                // Custom value
                XObject val = eval(node->memberValues[i].get());
                if (val.isInt())
                    autoValue = val.asInt() + 1;
                enumDef.members[memberName] = std::move(val);
            }
            else
            {
                // Auto-increment integer value
                enumDef.members[memberName] = XObject::makeInt(autoValue);
                autoValue++;
            }
        }

        currentEnv_->set(node->name, XObject::makeEnum(std::move(enumDef)));
    }

    // ---- Decorated function definition -------------------------------------

    void Interpreter::execDecoratedFnDef(const DecoratedFnDef *node)
    {
        // First, execute the function definition normally
        execFnDef(node->fnDef.get());

        // Then wrap it with each decorator (bottom-up: last decorator is innermost)
        std::string fnName = node->fnDef->name;
        XObject fn = currentEnv_->get(fnName, node->line);

        for (auto it = node->decorators.rbegin(); it != node->decorators.rend(); ++it)
        {
            const std::string &decoratorName = *it;

            // Look up decorator function
            XObject decoratorFn;
            if (currentEnv_->has(decoratorName))
            {
                decoratorFn = currentEnv_->get(decoratorName, node->line);
            }
            else
            {
                auto bit = builtins_.find(decoratorName);
                if (bit != builtins_.end())
                {
                    // Wrap builtin as a call
                    std::vector<XObject> args = {fn};
                    fn = bit->second(args, node->line);
                    continue;
                }
                throw UndefinedVariableError(decoratorName, node->line);
            }

            if (!decoratorFn.isFunction())
                throw TypeError("decorator '" + decoratorName + "' is not a function", node->line);

            // Call the decorator with the function as argument
            std::vector<XObject> args = {fn};
            fn = callUserFn(decoratorFn.asFunction(), args, node->line);
        }

        // Replace the function in the environment with the decorated version
        currentEnv_->set(fnName, std::move(fn));
    }

    // ---- Generator creation ------------------------------------------------

    XObject Interpreter::createGenerator(const XFunction &fn, std::vector<XObject> &args, int line)
    {
        XGenerator gen;
        gen.fnName = fn.name;

        // Create environment for the generator body
        Environment *closureEnv = fn.closureEnv ? fn.closureEnv : currentEnv_;

        // Capture a shared owned env for the generator's closure
        auto genEnv = std::make_shared<Environment>(closureEnv);

        // Bind parameters
        for (size_t i = 0; i < fn.params.size(); i++)
        {
            if (i < args.size())
                genEnv->define(fn.params[i], std::move(args[i]));
            else if (i < fn.defaults.size() && fn.defaults[i] != nullptr)
                genEnv->define(fn.params[i], eval(fn.defaults[i]));
            else
                genEnv->define(fn.params[i], XObject::makeNone());
        }

        // Bind variadic
        if (fn.isVariadic && !fn.variadicName.empty())
        {
            XList varArgs;
            for (size_t i = fn.params.size(); i < args.size(); i++)
                varArgs.push_back(std::move(args[i]));
            genEnv->define(fn.variadicName, XObject::makeList(std::move(varArgs)));
        }

        // Capture references we need in the worker thread
        auto state = gen.state;
        const auto *body = fn.body;

        // Capture builtins_ and output_ references for the generator's own interpreter
        // We create a new interpreter that shares our builtins
        auto &outerOutput = output_;
        auto &outerBuiltins = builtins_;
        auto &outerShellState = shellState_;

        state->worker = std::thread([state, body, genEnv, &outerOutput, &outerBuiltins, &outerShellState]()
                                    {
            // Wait for first next() call
            {
                std::unique_lock<std::mutex> lk(state->mtx);
                state->cv.wait(lk, [&state] {
                    return state->phase == GeneratorState::RUNNING || state->phase == GeneratorState::DONE;
                });
            }
            if (state->phase == GeneratorState::DONE) return;

            // Create a mini-interpreter for the generator body
            Interpreter genInterp;
            genInterp.output_ = {};  // generator captures output separately
            genInterp.activeGeneratorState_ = state.get();

            // We execute in the generator's env
            auto *savedEnv = genInterp.currentEnv_;
            genInterp.currentEnv_ = genEnv.get();

            try {
                if (body) {
                    for (const auto &stmt : *body) {
                        genInterp.exec(stmt.get());
                    }
                }
            } catch (const GiveSignal &sig) {
                // give from generator = final value (store but mark done)
                std::lock_guard<std::mutex> lk(state->mtx);
                delete state->yieldedValue;
                state->yieldedValue = new XObject(sig.value.clone());
            } catch (...) {
                std::lock_guard<std::mutex> lk(state->mtx);
                state->error = std::current_exception();
            }

            genInterp.currentEnv_ = savedEnv;

            // Signal completion
            {
                std::lock_guard<std::mutex> lk(state->mtx);
                state->phase = GeneratorState::DONE;
            }
            state->cv.notify_all(); });

        state->started = true;
        return XObject::makeGenerator(std::move(gen));
    }

    // ---- Try / Catch / Finally ---------------------------------------------

    void Interpreter::execTryCatch(const TryCatchStmt *node)
    {
        try
        {
            Environment tryEnv(currentEnv_);
            execBlock(node->tryBody, tryEnv);
        }
        catch (const XellError &e)
        {
            if (!node->catchBody.empty())
            {
                Environment catchEnv(currentEnv_);
                // Bind the error to the catch variable as a map
                XMap errMap;
                errMap.set("message", XObject::makeString(e.detail()));
                errMap.set("type", XObject::makeString(e.category()));
                errMap.set("line", XObject::makeInt(e.line()));
                catchEnv.define(node->catchVarName, XObject::makeMap(std::move(errMap)));

                try
                {
                    execBlock(node->catchBody, catchEnv);
                }
                catch (...)
                {
                    // If finally exists, run it before re-throwing
                    if (!node->finallyBody.empty())
                    {
                        Environment finallyEnv(currentEnv_);
                        execBlock(node->finallyBody, finallyEnv);
                    }
                    throw;
                }
            }
        }
        catch (...)
        {
            // Non-XellError exceptions — still run finally
            if (!node->finallyBody.empty())
            {
                Environment finallyEnv(currentEnv_);
                execBlock(node->finallyBody, finallyEnv);
            }
            throw;
        }

        // Always run finally
        if (!node->finallyBody.empty())
        {
            Environment finallyEnv(currentEnv_);
            execBlock(node->finallyBody, finallyEnv);
        }
    }

    // ---- InCase (switch/match) ---------------------------------------------

    void Interpreter::execInCase(const InCaseStmt *node)
    {
        XObject subject = eval(node->subject.get());

        for (const auto &clause : node->clauses)
        {
            for (const auto &val : clause.values)
            {
                XObject matchVal = eval(val.get());
                if (subject.equals(matchVal))
                {
                    Environment clauseEnv(currentEnv_);
                    execBlock(clause.body, clauseEnv);
                    return; // first match wins, exit
                }
            }
        }

        // No match — run else body
        if (!node->elseBody.empty())
        {
            Environment elseEnv(currentEnv_);
            execBlock(node->elseBody, elseEnv);
        }
    }

    // ---- Destructuring assignment: a, b = [1, 2] ---------------------------

    void Interpreter::execDestructuring(const DestructuringAssignment *node)
    {
        XObject value = eval(node->value.get());
        if (!value.isList())
            throw TypeError("destructuring requires a list on the right side, got " +
                                std::string(xtype_name(value.type())),
                            node->line);

        const auto &list = value.asList();
        for (size_t i = 0; i < node->names.size(); i++)
        {
            if (i < list.size())
                currentEnv_->set(node->names[i], list[i]);
            else
                currentEnv_->set(node->names[i], XObject::makeNone());
        }
    }

    // ========================================================================
    // String interpolation:  "hello {expr} world"
    // ========================================================================

    std::string Interpreter::interpolate(const std::string &raw, int line)
    {
        std::string result;
        size_t i = 0;
        while (i < raw.size())
        {
            if (raw[i] == '{')
            {
                // Find matching closing brace (handle nesting)
                size_t depth = 1;
                size_t j = i + 1;
                while (j < raw.size() && depth > 0)
                {
                    if (raw[j] == '{')
                        depth++;
                    if (raw[j] == '}')
                        depth--;
                    j++;
                }
                if (depth != 0)
                {
                    // Unmatched brace — treat as literal
                    result += raw.substr(i);
                    break;
                }
                // Extract expression text between { }
                std::string exprText = raw.substr(i + 1, j - i - 2);

                // Lex → parse → evaluate
                Lexer lexer(exprText);
                auto tokens = lexer.tokenize();
                Parser parser(tokens);
                auto prog = parser.parse();

                if (!prog.statements.empty())
                {
                    if (auto *es = dynamic_cast<ExprStmt *>(prog.statements[0].get()))
                    {
                        result += eval(es->expr.get()).toString();
                    }
                }
                i = j;
            }
            else
            {
                result += raw[i];
                i++;
            }
        }
        return result;
    }

} // namespace xell
