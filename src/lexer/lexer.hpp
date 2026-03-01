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
        Token readString();
        Token readRawString();
        Token readIdentifierOrKeyword();

        // Map-based keyword lookup (extensible)
        static TokenType lookupKeyword(const std::string &word);
        static bool isAlpha(char c);
        static bool isDigit(char c);
        static bool isAlphaNumeric(char c);
    };

} // namespace xell
