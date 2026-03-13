#include "parser.hpp"
#include "../lib/errors/error.hpp"
#include "../lexer/lexer.hpp"
#include <sstream>
#include <iostream>
#include <unordered_set>

namespace xell
{

    // ============================================================
    // Constructor
    // ============================================================

    Parser::Parser(const std::vector<Token> &tokens)
        : tokens_(tokens), pos_(0) {}

    // ============================================================
    // Token navigation
    // ============================================================

    const Token &Parser::current() const
    {
        return tokens_[pos_];
    }

    const Token &Parser::peekToken(int offset) const
    {
        size_t idx = pos_ + offset;
        if (idx >= tokens_.size())
            return tokens_.back(); // EOF
        return tokens_[idx];
    }

    bool Parser::check(TokenType type) const
    {
        return current().type == type;
    }

    bool Parser::isAtEnd() const
    {
        return current().type == TokenType::EOF_TOKEN;
    }

    Token Parser::advance()
    {
        Token tok = current();
        if (!isAtEnd())
            pos_++;
        return tok;
    }

    Token Parser::consume(TokenType type, const std::string &errorMsg)
    {
        if (check(type))
            return advance();

        std::string msg = errorMsg.empty()
                              ? "Expected " + tokenTypeToString(type) + " but got " +
                                    tokenTypeToString(current().type) + " ('" + current().value + "')"
                              : errorMsg;
        throw ParseError(msg, current().line);
    }

    void Parser::skipNewlines()
    {
        while (check(TokenType::NEWLINE))
            advance();
    }

    void Parser::consumeStatementEnd()
    {
        if (check(TokenType::DOT))
            advance();
    }

    bool Parser::canStartPrimary(TokenType type) const
    {
        return type == TokenType::NUMBER || type == TokenType::IMAGINARY || type == TokenType::STRING || type == TokenType::RAW_STRING || type == TokenType::BYTE_STRING || type == TokenType::TRUE_KW || type == TokenType::FALSE_KW || type == TokenType::NONE_KW || type == TokenType::IDENTIFIER || type == TokenType::LPAREN || type == TokenType::LBRACKET || type == TokenType::LBRACE || type == TokenType::NOT || type == TokenType::BANG || type == TokenType::MINUS || type == TokenType::PLUS_PLUS || type == TokenType::MINUS_MINUS || type == TokenType::ELLIPSIS || type == TokenType::YIELD || type == TokenType::AWAIT || type == TokenType::TILDE || type == TokenType::IF || type == TokenType::FOR || type == TokenType::WHILE || type == TokenType::LOOP || type == TokenType::INCASE || type == TokenType::SHELL_CMD;
    }

    std::unique_ptr<DestructuringPattern> Parser::parseDestructuringPattern(
        bool allowRest,
        std::vector<std::string> *flatNames)
    {
        if (check(TokenType::ELLIPSIS))
        {
            if (!allowRest)
                throw ParseError("Rest pattern is only allowed inside list destructuring", current().line);
            advance();
            std::string name = consume(TokenType::IDENTIFIER, "Expected variable name after '...'").value;
            if (flatNames && name != "_")
                flatNames->push_back(name);
            return std::make_unique<DestructuringPattern>(DestructuringPattern::Kind::REST, name);
        }

        if (check(TokenType::IDENTIFIER))
        {
            std::string name = advance().value;
            if (flatNames && name != "_")
                flatNames->push_back(name);
            return std::make_unique<DestructuringPattern>(DestructuringPattern::Kind::NAME, name);
        }

        if (check(TokenType::LBRACKET))
        {
            advance();
            auto pattern = std::make_unique<DestructuringPattern>(DestructuringPattern::Kind::LIST);
            bool sawRest = false;

            if (!check(TokenType::RBRACKET))
            {
                while (true)
                {
                    auto element = parseDestructuringPattern(true, flatNames);
                    if (element->kind == DestructuringPattern::Kind::REST)
                    {
                        if (sawRest)
                            throw ParseError("Only one rest pattern is allowed in a list destructuring pattern", current().line);
                        sawRest = true;
                    }
                    else if (sawRest)
                    {
                        throw ParseError("Rest pattern must be the last element in a list destructuring pattern", current().line);
                    }

                    pattern->elements.push_back(std::move(element));
                    if (!check(TokenType::COMMA))
                        break;
                    advance();
                    if (check(TokenType::RBRACKET))
                        break;
                }
            }

            consume(TokenType::RBRACKET, "Expected ']' after list destructuring pattern");
            return pattern;
        }

        if (check(TokenType::LBRACE))
        {
            advance();
            auto pattern = std::make_unique<DestructuringPattern>(DestructuringPattern::Kind::MAP);

            if (!check(TokenType::RBRACE))
            {
                while (true)
                {
                    std::string key;
                    std::unique_ptr<DestructuringPattern> entryPattern;

                    if (check(TokenType::IDENTIFIER) || check(TokenType::STRING))
                    {
                        key = current().value;
                        advance();
                    }
                    else
                    {
                        throw ParseError("Expected identifier or string key in map destructuring pattern", current().line);
                    }

                    if (check(TokenType::COLON))
                    {
                        advance();
                        entryPattern = parseDestructuringPattern(false, flatNames);
                    }
                    else
                    {
                        if (flatNames && key != "_")
                            flatNames->push_back(key);
                        entryPattern = std::make_unique<DestructuringPattern>(DestructuringPattern::Kind::NAME, key);
                    }

                    pattern->entries.emplace_back(key, std::move(entryPattern));

                    if (!check(TokenType::COMMA))
                        break;
                    advance();
                    if (check(TokenType::RBRACE))
                        break;
                }
            }

            consume(TokenType::RBRACE, "Expected '}' after map destructuring pattern");
            return pattern;
        }

        throw ParseError("Expected destructuring pattern", current().line);
    }

    std::unique_ptr<DestructuringPattern> Parser::parseTopLevelDestructuringPattern(
        std::vector<std::string> *flatNames)
    {
        if (check(TokenType::LBRACKET) || check(TokenType::LBRACE))
            return parseDestructuringPattern(false, flatNames);

        auto pattern = std::make_unique<DestructuringPattern>(DestructuringPattern::Kind::LIST);
        bool sawComma = false;
        bool sawRest = false;

        while (true)
        {
            std::unique_ptr<DestructuringPattern> element;
            if (check(TokenType::ELLIPSIS))
            {
                if (sawRest)
                    throw ParseError("Only one rest pattern is allowed in destructuring assignment", current().line);
                element = parseDestructuringPattern(true, flatNames);
                sawRest = true;
            }
            else if (check(TokenType::IDENTIFIER))
            {
                if (sawRest)
                    throw ParseError("Rest pattern must be the last element in destructuring assignment", current().line);
                element = parseDestructuringPattern(false, flatNames);
            }
            else
            {
                throw ParseError("Expected identifier in destructuring assignment", current().line);
            }

            pattern->elements.push_back(std::move(element));

            if (!check(TokenType::COMMA))
                break;
            sawComma = true;
            advance();
        }

        if (!sawComma && pattern->elements.size() == 1 &&
            pattern->elements[0]->kind == DestructuringPattern::Kind::NAME)
        {
            throw ParseError("Not a destructuring assignment", current().line);
        }

        return pattern;
    }

    // ============================================================
    // Top-level parse
    // ============================================================

    Program Parser::parse()
    {
        Program program;
        skipNewlines();
        while (!isAtEnd())
        {
            program.statements.push_back(parseStatement());
            skipNewlines();
        }
        return program;
    }

    // ============================================================
    // Error-recovering parse (for linter)
    // ============================================================

    Program Parser::parseLint(std::vector<CollectedParseError> &errors)
    {
        Program program;
        skipNewlines();
        while (!isAtEnd())
        {
            try
            {
                program.statements.push_back(parseStatement());
            }
            catch (const ParseError &e)
            {
                errors.push_back({e.line(), e.detail()});
                synchronize();
            }
            catch (const XellError &e)
            {
                errors.push_back({e.line(), e.detail()});
                synchronize();
            }
            skipNewlines();
        }
        return program;
    }

    // ============================================================
    // Error recovery: skip tokens until the next statement boundary
    // ============================================================

    void Parser::synchronize()
    {
        // Advance at least one token so we make progress
        if (!isAtEnd())
            advance();

        while (!isAtEnd())
        {
            // If we just consumed a newline or dot (statement enders), stop
            TokenType prev = tokens_[pos_ > 0 ? pos_ - 1 : 0].type;
            if (prev == TokenType::NEWLINE || prev == TokenType::DOT ||
                prev == TokenType::SEMICOLON)
                return;

            // If the current token can start a new statement, stop
            TokenType cur = current().type;
            switch (cur)
            {
            case TokenType::IF:
            case TokenType::FOR:
            case TokenType::WHILE:
            case TokenType::FN:
            case TokenType::GIVE:
            case TokenType::BREAK:
            case TokenType::CONTINUE:
            case TokenType::BRING:
            case TokenType::TRY:
            case TokenType::INCASE:
            case TokenType::LET:
            case TokenType::LOOP:
            case TokenType::DO:
            case TokenType::ENUM:
            case TokenType::ASYNC:
            case TokenType::AT:
                return;
            case TokenType::IDENTIFIER:
                // IDENT = ... is an assignment → new statement
                if (peekToken(1).type == TokenType::EQUAL)
                    return;
                // IDENT followed by newline/dot/eof could be an expression stmt
                // But let's just try it
                return;
            default:
                advance();
            }
        }
    }

    // ============================================================
    // Block: parse statements until ';'
    // ============================================================

    std::vector<StmtPtr> Parser::parseBlock(bool stopAtElifElse, bool stopAtGive)
    {
        std::vector<StmtPtr> stmts;
        skipNewlines();
        while (!check(TokenType::SEMICOLON) && !isAtEnd())
        {
            // If we're inside an if/elif block, also stop when we see elif/else
            if (stopAtElifElse &&
                (check(TokenType::ELIF) || check(TokenType::ELSE)))
                break;
            // If we're inside an expression-mode loop, stop when we see give (default value)
            if (stopAtGive && check(TokenType::GIVE))
                break;
            stmts.push_back(parseStatement());
            skipNewlines();
        }
        return stmts;
    }

    // ============================================================
    // Statement
    // ============================================================

