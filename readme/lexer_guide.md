# Xell Lexer — A Complete Beginner's Guide

> **What this document covers:** Every line of code in `token.hpp`, `lexer.hpp`, and `lexer.cpp` — the three files that make up Xell's lexer (tokenizer). By the time you finish reading this, you'll understand exactly how raw Xell source code is broken into tokens.

---

## Table of Contents

1. [What Is a Lexer?](#1-what-is-a-lexer)
2. [The Big Picture](#2-the-big-picture)
3. [File 1: `token.hpp` — Defining Token Types](#3-file-1-tokenhpp--defining-token-types)
4. [File 2: `lexer.hpp` — The Lexer Class Declaration](#4-file-2-lexerhpp--the-lexer-class-declaration)
5. [File 3: `lexer.cpp` — The Lexer Implementation](#5-file-3-lexercpp--the-lexer-implementation)
6. [How It All Fits Together](#6-how-it-all-fits-together)
7. [Common Patterns & Takeaways](#7-common-patterns--takeaways)

---

## 1. What Is a Lexer?

Imagine you're reading a sentence:

```
if count gt 5 : print "hello" ;
```

Your brain breaks that down into individual pieces: the word "if", the word "count", the word "gt", the number "5", a colon, the word "print", the string "hello", and a semicolon. Each piece has a **type** (keyword, identifier, number, string, punctuation) and a **value** (the actual text).

A **lexer** (also called a **tokenizer** or **scanner**) does exactly this for a programming language. It takes raw text and produces a flat list of **tokens** — small labeled chunks that the parser can then understand.

```
Input:  "if count gt 5 : print \"hello\" ;"

Output: [IF, IDENTIFIER("count"), GT, NUMBER("5"), COLON,
         IDENTIFIER("print"), STRING("hello"), SEMICOLON, EOF]
```

---

## 2. The Big Picture

The lexer has **three files**:

| File        | Purpose                                                                |
| ----------- | ---------------------------------------------------------------------- |
| `token.hpp` | **Defines** what a token is: its type (enum) and the `Token` struct    |
| `lexer.hpp` | **Declares** the `Lexer` class: its public interface and private state |
| `lexer.cpp` | **Implements** the lexer: the actual scanning logic                    |

The flow is simple:

```
Source code (string) → Lexer → vector<Token> → Parser
```

---

## 3. File 1: `token.hpp` — Defining Token Types

This file answers two questions: "What kinds of tokens exist?" and "What does a single token look like?"

### 3.1 The Header Guard

```cpp
#pragma once
```

This tells the compiler: "Only include this file once, even if it's `#include`-ed in multiple places." Without this, you'd get duplicate definition errors.

### 3.2 Includes

```cpp
#include <string>
#include <unordered_map>
```

- `<string>` gives us `std::string` for token values and names.
- `<unordered_map>` gives us the hash map we use to convert token types to human-readable strings.

### 3.3 The Namespace

```cpp
namespace xell {
```

Everything in the Xell project lives inside the `xell` namespace. This prevents naming collisions with other libraries. If someone else has a `Token` class, ours is `xell::Token` — no conflict.

### 3.4 The `TokenType` Enum

```cpp
enum class TokenType {
    // Literals
    NUMBER, STRING,

    // Boolean & None keywords (also used as literals)
    TRUE_KW, FALSE_KW, NONE_KW,
    ...
```

An `enum class` (also called a **scoped enum**) creates a set of named integer constants. Think of it as a list of all possible "categories" a token can belong to.

**Why `enum class` instead of plain `enum`?**

- `enum class` is **type-safe**: you can't accidentally compare `TokenType::NUMBER` to a plain integer.
- `enum class` is **scoped**: you must write `TokenType::NUMBER`, not just `NUMBER`. This prevents polluting the namespace.

Let me walk through each group:

#### Literals

```cpp
NUMBER,    // 42, 3.14
STRING,    // "hello world"
```

These represent raw data values typed directly in the source code.

#### Boolean & None Keywords

```cpp
TRUE_KW,    // true
FALSE_KW,   // false
NONE_KW,    // none
```

The `_KW` suffix reminds us these are keywords that happen to be literal values. We can't call them just `TRUE` because that might conflict with macros in some environments.

#### Control Flow Keywords

```cpp
FN,     // fn (function definition)
GIVE,   // give (Xell's return keyword)
IF,     // if
ELIF,   // elif
ELSE,   // else
FOR,    // for
WHILE,  // while
IN,     // in (used in for loops: "for x in list")
```

#### Import Keywords

```cpp
BRING,  // bring (Xell's import keyword)
FROM,   // from
AS,     // as (aliasing)
```

#### Logical Keywords

```cpp
AND,    // and
OR,     // or
NOT,    // not
```

#### Comparison Keywords

```cpp
IS,     // is (alias for ==)
EQ,     // eq (alias for ==)
NE,     // ne (alias for !=)
GT,     // gt (alias for >)
LT,     // lt (alias for <)
GE,     // ge (alias for >=)
LE,     // le (alias for <=)
```

Xell lets you write comparisons using words OR symbols. `x gt 5` and `x > 5` are identical.

#### Utility Keyword

```cpp
OF,     // of (e.g., "type() of name")
```

#### Arithmetic Operators

```cpp
PLUS,    // +
MINUS,   // -
STAR,    // *
SLASH,   // /
```

#### Increment/Decrement

```cpp
PLUS_PLUS,    // ++
MINUS_MINUS,  // --
```

These can be used as prefix (`++x`) or postfix (`x++`).

#### Assignment & Comparison Operators

```cpp
EQUAL,        // =  (assignment)
EQUAL_EQUAL,  // == (comparison)
BANG,         // !  (standalone NOT operator)
BANG_EQUAL,   // != (not equal)
```

The `!` operator (BANG) is a symbolic shorthand for `not`. `!ready` and `not ready` do the same thing.

#### Relational Operators

```cpp
GREATER,        // >
LESS,           // <
GREATER_EQUAL,  // >=
LESS_EQUAL,     // <=
```

#### Access Operators

```cpp
ARROW,  // -> (member access: config->host)
DOT,    // .  (statement terminator)
```

#### Delimiters

```cpp
LPAREN,    // (
RPAREN,    // )
LBRACKET,  // [
RBRACKET,  // ]
LBRACE,    // {
RBRACE,    // }
COMMA,     // ,
COLON,     // : (opens a block scope)
SEMICOLON, // ; (closes a block scope)
```

#### Special

```cpp
IDENTIFIER,  // variable names, function names: myVar, foo, x
NEWLINE,     // significant newlines (statement terminators)
EOF_TOKEN    // end of file — signals the parser to stop
```

### 3.5 The `tokenTypeNames()` Function

```cpp
inline const std::unordered_map<int, std::string>& tokenTypeNames() {
    static const std::unordered_map<int, std::string> map = {
        {(int)TokenType::NUMBER,      "NUMBER"},
        {(int)TokenType::STRING,      "STRING"},
        ...
    };
    return map;
}
```

This is a **map-based lookup table** that converts any `TokenType` to a human-readable string. We use this for error messages and debugging.

**Key design decisions:**

- `inline` — allows this to be defined in a header without causing multiple-definition errors.
- `static` — the map is created once and reused. The first time you call this function, C++ constructs the map. Every subsequent call returns the same object instantly.
- The key is `int` because `enum class` doesn't implicitly convert to `int`, so we cast with `(int)TokenType::NUMBER`.
- Using a **map** instead of a `switch` statement makes it trivial to add new token types: just add one line to the map.

### 3.6 The `tokenTypeToString()` Convenience Function

```cpp
inline std::string tokenTypeToString(TokenType type) {
    auto& names = tokenTypeNames();
    auto it = names.find((int)type);
    if (it != names.end()) return it->second;
    return "UNKNOWN";
}
```

This wraps the map lookup so callers can just write `tokenTypeToString(TokenType::NUMBER)` and get `"NUMBER"`.

### 3.7 The `Token` Struct

```cpp
struct Token {
    TokenType type;
    std::string value;
    int line;

    Token(TokenType type, std::string value, int line)
        : type(type), value(std::move(value)), line(line) {}
};
```

A single token stores three things:

1. **`type`** — which category it belongs to (NUMBER, IF, PLUS, etc.)
2. **`value`** — the raw text from the source code ("42", "if", "+")
3. **`line`** — which line number it appeared on (for error reporting)

The constructor uses `std::move(value)` to avoid unnecessarily copying the string. `std::move` tells C++: "I don't need this `value` parameter anymore, so transfer its memory directly to the member variable instead of copying it."

---

## 4. File 2: `lexer.hpp` — The Lexer Class Declaration

This file tells the world: "Here's what the Lexer can do" — without revealing the implementation details.

### 4.1 Includes

```cpp
#pragma once
#include "token.hpp"
#include <string>
#include <vector>
```

We include `token.hpp` because the lexer produces `Token` objects and needs `TokenType`.

### 4.2 The Lexer Class

```cpp
class Lexer {
public:
    explicit Lexer(const std::string& source);
    std::vector<Token> tokenize();
```

**Public interface — only two things:**

1. **Constructor** — takes the source code as a string. `explicit` prevents accidental implicit conversion (e.g., you can't accidentally pass a string where a Lexer is expected).
2. **`tokenize()`** — scans the entire source and returns a list of tokens.

### 4.3 Private State

```cpp
private:
    std::string source_;   // the entire source code
    size_t pos_;           // current position in source_
    int line_;             // current line number (for error reporting)
    int nestingDepth_;     // tracks () [] {} nesting depth
```

- **`source_`** — the raw Xell source code, stored as a string.
- **`pos_`** — an index into `source_`. Starts at 0, increments as we read characters.
- **`line_`** — tracks what line we're on. Incremented every time we see `\n`.
- **`nestingDepth_`** — when we're inside brackets/parens/braces, newlines should NOT be treated as statement terminators. This counter tracks nesting depth: every `(`, `[`, `{` increments it, every `)`, `]`, `}` decrements it. When depth > 0, NEWLINE tokens are suppressed.

### 4.4 Private Helper Methods

```cpp
char current() const;       // what character are we on right now?
char peek(int offset = 1) const;  // look ahead without consuming
void advance();              // move to the next character
bool isAtEnd() const;        // are we past the end of source_?
```

These are the **core navigation** methods. The lexer reads characters one at a time using `current()` and `advance()`. `peek()` lets us look ahead to decide between multi-character tokens (like `++` vs `+`).

```cpp
void skipWhitespaceAndComments();
void skipSingleLineComment();
void skipMultiLineComment();
```

These handle whitespace and both comment styles (`#` and `-->...<--`).

```cpp
Token readNumber();
Token readString();
Token readIdentifierOrKeyword();
```

Each of these reads one specific kind of token from the current position.

```cpp
static TokenType lookupKeyword(const std::string& word);
static bool isAlpha(char c);
static bool isDigit(char c);
static bool isAlphaNumeric(char c);
```

Utility functions. `static` means they don't need a `Lexer` object to work — they're just pure helper functions.

`lookupKeyword()` checks if a word like "if" or "while" is a keyword. If it's not in the keyword table, it's an `IDENTIFIER`.

---

## 5. File 3: `lexer.cpp` — The Lexer Implementation

This is where the real work happens. Let's go through it section by section.

### 5.1 The Keyword Map

```cpp
static const std::unordered_map<std::string, TokenType>& keywordMap() {
    static const std::unordered_map<std::string, TokenType> map = {
        {"fn",    TokenType::FN},
        {"give",  TokenType::GIVE},
        {"if",    TokenType::IF},
        ...
        {"of",    TokenType::OF},
    };
    return map;
}
```

This is the **heart of keyword recognition**. When we read a word like "while", we look it up in this map:

- Found → it's a keyword, return the corresponding `TokenType`
- Not found → it's a regular `IDENTIFIER`

**Why a map instead of a big `if-else` chain?**

1. **Extensible** — adding a new keyword is one line: `{"newkw", TokenType::NEW_KW}`.
2. **O(1) average lookup** — `unordered_map` uses hashing. An `if-else` chain would be O(n).
3. **Clean** — easy to see all keywords at a glance.

### 5.2 Constructor

```cpp
Lexer::Lexer(const std::string& source)
    : source_(source), pos_(0), line_(1), nestingDepth_(0) {}
```

The **member initializer list** (the part after `:`) sets all fields in one go:

- Start at position 0 (beginning of source)
- Start at line 1 (most editors start counting at 1)
- No nesting yet

### 5.3 Character Navigation

```cpp
char Lexer::current() const {
    if (isAtEnd()) return '\0';
    return source_[pos_];
}
```

Returns the character we're currently looking at. If we're past the end, returns the null character `'\0'` — a safe sentinel value.

```cpp
char Lexer::peek(int offset) const {
    size_t idx = pos_ + offset;
    if (idx >= source_.size()) return '\0';
    return source_[idx];
}
```

Look ahead by `offset` characters **without moving the position**. Default is 1 (look one character ahead). This is crucial for deciding between:

- `+` vs `++`
- `-` vs `--` vs `->`
- `=` vs `==`
- `!` vs `!=`
- `>` vs `>=`
- etc.

```cpp
void Lexer::advance() {
    if (!isAtEnd()) {
        if (source_[pos_] == '\n') line_++;
        pos_++;
    }
}
```

Move forward one character. **Important:** if we pass a newline, increment `line_`. This is how we track line numbers for error messages.

```cpp
bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}
```

Simple bounds check.

### 5.4 Character Classification

```cpp
bool Lexer::isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}
```

A "letter" for identifiers: lowercase, uppercase, or underscore. Xell identifiers can start with a letter or underscore.

```cpp
bool Lexer::isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool Lexer::isAlphaNumeric(char c) {
    return isAlpha(c) || isDigit(c);
}
```

After the first character, identifiers can also contain digits: `file2`, `my_var3`.

### 5.5 Whitespace and Comment Skipping

```cpp
void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = current();

        // Skip spaces and tabs (NOT newlines — those are significant)
        if (c == ' ' || c == '\t' || c == '\r') {
            advance();
            continue;
        }

        // Single-line comment: #
        if (c == '#') {
            skipSingleLineComment();
            continue;
        }

        // Multi-line comment: -->
        if (c == '-' && peek(1) == '-' && peek(2) == '>') {
            skipMultiLineComment();
            continue;
        }

        break;
    }
}
```

Before reading each token, we skip all whitespace and comments. The key insight: **newlines are NOT skipped here** — they're meaningful in Xell (they terminate statements), so they get their own `NEWLINE` token later.

The `continue` after each skip re-enters the loop to check for more whitespace/comments. The `break` exits when we hit actual code.

```cpp
void Lexer::skipSingleLineComment() {
    while (!isAtEnd() && current() != '\n') {
        advance();
    }
    // Don't consume the \n — let the main loop handle it as a NEWLINE token
}
```

Read until end of line, but **leave the `\n` in place** so the main tokenize loop can emit a NEWLINE token.

```cpp
void Lexer::skipMultiLineComment() {
    advance(); // -
    advance(); // -
    advance(); // >

    while (!isAtEnd()) {
        if (current() == '<' && peek(1) == '-' && peek(2) == '-') {
            advance(); // <
            advance(); // -
            advance(); // -
            return;
        }
        advance();
    }
    // Unterminated multi-line comment — hit EOF
}
```

Skip everything between `-->` and `<--`. First consume the opening `-->`, then scan until we find `<--` and consume it too. If we hit EOF without finding `<--`, we just silently stop.

### 5.6 Reading Numbers

```cpp
Token Lexer::readNumber() {
    int startLine = line_;
    std::string num;

    while (!isAtEnd() && isDigit(current())) {
        num += current();
        advance();
    }

    // Decimal point followed by digits → floating point
    if (!isAtEnd() && current() == '.' && isDigit(peek(1))) {
        num += '.';
        advance();
        while (!isAtEnd() && isDigit(current())) {
            num += current();
            advance();
        }
    }

    return Token(TokenType::NUMBER, num, startLine);
}
```

1. Collect digits: `42`
2. If we see a `.` followed by a digit, it's a decimal: `42.5`
3. The check `isDigit(peek(1))` prevents treating the `.` in `42.` (dot as statement terminator after a number) as a decimal point.

### 5.7 Reading Strings

```cpp
Token Lexer::readString() {
    int startLine = line_;
    advance(); // consume opening "

    std::string str;
    while (!isAtEnd() && current() != '"') {
        if (current() == '\\' && peek(1) == '"') {
            str += '"';
            advance(); advance();
        } else if (current() == '\\' && peek(1) == 'n') {
            str += '\n';
            advance(); advance();
        } else if (current() == '\\' && peek(1) == 't') {
            str += '\t';
            advance(); advance();
        } else if (current() == '\\' && peek(1) == '\\') {
            str += '\\';
            advance(); advance();
        } else {
            str += current();
            advance();
        }
    }

    if (isAtEnd()) {
        throw std::runtime_error("...");
    }

    advance(); // consume closing "
    return Token(TokenType::STRING, str, startLine);
}
```

**Escape sequences** are handled:
| Source text | Stored as | Meaning |
|-------------|-----------|---------|
| `\"` | `"` | Literal quote inside a string |
| `\n` | newline | Line break |
| `\t` | tab | Tab character |
| `\\` | `\` | Literal backslash |

String interpolation markers like `{name}` are preserved as-is in the string value. The interpreter will handle expanding them later.

If we hit EOF before finding the closing `"`, we throw an error.

### 5.8 Reading Identifiers and Keywords

```cpp
Token Lexer::readIdentifierOrKeyword() {
    int startLine = line_;
    std::string word;

    while (!isAtEnd() && isAlphaNumeric(current())) {
        word += current();
        advance();
    }

    TokenType type = lookupKeyword(word);
    return Token(type, word, startLine);
}
```

1. Collect all alphanumeric characters: `myVar`, `if`, `count123`
2. Look up the word in the keyword map
3. If found → it's a keyword (IF, FOR, WHILE, etc.)
4. If not found → it's an IDENTIFIER

```cpp
TokenType Lexer::lookupKeyword(const std::string& word) {
    auto& kw = keywordMap();
    auto it = kw.find(word);
    if (it != kw.end()) return it->second;
    return TokenType::IDENTIFIER;
}
```

### 5.9 The Main `tokenize()` Loop

This is the big one. The main loop that drives everything:

```cpp
std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;

    while (!isAtEnd()) {
        skipWhitespaceAndComments();
        if (isAtEnd()) break;

        char c = current();
        int tokenLine = line_;
```

Each iteration: skip whitespace/comments, then look at the current character to decide what kind of token to read.

#### Newlines

```cpp
if (c == '\n') {
    advance();
    if (nestingDepth_ == 0) {
        if (tokens.empty() || tokens.back().type != TokenType::NEWLINE) {
            tokens.emplace_back(TokenType::NEWLINE, "\\n", tokenLine);
        }
    }
    continue;
}
```

Newlines are statement terminators in Xell, BUT:

1. **Inside brackets/parens/braces** (`nestingDepth_ > 0`) → suppress them. This allows multi-line lists, maps, and function calls.
2. **Consecutive newlines** → collapse to one. `tokens.back().type != TokenType::NEWLINE` prevents duplicates.

#### Dispatch to Token Readers

```cpp
if (isDigit(c))  { tokens.push_back(readNumber());            continue; }
if (c == '"')    { tokens.push_back(readString());            continue; }
if (isAlpha(c))  { tokens.push_back(readIdentifierOrKeyword()); continue; }
```

If it starts with a digit → number. If it starts with `"` → string. If it starts with a letter/underscore → identifier or keyword.

#### Multi-Character Operators

The trickiest part: operators that share prefixes.

**`+` and `++`:**

```cpp
if (c == '+') {
    if (peek(1) == '+') {
        tokens.emplace_back(TokenType::PLUS_PLUS, "++", tokenLine);
        advance(); advance();
    } else {
        tokens.emplace_back(TokenType::PLUS, "+", tokenLine);
        advance();
    }
    continue;
}
```

Peek ahead: if the next character is also `+`, it's `++`. Otherwise it's just `+`.

**`-`, `--`, `->`, and `-->`:**

```cpp
if (c == '-') {
    if (peek(1) == '-' && peek(2) == '>') {
        skipMultiLineComment();   // -->  is a comment
    } else if (peek(1) == '-') {
        tokens.emplace_back(TokenType::MINUS_MINUS, "--", tokenLine);
        advance(); advance();
    } else if (peek(1) == '>') {
        tokens.emplace_back(TokenType::ARROW, "->", tokenLine);
        advance(); advance();
    } else {
        tokens.emplace_back(TokenType::MINUS, "-", tokenLine);
        advance();
    }
    continue;
}
```

This is a great example of **longest match**: we check the longest possible match first (`-->`), then shorter ones (`--`, `->`), then the single character (`-`).

**`=` and `==`:**

```cpp
if (c == '=') {
    if (peek(1) == '=') {
        tokens.emplace_back(TokenType::EQUAL_EQUAL, "==", tokenLine);
        advance(); advance();
    } else {
        tokens.emplace_back(TokenType::EQUAL, "=", tokenLine);
        advance();
    }
    continue;
}
```

**`!` and `!=`:**

```cpp
if (c == '!') {
    if (peek(1) == '=') {
        tokens.emplace_back(TokenType::BANG_EQUAL, "!=", tokenLine);
        advance(); advance();
    } else {
        tokens.emplace_back(TokenType::BANG, "!", tokenLine);
        advance();
    }
    continue;
}
```

The standalone `!` (BANG) works as a NOT operator: `!ready` is equivalent to `not ready`.

#### Nesting-Tracked Delimiters

```cpp
if (c == '(') {
    tokens.emplace_back(TokenType::LPAREN, "(", tokenLine);
    nestingDepth_++;
    advance();
    continue;
}
if (c == ')') {
    tokens.emplace_back(TokenType::RPAREN, ")", tokenLine);
    if (nestingDepth_ > 0) nestingDepth_--;
    advance();
    continue;
}
```

Every opening bracket increments `nestingDepth_`, every closing bracket decrements it. The guard `if (nestingDepth_ > 0)` prevents underflow if there's a stray `)`.

Same pattern for `[]` and `{}`.

#### Cleanup

```cpp
// Remove trailing NEWLINE before EOF
if (!tokens.empty() && tokens.back().type == TokenType::NEWLINE) {
    tokens.pop_back();
}

tokens.emplace_back(TokenType::EOF_TOKEN, "", line_);
return tokens;
```

1. Remove any trailing newline (the parser doesn't need it)
2. Add the EOF token — this signals "end of input" to the parser

---

## 6. How It All Fits Together

Here's a complete example. Given this Xell source:

```xell
name = "xell"
count = 42
if count gt 5 :
    print name
;
```

The lexer produces:

```
IDENTIFIER("name")  EQUAL("=")  STRING("xell")  NEWLINE
IDENTIFIER("count") EQUAL("=")  NUMBER("42")     NEWLINE
IF("if")  IDENTIFIER("count")  GT("gt")  NUMBER("5")  COLON(":")  NEWLINE
IDENTIFIER("print")  IDENTIFIER("name")  NEWLINE
SEMICOLON(";")
EOF
```

Notice:

- `"xell"` becomes `STRING` with value `xell` (no quotes in the value)
- `gt` becomes `GT` (recognized as a keyword)
- Newlines become `NEWLINE` tokens (statement separators)
- The parser will later figure out that `print name` is a paren-less function call

---

## 7. Common Patterns & Takeaways

### Pattern: Peek Before Consuming

The lexer frequently peeks ahead to disambiguate tokens (`+` vs `++`, `=` vs `==`). This "look ahead then decide" pattern is fundamental to lexer design.

### Pattern: Map-Based Dispatch

Instead of `switch` or `if-else` chains for keywords, we use an `unordered_map`. This makes the code more maintainable and faster for large keyword sets.

### Pattern: Nesting-Aware Newlines

Most scripting languages treat newlines as significant. The challenge: you don't want newlines inside `[1, 2,\n 3]` to terminate a statement. The `nestingDepth_` counter elegantly solves this.

### Pattern: Static Lazy Initialization

```cpp
static const std::unordered_map<...> map = { ... };
return map;
```

This creates the map once, on first use, and returns a reference to it forever after. No redundant allocations.

---

**Congratulations!** You now understand every line of the Xell lexer. The next step is understanding the AST (Abstract Syntax Tree) — see `ast_guide.md`.
