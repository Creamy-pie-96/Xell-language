# Xell Parser — A Complete Beginner's Guide

> **What this document covers:** Every line of code in `parser.hpp` and `parser.cpp` — the two files that make up Xell's recursive descent parser. By the time you finish reading this, you'll understand exactly how a flat list of tokens is transformed into an Abstract Syntax Tree.

---

## Table of Contents

1. [What Is a Parser?](#1-what-is-a-parser)
2. [Recursive Descent Parsing](#2-recursive-descent-parsing)
3. [Operator Precedence Climbing](#3-operator-precedence-climbing)
4. [File 1: `parser.hpp` — The Parser Declaration](#4-file-1-parserhpp--the-parser-declaration)
5. [File 2: `parser.cpp` — The Parser Implementation](#5-file-2-parsercpp--the-parser-implementation)
   - [Constructor & Token Navigation](#51-constructor--token-navigation)
   - [Top-Level Parse & Blocks](#52-top-level-parse--blocks)
   - [Statement Parsing](#53-statement-parsing)
   - [Expression Parsing (Precedence Climbing)](#54-expression-parsing-precedence-climbing)
   - [Primary Expressions](#55-primary-expressions)
   - [Postfix Operations](#56-postfix-operations)
   - [Helpers: Lists, Maps, Arguments](#57-helpers-lists-maps-arguments)
6. [Paren-Less Function Calls](#6-paren-less-function-calls)
7. [How It All Fits Together](#7-how-it-all-fits-together)
8. [Key Concepts Summary](#8-key-concepts-summary)

---

## 1. What Is a Parser?

The **parser** takes the flat list of tokens from the lexer and builds a tree structure (AST) that represents the program's meaning.

```
Tokens:  [IF, IDENTIFIER("count"), GT, NUMBER("5"), COLON, ...]

    ↓  Parser  ↓

AST:    IfStmt
        ├── condition: BinaryExpr(">")
        │               ├── left: Identifier("count")
        │               └── right: NumberLiteral(5)
        └── body: [...]
```

The parser enforces **grammar rules**: it knows that `if` must be followed by a condition, then `:`, then a block, then `;`. If the tokens don't follow these rules, it throws a `ParseError`.

---

## 2. Recursive Descent Parsing

Xell uses a **recursive descent parser**. This is the simplest and most intuitive parsing technique:

- Each grammar rule becomes a **function** (or method)
- To parse a rule, you call the function for that rule
- Functions can call each other (hence "recursive")
- You always move forward through the tokens (hence "descent")

For example:

- `parseStatement()` might call `parseIfStmt()`
- `parseIfStmt()` calls `parseExpression()` for the condition
- `parseExpression()` calls `parseLogicalOr()`
- `parseLogicalOr()` calls `parseLogicalAnd()`
- ... and so on down the precedence chain

This maps directly to the grammar:

```
STATEMENT    → IF_STMT | FOR_STMT | WHILE_STMT | ...
IF_STMT      → "if" EXPRESSION ":" BLOCK ";"
EXPRESSION   → LOGICAL_OR
LOGICAL_OR   → LOGICAL_AND { "or" LOGICAL_AND }
LOGICAL_AND  → EQUALITY { "and" EQUALITY }
...
```

Each grammar rule = one function. It's that simple.

---

## 3. Operator Precedence Climbing

The trickiest part of any parser is handling **operator precedence** — making sure `1 + 2 * 3` is parsed as `1 + (2 * 3)` and not `(1 + 2) * 3`.

Xell achieves this by having a **chain of functions**, each handling one precedence level:

```
parseExpression()        → entry point
  └─ parseLogicalOr()    → lowest precedence: or
       └─ parseLogicalAnd()  → and
            └─ parseEquality()    → ==, !=, is, eq, ne
                 └─ parseComparison()  → >, <, >=, <=, gt, lt, ge, le
                      └─ parseAddition()    → +, -
                           └─ parseMultiplication()  → *, /
                                └─ parseUnary()    → not, !, -, ++, --
                                     └─ parsePrimary()  → highest: literals, identifiers, parens
                                          └─ parsePostfix()  → [], ->, x++, x--
```

Each function:

1. Calls the **next higher precedence** function to get the left operand
2. Checks if the current token is an operator at **its own** precedence level
3. If yes: calls the next higher precedence function again for the right operand, then wraps both in a `BinaryExpr`
4. Repeats step 2–3 for left-associativity

This naturally produces the correct tree structure. Higher-precedence operators bind tighter because they're called deeper in the recursion.

---

## 4. File 1: `parser.hpp` — The Parser Declaration

### 4.1 Includes

```cpp
#pragma once
#include "ast.hpp"
#include "../lexer/token.hpp"
#include <vector>
#include <string>
#include <stdexcept>
```

The parser needs:

- `ast.hpp` — to build AST nodes
- `token.hpp` — to read token types
- `<stdexcept>` — for `std::runtime_error` (base class of `ParseError`)

### 4.2 ParseError Exception

```cpp
class ParseError : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};
```

A custom exception type for parser errors. `using std::runtime_error::runtime_error` inherits the parent's constructors — so `ParseError("message")` works automatically.

**Why a custom type?** So calling code can catch `ParseError` specifically:

```cpp
try { parser.parse(); }
catch (ParseError& e) { /* syntax error */ }
catch (std::exception& e) { /* other error */ }
```

### 4.3 The Parser Class

```cpp
class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens);
    Program parse();
```

**Public interface:**

1. **Constructor** — takes the token list from the lexer
2. **`parse()`** — converts all tokens into a `Program` (the root AST node)

### 4.4 Private State

```cpp
private:
    std::vector<Token> tokens_;
    size_t pos_;
```

- `tokens_` — the flat list of tokens to parse
- `pos_` — current position in the token list (starts at 0)

### 4.5 Token Navigation Methods

```cpp
const Token& current() const;       // what token are we on?
const Token& peekToken(int offset = 1) const;  // look ahead
bool check(TokenType type) const;    // is current token this type?
bool isAtEnd() const;                // are we at EOF?
Token advance();                     // consume current, return it, move forward
Token consume(TokenType type, ...);  // assert current is this type, then advance
void skipNewlines();                 // skip any NEWLINE tokens
void consumeStatementEnd();          // consume optional DOT terminator
```

These are the parser's "hands" — they reach into the token stream.

### 4.6 Statement and Expression Parsers

```cpp
// Statements
StmtPtr parseStatement();
StmtPtr parseAssignment(const std::string& name, int line);
StmtPtr parseIfStmt();
StmtPtr parseForStmt();
StmtPtr parseWhileStmt();
StmtPtr parseFnDef();
StmtPtr parseGiveStmt();
StmtPtr parseBringStmt();
std::vector<StmtPtr> parseBlock();

// Expressions (precedence climbing)
ExprPtr parseExpression();
ExprPtr parseLogicalOr();
ExprPtr parseLogicalAnd();
ExprPtr parseEquality();
ExprPtr parseComparison();
ExprPtr parseAddition();
ExprPtr parseMultiplication();
ExprPtr parseUnary();
ExprPtr parsePrimary();
ExprPtr parsePostfix(ExprPtr expr);
```

Each function corresponds to a grammar rule. The expression functions form the precedence chain.

```cpp
bool canStartPrimary(TokenType type) const;
```

This helper tells us whether a token type can start a new primary expression. It's used for detecting paren-less function call arguments.

---

## 5. File 2: `parser.cpp` — The Parser Implementation

### 5.1 Constructor & Token Navigation

```cpp
Parser::Parser(const std::vector<Token>& tokens)
    : tokens_(tokens), pos_(0) {}
```

Store the tokens and start at position 0.

```cpp
const Token& Parser::current() const {
    return tokens_[pos_];
}
```

Return the token we're currently looking at. We never go past the last token (EOF), so this is always safe.

```cpp
const Token& Parser::peekToken(int offset) const {
    size_t idx = pos_ + offset;
    if (idx >= tokens_.size()) return tokens_.back();  // EOF
    return tokens_[idx];
}
```

Look ahead by `offset` positions. If we'd go past the end, return the EOF token (a safe fallback).

```cpp
bool Parser::check(TokenType type) const {
    return current().type == type;
}
```

"Is the current token of this type?" This is used everywhere for conditional branching.

```cpp
bool Parser::isAtEnd() const {
    return current().type == TokenType::EOF_TOKEN;
}
```

```cpp
Token Parser::advance() {
    Token tok = current();
    if (!isAtEnd()) pos_++;
    return tok;
}
```

Consume the current token and move forward. Returns the consumed token so callers can use its value.

```cpp
Token Parser::consume(TokenType type, const std::string& errorMsg) {
    if (check(type)) return advance();

    std::string msg = errorMsg.empty()
        ? "Expected " + tokenTypeToString(type) + " but got " +
          tokenTypeToString(current().type) + " ('" + current().value + "')"
        : errorMsg;
    throw ParseError("[XELL ERROR] Line " + std::to_string(current().line) + " — " + msg);
}
```

**The most important method in the parser.** It says: "I expect the current token to be of this type. If it is, great — consume it and move on. If not, throw a parse error with a helpful message."

This is how the parser enforces grammar rules. When parsing `if EXPR : BLOCK ;`, we call:

- `consume(COLON, "Expected ':' after if condition")`
- Later: `consume(SEMICOLON, "Expected ';' to close if block")`

If the programmer forgot the `:`, they get a clear error message.

```cpp
void Parser::skipNewlines() {
    while (check(TokenType::NEWLINE)) advance();
}
```

Skip any consecutive newlines. Used between statements and at the start of blocks.

```cpp
void Parser::consumeStatementEnd() {
    if (check(TokenType::DOT)) advance();
}
```

In Xell, a `.` is an optional statement terminator. If present, consume it. If not, that's fine too.

```cpp
bool Parser::canStartPrimary(TokenType type) const {
    return type == TokenType::NUMBER || type == TokenType::STRING ||
           type == TokenType::TRUE_KW || type == TokenType::FALSE_KW ||
           type == TokenType::NONE_KW || type == TokenType::IDENTIFIER ||
           type == TokenType::LPAREN || type == TokenType::LBRACKET ||
           type == TokenType::LBRACE || type == TokenType::NOT ||
           type == TokenType::BANG || type == TokenType::MINUS ||
           type == TokenType::PLUS_PLUS || type == TokenType::MINUS_MINUS;
}
```

This lists every token type that could be the start of an expression. Used to detect paren-less call arguments and to know when expression parsing should stop.

### 5.2 Top-Level Parse & Blocks

```cpp
Program Parser::parse() {
    Program program;
    skipNewlines();
    while (!isAtEnd()) {
        program.statements.push_back(parseStatement());
        skipNewlines();
    }
    return program;
}
```

The entry point. Keep parsing statements until we hit EOF. Newlines between statements are skipped.

```cpp
std::vector<StmtPtr> Parser::parseBlock() {
    std::vector<StmtPtr> stmts;
    skipNewlines();
    while (!check(TokenType::SEMICOLON) && !isAtEnd()) {
        stmts.push_back(parseStatement());
        skipNewlines();
    }
    return stmts;
}
```

A **block** is a sequence of statements between `:` and `;`. This function collects statements until it sees `;` (or EOF). The `:` is consumed by the caller before this function, and the `;` is consumed by the caller after.

### 5.3 Statement Parsing

```cpp
StmtPtr Parser::parseStatement() {
    skipNewlines();
    TokenType type = current().type;

    // --- Keyword statements ---
    if (type == TokenType::IF)    return parseIfStmt();
    if (type == TokenType::FOR)   return parseForStmt();
    if (type == TokenType::WHILE) return parseWhileStmt();
    if (type == TokenType::FN)    return parseFnDef();
    if (type == TokenType::GIVE)  return parseGiveStmt();
    if (type == TokenType::BRING) return parseBringStmt();
```

First, check if the current token is a keyword that starts a specific statement type. This is a **top-down dispatch**: look at the first token and decide which parsing function to call.

```cpp
    // --- Assignment: IDENTIFIER = EXPR ---
    if (type == TokenType::IDENTIFIER && peekToken(1).type == TokenType::EQUAL) {
        std::string name = current().value;
        int ln = current().line;
        advance(); // consume identifier
        advance(); // consume =
        return parseAssignment(name, ln);
    }
```

If we see `IDENTIFIER =`, it's an assignment. We peek ahead one token to check for `=` without consuming anything.

**Why not just try parsing an expression?** Because `x = 10` starts with an identifier, and so does `x + 1`. We need to **look ahead** to distinguish assignment from expression.

```cpp
    // --- Expression statement (including paren-less calls) ---
    int ln = current().line;
    ExprPtr expr = parseExpression();
```

If it's not a keyword or assignment, parse it as an expression.

```cpp
    // Check for paren-less function call
    if (auto* ident = dynamic_cast<Identifier*>(expr.get())) {
        if (!check(TokenType::NEWLINE) && !check(TokenType::DOT) &&
            !check(TokenType::SEMICOLON) && !isAtEnd() &&
            canStartPrimary(current().type)) {

            std::string callee = ident->name;
            std::vector<ExprPtr> args;

            while (!check(TokenType::NEWLINE) && !check(TokenType::DOT) &&
                   !check(TokenType::SEMICOLON) && !isAtEnd() &&
                   canStartPrimary(current().type)) {
                args.push_back(parsePrimary());
            }

            expr = std::make_unique<CallExpr>(callee, std::move(args), ln);
        }
    }
```

**This is one of the most interesting parts of the parser.** After parsing the first expression, if it turned out to be a bare `Identifier` AND the next token can start a new expression (but isn't a statement boundary), we interpret the whole thing as a paren-less function call.

Example: `print "hello" "world"` → parsed as `CallExpr("print", [StringLiteral("hello"), StringLiteral("world")])`

The `dynamic_cast<Identifier*>` checks if the expression is a simple identifier (not a complex expression like `a + b`). Only bare identifiers can be paren-less call targets.

#### Parsing If/Elif/Else

```cpp
StmtPtr Parser::parseIfStmt() {
    int ln = current().line;
    advance(); // consume IF

    ExprPtr condition = parseExpression();
    consume(TokenType::COLON, "Expected ':' after if condition");
    auto body = parseBlock();
    consume(TokenType::SEMICOLON, "Expected ';' to close if block");
```

Parse the condition, expect `:`, parse the block, expect `;`.

```cpp
    // Parse elif clauses
    std::vector<ElifClause> elifs;
    skipNewlines();
    while (check(TokenType::ELIF)) {
        int elifLine = current().line;
        advance();
        ExprPtr elifCond = parseExpression();
        consume(TokenType::COLON, "...");
        auto elifBody = parseBlock();
        consume(TokenType::SEMICOLON, "...");

        ElifClause clause;
        clause.condition = std::move(elifCond);
        clause.body = std::move(elifBody);
        clause.line = elifLine;
        elifs.push_back(std::move(clause));
        skipNewlines();
    }
```

After the `if` block, keep looking for `elif` blocks. Each one gets its own condition and body.

```cpp
    // Parse else clause
    std::vector<StmtPtr> elseBody;
    if (check(TokenType::ELSE)) {
        advance();
        consume(TokenType::COLON, "Expected ':' after else");
        elseBody = parseBlock();
        consume(TokenType::SEMICOLON, "Expected ';' to close else block");
    }

    return std::make_unique<IfStmt>(
        std::move(condition), std::move(body),
        std::move(elifs), std::move(elseBody), ln);
}
```

Optionally parse an `else` block. Then build the final `IfStmt` node.

#### Parsing For Loop

```cpp
StmtPtr Parser::parseForStmt() {
    int ln = current().line;
    advance(); // consume FOR

    std::string varName = consume(TokenType::IDENTIFIER, "...").value;
    consume(TokenType::IN, "Expected 'in' after for loop variable");
    ExprPtr iterable = parseExpression();
    consume(TokenType::COLON, "Expected ':' after for expression");
    auto body = parseBlock();
    consume(TokenType::SEMICOLON, "Expected ';' to close for block");

    return std::make_unique<ForStmt>(varName, std::move(iterable), std::move(body), ln);
}
```

Pattern: `for IDENTIFIER in EXPRESSION : BLOCK ;`

The `consume()` calls enforce each required element of the syntax.

#### Parsing While Loop

```cpp
StmtPtr Parser::parseWhileStmt() {
    int ln = current().line;
    advance(); // consume WHILE

    ExprPtr condition = parseExpression();
    consume(TokenType::COLON, "...");
    auto body = parseBlock();
    consume(TokenType::SEMICOLON, "...");

    return std::make_unique<WhileStmt>(std::move(condition), std::move(body), ln);
}
```

Same pattern: `while EXPRESSION : BLOCK ;`

#### Parsing Function Definition

```cpp
StmtPtr Parser::parseFnDef() {
    int ln = current().line;
    advance(); // consume FN

    std::string name = consume(TokenType::IDENTIFIER, "Expected function name after 'fn'").value;
    consume(TokenType::LPAREN, "Expected '(' after function name");

    std::vector<std::string> params;
    if (!check(TokenType::RPAREN)) {
        params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name").value);
        while (check(TokenType::COMMA)) {
            advance(); // consume comma
            params.push_back(consume(TokenType::IDENTIFIER, "...").value);
        }
    }
    consume(TokenType::RPAREN, "Expected ')' after parameters");

    consume(TokenType::COLON, "Expected ':' after function signature");
    auto body = parseBlock();
    consume(TokenType::SEMICOLON, "Expected ';' to close function body");

    return std::make_unique<FnDef>(name, std::move(params), std::move(body), ln);
}
```

Pattern: `fn NAME ( PARAMS ) : BLOCK ;`

Parameters are comma-separated identifiers inside parentheses. The loop handles zero, one, or many parameters.

#### Parsing Give Statement

```cpp
StmtPtr Parser::parseGiveStmt() {
    int ln = current().line;
    advance(); // consume GIVE

    ExprPtr value = nullptr;
    if (!check(TokenType::NEWLINE) && !check(TokenType::DOT) &&
        !check(TokenType::SEMICOLON) && !isAtEnd()) {
        value = parseExpression();
    }

    consumeStatementEnd();
    return std::make_unique<GiveStmt>(std::move(value), ln);
}
```

`give` can appear with or without a value:

- `give a + b` → returns the expression's value
- `give` → returns none (value is nullptr)

The check ensures we don't try to parse a newline or semicolon as an expression.

#### Parsing Bring Statement

```cpp
StmtPtr Parser::parseBringStmt() {
    int ln = current().line;
    advance(); // consume BRING

    bool bringAll = false;
    std::vector<std::string> names;

    if (check(TokenType::STAR)) {
        bringAll = true;
        advance();
    } else {
        names.push_back(consume(TokenType::IDENTIFIER, "...").value);
        while (check(TokenType::COMMA)) {
            advance();
            names.push_back(consume(TokenType::IDENTIFIER, "...").value);
        }
    }

    consume(TokenType::FROM, "Expected 'from' in bring statement");
    std::string path = consume(TokenType::STRING, "Expected file path string after 'from'").value;

    std::vector<std::string> aliases;
    if (check(TokenType::AS)) {
        advance();
        aliases.push_back(consume(TokenType::IDENTIFIER, "...").value);
        while (check(TokenType::COMMA)) {
            advance();
            aliases.push_back(consume(TokenType::IDENTIFIER, "...").value);
        }
    }

    consumeStatementEnd();
    return std::make_unique<BringStmt>(bringAll, std::move(names), path, std::move(aliases), ln);
}
```

Handles all forms:

- `bring * from "file.xel"`
- `bring setup from "file.xel"`
- `bring setup, deploy from "file.xel" as s, d`

### 5.4 Expression Parsing (Precedence Climbing)

This is the heart of the parser. Each function handles one precedence level.

#### Entry Point

```cpp
ExprPtr Parser::parseExpression() {
    return parseLogicalOr();
}
```

Starts at the lowest precedence. The chain will naturally handle higher precedence operations deeper in the recursion.

#### Logical OR (lowest precedence)

```cpp
ExprPtr Parser::parseLogicalOr() {
    auto left = parseLogicalAnd();
    while (check(TokenType::OR)) {
        int ln = current().line;
        advance();
        auto right = parseLogicalAnd();
        left = std::make_unique<BinaryExpr>(std::move(left), "or", std::move(right), ln);
    }
    return left;
}
```

**The pattern for all binary operators:**

1. Parse the left operand by calling the **next higher** precedence function
2. While the current token is an operator at this level:
   a. Consume the operator
   b. Parse the right operand (same higher function)
   c. Wrap both in a `BinaryExpr`
   d. The result becomes the new "left" for the next iteration

The `while` loop handles **left associativity**: `a or b or c` becomes `(a or b) or c`.

#### Logical AND

```cpp
ExprPtr Parser::parseLogicalAnd() {
    auto left = parseEquality();
    while (check(TokenType::AND)) {
        int ln = current().line;
        advance();
        auto right = parseEquality();
        left = std::make_unique<BinaryExpr>(std::move(left), "and", std::move(right), ln);
    }
    return left;
}
```

Same pattern. AND binds tighter than OR, so it calls `parseEquality()` for its operands.

#### Equality

```cpp
ExprPtr Parser::parseEquality() {
    auto left = parseComparison();
    while (check(TokenType::EQUAL_EQUAL) || check(TokenType::BANG_EQUAL) ||
           check(TokenType::IS) || check(TokenType::EQ) || check(TokenType::NE)) {
        int ln = current().line;
        TokenType opType = current().type;
        advance();

        std::string op;
        if (opType == TokenType::EQUAL_EQUAL || opType == TokenType::IS || opType == TokenType::EQ)
            op = "==";
        else
            op = "!=";

        auto right = parseComparison();
        left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), ln);
    }
    return left;
}
```

**Normalization in action:** `is`, `eq`, and `==` all become `"=="`. `ne` and `!=` both become `"!="`. The interpreter only needs to handle the symbolic forms.

#### Comparison

```cpp
ExprPtr Parser::parseComparison() {
    auto left = parseAddition();
    while (check(TokenType::GREATER) || check(TokenType::LESS) ||
           check(TokenType::GREATER_EQUAL) || check(TokenType::LESS_EQUAL) ||
           check(TokenType::GT) || check(TokenType::LT) ||
           check(TokenType::GE) || check(TokenType::LE)) {
        int ln = current().line;
        TokenType opType = current().type;
        advance();

        std::string op;
        if (opType == TokenType::GREATER || opType == TokenType::GT) op = ">";
        else if (opType == TokenType::LESS || opType == TokenType::LT) op = "<";
        else if (opType == TokenType::GREATER_EQUAL || opType == TokenType::GE) op = ">=";
        else op = "<=";

        auto right = parseAddition();
        left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), ln);
    }
    return left;
}
```

Same pattern again: keyword comparisons are normalized to symbolic forms.

#### Addition & Multiplication

```cpp
ExprPtr Parser::parseAddition() {
    auto left = parseMultiplication();
    while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
        int ln = current().line;
        std::string op = current().value;
        advance();
        auto right = parseMultiplication();
        left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), ln);
    }
    return left;
}

ExprPtr Parser::parseMultiplication() {
    auto left = parseUnary();
    while (check(TokenType::STAR) || check(TokenType::SLASH)) {
        int ln = current().line;
        std::string op = current().value;
        advance();
        auto right = parseUnary();
        left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), ln);
    }
    return left;
}
```

These are straightforward — `*` and `/` bind tighter than `+` and `-` because `parseMultiplication()` is called deeper in the chain.

#### Unary Expressions

```cpp
ExprPtr Parser::parseUnary() {
    // 'not' or '!' — logical negation
    if (check(TokenType::NOT) || check(TokenType::BANG)) {
        int ln = current().line;
        advance();
        auto operand = parseUnary();  // recursive! handles !!x or not not x
        return std::make_unique<UnaryExpr>("not", std::move(operand), ln);
    }

    // unary minus
    if (check(TokenType::MINUS)) {
        int ln = current().line;
        advance();
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>("-", std::move(operand), ln);
    }

    // prefix ++ and --
    if (check(TokenType::PLUS_PLUS)) {
        int ln = current().line;
        advance();
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>("++", std::move(operand), ln);
    }
    if (check(TokenType::MINUS_MINUS)) {
        int ln = current().line;
        advance();
        auto operand = parseUnary();
        return std::make_unique<UnaryExpr>("--", std::move(operand), ln);
    }

    return parsePrimary();
}
```

Key observations:

1. **`!` is normalized to `"not"`** — the parser treats `!ready` and `not ready` identically.
2. **Unary operators are right-recursive:** `parseUnary()` calls itself. This handles chains like `!!x` (which becomes `not (not x)`).
3. **Prefix `++`/`--`** are handled here (before the operand). Postfix `++`/`--` are handled in `parsePostfix()` (after the operand).

### 5.5 Primary Expressions

```cpp
ExprPtr Parser::parsePrimary() {
    int ln = current().line;

    // Number literal
    if (check(TokenType::NUMBER)) {
        double val = std::stod(current().value);
        advance();
        return parsePostfix(std::make_unique<NumberLiteral>(val, ln));
    }

    // String literal
    if (check(TokenType::STRING)) {
        std::string val = current().value;
        advance();
        return parsePostfix(std::make_unique<StringLiteral>(std::move(val), ln));
    }

    // Boolean literals
    if (check(TokenType::TRUE_KW)) { advance(); return parsePostfix(std::make_unique<BoolLiteral>(true, ln)); }
    if (check(TokenType::FALSE_KW)) { advance(); return parsePostfix(std::make_unique<BoolLiteral>(false, ln)); }

    // None literal
    if (check(TokenType::NONE_KW)) { advance(); return parsePostfix(std::make_unique<NoneLiteral>(ln)); }

    // List literal
    if (check(TokenType::LBRACKET)) { return parsePostfix(parseListLiteral()); }

    // Map literal
    if (check(TokenType::LBRACE)) { return parsePostfix(parseMapLiteral()); }

    // Grouped expression: ( expr )
    if (check(TokenType::LPAREN)) {
        advance();
        auto expr = parseExpression();
        consume(TokenType::RPAREN, "Expected ')' after grouped expression");
        return parsePostfix(std::move(expr));
    }
```

Each primary expression is simple to parse — check the token type, consume it, create the appropriate AST node.

**Notice:** every result goes through `parsePostfix()`. This is because any primary expression can be followed by postfix operations: `ports[0]`, `config->host`, `count++`.

```cpp
    // Identifier or function call
    if (check(TokenType::IDENTIFIER)) {
        std::string name = current().value;
        advance();

        // Function call with parens: name(args)
        if (check(TokenType::LPAREN)) {
            advance();
            auto args = parseArgList();
            consume(TokenType::RPAREN, "Expected ')'...");
            return parsePostfix(std::make_unique<CallExpr>(name, std::move(args), ln));
        }

        return parsePostfix(std::make_unique<Identifier>(std::move(name), ln));
    }
```

If we see an identifier followed by `(`, it's a function call. Otherwise it's a plain identifier.

```cpp
    throw ParseError("[XELL ERROR] Line " + std::to_string(current().line) +
                     " — Unexpected token: ...");
}
```

If nothing matched, the token can't start an expression — throw an error.

### 5.6 Postfix Operations

```cpp
ExprPtr Parser::parsePostfix(ExprPtr expr) {
    while (true) {
        int ln = current().line;

        // Index access: expr[index]
        if (check(TokenType::LBRACKET)) {
            advance();
            auto index = parseExpression();
            consume(TokenType::RBRACKET, "Expected ']' after index");
            expr = std::make_unique<IndexAccess>(std::move(expr), std::move(index), ln);
            continue;
        }

        // Member access: expr->member
        if (check(TokenType::ARROW)) {
            advance();
            std::string member = consume(TokenType::IDENTIFIER, "...").value;

            // Check for method call: expr->method(args)
            if (check(TokenType::LPAREN)) {
                advance();
                auto args = parseArgList();
                consume(TokenType::RPAREN, "...");
                args.insert(args.begin(), std::move(expr));
                expr = std::make_unique<CallExpr>(member, std::move(args), ln);
                continue;
            }

            expr = std::make_unique<MemberAccess>(std::move(expr), member, ln);
            continue;
        }

        // Postfix ++
        if (check(TokenType::PLUS_PLUS)) {
            advance();
            expr = std::make_unique<PostfixExpr>("++", std::move(expr), ln);
            continue;
        }

        // Postfix --
        if (check(TokenType::MINUS_MINUS)) {
            advance();
            expr = std::make_unique<PostfixExpr>("--", std::move(expr), ln);
            continue;
        }

        break;
    }
    return expr;
}
```

This function handles everything that comes **after** a primary expression:

1. **Index access**: `expr[index]` → wraps in `IndexAccess`
2. **Member access**: `expr->member` → wraps in `MemberAccess`
3. **Method call**: `expr->method(args)` → wraps in `CallExpr` with `expr` as the first argument
4. **Postfix `++`/`--`**: `expr++` → wraps in `PostfixExpr`

The `while (true)` loop allows **chaining**: `config->tags[0]` works by applying `->tags` first (making it a `MemberAccess`), then `[0]` on the result (making it an `IndexAccess`).

### 5.7 Helpers: Lists, Maps, Arguments

#### List Literal

```cpp
ExprPtr Parser::parseListLiteral() {
    int ln = current().line;
    advance(); // consume [

    std::vector<ExprPtr> elements;
    skipNewlines();
    if (!check(TokenType::RBRACKET)) {
        elements.push_back(parseExpression());
        while (check(TokenType::COMMA)) {
            advance();
            skipNewlines();
            if (check(TokenType::RBRACKET)) break;  // trailing comma OK
            elements.push_back(parseExpression());
        }
    }
    skipNewlines();
    consume(TokenType::RBRACKET, "Expected ']' to close list");
    return std::make_unique<ListLiteral>(std::move(elements), ln);
}
```

Parses `[expr, expr, ...]`. Handles:

- Empty lists: `[]`
- Trailing commas: `[1, 2,]`
- Multi-line lists (newlines are skipped because the lexer suppresses them inside brackets)

#### Map Literal

```cpp
ExprPtr Parser::parseMapLiteral() {
    int ln = current().line;
    advance(); // consume {

    std::vector<std::pair<std::string, ExprPtr>> entries;
    skipNewlines();
    if (!check(TokenType::RBRACE)) {
        std::string key = consume(TokenType::IDENTIFIER, "Expected map key").value;
        consume(TokenType::COLON, "Expected ':' after map key");
        skipNewlines();
        ExprPtr value = parseExpression();
        entries.emplace_back(key, std::move(value));

        while (check(TokenType::COMMA)) {
            advance();
            skipNewlines();
            if (check(TokenType::RBRACE)) break;
            key = consume(TokenType::IDENTIFIER, "...").value;
            consume(TokenType::COLON, "...");
            skipNewlines();
            value = parseExpression();
            entries.emplace_back(key, std::move(value));
        }
    }
    skipNewlines();
    consume(TokenType::RBRACE, "Expected '}' to close map");
    return std::make_unique<MapLiteral>(std::move(entries), ln);
}
```

Parses `{ key: value, key: value, ... }`. Keys must be identifiers (no quotes needed).

#### Argument List

```cpp
std::vector<ExprPtr> Parser::parseArgList() {
    std::vector<ExprPtr> args;
    skipNewlines();
    if (!check(TokenType::RPAREN)) {
        args.push_back(parseExpression());
        while (check(TokenType::COMMA)) {
            advance();
            skipNewlines();
            args.push_back(parseExpression());
        }
    }
    skipNewlines();
    return args;
}
```

Parses comma-separated expressions inside function call parentheses. Used by both `parsePrimary()` (for `foo(args)`) and `parsePostfix()` (for `expr->method(args)`).

---

## 6. Paren-Less Function Calls

One of Xell's unique features is that function calls don't require parentheses:

```xell
print "hello"           # same as print("hello")
copy "a.txt" "b.txt"   # same as copy("a.txt", "b.txt")
mkdir "output"          # same as mkdir("output")
```

Here's how the parser handles this:

1. Parse the expression normally → we get `Identifier("print")`
2. Notice the next token is `STRING("hello")` — it can start a primary expression
3. We're not at a statement boundary (no newline, dot, or semicolon)
4. Therefore: interpret `print` as a function name, and keep parsing primary expressions as arguments
5. Result: `CallExpr("print", [StringLiteral("hello")])`

The key check is `canStartPrimary(current().type)` — it tells us whether the next token could be the start of an argument expression.

**Important limitation:** paren-less calls only work when the function name is a bare identifier. You can't do `obj->method "arg"` without parens — you'd need `obj->method("arg")`.

---

## 7. How It All Fits Together

Let's trace through parsing this program:

```xell
x = 1 + 2 * 3
```

1. `parseStatement()` — sees `IDENTIFIER("x")` followed by `EQUAL` → assignment
2. `parseAssignment("x")` → calls `parseExpression()`
3. `parseExpression()` → `parseLogicalOr()` → `parseLogicalAnd()` → `parseEquality()` → `parseComparison()` → `parseAddition()`
4. `parseAddition()`:
   - Calls `parseMultiplication()` for left operand
   - `parseMultiplication()`:
     - Calls `parseUnary()` → `parsePrimary()` → sees `NUMBER("1")` → returns `NumberLiteral(1)`
     - No `*` or `/` follows → returns `NumberLiteral(1)`
   - Sees `+` → consumes it
   - Calls `parseMultiplication()` for right operand
   - `parseMultiplication()`:
     - Calls `parseUnary()` → `parsePrimary()` → sees `NUMBER("2")` → returns `NumberLiteral(2)`
     - Sees `*` → consumes it
     - Calls `parseUnary()` → `parsePrimary()` → sees `NUMBER("3")` → returns `NumberLiteral(3)`
     - Returns `BinaryExpr("*", NumberLiteral(2), NumberLiteral(3))`
   - Returns `BinaryExpr("+", NumberLiteral(1), BinaryExpr("*", NumberLiteral(2), NumberLiteral(3)))`

Final AST:

```
Assignment("x")
└── BinaryExpr("+")
    ├── NumberLiteral(1)
    └── BinaryExpr("*")
        ├── NumberLiteral(2)
        └── NumberLiteral(3)
```

**Precedence is correct:** multiplication is deeper in the tree than addition.

---

## 8. Key Concepts Summary

### Recursive Descent

One function per grammar rule. Functions call each other to handle nested structures. It's intuitive and maps directly to the grammar.

### Precedence Climbing

A chain of functions from lowest to highest precedence. Each function calls the next one up for its operands. This naturally produces correct AST trees.

### `consume()` Is the Grammar Police

Every required token is enforced with `consume()`. Missing a `:` after `if`? Missing a `;` to close a block? The parser catches it immediately with a clear error message.

### Normalization

The parser converts keyword operators (`is`, `eq`, `gt`, etc.) to their symbolic forms (`==`, `>`, etc.). It also normalizes `!` to `"not"`. This simplifies the interpreter by reducing the number of cases to handle.

### Postfix Chaining

All primary expressions go through `parsePostfix()`, which loops to handle any number of chained operations: `config->tags[0]->length()` is parsed correctly.

### Paren-Less Calls

A unique Xell feature that makes shell-like syntax natural. The parser detects when a bare identifier is followed by expression-starting tokens and treats it as a function call.

---

**Congratulations!** You now understand every line of the Xell parser. With the lexer, AST, and parser knowledge combined, you understand the entire front-end of the Xell language compiler.