    StmtPtr Parser::parseStatement()
    {
        skipNewlines();

        TokenType type = current().type;

        // --- Keyword statements ---
        if (type == TokenType::IF)
            return parseIfStmt();
        if (type == TokenType::FOR)
            return parseForStmt();
        if (type == TokenType::WHILE)
            return parseWhileStmt();
        if (type == TokenType::LOOP)
            return parseLoopStmt();
        if (type == TokenType::DO)
            return parseDoWhileStmt();
        if (type == TokenType::FN)
            return parseFnDef();
        if (type == TokenType::GIVE)
            return parseGiveStmt();
        if (type == TokenType::BREAK)
        {
            int ln = current().line;
            advance(); // consume break
            // Check if there's a value expression following (break VALUE)
            ExprPtr breakVal = nullptr;
            skipNewlines();
            if (!check(TokenType::NEWLINE) && !check(TokenType::SEMICOLON) &&
                !check(TokenType::EOF_TOKEN) && canStartPrimary(current().type))
            {
                breakVal = parseExpression();
            }
            consumeStatementEnd();
            return std::make_unique<BreakStmt>(std::move(breakVal), ln);
        }
        if (type == TokenType::CONTINUE)
        {
            int ln = current().line;
            advance(); // consume continue
            consumeStatementEnd();
            return std::make_unique<ContinueStmt>(ln);
        }
        if (type == TokenType::BRING)
            return parseBringStmt();
        if (type == TokenType::FROM)
            return parseBringStmt(); // from "dir" bring ...
        if (type == TokenType::MODULE)
            return parseModuleDef();
        if (type == TokenType::EXPORT)
            return parseExportDecl();
        if (type == TokenType::TRY)
            return parseTryCatchStmt();
        if (type == TokenType::THROW)
            return parseThrowStmt();
        if (type == TokenType::INCASE)
            return parseInCaseStmt();
        if (type == TokenType::LET)
            return parseLetStmt();
        if (type == TokenType::ENUM)
            return parseEnumDef();
        if (type == TokenType::STRUCT)
            return parseStructDef();
        if (type == TokenType::CLASS)
            return parseClassDef();
        if (type == TokenType::ABSTRACT)
        {
            advance(); // consume 'abstract'
            skipNewlines();
            // 'abstract' followed by class name (no 'class' keyword needed)
            return parseClassDef(true, false);
        }
        if (type == TokenType::MIXIN)
        {
            advance(); // consume 'mixin'
            skipNewlines();
            return parseClassDef(false, true);
        }
        if (type == TokenType::INTERFACE)
            return parseInterfaceDef();
        if (type == TokenType::IMMUTABLE)
        {
            int ln = current().line;
            advance(); // consume 'immutable'
            std::string name = consume(TokenType::IDENTIFIER, "Expected variable name after 'immutable'").value;
            consume(TokenType::EQUAL, "Expected '=' after immutable variable name");
            auto value = parseExpression();
            return std::make_unique<ImmutableBinding>(name, std::move(value), ln);
        }
        if (type == TokenType::AT)
            return parseDecoratedFnDef();
        if (type == TokenType::ASYNC)
        {
            // async fn name(...): ...
            advance(); // consume async
            skipNewlines();
            if (!check(TokenType::FN))
                throw ParseError("Expected 'fn' after 'async'", current().line);
            return parseFnDef(true); // isAsync = true
        }

        // --- Augmented assignment: IDENTIFIER += EXPR etc ---
        if (type == TokenType::IDENTIFIER &&
            (peekToken(1).type == TokenType::PLUS_EQUAL ||
             peekToken(1).type == TokenType::MINUS_EQUAL ||
             peekToken(1).type == TokenType::STAR_EQUAL ||
             peekToken(1).type == TokenType::SLASH_EQUAL ||
             peekToken(1).type == TokenType::PERCENT_EQUAL ||
             peekToken(1).type == TokenType::AMP_EQUAL ||
             peekToken(1).type == TokenType::PIPE_EQUAL ||
             peekToken(1).type == TokenType::CARET_EQUAL ||
             peekToken(1).type == TokenType::LSHIFT_EQUAL ||
             peekToken(1).type == TokenType::RSHIFT_EQUAL ||
             peekToken(1).type == TokenType::STAR_STAR_EQUAL))
        {
            std::string name = current().value;
            int ln = current().line;
            advance();               // consume identifier
            Token opTok = advance(); // consume += etc.
            // Map += to +, -= to -, etc.
            std::string op;
            switch (opTok.type)
            {
            case TokenType::PLUS_EQUAL:
                op = "+";
                break;
            case TokenType::MINUS_EQUAL:
                op = "-";
                break;
            case TokenType::STAR_EQUAL:
                op = "*";
                break;
            case TokenType::SLASH_EQUAL:
                op = "/";
                break;
            case TokenType::PERCENT_EQUAL:
                op = "%";
                break;
            case TokenType::AMP_EQUAL:
                op = "&";
                break;
            case TokenType::PIPE_EQUAL:
                op = "|";
                break;
            case TokenType::CARET_EQUAL:
                op = "^";
                break;
            case TokenType::LSHIFT_EQUAL:
                op = "<<";
                break;
            case TokenType::RSHIFT_EQUAL:
                op = ">>";
                break;
            case TokenType::STAR_STAR_EQUAL:
                op = "**";
                break;
            default:
                op = "+";
                break;
            }
            ExprPtr rhs = parseExpression();
            // Desugar: x += expr → x = x + expr
            ExprPtr lhs = std::make_unique<Identifier>(name, ln);
            ExprPtr combined = std::make_unique<BinaryExpr>(std::move(lhs), op, std::move(rhs), ln);
            consumeStatementEnd();
            return std::make_unique<Assignment>(name, std::move(combined), ln);
        }

        // --- Assignment: IDENTIFIER = EXPR ---
        if (type == TokenType::IDENTIFIER && peekToken(1).type == TokenType::EQUAL)
        {
            std::string name = current().value;
            int ln = current().line;
            advance(); // consume identifier
            advance(); // consume =
            return parseAssignment(name, ln);
        }

        // --- Destructuring assignment ---
        if (type == TokenType::IDENTIFIER || type == TokenType::ELLIPSIS ||
            type == TokenType::LBRACKET || type == TokenType::LBRACE)
        {
            size_t savedPos = pos_;
            std::vector<std::string> names;
            std::unique_ptr<DestructuringPattern> pattern;
            bool parsedPattern = false;

            try
            {
                pattern = parseTopLevelDestructuringPattern(&names);
                parsedPattern = true;
            }
            catch (const ParseError &)
            {
                pos_ = savedPos;
            }

            if (parsedPattern && check(TokenType::EQUAL))
            {
                int ln = tokens_[savedPos].line;
                advance(); // consume '='
                ExprPtr value = parseExpression();
                consumeStatementEnd();
                return std::make_unique<DestructuringAssignment>(std::move(pattern), std::move(names), std::move(value), ln);
            }

            pos_ = savedPos;
        }

        // --- Expression statement (including paren-less calls) ---
        int ln = current().line;
        ExprPtr expr = parseExpression();

        // Check for paren-less function call:
        // If expr is a bare Identifier and the next token can start a primary
        // (and is NOT a statement boundary), treat as a call with space-separated args
        if (auto *ident = dynamic_cast<Identifier *>(expr.get()))
        {
            if (!check(TokenType::NEWLINE) && !check(TokenType::DOT) &&
                !check(TokenType::SEMICOLON) && !isAtEnd() &&
                canStartPrimary(current().type))
            {

                std::string callee = ident->name;
                std::vector<ExprPtr> args;

                // Parse space-separated primary expressions as arguments
                while (!check(TokenType::NEWLINE) && !check(TokenType::DOT) &&
                       !check(TokenType::SEMICOLON) && !isAtEnd() &&
                       canStartPrimary(current().type))
                {
                    args.push_back(parsePrimary());
                }

                expr = std::make_unique<CallExpr>(callee, std::move(args), ln);
            }
        }

        // Member assignment: expr->field = value  (e.g., self->x = 42, p1->name = "hello")
        if (auto *mem = dynamic_cast<MemberAccess *>(expr.get()))
        {
            if (check(TokenType::EQUAL))
            {
                advance(); // consume =
                ExprPtr value = parseExpression();
                std::string member = mem->member;
                ExprPtr object = std::move(mem->object);
                consumeStatementEnd();
                return std::make_unique<MemberAssignment>(std::move(object), member, std::move(value), ln);
            }
            // Augmented member assignment: expr->field += value  etc.
            // Store the op in the AST node; the interpreter does read-modify-write.
            if (check(TokenType::PLUS_EQUAL) || check(TokenType::MINUS_EQUAL) ||
                check(TokenType::STAR_EQUAL) || check(TokenType::SLASH_EQUAL) ||
                check(TokenType::PERCENT_EQUAL) ||
                check(TokenType::AMP_EQUAL) || check(TokenType::PIPE_EQUAL) ||
                check(TokenType::CARET_EQUAL) || check(TokenType::LSHIFT_EQUAL) ||
                check(TokenType::RSHIFT_EQUAL) || check(TokenType::STAR_STAR_EQUAL))
            {
                std::string op;
                switch (current().type)
                {
                case TokenType::PLUS_EQUAL:
                    op = "+";
                    break;
                case TokenType::MINUS_EQUAL:
                    op = "-";
                    break;
                case TokenType::STAR_EQUAL:
                    op = "*";
                    break;
                case TokenType::SLASH_EQUAL:
                    op = "/";
                    break;
                case TokenType::PERCENT_EQUAL:
                    op = "%";
                    break;
                case TokenType::AMP_EQUAL:
                    op = "&";
                    break;
                case TokenType::PIPE_EQUAL:
                    op = "|";
                    break;
                case TokenType::CARET_EQUAL:
                    op = "^";
                    break;
                case TokenType::LSHIFT_EQUAL:
                    op = "<<";
                    break;
                case TokenType::RSHIFT_EQUAL:
                    op = ">>";
                    break;
                case TokenType::STAR_STAR_EQUAL:
                    op = "**";
                    break;
                default:
                    op = "+";
                    break;
                }
                advance(); // consume +=
                ExprPtr rhs = parseExpression();
                std::string member = mem->member;
                ExprPtr object = std::move(mem->object);
                consumeStatementEnd();
                return std::make_unique<MemberAssignment>(std::move(object), member, std::move(rhs), ln, op);
            }
        }

        // Index assignment: expr[idx] = value  (e.g., list[0] = 42, map["key"] = val)
        if (auto *idx = dynamic_cast<IndexAccess *>(expr.get()))
        {
            if (check(TokenType::EQUAL))
            {
                advance(); // consume =
                ExprPtr value = parseExpression();
                ExprPtr object = std::move(idx->object);
                ExprPtr index = std::move(idx->index);
                consumeStatementEnd();
                return std::make_unique<IndexAssignment>(std::move(object), std::move(index), std::move(value), ln);
            }
            // Augmented index assignment: expr[idx] += value  etc.
            if (check(TokenType::PLUS_EQUAL) || check(TokenType::MINUS_EQUAL) ||
                check(TokenType::STAR_EQUAL) || check(TokenType::SLASH_EQUAL) ||
                check(TokenType::PERCENT_EQUAL) ||
                check(TokenType::AMP_EQUAL) || check(TokenType::PIPE_EQUAL) ||
                check(TokenType::CARET_EQUAL) || check(TokenType::LSHIFT_EQUAL) ||
                check(TokenType::RSHIFT_EQUAL) || check(TokenType::STAR_STAR_EQUAL))
            {
                std::string op;
                switch (current().type)
                {
                case TokenType::PLUS_EQUAL:
                    op = "+";
                    break;
                case TokenType::MINUS_EQUAL:
                    op = "-";
                    break;
                case TokenType::STAR_EQUAL:
                    op = "*";
                    break;
                case TokenType::SLASH_EQUAL:
                    op = "/";
                    break;
                case TokenType::PERCENT_EQUAL:
                    op = "%";
                    break;
                case TokenType::AMP_EQUAL:
                    op = "&";
                    break;
                case TokenType::PIPE_EQUAL:
                    op = "|";
                    break;
                case TokenType::CARET_EQUAL:
                    op = "^";
                    break;
                case TokenType::LSHIFT_EQUAL:
                    op = "<<";
                    break;
                case TokenType::RSHIFT_EQUAL:
                    op = ">>";
                    break;
                case TokenType::STAR_STAR_EQUAL:
                    op = "**";
                    break;
                default:
                    op = "+";
                    break;
                }
                advance(); // consume +=
                ExprPtr rhs = parseExpression();
                ExprPtr object = std::move(idx->object);
                ExprPtr index = std::move(idx->index);
                consumeStatementEnd();
                return std::make_unique<IndexAssignment>(std::move(object), std::move(index), std::move(rhs), ln, op);
            }
        }

        consumeStatementEnd();
        return std::make_unique<ExprStmt>(std::move(expr), ln);
    }

    // ============================================================
    // Assignment
    // ============================================================

    StmtPtr Parser::parseAssignment(const std::string &name, int line)
    {
        ExprPtr value = parseExpression();

        // Support paren-less function call on assignment RHS:
        //   result = check 42
        // (same shorthand behavior as expression statements)
        if (auto *ident = dynamic_cast<Identifier *>(value.get()))
        {
            if (!check(TokenType::NEWLINE) && !check(TokenType::DOT) &&
                !check(TokenType::SEMICOLON) && !isAtEnd() &&
                canStartPrimary(current().type))
            {
                std::string callee = ident->name;
                std::vector<ExprPtr> args;
                while (!check(TokenType::NEWLINE) && !check(TokenType::DOT) &&
                       !check(TokenType::SEMICOLON) && !isAtEnd() &&
                       canStartPrimary(current().type))
                {
                    args.push_back(parsePrimary());
                }
                value = std::make_unique<CallExpr>(callee, std::move(args), line);
            }
        }

        consumeStatementEnd();
        return std::make_unique<Assignment>(name, std::move(value), line);
    }

    // ============================================================
    // If / Elif / Else
    // ============================================================

    StmtPtr Parser::parseIfStmt()
    {
        int ln = current().line;
        advance(); // consume IF

        ExprPtr condition = parseExpression();
        consume(TokenType::COLON, "Expected ':' after if condition");
        auto body = parseBlock(true); // stop at elif/else too

        // Consume optional ';' between branches
        bool hadSemicolon = false;
        if (check(TokenType::SEMICOLON))
        {
            advance();
            hadSemicolon = true;
        }

        // Parse elif clauses
        std::vector<ElifClause> elifs;
        skipNewlines();
        while (check(TokenType::ELIF))
        {
            int elifLine = current().line;
            advance(); // consume ELIF
            ExprPtr elifCond = parseExpression();
            consume(TokenType::COLON, "Expected ':' after elif condition");
            auto elifBody = parseBlock(true); // stop at elif/else too

            // Consume optional ';' between branches
            if (check(TokenType::SEMICOLON))
            {
                advance();
                hadSemicolon = true;
            }
            else
            {
                hadSemicolon = false;
            }

            ElifClause clause;
            clause.condition = std::move(elifCond);
            clause.body = std::move(elifBody);
            clause.line = elifLine;
            elifs.push_back(std::move(clause));
            skipNewlines();
        }

        // Parse else clause
        std::vector<StmtPtr> elseBody;
        if (check(TokenType::ELSE))
        {
            advance(); // consume ELSE
            consume(TokenType::COLON, "Expected ':' after else");
            elseBody = parseBlock(false); // normal block, no elif/else after this

            // The ';' after the else block is the final closer
            if (!hadSemicolon || check(TokenType::SEMICOLON))
            {
                consume(TokenType::SEMICOLON, "Expected ';' to close if/elif/else block");
            }
        }
        else
        {
            // No else clause — if we didn't consume a ';' yet, need one now
            if (!hadSemicolon)
            {
                consume(TokenType::SEMICOLON, "Expected ';' to close if block");
            }
        }

        return std::make_unique<IfStmt>(
            std::move(condition), std::move(body),
            std::move(elifs), std::move(elseBody), ln);
    }

    // ============================================================
    // For loop
    // ============================================================

    StmtPtr Parser::parseForStmt()
    {
        int ln = current().line;
        advance(); // consume FOR

        // ---- Parse targets: IDENT { ',' IDENT } [ ',' '...' IDENT ] ----
        std::vector<std::string> varNames;
        bool hasRest = false;
        std::string restName;

        // First target (could be ...rest if single rest target)
        if (check(TokenType::ELLIPSIS))
        {
            advance(); // consume ...
            restName = consume(TokenType::IDENTIFIER, "Expected variable name after '...'").value;
            hasRest = true;
        }
        else
        {
            varNames.push_back(consume(TokenType::IDENTIFIER, "Expected loop variable name after 'for'").value);
        }

        // Additional targets separated by commas
        while (!hasRest && check(TokenType::COMMA))
        {
            auto nextTok = peekToken(1);
            if (nextTok.type == TokenType::ELLIPSIS)
            {
                advance(); // consume comma
                advance(); // consume ...
                restName = consume(TokenType::IDENTIFIER, "Expected variable name after '...'").value;
                hasRest = true;
                break;
            }
            else if (nextTok.type == TokenType::IDENTIFIER)
            {
                // Check if this is still a target: peek further to see if token after ident is COMMA, IN, or ELLIPSIS
                auto afterIdent = peekToken(2);
                if (afterIdent.type == TokenType::COMMA || afterIdent.type == TokenType::IN)
                {
                    advance(); // consume comma
                    varNames.push_back(consume(TokenType::IDENTIFIER, "Expected variable name").value);
                }
                else
                {
                    // Not a target — it's the start of source expressions: for a in expr, expr
                    break;
                }
            }
            else
            {
                break;
            }
        }

        consume(TokenType::IN, "Expected 'in' after for loop variable(s)");

        // ---- Parse sources: EXPR { ',' EXPR } ----
        std::vector<ExprPtr> iterables;
        iterables.push_back(parseExpression());
        while (check(TokenType::COMMA))
        {
            advance(); // consume comma
            iterables.push_back(parseExpression());
        }

        consume(TokenType::COLON, "Expected ':' after for expression");
        auto body = parseBlock();
        consume(TokenType::SEMICOLON, "Expected ';' to close for block");

        return std::make_unique<ForStmt>(std::move(varNames), std::move(iterables),
                                         std::move(body), hasRest, std::move(restName), ln);
    }

    // ============================================================
    // While loop
    // ============================================================

    StmtPtr Parser::parseWhileStmt()
    {
        int ln = current().line;
        advance(); // consume WHILE

        ExprPtr condition = parseExpression();
        consume(TokenType::COLON, "Expected ':' after while condition");
        auto body = parseBlock();
        consume(TokenType::SEMICOLON, "Expected ';' to close while block");

        return std::make_unique<WhileStmt>(std::move(condition), std::move(body), ln);
    }

    // ============================================================
    // Infinite loop statement:  loop : BLOCK ;
    // ============================================================

    StmtPtr Parser::parseLoopStmt()
    {
        int ln = current().line;
        advance(); // consume LOOP

        consume(TokenType::COLON, "Expected ':' after 'loop'");
        auto body = parseBlock();
        consume(TokenType::SEMICOLON, "Expected ';' to close loop block");

        return std::make_unique<LoopStmt>(std::move(body), /*safeLoop=*/false, ln);
    }

    // ============================================================
    // Do-while statement:  do : BLOCK ; while CONDITION
    // ============================================================

    StmtPtr Parser::parseDoWhileStmt()
    {
        int ln = current().line;
        advance(); // consume DO

        consume(TokenType::COLON, "Expected ':' after 'do'");
        auto body = parseBlock();
        consume(TokenType::SEMICOLON, "Expected ';' to close do block");

        consume(TokenType::WHILE, "Expected 'while' after do block");
        ExprPtr condition = parseExpression();

        return std::make_unique<DoWhileStmt>(std::move(body), std::move(condition), ln);
    }

