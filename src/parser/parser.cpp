#include "parser.hpp"
#include "../lib/errors/error.hpp"
#include <sstream>
#include <iostream>

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
        return type == TokenType::NUMBER || type == TokenType::STRING || type == TokenType::RAW_STRING || type == TokenType::TRUE_KW || type == TokenType::FALSE_KW || type == TokenType::NONE_KW || type == TokenType::IDENTIFIER || type == TokenType::LPAREN || type == TokenType::LBRACKET || type == TokenType::LBRACE || type == TokenType::NOT || type == TokenType::BANG || type == TokenType::MINUS || type == TokenType::PLUS_PLUS || type == TokenType::MINUS_MINUS || type == TokenType::ELLIPSIS;
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

    std::vector<StmtPtr> Parser::parseBlock(bool stopAtElifElse)
    {
        std::vector<StmtPtr> stmts;
        skipNewlines();
        while (!check(TokenType::SEMICOLON) && !isAtEnd())
        {
            // If we're inside an if/elif block, also stop when we see elif/else
            if (stopAtElifElse &&
                (check(TokenType::ELIF) || check(TokenType::ELSE)))
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
        if (type == TokenType::FN)
            return parseFnDef();
        if (type == TokenType::GIVE)
            return parseGiveStmt();
        if (type == TokenType::BREAK)
        {
            int ln = current().line;
            advance(); // consume break
            consumeStatementEnd();
            return std::make_unique<BreakStmt>(ln);
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
        if (type == TokenType::TRY)
            return parseTryCatchStmt();
        if (type == TokenType::INCASE)
            return parseInCaseStmt();

        // --- Augmented assignment: IDENTIFIER += EXPR etc ---
        if (type == TokenType::IDENTIFIER &&
            (peekToken(1).type == TokenType::PLUS_EQUAL ||
             peekToken(1).type == TokenType::MINUS_EQUAL ||
             peekToken(1).type == TokenType::STAR_EQUAL ||
             peekToken(1).type == TokenType::SLASH_EQUAL ||
             peekToken(1).type == TokenType::PERCENT_EQUAL))
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

        // --- Destructuring assignment: a, b = [1, 2] ---
        // Detect: IDENTIFIER COMMA IDENTIFIER ... EQUAL
        if (type == TokenType::IDENTIFIER)
        {
            // Look ahead to see if this is a destructuring pattern
            size_t look = pos_;
            std::vector<std::string> names;
            bool isDestructuring = false;
            while (look < tokens_.size())
            {
                if (tokens_[look].type == TokenType::IDENTIFIER)
                {
                    names.push_back(tokens_[look].value);
                    look++;
                    if (look < tokens_.size() && tokens_[look].type == TokenType::COMMA)
                    {
                        look++;
                        continue;
                    }
                    if (look < tokens_.size() && tokens_[look].type == TokenType::EQUAL)
                    {
                        isDestructuring = names.size() >= 2;
                    }
                    break;
                }
                break;
            }
            if (isDestructuring)
            {
                int ln = current().line;
                // Consume all: ident, ident, ... = expr
                names.clear();
                names.push_back(current().value);
                advance(); // first ident
                while (check(TokenType::COMMA))
                {
                    advance(); // consume comma
                    names.push_back(consume(TokenType::IDENTIFIER, "Expected identifier in destructuring").value);
                }
                consume(TokenType::EQUAL, "Expected '=' in destructuring assignment");
                ExprPtr value = parseExpression();
                consumeStatementEnd();
                return std::make_unique<DestructuringAssignment>(std::move(names), std::move(value), ln);
            }
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

        consumeStatementEnd();
        return std::make_unique<ExprStmt>(std::move(expr), ln);
    }

    // ============================================================
    // Assignment
    // ============================================================

    StmtPtr Parser::parseAssignment(const std::string &name, int line)
    {
        ExprPtr value = parseExpression();
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

        std::string varName = consume(TokenType::IDENTIFIER, "Expected loop variable name after 'for'").value;
        consume(TokenType::IN, "Expected 'in' after for loop variable");
        ExprPtr iterable = parseExpression();
        consume(TokenType::COLON, "Expected ':' after for expression");
        auto body = parseBlock();
        consume(TokenType::SEMICOLON, "Expected ';' to close for block");

        return std::make_unique<ForStmt>(varName, std::move(iterable), std::move(body), ln);
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
    // Function definition
    // ============================================================

    StmtPtr Parser::parseFnDef()
    {
        int ln = current().line;
        advance(); // consume FN

        std::string name = consume(TokenType::IDENTIFIER, "Expected function name after 'fn'").value;
        consume(TokenType::LPAREN, "Expected '(' after function name");

        std::vector<std::string> params;
        std::vector<ExprPtr> defaults;
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
                params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name").value);
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
                    params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name after ','").value);
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

        consume(TokenType::COLON, "Expected ':' after function signature");
        auto body = parseBlock();
        consume(TokenType::SEMICOLON, "Expected ';' to close function body");

        auto fnDef = std::make_unique<FnDef>(name, std::move(params), std::move(body), ln);
        fnDef->defaults = std::move(defaults);
        fnDef->isVariadic = isVariadic;
        fnDef->variadicName = std::move(variadicName);
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

        // catch varname :
        std::string catchVar;
        std::vector<StmtPtr> catchBody;
        if (check(TokenType::CATCH))
        {
            advance(); // consume CATCH
            catchVar = consume(TokenType::IDENTIFIER, "Expected variable name after 'catch'").value;
            consume(TokenType::COLON, "Expected ':' after catch variable");
            catchBody = parseBlock();
            consume(TokenType::SEMICOLON, "Expected ';' to close catch block");
            skipNewlines();
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
            std::move(tryBody), std::move(catchVar),
            std::move(catchBody), std::move(finallyBody), ln);
    }

    // ============================================================
    // InCase (switch/match)
    // ============================================================

    StmtPtr Parser::parseInCaseStmt()
    {
        int ln = current().line;
        advance(); // consume INCASE

        ExprPtr subject = parseExpression();
        consume(TokenType::COLON, "Expected ':' after incase expression");
        skipNewlines();

        std::vector<InCaseClause> clauses;
        std::vector<StmtPtr> elseBody;

        // Parse clauses: is EXPR [or EXPR ...] : block ;
        while (check(TokenType::IS))
        {
            InCaseClause clause;
            clause.line = current().line;
            advance(); // consume IS

            // First value — use parseLogicalAnd so 'or' is not consumed as logical OR
            clause.values.push_back(parseLogicalAnd());
            // Additional values separated by 'or'
            while (check(TokenType::OR))
            {
                advance(); // consume OR
                clause.values.push_back(parseLogicalAnd());
            }

            consume(TokenType::COLON, "Expected ':' after incase value(s)");
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
    // Bring statement (import)
    // ============================================================

    StmtPtr Parser::parseBringStmt()
    {
        int ln = current().line;
        advance(); // consume BRING

        bool bringAll = false;
        std::vector<std::string> names;

        if (check(TokenType::STAR))
        {
            bringAll = true;
            advance(); // consume *
        }
        else
        {
            names.push_back(consume(TokenType::IDENTIFIER, "Expected name after 'bring'").value);
            while (check(TokenType::COMMA))
            {
                advance(); // consume comma
                names.push_back(consume(TokenType::IDENTIFIER, "Expected name after ','").value);
            }
        }

        consume(TokenType::FROM, "Expected 'from' in bring statement");
        std::string path = consume(TokenType::STRING, "Expected file path string after 'from'").value;

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

        consumeStatementEnd();
        return std::make_unique<BringStmt>(bringAll, std::move(names), path, std::move(aliases), ln);
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

    // ---- Pipe: expr | expr  (highest new level) ----

    ExprPtr Parser::parsePipe()
    {
        auto left = parseLogicalOr();
        while (check(TokenType::PIPE))
        {
            int ln = current().line;
            advance();
            auto right = parseLogicalOr();
            left = std::make_unique<BinaryExpr>(std::move(left), "|", std::move(right), ln);
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
        auto left = parseEquality();
        while (check(TokenType::AND))
        {
            int ln = current().line;
            advance();
            auto right = parseEquality();
            left = std::make_unique<BinaryExpr>(std::move(left), "and", std::move(right), ln);
        }
        return left;
    }

    ExprPtr Parser::parseEquality()
    {
        auto left = parseComparison();
        while (check(TokenType::EQUAL_EQUAL) || check(TokenType::BANG_EQUAL) ||
               check(TokenType::IS) || check(TokenType::EQ) || check(TokenType::NE))
        {
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

    ExprPtr Parser::parseComparison()
    {
        auto left = parseAddition();

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
        auto right = parseAddition();
        auto result = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), ln);

        // Chained comparisons: a < b < c → (a < b) and (b < c)
        // For each additional comparison, we reparse the chain.
        // Since we can't clone AST nodes, we don't truly chain.
        // Instead, each comparison is independent: a < b, then b < c.
        // This means the middle expression is evaluated twice at runtime.
        // For correctness we just chain as: (a < b) and (b < c).
        // We track the last right-side expression's line for re-parsing.
        // But we already consumed it. So for now, single comparisons only.
        // Multiple comparison operators without chaining just left-associate.
        while (isCompOp())
        {
            ln = current().line;
            op = readOp();
            right = parseAddition();
            result = std::make_unique<BinaryExpr>(std::move(result), op, std::move(right), ln);
        }

        return result;
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
        auto left = parseUnary();
        while (check(TokenType::STAR) || check(TokenType::SLASH) || check(TokenType::PERCENT))
        {
            int ln = current().line;
            std::string op = current().value;
            advance();
            auto right = parseUnary();
            left = std::make_unique<BinaryExpr>(std::move(left), op, std::move(right), ln);
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

        return parsePrimary();
    }

    // ============================================================
    // Primary expressions
    // ============================================================

    ExprPtr Parser::parsePrimary()
    {
        int ln = current().line;

        // Number literal
        if (check(TokenType::NUMBER))
        {
            double val = std::stod(current().value);
            advance();
            return parsePostfix(std::make_unique<NumberLiteral>(val, ln));
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

        // Map literal
        if (check(TokenType::LBRACE))
        {
            return parsePostfix(parseMapLiteral());
        }

        // Spread operator: ...expr
        if (check(TokenType::ELLIPSIS))
        {
            advance(); // consume ...
            auto operand = parsePrimary();
            return std::make_unique<SpreadExpr>(std::move(operand), ln);
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

            // Regular grouped expression
            advance();
            auto expr = parseExpression();
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

            // Index access: expr[index]
            if (check(TokenType::LBRACKET))
            {
                advance();
                auto index = parseExpression();
                consume(TokenType::RBRACKET, "Expected ']' after index");
                expr = std::make_unique<IndexAccess>(std::move(expr), std::move(index), ln);
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
                    expr = std::make_unique<CallExpr>(member, std::move(args), ln);
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

    ExprPtr Parser::parseMapLiteral()
    {
        int ln = current().line;
        advance(); // consume {

        // Helper: parse a map key (IDENTIFIER or STRING)
        auto parseKey = [this]() -> std::string
        {
            if (check(TokenType::IDENTIFIER))
                return advance().value;
            if (check(TokenType::STRING) || check(TokenType::RAW_STRING))
                return advance().value;
            throw ParseError(
                "Expected map key (identifier or string), got " +
                    tokenTypeToString(current().type) + " ('" + current().value + "')",
                current().line);
        };

        std::vector<std::pair<std::string, ExprPtr>> entries;
        skipNewlines();
        if (!check(TokenType::RBRACE))
        {
            std::string key = parseKey();
            consume(TokenType::COLON, "Expected ':' after map key");
            skipNewlines();
            ExprPtr value = parseExpression();
            entries.emplace_back(key, std::move(value));

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
                entries.emplace_back(key, std::move(value));
            }
        }
        skipNewlines();
        consume(TokenType::RBRACE, "Expected '}' to close map");
        return std::make_unique<MapLiteral>(std::move(entries), ln);
    }

    std::vector<ExprPtr> Parser::parseArgList()
    {
        std::vector<ExprPtr> args;
        skipNewlines();
        if (!check(TokenType::RPAREN))
        {
            if (check(TokenType::ELLIPSIS))
            {
                int sln = current().line;
                advance(); // consume ...
                auto operand = parseExpression();
                args.push_back(std::make_unique<SpreadExpr>(std::move(operand), sln));
            }
            else
            {
                args.push_back(parseExpression());
            }
            while (check(TokenType::COMMA))
            {
                advance();
                skipNewlines();
                if (check(TokenType::ELLIPSIS))
                {
                    int sln = current().line;
                    advance(); // consume ...
                    auto operand = parseExpression();
                    args.push_back(std::make_unique<SpreadExpr>(std::move(operand), sln));
                }
                else
                {
                    args.push_back(parseExpression());
                }
            }
        }
        skipNewlines();
        return args;
    }

} // namespace xell
