#include "interpreter.hpp"
#include "../builtins/register_all.hpp"
#include "../lexer/lexer.hpp"
#include "../parser/parser.hpp"
#include <sstream>
#include <cmath>
#include <algorithm>
#include <cassert>
#include <iostream>

namespace xell
{

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
        registerAllBuiltins(builtins_, output_);
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
        if (auto *p = dynamic_cast<const ExprStmt *>(stmt))
            return execExprStmt(p);
        if (auto *p = dynamic_cast<const BringStmt *>(stmt))
        {
            throw NotImplementedError("bring", p->line);
        }
    }

    void Interpreter::execBlock(const std::vector<StmtPtr> &stmts, Environment &env)
    {
        auto *savedEnv = currentEnv_;
        currentEnv_ = &env;
        for (const auto &stmt : stmts)
        {
            exec(stmt.get());
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

        const auto &list = iterable.asList();
        for (size_t i = 0; i < list.size(); i++)
        {
            loopEnv.define(node->varName, list[i]);
            for (const auto &stmt : node->body)
            {
                exec(stmt.get());
            }
        }

        currentEnv_ = savedEnv;
    }

    void Interpreter::execWhile(const WhileStmt *node)
    {
        Environment loopEnv(currentEnv_);
        auto *savedEnv = currentEnv_;
        currentEnv_ = &loopEnv;

        while (eval(node->condition.get()).truthy())
        {
            for (const auto &stmt : node->body)
            {
                exec(stmt.get());
            }
        }

        currentEnv_ = savedEnv;
    }

    void Interpreter::execFnDef(const FnDef *node)
    {
        // Capture the current environment as the lexical closure scope
        auto fn = XObject::makeFunction(node->name, node->params, &node->body, currentEnv_);
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
    // Expression evaluation
    // ========================================================================

    XObject Interpreter::eval(const Expr *expr)
    {
        // Literals
        if (auto *p = dynamic_cast<const NumberLiteral *>(expr))
            return XObject::makeNumber(p->value);

        if (auto *p = dynamic_cast<const BoolLiteral *>(expr))
            return XObject::makeBool(p->value);

        if (dynamic_cast<const NoneLiteral *>(expr))
            return XObject::makeNone();

        if (auto *p = dynamic_cast<const StringLiteral *>(expr))
        {
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

        throw NotImplementedError("unknown expression node", expr->line);
    }

    // ---- Binary expressions ------------------------------------------------

    XObject Interpreter::evalBinary(const BinaryExpr *node)
    {
        const std::string &op = node->op;

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
            if (left.isNumber() && right.isNumber())
                return XObject::makeNumber(left.asNumber() + right.asNumber());
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
            if (!left.isNumber() || !right.isNumber())
                throw TypeError("unsupported operand types for " + op + ": " +
                                    std::string(xtype_name(left.type())) + " and " +
                                    std::string(xtype_name(right.type())),
                                node->line);
            double l = left.asNumber(), r = right.asNumber();
            if (op == "-")
                return XObject::makeNumber(l - r);
            if (op == "*")
                return XObject::makeNumber(l * r);
            if (op == "%")
            {
                if (r == 0.0)
                    throw DivisionByZeroError(node->line);
                return XObject::makeNumber(std::fmod(l, r));
            }
            // op == "/"
            if (r == 0.0)
                throw DivisionByZeroError(node->line);
            return XObject::makeNumber(l / r);
        }

        // Comparison (numbers and strings)
        if (op == ">" || op == "<" || op == ">=" || op == "<=")
        {
            if (left.isNumber() && right.isNumber())
            {
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
            if (!val.isNumber())
                throw TypeError("unary '-' requires a number, got " +
                                    std::string(xtype_name(val.type())),
                                node->line);
            return XObject::makeNumber(-val.asNumber());
        }

        // Prefix ++ and --
        if (op == "++" || op == "--")
        {
            auto *ident = dynamic_cast<const Identifier *>(node->operand.get());
            if (!ident)
                throw TypeError("prefix " + op + " requires a variable", node->line);

            XObject val = currentEnv_->get(ident->name, node->line);
            if (!val.isNumber())
                throw TypeError("prefix " + op + " requires a number", node->line);

            double newVal = val.asNumber() + (op == "++" ? 1.0 : -1.0);
            XObject result = XObject::makeNumber(newVal);
            currentEnv_->set(ident->name, XObject::makeNumber(newVal));
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
        if (!val.isNumber())
            throw TypeError("postfix " + node->op + " requires a number", node->line);

        double oldVal = val.asNumber();
        double newVal = oldVal + (node->op == "++" ? 1.0 : -1.0);
        currentEnv_->set(ident->name, XObject::makeNumber(newVal));
        return XObject::makeNumber(oldVal); // postfix returns old value
    }

    // ---- Function calls ----------------------------------------------------

    XObject Interpreter::evalCall(const CallExpr *node)
    {
        // Evaluate arguments
        std::vector<XObject> args;
        for (const auto &arg : node->args)
        {
            args.push_back(eval(arg.get()));
        }

        // Check builtins first
        auto bit = builtins_.find(node->callee);
        if (bit != builtins_.end())
        {
            return bit->second(args, node->line);
        }

        // Look up user-defined function
        XObject fnObj = currentEnv_->get(node->callee, node->line);
        if (!fnObj.isFunction())
            throw TypeError("'" + node->callee + "' is not a function", node->line);

        return callUserFn(fnObj.asFunction(), args, node->line);
    }

    XObject Interpreter::callUserFn(const XFunction &fn, std::vector<XObject> &args, int line)
    {
        // Arity check
        if (args.size() != fn.params.size())
            throw ArityError(fn.name, (int)fn.params.size(), (int)args.size(), line);

        // Recursion guard
        if (callDepth_ >= MAX_CALL_DEPTH)
            throw RecursionError(MAX_CALL_DEPTH, line);

        // Lexical scoping: parent = the environment where the function was *defined*
        Environment fnEnv(fn.closureEnv ? fn.closureEnv : currentEnv_);

        // Bind parameters
        for (size_t i = 0; i < fn.params.size(); i++)
        {
            fnEnv.define(fn.params[i], std::move(args[i]));
        }

        // Execute body, catching GiveSignal for return values
        callDepth_++;
        auto *savedEnv = currentEnv_;
        currentEnv_ = &fnEnv;

        XObject result = XObject::makeNone();
        try
        {
            if (fn.body)
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

        if (obj.isMap())
        {
            if (!idx.isString())
                throw TypeError("map key must be a string", node->line);
            const XObject *val = obj.asMap().get(idx.asString());
            if (!val)
                throw KeyError(idx.asString(), node->line);
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
            elements.push_back(eval(elem.get()));
        }
        return XObject::makeList(std::move(elements));
    }

    // ---- Map literal -------------------------------------------------------

    XObject Interpreter::evalMap(const MapLiteral *node)
    {
        XMap map;
        for (const auto &entry : node->entries)
        {
            map.set(entry.first, eval(entry.second.get()));
        }
        return XObject::makeMap(std::move(map));
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