    // ============================================================
    // Function definition
    // ============================================================

    StmtPtr Parser::parseFnDef(bool isAsync)
    {
        int ln = current().line;
        advance(); // consume FN

        std::string name = consume(TokenType::IDENTIFIER, "Expected function name after 'fn'").value;
        consume(TokenType::LPAREN, "Expected '(' after function name");

        std::vector<std::string> params;
        std::vector<ExprPtr> defaults;
        std::vector<std::string> paramTypes;
        bool isVariadic = false;
        std::string variadicName;

        if (!check(TokenType::RPAREN))
        {
            // Check for variadic: ...name
            if (check(TokenType::ELLIPSIS))
            {
                advance(); // consume ...
                variadicName = consume(TokenType::IDENTIFIER, "Expected parameter name after '...'").value;
                isVariadic = true;
            }
            else
            {
                // Check for typed parameter: Type(name) syntax
                // If IDENTIFIER followed by LPAREN, it could be Type(name)
                if (check(TokenType::IDENTIFIER) && peekToken(1).type == TokenType::LPAREN)
                {
                    // Lookahead: IDENTIFIER LPAREN IDENTIFIER RPAREN → Type(name)
                    size_t savedPos = pos_;
                    std::string potentialType = current().value;
                    advance(); // consume potential type name
                    advance(); // consume (
                    if (check(TokenType::IDENTIFIER))
                    {
                        std::string paramName = current().value;
                        advance(); // consume param name
                        if (check(TokenType::RPAREN))
                        {
                            advance(); // consume )
                            params.push_back(paramName);
                            paramTypes.push_back(potentialType);
                        }
                        else
                        {
                            // Not Type(name), backtrack
                            pos_ = savedPos;
                            params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name").value);
                            // Check for colon type annotation
                            if (check(TokenType::COLON) && peekToken(1).type == TokenType::IDENTIFIER)
                            {
                                advance(); // consume :
                                paramTypes.push_back(consume(TokenType::IDENTIFIER, "Expected type name").value);
                            }
                            else
                            {
                                paramTypes.push_back("");
                            }
                        }
                    }
                    else
                    {
                        // Not Type(name), backtrack
                        pos_ = savedPos;
                        params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name").value);
                        if (check(TokenType::COLON) && peekToken(1).type == TokenType::IDENTIFIER)
                        {
                            advance();
                            paramTypes.push_back(consume(TokenType::IDENTIFIER, "Expected type name").value);
                        }
                        else
                        {
                            paramTypes.push_back("");
                        }
                    }
                }
                else
                {
                    params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name").value);
                    // Check for type annotation: param: Type
                    if (check(TokenType::COLON))
                    {
                        // Lookahead: is this a type annotation (identifier) or a block start?
                        if (peekToken(1).type == TokenType::IDENTIFIER)
                        {
                            advance(); // consume :
                            paramTypes.push_back(consume(TokenType::IDENTIFIER, "Expected type name").value);
                        }
                        else
                        {
                            paramTypes.push_back(""); // no annotation
                        }
                    }
                    else
                    {
                        paramTypes.push_back(""); // no annotation
                    }
                }
                // Check for default value
                if (check(TokenType::EQUAL))
                {
                    advance(); // consume =
                    defaults.push_back(parseExpression());
                }
                else
                {
                    defaults.push_back(nullptr);
                }

                while (check(TokenType::COMMA))
                {
                    advance(); // consume comma
                    // Check for variadic: ...name
                    if (check(TokenType::ELLIPSIS))
                    {
                        advance(); // consume ...
                        variadicName = consume(TokenType::IDENTIFIER, "Expected parameter name after '...'").value;
                        isVariadic = true;
                        break; // variadic must be last
                    }
                    // Check for typed parameter: Type(name) syntax
                    if (check(TokenType::IDENTIFIER) && peekToken(1).type == TokenType::LPAREN)
                    {
                        size_t savedPos = pos_;
                        std::string potentialType = current().value;
                        advance(); // consume potential type name
                        advance(); // consume (
                        if (check(TokenType::IDENTIFIER))
                        {
                            std::string paramName = current().value;
                            advance(); // consume param name
                            if (check(TokenType::RPAREN))
                            {
                                advance(); // consume )
                                params.push_back(paramName);
                                paramTypes.push_back(potentialType);
                            }
                            else
                            {
                                pos_ = savedPos;
                                params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name after ','").value);
                                if (check(TokenType::COLON) && peekToken(1).type == TokenType::IDENTIFIER)
                                {
                                    advance();
                                    paramTypes.push_back(consume(TokenType::IDENTIFIER, "Expected type name").value);
                                }
                                else
                                {
                                    paramTypes.push_back("");
                                }
                            }
                        }
                        else
                        {
                            pos_ = savedPos;
                            params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name after ','").value);
                            if (check(TokenType::COLON) && peekToken(1).type == TokenType::IDENTIFIER)
                            {
                                advance();
                                paramTypes.push_back(consume(TokenType::IDENTIFIER, "Expected type name").value);
                            }
                            else
                            {
                                paramTypes.push_back("");
                            }
                        }
                    }
                    else
                    {
                        params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name after ','").value);
                        // Check for type annotation
                        if (check(TokenType::COLON))
                        {
                            if (peekToken(1).type == TokenType::IDENTIFIER)
                            {
                                advance(); // consume :
                                paramTypes.push_back(consume(TokenType::IDENTIFIER, "Expected type name").value);
                            }
                            else
                            {
                                paramTypes.push_back("");
                            }
                        }
                        else
                        {
                            paramTypes.push_back("");
                        }
                    }
                    if (check(TokenType::EQUAL))
                    {
                        advance(); // consume =
                        defaults.push_back(parseExpression());
                    }
                    else
                    {
                        defaults.push_back(nullptr);
                    }
                }
            }
        }
        consume(TokenType::RPAREN, "Expected ')' after parameters");

        // Optional return type annotation: -> Type
        std::string returnType;
        if (check(TokenType::ARROW))
        {
            advance(); // consume ->
            returnType = consume(TokenType::IDENTIFIER, "Expected return type after '->'").value;
        }

        consume(TokenType::COLON, "Expected ':' after function signature");
        auto body = parseBlock();
        consume(TokenType::SEMICOLON, "Expected ';' to close function body");

        auto fnDef = std::make_unique<FnDef>(name, std::move(params), std::move(body), ln);
        fnDef->defaults = std::move(defaults);
        fnDef->isVariadic = isVariadic;
        fnDef->variadicName = std::move(variadicName);
        fnDef->paramTypes = std::move(paramTypes);
        fnDef->returnType = std::move(returnType);
        fnDef->isAsync = isAsync;
        return fnDef;
    }

    // ============================================================
    // Try / Catch / Finally
    // ============================================================

    StmtPtr Parser::parseTryCatchStmt()
    {
        int ln = current().line;
        advance(); // consume TRY

        consume(TokenType::COLON, "Expected ':' after 'try'");
        auto tryBody = parseBlock();
        consume(TokenType::SEMICOLON, "Expected ';' to close try block");
        skipNewlines();

        // Parse one or more catch clauses:
        //   catch varname :                          (catch-all)
        //   catch varname is TypeError :             (single type)
        //   catch varname is TypeError or ValueError : (multi-type)
        std::vector<CatchClause> catchClauses;
        while (check(TokenType::CATCH))
        {
            advance(); // consume CATCH
            CatchClause clause;
            clause.varName = consume(TokenType::IDENTIFIER, "Expected variable name after 'catch'").value;

            // Optional type filter: is TypeName [or TypeName]*
            if (check(TokenType::IS))
            {
                advance(); // consume IS
                clause.errorTypes.push_back(
                    consume(TokenType::IDENTIFIER, "Expected error type name after 'is'").value);
                while (check(TokenType::OR))
                {
                    advance(); // consume OR
                    clause.errorTypes.push_back(
                        consume(TokenType::IDENTIFIER, "Expected error type name after 'or'").value);
                }
            }

            consume(TokenType::COLON, "Expected ':' after catch clause");
            clause.body = parseBlock();
            consume(TokenType::SEMICOLON, "Expected ';' to close catch block");
            skipNewlines();
            catchClauses.push_back(std::move(clause));
        }

        // optional finally :
        std::vector<StmtPtr> finallyBody;
        if (check(TokenType::FINALLY))
        {
            advance(); // consume FINALLY
            consume(TokenType::COLON, "Expected ':' after 'finally'");
            finallyBody = parseBlock();
            consume(TokenType::SEMICOLON, "Expected ';' to close finally block");
        }

        return std::make_unique<TryCatchStmt>(
            std::move(tryBody), std::move(catchClauses),
            std::move(finallyBody), ln);
    }

    // ============================================================
    // Throw statement
    // ============================================================

    StmtPtr Parser::parseThrowStmt()
    {
        int ln = current().line;
        advance(); // consume THROW

        // Bare throw (re-throw) — no expression follows
        ExprPtr value = nullptr;
        skipNewlines();
        if (!check(TokenType::NEWLINE) && !check(TokenType::SEMICOLON) &&
            !check(TokenType::EOF_TOKEN) && canStartPrimary(current().type))
        {
            value = parseExpression();
        }
        consumeStatementEnd();
        return std::make_unique<ThrowStmt>(std::move(value), ln);
    }

    // ============================================================
    // InCase (switch/match)
    // ============================================================

    // Helper: check if an identifier is a built-in type name for incase type patterns
    static bool isBuiltinTypeName(const std::string &name)
    {
        static const std::unordered_set<std::string> typeNames = {
            "int", "float", "complex", "bool", "string", "list", "tuple",
            "set", "frozen_set", "map", "function", "enum", "bytes",
            "generator", "struct_def", "module", "none"};
        return typeNames.count(name) > 0;
    }

    StmtPtr Parser::parseInCaseStmt()
    {
        int ln = current().line;
        advance(); // consume INCASE

        ExprPtr subject = parseExpression();
        consume(TokenType::COLON, "Expected ':' after incase expression");
        skipNewlines();

        std::vector<InCaseClause> clauses;
        std::vector<StmtPtr> elseBody;

        // Parse clauses: is / belong / bind
        while (check(TokenType::IS) || check(TokenType::BELONG) || check(TokenType::BIND))
        {
            InCaseClause clause;
            clause.line = current().line;
            TokenType kw = current().type;
            advance(); // consume IS / BELONG / BIND

            if (kw == TokenType::BELONG)
            {
                // belong TypeName [if guard] :
                clause.kind = ClauseKind::BELONG_TYPE;
                // Accept IDENTIFIER or keyword type names (int, string, bool, none, etc.)
                std::string typeName;
                if (check(TokenType::IDENTIFIER))
                    typeName = current().value;
                else if (isBuiltinTypeName(current().value))
                    typeName = current().value; // e.g., "none", "int", "string"
                else
                    throw ParseError("Expected type name after 'belong'", clause.line);
                clause.typeName = typeName;
                advance();
                goto parse_guard;
            }
            else if (kw == TokenType::BIND)
            {
                // bind varname [if guard] :
                clause.kind = ClauseKind::BIND_CAPTURE;
                if (!check(TokenType::IDENTIFIER))
                    throw ParseError("Expected variable name after 'bind'", clause.line);
                clause.bindName = current().value;
                advance();
                goto parse_guard;
            }
            // else IS: fall through to parse values

        parse_values:
            // First value
            clause.values.push_back(parseLogicalAnd());
            // Additional values separated by 'or'
            while (check(TokenType::OR))
            {
                advance(); // consume OR
                clause.values.push_back(parseLogicalAnd());
            }

        parse_guard:
            // Optional guard clause: if CONDITION
            if (check(TokenType::IF))
            {
                advance(); // consume IF
                clause.guard = parseExpression();
            }

            consume(TokenType::COLON, "Expected ':' after incase clause");
            clause.body = parseBlock();
            consume(TokenType::SEMICOLON, "Expected ';' to close incase clause");
            skipNewlines();

            clauses.push_back(std::move(clause));
        }

        // Optional else clause
        if (check(TokenType::ELSE))
        {
            advance(); // consume ELSE
            consume(TokenType::COLON, "Expected ':' after 'else'");
            elseBody = parseBlock();
            consume(TokenType::SEMICOLON, "Expected ';' to close else block");
            skipNewlines();
        }

        // Closing semicolon for the entire incase
        consume(TokenType::SEMICOLON, "Expected ';' to close incase statement");

        return std::make_unique<InCaseStmt>(
            std::move(subject), std::move(clauses), std::move(elseBody), ln);
    }

    // ============================================================
    // Enum definition: enum Name: Member1, Member2, ...;
    // Also supports custom values: enum Name: A = 1, B = 2;
    // ============================================================

    StmtPtr Parser::parseEnumDef()
    {
        int ln = current().line;
        advance(); // consume ENUM

        std::string name = consume(TokenType::IDENTIFIER, "Expected enum name after 'enum'").value;
        consume(TokenType::COLON, "Expected ':' after enum name");
        skipNewlines();

        std::vector<std::string> members;
        std::vector<ExprPtr> memberValues;

        // Parse first member
        members.push_back(consume(TokenType::IDENTIFIER, "Expected enum member name").value);
        if (check(TokenType::EQUAL))
        {
            advance(); // consume =
            memberValues.push_back(parseExpression());
        }
        else
        {
            memberValues.push_back(nullptr); // auto-increment
        }

        while (check(TokenType::COMMA))
        {
            advance(); // consume comma
            skipNewlines();
            if (check(TokenType::SEMICOLON))
                break; // trailing comma
            members.push_back(consume(TokenType::IDENTIFIER, "Expected enum member name").value);
            if (check(TokenType::EQUAL))
            {
                advance(); // consume =
                memberValues.push_back(parseExpression());
            }
            else
            {
                memberValues.push_back(nullptr);
            }
        }

        consume(TokenType::SEMICOLON, "Expected ';' to close enum definition");

        return std::make_unique<EnumDef>(name, std::move(members), std::move(memberValues), ln);
    }

    // ============================================================
    // Struct definition: struct Name : field = default ... fn method(...) : ... ; ;
    // ============================================================

