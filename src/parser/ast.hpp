#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>

namespace xell
{

    // ============================================================
    // Forward declarations & smart-pointer aliases
    // ============================================================

    struct Expr;
    struct Stmt;

    using ExprPtr = std::unique_ptr<Expr>;
    using StmtPtr = std::unique_ptr<Stmt>;

    // ============================================================
    // Base classes
    // ============================================================

    struct Expr
    {
        int line = 0;
        virtual ~Expr() = default;
    };

    struct Stmt
    {
        int line = 0;
        virtual ~Stmt() = default;
    };

    // ============================================================
    // Expression nodes
    // ============================================================

    struct NumberLiteral : Expr
    {
        double value;
        explicit NumberLiteral(double v, int ln = 0) : value(v) { line = ln; }
    };

    // Integer literal: 42, 0, -1 (produced by parser for NUMBER tokens without '.')
    struct IntLiteral : Expr
    {
        int64_t value;
        explicit IntLiteral(int64_t v, int ln = 0) : value(v) { line = ln; }
    };

    // Float literal: 3.14, 0.5 (produced by parser for NUMBER tokens with '.')
    struct FloatLiteral : Expr
    {
        double value;
        explicit FloatLiteral(double v, int ln = 0) : value(v) { line = ln; }
    };

    // Imaginary literal: 2i, 3.14i (for building complex numbers)
    struct ImaginaryLiteral : Expr
    {
        double value; // the imaginary coefficient
        explicit ImaginaryLiteral(double v, int ln = 0) : value(v) { line = ln; }
    };

    struct StringLiteral : Expr
    {
        std::string value; // raw content with {interpolation} markers preserved
        bool isRaw;        // true for r"..." strings — no interpolation
        explicit StringLiteral(std::string v, int ln = 0, bool raw = false)
            : value(std::move(v)), isRaw(raw) { line = ln; }
    };

    struct BoolLiteral : Expr
    {
        bool value;
        explicit BoolLiteral(bool v, int ln = 0) : value(v) { line = ln; }
    };

    struct NoneLiteral : Expr
    {
        explicit NoneLiteral(int ln = 0) { line = ln; }
    };

    struct Identifier : Expr
    {
        std::string name;
        explicit Identifier(std::string n, int ln = 0) : name(std::move(n)) { line = ln; }
    };

    struct ListLiteral : Expr
    {
        std::vector<ExprPtr> elements;
        explicit ListLiteral(std::vector<ExprPtr> elems, int ln = 0)
            : elements(std::move(elems)) { line = ln; }
    };

    struct TupleLiteral : Expr
    {
        std::vector<ExprPtr> elements;
        explicit TupleLiteral(std::vector<ExprPtr> elems, int ln = 0)
            : elements(std::move(elems)) { line = ln; }
    };

    struct SetLiteral : Expr
    {
        std::vector<ExprPtr> elements;
        explicit SetLiteral(std::vector<ExprPtr> elems, int ln = 0)
            : elements(std::move(elems)) { line = ln; }
    };

    // Frozen (immutable) set: <1, 2, 3>
    struct FrozenSetLiteral : Expr
    {
        std::vector<ExprPtr> elements;
        explicit FrozenSetLiteral(std::vector<ExprPtr> elems, int ln = 0)
            : elements(std::move(elems)) { line = ln; }
    };

    struct MapLiteral : Expr
    {
        std::vector<std::pair<std::string, ExprPtr>> entries;
        explicit MapLiteral(std::vector<std::pair<std::string, ExprPtr>> e, int ln = 0)
            : entries(std::move(e)) { line = ln; }
    };

    struct BinaryExpr : Expr
    {
        ExprPtr left;
        std::string op; // normalized: +, -, *, /, ==, !=, >, <, >=, <=, and, or
        ExprPtr right;
        BinaryExpr(ExprPtr l, std::string o, ExprPtr r, int ln = 0)
            : left(std::move(l)), op(std::move(o)), right(std::move(r)) { line = ln; }
    };

