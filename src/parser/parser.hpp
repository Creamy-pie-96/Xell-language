#pragma once

#include "ast.hpp"
#include "../lexer/token.hpp"
#include "../lib/errors/error.hpp"
#include <vector>
#include <string>

namespace xell
{

    /// A parse error collected during error-recovery parsing.
    struct CollectedParseError
    {
        int line;
        std::string message;
    };

    class Parser
    {
    public:
        explicit Parser(const std::vector<Token> &tokens);
        Program parse();

        /// Error-recovering parse: returns a partial AST + all parse errors.
        /// Used by the linter so it can feed whatever parsed into static analysis
        /// even when some statements are malformed.
        Program parseLint(std::vector<CollectedParseError> &errors);

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

        // Error recovery: skip tokens until a plausible statement boundary
        void synchronize();

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
        std::vector<StmtPtr> parseBlock(bool stopAtElifElse = false);

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