    StmtPtr Parser::parseStructDef()
    {
        int ln = current().line;
        advance(); // consume STRUCT

        std::string name = consume(TokenType::IDENTIFIER, "Expected struct name after 'struct'").value;
        consume(TokenType::COLON, "Expected ':' after struct name");
        skipNewlines();

        std::vector<StructFieldDef> fields;
        std::vector<std::unique_ptr<FnDef>> methods;

        // Parse struct body until closing ';'
        while (!check(TokenType::SEMICOLON) && !isAtEnd())
        {
            skipNewlines();
            if (check(TokenType::SEMICOLON))
                break;

            // Method definition: fn name(...) : ... ;
            if (check(TokenType::FN))
            {
                auto fnStmt = parseFnDef();
                auto *fn = dynamic_cast<FnDef *>(fnStmt.get());
                if (fn)
                {
                    fnStmt.release();
                    methods.push_back(std::unique_ptr<FnDef>(fn));
                }
                skipNewlines();
                continue;
            }

            // Field definition: name = expr
            if (check(TokenType::IDENTIFIER) && peekToken(1).type == TokenType::EQUAL)
            {
                StructFieldDef field;
                field.line = current().line;
                field.name = current().value;
                advance(); // consume field name
                advance(); // consume =
                field.defaultValue = parseExpression();
                fields.push_back(std::move(field));
                // Consume optional newline/dot statement terminator
                if (check(TokenType::NEWLINE) || check(TokenType::DOT))
                    advance();
                skipNewlines();
                continue;
            }

            throw ParseError("Expected field definition or method in struct body", current().line);
        }

        consume(TokenType::SEMICOLON, "Expected ';' to close struct definition");

        return std::make_unique<StructDef>(name, std::move(fields), std::move(methods), ln);
    }

    // ============================================================
    // Class definition: class Name [inherits Parent1, Parent2] : body ;
    // ============================================================

    StmtPtr Parser::parseClassDef(bool isAbstract, bool isMixin)
    {
        int ln = current().line;
        if (!isAbstract && !isMixin)
            advance(); // consume CLASS (abstract/mixin already consumed by caller)

        std::string errorContext = isMixin ? "mixin" : (isAbstract ? "abstract" : "class");
        std::string name = consume(TokenType::IDENTIFIER, "Expected name after '" + errorContext + "'").value;

        // Parse optional 'inherits' clause
        std::vector<std::string> parents;
        if (check(TokenType::INHERITS))
        {
            advance(); // consume INHERITS
            parents.push_back(consume(TokenType::IDENTIFIER, "Expected parent class name after 'inherits'").value);
            while (check(TokenType::COMMA))
            {
                advance(); // consume comma
                parents.push_back(consume(TokenType::IDENTIFIER, "Expected parent class name").value);
            }
        }

        // Parse optional 'with' clause (mixins)
        std::vector<std::string> mixins;
        if (check(TokenType::WITH))
        {
            advance(); // consume WITH
            mixins.push_back(consume(TokenType::IDENTIFIER, "Expected mixin name after 'with'").value);
            while (check(TokenType::COMMA))
            {
                advance(); // consume comma
                mixins.push_back(consume(TokenType::IDENTIFIER, "Expected mixin name").value);
            }
        }

        // Parse optional 'implements' clause
        std::vector<std::string> interfaces;
        if (check(TokenType::IMPLEMENTS))
        {
            advance(); // consume IMPLEMENTS
            interfaces.push_back(consume(TokenType::IDENTIFIER, "Expected interface name after 'implements'").value);
            while (check(TokenType::COMMA))
            {
                advance(); // consume comma
                interfaces.push_back(consume(TokenType::IDENTIFIER, "Expected interface name").value);
            }
        }

        consume(TokenType::COLON, "Expected ':' after class name");
        skipNewlines();

        std::vector<StructFieldDef> fields;
        std::vector<std::unique_ptr<FnDef>> methods;
        std::vector<PropertyDef> properties;

        // Current access level — default is public (everything before any block)
        AccessLevel currentAccess = AccessLevel::PUBLIC;

        // Parse class body until closing ';'
        while (!check(TokenType::SEMICOLON) && !isAtEnd())
        {
            skipNewlines();
            if (check(TokenType::SEMICOLON))
                break;

            // Access control labels: private: / protected: / public:
            if (check(TokenType::PRIVATE) || check(TokenType::PROTECTED) || check(TokenType::PUBLIC))
            {
                if (check(TokenType::PRIVATE))
                    currentAccess = AccessLevel::PRIVATE;
                else if (check(TokenType::PROTECTED))
                    currentAccess = AccessLevel::PROTECTED;
                else
                    currentAccess = AccessLevel::PUBLIC;
                advance(); // consume the access keyword
                consume(TokenType::COLON, "Expected ':' after access modifier");
                skipNewlines();
                continue;
            }

            // Static members: static fn name(...) : ... ; or static name = expr
            if (check(TokenType::STATIC))
            {
                advance(); // consume 'static'

                if (check(TokenType::FN))
                {
                    auto fnStmt = parseFnDef();
                    auto *fn = dynamic_cast<FnDef *>(fnStmt.get());
                    if (fn)
                    {
                        fn->access = currentAccess;
                        fn->isStatic = true;
                        fnStmt.release();
                        methods.push_back(std::unique_ptr<FnDef>(fn));
                    }
                    skipNewlines();
                    continue;
                }

                if (check(TokenType::IDENTIFIER) && peekToken(1).type == TokenType::EQUAL)
                {
                    StructFieldDef field;
                    field.line = current().line;
                    field.name = current().value;
                    field.access = currentAccess;
                    field.isStatic = true;
                    advance(); // consume field name
                    advance(); // consume =
                    field.defaultValue = parseExpression();
                    fields.push_back(std::move(field));
                    if (check(TokenType::NEWLINE) || check(TokenType::DOT))
                        advance();
                    skipNewlines();
                    continue;
                }

                throw ParseError("Expected field or method after 'static'", current().line);
            }

            // Property getter: get name(self) : ... ;
            // Property setter: set name(self, val) : ... ;
            // "get" and "set" are contextual keywords (identifiers outside class bodies)
            if (check(TokenType::IDENTIFIER) &&
                (current().value == "get" || current().value == "set") &&
                peekToken(1).type == TokenType::IDENTIFIER)
            {
                bool isGetter = (current().value == "get");
                int propLine = current().line;
                advance(); // consume get/set

                std::string propName = consume(TokenType::IDENTIFIER,
                                               isGetter ? "Expected property name after 'get'" : "Expected property name after 'set'")
                                           .value;

                // Parse as a function definition: (params) : body ;
                consume(TokenType::LPAREN, "Expected '(' after property name");
                std::vector<std::string> params;
                if (!check(TokenType::RPAREN))
                {
                    params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter").value);
                    while (check(TokenType::COMMA))
                    {
                        advance();
                        params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter").value);
                    }
                }
                consume(TokenType::RPAREN, "Expected ')' after parameters");
                consume(TokenType::COLON, "Expected ':' after property parameters");
                skipNewlines();

                // Parse body statements until ';'
                std::vector<StmtPtr> body;
                while (!check(TokenType::SEMICOLON) && !isAtEnd())
                {
                    body.push_back(parseStatement());
                    skipNewlines();
                }
                consume(TokenType::SEMICOLON, "Expected ';' to end property body");

                auto fnDef = std::make_unique<FnDef>(
                    (isGetter ? "__get_" : "__set_") + propName,
                    std::move(params), std::move(body), propLine);

                // Find or create the property entry
                PropertyDef *propDef = nullptr;
                for (auto &p : properties)
                {
                    if (p.name == propName)
                    {
                        propDef = &p;
                        break;
                    }
                }
                if (!propDef)
                {
                    PropertyDef newProp;
                    newProp.name = propName;
                    newProp.line = propLine;
                    newProp.access = currentAccess;
                    properties.push_back(std::move(newProp));
                    propDef = &properties.back();
                }

                if (isGetter)
                    propDef->getter = std::move(fnDef);
                else
                    propDef->setter = std::move(fnDef);

                skipNewlines();
                continue;
            }

            // Method definition: fn name(...) : ... ;
            // In abstract classes, fn name(...) ; (no colon/body) is an abstract method
            if (check(TokenType::FN))
            {
                if (isAbstract)
                {
                    // Check if this is an abstract method (no body)
                    // Save position, parse name + params, check if next is ; (abstract) or : (default)
                    size_t savedPos = pos_;
                    advance(); // consume FN
                    if (check(TokenType::IDENTIFIER))
                    {
                        std::string methodName = current().value;
                        int methodLn = current().line;
                        advance(); // consume name
                        if (check(TokenType::LPAREN))
                        {
                            advance(); // consume (
                            std::vector<std::string> params;
                            if (!check(TokenType::RPAREN))
                            {
                                params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter").value);
                                while (check(TokenType::COMMA))
                                {
                                    advance();
                                    params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter").value);
                                }
                            }
                            consume(TokenType::RPAREN, "Expected ')' after parameters");
                            skipNewlines();
                            if (check(TokenType::SEMICOLON))
                            {
                                // Abstract method — no body
                                advance(); // consume ;
                                auto fn = std::make_unique<FnDef>(methodName, std::move(params),
                                                                  std::vector<StmtPtr>{}, methodLn);
                                fn->access = currentAccess;
                                fn->isAbstract = true;
                                methods.push_back(std::move(fn));
                                skipNewlines();
                                continue;
                            }
                        }
                    }
                    // Not abstract — backtrack and parse as normal method
                    pos_ = savedPos;
                }

                auto fnStmt = parseFnDef();
                auto *fn = dynamic_cast<FnDef *>(fnStmt.get());
                if (fn)
                {
                    fn->access = currentAccess;
                    fnStmt.release();
                    methods.push_back(std::unique_ptr<FnDef>(fn));
                }
                skipNewlines();
                continue;
            }

            // Field definition: name = expr
            if (check(TokenType::IDENTIFIER) && peekToken(1).type == TokenType::EQUAL)
            {
                StructFieldDef field;
                field.line = current().line;
                field.name = current().value;
                field.access = currentAccess;
                advance(); // consume field name
                advance(); // consume =
                field.defaultValue = parseExpression();
                fields.push_back(std::move(field));
                if (check(TokenType::NEWLINE) || check(TokenType::DOT))
                    advance();
                skipNewlines();
                continue;
            }

            throw ParseError("Expected field definition or method in class body", current().line);
        }

        consume(TokenType::SEMICOLON, "Expected ';' to close class definition");

        return std::make_unique<ClassDef>(name, std::move(parents), std::move(mixins), std::move(interfaces), std::move(fields), std::move(methods), std::move(properties), ln, isAbstract, isMixin);
    }

    // ============================================================
    // Interface definition: interface Name : fn method(self) ; fn method2(self, x) ; ;
    // ============================================================

    StmtPtr Parser::parseInterfaceDef()
    {
        int ln = current().line;
        advance(); // consume INTERFACE

        std::string name = consume(TokenType::IDENTIFIER, "Expected interface name after 'interface'").value;

        consume(TokenType::COLON, "Expected ':' after interface name");
        skipNewlines();

        std::vector<InterfaceMethodSig> sigs;

        while (!check(TokenType::SEMICOLON) && !isAtEnd())
        {
            skipNewlines();
            if (check(TokenType::SEMICOLON))
                break;

            // Each method signature: fn name(params) ;
            if (!check(TokenType::FN))
                throw ParseError("Expected 'fn' in interface body (interfaces can only contain method signatures)", current().line);

            advance(); // consume FN
            std::string methodName = consume(TokenType::IDENTIFIER, "Expected method name in interface").value;
            consume(TokenType::LPAREN, "Expected '(' after method name in interface");

            int paramCount = 0;
            if (!check(TokenType::RPAREN))
            {
                consume(TokenType::IDENTIFIER, "Expected parameter name"); // first param (typically 'self')
                paramCount++;
                while (check(TokenType::COMMA))
                {
                    advance(); // consume comma
                    consume(TokenType::IDENTIFIER, "Expected parameter name");
                    paramCount++;
                }
            }
            consume(TokenType::RPAREN, "Expected ')' after interface method parameters");

            // Interface methods have NO body — just end with ;
            consume(TokenType::SEMICOLON, "Expected ';' after interface method signature (no body allowed)");
            skipNewlines();

            InterfaceMethodSig sig;
            sig.name = methodName;
            sig.paramCount = paramCount;
            sig.line = ln;
            sigs.push_back(std::move(sig));
        }

        consume(TokenType::SEMICOLON, "Expected ';' to close interface definition");

        return std::make_unique<InterfaceDef>(name, std::move(sigs), ln);
    }

    // ============================================================
    // Decorated function/class: @decorator fn/class/abstract/mixin ...
    // Multiple decorators stack: @dec1 @dec2 fn/class name(...): ... ;
    // ============================================================