    struct UnaryExpr : Expr
    {
        std::string op; // "not", "!", "-", "++", "--"
        ExprPtr operand;
        UnaryExpr(std::string o, ExprPtr operand, int ln = 0)
            : op(std::move(o)), operand(std::move(operand)) { line = ln; }
    };

    // Postfix ++ and --  (e.g.  count++  count--)
    struct PostfixExpr : Expr
    {
        std::string op; // "++" or "--"
        ExprPtr operand;
        PostfixExpr(std::string o, ExprPtr operand, int ln = 0)
            : op(std::move(o)), operand(std::move(operand)) { line = ln; }
    };

    struct CallExpr : Expr
    {
        std::string callee;
        std::vector<ExprPtr> args;
        CallExpr(std::string callee, std::vector<ExprPtr> args, int ln = 0)
            : callee(std::move(callee)), args(std::move(args)) { line = ln; }
    };

    struct IndexAccess : Expr
    {
        ExprPtr object;
        ExprPtr index;
        IndexAccess(ExprPtr obj, ExprPtr idx, int ln = 0)
            : object(std::move(obj)), index(std::move(idx)) { line = ln; }
    };

    struct MemberAccess : Expr
    {
        ExprPtr object;
        std::string member;
        MemberAccess(ExprPtr obj, std::string mem, int ln = 0)
            : object(std::move(obj)), member(std::move(mem)) { line = ln; }
    };

    // Ternary expression: value if condition else alternative
    struct TernaryExpr : Expr
    {
        ExprPtr value;       // expression before "if"
        ExprPtr condition;   // condition after "if"
        ExprPtr alternative; // expression after "else"
        TernaryExpr(ExprPtr val, ExprPtr cond, ExprPtr alt, int ln = 0)
            : value(std::move(val)), condition(std::move(cond)), alternative(std::move(alt)) { line = ln; }
    };

    // Lambda expression: x => expr  or  (a, b) => expr  or  x => : block ;
    struct LambdaExpr : Expr
    {
        std::vector<std::string> params;
        std::vector<StmtPtr> body; // for multi-line: x => : ... ;
        ExprPtr singleExpr;        // for inline: x => x * 2 (body will be empty)
        LambdaExpr(std::vector<std::string> params, std::vector<StmtPtr> body, ExprPtr singleExpr, int ln = 0)
            : params(std::move(params)), body(std::move(body)), singleExpr(std::move(singleExpr)) { line = ln; }
    };

    // Spread expression: ...list inside list literals or function calls
    struct SpreadExpr : Expr
    {
        ExprPtr operand;
        explicit SpreadExpr(ExprPtr op, int ln = 0) : operand(std::move(op)) { line = ln; }
    };

    // ============================================================
    // Statement nodes
    // ============================================================

    struct ExprStmt : Stmt
    {
        ExprPtr expr;
        explicit ExprStmt(ExprPtr e, int ln = 0) : expr(std::move(e)) { line = ln; }
    };

    struct Assignment : Stmt
    {
        std::string name;
        ExprPtr value;
        Assignment(std::string n, ExprPtr v, int ln = 0)
            : name(std::move(n)), value(std::move(v)) { line = ln; }
    };

    // Helper for elif clauses
    struct ElifClause
    {
        ExprPtr condition;
        std::vector<StmtPtr> body;
        int line = 0;
    };

    struct IfStmt : Stmt
    {
        ExprPtr condition;
        std::vector<StmtPtr> body;
        std::vector<ElifClause> elifs;
        std::vector<StmtPtr> elseBody;
        IfStmt(ExprPtr cond, std::vector<StmtPtr> body,
               std::vector<ElifClause> elifs, std::vector<StmtPtr> elseBody, int ln = 0)
            : condition(std::move(cond)), body(std::move(body)),
              elifs(std::move(elifs)), elseBody(std::move(elseBody)) { line = ln; }
    };

