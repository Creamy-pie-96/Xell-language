# Xell AST — A Complete Beginner's Guide

> **What this document covers:** Every line of code in `ast.hpp` — the file that defines all the node types in Xell's Abstract Syntax Tree. By the time you finish reading this, you'll understand exactly how parsed Xell code is represented in memory.

---

## Table of Contents

1. [What Is an AST?](#1-what-is-an-ast)
2. [Why Do We Need an AST?](#2-why-do-we-need-an-ast)
3. [The Design of Xell's AST](#3-the-design-of-xells-ast)
4. [Walking Through `ast.hpp`](#4-walking-through-asthpp)
   - [Includes and Forward Declarations](#41-includes-and-forward-declarations)
   - [Base Classes: Expr and Stmt](#42-base-classes-expr-and-stmt)
   - [Expression Nodes](#43-expression-nodes)
   - [Statement Nodes](#44-statement-nodes)
   - [The Program Struct](#45-the-program-struct)
5. [How the AST Represents Real Code](#5-how-the-ast-represents-real-code)
6. [Key Concepts Summary](#6-key-concepts-summary)

---

## 1. What Is an AST?

An **Abstract Syntax Tree** (AST) is a tree-shaped data structure that represents the structure of your program. Think of it like a family tree, but for code.

Consider this Xell code:

```xell
x = 1 + 2 * 3
```

The lexer gives us flat tokens: `IDENTIFIER("x") EQUAL NUMBER("1") PLUS NUMBER("2") STAR NUMBER("3")`. But those tokens don't capture **structure** — which operations happen first?

The AST does:

```
Assignment
├── name: "x"
└── value: BinaryExpr(+)
            ├── left: NumberLiteral(1)
            └── right: BinaryExpr(*)
                        ├── left: NumberLiteral(2)
                        └── right: NumberLiteral(3)
```

Notice how `2 * 3` is nested deeper than `1 + ...`. This encodes that multiplication happens before addition — **operator precedence is baked into the tree structure**.

---

## 2. Why Do We Need an AST?

| Stage           | Input              | Output                                   |
| --------------- | ------------------ | ---------------------------------------- |
| **Lexer**       | Source code (text) | Flat list of tokens                      |
| **Parser**      | Tokens             | AST (tree structure)                     |
| **Interpreter** | AST                | Program execution (values, side effects) |

The AST is the **intermediate representation** that bridges the gap between text and execution:

- The **lexer** knows about characters and words, but not about meaning.
- The **parser** knows about grammar rules and builds the tree.
- The **interpreter** walks the tree and evaluates each node.

The AST is "abstract" because it throws away details that don't matter for execution:

- Parentheses → already encoded in the tree shape
- Comments → already discarded by the lexer
- Whitespace → irrelevant
- `:` and `;` delimiters → block boundaries are represented as child node lists

---

## 3. The Design of Xell's AST

Xell's AST uses **inheritance** with two base types:

```
         ┌──────────┐
         │   Expr   │  ← base class for all expressions
         └──────────┘
              ▲
    ┌─────────┼─────────────┬─────────────┐
NumberLiteral BinaryExpr  Identifier  CallExpr  ... etc

         ┌──────────┐
         │   Stmt   │  ← base class for all statements
         └──────────┘
              ▲
    ┌─────────┼─────────────┬─────────────┐
 Assignment  IfStmt     ForStmt    FnDef    ... etc
```

**Expressions** produce a value: `1 + 2`, `"hello"`, `foo()`  
**Statements** perform an action: `x = 10`, `if ... :`, `for ... in ... :`

The key insight: **Expr and Stmt are abstract base classes with a virtual destructor**. This allows us to store any kind of node using `unique_ptr<Expr>` or `unique_ptr<Stmt>`, and the interpreter can use `dynamic_cast` to figure out what kind of node it's looking at.

---

## 4. Walking Through `ast.hpp`

### 4.1 Includes and Forward Declarations

```cpp
#pragma once

#include <string>
#include <vector>
#include <memory>
#include <utility>
```

| Include     | What it gives us                                                    |
| ----------- | ------------------------------------------------------------------- |
| `<string>`  | `std::string` for identifiers, operators, paths                     |
| `<vector>`  | `std::vector` for lists of statements, arguments, etc.              |
| `<memory>`  | `std::unique_ptr` for owning pointers to AST nodes                  |
| `<utility>` | `std::move` and `std::pair` for efficient transfers and map entries |

```cpp
namespace xell {
```

Everything lives in the `xell` namespace.

```cpp
struct Expr;
struct Stmt;

using ExprPtr = std::unique_ptr<Expr>;
using StmtPtr = std::unique_ptr<Stmt>;
```

**Forward declarations** tell the compiler "these types exist, I'll define them later." This is needed because some node types reference each other (e.g., a `BinaryExpr` contains two `ExprPtr`s).

**Type aliases** make the code much cleaner:

- `ExprPtr` instead of `std::unique_ptr<Expr>` everywhere
- `StmtPtr` instead of `std::unique_ptr<Stmt>` everywhere

**Why `unique_ptr`?** Each parent node **owns** its children. When a parent is destroyed, all its children are automatically destroyed. No manual `delete`, no memory leaks. This is called **RAII** (Resource Acquisition Is Initialization).

### 4.2 Base Classes: Expr and Stmt

```cpp
struct Expr {
    int line = 0;
    virtual ~Expr() = default;
};

struct Stmt {
    int line = 0;
    virtual ~Stmt() = default;
};
```

Both base classes have:

1. **`line`** — the source line number where this node appeared. Used for error messages during interpretation.
2. **`virtual ~Expr() = default`** — a **virtual destructor**. This is essential when using inheritance with pointers.

**Why is the virtual destructor important?**

Without it, deleting a `unique_ptr<Expr>` that actually points to a `BinaryExpr` would only call `Expr`'s destructor — `BinaryExpr`'s destructor (which needs to free its child nodes) would never run. That's a memory leak. The `virtual` keyword ensures the correct destructor is called based on the actual type of the object.

**Why `struct` instead of `class`?**

In C++, `struct` and `class` are nearly identical. The only difference: `struct` members are `public` by default, `class` members are `private` by default. For simple data-holding types like AST nodes, `struct` is cleaner — we want everything public.

### 4.3 Expression Nodes

Each expression node represents one kind of value-producing code.

#### NumberLiteral

```cpp
struct NumberLiteral : Expr {
    double value;
    explicit NumberLiteral(double v, int ln = 0) : value(v) { line = ln; }
};
```

Represents: `42`, `3.14`, `100`

- Inherits from `Expr` (it's an expression — it produces a value)
- Stores the numeric `value` as a `double` (all Xell numbers are floating-point)
- `explicit` prevents accidental implicit construction

#### StringLiteral

```cpp
struct StringLiteral : Expr {
    std::string value;
    explicit StringLiteral(std::string v, int ln = 0) : value(std::move(v)) { line = ln; }
};
```

Represents: `"hello"`, `"Hello, {name}!"`

- The `value` stores the raw content with `{interpolation}` markers preserved as-is
- `std::move(v)` transfers the string's memory instead of copying it

#### BoolLiteral

```cpp
struct BoolLiteral : Expr {
    bool value;
    explicit BoolLiteral(bool v, int ln = 0) : value(v) { line = ln; }
};
```

Represents: `true`, `false`

#### NoneLiteral

```cpp
struct NoneLiteral : Expr {
    explicit NoneLiteral(int ln = 0) { line = ln; }
};
```

Represents: `none`

No value field needed — the presence of the node itself tells the interpreter "this is none."

#### Identifier

```cpp
struct Identifier : Expr {
    std::string name;
    explicit Identifier(std::string n, int ln = 0) : name(std::move(n)) { line = ln; }
};
```

Represents: `myVar`, `count`, `config`

An identifier is a name that refers to a variable. The interpreter will look up its value in the current scope at runtime.

#### ListLiteral

```cpp
struct ListLiteral : Expr {
    std::vector<ExprPtr> elements;
    explicit ListLiteral(std::vector<ExprPtr> elems, int ln = 0)
        : elements(std::move(elems)) { line = ln; }
};
```

Represents: `[1, 2, 3]`, `["hello", true, none]`, `[]`

`elements` is a vector of expression pointers — each element can be any kind of expression (number, string, even another list).

#### MapLiteral

```cpp
struct MapLiteral : Expr {
    std::vector<std::pair<std::string, ExprPtr>> entries;
    explicit MapLiteral(std::vector<std::pair<std::string, ExprPtr>> e, int ln = 0)
        : entries(std::move(e)) { line = ln; }
};
```

Represents: `{ host: "localhost", port: 3000 }`, `{}`

Each entry is a `pair<string, ExprPtr>` — a key (always a string/identifier) and a value (any expression).

#### BinaryExpr

```cpp
struct BinaryExpr : Expr {
    ExprPtr left;
    std::string op;  // normalized: +, -, *, /, ==, !=, >, <, >=, <=, and, or
    ExprPtr right;
    BinaryExpr(ExprPtr l, std::string o, ExprPtr r, int ln = 0)
        : left(std::move(l)), op(std::move(o)), right(std::move(r)) { line = ln; }
};
```

Represents: `1 + 2`, `x > 5`, `a and b`, `count == 10`

This is the **most common expression type**. It has:

- `left` — the expression on the left of the operator
- `op` — the operator as a normalized string
- `right` — the expression on the right

**Why "normalized"?** The parser converts keyword operators to their symbolic form:

- `is`, `eq` → `"=="`
- `ne` → `"!="`
- `gt` → `">"`
- etc.

This way, the interpreter only needs to handle one form of each operation.

#### UnaryExpr

```cpp
struct UnaryExpr : Expr {
    std::string op;  // "not", "!", "-", "++", "--"
    ExprPtr operand;
    UnaryExpr(std::string o, ExprPtr operand, int ln = 0)
        : op(std::move(o)), operand(std::move(operand)) { line = ln; }
};
```

Represents: `not ready`, `!ready`, `-5`, `++count`, `--index`

A unary expression has one operand. In Xell:

- `not` and `!` are both logical negation (the parser normalizes `!` to `"not"`)
- `-` is numeric negation
- `++` is prefix increment (before the operand: `++count`)
- `--` is prefix decrement (before the operand: `--index`)

#### PostfixExpr

```cpp
struct PostfixExpr : Expr {
    std::string op;  // "++" or "--"
    ExprPtr operand;
    PostfixExpr(std::string o, ExprPtr operand, int ln = 0)
        : op(std::move(o)), operand(std::move(operand)) { line = ln; }
};
```

Represents: `count++`, `index--`

**Why separate from UnaryExpr?** Because prefix and postfix `++`/`--` have different semantics:

- **Prefix** `++count` — increment, then return the new value
- **Postfix** `count++` — return the old value, then increment

Having separate node types makes it trivial for the interpreter to distinguish them.

#### CallExpr

```cpp
struct CallExpr : Expr {
    std::string callee;
    std::vector<ExprPtr> args;
    CallExpr(std::string callee, std::vector<ExprPtr> args, int ln = 0)
        : callee(std::move(callee)), args(std::move(args)) { line = ln; }
};
```

Represents: `greet("Alice")`, `add(1, 2)`, `print "hello"` (paren-less call)

- `callee` is the function name
- `args` is a list of argument expressions

Both `print("hello")` and `print "hello"` produce the same `CallExpr` node — the parser handles both syntaxes.

#### IndexAccess

```cpp
struct IndexAccess : Expr {
    ExprPtr object;
    ExprPtr index;
    IndexAccess(ExprPtr obj, ExprPtr idx, int ln = 0)
        : object(std::move(obj)), index(std::move(idx)) { line = ln; }
};
```

Represents: `ports[0]`, `config["host"]`

- `object` is the thing being indexed (a list or map)
- `index` is the expression inside the brackets

#### MemberAccess

```cpp
struct MemberAccess : Expr {
    ExprPtr object;
    std::string member;
    MemberAccess(ExprPtr obj, std::string mem, int ln = 0)
        : object(std::move(obj)), member(std::move(mem)) { line = ln; }
};
```

Represents: `config->host`, `user->name`

The `->` operator accesses a member of a map. `member` is stored as a plain string (not an expression) because map keys in Xell are always identifiers.

### 4.4 Statement Nodes

Statements perform actions but don't produce values.

#### ExprStmt

```cpp
struct ExprStmt : Stmt {
    ExprPtr expr;
    explicit ExprStmt(ExprPtr e, int ln = 0) : expr(std::move(e)) { line = ln; }
};
```

Represents: any expression used as a statement.

```xell
print "hello"      # CallExpr wrapped in ExprStmt
count++             # PostfixExpr wrapped in ExprStmt
42                  # NumberLiteral wrapped in ExprStmt (valid but useless)
```

When the parser encounters an expression that isn't part of an assignment, if-statement, etc., it wraps it in an `ExprStmt`.

#### Assignment

```cpp
struct Assignment : Stmt {
    std::string name;
    ExprPtr value;
    Assignment(std::string n, ExprPtr v, int ln = 0)
        : name(std::move(n)), value(std::move(v)) { line = ln; }
};
```

Represents: `x = 10`, `name = "xell"`, `result = add(1, 2)`

- `name` — the variable being assigned to
- `value` — the expression whose result is stored

#### ElifClause (Helper)

```cpp
struct ElifClause {
    ExprPtr condition;
    std::vector<StmtPtr> body;
    int line = 0;
};
```

This is NOT a statement — it's a helper struct used inside `IfStmt`. Each `elif` branch has its own condition and body.

#### IfStmt

```cpp
struct IfStmt : Stmt {
    ExprPtr condition;
    std::vector<StmtPtr> body;
    std::vector<ElifClause> elifs;
    std::vector<StmtPtr> elseBody;
    ...
};
```

Represents:

```xell
if score ge 90 :
    print "A"
;
elif score ge 75 :
    print "B"
;
else :
    print "F"
;
```

- `condition` — the `if` expression to evaluate
- `body` — statements inside the `if` block
- `elifs` — zero or more `elif` clauses
- `elseBody` — statements inside the `else` block (empty if no `else`)

#### ForStmt

```cpp
struct ForStmt : Stmt {
    std::string varName;
    ExprPtr iterable;
    std::vector<StmtPtr> body;
    ...
};
```

Represents:

```xell
for port in ports :
    print port
;
```

- `varName` — the loop variable name (`"port"`)
- `iterable` — the expression being iterated (`ports`)
- `body` — statements inside the loop

#### WhileStmt

```cpp
struct WhileStmt : Stmt {
    ExprPtr condition;
    std::vector<StmtPtr> body;
    ...
};
```

Represents:

```xell
while count lt 5 :
    count = count + 1
;
```

#### FnDef

```cpp
struct FnDef : Stmt {
    std::string name;
    std::vector<std::string> params;
    std::vector<StmtPtr> body;
    ...
};
```

Represents:

```xell
fn greet(name, greeting) :
    print "{greeting}, {name}!"
;
```

- `name` — the function name
- `params` — parameter names as strings
- `body` — the function's body statements

#### GiveStmt

```cpp
struct GiveStmt : Stmt {
    ExprPtr value;  // nullptr → give with no value (returns none)
    explicit GiveStmt(ExprPtr v, int ln = 0) : value(std::move(v)) { line = ln; }
};
```

Represents: `give a + b`, `give` (no value → returns none)

`give` is Xell's return keyword. If `value` is `nullptr`, it means the function returns `none`.

#### BringStmt

```cpp
struct BringStmt : Stmt {
    bool bringAll;                    // true for "bring * from ..."
    std::vector<std::string> names;   // names to bring
    std::string path;                 // file path
    std::vector<std::string> aliases; // optional aliases
    ...
};
```

Represents:

```xell
bring * from "./helpers.xel"
bring setup, deploy from "./helpers.xel" as s, d
```

- `bringAll` — if true, import everything (`bring *`)
- `names` — specific names to import (empty if `bringAll`)
- `path` — the file path string
- `aliases` — optional rename aliases (positional mapping: first alias → first name)

### 4.5 The Program Struct

```cpp
struct Program {
    std::vector<StmtPtr> statements;
};
```

The top-level container. A Xell program is simply a list of statements.

```
Program
├── Assignment(name, "xell")
├── Assignment(ports, [3000, 8080])
├── FnDef(setup, [project], [...])
├── IfStmt(condition, [...], [...], [...])
├── ForStmt(port, ports, [...])
└── ExprStmt(CallExpr(print, result))
```

---

## 5. How the AST Represents Real Code

Let's trace a complete example:

```xell
fn add(a, b) :
    give a + b
;

result = add(10, 20)
print result
```

The AST looks like:

```
Program
├── FnDef
│   ├── name: "add"
│   ├── params: ["a", "b"]
│   └── body:
│       └── GiveStmt
│           └── value: BinaryExpr("+")
│                       ├── left: Identifier("a")
│                       └── right: Identifier("b")
│
├── Assignment
│   ├── name: "result"
│   └── value: CallExpr
│               ├── callee: "add"
│               └── args:
│                   ├── NumberLiteral(10)
│                   └── NumberLiteral(20)
│
└── ExprStmt
    └── expr: CallExpr
              ├── callee: "print"
              └── args:
                  └── Identifier("result")
```

Notice how `print result` (paren-less call) produces the same `CallExpr` structure as `print(result)` would.

---

## 6. Key Concepts Summary

### Inheritance for Node Types

All expressions inherit from `Expr`, all statements from `Stmt`. This lets us store heterogeneous collections (a function body can contain any mix of statement types) using `vector<StmtPtr>`.

### Ownership with `unique_ptr`

Every parent owns its children. When a node is destroyed, all its subtrees are automatically freed. No manual memory management needed.

### `std::move` Everywhere

AST nodes are built by moving data into them, not copying. This is critical for performance — an `ExprPtr` can't be copied (it's unique), it can only be moved.

### Expressions vs Statements

The distinction matters for the interpreter:

- **Expressions** are evaluated → they return a value
- **Statements** are executed → they cause side effects (printing, assigning, etc.)
- An expression can be wrapped in `ExprStmt` to be used as a statement

### Line Numbers for Error Reporting

Every node stores its source line number. When the interpreter encounters a runtime error (division by zero, undefined variable, etc.), it can point to the exact line.

---

**Congratulations!** You now understand every line of the Xell AST. The next step is understanding the parser — see `parser_guide.md`.