    StmtPtr Parser::parseDecoratedFnDef()
    {
        int ln = current().line;
        std::vector<std::string> decorators;

        // Collect all @decorator tokens
        while (check(TokenType::AT))
        {
            advance(); // consume @
            // Accept IDENTIFIER or keyword tokens that can serve as decorator names
            if (check(TokenType::IDENTIFIER))
            {
                decorators.push_back(current().value);
                advance();
            }
            else if (check(TokenType::IMMUTABLE))
            {
                decorators.push_back("immutable");
                advance();
            }
            else
            {
                throw ParseError("Expected decorator name after '@'", current().line);
            }

            // ── Debug/trace standalone decorators (Phase 5) ──────
            // These are standalone statements, not function/class decorators
            const std::string &lastDec = decorators.back();

            // @debug on / @debug off / @debug sample N / @debug (before fn)
            if (lastDec == "debug" && decorators.size() == 1)
            {
                skipNewlines();
                // @debug on
                if (check(TokenType::IDENTIFIER) && current().value == "on")
                {
                    advance();
                    return std::make_unique<DebugToggleStmt>(true, ln);
                }
                // @debug off
                if (check(TokenType::IDENTIFIER) && current().value == "off")
                {
                    advance();
                    return std::make_unique<DebugToggleStmt>(false, ln);
                }
                // @debug sample N
                if (check(TokenType::IDENTIFIER) && current().value == "sample")
                {
                    advance();
                    skipNewlines();
                    if (!check(TokenType::NUMBER))
                        throw ParseError("Expected number after '@debug sample'", current().line);
                    int n = std::stoi(current().value);
                    advance();
                    return std::make_unique<DebugSampleStmt>(n, ln);
                }
                // @debug before fn → falls through to normal decorator path
            }

            // @breakpoint("name") / @breakpoint pause / @breakpoint pause N
            // @breakpoint("name") when EXPR
            if (lastDec == "breakpoint" && decorators.size() == 1)
            {
                auto bp = std::make_unique<BreakpointStmt>(ln);
                skipNewlines();
                // @breakpoint pause [N]
                if (check(TokenType::IDENTIFIER) && current().value == "pause")
                {
                    advance();
                    bp->isPause = true;
                    skipNewlines();
                    // Optional timeout in seconds
                    if (check(TokenType::NUMBER))
                    {
                        bp->pauseSeconds = std::stoi(current().value);
                        advance();
                    }
                    return bp;
                }
                // @breakpoint("name") [when EXPR]
                if (check(TokenType::LPAREN))
                {
                    advance(); // consume (
                    if (check(TokenType::STRING))
                    {
                        bp->name = current().value;
                        advance();
                    }
                    if (!check(TokenType::RPAREN))
                        throw ParseError("Expected ')' after breakpoint name", current().line);
                    advance(); // consume )
                    skipNewlines();
                    // Optional: when EXPR
                    if (check(TokenType::IDENTIFIER) && current().value == "when")
                    {
                        advance();
                        skipNewlines();
                        bp->condition = parseExpression();
                    }
                    return bp;
                }
                // @breakpoint with no args — anonymous snapshot
                return bp;
            }

            // @watch("expression")
            if (lastDec == "watch" && decorators.size() == 1)
            {
                skipNewlines();
                if (!check(TokenType::LPAREN))
                    throw ParseError("Expected '(' after '@watch'", current().line);
                advance(); // consume (
                if (!check(TokenType::STRING))
                    throw ParseError("Expected string expression in @watch(\"...\")", current().line);
                std::string expr = current().value;
                advance();
                if (!check(TokenType::RPAREN))
                    throw ParseError("Expected ')' after watch expression", current().line);
                advance(); // consume )
                // Parse the expression string into an AST node
                Lexer watchLexer(expr);
                auto watchTokens = watchLexer.tokenize();
                Parser watchParser(watchTokens);
                ExprPtr parsedExpr = watchParser.parseExpression();
                return std::make_unique<WatchStmt>(std::move(expr), std::move(parsedExpr), ln);
            }

            // @checkpoint("name")
            if (lastDec == "checkpoint" && decorators.size() == 1)
            {
                skipNewlines();
                if (!check(TokenType::LPAREN))
                    throw ParseError("Expected '(' after '@checkpoint'", current().line);
                advance();
                if (!check(TokenType::STRING))
                    throw ParseError("Expected string name in @checkpoint(\"...\")", current().line);
                std::string name = current().value;
                advance();
                if (!check(TokenType::RPAREN))
                    throw ParseError("Expected ')' after checkpoint name", current().line);
                advance();
                return std::make_unique<CheckpointStmt>(std::move(name), ln);
            }

            // @track / @notrack — selective tracing
            if ((lastDec == "track" || lastDec == "notrack") && decorators.size() == 1)
            {
                auto track = std::make_unique<TrackStmt>(ln);
                track->isNotrack = (lastDec == "notrack");
                // Do NOT skipNewlines here — categories must be on same line as @track
                // A newline after @track means "no categories" (valid no-op)

                // Helper lambda: check if current token is a usable category name
                // Category names can be identifiers OR keywords (fn, class, for, while, loop)
                auto isTrackCategory = [&]() -> bool
                {
                    return check(TokenType::IDENTIFIER) ||
                           check(TokenType::FN) ||
                           check(TokenType::CLASS) ||
                           check(TokenType::FOR) ||
                           check(TokenType::WHILE) ||
                           check(TokenType::LOOP);
                };

                // Parse categories and category(items) groups
                while (isTrackCategory())
                {
                    std::string cat = current().value;
                    advance();
                    // Check for parenthesized items: var(x,y,z), fn(a,b), class(C)
                    if (check(TokenType::LPAREN))
                    {
                        advance(); // consume (
                        std::vector<std::string> items;
                        while (!check(TokenType::RPAREN) && !isAtEnd())
                        {
                            if (check(TokenType::IDENTIFIER))
                            {
                                items.push_back(current().value);
                                advance();
                            }
                            if (check(TokenType::COMMA))
                                advance();
                        }
                        if (!check(TokenType::RPAREN))
                            throw ParseError("Expected ')' in @" + lastDec + " " + cat + "(...)", current().line);
                        advance(); // consume )
                        if (cat == "var")
                            track->vars = std::move(items);
                        else if (cat == "fn")
                            track->fns = std::move(items);
                        else if (cat == "class")
                            track->classes = std::move(items);
                        else if (cat == "obj")
                            track->objs = std::move(items);
                        else
                            throw ParseError("Unknown @" + lastDec + " category '" + cat + "'; expected var, fn, class, or obj", current().line);
                    }
                    else
                    {
                        // Bare category name: loop, conditions, scope, etc.
                        track->categories.push_back(cat);
                    }
                    // Do NOT skipNewlines here — @track ends at end of line
                }
                return track;
            }

            // @profile fn myFunc — measure function execution time
            // @profile — profile next statement
            if (lastDec == "profile" && decorators.size() == 1)
            {
                std::string targetFn;
                skipNewlines();
                if (check(TokenType::FN))
                {
                    advance(); // consume 'fn'
                    if (check(TokenType::IDENTIFIER))
                    {
                        targetFn = current().value;
                        advance();
                    }
                    else
                    {
                        throw ParseError("Expected function name after @profile fn", current().line);
                    }
                }
                return std::make_unique<ProfileStmt>(std::move(targetFn), ln);
            }

            // @log "message" — always log
            // @log when EXPR "message" — conditional log
            if (lastDec == "log" && decorators.size() == 1)
            {
                skipNewlines();
                ExprPtr condition;
                // Check for "when" keyword
                if (check(TokenType::IDENTIFIER) && current().value == "when")
                {
                    advance(); // consume 'when'
                    skipNewlines();
                    // Parse condition expression up to the string literal
                    // We'll collect tokens until we see a STRING
                    condition = parseExpression();
                }
                skipNewlines();
                // Now expect a string literal for the message
                if (!check(TokenType::STRING))
                    throw ParseError("Expected string message after @log", current().line);
                std::string msg = current().value;
                advance();
                return std::make_unique<LogStmt>(std::move(msg), std::move(condition), ln);
            }

            // ── End debug/trace decorators ──────────────────────

            skipNewlines();
        }

        // Check for class / abstract class / mixin after decorators
        if (check(TokenType::CLASS))
        {
            auto classStmt = parseClassDef();
            auto classDef = std::unique_ptr<ClassDef>(static_cast<ClassDef *>(classStmt.release()));
            return std::make_unique<DecoratedClassDef>(std::move(decorators), std::move(classDef), ln);
        }
        if (check(TokenType::ABSTRACT))
        {
            advance(); // consume abstract
            skipNewlines();
            auto classStmt = parseClassDef(true, false);
            auto classDef = std::unique_ptr<ClassDef>(static_cast<ClassDef *>(classStmt.release()));
            return std::make_unique<DecoratedClassDef>(std::move(decorators), std::move(classDef), ln);
        }
        if (check(TokenType::MIXIN))
        {
            advance(); // consume mixin
            skipNewlines();
            auto classStmt = parseClassDef(false, true);
            auto classDef = std::unique_ptr<ClassDef>(static_cast<ClassDef *>(classStmt.release()));
            return std::make_unique<DecoratedClassDef>(std::move(decorators), std::move(classDef), ln);
        }

        // @safe_loop on loop statement
        if (check(TokenType::LOOP))
        {
            bool hasSafeLoop = false;
            for (const auto &dec : decorators)
            {
                if (dec == "safe_loop")
                    hasSafeLoop = true;
                else
                    throw ParseError("Unknown decorator '@" + dec + "' for loop statement; only @safe_loop is supported", ln);
            }
            auto loopStmt = parseLoopStmt();
            auto loopDef = std::unique_ptr<LoopStmt>(static_cast<LoopStmt *>(loopStmt.release()));
            loopDef->safeLoop = hasSafeLoop;
            return loopDef;
        }

        // @eager on bring statement
        if (check(TokenType::BRING) || check(TokenType::FROM))
        {
            bool hasEager = false;
            for (const auto &dec : decorators)
            {
                if (dec == "eager")
                    hasEager = true;
                else
                    throw ParseError("Unknown decorator '@" + dec + "' for bring statement; only @eager is supported", ln);
            }
            auto bringStmt = parseBringStmt();
            if (auto *bs = dynamic_cast<BringStmt *>(bringStmt.get()))
                bs->isEager = hasEager;
            return bringStmt;
        }

        // Expect fn or async fn
        bool isAsync = false;
        if (check(TokenType::ASYNC))
        {
            isAsync = true;
            advance(); // consume async
            skipNewlines();
        }

        if (!check(TokenType::FN))
            throw ParseError("Expected 'fn', 'class', 'abstract', 'mixin', 'loop', or 'bring' after decorator(s)", current().line);

        auto fnStmt = parseFnDef(isAsync);
        auto fnDef = std::unique_ptr<FnDef>(static_cast<FnDef *>(fnStmt.release()));

        return std::make_unique<DecoratedFnDef>(std::move(decorators), std::move(fnDef), ln);
    }

    // ============================================================
    // Give statement (return)
    // ============================================================

    StmtPtr Parser::parseGiveStmt()
    {
        int ln = current().line;
        advance(); // consume GIVE

        ExprPtr value = nullptr;
        if (!check(TokenType::NEWLINE) && !check(TokenType::DOT) &&
            !check(TokenType::SEMICOLON) && !isAtEnd())
        {
            value = parseExpression();
        }

        consumeStatementEnd();
        return std::make_unique<GiveStmt>(std::move(value), ln);
    }

    // ============================================================
    // Bring statement (import) — supports both legacy and new module syntax
    // ============================================================
    //
    // Legacy: bring name1, name2 from "file" [as a, b]
    //         bring * from "file"
    //
    // New:    bring module_path
    //         bring X of module->path as alias
    //         bring X, Y of module->path as a, b
    //         bring * of module->path [as alias]
    //         from "dir" bring X of module->path
    //         bring X of A and Y of B as a, b
    //
    // Detection: if FROM follows names → old syntax
    //            if OF follows names → new syntax
    //            if no FROM or OF → new syntax (whole module bring)

    StmtPtr Parser::parseBringStmt()
    {
        int ln = current().line;

        // ── Handle "from" starting a bring stmt ──
        std::string fromDir;
        if (check(TokenType::FROM))
        {
            advance(); // consume FROM

            if (check(TokenType::STRING))
            {
                fromDir = current().value;
                advance(); // consume string

                if (!check(TokenType::BRING))
                    throw ParseError("Expected 'bring' after 'from \"dir\"'", current().line);
            }
            else
            {
                throw ParseError("Expected directory string after 'from'", current().line);
            }
        }

        if (check(TokenType::BRING))
            advance(); // consume BRING

        // ── Parse first bring part ──
        std::vector<BringPart> parts;

        auto parseBringPart = [this]() -> BringPart
        {
            BringPart part;

            if (check(TokenType::STAR))
            {
                part.bringAll = true;
                advance(); // consume *
            }
            else
            {
                // Collect identifiers
                part.items.push_back(consume(TokenType::IDENTIFIER, "Expected name after 'bring'").value);
                while (check(TokenType::COMMA))
                {
                    advance(); // consume comma
                    skipNewlines();
                    part.items.push_back(consume(TokenType::IDENTIFIER, "Expected name after ','").value);
                }
            }

            // Check for "of MODULE_PATH" (module-based resolution)
            if (check(TokenType::OF))
            {
                advance(); // consume OF
                part.hasModulePath = true;
                part.modulePath.push_back(consume(TokenType::IDENTIFIER, "Expected module name after 'of'").value);
                while (check(TokenType::ARROW))
                {
                    advance(); // consume ->
                    part.modulePath.push_back(consume(TokenType::IDENTIFIER, "Expected module name after '->'").value);
                }
            }
            // Check for "from \"file\"" (file-based resolution)
            else if (check(TokenType::FROM))
            {
                advance(); // consume FROM
                part.filePath = consume(TokenType::STRING, "Expected file path string after 'from'").value;
            }

            return part;
        };

        parts.push_back(parseBringPart());

        // ── "and" chaining ──
        while (check(TokenType::AND))
        {
            advance(); // consume AND
            skipNewlines();

            if (check(TokenType::FROM))
            {
                throw ParseError("'and from' chaining not yet supported — use separate bring statements", current().line);
            }

            // Optional "bring" keyword after "and"
            if (check(TokenType::BRING))
                advance();

            parts.push_back(parseBringPart());
        }

        // ── Parse aliases: "as alias1, alias2" ──
        std::vector<std::string> aliases;
        if (check(TokenType::AS))
        {
            advance(); // consume AS
            aliases.push_back(consume(TokenType::IDENTIFIER, "Expected alias name after 'as'").value);
            while (check(TokenType::COMMA))
            {
                advance();
                aliases.push_back(consume(TokenType::IDENTIFIER, "Expected alias name after ','").value);
            }
        }

        // ── Alias count validation ──
        // Aliases are assigned left-to-right. Fewer aliases than items is OK
        // (unaliased items keep their original names). More aliases than items is an error.
        if (!aliases.empty())
        {
            size_t totalItems = 0;
            for (const auto &part : parts)
            {
                if (part.bringAll)
                    totalItems += 1;
                else
                    totalItems += part.items.size();
            }
            if (aliases.size() > totalItems)
            {
                throw ParseError("Too many aliases: " + std::to_string(aliases.size()) +
                                     " aliases for " + std::to_string(totalItems) + " imported items",
                                 ln);
            }
        }

        consumeStatementEnd();
        return std::make_unique<BringStmt>(std::move(parts), std::move(aliases), std::move(fromDir), ln);
    }

    // ============================================================
    // Module definition: module name : body ;
    // ============================================================

    StmtPtr Parser::parseModuleDef()
    {
        int ln = current().line;
        advance(); // consume MODULE

        std::string name = consume(TokenType::IDENTIFIER, "Expected module name after 'module'").value;
        consume(TokenType::COLON, "Expected ':' after module name");
        skipNewlines();

        // Parse module body — collects statements until ';'
        // First, parse any 'requires' declarations at the top of the body
        std::vector<std::string> requiresList;
        std::vector<std::pair<std::vector<std::string>, std::vector<std::string>>> requiresItemsList;

        while (check(TokenType::REQUIRES))
        {
            advance(); // consume REQUIRES
            // Parse: requires IDENTIFIER
            //    or: requires ITEMS of MODULE_PATH
            std::vector<std::string> items;
            items.push_back(consume(TokenType::IDENTIFIER, "Expected module/item name after 'requires'").value);
            while (check(TokenType::COMMA))
            {
                advance();
                items.push_back(consume(TokenType::IDENTIFIER, "Expected name after ','").value);
            }

            if (check(TokenType::OF))
            {
                advance(); // consume OF
                std::vector<std::string> path;
                path.push_back(consume(TokenType::IDENTIFIER, "Expected module name after 'of'").value);
                while (check(TokenType::ARROW))
                {
                    advance();
                    path.push_back(consume(TokenType::IDENTIFIER, "Expected module name after '->'").value);
                }
                requiresItemsList.emplace_back(std::move(items), std::move(path));
            }
            else
            {
                // Simple: requires json → whole module dependency
                for (auto &item : items)
                    requiresList.push_back(std::move(item));
            }
            consumeStatementEnd();
        }

        auto body = parseBlock();
        consume(TokenType::SEMICOLON, "Expected ';' to close module block");

        auto mod = std::make_unique<ModuleDef>(std::move(name), std::move(body), ln);
        mod->requires_ = std::move(requiresList);
        mod->requiresItems = std::move(requiresItemsList);
        return mod;
    }

    // ============================================================
    // Export declaration: export fn/class/struct/var/module
    // ============================================================

