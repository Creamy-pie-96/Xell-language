#include "parser.hpp"
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
        throw ParseError("[XELL ERROR] Line " + std::to_string(current().line) + " — " + msg);
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
        return type == TokenType::NUMBER || type == TokenType::STRING || type == TokenType::TRUE_KW || type == TokenType::FALSE_KW || type == TokenType::NONE_KW || type == TokenType::IDENTIFIER || type == TokenType::LPAREN || type == TokenType::LBRACKET || type == TokenType::LBRACE || type == TokenType::NOT || type == TokenType::BANG || type == TokenType::MINUS || type == TokenType::PLUS_PLUS || type == TokenType::MINUS_MINUS;
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
    // Block: parse statements until ';'
    // ============================================================

    std::vector<StmtPtr> Parser::parseBlock()
    {
        std::vector<StmtPtr> stmts;
        skipNewlines();
        while (!check(TokenType::SEMICOLON) && !isAtEnd())
        {
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
        if (type == TokenType::BRING)
            return parseBringStmt();

        // --- Assignment: IDENTIFIER = EXPR ---
        if (type == TokenType::IDENTIFIER && peekToken(1).type == TokenType::EQUAL)
        {
            std::string name = current().value;
            int ln = current().line;
            advance(); // consume identifier
            advance(); // consume =
            return parseAssignment(name, ln);
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
        auto body = parseBlock();
        consume(TokenType::SEMICOLON, "Expected ';' to close if block");

        // Parse elif clauses
        std::vector<ElifClause> elifs;
        skipNewlines();
        while (check(TokenType::ELIF))
        {
            int elifLine = current().line;
            advance(); // consume ELIF
            ExprPtr elifCond = parseExpression();
            consume(TokenType::COLON, "Expected ':' after elif condition");
            auto elifBody = parseBlock();
            consume(TokenType::SEMICOLON, "Expected ';' to close elif block");

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
            elseBody = parseBlock();
            consume(TokenType::SEMICOLON, "Expected ';' to close else block");
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
        if (!check(TokenType::RPAREN))
        {
            params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name").value);
            while (check(TokenType::COMMA))
            {
                advance(); // consume comma
                params.push_back(consume(TokenType::IDENTIFIER, "Expected parameter name after ','").value);
            }
        }
        consume(TokenType::RPAREN, "Expected ')' after parameters");

        consume(TokenType::COLON, "Expected ':' after function signature");
        auto body = parseBlock();
        consume(TokenType::SEMICOLON, "Expected ';' to close function body");

        return std::make_unique<FnDef>(name, std::move(params), std::move(body), ln);
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
        return parseLogicalOr();
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
        while (check(TokenType::GREATER) || check(TokenType::LESS) ||
               check(TokenType::GREATER_EQUAL) || check(TokenType::LESS_EQUAL) ||
               check(TokenType::GT) || check(TokenType::LT) ||
               check(TokenType::GE) || check(TokenType::LE))
        {
            int ln = current().line;
            TokenType opType = current().type;
            advance();

            std::string op;
            if (opType == TokenType::GREATER || opType == TokenType::GT)
                op = ">";
            else if (opType == TokenType::LESS || opType == TokenType::LT)
                op = "<";
            else if (opType == TokenType::GREATER_EQUAL || opType == TokenType::GE)
                op = ">=";
            else
                op = "<=";

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
        auto left = parseUnary();
        while (check(TokenType::STAR) || check(TokenType::SLASH))
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

        // Grouped expression: ( expr )
        if (check(TokenType::LPAREN))
        {
            advance();
            auto expr = parseExpression();
            consume(TokenType::RPAREN, "Expected ')' after grouped expression");
            return parsePostfix(std::move(expr));
        }

        // Identifier or function call
        if (check(TokenType::IDENTIFIER))
        {
            std::string name = current().value;
            advance();

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

        throw ParseError("[XELL ERROR] Line " + std::to_string(current().line) +
                         " — Unexpected token: " + tokenTypeToString(current().type) +
                         " ('" + current().value + "')");
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
            elements.push_back(parseExpression());
            while (check(TokenType::COMMA))
            {
                advance();
                skipNewlines();
                if (check(TokenType::RBRACKET))
                    break; // trailing comma
                elements.push_back(parseExpression());
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

        std::vector<std::pair<std::string, ExprPtr>> entries;
        skipNewlines();
        if (!check(TokenType::RBRACE))
        {
            std::string key = consume(TokenType::IDENTIFIER, "Expected map key (identifier)").value;
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
                key = consume(TokenType::IDENTIFIER, "Expected map key (identifier)").value;
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
            args.push_back(parseExpression());
            while (check(TokenType::COMMA))
            {
                advance();
                skipNewlines();
                args.push_back(parseExpression());
            }
        }
        skipNewlines();
        return args;
    }

} // namespace xell
