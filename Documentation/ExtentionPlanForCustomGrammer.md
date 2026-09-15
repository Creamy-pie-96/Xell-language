# Xell Dynamic Grammar System – Complete Implementation Plan

> A comprehensive blueprint for building a fully extensible, user‑definable grammar system for the Xell programming language.

---

## Table of Contents

1. [Vision & Philosophy](#vision--philosophy)
2. [Core Architecture](#core-architecture)
3. [Implementation Phases](#implementation-phases)
4. [Grammar Definition Format (.xeg)](#grammar-definition-format-xeg)
5. [Parser Engine Design](#parser-engine-design)
6. [Integration with Existing Code](#integration-with-existing-code)
7. [Performance Considerations](#performance-considerations)
8. [User Experience](#user-experience)
9. [Edge Cases & Error Handling](#edge-cases--error-handling)
10. [Future Extensions](#future-extensions)

---

## Vision & Philosophy

### The Big Idea

Xell becomes a **language construction kit** – a platform where users can define their own syntax while keeping the core semantics (safety, memory management, concurrency) intact.

**Users can:** Change keywords, operators, statement order, block delimiters, and entire syntax patterns.
**Users cannot:** Change scoping rules, type system, memory management, concurrency model, or built‑in behaviour.

### Design Principles

1. **Semantics are sacred** – The core interpreter never changes.
2. **Syntax is data** – Grammar rules are loaded from files at runtime.
3. **Safety first** – Users cannot create ambiguous or unsafe grammars.
4. **Performance matters** – Grammar parsing and execution should be fast.
5. **Gradual complexity** – Start with simple keyword mapping, grow to full grammar.

---

## Core Architecture

### The Stack

```
┌─────────────────────────────────────────────────────────────────────────────┐
│ USER LAYER                                                                  │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ User Code (.xel / .xell)                                               │ │
│ │ - Written in user's custom syntax                                      │ │
│ │ - Contains @grammar directive                                          │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ GRAMMAR LAYER                                                               │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ Grammar File (.xeg)                                                    │ │
│ │ - Keyword mappings                                                     │ │
│ │ - Operator precedence and associativity                               │ │
│ │ - Statement patterns                                                   │ │
│ │ - Expression patterns                                                  │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ PARSER ENGINE                                                              │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ Grammar Parser                                                         │ │
│ │ - Parses .xeg files into internal GrammarDef struct                   │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ Pattern Matcher                                                        │ │
│ │ - Matches token sequences against grammar rules                       │ │
│ │ - Extracts named parts (name, params, body, etc.)                    │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ AST Builder                                                            │ │
│ │ - Constructs canonical Xell AST nodes                                 │ │
│ │ - Validates semantic constraints                                      │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│ CORE INTERPRETER                                                           │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ Lexer (unchanged)                                                      │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ Interpreter (unchanged)                                                │ │
│ │ - Only sees canonical AST                                              │ │
│ │ - Completely unaware of user syntax                                   │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
│ ┌─────────────────────────────────────────────────────────────────────────┐ │
│ │ Built‑ins (unchanged)                                                 │ │
│ │ - print, len, type, run, etc.                                         │ │
│ └─────────────────────────────────────────────────────────────────────────┘ │
└─────────────────────────────────────────────────────────────────────────────┘
```

### Key Insight

**The interpreter never changes.** The grammar system sits *between* the lexer and the interpreter, transforming user syntax into canonical Xell AST.

---

## Implementation Phases

### Phase 1: Grammar Definition Format (.xeg)

**Goal:** Design and implement a file format for defining grammar rules.

**Tasks:**
1. Define JSON schema for `.xeg` files.
2. Implement `.xeg` parser (C++).
3. Validate grammar rules.
4. Load grammar into `GrammarDef` struct.

**Estimated effort:** 2‑3 weeks

**Deliverables:**
- `.xeg` file format specification.
- `GrammarDef` C++ struct.
- `GrammarParser` class.

---

### Phase 2: Keyword and Operator Mapping

**Goal:** Extend the dialect system to support full keyword and operator mapping.

**Tasks:**
1. Replace hard‑coded keyword lookup with grammar‑driven lookup.
2. Replace hard‑coded operator mapping with grammar‑driven mapping.
3. Support operator precedence and associativity configuration.

**Estimated effort:** 1‑2 weeks

**Deliverables:**
- Grammar‑driven lexer (keyword mapping).
- Grammar‑driven parser (operator precedence).

---

### Phase 3: Statement Pattern Matching

**Goal:** Allow users to define statement syntax patterns.

**Tasks:**
1. Design pattern language for statements (e.g., `def {name}({params}): {body}`).
2. Implement pattern matcher that matches token sequences.
3. Implement AST builder that constructs canonical AST from matched parts.
4. Support block delimiters (explicit `:` … `;` or indentation‑based).

**Estimated effort:** 4‑6 weeks

**Deliverables:**
- Pattern matching engine.
- AST builder for all canonical statement types.
- Block detection (indentation or explicit).

---

### Phase 4: Expression Pattern Matching

**Goal:** Allow users to define expression syntax.

**Tasks:**
1. Design pattern language for expressions.
2. Implement Pratt parser (or similar) for grammar‑driven expression parsing.
3. Support operator precedence and associativity.
4. Support custom operators.

**Estimated effort:** 3‑4 weeks

**Deliverables:**
- Grammar‑driven Pratt parser.
- Custom operator support.

---

### Phase 5: Grammar Loading and Caching

**Goal:** Load grammar files efficiently and cache them.

**Tasks:**
1. Load grammar at compile‑time (when `@grammar` directive is encountered).
2. Cache loaded grammars in a global registry.
3. Parse grammar once, reuse for all files in the project.

**Estimated effort:** 1‑2 weeks

**Deliverables:**
- Grammar registry.
- Grammar cache.

---

### Phase 6: Integration with Existing Tooling

**Goal:** Update tools (VSCode extension, linter, etc.) to support custom grammars.

**Tasks:**
1. VSCode extension loads grammar and updates tokeniser.
2. Linter uses grammar for diagnostics.
3. Autocomplete uses grammar for suggestions.

**Estimated effort:** 2‑3 weeks

**Deliverables:**
- Updated VSCode extension.
- Updated linter.
- Updated autocomplete.

---

### Phase 7: Documentation and Examples

**Goal:** Provide comprehensive documentation and examples.

**Tasks:**
1. Write grammar system documentation.
2. Create example grammars (Pythonish, Rubyish, JavaScriptish, MATLABish, etc.).
3. Write tutorial for creating custom grammars.

**Estimated effort:** 1‑2 weeks

**Deliverables:**
- Documentation.
- Example grammars.
- Tutorial.

---

## Grammar Definition Format (.xeg)

### Full Specification

```json
{
  "name": "My Dialect",
  "version": "1.0",
  "description": "A custom dialect for my team",

  "keywords": {
    "fn": "def",
    "give": "return",
    "if": "if",
    "elif": "elseif",
    "else": "otherwise",
    "for": "for",
    "while": "while",
    "true": "True",
    "false": "False",
    "none": "None"
  },

  "operators": {
    "+": { "precedence": 8, "associativity": "left" },
    "-": { "precedence": 8, "associativity": "left" },
    "*": { "precedence": 9, "associativity": "left" },
    "/": { "precedence": 9, "associativity": "left" },
    "**": { "precedence": 10, "associativity": "right" },
    "==": { "precedence": 5, "associativity": "left" },
    "!=": { "precedence": 5, "associativity": "left" },
    ">": { "precedence": 6, "associativity": "left" },
    "<": { "precedence": 6, "associativity": "left" },
    ">=": { "precedence": 6, "associativity": "left" },
    "<=": { "precedence": 6, "associativity": "left" },
    "and": { "precedence": 3, "associativity": "left" },
    "or": { "precedence": 2, "associativity": "left" },
    "not": { "precedence": 11, "associativity": "right" }
  },

  "statements": {
    "function_definition": {
      "pattern": [
        "keyword:def",
        "identifier:{name}",
        "(",
        "params:{params}",
        ")",
        ":",
        "block:{body}"
      ],
      "ast": {
        "type": "FnDef",
        "name": "{name}",
        "params": "{params}",
        "body": "{body}"
      }
    },

    "return_statement": {
      "pattern": [
        "keyword:return",
        "expr:{value}"
      ],
      "ast": {
        "type": "GiveStmt",
        "value": "{value}"
      }
    },

    "if_statement": {
      "pattern": [
        "keyword:if",
        "expr:{condition}",
        ":",
        "block:{body}",
        "elif_clauses:{elifs}",
        "else_clause:{else}"
      ],
      "ast": {
        "type": "IfStmt",
        "condition": "{condition}",
        "body": "{body}",
        "elifs": "{elifs}",
        "else_body": "{else}"
      }
    },

    "for_statement": {
      "pattern": [
        "keyword:for",
        "identifier:{var}",
        "keyword:in",
        "expr:{iterable}",
        ":",
        "block:{body}"
      ],
      "ast": {
        "type": "ForStmt",
        "varNames": ["{var}"],
        "iterables": ["{iterable}"],
        "body": "{body}"
      }
    },

    "assignment": {
      "pattern": [
        "identifier:{name}",
        "=",
        "expr:{value}"
      ],
      "ast": {
        "type": "Assignment",
        "name": "{name}",
        "value": "{value}"
      }
    }
  },

  "expressions": {
    "binary": {
      "pattern": [
        "left:{left}",
        "operator:{op}",
        "right:{right}"
      ],
      "ast": {
        "type": "BinaryExpr",
        "left": "{left}",
        "op": "{op}",
        "right": "{right}"
      }
    },

    "function_call": {
      "pattern": [
        "name:{name}",
        "(",
        "args:{args}",
        ")"
      ],
      "ast": {
        "type": "CallExpr",
        "callee": "{name}",
        "args": "{args}"
      }
    }
  },

  "blocks": {
    "explicit": {
      "start": ":",
      "end": ";"
    }
  },

  "comments": {
    "line": "#",
    "block": {
      "start": "/*",
      "end": "*/"
    }
  },

  "strings": {
    "delimiters": ["\"", "'"],
    "escape": "\\"
  },

  "numbers": {
    "prefixes": {
      "0x": "hex",
      "0o": "octal",
      "0b": "binary"
    },
    "suffixes": {
      "i": "imaginary"
    },
    "exponent": "E"
  }
}
```

### Grammar Rule Pattern Language

#### Tokens

| Token Type | Syntax | Description |
|------------|--------|-------------|
| Keyword | `keyword:if` | Matches the keyword `if` |
| Identifier | `identifier:{name}` | Matches an identifier, captures as `{name}` |
| Number | `number:{value}` | Matches a number, captures as `{value}` |
| String | `string:{value}` | Matches a string, captures as `{value}` |
| Operator | `operator:{op}` | Matches an operator, captures as `{op}` |
| Delimiter | `(` or `)` etc. | Matches a literal character |
| Expr | `expr:{name}` | Matches any expression, captures as `{name}` |
| Block | `block:{name}` | Matches a block of statements, captures as `{name}` |
| List | `list:{name}` | Matches a comma‑separated list, captures as `{name}` |
| Optional | `optional:{pattern}` | Matches the pattern optionally |
| Repeat | `repeat:{pattern}` | Matches the pattern zero or more times |

#### AST Construction

| Syntax | Description |
|--------|-------------|
| `{name}` | References a captured value |
| `{name.field}` | References a field of a captured value |
| `["value1", "value2"]` | Creates a list literal |
| `{ "key": "{value}" }` | Creates a map literal |

---

## Parser Engine Design

### GrammarDef Struct

```cpp
struct GrammarDef {
    std::string name;
    std::string version;
    std::string description;
    
    // Keyword mapping: canonical → custom
    std::unordered_map<std::string, std::string> keywords;
    
    // Operator mapping: canonical → {custom, precedence, associativity}
    struct OperatorInfo {
        std::string symbol;
        int precedence;
        bool rightAssociative;
    };
    std::unordered_map<std::string, OperatorInfo> operators;
    
    // Statement rules
    struct StatementRule {
        std::string name;
        std::vector<TokenPattern> pattern;
        std::unordered_map<std::string, std::string> astMapping;
    };
    std::vector<StatementRule> statements;
    
    // Expression rules
    struct ExpressionRule {
        std::string name;
        std::vector<TokenPattern> pattern;
        std::unordered_map<std::string, std::string> astMapping;
    };
    std::vector<ExpressionRule> expressions;
    
    // Block configuration
    struct BlockConfig {
        std::string start;
        std::string end;
        bool indentationBased = false;
    };
    BlockConfig blocks;
    
    // Comment configuration
    struct CommentConfig {
        std::string line;
        struct BlockComment {
            std::string start;
            std::string end;
        } block;
    };
    CommentConfig comments;
};
```

### TokenPattern Struct

```cpp
struct TokenPattern {
    enum Type {
        KEYWORD,
        IDENTIFIER,
        NUMBER,
        STRING,
        OPERATOR,
        DELIMITER,
        EXPR,
        BLOCK,
        LIST,
        OPTIONAL,
        REPEAT,
        INDENT,
        DEDENT
    };
    
    Type type;
    std::string value;       // For literal matches
    std::string capture;     // For named captures
    std::vector<TokenPattern> children;  // For nested patterns
};
```

### Pattern Matcher

```cpp
class PatternMatcher {
public:
    struct MatchResult {
        bool success;
        size_t consumed;  // Number of tokens consumed
        std::unordered_map<std::string, std::string> captures;
        std::unordered_map<std::string, std::vector<std::string>> listCaptures;
    };
    
    MatchResult match(const std::vector<TokenPattern> &pattern,
                      const std::vector<Token> &tokens,
                      size_t start);
    
private:
    bool matchToken(const TokenPattern &pattern,
                    const Token &token,
                    std::unordered_map<std::string, std::string> &captures);
};
```

### AST Builder

```cpp
class ASTBuilder {
public:
    ExprPtr build(const std::unordered_map<std::string, std::string> &captures,
                  const std::unordered_map<std::string, std::string> &mapping);
    
    StmtPtr buildStmt(const std::unordered_map<std::string, std::string> &captures,
                      const std::unordered_map<std::string, std::string> &mapping);
    
private:
    ExprPtr buildExpr(const std::string &type,
                      const std::unordered_map<std::string, std::string> &captures);
    
    std::vector<StmtPtr> buildBlock(const std::string &blockContent,
                                    const std::unordered_map<std::string, std::string> &captures);
};
```

### DynamicParser Class

```cpp
class DynamicParser {
public:
    DynamicParser();
    
    // Load a grammar from a .xeg file
    bool loadGrammar(const std::string &path);
    
    // Parse source code using the loaded grammar
    Program parse(const std::string &source);
    
    // Parse source code using a specific grammar
    Program parseWithGrammar(const std::string &source, const GrammarDef &grammar);
    
    // Get the active grammar
    const GrammarDef &activeGrammar() const { return grammar_; }
    
    // Get the default grammar
    static GrammarDef defaultGrammar();
    
private:
    GrammarDef grammar_;
    std::unordered_map<std::string, std::vector<GrammarRule>> parseTable_;
    
    // Internal parsing methods
    StmtPtr parseStatement(const std::vector<Token> &tokens, size_t &pos);
    ExprPtr parseExpression(const std::vector<Token> &tokens, size_t &pos);
    ExprPtr parsePrefixExpr(const std::vector<Token> &tokens, size_t &pos);
    ExprPtr parseInfixExpr(ExprPtr left, const std::vector<Token> &tokens, size_t &pos, int minPrecedence);
    
    // Build parse table for O(1) rule lookup
    void buildParseTable();
};
```

---

## Integration with Existing Code

### Lexer Integration

The lexer remains unchanged. It produces tokens (`NUMBER`, `IDENTIFIER`, `STRING`, etc.) which the parser uses.

```cpp
// No changes needed to lexer.hpp or lexer.cpp
```

### Parser Integration

The `Parser` class becomes a **wrapper** around the `DynamicParser`.

```cpp
class Parser {
public:
    explicit Parser(const std::vector<Token> &tokens,
                    const std::string &grammarPath = "");
    
    Program parse();
    Program parseLint(std::vector<CollectedParseError> &errors);
    
private:
    std::vector<Token> tokens_;
    DynamicParser dynamicParser_;
    bool useDynamicParser_;
    
    // If grammar path is provided, use dynamic parser
    // Otherwise, use the hard‑coded parser (fallback)
};
```

### Interpreter Integration

**No changes needed.** The interpreter only sees the canonical AST.

```cpp
// No changes needed to interpreter.hpp or interpreter.cpp
```

### Module Resolver Integration

The module resolver needs to detect `@grammar` directives and load the appropriate grammar.

```cpp
// In ModuleResolver::resolveFromFileSystem()
void resolveGrammar(const std::string &filePath) {
    // Read the first few lines of the file
    // Look for @grammar "path.xeg"
    // If found, load the grammar file
    // If not, use the default grammar
}
```

---

## Performance Considerations

### Grammar Loading

Grammar files are parsed **once** per session and cached.

```cpp
class GrammarRegistry {
public:
    static GrammarRegistry &instance() {
        static GrammarRegistry registry;
        return registry;
    }
    
    const GrammarDef &getGrammar(const std::string &path) {
        auto it = cache_.find(path);
        if (it != cache_.end()) {
            return it->second;
        }
        
        // Load and parse grammar
        GrammarDef grammar = loadGrammarFile(path);
        cache_[path] = grammar;
        return cache_[path];
    }
    
private:
    std::unordered_map<std::string, GrammarDef> cache_;
};
```

### Parser Performance

1. **Parse table** – Build a parse table for O(1) rule lookup.
2. **Memoization** – Cache parse results for repeated patterns.
3. **Incremental parsing** – Only parse changed parts of the file.

### Runtime Performance

The grammar system adds a **one‑time overhead**:

1. Grammar parsing and caching – negligible.
2. Source parsing – slightly slower than hard‑coded parser.
3. Execution – **no overhead** (the interpreter sees the same AST).

---

## User Experience

### Creating a Grammar

**Step 1:** Create a `.xeg` file.

```json
{
  "name": "Pythonish Xell",
  "keywords": {
    "fn": "def",
    "give": "return",
    "if": "if",
    "elif": "elif",
    "else": "else",
    "for": "for",
    "while": "while",
    "true": "True",
    "false": "False",
    "none": "None"
  }
}
```

**Step 2:** Add `@grammar` directive to your source file.

```xell
@grammar "pythonish.xeg"

def greet(name):
    print("Hello, " + name)
;

def add(a, b):
    return a + b
;
```

**Step 3:** Run the file normally.

```bash
xell script.xel
```

### Sharing a Grammar

```bash
# Install a grammar globally
cp my_grammar.xeg ~/.local/share/xell/grammars/

# Use it from any file
@grammar "my_grammar.xeg"
```

### Creating a Project‑Specific Grammar

```bash
# Place the grammar in the project root
./my_project/
├── grammar.xeg
├── src/
│   └── main.xel
└── README.md
```

```xell
@grammar "grammar.xeg"
```

---

## Edge Cases & Error Handling

### 1. Ambiguous Grammars

**The Problem:** The same token sequence matches multiple rules.

**The Solution:** The parser tries rules in the order they appear in the grammar file. The first match wins.

```json
{
  "statements": {
    "if_statement": { ... },     // Tries this first
    "function_definition": { ... }  // Tries this second
  }
}
```

**Error message:** If no rule matches, the parser throws a `ParseError` with the token position.

---

### 2. Missing Dependencies

**The Problem:** A grammar rule references a pattern that hasn't been defined.

**The Solution:** The grammar parser validates all references at load time.

```json
{
  "statements": {
    "my_statement": {
      "pattern": ["custom", "pattern", "block:{my_block}"],
      "ast": { ... }
    }
  }
}
```

**Error message:** "Grammar error: block pattern 'my_block' not found."

---

### 3. Recursive Definitions

**The Problem:** A rule references itself, causing infinite recursion.

**The Solution:** The parser detects recursive rules and throws an error.

```json
{
  "expressions": {
    "binary": {
      "pattern": ["left:{expr}", "operator:{op}", "right:{expr}"],
      "ast": { ... }
    }
  }
}
```

This is **allowed** because `expr` is a built‑in pattern. The parser handles recursion naturally.

---

### 4. Malformed `.xeg` Files

**The Problem:** The user made a syntax error in the `.xeg` file.

**The Solution:** The grammar parser validates JSON syntax and required fields.

**Error message:** "Invalid grammar file 'my_grammar.xeg': missing required field 'keywords'."

---

### 5. Conflicting Operator Precedence

**The Problem:** Two operators have the same precedence but different associativity.

**The Solution:** The grammar validator checks for conflicts.

```json
{
  "operators": {
    "+": { "precedence": 8, "associativity": "left" },
    "-": { "precedence": 8, "associativity": "right" }
  }
}
```

**Error message:** "Grammar error: operators '+' and '-' have the same precedence but different associativity."

---

### 6. Unsupported Features

**The Problem:** The user tries to define a pattern that the parser engine doesn't support.

**The Solution:** The grammar parser validates all patterns against the supported set.

**Error message:** "Grammar error: unsupported pattern type 'custom_token'."

---

### 7. Memory and Performance Issues

**The Problem:** A grammar with hundreds of rules slows down parsing.

**The Solution:** The parse table ensures O(1) rule lookup, even for large grammars.

**Recommendation:** Keep grammars under 100 rules for optimal performance.

---

## Future Extensions

### 1. Grammar Inheritance

Users can extend existing grammars rather than writing from scratch.

```json
{
  "inherits": "default.xeg",
  "keywords": {
    "fn": "def"
  }
}
```

### 2. Grammar Versioning

Grammars can declare compatibility with specific Xell versions.

```json
{
  "name": "My Grammar",
  "xell_version": ">=0.1.0"
}
```

### 3. Grammar Testing

Users can test their grammar against example code.

```bash
xell --test-grammar my_grammar.xeg test_cases.xel
```

### 4. Grammar Generation

Users can generate `.xeg` files from existing codebases.

```bash
xell --gen-grammar src/ > grammar.xeg
```

### 5. Grammar IDE Support

VSCode extension provides syntax highlighting and autocomplete for `.xeg` files.

### 6. Grammar Marketplace

A repository of community‑contributed grammars.

### 7. Grammar Optimization

The parser engine can suggest optimisations (e.g., reordering rules for faster matching).

### 8. Grammar Visual Editor

A GUI tool for creating grammars visually.

### 9. Grammar as Code

Grammars can be written in Xell itself.

```xell
grammar "pythonish.xeg" {
    keywords: {
        "fn": "def",
        "give": "return"
    }
}
```

### 10. Dynamic Grammar Switching

Grammars can be changed at runtime within a single session.

```xell
@grammar "pythonish.xeg"
def greet(name): print("Hello") ;

@grammar "rubyish.xeg"
def greet(name): puts("Hello") ;
```

---

## Glossary

| Term | Definition |
|------|------------|
| **AST** | Abstract Syntax Tree – the internal representation of code |
| **Canonical AST** | The standard AST that the interpreter understands |
| **Grammar** | A set of rules defining the syntax of a language |
| **Grammar Engine** | The component that parses user code using grammar rules |
| **Pattern** | A sequence of tokens that matches a specific syntax construct |
| **Parse Table** | A data structure that maps token sequences to grammar rules |
| **Precedence** | The order in which operators are evaluated (e.g., `*` before `+`) |
| **Associativity** | The order in which operators of the same precedence are evaluated (left‑to‑right or right‑to‑left) |
| **Mathy Block** | A performance‑optimised execution context for mathematical code |

---

## Conclusion

The Dynamic Grammar System transforms Xell from a static language into a **living language platform**. Users can define their own syntax, build DSLs, and customise the language to their exact needs – all while keeping the core semantics safe and predictable.

**This is the future of Xell.**

---

**Next Steps:**

1. Start with Phase 1: Define the `.xeg` format.
2. Implement the grammar parser.
3. Integrate with the existing parser.
4. Test with example grammars.
5. Write documentation and tutorials.