    StmtPtr Parser::parseExportDecl()
    {
        int ln = current().line;
        advance(); // consume EXPORT
        skipNewlines();

        // Parse the declaration that follows export
        StmtPtr decl;
        TokenType nextType = current().type;

        if (nextType == TokenType::FN)
            decl = parseFnDef();
        else if (nextType == TokenType::CLASS)
            decl = parseClassDef();
        else if (nextType == TokenType::ABSTRACT)
        {
            advance();
            decl = parseClassDef(true);
        }
        else if (nextType == TokenType::MIXIN)
        {
            advance();
            decl = parseClassDef(false, true);
        }
        else if (nextType == TokenType::STRUCT)
            decl = parseStructDef();
        else if (nextType == TokenType::MODULE)
            decl = parseModuleDef();
        else if (nextType == TokenType::ENUM)
            decl = parseEnumDef();
        else if (nextType == TokenType::IDENTIFIER)
        {
            // export x = value (variable export)
            std::string varName = current().value;
            int varLine = current().line;
            advance();
            if (check(TokenType::EQUAL))
            {
                advance();
                auto value = parseExpression();
                consumeStatementEnd();
                decl = std::make_unique<Assignment>(std::move(varName), std::move(value), varLine);
            }
            else
            {
                throw ParseError("Expected '=' after variable name in export declaration", current().line);
            }
        }
        else if (nextType == TokenType::IMMUTABLE)
        {
            // export immutable x = value
            advance(); // consume immutable
            std::string varName = consume(TokenType::IDENTIFIER, "Expected variable name after 'immutable'").value;
            consume(TokenType::EQUAL, "Expected '=' in immutable binding");
            auto value = parseExpression();
            consumeStatementEnd();
            decl = std::make_unique<ImmutableBinding>(std::move(varName), std::move(value), ln);
        }
        else
        {
            throw ParseError("Expected declaration after 'export' (fn, class, struct, module, or variable)", current().line);
        }

        return std::make_unique<ExportDecl>(std::move(decl), ln);
    }

    // ============================================================
    // Expression parsing — precedence climbing
    // ============================================================

    ExprPtr Parser::parseExpression()
    {
        auto expr = parseShellOr();

        // Ternary: value if condition else alternative
        if (check(TokenType::IF))
        {
            int ln = current().line;
            advance(); // consume IF
            auto condition = parseShellOr();
            consume(TokenType::ELSE, "Expected 'else' in ternary expression");
            auto alternative = parseShellOr();
            return std::make_unique<TernaryExpr>(std::move(expr), std::move(condition), std::move(alternative), ln);
        }

        return expr;
    }

    // ---- Shell OR: expr || expr  (lowest new level) ----

    ExprPtr Parser::parseShellOr()
    {
        auto left = parseShellAnd();
        while (check(TokenType::PIPE_PIPE))
        {
            int ln = current().line;
            advance();
            auto right = parseShellAnd();
            left = std::make_unique<BinaryExpr>(std::move(left), "||", std::move(right), ln);
        }
        return left;
    }

    // ---- Shell AND: expr && expr ----

    ExprPtr Parser::parseShellAnd()
    {
        auto left = parsePipe();
        while (check(TokenType::AMP_AMP))
        {
            int ln = current().line;
            advance();
            auto right = parsePipe();
            left = std::make_unique<BinaryExpr>(std::move(left), "&&", std::move(right), ln);
        }
        return left;
    }

    // ---- Pipe: expr |> expr  ----

    ExprPtr Parser::parsePipe()
    {
        auto left = parseLogicalOr();
        while (check(TokenType::PIPE))
        {
            int ln = current().line;
            advance();
            auto right = parseLogicalOr();
            left = std::make_unique<BinaryExpr>(std::move(left), "|>", std::move(right), ln);
        }
        return left;
    }

    ExprPtr Parser::parseLogicalOr()
    {
        auto left = parseLogicalAnd();
        while (check(TokenType::OR))
        {
            int ln = current().line;
            advance();
            auto right = parseLogicalAnd();
            left = std::make_unique<BinaryExpr>(std::move(left), "or", std::move(right), ln);
        }
        return left;
    }

    ExprPtr Parser::parseLogicalAnd()
    {
        auto left = parseBitwiseOr();
        while (check(TokenType::AND))
        {
            int ln = current().line;
            advance();
            auto right = parseBitwiseOr();
            left = std::make_unique<BinaryExpr>(std::move(left), "and", std::move(right), ln);
        }
        return left;
    }

    ExprPtr Parser::parseBitwiseOr()
    {
        auto left = parseBitwiseXor();
        while (check(TokenType::BAR))
        {
            int ln = current().line;
            advance();
            auto right = parseBitwiseXor();
            left = std::make_unique<BinaryExpr>(std::move(left), "|", std::move(right), ln);
        }
        return left;
    }

    ExprPtr Parser::parseBitwiseXor()
    {
        auto left = parseBitwiseAnd();
        while (check(TokenType::CARET))
        {
            int ln = current().line;
            advance();
            auto right = parseBitwiseAnd();
            left = std::make_unique<BinaryExpr>(std::move(left), "^", std::move(right), ln);
        }
        return left;
    }

    ExprPtr Parser::parseBitwiseAnd()
    {
        auto left = parseEquality();
        while (check(TokenType::AMP))
        {
            int ln = current().line;
            advance();
            auto right = parseEquality();
            left = std::make_unique<BinaryExpr>(std::move(left), "&", std::move(right), ln);
        }
        return left;
    }

