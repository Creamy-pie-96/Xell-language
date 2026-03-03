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
        currentInterpreter_ = this;
        setInstanceHashCallback(&Interpreter::instanceHashCallback);
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

    void Interpreter::loadModule(const std::string &moduleName)
    {
        if (!moduleRegistry_.isBuiltinModule(moduleName))
            throw RuntimeError("Unknown module '" + moduleName + "'", 0);
        const auto &functions = moduleRegistry_.moduleFunctions(moduleName);
        for (const auto &fnName : functions)
        {
            auto it = allBuiltins_.find(fnName);
            if (it != allBuiltins_.end())
                builtins_[fnName] = it->second;
        }
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
        allBuiltins_.clear();

        // Module-aware registration: Tier 1 → builtins_, everything → allBuiltins_
        registerBuiltinsWithModules(builtins_, allBuiltins_, moduleRegistry_,
                                    output_, shellState_);

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

        // Mirror the 5 HOF builtins into allBuiltins_ so they're discoverable
        for (const auto &name : {"map", "filter", "reduce", "any", "all"})
            allBuiltins_[name] = builtins_[name];

        // ---- Override print to support __print__/__str__ magic methods ----
        builtins_["print"] = [this](std::vector<XObject> &args, int line) -> XObject
        {
            std::string lineStr;
            for (size_t i = 0; i < args.size(); i++)
            {
                if (i > 0)
                    lineStr += " ";
                // Check for __print__ or __str__ magic method on instances
                if (args[i].isInstance())
                {
                    XObject result;
                    std::vector<XObject> noArgs;
                    if (callMagicMethod(args[i], "__print__", noArgs, line, result))
                    {
                        lineStr += result.toString();
                        continue;
                    }
                    if (callMagicMethod(args[i], "__str__", noArgs, line, result))
                    {
                        lineStr += result.toString();
                        continue;
                    }
                }
                lineStr += args[i].toString();
            }
            output_.push_back(lineStr);
            return XObject::makeInt(0);
        };
        allBuiltins_["print"] = builtins_["print"];

        // ---- Override len to support __len__ magic method ----
        builtins_["len"] = [this](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("len", 1, (int)args.size(), line);
            auto &obj = args[0];
            // Check for __len__ magic method on instances
            if (obj.isInstance())
            {
                XObject result;
                std::vector<XObject> noArgs;
                if (callMagicMethod(obj, "__len__", noArgs, line, result))
                    return result;
            }
            if (obj.isString())
                return XObject::makeInt((int64_t)obj.asString().size());
            if (obj.isList())
                return XObject::makeInt((int64_t)obj.asList().size());
            if (obj.isTuple())
                return XObject::makeInt((int64_t)obj.asTuple().size());
            if (obj.isSet())
                return XObject::makeInt((int64_t)obj.asSet().size());
            if (obj.isFrozenSet())
                return XObject::makeInt((int64_t)obj.asFrozenSet().size());
            if (obj.isMap())
                return XObject::makeInt((int64_t)obj.asMap().size());
            throw TypeError("len() expects a string, list, tuple, set, frozen_set, or map", line);
        };
        allBuiltins_["len"] = builtins_["len"];

        // ---- Override contains to support __contains__ magic method ----
        builtins_["contains"] = [this](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("contains", 2, (int)args.size(), line);
            auto &collection = args[0];
            auto &element = args[1];
            // Check for __contains__ magic method on instances
            if (collection.isInstance())
            {
                XObject result;
                std::vector<XObject> containsArgs = {element};
                if (callMagicMethod(collection, "__contains__", containsArgs, line, result))
                    return result;
            }
            if (collection.isString() && element.isString())
                return XObject::makeBool(collection.asString().find(element.asString()) != std::string::npos);
            if (collection.isList())
            {
                for (const auto &item : collection.asList())
                    if (item.equals(element))
                        return XObject::makeBool(true);
                return XObject::makeBool(false);
            }
            if (collection.isTuple())
            {
                for (const auto &item : collection.asTuple())
                    if (item.equals(element))
                        return XObject::makeBool(true);
                return XObject::makeBool(false);
            }
            if (collection.isSet())
                return XObject::makeBool(collection.asSet().has(element));
            if (collection.isFrozenSet())
                return XObject::makeBool(collection.asFrozenSet().has(element));
            if (collection.isMap())
                return XObject::makeBool(collection.asMap().has(element));
            throw TypeError("contains() expects a string, list, tuple, set, frozen_set, or map as first argument", line);
        };
        allBuiltins_["contains"] = builtins_["contains"];
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
        {
            if (p->value)
            {
                XObject val = eval(p->value.get());
                throw BreakSignal{std::move(val), true};
            }
            throw BreakSignal{};
        }
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
        if (auto *p = dynamic_cast<const LetStmt *>(stmt))
            return execLet(p);
        if (auto *p = dynamic_cast<const LoopStmt *>(stmt))
            return execLoop(p);
        if (auto *p = dynamic_cast<const DestructuringAssignment *>(stmt))
            return execDestructuring(p);
        if (auto *p = dynamic_cast<const EnumDef *>(stmt))
            return execEnumDef(p);
        if (auto *p = dynamic_cast<const DecoratedFnDef *>(stmt))
            return execDecoratedFnDef(p);
        if (auto *p = dynamic_cast<const DecoratedClassDef *>(stmt))
            return execDecoratedClassDef(p);
        if (auto *p = dynamic_cast<const StructDef *>(stmt))
            return execStructDef(p);
        if (auto *p = dynamic_cast<const ClassDef *>(stmt))
            return execClassDef(p);
        if (auto *p = dynamic_cast<const InterfaceDef *>(stmt))
            return execInterfaceDef(p);
        if (auto *p = dynamic_cast<const ImmutableBinding *>(stmt))
        {
            XObject value = eval(p->value.get());
            currentEnv_->defineImmutable(p->name, std::move(value));
            return;
        }
        if (auto *p = dynamic_cast<const MemberAssignment *>(stmt))
            return execMemberAssignment(p);
        if (auto *p = dynamic_cast<const IndexAssignment *>(stmt))
            return execIndexAssignment(p);
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
        currentEnv_->set(node->name, std::move(value), node->line);
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
        // ---- Evaluate all source iterables ----
        std::vector<XObject> sources;
        for (const auto &iterExpr : node->iterables)
            sources.push_back(eval(iterExpr.get()));

        // ---- Collect iteration items per source ----
        // Convert each source to a vector of XObjects to iterate over.
        // Support lists, tuples, and generators.
        auto toIterable = [&](XObject &src, int line) -> std::vector<XObject>
        {
            if (src.isList())
                return src.asList();
            if (src.isTuple())
            {
                auto &tup = src.asTuple();
                return std::vector<XObject>(tup.begin(), tup.end());
            }
            if (src.isMap())
            {
                // Iterate over map → each element is a [key, value] list
                std::vector<XObject> items;
                for (auto it = src.asMap().begin(); it.valid(); it.next())
                {
                    std::vector<XObject> pair;
                    pair.push_back(it.key().clone());
                    pair.push_back(it.value().clone());
                    items.push_back(XObject::makeList(std::move(pair)));
                }
                return items;
            }
            if (src.isSet())
            {
                // Iterate over set → each element
                return src.asSet().elements();
            }
            if (src.isString())
            {
                // Iterate over string → each character as a string
                std::vector<XObject> chars;
                for (char c : src.asString())
                    chars.push_back(XObject::makeString(std::string(1, c)));
                return chars;
            }
            if (src.isGenerator())
            {
                // Eagerly collect generator values
                std::vector<XObject> items;
                auto &gen = src.asGeneratorMut();
                auto &state = gen.state;
                while (true)
                {
                    std::unique_lock<std::mutex> lock(state->mtx);
                    if (state->phase == GeneratorState::Phase::DONE)
                        break;
                    if (!state->started)
                    {
                        state->started = true;
                        state->phase = GeneratorState::Phase::RUNNING;
                        lock.unlock();
                        state->cv.notify_all(); // wake worker thread
                        lock.lock();
                    }
                    else
                    {
                        state->phase = GeneratorState::Phase::RUNNING;
                        lock.unlock();
                        state->cv.notify_all();
                        lock.lock();
                    }
                    state->cv.wait(lock, [&]
                                   { return state->phase == GeneratorState::Phase::YIELDED ||
                                            state->phase == GeneratorState::Phase::DONE; });
                    if (state->error)
                        std::rethrow_exception(state->error);
                    if (state->phase == GeneratorState::Phase::DONE)
                        break;
                    items.push_back(state->yieldedValue ? state->yieldedValue->clone() : XObject::makeNone());
                }
                return items;
            }
            if (src.isInstance())
            {
                // Check for __iter__ magic method → should return a list
                XObject iterResult;
                std::vector<XObject> iterArgs;
                if (callMagicMethod(src, "__iter__", iterArgs, line, iterResult))
                {
                    if (iterResult.isList())
                        return iterResult.asList();
                    if (iterResult.isTuple())
                    {
                        auto &tup = iterResult.asTuple();
                        return std::vector<XObject>(tup.begin(), tup.end());
                    }
                    throw TypeError("__iter__ must return a list or tuple, got " +
                                        std::string(xtype_name(iterResult.type())),
                                    line);
                }
                throw IterationError("'" + src.asInstance().typeName +
                                         "' is not iterable (no __iter__ method defined)",
                                     line);
            }
            throw TypeError("for..in requires a list, tuple, map, set, string, generator, or iterable instance, got " +
                                std::string(xtype_name(src.type())),
                            line);
        };

        std::vector<std::vector<XObject>> allItems;
        for (size_t i = 0; i < sources.size(); i++)
            allItems.push_back(toIterable(sources[i], node->line));

        // ---- Determine iteration count (zip semantics: shortest) ----
        size_t iterCount = 0;
        if (!allItems.empty())
        {
            iterCount = allItems[0].size();
            for (size_t i = 1; i < allItems.size(); i++)
                iterCount = std::min(iterCount, allItems[i].size());
        }

        const size_t numTargets = node->varNames.size();
        const size_t numSources = allItems.size();

        Environment loopEnv(currentEnv_);
        auto *savedEnv = currentEnv_;
        currentEnv_ = &loopEnv;

        try
        {
            for (size_t i = 0; i < iterCount; i++)
            {
                if (numSources == 1 && numTargets == 1 && !node->hasRest)
                {
                    // ---- Simple case: for x in list ----
                    loopEnv.define(node->varNames[0], allItems[0][i]);
                }
                else if (numSources > 1)
                {
                    // ---- Parallel iteration: for a, b in list1, list2 ----
                    // Each source provides one value per iteration
                    for (size_t t = 0; t < numTargets && t < numSources; t++)
                        loopEnv.define(node->varNames[t], allItems[t][i]);
                    // Rest capture gets remaining sources
                    if (node->hasRest && numSources > numTargets)
                    {
                        std::vector<XObject> rest;
                        for (size_t t = numTargets; t < numSources; t++)
                            rest.push_back(allItems[t][i]);
                        loopEnv.define(node->restName, XObject::makeList(std::move(rest)));
                    }
                    else if (node->hasRest)
                    {
                        loopEnv.define(node->restName, XObject::makeList({}));
                    }
                }
                else
                {
                    // ---- Single source, multiple targets: destructuring ----
                    // Each element of the source is destructured into the target variables
                    const XObject &elem = allItems[0][i];
                    std::vector<XObject> inner;
                    if (elem.isList())
                        inner = elem.asList();
                    else if (elem.isTuple())
                    {
                        auto &tup = elem.asTuple();
                        inner = std::vector<XObject>(tup.begin(), tup.end());
                    }
                    else
                    {
                        throw TypeError("Cannot destructure " + std::string(xtype_name(elem.type())) +
                                            " in for loop; expected list or tuple",
                                        node->line);
                    }

                    // Assign named targets
                    for (size_t t = 0; t < numTargets && t < inner.size(); t++)
                        loopEnv.define(node->varNames[t], inner[t]);
                    // Fill missing targets with none
                    for (size_t t = inner.size(); t < numTargets; t++)
                        loopEnv.define(node->varNames[t], XObject::makeNone());

                    // Rest capture
                    if (node->hasRest)
                    {
                        std::vector<XObject> rest;
                        for (size_t t = numTargets; t < inner.size(); t++)
                            rest.push_back(inner[t]);
                        loopEnv.define(node->restName, XObject::makeList(std::move(rest)));
                    }
                }

                try
                {
                    for (const auto &stmt : node->body)
                    {
                        exec(stmt.get());
                    }
                }
                catch (const BreakSignal &bs)
                {
                    if (bs.hasValue)
                        throw RuntimeError("Cannot use 'break VALUE' in a statement-mode for loop; "
                                           "use expression-mode (x = for ...) to capture values",
                                           node->line);
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
                catch (const BreakSignal &bs)
                {
                    if (bs.hasValue)
                        throw RuntimeError("Cannot use 'break VALUE' in a statement-mode while loop; "
                                           "use expression-mode (x = while ...) to capture values",
                                           node->line);
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

    // ============================================================
    // Infinite loop statement:  loop : BLOCK ;
    // ============================================================

    // Helper: recursively check if a block of statements contains a BreakStmt.
    // Does NOT descend into nested function definitions (breaks there don't affect this loop).
    static bool bodyContainsBreak(const std::vector<StmtPtr> &stmts)
    {
        for (const auto &s : stmts)
        {
            if (dynamic_cast<const BreakStmt *>(s.get()))
                return true;
            if (auto *p = dynamic_cast<const IfStmt *>(s.get()))
            {
                if (bodyContainsBreak(p->body))
                    return true;
                for (const auto &elif : p->elifs)
                    if (bodyContainsBreak(elif.body))
                        return true;
                if (bodyContainsBreak(p->elseBody))
                    return true;
            }
            if (auto *p = dynamic_cast<const ForStmt *>(s.get()))
                if (bodyContainsBreak(p->body))
                    return true;
            if (auto *p = dynamic_cast<const WhileStmt *>(s.get()))
                if (bodyContainsBreak(p->body))
                    return true;
            if (auto *p = dynamic_cast<const LoopStmt *>(s.get()))
                if (bodyContainsBreak(p->body))
                    return true;
            if (auto *p = dynamic_cast<const TryCatchStmt *>(s.get()))
            {
                if (bodyContainsBreak(p->tryBody))
                    return true;
                if (bodyContainsBreak(p->catchBody))
                    return true;
                if (bodyContainsBreak(p->finallyBody))
                    return true;
            }
            if (auto *p = dynamic_cast<const InCaseStmt *>(s.get()))
            {
                for (const auto &clause : p->clauses)
                    if (bodyContainsBreak(clause.body))
                        return true;
                if (bodyContainsBreak(p->elseBody))
                    return true;
            }
            if (auto *p = dynamic_cast<const LetStmt *>(s.get()))
                if (bodyContainsBreak(p->body))
                    return true;
            // Don't descend into FnDef, ClassDef — breaks in those are local
        }
        return false;
    }

    void Interpreter::execLoop(const LoopStmt *node)
    {
        // @safe_loop: check at runtime that the body contains a break
        if (node->safeLoop && !bodyContainsBreak(node->body))
        {
            throw RuntimeError("@safe_loop: loop body does not contain a 'break' statement; "
                               "this would cause an infinite loop",
                               node->line);
        }

        Environment loopEnv(currentEnv_);
        auto *savedEnv = currentEnv_;
        currentEnv_ = &loopEnv;

        try
        {
            while (true)
            {
                try
                {
                    for (const auto &stmt : node->body)
                    {
                        exec(stmt.get());
                    }
                }
                catch (const BreakSignal &bs)
                {
                    if (bs.hasValue)
                        throw RuntimeError("Cannot use 'break VALUE' in a statement-mode loop; "
                                           "use expression-mode (x = loop: ...) to capture values",
                                           node->line);
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

    // ============================================================
    // Expression-mode if: if cond: val elif cond: val else: val
    // ============================================================

    XObject Interpreter::evalIfExpr(const IfExpr *node)
    {
        for (const auto &branch : node->branches)
        {
            if (!branch.condition)
            {
                // else branch — always taken
                return eval(branch.value.get());
            }
            if (eval(branch.condition.get()).truthy())
            {
                return eval(branch.value.get());
            }
        }
        // Should not reach here if parser enforced else branch
        return XObject::makeNone();
    }

    // ============================================================
    // Expression-mode for: x = for i in list: BLOCK give DEFAULT ;
    // ============================================================

    XObject Interpreter::evalForExpr(const ForExpr *node)
    {
        // Reuse the iterable conversion logic from execFor
        std::vector<XObject> sources;
        for (const auto &iterExpr : node->iterables)
            sources.push_back(eval(iterExpr.get()));

        auto toIterable = [&](XObject &src, int line) -> std::vector<XObject>
        {
            if (src.isList())
                return src.asList();
            if (src.isTuple())
            {
                auto &tup = src.asTuple();
                return std::vector<XObject>(tup.begin(), tup.end());
            }
            if (src.isMap())
            {
                std::vector<XObject> items;
                for (auto it = src.asMap().begin(); it.valid(); it.next())
                {
                    std::vector<XObject> pair;
                    pair.push_back(it.key().clone());
                    pair.push_back(it.value().clone());
                    items.push_back(XObject::makeList(std::move(pair)));
                }
                return items;
            }
            if (src.isSet())
                return src.asSet().elements();
            if (src.isString())
            {
                std::vector<XObject> chars;
                for (char c : src.asString())
                    chars.push_back(XObject::makeString(std::string(1, c)));
                return chars;
            }
            if (src.isInstance())
            {
                XObject iterResult;
                std::vector<XObject> iterArgs;
                if (callMagicMethod(src, "__iter__", iterArgs, line, iterResult))
                {
                    if (iterResult.isList())
                        return iterResult.asList();
                    if (iterResult.isTuple())
                    {
                        auto &tup = iterResult.asTuple();
                        return std::vector<XObject>(tup.begin(), tup.end());
                    }
                    throw TypeError("__iter__ must return a list or tuple", line);
                }
                throw IterationError("Object is not iterable (no __iter__ method)", line);
            }
            throw TypeError("for expression requires an iterable, got " +
                                std::string(xtype_name(src.type())),
                            line);
        };

        std::vector<std::vector<XObject>> allItems;
        for (size_t i = 0; i < sources.size(); i++)
            allItems.push_back(toIterable(sources[i], node->line));

        size_t iterCount = 0;
        if (!allItems.empty())
        {
            iterCount = allItems[0].size();
            for (size_t i = 1; i < allItems.size(); i++)
                iterCount = std::min(iterCount, allItems[i].size());
        }

        const size_t numTargets = node->varNames.size();
        const size_t numSources = allItems.size();

        Environment loopEnv(currentEnv_);
        auto *savedEnv = currentEnv_;
        currentEnv_ = &loopEnv;

        XObject result = XObject::makeNone();
        bool gotValue = false;

        try
        {
            for (size_t i = 0; i < iterCount; i++)
            {
                // Bind loop variables (same logic as execFor)
                if (numSources == 1 && numTargets == 1 && !node->hasRest)
                {
                    loopEnv.define(node->varNames[0], allItems[0][i]);
                }
                else if (numSources > 1)
                {
                    for (size_t t = 0; t < numTargets && t < numSources; t++)
                        loopEnv.define(node->varNames[t], allItems[t][i]);
                    if (node->hasRest && numSources > numTargets)
                    {
                        std::vector<XObject> rest;
                        for (size_t t = numTargets; t < numSources; t++)
                            rest.push_back(allItems[t][i]);
                        loopEnv.define(node->restName, XObject::makeList(std::move(rest)));
                    }
                    else if (node->hasRest)
                        loopEnv.define(node->restName, XObject::makeList({}));
                }
                else
                {
                    const XObject &elem = allItems[0][i];
                    std::vector<XObject> inner;
                    if (elem.isList())
                        inner = elem.asList();
                    else if (elem.isTuple())
                    {
                        auto &tup = elem.asTuple();
                        inner = std::vector<XObject>(tup.begin(), tup.end());
                    }
                    else
                        throw TypeError("Cannot destructure in for expression", node->line);

                    for (size_t t = 0; t < numTargets && t < inner.size(); t++)
                        loopEnv.define(node->varNames[t], inner[t]);
                    for (size_t t = inner.size(); t < numTargets; t++)
                        loopEnv.define(node->varNames[t], XObject::makeNone());
                    if (node->hasRest)
                    {
                        std::vector<XObject> rest;
                        for (size_t t = numTargets; t < inner.size(); t++)
                            rest.push_back(inner[t]);
                        loopEnv.define(node->restName, XObject::makeList(std::move(rest)));
                    }
                }

                try
                {
                    for (const auto &stmt : node->body)
                        exec(stmt.get());
                }
                catch (const BreakSignal &bs)
                {
                    if (bs.hasValue)
                    {
                        result = std::move(const_cast<BreakSignal &>(bs).value);
                        gotValue = true;
                    }
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

        if (gotValue)
            return result;
        if (node->defaultValue)
            return eval(node->defaultValue.get());
        return XObject::makeNone();
    }

    // ============================================================
    // Expression-mode while: x = while cond: BLOCK give DEFAULT ;
    // ============================================================

    XObject Interpreter::evalWhileExpr(const WhileExpr *node)
    {
        Environment loopEnv(currentEnv_);
        auto *savedEnv = currentEnv_;
        currentEnv_ = &loopEnv;

        XObject result = XObject::makeNone();
        bool gotValue = false;

        try
        {
            while (eval(node->condition.get()).truthy())
            {
                try
                {
                    for (const auto &stmt : node->body)
                        exec(stmt.get());
                }
                catch (const BreakSignal &bs)
                {
                    if (bs.hasValue)
                    {
                        result = std::move(const_cast<BreakSignal &>(bs).value);
                        gotValue = true;
                    }
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

        if (gotValue)
            return result;
        if (node->defaultValue)
            return eval(node->defaultValue.get());
        return XObject::makeNone();
    }

    // ============================================================
    // Expression-mode loop: x = loop: BLOCK give DEFAULT ;
    // ============================================================

    XObject Interpreter::evalLoopExpr(const LoopExpr *node)
    {
        Environment loopEnv(currentEnv_);
        auto *savedEnv = currentEnv_;
        currentEnv_ = &loopEnv;

        XObject result = XObject::makeNone();
        bool gotValue = false;

        try
        {
            while (true)
            {
                try
                {
                    for (const auto &stmt : node->body)
                        exec(stmt.get());
                }
                catch (const BreakSignal &bs)
                {
                    if (bs.hasValue)
                    {
                        result = std::move(const_cast<BreakSignal &>(bs).value);
                        gotValue = true;
                    }
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

        if (gotValue)
            return result;
        if (node->defaultValue)
            return eval(node->defaultValue.get());
        return XObject::makeNone();
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
        fnRef.isGenerator = containsYield(node->body);

        // Async flag
        fnRef.isAsync = node->isAsync;

        // Store type annotations for overload resolution
        fnRef.typeAnnotations = node->paramTypes;

        // ---- Overload detection ----
        // Check if a function with this name already exists — if so, build overload set
        if (currentEnv_->has(node->name))
        {
            try
            {
                XObject existing = currentEnv_->get(node->name, node->line);
                if (existing.isFunction())
                {
                    const XFunction &existingFn = existing.asFunction();

                    // Check if the new function has type annotations
                    bool newHasTypes = false;
                    for (const auto &t : fnRef.typeAnnotations)
                        if (!t.empty())
                        {
                            newHasTypes = true;
                            break;
                        }

                    bool existingHasTypes = false;
                    for (const auto &t : existingFn.typeAnnotations)
                        if (!t.empty())
                        {
                            existingHasTypes = true;
                            break;
                        }

                    size_t newArity = fnRef.params.size();
                    size_t existingArity = existingFn.params.size();

                    // Same arity, both dynamic (no types) → just overwrite (redefine)
                    if (newArity == existingArity && !newHasTypes && !existingHasTypes)
                    {
                        // Normal redefinition — no overloading
                    }
                    else
                    {
                        // Validation: can't add typed overload if dynamic exists at same arity
                        auto checkConflict = [&](const XFunction &dynFn, const XFunction &typedFn)
                        {
                            bool dynHasT = false;
                            for (const auto &t : dynFn.typeAnnotations)
                                if (!t.empty())
                                {
                                    dynHasT = true;
                                    break;
                                }
                            bool typedHasT = false;
                            for (const auto &t : typedFn.typeAnnotations)
                                if (!t.empty())
                                {
                                    typedHasT = true;
                                    break;
                                }

                            if (dynFn.params.size() == typedFn.params.size() &&
                                !dynHasT && typedHasT)
                            {
                                throw ParseError("Cannot add type-specific overload for '" + node->name +
                                                     "' — a dynamic overload with " + std::to_string(dynFn.params.size()) +
                                                     " param(s) already exists",
                                                 node->line);
                            }
                        };

                        // Check existing function against new
                        checkConflict(existingFn, fnRef);
                        checkConflict(fnRef, existingFn);

                        // Also check existing overloads
                        for (const auto &ovl : existingFn.overloads)
                        {
                            if (ovl.isFunction())
                            {
                                checkConflict(ovl.asFunction(), fnRef);
                                checkConflict(fnRef, ovl.asFunction());
                            }
                        }

                        // Build overload set: collect existing overloads + existing fn
                        for (const auto &ovl : existingFn.overloads)
                            fnRef.overloads.push_back(ovl);
                        fnRef.overloads.push_back(existing);
                    }
                }
            }
            catch (const UndefinedVariableError &)
            {
                // Variable exists but not accessible — proceed normally
            }
        }

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
        std::string rawPath = node->path;

        // ── Check if the path refers to a built-in module ──────────────
        if (moduleRegistry_.isBuiltinModule(rawPath))
        {
            const auto &functions = moduleRegistry_.moduleFunctions(rawPath);

            if (node->bringAll)
            {
                // bring * from "module" — inject all module functions
                for (const auto &fnName : functions)
                {
                    auto it = allBuiltins_.find(fnName);
                    if (it != allBuiltins_.end())
                        builtins_[fnName] = it->second;
                }
            }
            else
            {
                // bring name1, name2 from "module" [as alias1, alias2]
                for (size_t i = 0; i < node->names.size(); ++i)
                {
                    const std::string &name = node->names[i];
                    std::string alias = (i < node->aliases.size() && !node->aliases[i].empty())
                                            ? node->aliases[i]
                                            : name;

                    auto it = allBuiltins_.find(name);
                    if (it == allBuiltins_.end() || !moduleRegistry_.moduleHasFunction(rawPath, name))
                        throw BringError("Name '" + name + "' not found in module '" + rawPath + "'", node->line);
                    builtins_[alias] = it->second;
                }
            }
            return; // done — no file I/O needed
        }

        // ── File-based bring (user .xel modules / 3rd party) ──────────

        // 1. Resolve the file path relative to the current source file
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

        // Expression-mode constructs
        if (auto *p = dynamic_cast<const IfExpr *>(expr))
            return evalIfExpr(p);
        if (auto *p = dynamic_cast<const ForExpr *>(expr))
            return evalForExpr(p);
        if (auto *p = dynamic_cast<const WhileExpr *>(expr))
            return evalWhileExpr(p);
        if (auto *p = dynamic_cast<const LoopExpr *>(expr))
            return evalLoopExpr(p);

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

        // ================================================================
        // Magic method dispatch for operator overloading on instances
        // Check if the left operand is an instance with the corresponding
        // __op__ method before falling through to default behavior.
        // ================================================================
        if (left.isInstance())
        {
            static const std::unordered_map<std::string, std::string> opToMagic = {
                {"+", "__add__"},
                {"-", "__sub__"},
                {"*", "__mul__"},
                {"/", "__div__"},
                {"%", "__mod__"},
                {"==", "__eq__"},
                {"!=", "__ne__"},
                {"<", "__lt__"},
                {">", "__gt__"},
                {"<=", "__le__"},
                {">=", "__ge__"},
            };
            auto it = opToMagic.find(op);
            if (it != opToMagic.end())
            {
                XObject result;
                std::vector<XObject> magicArgs = {std::move(right)};
                if (callMagicMethod(left, it->second, magicArgs, node->line, result))
                    return result;
                // If __ne__ not defined but __eq__ is, derive != from ==
                if (op == "!=")
                {
                    std::vector<XObject> eqArgs = {std::move(magicArgs[0])};
                    if (callMagicMethod(left, "__eq__", eqArgs, node->line, result))
                        return XObject::makeBool(!result.truthy());
                    // Restore right from eqArgs (magicArgs[0] was moved into eqArgs)
                    right = std::move(eqArgs[0]);
                }
                else
                {
                    // Restore right for fallback
                    right = std::move(magicArgs[0]);
                }
            }
        }

        // Equality / inequality (any types)
        if (op == "==")
            return XObject::makeBool(left.equals(right));
        if (op == "!=")
            return XObject::makeBool(!left.equals(right));

        // Instance-of check: obj is ClassName
        if (op == "is")
        {
            if (left.isInstance() && right.isStructDef())
            {
                const XInstance &inst = left.asInstance();
                const XStructDef &def = right.asStructDef();
                return XObject::makeBool(inst.structDef->isOrInherits(def.name));
            }
            // Fallback: treat as equality for non-instance operands
            return XObject::makeBool(left.equals(right));
        }

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
            // Magic method: __neg__
            if (val.isInstance())
            {
                XObject result;
                std::vector<XObject> noArgs;
                if (callMagicMethod(val, "__neg__", noArgs, node->line, result))
                    return result;
            }
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

    // ---- Overload resolution helpers -----------------------------------------

    static bool matchesType(const XObject &obj, const std::string &typeName)
    {
        if (typeName.empty())
            return true; // no annotation = accepts anything
        if (typeName == "str" || typeName == "string" || typeName == "String")
            return obj.isString();
        if (typeName == "int" || typeName == "Int")
            return obj.isInt();
        if (typeName == "float" || typeName == "Float")
            return obj.isFloat();
        if (typeName == "bool")
            return obj.isBool();
        if (typeName == "list" || typeName == "List")
            return obj.isList();
        if (typeName == "map" || typeName == "Map")
            return obj.isMap();
        if (typeName == "fn" || typeName == "func")
            return obj.isFunction();
        if (typeName == "set" || typeName == "Set")
            return obj.isSet();
        if (typeName == "tuple" || typeName == "Tuple")
            return obj.isTuple();
        if (typeName == "complex" || typeName == "Complex")
            return obj.isComplex();
        if (typeName == "num" || typeName == "number")
            return obj.isNumber();
        if (typeName == "none")
            return obj.isNone();
        if (typeName == "bytes")
            return obj.isBytes();
        if (typeName == "frozen_set" || typeName == "iset" || typeName == "iSet")
            return obj.isFrozenSet();
        return false; // unknown type name
    }

    static bool fnHasTypeAnnotations(const XFunction &fn)
    {
        for (const auto &t : fn.typeAnnotations)
            if (!t.empty())
                return true;
        return false;
    }

    static bool typesMatchFn(const XFunction &fn, const std::vector<XObject> &args)
    {
        for (size_t i = 0; i < fn.params.size() && i < args.size(); i++)
        {
            if (i < fn.typeAnnotations.size() && !fn.typeAnnotations[i].empty())
            {
                if (!matchesType(args[i], fn.typeAnnotations[i]))
                    return false;
            }
        }
        return true;
    }

    static size_t countRequiredParams(const XFunction &fn)
    {
        size_t minReq = 0;
        for (size_t i = 0; i < fn.params.size(); i++)
        {
            if (i >= fn.defaults.size() || fn.defaults[i] == nullptr)
                minReq = i + 1;
        }
        return minReq;
    }

    static bool arityMatches(const XFunction &fn, size_t argCount)
    {
        size_t minReq = countRequiredParams(fn);
        size_t maxReq = fn.isVariadic ? SIZE_MAX : fn.params.size();
        return argCount >= minReq && argCount <= maxReq;
    }

    // ---- Function calls ----------------------------------------------------

    XObject Interpreter::evalCall(const CallExpr *node)
    {
        // Evaluate arguments, handling spread and named (keyword) args
        std::vector<XObject> args;
        std::vector<std::pair<std::string, XObject>> namedArgs;
        bool hasNamedArgs = false;
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
            else if (auto *named = dynamic_cast<const NamedArgExpr *>(arg.get()))
            {
                // Collect named args separately for keyword argument matching
                namedArgs.emplace_back(named->name, eval(named->value.get()));
                hasNamedArgs = true;
            }
            else
            {
                if (hasNamedArgs)
                    throw TypeError("positional argument follows keyword argument", node->line);
                args.push_back(eval(arg.get()));
            }
        }
        const std::vector<std::pair<std::string, XObject>> *namedArgsPtr =
            hasNamedArgs ? &namedArgs : nullptr;

        // Check builtins first, BUT user-defined functions take precedence
        // (allows shadowing builtins, like Python)
        if (currentEnv_->has(node->callee))
        {
            XObject fnObj = currentEnv_->get(node->callee, node->line);
            if (fnObj.isFunction())
            {
                const XFunction &fn = fnObj.asFunction();
                // If this function has overloads, resolve the best match
                if (!fn.overloads.empty())
                {
                    // Collect all candidates
                    std::vector<const XFunction *> candidates;
                    candidates.push_back(&fn);
                    for (const auto &ovl : fn.overloads)
                    {
                        if (ovl.isFunction())
                            candidates.push_back(&ovl.asFunction());
                    }

                    // Resolution: exact type match first, then arity-only (dynamic)
                    const XFunction *typeMatch = nullptr;
                    const XFunction *arityMatch = nullptr;

                    for (const auto *cand : candidates)
                    {
                        if (!arityMatches(*cand, args.size()))
                            continue;

                        if (fnHasTypeAnnotations(*cand))
                        {
                            if (typesMatchFn(*cand, args))
                            {
                                typeMatch = cand;
                                break; // first exact type match wins
                            }
                        }
                        else
                        {
                            if (!arityMatch)
                                arityMatch = cand; // first arity match (dynamic)
                        }
                    }

                    if (typeMatch)
                        return callUserFn(*typeMatch, args, node->line, nullptr, namedArgsPtr);
                    if (arityMatch)
                        return callUserFn(*arityMatch, args, node->line, nullptr, namedArgsPtr);

                    // No match found
                    throw TypeError("no matching overload for '" + node->callee +
                                        "' with " + std::to_string(args.size()) + " argument(s)",
                                    node->line);
                }
                return callUserFn(fn, args, node->line, nullptr, namedArgsPtr);
            }

            // ---- Struct/Class construction: Name(args...) ----
            if (fnObj.isStructDef())
            {
                const XStructDef &def = fnObj.asStructDef();

                // Interfaces cannot be instantiated
                if (def.isInterface)
                    throw TypeError("cannot instantiate interface '" + def.name + "'", node->line);

                // Abstract classes cannot be instantiated directly
                if (def.isAbstract)
                    throw TypeError("cannot instantiate abstract class '" + def.name + "'", node->line);

                // Mixins cannot be instantiated
                if (def.isMixin)
                    throw TypeError("cannot instantiate mixin '" + def.name + "'", node->line);

                // Singleton: return cached instance if it already exists
                if (def.isSingleton && !def.singletonInstance.isNone())
                    return def.singletonInstance;

                auto defPtr = fnObj.asStructDefShared();
                XInstance inst(def.name, defPtr);

                if (def.isClass)
                {
                    // Class construction: initialize ALL inherited fields to defaults
                    auto allFields = def.allFields();
                    for (const auto &fi : allFields)
                        inst.fields[fi.name] = fi.defaultValue.clone();

                    // Look for __init__ method (own or inherited)
                    auto [initMethodInfo, initOwner] = def.findMethodWithOwner("__init__");
                    if (initMethodInfo && initMethodInfo->fnObject.isFunction())
                    {
                        // Call __init__(self, args...)
                        // Create the instance object — ref-counted so copies share data
                        XObject instObj = XObject::makeInstance(std::move(inst));
                        // Keep a copy — callUserFn will move from initArgs[0]
                        XObject result = instObj;
                        std::vector<XObject> initArgs;
                        initArgs.push_back(std::move(instObj));
                        for (auto &a : args)
                            initArgs.push_back(std::move(a));
                        // Pass correct parent for the class that defines __init__
                        std::shared_ptr<XStructDef> parentDef = nullptr;
                        if (initOwner && initOwner->isClass && !initOwner->parents.empty())
                            parentDef = initOwner->parents[0];
                        auto *savedMethodClass = executingMethodClass_;
                        executingMethodClass_ = initOwner;
                        callUserFn(initMethodInfo->fnObject.asFunction(), initArgs, node->line, parentDef, namedArgsPtr);
                        executingMethodClass_ = savedMethodClass;
                        // @immutable: freeze instance after __init__ completes
                        if (def.isImmutable)
                            result.asInstanceMut().frozen = true;
                        // @singleton: cache the instance
                        if (def.isSingleton)
                            defPtr->singletonInstance = result;
                        // Return the instance — `result` shares the same XData,
                        // so mutations by __init__ are visible
                        return result;
                    }
                    else
                    {
                        // No __init__: fall through to struct-style positional/named construction
                        // Merge named arg values into args for raw AST field matching
                        if (hasNamedArgs)
                        {
                            for (auto &[n, v] : namedArgs)
                                args.push_back(std::move(v));
                        }
                        // Check for named arguments
                        bool hasNamed = false;
                        for (const auto &rawArg : node->args)
                            if (dynamic_cast<const NamedArgExpr *>(rawArg.get()))
                            {
                                hasNamed = true;
                                break;
                            }

                        if (hasNamed)
                        {
                            size_t ai = 0;
                            for (const auto &rawArg : node->args)
                            {
                                auto *na = dynamic_cast<const NamedArgExpr *>(rawArg.get());
                                if (!na)
                                    throw ParseError("cannot mix positional and named arguments in class construction", node->line);
                                bool found = false;
                                for (const auto &fi : allFields)
                                    if (fi.name == na->name)
                                    {
                                        found = true;
                                        break;
                                    }
                                if (!found)
                                    throw AttributeError("'" + def.name + "' has no field '" + na->name + "'", node->line);
                                inst.fields[na->name] = std::move(args[ai++]);
                            }
                        }
                        else
                        {
                            if (args.size() > allFields.size())
                                throw ArityError(def.name, (int)allFields.size(), (int)args.size(), node->line);
                            for (size_t i = 0; i < args.size(); i++)
                                inst.fields[allFields[i].name] = std::move(args[i]);
                        }
                        // @immutable: freeze instance after construction
                        if (def.isImmutable)
                            inst.frozen = true;
                        auto resultObj = XObject::makeInstance(std::move(inst));
                        // @singleton: cache the instance
                        if (def.isSingleton)
                            defPtr->singletonInstance = resultObj;
                        return resultObj;
                    }
                }
                else
                {
                    // Struct construction (no __init__): Initialize all fields to their defaults
                    for (const auto &fi : def.fields)
                        inst.fields[fi.name] = fi.defaultValue.clone();

                    // Merge named arg values into args for raw AST field matching
                    if (hasNamedArgs)
                    {
                        for (auto &[n, v] : namedArgs)
                            args.push_back(std::move(v));
                    }

                    // Check for named arguments (NamedArgExpr) from the raw AST
                    bool hasNamed = false;
                    for (const auto &rawArg : node->args)
                    {
                        if (dynamic_cast<const NamedArgExpr *>(rawArg.get()))
                        {
                            hasNamed = true;
                            break;
                        }
                    }

                    if (hasNamed)
                    {
                        size_t ai = 0;
                        for (const auto &rawArg : node->args)
                        {
                            auto *na = dynamic_cast<const NamedArgExpr *>(rawArg.get());
                            if (!na)
                                throw ParseError("cannot mix positional and named arguments in struct construction", node->line);
                            bool found = false;
                            for (const auto &fi : def.fields)
                                if (fi.name == na->name)
                                {
                                    found = true;
                                    break;
                                }
                            if (!found)
                                throw AttributeError("'" + def.name + "' has no field '" + na->name + "'", node->line);
                            inst.fields[na->name] = std::move(args[ai++]);
                        }
                    }
                    else
                    {
                        if (args.size() > def.fields.size())
                            throw ArityError(def.name, (int)def.fields.size(), (int)args.size(), node->line);
                        for (size_t i = 0; i < args.size(); i++)
                            inst.fields[def.fields[i].name] = std::move(args[i]);
                    }

                    return XObject::makeInstance(std::move(inst));
                }
            }
        }

        // ---- Instance method call: obj->method(args) ----
        // The parser rewrites obj->method(a, b) as CallExpr("method", [obj, a, b])
        // So if the first arg is an instance, check its struct for the method.
        // This must be checked BEFORE builtins so struct methods shadow builtin names.
        // Uses findMethodWithOwner() to get the defining class, enabling correct
        // parent resolution in deep inheritance hierarchies.
        if (!args.empty() && args[0].isInstance())
        {
            const XInstance &inst = args[0].asInstance();
            auto [mi, ownerClass] = inst.structDef->findMethodWithOwner(node->callee);
            if (mi && mi->fnObject.isFunction())
            {
                // Check access level for method call
                if (ownerClass)
                    checkAccess(mi->access, node->callee, *ownerClass, node->line);
                // Pass the first parent of the DEFINING class as parentClassDef
                std::shared_ptr<XStructDef> parentDef = nullptr;
                if (ownerClass && ownerClass->isClass && !ownerClass->parents.empty())
                    parentDef = ownerClass->parents[0];
                // Track which class defines this method for access control
                auto *savedMethodClass = executingMethodClass_;
                executingMethodClass_ = ownerClass;
                auto result = callUserFn(mi->fnObject.asFunction(), args, node->line, parentDef);
                executingMethodClass_ = savedMethodClass;
                return result;
            }
        }

        // ---- Parent method call: parent->method(args) ----
        // Inside a class method, `parent` is the parent class definition.
        // parent->method(args) should call the parent's method with `self` injected.
        // The parser rewrites parent->method(a, b) as CallExpr("method", [parent, a, b])
        // We detect that the first arg is a STRUCT_DEF (class), look up the method,
        // then replace the first arg with `self` from the current scope.
        // We use findMethodWithOwner to correctly set `parent` for the called method.
        if (!args.empty() && args[0].isStructDef())
        {
            const XStructDef &calledParentDef = args[0].asStructDef();
            if (calledParentDef.isClass)
            {
                // Check for static method call first: ClassName->static_method(args)
                // Static methods don't take `self`, so strip the class def from args
                const XStructMethodInfo *smi = calledParentDef.findStaticMethod(node->callee);
                if (smi && smi->fnObject.isFunction())
                {
                    // Remove the class def from args — static methods have no self
                    args.erase(args.begin());
                    return callUserFn(smi->fnObject.asFunction(), args, node->line);
                }

                auto [mi, ownerClass] = calledParentDef.findMethodWithOwner(node->callee);
                if (mi && mi->fnObject.isFunction())
                {
                    // Inject `self` from current scope instead of the class def
                    if (currentEnv_->has("self"))
                    {
                        args[0] = currentEnv_->get("self", node->line);
                        // Pass the owner class's first parent for correct parent chain
                        std::shared_ptr<XStructDef> nextParent = nullptr;
                        if (ownerClass && ownerClass->isClass && !ownerClass->parents.empty())
                            nextParent = ownerClass->parents[0];
                        auto *savedMethodClass = executingMethodClass_;
                        executingMethodClass_ = ownerClass;
                        auto result = callUserFn(mi->fnObject.asFunction(), args, node->line, nextParent);
                        executingMethodClass_ = savedMethodClass;
                        return result;
                    }
                }
            }
        }

        auto bit = builtins_.find(node->callee);
        if (bit != builtins_.end())
        {
            return bit->second(args, node->line);
        }

        // ---- Frozen struct/class construction: ~Name(args...) ----
        if (node->callee.size() > 1 && node->callee[0] == '~')
        {
            std::string baseName = node->callee.substr(1);
            if (currentEnv_->has(baseName))
            {
                XObject fnObj = currentEnv_->get(baseName, node->line);
                if (fnObj.isStructDef())
                {
                    const XStructDef &def = fnObj.asStructDef();

                    // Singleton: return cached instance if it already exists
                    if (def.isSingleton && !def.singletonInstance.isNone())
                        return def.singletonInstance;

                    auto defPtr = fnObj.asStructDefShared();
                    XInstance inst(def.name, defPtr);
                    inst.frozen = true;

                    if (def.isClass)
                    {
                        // Class frozen construction: use allFields, support __init__
                        auto allFields = def.allFields();
                        for (const auto &fi : allFields)
                            inst.fields[fi.name] = fi.defaultValue.clone();

                        auto [initMethodInfo, initOwner] = def.findMethodWithOwner("__init__");
                        if (initMethodInfo && initMethodInfo->fnObject.isFunction())
                        {
                            // Call __init__ — but freeze AFTER init completes
                            inst.frozen = false;
                            XObject instObj = XObject::makeInstance(std::move(inst));
                            // Keep a copy — callUserFn will move from initArgs[0]
                            XObject result = instObj;
                            std::vector<XObject> initArgs;
                            initArgs.push_back(std::move(instObj));
                            for (auto &a : args)
                                initArgs.push_back(std::move(a));
                            std::shared_ptr<XStructDef> parentDef = nullptr;
                            if (initOwner && initOwner->isClass && !initOwner->parents.empty())
                                parentDef = initOwner->parents[0];
                            auto *savedMethodClass = executingMethodClass_;
                            executingMethodClass_ = initOwner;
                            callUserFn(initMethodInfo->fnObject.asFunction(), initArgs, node->line, parentDef, namedArgsPtr);
                            executingMethodClass_ = savedMethodClass;
                            // Freeze after __init__ — result shares same XData
                            result.asInstanceMut().frozen = true;
                            return result;
                        }
                        else
                        {
                            // No __init__: struct-style positional/named
                            // Merge named arg values into args for raw AST field matching
                            if (hasNamedArgs)
                            {
                                for (auto &[n, v] : namedArgs)
                                    args.push_back(std::move(v));
                            }
                            bool hasNamed = false;
                            for (const auto &rawArg : node->args)
                                if (dynamic_cast<const NamedArgExpr *>(rawArg.get()))
                                {
                                    hasNamed = true;
                                    break;
                                }
                            if (hasNamed)
                            {
                                size_t ai = 0;
                                for (const auto &rawArg : node->args)
                                {
                                    auto *na = dynamic_cast<const NamedArgExpr *>(rawArg.get());
                                    if (!na)
                                        throw ParseError("cannot mix positional and named arguments in class construction", node->line);
                                    bool found = false;
                                    for (const auto &fi : allFields)
                                        if (fi.name == na->name)
                                        {
                                            found = true;
                                            break;
                                        }
                                    if (!found)
                                        throw AttributeError("'" + def.name + "' has no field '" + na->name + "'", node->line);
                                    inst.fields[na->name] = std::move(args[ai++]);
                                }
                            }
                            else
                            {
                                if (args.size() > allFields.size())
                                    throw ArityError(def.name, (int)allFields.size(), (int)args.size(), node->line);
                                for (size_t i = 0; i < args.size(); i++)
                                    inst.fields[allFields[i].name] = std::move(args[i]);
                            }
                            return XObject::makeInstance(std::move(inst));
                        }
                    }
                    else
                    {
                        // Struct frozen construction (original)
                        for (const auto &fi : def.fields)
                            inst.fields[fi.name] = fi.defaultValue.clone();

                        // Merge named arg values into args for raw AST field matching
                        if (hasNamedArgs)
                        {
                            for (auto &[n, v] : namedArgs)
                                args.push_back(std::move(v));
                        }

                        bool hasNamed = false;
                        for (const auto &rawArg : node->args)
                            if (dynamic_cast<const NamedArgExpr *>(rawArg.get()))
                            {
                                hasNamed = true;
                                break;
                            }

                        if (hasNamed)
                        {
                            size_t ai = 0;
                            for (const auto &rawArg : node->args)
                            {
                                auto *na = dynamic_cast<const NamedArgExpr *>(rawArg.get());
                                if (!na)
                                    throw ParseError("cannot mix positional and named arguments in struct construction", node->line);
                                bool found = false;
                                for (const auto &fi : def.fields)
                                    if (fi.name == na->name)
                                    {
                                        found = true;
                                        break;
                                    }
                                if (!found)
                                    throw AttributeError("'" + def.name + "' has no field '" + na->name + "'", node->line);
                                inst.fields[na->name] = std::move(args[ai++]);
                            }
                        }
                        else
                        {
                            if (args.size() > def.fields.size())
                                throw ArityError(def.name, (int)def.fields.size(), (int)args.size(), node->line);
                            for (size_t i = 0; i < args.size(); i++)
                                inst.fields[def.fields[i].name] = std::move(args[i]);
                        }

                        return XObject::makeInstance(std::move(inst));
                    }
                }
            }
        }

        // If the function exists in a Tier 2 module, give a helpful error
        auto abit = allBuiltins_.find(node->callee);
        if (abit != allBuiltins_.end())
        {
            std::string modName = moduleRegistry_.findModuleForFunction(node->callee);
            throw RuntimeError("'" + node->callee + "' requires: bring * from \"" +
                                   modName + "\"",
                               node->line);
        }

        // ---- Instance method call: obj->method(args) ----
        // (Already checked above before builtins, this is a second pass
        //  for the case where the first arg became an instance after the
        //  Tier 2 module check.)
        if (!args.empty() && args[0].isInstance())
        {
            const XInstance &inst = args[0].asInstance();
            auto [mi, ownerClass] = inst.structDef->findMethodWithOwner(node->callee);
            if (mi && mi->fnObject.isFunction())
            {
                if (ownerClass)
                    checkAccess(mi->access, node->callee, *ownerClass, node->line);
                std::shared_ptr<XStructDef> parentDef = nullptr;
                if (ownerClass && ownerClass->isClass && !ownerClass->parents.empty())
                    parentDef = ownerClass->parents[0];
                auto *savedMethodClass = executingMethodClass_;
                executingMethodClass_ = ownerClass;
                auto result = callUserFn(mi->fnObject.asFunction(), args, node->line, parentDef);
                executingMethodClass_ = savedMethodClass;
                return result;
            }
        }

        // Look up user-defined function (throws if not found)
        XObject fnObj = currentEnv_->get(node->callee, node->line);

        // Magic method: __call__ — if the callee is an instance with __call__
        if (fnObj.isInstance())
        {
            XObject result;
            if (callMagicMethod(fnObj, "__call__", args, node->line, result))
                return result;
            throw TypeError("'" + node->callee + "' is not callable (no __call__ method)", node->line);
        }

        if (!fnObj.isFunction())
            throw TypeError("'" + node->callee + "' is not a function", node->line);

        const XFunction &fn = fnObj.asFunction();
        if (!fn.overloads.empty())
        {
            std::vector<const XFunction *> candidates;
            candidates.push_back(&fn);
            for (const auto &ovl : fn.overloads)
                if (ovl.isFunction())
                    candidates.push_back(&ovl.asFunction());

            const XFunction *typeMatch = nullptr;
            const XFunction *arityMatch = nullptr;
            for (const auto *cand : candidates)
            {
                if (!arityMatches(*cand, args.size()))
                    continue;
                if (fnHasTypeAnnotations(*cand))
                {
                    if (typesMatchFn(*cand, args))
                    {
                        typeMatch = cand;
                        break;
                    }
                }
                else
                {
                    if (!arityMatch)
                        arityMatch = cand;
                }
            }
            if (typeMatch)
                return callUserFn(*typeMatch, args, node->line, nullptr, namedArgsPtr);
            if (arityMatch)
                return callUserFn(*arityMatch, args, node->line, nullptr, namedArgsPtr);
            throw TypeError("no matching overload for '" + node->callee +
                                "' with " + std::to_string(args.size()) + " argument(s)",
                            node->line);
        }

        return callUserFn(fn, args, node->line, nullptr, namedArgsPtr);
    }

    XObject Interpreter::callUserFn(const XFunction &fn, std::vector<XObject> &args, int line,
                                    std::shared_ptr<XStructDef> parentClassDef,
                                    const std::vector<std::pair<std::string, XObject>> *namedArgs)
    {
        // ---- Keyword argument resolution ----
        // If named args are provided, reorder/fill them into the positional args
        // vector to match parameter names. Done BEFORE generator check so kwargs
        // work with generators too.
        if (namedArgs && !namedArgs->empty())
        {
            // Build a map of param_name -> index
            std::unordered_map<std::string, size_t> paramIndex;
            for (size_t i = 0; i < fn.params.size(); i++)
                paramIndex[fn.params[i]] = i;

            // Expand args to full param size, filling with sentinel "unfilled" values
            size_t totalParams = fn.params.size();
            std::vector<XObject> resolved(totalParams, XObject::makeNone());
            std::vector<bool> filled(totalParams, false);

            // First, place positional args
            for (size_t i = 0; i < args.size() && i < totalParams; i++)
            {
                resolved[i] = std::move(args[i]);
                filled[i] = true;
            }

            // Then, place named args by matching parameter names
            for (const auto &[name, value] : *namedArgs)
            {
                auto it = paramIndex.find(name);
                if (it == paramIndex.end())
                    throw TypeError("'" + fn.name + "()' got an unexpected keyword argument '" + name + "'", line);
                size_t idx = it->second;
                if (filled[idx])
                    throw TypeError("'" + fn.name + "()' got multiple values for argument '" + name + "'", line);
                resolved[idx] = value;
                filled[idx] = true;
            }

            // Fill unfilled slots with defaults
            for (size_t i = 0; i < totalParams; i++)
            {
                if (!filled[i])
                {
                    if (i < fn.defaults.size() && fn.defaults[i] != nullptr)
                        resolved[i] = eval(fn.defaults[i]);
                    else
                        throw TypeError("'" + fn.name + "()' missing required argument: '" + fn.params[i] + "'", line);
                }
            }

            args = std::move(resolved);
        }

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

        // If a parent class definition was explicitly provided, bind `parent`
        // This enables parent->method(args) calls inside class methods.
        // The parentClassDef is the FIRST parent of the class that DEFINES the
        // method being called, ensuring correct parent resolution in deep hierarchies.
        if (parentClassDef)
        {
            fnEnv.define("parent", XObject::makeStructDef(parentClassDef));
        }
        else
        {
            // Fallback: if no explicit parent provided but self is a class instance
            // parameter, use the instance's class parents (works for single-level
            // hierarchies). We check fn.params rather than walking the scope chain
            // to avoid traversing potentially-dangling closure environments.
            bool hasSelfParam = false;
            for (const auto &p : fn.params)
                if (p == "self")
                {
                    hasSelfParam = true;
                    break;
                }
            if (hasSelfParam && fnEnv.has("self"))
            {
                XObject selfObj = fnEnv.get("self", line);
                if (selfObj.isInstance())
                {
                    const XInstance &selfInst = selfObj.asInstance();
                    if (selfInst.structDef && selfInst.structDef->isClass &&
                        !selfInst.structDef->parents.empty())
                    {
                        fnEnv.define("parent",
                                     XObject::makeStructDef(selfInst.structDef->parents[0]));
                    }
                }
            }
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

        // Magic method: __get__(self, key) for instances
        if (obj.isInstance())
        {
            XObject result;
            std::vector<XObject> getArgs = {idx};
            if (callMagicMethod(obj, "__get__", getArgs, node->line, result))
                return result;
        }

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

        // Instance field access: inst->field
        if (obj.isInstance())
        {
            const XInstance &inst = obj.asInstance();

            // Check for property getter first: get name(self) : ... ;
            if (inst.structDef && inst.structDef->isClass)
            {
                const XPropertyInfo *prop = inst.structDef->findProperty(node->member);
                if (prop)
                {
                    if (prop->getter.isNone())
                        throw AttributeError("property '" + node->member + "' is write-only", node->line);
                    // Call the getter with (self)
                    const XFunction &getterFn = prop->getter.asFunction();
                    std::vector<XObject> getterArgs;
                    getterArgs.push_back(obj);
                    // Set executingMethodClass_ for access control inside getter
                    auto *savedMethodClass = executingMethodClass_;
                    executingMethodClass_ = inst.structDef.get();
                    auto result = callUserFn(getterFn, getterArgs, node->line);
                    executingMethodClass_ = savedMethodClass;
                    return result;
                }
            }

            auto it = inst.fields.find(node->member);
            if (it != inst.fields.end())
            {
                // Check field access level
                if (inst.structDef && inst.structDef->isClass)
                {
                    auto [fi, ownerClass] = inst.structDef->findFieldWithOwner(node->member);
                    if (fi && ownerClass)
                        checkAccess(fi->access, node->member, *ownerClass, node->line);
                }
                return it->second;
            }

            // Check if it's a method name (for passing methods as values)
            // Uses findMethod() to search the full inheritance chain
            if (inst.structDef)
            {
                auto [mi, ownerClass] = inst.structDef->findMethodWithOwner(node->member);
                if (mi)
                {
                    if (ownerClass)
                        checkAccess(mi->access, node->member, *ownerClass, node->line);
                    return mi->fnObject;
                }
            }

            throw AttributeError("'" + inst.typeName + "' has no field '" + node->member + "'", node->line);
        }

        // Struct/Class definition member access — allows parent->method and static member references
        if (obj.isStructDef())
        {
            const XStructDef &def = obj.asStructDef();

            // Check static fields first (ClassName->staticField)
            const XStructFieldInfo *sfi = def.findStaticField(node->member);
            if (sfi)
                return sfi->defaultValue;

            // Check static methods (ClassName->staticMethod)
            const XStructMethodInfo *smi = def.findStaticMethod(node->member);
            if (smi)
                return smi->fnObject;

            // Allow instance method lookup on class definitions (for parent->method patterns)
            const XStructMethodInfo *mi = def.findMethod(node->member);
            if (mi)
                return mi->fnObject;
            std::string kind = def.isClass ? "class" : "struct";
            throw AttributeError("'" + def.name + "' " + kind + " has no member '" + node->member + "'", node->line);
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

    // ---- Struct definition -------------------------------------------------

    void Interpreter::execStructDef(const StructDef *node)
    {
        auto def = std::make_shared<XStructDef>(node->name);

        // Evaluate field default values
        for (const auto &field : node->fields)
        {
            XStructFieldInfo fi;
            fi.name = field.name;
            fi.defaultValue = field.defaultValue ? eval(field.defaultValue.get())
                                                 : XObject::makeNone();
            def->fields.push_back(std::move(fi));
        }

        // Compile methods: create XFunction objects
        for (const auto &method : node->methods)
        {
            XStructMethodInfo mi;
            mi.name = method->name;
            auto fnObj = XObject::makeFunction(method->name, method->params,
                                               &method->body, currentEnv_);
            // Copy default parameter info
            XFunction &fnRef = const_cast<XFunction &>(fnObj.asFunction());
            fnRef.defaults.clear();
            for (const auto &d : method->defaults)
                fnRef.defaults.push_back(d.get());
            fnRef.isVariadic = method->isVariadic;
            fnRef.variadicName = method->variadicName;
            fnRef.isGenerator = containsYield(method->body);
            fnRef.isAsync = method->isAsync;
            fnRef.typeAnnotations = method->paramTypes;
            mi.fnObject = std::move(fnObj);
            def->methods.push_back(std::move(mi));
        }

        currentEnv_->set(node->name, XObject::makeStructDef(def));
    }

    // ---- Class definition: class Name [inherits P1, P2] : body ; ----------

    // Helper: convert AST AccessLevel to runtime AccessLevel
    static AccessLevel astToRuntimeAccess(xell::AccessLevel astLevel)
    {
        switch (astLevel)
        {
        case xell::AccessLevel::PRIVATE:
            return AccessLevel::PRIVATE;
        case xell::AccessLevel::PROTECTED:
            return AccessLevel::PROTECTED;
        default:
            return AccessLevel::PUBLIC;
        }
    }

    // Access control enforcement.
    // Checks whether the current execution context (scope) has permission to
    // access a member with the given access level on the given owning class.
    //
    // Rules:
    //   PUBLIC    — accessible from anywhere
    //   PROTECTED — accessible from this class or subclasses (self is in scope
    //               and self's class isOrInherits owning class)
    //   PRIVATE   — accessible only from this class (self is in scope and
    //               self's class name matches owning class name)
    void Interpreter::checkAccess(AccessLevel access, const std::string &memberName,
                                  const XStructDef &owningClass, int line)
    {
        if (access == AccessLevel::PUBLIC)
            return; // always OK

        // Determine the class context of the currently executing code.
        // We use executingMethodClass_ (set by method dispatch sites) which tracks
        // which class DEFINED the currently running method. This is more accurate
        // than looking at self's concrete type, because a parent class's public method
        // should be able to access its own private fields even when called on a subclass
        // instance.
        const XStructDef *callerClass = executingMethodClass_;

        if (access == AccessLevel::PRIVATE)
        {
            // Private: only accessible from methods defined in the SAME class
            if (!callerClass || callerClass->name != owningClass.name)
                throw AccessError("'" + memberName + "' is private in '" + owningClass.name + "'", line);
        }
        else if (access == AccessLevel::PROTECTED)
        {
            // Protected: accessible from this class or subclasses
            if (!callerClass || !callerClass->isOrInherits(owningClass.name))
                throw AccessError("'" + memberName + "' is protected in '" + owningClass.name + "'", line);
        }
    }

    void Interpreter::execClassDef(const ClassDef *node)
    {
        auto def = std::make_shared<XStructDef>(node->name);
        def->isClass = !node->isMixin; // mixins are not classes
        def->isAbstract = node->isAbstract;
        def->isMixin = node->isMixin;

        // Resolve parent classes from the current environment
        for (const auto &parentName : node->parents)
        {
            XObject parentObj = currentEnv_->get(parentName, node->line);
            if (!parentObj.isStructDef())
                throw TypeError("'" + parentName + "' is not a class/struct", node->line);
            const XStructDef &parentDef = parentObj.asStructDef();
            if (!parentDef.isClass)
                throw TypeError("'" + parentName + "' is a struct, not a class (cannot inherit from structs)", node->line);
            def->parents.push_back(parentObj.asStructDefShared());
        }

        // Resolve implemented interfaces
        for (const auto &ifaceName : node->interfaces)
        {
            XObject ifaceObj = currentEnv_->get(ifaceName, node->line);
            if (!ifaceObj.isStructDef())
                throw TypeError("'" + ifaceName + "' is not an interface", node->line);
            const XStructDef &ifaceDef = ifaceObj.asStructDef();
            if (!ifaceDef.isInterface)
                throw TypeError("'" + ifaceName + "' is not an interface (use 'inherits' for classes)", node->line);
            def->interfaces.push_back(ifaceObj.asStructDefShared());
        }

        // Resolve mixins
        for (const auto &mixinName : node->mixins)
        {
            XObject mixinObj = currentEnv_->get(mixinName, node->line);
            if (!mixinObj.isStructDef())
                throw TypeError("'" + mixinName + "' is not a mixin", node->line);
            const XStructDef &mixinDef = mixinObj.asStructDef();
            if (!mixinDef.isMixin)
                throw TypeError("'" + mixinName + "' is not a mixin (use 'inherits' for classes)", node->line);
            def->mixins.push_back(mixinObj.asStructDefShared());
        }

        // Evaluate field default values — separate static from instance
        for (const auto &field : node->fields)
        {
            XStructFieldInfo fi;
            fi.name = field.name;
            fi.defaultValue = field.defaultValue ? eval(field.defaultValue.get())
                                                 : XObject::makeNone();
            fi.access = astToRuntimeAccess(field.access);
            if (field.isStatic)
                def->staticFields.push_back(std::move(fi));
            else
                def->fields.push_back(std::move(fi));
        }

        // Compile methods: create XFunction objects — separate static from instance
        for (const auto &method : node->methods)
        {
            // Mixins cannot have __init__
            if (def->isMixin && method->name == "__init__")
                throw TypeError("mixin '" + node->name + "' cannot have __init__ (constructors not allowed in mixins)", node->line);

            XStructMethodInfo mi;
            mi.name = method->name;
            mi.access = astToRuntimeAccess(method->access);
            mi.isAbstract = method->isAbstract;

            if (method->isAbstract)
            {
                // Abstract methods: create a lightweight placeholder function
                auto fnObj = XObject::makeFunction(method->name, method->params,
                                                   nullptr, currentEnv_);
                mi.fnObject = std::move(fnObj);
                def->methods.push_back(std::move(mi));
                continue;
            }

            auto fnObj = XObject::makeFunction(method->name, method->params,
                                               &method->body, currentEnv_);
            // Copy default parameter info
            XFunction &fnRef = const_cast<XFunction &>(fnObj.asFunction());
            fnRef.defaults.clear();
            for (const auto &d : method->defaults)
                fnRef.defaults.push_back(d.get());
            fnRef.isVariadic = method->isVariadic;
            fnRef.variadicName = method->variadicName;
            fnRef.isGenerator = containsYield(method->body);
            fnRef.isAsync = method->isAsync;
            fnRef.typeAnnotations = method->paramTypes;
            mi.fnObject = std::move(fnObj);
            if (method->isStatic)
                def->staticMethods.push_back(std::move(mi));
            else
                def->methods.push_back(std::move(mi));
        }

        // Compile properties: create XFunction objects for getters/setters
        for (auto &prop : node->properties)
        {
            XPropertyInfo pi;
            pi.name = prop.name;
            pi.access = astToRuntimeAccess(prop.access);
            if (prop.getter)
            {
                auto getterFn = XObject::makeFunction(
                    "__get_" + prop.name, prop.getter->params,
                    &prop.getter->body, currentEnv_);
                pi.getter = std::move(getterFn);
            }
            else
            {
                pi.getter = XObject::makeNone();
            }
            if (prop.setter)
            {
                auto setterFn = XObject::makeFunction(
                    "__set_" + prop.name, prop.setter->params,
                    &prop.setter->body, currentEnv_);
                pi.setter = std::move(setterFn);
            }
            else
            {
                pi.setter = XObject::makeNone();
            }
            def->properties.push_back(std::move(pi));
        }

        // Validate interface implementation — check all required methods exist
        for (const auto &iface : def->interfaces)
        {
            for (const auto &reqMethod : iface->methods)
            {
                // Search for the method in the class (including inherited methods)
                const XStructMethodInfo *found = def->findMethod(reqMethod.name);
                if (!found)
                {
                    throw TypeError("class '" + node->name + "' does not implement method '" +
                                        reqMethod.name + "' required by interface '" + iface->name + "'",
                                    node->line);
                }
                // Check parameter count matches
                const XFunction &reqFn = reqMethod.fnObject.asFunction();
                const XFunction &implFn = found->fnObject.asFunction();
                if (implFn.params.size() != reqFn.params.size())
                {
                    throw TypeError("method '" + reqMethod.name + "' in class '" + node->name +
                                        "' has " + std::to_string(implFn.params.size()) +
                                        " parameter(s), but interface '" + iface->name +
                                        "' requires " + std::to_string(reqFn.params.size()),
                                    node->line);
                }
            }
        }

        // Validate abstract method implementation — if not abstract itself,
        // all abstract methods from the entire inheritance chain must be implemented
        if (!def->isAbstract)
        {
            // Collect all abstract methods from the entire ancestor chain
            std::vector<std::pair<std::string, std::string>> abstractMethods; // (method name, originating class)
            std::function<void(const XStructDef &)> collectAbstract = [&](const XStructDef &cls)
            {
                for (const auto &m : cls.methods)
                {
                    if (m.isAbstract)
                        abstractMethods.push_back({m.name, cls.name});
                }
                for (const auto &p : cls.parents)
                    collectAbstract(*p);
            };
            for (const auto &parent : def->parents)
                collectAbstract(*parent);

            // Check each abstract method is implemented by this class or its ancestors
            for (const auto &[methodName, originClass] : abstractMethods)
            {
                const XStructMethodInfo *found = def->findMethod(methodName);
                if (!found || found->isAbstract)
                {
                    throw TypeError("class '" + node->name +
                                        "' does not implement abstract method '" +
                                        methodName + "' from '" + originClass + "'",
                                    node->line);
                }
            }
        }

        currentEnv_->set(node->name, XObject::makeStructDef(def));
    }

    // ---- Interface definition ------------------------------------------------

    void Interpreter::execInterfaceDef(const InterfaceDef *node)
    {
        auto def = std::make_shared<XStructDef>(node->name);
        def->isInterface = true;

        // Store method signatures as methods with empty bodies
        // (they serve as contract declarations)
        for (const auto &sig : node->methodSigs)
        {
            XStructMethodInfo mi;
            mi.name = sig.name;
            mi.access = AccessLevel::PUBLIC;
            // Store param count in a lightweight way — we create a minimal function placeholder
            std::vector<std::string> params;
            for (int i = 0; i < sig.paramCount; i++)
                params.push_back("_p" + std::to_string(i));
            mi.fnObject = XObject::makeFunction(sig.name, params, nullptr, currentEnv_);
            def->methods.push_back(std::move(mi));
        }

        currentEnv_->set(node->name, XObject::makeStructDef(def));
    }

    // ---- Member assignment: obj->field = value -----------------------------

    void Interpreter::execMemberAssignment(const MemberAssignment *node)
    {
        // Evaluate the object — ref-counted, so mutations propagate
        XObject obj = eval(node->object.get());
        XObject value = eval(node->value.get());

        if (obj.isInstance())
        {
            XInstance &inst = obj.asInstanceMut();
            // Check if instance is frozen (created with ~ prefix)
            if (inst.frozen)
                throw ImmutabilityError("cannot modify field '" + node->member +
                                            "' on frozen instance of '" + inst.typeName + "'",
                                        node->line);

            // Check for property setter first: set name(self, val) : ... ;
            if (inst.structDef && inst.structDef->isClass)
            {
                const XPropertyInfo *prop = inst.structDef->findProperty(node->member);
                if (prop)
                {
                    if (prop->setter.isNone())
                        throw AttributeError("property '" + node->member + "' is read-only", node->line);
                    // Call the setter with (self, value)
                    const XFunction &setterFn = prop->setter.asFunction();
                    std::vector<XObject> setterArgs;
                    setterArgs.push_back(obj);
                    setterArgs.push_back(std::move(value));
                    auto *savedMethodClass = executingMethodClass_;
                    executingMethodClass_ = inst.structDef.get();
                    callUserFn(setterFn, setterArgs, node->line);
                    executingMethodClass_ = savedMethodClass;
                    return;
                }
            }

            // Only allow setting fields that exist in the struct/class definition
            // For classes, use findField() to search the full inheritance chain
            bool found = false;
            if (inst.structDef->isClass)
            {
                const XStructFieldInfo *fi = inst.structDef->findField(node->member);
                found = (fi != nullptr);
                // Check access level on write
                if (fi)
                {
                    auto [fiOwned, ownerClass] = inst.structDef->findFieldWithOwner(node->member);
                    if (fiOwned && ownerClass)
                        checkAccess(fiOwned->access, node->member, *ownerClass, node->line);
                }
            }
            else
            {
                for (const auto &fi : inst.structDef->fields)
                {
                    if (fi.name == node->member)
                    {
                        found = true;
                        break;
                    }
                }
            }
            if (!found)
                throw AttributeError("'" + inst.typeName + "' has no field '" + node->member + "'", node->line);
            inst.fields[node->member] = std::move(value);
            return;
        }

        if (obj.isMap())
        {
            // Map member assignment: m->key = value
            XObject key = XObject::makeString(node->member);
            obj.asMapMut().set(key, std::move(value));
            return;
        }

        // Static field assignment: ClassName->staticField = value
        if (obj.isStructDef())
        {
            XStructDef &def = const_cast<XStructDef &>(obj.asStructDef());
            XStructFieldInfo *sfi = def.findStaticFieldMut(node->member);
            if (sfi)
            {
                sfi->defaultValue = std::move(value);
                return;
            }
            std::string kind = def.isClass ? "class" : "struct";
            throw AttributeError("'" + def.name + "' " + kind + " has no static field '" + node->member + "'", node->line);
        }

        throw TypeError("member assignment not supported on " +
                            std::string(xtype_name(obj.type())),
                        node->line);
    }

    // ---- Index assignment: list[i] = val  or  map[key] = val ---------------

    void Interpreter::execIndexAssignment(const IndexAssignment *node)
    {
        XObject obj = eval(node->object.get());
        XObject idx = eval(node->index.get());
        XObject value = eval(node->value.get());

        // Magic method: __set__(self, key, val) for instances
        if (obj.isInstance())
        {
            XObject result;
            std::vector<XObject> setArgs = {idx, std::move(value)};
            if (callMagicMethod(obj, "__set__", setArgs, node->line, result))
                return;
        }

        if (obj.isList())
        {
            if (!idx.isNumber())
                throw TypeError("list index must be a number", node->line);
            int index = (int)idx.asNumber();
            auto &list = obj.asListMut();
            if (index < 0)
                index += (int)list.size();
            if (index < 0 || index >= (int)list.size())
                throw IndexError("list index " + std::to_string(index) + " out of range", node->line);
            list[index] = std::move(value);
            return;
        }

        if (obj.isMap())
        {
            obj.asMapMut().set(idx, std::move(value));
            return;
        }

        throw TypeError("index assignment not supported on " +
                            std::string(xtype_name(obj.type())),
                        node->line);
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

    // ---- Decorated class definition -----------------------------------------

    void Interpreter::execDecoratedClassDef(const DecoratedClassDef *node)
    {
        // First, execute the class definition normally
        execClassDef(node->classDef.get());

        // Then apply decorators (bottom-up: last decorator is innermost)
        std::string className = node->classDef->name;

        for (auto it = node->decorators.rbegin(); it != node->decorators.rend(); ++it)
        {
            const std::string &decoratorName = *it;

            // Built-in class decorators: set flags on the XStructDef
            if (decoratorName == "dataclass")
            {
                XObject classObj = currentEnv_->get(className, node->line);
                if (!classObj.isStructDef())
                    throw TypeError("@dataclass can only be applied to classes", node->line);
                auto defPtr = classObj.asStructDefShared();
                defPtr->isDataclass = true;
                // @dataclass: the class already supports field-based construction.
                // No __init__ needed — the default struct-style construction handles it.
                continue;
            }
            if (decoratorName == "immutable")
            {
                XObject classObj = currentEnv_->get(className, node->line);
                if (!classObj.isStructDef())
                    throw TypeError("@immutable can only be applied to classes", node->line);
                auto defPtr = classObj.asStructDefShared();
                defPtr->isImmutable = true;
                continue;
            }
            if (decoratorName == "singleton")
            {
                XObject classObj = currentEnv_->get(className, node->line);
                if (!classObj.isStructDef())
                    throw TypeError("@singleton can only be applied to classes", node->line);
                auto defPtr = classObj.asStructDefShared();
                defPtr->isSingleton = true;
                continue;
            }

            // User-defined class decorators: call decorator(classObj) and replace
            XObject classObj = currentEnv_->get(className, node->line);
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
                    std::vector<XObject> args = {classObj};
                    classObj = bit->second(args, node->line);
                    currentEnv_->set(className, std::move(classObj));
                    continue;
                }
                throw UndefinedVariableError(decoratorName, node->line);
            }

            if (!decoratorFn.isFunction())
                throw TypeError("decorator '" + decoratorName + "' is not a function", node->line);

            std::vector<XObject> args = {classObj};
            classObj = callUserFn(decoratorFn.asFunction(), args, node->line);
            currentEnv_->set(className, std::move(classObj));
        }
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
        // IMPORTANT: capture raw pointer, NOT shared_ptr, to avoid circular dependency.
        // The thread holding a shared_ptr would prevent ~GeneratorState from running,
        // but ~GeneratorState::join() is what terminates the thread — deadlock.
        // Raw pointer is safe because ~GeneratorState() joins the thread before cleanup.
        GeneratorState *stateRaw = gen.state.get();
        const auto *body = fn.body;

        // Capture builtins_ and output_ references for the generator's own interpreter
        // We create a new interpreter that shares our builtins
        auto &outerOutput = output_;
        auto &outerBuiltins = builtins_;
        auto &outerShellState = shellState_;

        gen.state->worker = std::thread([stateRaw, body, genEnv, &outerOutput, &outerBuiltins, &outerShellState]()
                                        {
            // Wait for first next() call
            {
                std::unique_lock<std::mutex> lk(stateRaw->mtx);
                stateRaw->cv.wait(lk, [stateRaw] {
                    return stateRaw->phase == GeneratorState::RUNNING || stateRaw->phase == GeneratorState::DONE;
                });
            }
            if (stateRaw->phase == GeneratorState::DONE) return;

            // Create a mini-interpreter for the generator body
            Interpreter genInterp;
            genInterp.output_ = {};  // generator captures output separately
            genInterp.activeGeneratorState_ = stateRaw;

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
                std::lock_guard<std::mutex> lk(stateRaw->mtx);
                delete stateRaw->yieldedValue;
                stateRaw->yieldedValue = new XObject(sig.value.clone());
            } catch (...) {
                std::lock_guard<std::mutex> lk(stateRaw->mtx);
                stateRaw->error = std::current_exception();
            }

            genInterp.currentEnv_ = savedEnv;

            // Signal completion
            {
                std::lock_guard<std::mutex> lk(stateRaw->mtx);
                stateRaw->phase = GeneratorState::DONE;
            }
            stateRaw->cv.notify_all(); });

        gen.state->started = true;
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

    // ---- let ... be (RAII / Context Manager) ---------------------------------

    void Interpreter::execLet(const LetStmt *node)
    {
        int ln = node->line;
        Environment letEnv(currentEnv_);

        // Track entered resources for reverse-order cleanup
        struct EnteredResource
        {
            XObject resource; // the original object (call __exit__ on this)
            int line;
        };
        std::vector<EnteredResource> entered;

        // Helper: call __exit__ on all entered resources in reverse order.
        // Returns true if all __exit__ calls succeeded; if one throws, captures it.
        auto callExitsReverse = [&](std::exception_ptr &exitExc)
        {
            for (int i = (int)entered.size() - 1; i >= 0; --i)
            {
                try
                {
                    std::vector<XObject> noArgs;
                    XObject ignored;
                    if (!callMagicMethod(entered[i].resource, "__exit__",
                                         noArgs, entered[i].line, ignored))
                    {
                        throw TypeError("__exit__ not found on resource", entered[i].line);
                    }
                }
                catch (...)
                {
                    if (!exitExc)
                        exitExc = std::current_exception();
                    // Continue calling remaining __exit__ methods even if one fails
                }
            }
        };

        // Phase 1: Evaluate each binding, call __enter__, bind result
        // Use letEnv so earlier bindings are visible to later binding expressions
        auto *savedEnv = currentEnv_;
        currentEnv_ = &letEnv;
        for (size_t i = 0; i < node->bindings.size(); ++i)
        {
            const auto &binding = node->bindings[i];
            try
            {
                // Evaluate the resource expression
                XObject resource = eval(binding.expr.get());

                // Validate the resource has __enter__ and __exit__
                if (!resource.isInstance())
                    throw TypeError("let ... be requires an instance with __enter__ and __exit__", binding.line);

                // Call __enter__
                std::vector<XObject> noArgs;
                XObject enterResult;
                if (!callMagicMethod(resource, "__enter__", noArgs, binding.line, enterResult))
                    throw TypeError("let ... be requires __enter__ on '" +
                                        (resource.asInstance().structDef
                                             ? resource.asInstance().structDef->name
                                             : std::string("unknown")) +
                                        "'",
                                    binding.line);

                // Check __exit__ exists too (fail early)
                const XInstance &inst = resource.asInstance();
                if (inst.structDef)
                {
                    auto [mi, _] = inst.structDef->findMethodWithOwner("__exit__");
                    if (!mi)
                        throw TypeError("let ... be requires __exit__ on '" +
                                            inst.structDef->name + "'",
                                        binding.line);
                }

                // Track the resource for cleanup
                entered.push_back({resource, binding.line});

                // Bind the __enter__ result to the name (unless "_" discard)
                if (binding.name != "_")
                    letEnv.define(binding.name, std::move(enterResult));
            }
            catch (...)
            {
                // __enter__ of binding i failed — clean up already-entered (0..i-1)
                std::exception_ptr exitExc;
                callExitsReverse(exitExc);
                currentEnv_ = savedEnv;
                // Re-throw the original __enter__ failure (exitExc lost if both fail)
                throw;
            }
        }

        // Phase 2: Execute the body block, then always run __exit__ in reverse
        // currentEnv_ is already pointing to letEnv from Phase 1
        std::exception_ptr bodyExc;
        try
        {
            for (const auto &stmt : node->body)
                exec(stmt.get());
        }
        catch (...)
        {
            bodyExc = std::current_exception();
        }

        // Restore parent environment
        currentEnv_ = savedEnv;

        // Phase 3: Call __exit__ on all resources in reverse order
        std::exception_ptr exitExc;
        callExitsReverse(exitExc);

        // Phase 4: Propagate exceptions
        // If the body threw, re-throw it (even if __exit__ also threw — body error takes priority,
        // unless we want __exit__ to replace it like Python. Per the plan: __exit__ error replaces.)
        if (bodyExc)
        {
            if (exitExc)
                std::rethrow_exception(exitExc); // __exit__ error replaces body error (per plan)
            std::rethrow_exception(bodyExc);
        }
        if (exitExc)
            std::rethrow_exception(exitExc);
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
    // Static members for __hash__ callback
    // ========================================================================

    Interpreter *Interpreter::currentInterpreter_ = nullptr;

    bool Interpreter::instanceHashCallback(const XObject &instance, int64_t &result)
    {
        if (!currentInterpreter_)
            return false;
        XObject hashResult;
        std::vector<XObject> args;
        if (currentInterpreter_->callMagicMethod(instance, "__hash__", args, 0, hashResult))
        {
            if (hashResult.isInt())
            {
                result = hashResult.asInt();
                return true;
            }
            if (hashResult.isFloat())
            {
                result = static_cast<int64_t>(hashResult.asNumber());
                return true;
            }
            throw HashError("__hash__ must return an integer", 0);
        }
        return false;
    }

    // ========================================================================
    // Magic method helper: call __dunder__ on an instance if it exists
    // ========================================================================

    bool Interpreter::callMagicMethod(const XObject &instance, const std::string &methodName,
                                      std::vector<XObject> &args, int line, XObject &result)
    {
        if (!instance.isInstance())
            return false;
        const XInstance &inst = instance.asInstance();
        if (!inst.structDef)
            return false;
        auto [mi, ownerClass] = inst.structDef->findMethodWithOwner(methodName);
        if (!mi || !mi->fnObject.isFunction())
            return false;

        // Build args: [self, ...args]
        std::vector<XObject> callArgs;
        callArgs.push_back(instance);
        for (auto &a : args)
            callArgs.push_back(std::move(a));

        std::shared_ptr<XStructDef> parentDef = nullptr;
        if (ownerClass && ownerClass->isClass && !ownerClass->parents.empty())
            parentDef = ownerClass->parents[0];
        auto *savedMethodClass = executingMethodClass_;
        executingMethodClass_ = ownerClass;
        result = callUserFn(mi->fnObject.asFunction(), callArgs, line, parentDef);
        executingMethodClass_ = savedMethodClass;
        return true;
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
                        XObject val = eval(es->expr.get());
                        // Check for __str__ or __print__ magic method on instances
                        if (val.isInstance())
                        {
                            XObject magicResult;
                            std::vector<XObject> noArgs;
                            if (callMagicMethod(val, "__str__", noArgs, line, magicResult))
                            {
                                result += magicResult.toString();
                                i = j;
                                continue;
                            }
                            if (callMagicMethod(val, "__print__", noArgs, line, magicResult))
                            {
                                result += magicResult.toString();
                                i = j;
                                continue;
                            }
                        }
                        result += val.toString();
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
