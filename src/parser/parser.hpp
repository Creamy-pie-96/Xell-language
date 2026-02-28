#pragma once

#include "ast.hpp"
#include "../lexer/token.hpp"
#include <vector>
#include <string>
#include <stdexcept>

namespace xell
{

    class ParseError : public std::runtime_error
    {
    public:
        using std::runtime_error::runtime_error;
    };

    class Parser
    {
    public:
        explicit Parser(const std::vector<Token> &tokens);
        Program parse();

    private:
        std::vector<Token> tokens_;
        size_t pos_;

        // Token navigation
        const Token &current() const;
        const Token &peekToken(int offset = 1) const;
        bool check(TokenType type) const;
        bool isAtEnd() const;
        Token advance();
        Token consume(TokenType type, const std::string &errorMsg = "");
        void skipNewlines();
        void consumeStatementEnd();

        // Check if a token type can start a primary expression
        bool canStartPrimary(TokenType type) const;

        // Statements
        StmtPtr parseStatement();
        StmtPtr parseAssignment(const std::string &name, int line);
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

        // Literal helpers
        ExprPtr parseListLiteral();
        ExprPtr parseMapLiteral();
        std::vector<ExprPtr> parseArgList();
    };

} // namespace xell
