#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>
#include "../common/access_level.hpp"

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
        std::vector<std::pair<ExprPtr, ExprPtr>> entries;
        explicit MapLiteral(std::vector<std::pair<ExprPtr, ExprPtr>> e, int ln = 0)
            : entries(std::move(e)) { line = ln; }
    };

    // ---- Comprehension clauses ----
    // A clause is either a for-clause or an if-clause.
    struct CompClause
    {
        bool isFor;                    // true = for-clause, false = if-clause
        std::vector<std::string> vars; // for-clause: loop variable names
        ExprPtr iterable;              // for-clause: iterable expression
        ExprPtr condition;             // if-clause: condition expression
    };

    // [expr for x in iterable if cond ...]
    struct ListComprehension : Expr
    {
        ExprPtr valueExpr;
        std::vector<CompClause> clauses;
        ListComprehension(ExprPtr val, std::vector<CompClause> cls, int ln = 0)
            : valueExpr(std::move(val)), clauses(std::move(cls)) { line = ln; }
    };

    // {expr for x in iterable if cond ...}
    struct SetComprehension : Expr
    {
        ExprPtr valueExpr;
        std::vector<CompClause> clauses;
        SetComprehension(ExprPtr val, std::vector<CompClause> cls, int ln = 0)
            : valueExpr(std::move(val)), clauses(std::move(cls)) { line = ln; }
    };

    // {keyExpr: valExpr for x in iterable if cond ...}
    struct MapComprehension : Expr
    {
        ExprPtr keyExpr;
        ExprPtr valueExpr;
        std::vector<CompClause> clauses;
        MapComprehension(ExprPtr k, ExprPtr v, std::vector<CompClause> cls, int ln = 0)
            : keyExpr(std::move(k)), valueExpr(std::move(v)), clauses(std::move(cls)) { line = ln; }
    };

    struct BinaryExpr : Expr
    {
        ExprPtr left;
        std::string op; // normalized: +, -, *, /, ==, !=, >, <, >=, <=, and, or
        ExprPtr right;
        BinaryExpr(ExprPtr l, std::string o, ExprPtr r, int ln = 0)
            : left(std::move(l)), op(std::move(o)), right(std::move(r)) { line = ln; }
    };

    // a < b < c >= d  →  operands=[a,b,c,d], ops=["<","<",">="]
    // Evaluated left-to-right with short-circuit: (a<b) and (b<c) and (c>=d)
    struct ChainedComparisonExpr : Expr
    {
        std::vector<ExprPtr> operands; // N operands
        std::vector<std::string> ops;  // N-1 operators
        ChainedComparisonExpr(std::vector<ExprPtr> operands, std::vector<std::string> ops, int ln = 0)
            : operands(std::move(operands)), ops(std::move(ops)) { line = ln; }
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
        bool isMethodCall = false; // true when rewritten from obj->method(args)
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

    // obj[start:end] or obj[start:end:step] — all three are optional (nullable)
    struct SliceExpr : Expr
    {
        ExprPtr object;
        ExprPtr start; // nullable → from beginning
        ExprPtr end;   // nullable → to end
        ExprPtr step;  // nullable → default 1
        SliceExpr(ExprPtr obj, ExprPtr s, ExprPtr e, ExprPtr st, int ln = 0)
            : object(std::move(obj)), start(std::move(s)),
              end(std::move(e)), step(std::move(st)) { line = ln; }
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

    // If expression: if cond: value elif cond: value else: value
    struct IfExprBranch
    {
        ExprPtr condition; // nullptr for else branch
        ExprPtr value;
        int line = 0;
    };

    struct IfExpr : Expr
    {
        std::vector<IfExprBranch> branches; // if + elifs + else
        IfExpr(std::vector<IfExprBranch> branches, int ln = 0)
            : branches(std::move(branches)) { line = ln; }
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

    // Shell command expression: $cmd args...
    // Standalone → print output; Assigned → returns list of lines or structured data
    struct ShellCmdExpr : Expr
    {
        std::string command; // raw command string (everything after $)
        explicit ShellCmdExpr(std::string cmd, int ln = 0) : command(std::move(cmd)) { line = ln; }
    };

    // Named argument: name: value (used in struct construction)
    struct NamedArgExpr : Expr
    {
        std::string name;
        ExprPtr value;
        NamedArgExpr(std::string name, ExprPtr val, int ln = 0)
            : name(std::move(name)), value(std::move(val)) { line = ln; }
    };

    // Yield expression: yield value (for generators)
    struct YieldExpr : Expr
    {
        ExprPtr value; // nullptr → yield with no value (yields none)
        explicit YieldExpr(ExprPtr v, int ln = 0) : value(std::move(v)) { line = ln; }
    };

    // Await expression: await expr (for async)
    struct AwaitExpr : Expr
    {
        ExprPtr operand;
        explicit AwaitExpr(ExprPtr op, int ln = 0) : operand(std::move(op)) { line = ln; }
    };

    // Bytes literal: b"\x48\x65\x6c\x6c\x6f"
    struct BytesLiteral : Expr
    {
        std::string bytes; // raw byte data
        explicit BytesLiteral(std::string b, int ln = 0) : bytes(std::move(b)) { line = ln; }
    };

    // For expression: x = for i in list: if cond: break val; give default;
    struct ForExpr : Expr
    {
        std::vector<std::string> varNames;
        std::vector<ExprPtr> iterables;
        std::vector<StmtPtr> body;
        ExprPtr defaultValue; // optional give VALUE at end (default if no break value)
        bool hasRest = false;
        std::string restName;
        ForExpr(std::vector<std::string> vars, std::vector<ExprPtr> iters,
                std::vector<StmtPtr> body, ExprPtr defVal, bool hasRest,
                std::string restName, int ln = 0)
            : varNames(std::move(vars)), iterables(std::move(iters)),
              body(std::move(body)), defaultValue(std::move(defVal)),
              hasRest(hasRest), restName(std::move(restName)) { line = ln; }
    };

    // While expression: x = while cond: ... break val; give default;
    struct WhileExpr : Expr
    {
        ExprPtr condition;
        std::vector<StmtPtr> body;
        ExprPtr defaultValue; // optional give VALUE at end
        WhileExpr(ExprPtr cond, std::vector<StmtPtr> body, ExprPtr defVal, int ln = 0)
            : condition(std::move(cond)), body(std::move(body)),
              defaultValue(std::move(defVal)) { line = ln; }
    };

    // Loop expression: x = loop: ... break val; give default;
    struct LoopExpr : Expr
    {
        std::vector<StmtPtr> body;
        ExprPtr defaultValue; // optional give VALUE at end
        LoopExpr(std::vector<StmtPtr> body, ExprPtr defVal, int ln = 0)
            : body(std::move(body)), defaultValue(std::move(defVal)) { line = ln; }
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

    // Immutable binding: immutable x = expr
    struct ImmutableBinding : Stmt
    {
        std::string name;
        ExprPtr value;
        ImmutableBinding(std::string n, ExprPtr v, int ln = 0)
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
        std::vector<std::string> varNames; // loop target variables (one or more)
        bool hasRest = false;              // true if last target is ...name
        std::string restName;              // name of the rest capture variable
        std::vector<ExprPtr> iterables;    // one or more source expressions
        std::vector<StmtPtr> body;
        ForStmt(std::vector<std::string> vars, std::vector<ExprPtr> iters,
                std::vector<StmtPtr> body, bool hasRest, std::string restName, int ln = 0)
            : varNames(std::move(vars)), hasRest(hasRest), restName(std::move(restName)),
              iterables(std::move(iters)), body(std::move(body)) { line = ln; }
    };

    struct WhileStmt : Stmt
    {
        ExprPtr condition;
        std::vector<StmtPtr> body;
        WhileStmt(ExprPtr cond, std::vector<StmtPtr> body, int ln = 0)
            : condition(std::move(cond)), body(std::move(body)) { line = ln; }
    };

    // loop : BLOCK ;  — infinite loop (breaks required)
    struct LoopStmt : Stmt
    {
        std::vector<StmtPtr> body;
        bool safeLoop = false; // @safe_loop decorator applied
        LoopStmt(std::vector<StmtPtr> body, bool safe = false, int ln = 0)
            : body(std::move(body)), safeLoop(safe) { line = ln; }
    };

    // do : BLOCK ; while CONDITION  — post-condition loop
    struct DoWhileStmt : Stmt
    {
        std::vector<StmtPtr> body;
        ExprPtr condition;
        DoWhileStmt(std::vector<StmtPtr> body, ExprPtr cond, int ln = 0)
            : body(std::move(body)), condition(std::move(cond)) { line = ln; }
    };

    struct FnDef : Stmt
    {
        std::string name;
        std::vector<std::string> params;
        std::vector<ExprPtr> defaults; // default value for each param (nullptr = no default)
        bool isVariadic = false;       // true if last param is ...name
        std::string variadicName;      // name of variadic param (without ...)
        std::vector<StmtPtr> body;
        bool isAsync = false;                     // true for async fn
        bool isStatic = false;                    // true for static class members
        bool isAbstract = false;                  // true for abstract methods (no body)
        AccessLevel access = AccessLevel::PUBLIC; // access level when used as class method

        // Type annotations (optional)
        std::vector<std::string> paramTypes; // type annotation per param (empty string = no annotation)
        std::string returnType;              // return type annotation (empty = none)

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
        ExprPtr value; // nullptr → break with no value (statement loop), non-null → break VALUE (expression loop)
        explicit BreakStmt(ExprPtr v = nullptr, int ln = 0) : value(std::move(v)) { line = ln; }
    };

    struct ContinueStmt : Stmt
    {
        explicit ContinueStmt(int ln = 0) { line = ln; }
    };

    // A single catch clause: catch varname [is Type [or Type]*] : body ;
    struct CatchClause
    {
        std::string varName;                 // error variable name
        std::vector<std::string> errorTypes; // empty = catch-all, else matched types
        std::vector<StmtPtr> body;
    };

    struct TryCatchStmt : Stmt
    {
        std::vector<StmtPtr> tryBody;
        std::vector<CatchClause> catchClauses; // one or more catch blocks
        std::vector<StmtPtr> finallyBody;      // empty if no finally block
        TryCatchStmt(std::vector<StmtPtr> tryB, std::vector<CatchClause> catches,
                     std::vector<StmtPtr> finallyB, int ln = 0)
            : tryBody(std::move(tryB)), catchClauses(std::move(catches)),
              finallyBody(std::move(finallyB)) { line = ln; }
    };

    // throw expr — user-thrown error
    // expr can be: a string (becomes RuntimeError message),
    //              a map with "message"/"type" keys (structured error),
    //              or omitted (nullptr) for re-throw inside a catch block.
    struct ThrowStmt : Stmt
    {
        ExprPtr value; // nullptr → bare throw (re-throw)
        explicit ThrowStmt(ExprPtr v, int ln = 0) : value(std::move(v)) { line = ln; }
    };

    // incase x : is 1 or 2 : ... ; belong int : ... ; bind v if v > 0 : ... ; else : ... ; ;
    enum class ClauseKind
    {
        IS_VALUE,     // is <value> [or <value> ...] [if guard] — value equality check
        BELONG_TYPE,  // belong <TypeName> [if guard]          — type/class check
        BIND_CAPTURE, // bind <name> [if guard]                — capture subject into name
    };

    struct InCaseClause
    {
        ClauseKind kind = ClauseKind::IS_VALUE;
        std::vector<ExprPtr> values; // IS_VALUE: one or more values joined by 'or'
        std::string typeName;        // BELONG_TYPE: the type name to match
        std::string bindName;        // BIND_CAPTURE / IS_VALUE: optional capture name
        ExprPtr guard;               // optional guard condition
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

    // Expression-mode incase: x = incase val : is 1 : "one" belong int : "int" bind v if v > 0 : v else : 0 ;
    struct InCaseExprClause
    {
        ClauseKind kind = ClauseKind::IS_VALUE;
        std::vector<ExprPtr> values; // IS_VALUE: one or more values joined by 'or'
        std::string typeName;        // BELONG_TYPE: the type name to match
        std::string bindName;        // BIND_CAPTURE: capture name
        ExprPtr guard;               // optional guard condition
        ExprPtr result;              // the result expression for this clause
        int line = 0;
    };

    struct InCaseExpr : Expr
    {
        ExprPtr subject;
        std::vector<InCaseExprClause> clauses;
        ExprPtr elseValue; // required for expression mode
        InCaseExpr(ExprPtr subj, std::vector<InCaseExprClause> clauses,
                   ExprPtr elseVal, int ln = 0)
            : subject(std::move(subj)), clauses(std::move(clauses)),
              elseValue(std::move(elseVal)) { line = ln; }
    };

    struct DestructuringPattern
    {
        enum class Kind
        {
            NAME,
            LIST,
            MAP,
            REST
        };

        struct MapEntry
        {
            std::string key;
            std::unique_ptr<DestructuringPattern> pattern;

            MapEntry(std::string key, std::unique_ptr<DestructuringPattern> pattern)
                : key(std::move(key)), pattern(std::move(pattern)) {}
        };

        Kind kind;
        std::string name;
        std::vector<std::unique_ptr<DestructuringPattern>> elements;
        std::vector<MapEntry> entries;

        explicit DestructuringPattern(Kind kind, std::string name = "")
            : kind(kind), name(std::move(name)) {}
    };

    // Destructuring assignment: a, b = [1, 2], [a, [b, c], ...rest] = value,
    // {x, y: alias} = point
    struct DestructuringAssignment : Stmt
    {
        std::unique_ptr<DestructuringPattern> pattern;
        std::vector<std::string> names;
        ExprPtr value;
        DestructuringAssignment(std::unique_ptr<DestructuringPattern> pattern,
                                std::vector<std::string> names,
                                ExprPtr val, int ln = 0)
            : pattern(std::move(pattern)), names(std::move(names)),
              value(std::move(val)) { line = ln; }
    };

    // Bring statement — unified module import system
    // Supports: bring X of module, bring * of module, bring X from "file",
    //           from "dir" bring ..., and chaining, as aliases, @eager
    struct BringPart
    {
        bool bringAll = false;               // bring * of ...
        std::vector<std::string> items;      // specific items to bring
        std::vector<std::string> modulePath; // module path: ["lib", "math_lib"]
        bool hasModulePath = false;          // true if "of PATH" was specified
        std::string filePath;                // from "file.xel" (empty if module-based)
    };

    struct BringStmt : Stmt
    {
        std::vector<BringPart> parts;     // chained with "and"
        std::vector<std::string> aliases; // as alias1, alias2 ...
        std::string fromDir;              // from "dir" (empty if not specified)
        bool isEager = false;             // @eager decorator applied
        BringStmt(std::vector<BringPart> parts,
                  std::vector<std::string> aliases,
                  std::string fromDir, int ln = 0)
            : parts(std::move(parts)), aliases(std::move(aliases)),
              fromDir(std::move(fromDir)) { line = ln; }
    };

    // Enum definition: enum Color: Red, Green, Blue;
    struct EnumDef : Stmt
    {
        std::string name;
        std::vector<std::string> members;  // member names
        std::vector<ExprPtr> memberValues; // optional custom values (nullptr = auto)
        EnumDef(std::string name, std::vector<std::string> members,
                std::vector<ExprPtr> values, int ln = 0)
            : name(std::move(name)), members(std::move(members)),
              memberValues(std::move(values)) { line = ln; }
    };

    // ---- OOP: Struct definition ----
    // struct Point : x = 0  y = 0  fn distance(self, other) : ... ; ;
    struct StructFieldDef
    {
        std::string name;
        ExprPtr defaultValue;
        int line = 0;
        AccessLevel access = AccessLevel::PUBLIC;
        bool isStatic = false;
    };

    struct StructDef : Stmt
    {
        std::string name;
        std::vector<StructFieldDef> fields;
        std::vector<std::unique_ptr<FnDef>> methods;
        StructDef(std::string name, std::vector<StructFieldDef> fields,
                  std::vector<std::unique_ptr<FnDef>> methods, int ln = 0)
            : name(std::move(name)), fields(std::move(fields)),
              methods(std::move(methods)) { line = ln; }
    };

    // ---- OOP: Property definition (get/set) ----
    struct PropertyDef
    {
        std::string name;
        std::unique_ptr<FnDef> getter; // get name(self) : ... ; (may be null)
        std::unique_ptr<FnDef> setter; // set name(self, val) : ... ; (may be null)
        int line = 0;
        AccessLevel access = AccessLevel::PUBLIC;
    };

    // ---- OOP: Interface definition ----
    // interface Drawable : fn draw(self) ; fn resize(self, factor) ; ;
    struct InterfaceMethodSig
    {
        std::string name;
        int paramCount; // number of params including self
        int line = 0;
    };

    struct InterfaceDef : Stmt
    {
        std::string name;
        std::vector<InterfaceMethodSig> methodSigs;
        InterfaceDef(std::string name, std::vector<InterfaceMethodSig> sigs, int ln = 0)
            : name(std::move(name)), methodSigs(std::move(sigs)) { line = ln; }
    };

    // ---- OOP: Class definition ----
    // class Animal [inherits Base1, Base2] [with Mixin1, Mixin2] [implements Iface1, Iface2] : fields + methods + __init__ ;
    // Also used for abstract classes: abstract Animal : ... ;
    struct ClassDef : Stmt
    {
        std::string name;
        std::vector<std::string> parents;    // inherits list (may be empty)
        std::vector<std::string> mixins;     // with list (may be empty)
        std::vector<std::string> interfaces; // implements list (may be empty)
        std::vector<StructFieldDef> fields;
        std::vector<std::unique_ptr<FnDef>> methods; // includes __init__ if present
        std::vector<PropertyDef> properties;         // get/set property definitions
        bool isAbstract = false;                     // true if defined with 'abstract' keyword
        bool isMixin = false;                        // true if defined with 'mixin' keyword
        ClassDef(std::string name, std::vector<std::string> parents,
                 std::vector<std::string> mixins,
                 std::vector<std::string> interfaces,
                 std::vector<StructFieldDef> fields,
                 std::vector<std::unique_ptr<FnDef>> methods,
                 std::vector<PropertyDef> properties, int ln = 0,
                 bool isAbstract = false, bool isMixin = false)
            : name(std::move(name)), parents(std::move(parents)),
              mixins(std::move(mixins)),
              interfaces(std::move(interfaces)),
              fields(std::move(fields)), methods(std::move(methods)),
              properties(std::move(properties)), isAbstract(isAbstract),
              isMixin(isMixin) { line = ln; }
    };

    // Member assignment: obj->field = expr
    // If augmentedOp is non-empty, this is an augmented assignment:
    //   obj->field += expr  ⇒  augmentedOp = "+"
    // The interpreter reads the current value, applies the op, then writes back.
    struct MemberAssignment : Stmt
    {
        ExprPtr object;          // the object expression (e.g., self, p1, a->b)
        std::string member;      // field name
        ExprPtr value;           // RHS expression
        std::string augmentedOp; // "", "+", "-", "*", "/", "%"
        MemberAssignment(ExprPtr obj, std::string mem, ExprPtr val, int ln = 0,
                         std::string augOp = "")
            : object(std::move(obj)), member(std::move(mem)), value(std::move(val)),
              augmentedOp(std::move(augOp)) { line = ln; }
    };

    // Index assignment: list[i] = expr  or  map[key] = expr
    // If augmentedOp is non-empty, this is an augmented assignment:
    //   list[i] += expr  ⇒  augmentedOp = "+"
    struct IndexAssignment : Stmt
    {
        ExprPtr object;          // the container
        ExprPtr index;           // the index/key
        ExprPtr value;           // RHS
        std::string augmentedOp; // "", "+", "-", "*", "/", "%"
        IndexAssignment(ExprPtr obj, ExprPtr idx, ExprPtr val, int ln = 0,
                        std::string augOp = "")
            : object(std::move(obj)), index(std::move(idx)), value(std::move(val)),
              augmentedOp(std::move(augOp)) { line = ln; }
    };

    // Decorated function: @decorator fn name(...): ... ;
    struct DecoratedFnDef : Stmt
    {
        std::vector<std::string> decorators; // decorator names (applied bottom-up)
        std::unique_ptr<FnDef> fnDef;
        DecoratedFnDef(std::vector<std::string> decorators, std::unique_ptr<FnDef> fn, int ln = 0)
            : decorators(std::move(decorators)), fnDef(std::move(fn)) { line = ln; }
    };

    // Decorated class: @decorator class Name : ... ;
    struct DecoratedClassDef : Stmt
    {
        std::vector<std::string> decorators; // decorator names (applied bottom-up)
        std::unique_ptr<ClassDef> classDef;
        DecoratedClassDef(std::vector<std::string> decorators, std::unique_ptr<ClassDef> cls, int ln = 0)
            : decorators(std::move(decorators)), classDef(std::move(cls)) { line = ln; }
    };

    // let EXPR be NAME, EXPR be NAME : BLOCK ;   (RAII / context manager)
    struct LetBinding
    {
        ExprPtr expr;     // resource expression (evaluated, then __enter__ called)
        std::string name; // binding name (receives __enter__ return value), "_" = discard
        int line = 0;
    };

    struct LetStmt : Stmt
    {
        std::vector<LetBinding> bindings; // one or more (expr, name) pairs
        std::vector<StmtPtr> body;        // block executed with resources in scope
        LetStmt(std::vector<LetBinding> bindings, std::vector<StmtPtr> body, int ln = 0)
            : bindings(std::move(bindings)), body(std::move(body)) { line = ln; }
    };

    // ============================================================
    // Module system AST nodes
    // ============================================================

    // module name : body ;
    struct ModuleDef : Stmt
    {
        std::string name;
        std::vector<StmtPtr> body;                       // entire module body
        std::vector<std::string> requires_;              // requires declarations (module names)
        std::vector<std::pair<std::vector<std::string>,  // requires items
                              std::vector<std::string>>> // requires paths (of X->Y)
            requiresItems;
        bool isExported = false; // true if preceded by export
        ModuleDef(std::string name, std::vector<StmtPtr> body, int ln = 0)
            : name(std::move(name)), body(std::move(body)) { line = ln; }
    };

    // ============================================================
    // Debug & Tracing decorators (Phase 5)
    // ============================================================

    // @debug on / @debug off — toggle section tracing
    struct DebugToggleStmt : Stmt
    {
        bool enable; // true = on, false = off
        DebugToggleStmt(bool on, int ln = 0) : enable(on) { line = ln; }
    };

    // @debug sample N — sampling mode for loops/calls
    struct DebugSampleStmt : Stmt
    {
        int sampleSize;
        DebugSampleStmt(int n, int ln = 0) : sampleSize(n) { line = ln; }
    };

    // @breakpoint("name") — snapshot (non-blocking)
    // @breakpoint pause — pause execution
    // @breakpoint pause N — pause for N seconds
    // @breakpoint("name") when EXPR — conditional snapshot
    struct BreakpointStmt : Stmt
    {
        std::string name;     // breakpoint label (may be empty)
        bool isPause = false; // true for @breakpoint pause
        int pauseSeconds = 0; // >0 for timed pause, 0 for Tab-resume
        ExprPtr condition;    // non-null for conditional breakpoint
        BreakpointStmt(int ln = 0) { line = ln; }
    };

    // @watch("expression") — alerts when expression becomes true
    struct WatchStmt : Stmt
    {
        std::string expression; // raw string of the watch expression
        ExprPtr parsed;         // parsed expression for evaluation
        WatchStmt(std::string expr, ExprPtr parsedExpr, int ln = 0)
            : expression(std::move(expr)), parsed(std::move(parsedExpr)) { line = ln; }
    };

    // @checkpoint("name") — full state save for time travel
    struct CheckpointStmt : Stmt
    {
        std::string name;
        CheckpointStmt(std::string n, int ln = 0) : name(std::move(n)) { line = ln; }
    };

    // @track var(x,y) fn(a,b) loop conditions — selective tracking
    struct TrackStmt : Stmt
    {
        std::vector<std::string> vars;
        std::vector<std::string> fns;
        std::vector<std::string> classes;
        std::vector<std::string> objs;       // @track obj(x,y) — instance tracking
        std::vector<std::string> categories; // "loop", "conditions", "scope", "perf", etc.
        bool isNotrack = false;              // true for @notrack
        TrackStmt(int ln = 0) { line = ln; }
    };

    // @profile fn myFunc — measure function execution time
    // @profile — profile the next statement
    struct ProfileStmt : Stmt
    {
        std::string targetFn; // function name (empty = profile next stmt)
        ProfileStmt(std::string fn, int ln = 0) : targetFn(std::move(fn)) { line = ln; }
    };

    // @log "message {var}" — always print
    // @log when EXPR "message {var}" — conditional log
    struct LogStmt : Stmt
    {
        std::string message; // raw message with {interpolation} placeholders
        ExprPtr condition;   // non-null for @log when EXPR
        LogStmt(std::string msg, ExprPtr cond, int ln = 0)
            : message(std::move(msg)), condition(std::move(cond)) { line = ln; }
    };

    // export fn/class/struct/var/module — wraps any declaration
    struct ExportDecl : Stmt
    {
        StmtPtr declaration; // the wrapped fn/class/struct/assignment/module
        ExportDecl(StmtPtr decl, int ln = 0)
            : declaration(std::move(decl)) { line = ln; }
    };

    // ============================================================
    // Top-level program
    // ============================================================

    struct Program
    {
        std::vector<StmtPtr> statements;
    };

} // namespace xell
