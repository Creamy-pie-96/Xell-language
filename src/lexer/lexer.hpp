#pragma once

#include "token.hpp"
#include <string>
#include <vector>

namespace xell
{

    class Lexer
    {
    public:
        explicit Lexer(const std::string &source);
        std::vector<Token> tokenize();

    private:
        std::string source_;
        size_t pos_;
        int line_;
        int nestingDepth_; // tracks () [] {} nesting — suppress NEWLINE inside

        char current() const;
        char peek(int offset = 1) const;
        void advance();
        bool isAtEnd() const;

        void skipWhitespaceAndComments();
        void skipSingleLineComment();
        void skipMultiLineComment();

        Token readNumber();
        Token readString(char quoteChar = '"');
        Token readRawString(char quoteChar = '"');
        Token readByteString(char quoteChar = '"');
        Token readIdentifierOrKeyword();

        // Map-based keyword lookup (extensible)
        static TokenType lookupKeyword(const std::string &word);
        static bool isAlpha(char c);
        static bool isDigit(char c);
        static bool isHexDigit(char c);
        static bool isAlphaNumeric(char c);
        Token readFString(char quoteChar);
        Token readShellBlock();
        std::string readInterpolatedExpr(int exprStartLine);

        enum class ShellLexState
        {
            NONE,
            AFTER_SHELL,
            AFTER_SHELL_ARROW,
            AFTER_SHELL_CAPTURE,
            IN_SHELL_ARGS,
            AFTER_SHELL_ARGS
        };

        ShellLexState shellLexState_ = ShellLexState::NONE;
        int shellArgsParenDepth_ = 0;
    };

} // namespace xell