    struct ForStmt : Stmt
    {
        std::string varName;
        ExprPtr iterable;
        std::vector<StmtPtr> body;
        ForStmt(std::string var, ExprPtr iter, std::vector<StmtPtr> body, int ln = 0)
            : varName(std::move(var)), iterable(std::move(iter)), body(std::move(body)) { line = ln; }
    };

    struct WhileStmt : Stmt
    {
        ExprPtr condition;
        std::vector<StmtPtr> body;
        WhileStmt(ExprPtr cond, std::vector<StmtPtr> body, int ln = 0)
            : condition(std::move(cond)), body(std::move(body)) { line = ln; }
    };

    struct FnDef : Stmt
    {
        std::string name;
        std::vector<std::string> params;
        std::vector<ExprPtr> defaults; // default value for each param (nullptr = no default)
        bool isVariadic = false;       // true if last param is ...name
        std::string variadicName;      // name of variadic param (without ...)
        std::vector<StmtPtr> body;
        FnDef(std::string name, std::vector<std::string> params, std::vector<StmtPtr> body, int ln = 0)
            : name(std::move(name)), params(std::move(params)), body(std::move(body)) { line = ln; }
    };

    struct GiveStmt : Stmt
    {
        ExprPtr value; // nullptr → give with no value (returns none)
        explicit GiveStmt(ExprPtr v, int ln = 0) : value(std::move(v)) { line = ln; }
    };

    struct BreakStmt : Stmt
    {
        explicit BreakStmt(int ln = 0) { line = ln; }
    };

    struct ContinueStmt : Stmt
    {
        explicit ContinueStmt(int ln = 0) { line = ln; }
    };

    struct TryCatchStmt : Stmt
    {
        std::vector<StmtPtr> tryBody;
        std::string catchVarName; // name of error variable in catch block (e.g., "e" in "catch e :")
        std::vector<StmtPtr> catchBody;
        std::vector<StmtPtr> finallyBody; // empty if no finally block
        TryCatchStmt(std::vector<StmtPtr> tryB, std::string catchVar,
                     std::vector<StmtPtr> catchB, std::vector<StmtPtr> finallyB, int ln = 0)
            : tryBody(std::move(tryB)), catchVarName(std::move(catchVar)),
              catchBody(std::move(catchB)), finallyBody(std::move(finallyB)) { line = ln; }
    };

    // incase x : is 1 or 2 : ... ; is 3 : ... ; else : ... ; ;
    struct InCaseClause
    {
        std::vector<ExprPtr> values; // one or more values joined by 'or'
        std::vector<StmtPtr> body;
        int line = 0;
    };

    struct InCaseStmt : Stmt
    {
        ExprPtr subject;                   // the value being matched
        std::vector<InCaseClause> clauses; // is ... : ... ; branches
        std::vector<StmtPtr> elseBody;     // else : ... ;
        InCaseStmt(ExprPtr subj, std::vector<InCaseClause> clauses,
                   std::vector<StmtPtr> elseBody, int ln = 0)
            : subject(std::move(subj)), clauses(std::move(clauses)),
              elseBody(std::move(elseBody)) { line = ln; }
    };

    // Destructuring assignment: a, b = [1, 2]
    struct DestructuringAssignment : Stmt
    {
        std::vector<std::string> names;
        ExprPtr value;
        DestructuringAssignment(std::vector<std::string> names, ExprPtr val, int ln = 0)
            : names(std::move(names)), value(std::move(val)) { line = ln; }
    };

    struct BringStmt : Stmt
    {
        bool bringAll;                    // true for "bring * from ..."
        std::vector<std::string> names;   // names to bring (empty if bringAll)
        std::string path;                 // file path string
        std::vector<std::string> aliases; // optional aliases
        BringStmt(bool all, std::vector<std::string> names, std::string path,
                  std::vector<std::string> aliases, int ln = 0)
            : bringAll(all), names(std::move(names)), path(std::move(path)),
              aliases(std::move(aliases)) { line = ln; }
    };

    // ============================================================
    // Top-level program
    // ============================================================

    struct Program
    {
        std::vector<StmtPtr> statements;
    };

} // namespace xell