    ExprPtr Parser::parseEquality()
    {
        auto left = parseComparison();

        // Helper: check for "not in" (two-token lookahead)
        auto isNotIn = [this]()
        {
            return (check(TokenType::NOT) || check(TokenType::BANG)) &&
                   peekToken(1).type == TokenType::IN;
        };

        // Helper: check for "is not" (two-token lookahead)
        auto isIsNot = [this]()
        {
            return check(TokenType::IS) &&
                   (peekToken(1).type == TokenType::NOT || peekToken(1).type == TokenType::BANG);
        };

        while (check(TokenType::EQUAL_EQUAL) || check(TokenType::BANG_EQUAL) ||
               check(TokenType::IS) || check(TokenType::EQ) || check(TokenType::NE) ||
               check(TokenType::IN) || isNotIn())
        {
            int ln = current().line;
            TokenType opType = current().type;
            advance();

            std::string op;
            if (opType == TokenType::IS)
            {
                // Check for "is not" compound operator
                if (check(TokenType::NOT) || check(TokenType::BANG))
                {
                    advance(); // consume NOT/BANG
                    op = "is not";
                }
                else
                {
                    op = "is"; // instance-of check: obj is ClassName
                }
            }
            else if (opType == TokenType::IN)
                op = "in"; // containment check: x in collection
            else if ((opType == TokenType::NOT || opType == TokenType::BANG) && check(TokenType::IN))
            {
                advance(); // consume the IN token
                op = "not in";
            }
            else if (opType == TokenType::EQUAL_EQUAL || opType == TokenType::EQ)
                op = "==";
            else
                op = "!=";

            auto right = parseComparison();
            left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), ln);
        }
        return left;
    }

    ExprPtr Parser::parseComparison()
    {
        auto left = parseShift();

        auto isCompOp = [this]()
        {
            return check(TokenType::GREATER) || check(TokenType::LESS) ||
                   check(TokenType::GREATER_EQUAL) || check(TokenType::LESS_EQUAL) ||
                   check(TokenType::GT) || check(TokenType::LT) ||
                   check(TokenType::GE) || check(TokenType::LE);
        };

        auto readOp = [this]() -> std::string
        {
            TokenType opType = current().type;
            advance();
            if (opType == TokenType::GREATER || opType == TokenType::GT)
                return ">";
            if (opType == TokenType::LESS || opType == TokenType::LT)
                return "<";
            if (opType == TokenType::GREATER_EQUAL || opType == TokenType::GE)
                return ">=";
            return "<=";
        };

        if (!isCompOp())
            return left;

        // First comparison
        int ln = current().line;
        std::string op = readOp();
        auto right = parseShift();

        // Check if this is a single comparison or a chain
        if (!isCompOp())
        {
            // Single comparison — emit plain BinaryExpr
            return std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), ln);
        }

        // Chained comparisons: a < b < c >= d → ChainedComparisonExpr
        std::vector<ExprPtr> operands;
        std::vector<std::string> ops;

        operands.push_back(std::move(left));
        ops.push_back(op);
        operands.push_back(std::move(right));

        while (isCompOp())
        {
            ops.push_back(readOp());
            operands.push_back(parseShift());
        }

        return std::make_unique<ChainedComparisonExpr>(std::move(operands), std::move(ops), ln);
    }

    ExprPtr Parser::parseShift()
    {
        auto left = parseAddition();
        while (check(TokenType::LSHIFT) || check(TokenType::RSHIFT))
        {
            int ln = current().line;
            std::string op = (current().type == TokenType::LSHIFT) ? "<<" : ">>";
            advance();
            auto right = parseAddition();
            left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), ln);
        }
        return left;
    }

    ExprPtr Parser::parseAddition()
    {
        auto left = parseMultiplication();
        while (check(TokenType::PLUS) || check(TokenType::MINUS))
        {
            int ln = current().line;
            std::string op = current().value;
            advance();
            auto right = parseMultiplication();
            left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), ln);
        }
        return left;
    }

    ExprPtr Parser::parseMultiplication()
    {
        auto left = parseExponentiation();
        while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT))
        {
            int ln = current().line;
            std::string op = current().value;
            advance();
            auto right = parseExponentiation();
            left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), ln);
        }
        return left;
    }

    // ---- Exponentiation: right-associative ** ----

    ExprPtr Parser::parseExponentiation()
    {
        auto left = parseUnary();
        if (check(TokenType::STAR_STAR))
        {
            int ln = current().line;
            advance();
            auto right = parseExponentiation(); // right-associative: recurse into self
            left = std::make_unique<BinaryExpr>(std::move(left), "**", std::move(right), ln);
        }
        return left;
    }

    ExprPtr Parser::parseUnary()
    {
        // 'not' or '!' — logical negation
        if (check(TokenType::NOT) || check(TokenType::BANG))
        {
            int ln = current().line;
            advance();
            auto operand = parseUnary();
            return std::make_unique<UnaryExpr>("not", std::move(operand), ln);
        }

        // unary minus
        if (check(TokenType::MINUS))
        {
            int ln = current().line;
            advance();
            auto operand = parseUnary();
            return std::make_unique<UnaryExpr>("-", std::move(operand), ln);
        }

        // prefix ++ and --
        if (check(TokenType::PLUS_PLUS))
        {
            int ln = current().line;
            advance();
            auto operand = parseUnary();
            return std::make_unique<UnaryExpr>("++", std::move(operand), ln);
        }
        if (check(TokenType::MINUS_MINUS))
        {
            int ln = current().line;
            advance();
            auto operand = parseUnary();
            return std::make_unique<UnaryExpr>("--", std::move(operand), ln);
        }

        // yield expression: yield or yield expr
        if (check(TokenType::YIELD))
        {
            int ln = current().line;
            advance(); // consume yield
            ExprPtr value = nullptr;
            // yield with no value if followed by statement boundary
            if (!check(TokenType::NEWLINE) && !check(TokenType::DOT) &&
                !check(TokenType::SEMICOLON) && !isAtEnd() &&
                canStartPrimary(current().type))
            {
                value = parseExpression();
            }
            return std::make_unique<YieldExpr>(std::move(value), ln);
        }

        // await expression: await expr
        if (check(TokenType::AWAIT))
        {
            int ln = current().line;
            advance(); // consume await
            auto operand = parseUnary();
            return std::make_unique<AwaitExpr>(std::move(operand), ln);
        }

        // ~ — frozen set (~{...}), bitwise NOT (integer), or smart-cast prefix (~type(args))
        if (check(TokenType::TILDE))
        {
            // Lookahead: ~{ → frozen set; ~identifier( → smart-cast; else → bitwise NOT
            if (peekToken(1).type == TokenType::LBRACE)
            {
                // Frozen set: handled in parsePrimary
                return parsePrimary();
            }
            if (peekToken(1).type == TokenType::IDENTIFIER && peekToken(2).type == TokenType::LPAREN)
            {
                // Smart-cast: handled in parsePrimary
                return parsePrimary();
            }
            int ln = current().line;
            advance(); // consume ~
            auto operand = parseUnary();
            return std::make_unique<UnaryExpr>("~", std::move(operand), ln);
        }

        return parsePrimary();
    }

    // ============================================================
    // Primary expressions
    // ============================================================

    ExprPtr Parser::parsePrimary()
    {
        int ln = current().line;

        // Number literal → IntLiteral or FloatLiteral
        if (check(TokenType::NUMBER))
        {
            const std::string &val = current().value;
            advance();
            if (val.find('.') != std::string::npos ||
                val.find('E') != std::string::npos)
            {
                // Has decimal point or scientific notation → float
                return parsePostfix(std::make_unique<FloatLiteral>(std::stod(val), ln));
            }
            else if (val.size() > 2 && val[0] == '0' && (val[1] == 'b' || val[1] == 'B'))
            {
                // Binary literal: 0b1010
                return parsePostfix(std::make_unique<IntLiteral>(
                    std::stoll(val.substr(2), nullptr, 2), ln));
            }
            else if (val.size() > 2 && val[0] == '0' && (val[1] == 'o' || val[1] == 'O'))
            {
                // Octal literal: 0o77
                return parsePostfix(std::make_unique<IntLiteral>(
                    std::stoll(val.substr(2), nullptr, 8), ln));
            }
            else
            {
                // Decimal or hex (0x prefix auto-detected by stoll base 0)
                return parsePostfix(std::make_unique<IntLiteral>(std::stoll(val, nullptr, 0), ln));
            }
        }

        // Imaginary literal → ImaginaryLiteral (for complex numbers)
        if (check(TokenType::IMAGINARY))
        {
            double val = std::stod(current().value);
            advance();
            return parsePostfix(std::make_unique<ImaginaryLiteral>(val, ln));
        }

        // String literal
        if (check(TokenType::STRING))
        {
            std::string val = current().value;
            advance();
            return parsePostfix(std::make_unique<StringLiteral>(std::move(val), ln));
        }

        // Raw string literal (r"..." — no interpolation)
        if (check(TokenType::RAW_STRING))
        {
            std::string val = current().value;
            advance();
            return parsePostfix(std::make_unique<StringLiteral>(std::move(val), ln, true));
        }

        // Byte string literal (b"..." — raw binary data)
        if (check(TokenType::BYTE_STRING))
        {
            std::string val = current().value;
            advance();
            return parsePostfix(std::make_unique<BytesLiteral>(std::move(val), ln));
        }

        // Boolean literals
        if (check(TokenType::TRUE_KW))
        {
            advance();
            return parsePostfix(std::make_unique<BoolLiteral>(true, ln));
        }
        if (check(TokenType::FALSE_KW))
        {
            advance();
            return parsePostfix(std::make_unique<BoolLiteral>(false, ln));
        }

        // None literal
        if (check(TokenType::NONE_KW))
        {
            advance();
            return parsePostfix(std::make_unique<NoneLiteral>(ln));
        }

        // List literal
        if (check(TokenType::LBRACKET))
        {
            return parsePostfix(parseListLiteral());
        }

        // Map or Set literal: { ... }
        if (check(TokenType::LBRACE))
        {
            return parsePostfix(parseBraceExpr());
        }

        // Shell command: $cmd args...
        if (check(TokenType::SHELL_CMD))
        {
            return parsePostfix(parseShellCmdExpr());
        }

        // Frozen set literal: ~{expr, expr, ...}
        if (check(TokenType::TILDE) && peekToken(1).type == TokenType::LBRACE)
        {
            return parsePostfix(parseFrozenSetLiteral());
        }

        // Spread operator: ...expr
        if (check(TokenType::ELLIPSIS))
        {
            advance(); // consume ...
            auto operand = parsePrimary();
            return std::make_unique<SpreadExpr>(std::move(operand), ln);
        }

        // Smart-cast prefix: ~List(args), ~Tuple(args), etc.
        if (check(TokenType::TILDE))
        {
            advance(); // consume ~
            if (check(TokenType::IDENTIFIER))
            {
                std::string name = "~" + current().value;
                advance(); // consume identifier
                if (check(TokenType::LPAREN))
                {
                    advance(); // consume (
                    auto args = parseArgList();
                    consume(TokenType::RPAREN, "Expected ')' after smart-cast arguments");
                    return parsePostfix(std::make_unique<CallExpr>(name, std::move(args), ln));
                }
                // ~name without parens → treat as identifier (future bitwise NOT)
                throw ParseError("Expected '(' after '~" + name.substr(1) + "'", ln);
            }
            // ~{ is frozen set — should have been caught above
            throw ParseError("Expected identifier or '{' after '~'", ln);
        }

        // Grouped expression or lambda: ( expr ) or (a, b) => expr
        if (check(TokenType::LPAREN))
        {
            // Lookahead: is this a lambda (a, b) => ... ?
            // Save position, try to match: LPAREN IDENT [, IDENT]* RPAREN FAT_ARROW
            size_t savedPos = pos_;
            bool isLambda = false;
            {
                size_t look = pos_ + 1; // skip LPAREN
                // Check for empty params: () =>
                if (look < tokens_.size() && tokens_[look].type == TokenType::RPAREN)
                {
                    look++;
                    if (look < tokens_.size() && tokens_[look].type == TokenType::FAT_ARROW)
                        isLambda = true;
                }
                else
                {
                    // Check for IDENT [, IDENT]* RPAREN FAT_ARROW
                    bool valid = true;
                    while (look < tokens_.size())
                    {
                        if (tokens_[look].type != TokenType::IDENTIFIER)
                        {
                            valid = false;
                            break;
                        }
                        look++;
                        if (look < tokens_.size() && tokens_[look].type == TokenType::COMMA)
                        {
                            look++;
                            continue;
                        }
                        break;
                    }
                    if (valid && look < tokens_.size() && tokens_[look].type == TokenType::RPAREN)
                    {
                        look++;
                        if (look < tokens_.size() && tokens_[look].type == TokenType::FAT_ARROW)
                            isLambda = true;
                    }
                }
            }

            if (isLambda)
            {
                advance(); // consume LPAREN
                std::vector<std::string> params;
                if (!check(TokenType::RPAREN))
                {
                    params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name").value);
                    while (check(TokenType::COMMA))
                    {
                        advance();
                        params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name").value);
                    }
                }
                consume(TokenType::RPAREN, "Expected ')' after lambda parameters");
                consume(TokenType::FAT_ARROW, "Expected '=>' after lambda parameters");

                if (check(TokenType::COLON))
                {
                    // Multi-line lambda: (a, b) => : block ;
                    advance(); // consume :
                    auto body = parseBlock();
                    consume(TokenType::SEMICOLON, "Expected ';' to close lambda body");
                    return std::make_unique<LambdaExpr>(std::move(params), std::move(body), nullptr, ln);
                }
                else
                {
                    auto expr = parseExpression();
                    return std::make_unique<LambdaExpr>(std::move(params), std::vector<StmtPtr>{}, std::move(expr), ln);
                }
            }

            // Regular grouped expression or tuple
            advance(); // consume (
            skipNewlines();

            // Empty parens: () → empty tuple (lambda case already handled above)
            if (check(TokenType::RPAREN))
            {
                advance();
                return parsePostfix(std::make_unique<TupleLiteral>(std::vector<ExprPtr>{}, ln));
            }

            auto expr = parseExpression();
            skipNewlines();

            // If comma follows, this is a tuple: (expr, expr, ...) or (expr,)
            if (check(TokenType::COMMA))
            {
                std::vector<ExprPtr> elements;
                elements.push_back(std::move(expr));
                while (check(TokenType::COMMA))
                {
                    advance();
                    skipNewlines();
                    if (check(TokenType::RPAREN))
                        break; // trailing comma
                    elements.push_back(parseExpression());
                    skipNewlines();
                }
                consume(TokenType::RPAREN, "Expected ')' to close tuple");
                return parsePostfix(std::make_unique<TupleLiteral>(std::move(elements), ln));
            }

            // Single expression in parens → grouped expression
            consume(TokenType::RPAREN, "Expected ')' after grouped expression");
            return parsePostfix(std::move(expr));
        }

        // Identifier, lambda (x => ...), or function call
        if (check(TokenType::IDENTIFIER))
        {
            std::string name = current().value;
            advance();

            // Single-param lambda: x => expr  or  x => : block ;
            if (check(TokenType::FAT_ARROW))
            {
                advance(); // consume =>
                std::vector<std::string> params = {name};
                if (check(TokenType::COLON))
                {
                    // Multi-line lambda
                    advance(); // consume :
                    auto body = parseBlock();
                    consume(TokenType::SEMICOLON, "Expected ';' to close lambda body");
                    return std::make_unique<LambdaExpr>(std::move(params), std::move(body), nullptr, ln);
                }
                else
                {
                    auto expr = parseExpression();
                    return std::make_unique<LambdaExpr>(std::move(params), std::vector<StmtPtr>{}, std::move(expr), ln);
                }
            }

            // Function call with parens: name(args)
            if (check(TokenType::LPAREN))
            {
                advance(); // consume (
                auto args = parseArgList();
                consume(TokenType::RPAREN, "Expected ')' after function arguments");
                return parsePostfix(std::make_unique<CallExpr>(name, std::move(args), ln));
            }

            return parsePostfix(std::make_unique<Identifier>(std::move(name), ln));
        }

        // ---- Expression-mode if/elif/else ----
        // x = if cond: value elif cond: value else: value ;
        if (check(TokenType::IF))
        {
            advance(); // consume IF
            std::vector<IfExprBranch> branches;

            // First if branch
            auto cond = parseExpression();
            consume(TokenType::COLON, "Expected ':' after if condition in expression");
            auto val = parseExpression();
            branches.push_back({std::move(cond), std::move(val), ln});

            // elif branches
            while (check(TokenType::ELIF))
            {
                int elifLn = current().line;
                advance(); // consume ELIF
                auto elifCond = parseExpression();
                consume(TokenType::COLON, "Expected ':' after elif condition in expression");
                auto elifVal = parseExpression();
                branches.push_back({std::move(elifCond), std::move(elifVal), elifLn});
            }

            // else branch (required for expression-mode)
            if (!check(TokenType::ELSE))
                throw ParseError("Expression-mode if requires an 'else' branch", ln);
            int elseLn = current().line;
            advance(); // consume ELSE
            consume(TokenType::COLON, "Expected ':' after 'else' in expression");
            auto elseVal = parseExpression();
            branches.push_back({nullptr, std::move(elseVal), elseLn}); // nullptr condition = else

            return parsePostfix(std::make_unique<IfExpr>(std::move(branches), ln));
        }

        // ---- Expression-mode for loop ----
        // x = for i in list: BLOCK give DEFAULT ;
        if (check(TokenType::FOR))
        {
            advance(); // consume FOR

            // Parse targets
            std::vector<std::string> varNames;
            bool hasRest = false;
            std::string restName;

            if (check(TokenType::ELLIPSIS))
            {
                advance();
                restName = consume(TokenType::IDENTIFIER, "Expected variable name after '...'").value;
                hasRest = true;
            }
            else
            {
                varNames.push_back(consume(TokenType::IDENTIFIER, "Expected loop variable name after 'for'").value);
            }

            while (!hasRest && check(TokenType::COMMA))
            {
                auto nextTok = peekToken(1);
                if (nextTok.type == TokenType::ELLIPSIS)
                {
                    advance();
                    advance();
                    restName = consume(TokenType::IDENTIFIER, "Expected variable name after '...'").value;
                    hasRest = true;
                    break;
                }
                else if (nextTok.type == TokenType::IDENTIFIER)
                {
                    auto afterIdent = peekToken(2);
                    if (afterIdent.type == TokenType::COMMA || afterIdent.type == TokenType::IN)
                    {
                        advance();
                        varNames.push_back(consume(TokenType::IDENTIFIER, "Expected variable name").value);
                    }
                    else
                        break;
                }
                else
                    break;
            }

            consume(TokenType::IN, "Expected 'in' after for expression variable(s)");

            std::vector<ExprPtr> iterables;
            iterables.push_back(parseExpression());
            while (check(TokenType::COMMA))
            {
                advance();
                iterables.push_back(parseExpression());
            }

            consume(TokenType::COLON, "Expected ':' after for expression iterable");
            auto body = parseBlock(false, true);

            // Parse optional give DEFAULT before ;
            ExprPtr defaultValue = nullptr;
            if (check(TokenType::GIVE))
            {
                advance(); // consume give
                defaultValue = parseExpression();
                consumeStatementEnd(); // consume trailing .
            }

            consume(TokenType::SEMICOLON, "Expected ';' to close for expression");

            return parsePostfix(std::make_unique<ForExpr>(
                std::move(varNames), std::move(iterables), std::move(body),
                std::move(defaultValue), hasRest, std::move(restName), ln));
        }

        // ---- Expression-mode while loop ----
        // x = while cond: BLOCK give DEFAULT ;
        if (check(TokenType::WHILE))
        {
            advance(); // consume WHILE
            auto condition = parseExpression();
            consume(TokenType::COLON, "Expected ':' after while condition in expression");
            auto body = parseBlock(false, true);

            ExprPtr defaultValue = nullptr;
            if (check(TokenType::GIVE))
            {
                advance();
                defaultValue = parseExpression();
                consumeStatementEnd();
            }

            consume(TokenType::SEMICOLON, "Expected ';' to close while expression");

            return parsePostfix(std::make_unique<WhileExpr>(
                std::move(condition), std::move(body), std::move(defaultValue), ln));
        }

        // ---- Expression-mode loop ----
        // x = loop: BLOCK give DEFAULT ;
        if (check(TokenType::LOOP))
        {
            advance(); // consume LOOP
            consume(TokenType::COLON, "Expected ':' after 'loop' in expression");
            auto body = parseBlock(false, true);

            ExprPtr defaultValue = nullptr;
            if (check(TokenType::GIVE))
            {
                advance();
                defaultValue = parseExpression();
                consumeStatementEnd();
            }

            consume(TokenType::SEMICOLON, "Expected ';' to close loop expression");

            return parsePostfix(std::make_unique<LoopExpr>(
                std::move(body), std::move(defaultValue), ln));
        }

        // ---- Expression-mode incase (switch expression) ----
        // x = incase val : is 1 : "one" belong int : "int" bind v if v > 0 : v else : 0 ;
        if (check(TokenType::INCASE))
        {
            advance(); // consume INCASE
            ExprPtr subject = parseExpression();
            consume(TokenType::COLON, "Expected ':' after incase expression in expression mode");
            skipNewlines();

            std::vector<InCaseExprClause> clauses;

            while (check(TokenType::IS) || check(TokenType::BELONG) || check(TokenType::BIND))
            {
                InCaseExprClause clause;
                clause.line = current().line;
                TokenType kw = current().type;
                advance(); // consume IS / BELONG / BIND

                if (kw == TokenType::BELONG)
                {
                    clause.kind = ClauseKind::BELONG_TYPE;
                    std::string typeName;
                    if (check(TokenType::IDENTIFIER))
                        typeName = current().value;
                    else if (isBuiltinTypeName(current().value))
                        typeName = current().value;
                    else
                        throw ParseError("Expected type name after 'belong'", clause.line);
                    clause.typeName = typeName;
                    advance();
                    goto expr_parse_guard;
                }
                else if (kw == TokenType::BIND)
                {
                    clause.kind = ClauseKind::BIND_CAPTURE;
                    if (!check(TokenType::IDENTIFIER))
                        throw ParseError("Expected variable name after 'bind'", clause.line);
                    clause.bindName = current().value;
                    advance();
                    goto expr_parse_guard;
                }
                // else IS: fall through to parse values

            expr_parse_values:
                clause.values.push_back(parseLogicalAnd());
                while (check(TokenType::OR))
                {
                    advance();
                    clause.values.push_back(parseLogicalAnd());
                }

            expr_parse_guard:
                if (check(TokenType::IF))
                {
                    advance(); // consume IF
                    clause.guard = parseComparison();
                }

                consume(TokenType::COLON, "Expected ':' after incase clause in expression mode");
                clause.result = parseComparison();
                skipNewlines();

                clauses.push_back(std::move(clause));
            }

            // else branch (required for expression-mode)
            if (!check(TokenType::ELSE))
                throw ParseError("Expression-mode incase requires an 'else' branch", ln);
            advance(); // consume ELSE
            consume(TokenType::COLON, "Expected ':' after 'else' in incase expression");
            ExprPtr elseValue = parseComparison();
            skipNewlines();

            consume(TokenType::SEMICOLON, "Expected ';' to close incase expression");

            return parsePostfix(std::make_unique<InCaseExpr>(
                std::move(subject), std::move(clauses), std::move(elseValue), ln));
        }
        throw ParseError("Unexpected token: " + tokenTypeToString(current().type) +
                             " ('" + current().value + "')",
                         current().line);
    }

    // ============================================================
    // Postfix: [index], ->member, ++, --
    // ============================================================

    ExprPtr Parser::parsePostfix(ExprPtr expr)
    {
        while (true)
        {
            int ln = current().line;

            // Index access or slice: expr[index] or expr[start:end:step]
            if (check(TokenType::LBRACKET))
            {
                advance();

                // Check for slice syntax: [:...], [start:...], etc.
                ExprPtr start = nullptr;
                ExprPtr end = nullptr;
                ExprPtr step = nullptr;

                // If first token is ':', start is omitted
                if (check(TokenType::COLON))
                {
                    // Slice with omitted start: [:end] or [:end:step] or [:]
                    advance(); // consume ':'
                    if (!check(TokenType::RBRACKET) && !check(TokenType::COLON))
                        end = parseExpression();
                    if (check(TokenType::COLON))
                    {
                        advance();
                        if (!check(TokenType::RBRACKET))
                            step = parseExpression();
                    }
                    consume(TokenType::RBRACKET, "Expected ']' after slice");
                    expr = std::make_unique<SliceExpr>(std::move(expr), std::move(start),
                                                       std::move(end), std::move(step), ln);
                    continue;
                }

                // Parse first expression (could be index or start of slice)
                auto first = parseExpression();

                if (check(TokenType::COLON))
                {
                    // It's a slice: [start:end] or [start:end:step]
                    advance(); // consume ':'
                    start = std::move(first);
                    if (!check(TokenType::RBRACKET) && !check(TokenType::COLON))
                        end = parseExpression();
                    if (check(TokenType::COLON))
                    {
                        advance();
                        if (!check(TokenType::RBRACKET))
                            step = parseExpression();
                    }
                    consume(TokenType::RBRACKET, "Expected ']' after slice");
                    expr = std::make_unique<SliceExpr>(std::move(expr), std::move(start),
                                                       std::move(end), std::move(step), ln);
                }
                else
                {
                    // Plain index access: expr[index]
                    consume(TokenType::RBRACKET, "Expected ']' after index");
                    expr = std::make_unique<IndexAccess>(std::move(expr), std::move(first), ln);
                }
                continue;
            }

            // Member access: expr->member  (optionally followed by method call parens)
            if (check(TokenType::ARROW))
            {
                advance();
                std::string member = consume(TokenType::IDENTIFIER, "Expected member name after '->'").value;

                if (check(TokenType::LPAREN))
                {
                    advance();
                    auto args = parseArgList();
                    consume(TokenType::RPAREN, "Expected ')' after method arguments");
                    // Method call: insert object as first arg
                    args.insert(args.begin(), std::move(expr));
                    auto call = std::make_unique<CallExpr>(member, std::move(args), ln);
                    call->isMethodCall = true;
                    expr = std::move(call);
                    continue;
                }

                expr = std::make_unique<MemberAccess>(std::move(expr), member, ln);
                continue;
            }

            // Postfix ++
            if (check(TokenType::PLUS_PLUS))
            {
                advance();
                expr = std::make_unique<PostfixExpr>("++", std::move(expr), ln);
                continue;
            }

            // Postfix --
            if (check(TokenType::MINUS_MINUS))
            {
                advance();
                expr = std::make_unique<PostfixExpr>("--", std::move(expr), ln);
                continue;
            }

            // "of" keyword: name of obj → obj->name (alias for member access)
            // Only valid when expr is an Identifier
            if (check(TokenType::OF))
            {
                auto *ident = dynamic_cast<const Identifier *>(expr.get());
                if (ident)
                {
                    std::string memberName = ident->name;
                    advance();               // consume 'of'
                    auto obj = parseUnary(); // parse the object expression
                    expr = std::make_unique<MemberAccess>(std::move(obj), memberName, ln);
                    continue;
                }
            }

            break;
        }
        return expr;
    }

    // ============================================================
    // Helpers
    // ============================================================

    ExprPtr Parser::parseListLiteral()
    {
        int ln = current().line;
        advance(); // consume [

        std::vector<ExprPtr> elements;
        skipNewlines();
        if (!check(TokenType::RBRACKET))
        {
            // Support spread: ...expr
            if (check(TokenType::ELLIPSIS))
            {
                advance(); // consume ...
                auto operand = parseExpression();
                elements.push_back(std::make_unique<SpreadExpr>(std::move(operand), ln));
            }
            else
            {
                elements.push_back(parseExpression());
            }

            // ---- Comprehension: [expr for x in iterable ...] ----
            skipNewlines();
            if (check(TokenType::FOR))
            {
                // First element is the value expression
                auto valueExpr = std::move(elements[0]);
                auto clauses = parseCompClauses();
                skipNewlines();
                consume(TokenType::RBRACKET, "Expected ']' to close list comprehension");
                return std::make_unique<ListComprehension>(std::move(valueExpr), std::move(clauses), ln);
            }

            while (check(TokenType::COMMA))
            {
                advance();
                skipNewlines();
                if (check(TokenType::RBRACKET))
                    break; // trailing comma
                if (check(TokenType::ELLIPSIS))
                {
                    int sln = current().line;
                    advance(); // consume ...
                    auto operand = parseExpression();
                    elements.push_back(std::make_unique<SpreadExpr>(std::move(operand), sln));
                }
                else
                {
                    elements.push_back(parseExpression());
                }
            }
        }
        skipNewlines();
        consume(TokenType::RBRACKET, "Expected ']' to close list");
        return std::make_unique<ListLiteral>(std::move(elements), ln);
    }

    ExprPtr Parser::parseBraceExpr()
    {
        int ln = current().line;
        advance(); // consume {
        skipNewlines();

        // Empty braces → empty map
        if (check(TokenType::RBRACE))
        {
            advance();
            return std::make_unique<MapLiteral>(
                std::vector<std::pair<ExprPtr, ExprPtr>>{}, ln);
        }

        // Lookahead: if (IDENTIFIER|STRING|RAW_STRING) followed by COLON → map or map comprehension
        // Also: if LBRACKET → computed key map {[expr]: value}
        bool isMapPattern = false;
        if (check(TokenType::LBRACKET))
        {
            // {[expr]: ...} is definitely a map with computed keys
            isMapPattern = true;
        }
        else if (check(TokenType::IDENTIFIER) || check(TokenType::STRING) || check(TokenType::RAW_STRING))
        {
            size_t saved = pos_;
            advance(); // consume potential key
            skipNewlines();
            if (check(TokenType::COLON))
                isMapPattern = true;
            pos_ = saved; // restore
        }

        if (isMapPattern)
            return parseMapEntries(ln);
        else
            return parseSetEntries(ln);
    }

    ExprPtr Parser::parseMapEntries(int ln)
    {
        // Helper: parse a map key.
        // - Bare identifier or string → wrap as StringLiteral (backward compat)
        // - [expr] → computed key: parse expression inside brackets
        auto parseKey = [this, ln]() -> ExprPtr
        {
            if (check(TokenType::LBRACKET))
            {
                advance(); // consume [
                skipNewlines();
                ExprPtr keyExpr = parseExpression();
                skipNewlines();
                consume(TokenType::RBRACKET, "Expected ']' after computed map key");
                return keyExpr;
            }
            if (check(TokenType::IDENTIFIER))
            {
                std::string name = advance().value;
                return std::make_unique<StringLiteral>(name, ln);
            }
            if (check(TokenType::STRING) || check(TokenType::RAW_STRING))
            {
                std::string val = advance().value;
                return std::make_unique<StringLiteral>(val, ln);
            }
            throw ParseError(
                "Expected map key (identifier, string, or [expr]), got " +
                    tokenTypeToString(current().type) + " ('" + current().value + "')",
                current().line);
        };

        std::vector<std::pair<ExprPtr, ExprPtr>> entries;
        skipNewlines();

        ExprPtr key = parseKey();
        consume(TokenType::COLON, "Expected ':' after map key");
        skipNewlines();
        ExprPtr value = parseExpression();

        // ---- Map comprehension: {key: value for k in iterable ...} ----
        skipNewlines();
        if (check(TokenType::FOR))
        {
            // For comprehension, use the key expression directly.
            // If it was a bare identifier wrapped as StringLiteral, convert to Identifier
            // so variable resolution works.
            ExprPtr keyExpr;
            if (auto *strLit = dynamic_cast<StringLiteral *>(key.get()))
            {
                // Bare identifier key in comprehension context → variable reference
                keyExpr = std::make_unique<Identifier>(strLit->value, ln);
            }
            else
            {
                // Computed key [expr] → use as-is
                keyExpr = std::move(key);
            }
            auto clauses = parseCompClauses();
            skipNewlines();
            consume(TokenType::RBRACE, "Expected '}' to close map comprehension");
            return std::make_unique<MapComprehension>(
                std::move(keyExpr), std::move(value), std::move(clauses), ln);
        }

        entries.emplace_back(std::move(key), std::move(value));

        while (check(TokenType::COMMA))
        {
            advance();
            skipNewlines();
            if (check(TokenType::RBRACE))
                break; // trailing comma
            key = parseKey();
            consume(TokenType::COLON, "Expected ':' after map key");
            skipNewlines();
            value = parseExpression();
            entries.emplace_back(std::move(key), std::move(value));
        }

        skipNewlines();
        consume(TokenType::RBRACE, "Expected '}' to close map");
        return std::make_unique<MapLiteral>(std::move(entries), ln);
    }

    ExprPtr Parser::parseSetEntries(int ln)
    {
        std::vector<ExprPtr> elements;
        skipNewlines();

        elements.push_back(parseExpression());

        // ---- Set comprehension: {expr for x in iterable ...} ----
        skipNewlines();
        if (check(TokenType::FOR))
        {
            auto valueExpr = std::move(elements[0]);
            auto clauses = parseCompClauses();
            skipNewlines();
            consume(TokenType::RBRACE, "Expected '}' to close set comprehension");
            return std::make_unique<SetComprehension>(std::move(valueExpr), std::move(clauses), ln);
        }

        while (check(TokenType::COMMA))
        {
            advance();
            skipNewlines();
            if (check(TokenType::RBRACE))
                break; // trailing comma
            elements.push_back(parseExpression());
        }

        skipNewlines();
        consume(TokenType::RBRACE, "Expected '}' to close set");
        return std::make_unique<SetLiteral>(std::move(elements), ln);
    }

    ExprPtr Parser::parseFrozenSetLiteral()
    {
        int ln = current().line;
        advance(); // consume ~
        advance(); // consume {
        skipNewlines();

        // Empty frozen set: ~{}
        if (check(TokenType::RBRACE))
        {
            advance();
            return std::make_unique<FrozenSetLiteral>(std::vector<ExprPtr>{}, ln);
        }

        std::vector<ExprPtr> elements;
        elements.push_back(parseExpression());

        while (check(TokenType::COMMA))
        {
            advance();
            skipNewlines();
            if (check(TokenType::RBRACE))
                break; // trailing comma
            elements.push_back(parseExpression());
        }

        skipNewlines();
        consume(TokenType::RBRACE, "Expected '}' to close frozen set");
        return std::make_unique<FrozenSetLiteral>(std::move(elements), ln);
    }

    ExprPtr Parser::parseShellCmdExpr()
    {
        int ln = current().line;
        std::string cmd = current().value; // command string collected by lexer
        advance();                         // consume SHELL_CMD
        return std::make_unique<ShellCmdExpr>(std::move(cmd), ln);
    }

    std::vector<ExprPtr> Parser::parseArgList()
    {
        std::vector<ExprPtr> args;
        skipNewlines();

        auto parseOneArg = [&]() -> ExprPtr
        {
            if (check(TokenType::ELLIPSIS))
            {
                int sln = current().line;
                advance(); // consume ...
                auto operand = parseExpression();
                return std::make_unique<SpreadExpr>(std::move(operand), sln);
            }
            // Check for named argument: identifier COLON expr
            if (check(TokenType::IDENTIFIER) && peekToken(1).type == TokenType::COLON)
            {
                int sln = current().line;
                std::string name = current().value;
                advance(); // consume identifier
                advance(); // consume colon
                auto val = parseExpression();
                return std::make_unique<NamedArgExpr>(std::move(name), std::move(val), sln);
            }
            return parseExpression();
        };

        if (!check(TokenType::RPAREN))
        {
            args.push_back(parseOneArg());
            while (check(TokenType::COMMA))
            {
                advance();
                skipNewlines();
                args.push_back(parseOneArg());
            }
        }
        skipNewlines();
        return args;
    }

    // ---- let EXPR be NAME [, EXPR be NAME]* : BLOCK ; ----
    StmtPtr Parser::parseLetStmt()
    {
        int ln = current().line;
        advance(); // consume 'let'
        skipNewlines();

        std::vector<LetBinding> bindings;

        // Parse first binding (at least one required)
        for (;;)
        {
            // Parse the resource expression
            ExprPtr expr = parseExpression();

            // Expect 'be'
            if (!check(TokenType::BE))
                throw ParseError("Expected 'be' after expression in let ... be", current().line);
            advance(); // consume 'be'
            skipNewlines();

            // Expect identifier or '_'
            if (!check(TokenType::IDENTIFIER))
                throw ParseError("Expected identifier after 'be' in let ... be", current().line);
            std::string name = current().value;
            int bindLn = current().line;
            advance(); // consume identifier
            skipNewlines();

            LetBinding binding;
            binding.expr = std::move(expr);
            binding.name = std::move(name);
            binding.line = bindLn;
            bindings.push_back(std::move(binding));

            // Check for comma (more bindings) or colon (start of block)
            if (check(TokenType::COMMA))
            {
                advance(); // consume ','
                skipNewlines();
                continue;
            }
            break;
        }

        // Expect ':'
        consume(TokenType::COLON, "Expected ':' after let ... be bindings");
        skipNewlines();

        // Parse block body
        auto body = parseBlock();

        // Expect ';'
        consume(TokenType::SEMICOLON, "Expected ';' to close let ... be block");

        return std::make_unique<LetStmt>(std::move(bindings), std::move(body), ln);
    }

    // ---- Comprehension clause parser ----
    // Parses: for x in iterable [if cond] [for y in iter2 [if cond2]] ...
    // Must be called when current token is FOR.
    // Uses parseShellOr() instead of parseExpression() for iterables and
    // conditions to avoid consuming 'if' as ternary syntax.
    std::vector<CompClause> Parser::parseCompClauses()
    {
        std::vector<CompClause> clauses;

        while (check(TokenType::FOR))
        {
            advance(); // consume 'for'
            skipNewlines();

            // Parse variable names (support destructuring: for k, v in ...)
            std::vector<std::string> vars;
            if (!check(TokenType::IDENTIFIER))
                throw ParseError("Expected variable name after 'for' in comprehension", current().line);
            vars.push_back(advance().value);
            while (check(TokenType::COMMA))
            {
                advance(); // consume ','
                skipNewlines();
                if (!check(TokenType::IDENTIFIER))
                    throw ParseError("Expected variable name after ',' in comprehension", current().line);
                vars.push_back(advance().value);
            }

            skipNewlines();
            consume(TokenType::IN, "Expected 'in' after variable name in comprehension");
            skipNewlines();

            // Use parseShellOr to avoid consuming 'if' as ternary
            auto iterable = parseShellOr();

            CompClause forClause;
            forClause.isFor = true;
            forClause.vars = std::move(vars);
            forClause.iterable = std::move(iterable);
            clauses.push_back(std::move(forClause));

            // Optional if-clause(s)
            skipNewlines();
            while (check(TokenType::IF))
            {
                advance(); // consume 'if'
                skipNewlines();
                // Use parseShellOr to avoid consuming nested 'if' as ternary
                auto cond = parseShellOr();

                CompClause ifClause;
                ifClause.isFor = false;
                ifClause.condition = std::move(cond);
                clauses.push_back(std::move(ifClause));
                skipNewlines();
            }
        }

        if (clauses.empty())
            throw ParseError("Expected at least one 'for' clause in comprehension", current().line);

        return clauses;
    }

} // namespace xell
