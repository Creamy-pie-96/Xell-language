#pragma once

#include <string>
#include <unordered_map>

namespace xell
{

    enum class TokenType
    {
        // Literals
        NUMBER,
        STRING,

        // Boolean & None keywords (also used as literals)
        TRUE_KW,
        FALSE_KW,
        NONE_KW,

        // Control flow keywords
        FN,
        GIVE,
        IF,
        ELIF,
        ELSE,
        FOR,
        WHILE,
        IN,

        // Import keywords
        BRING,
        FROM,
        AS,

        // Logical keywords
        AND,
        OR,
        NOT,

        // Comparison keywords
        IS,
        EQ,
        NE,
        GT,
        LT,
        GE,
        LE,

        // Utility keyword
        OF,

        // Arithmetic operators
        PLUS,
        MINUS,
        STAR,
        SLASH,

        // Increment / decrement
        PLUS_PLUS,   // ++
        MINUS_MINUS, // --

        // Assignment & comparison operators
        EQUAL,       // =
        EQUAL_EQUAL, // ==
        BANG,        // !   (standalone NOT)
        BANG_EQUAL,  // !=

        // Relational operators
        GREATER,       // >
        LESS,          // <
        GREATER_EQUAL, // >=
        LESS_EQUAL,    // <=

        // Access operators
        ARROW, // ->
        DOT,   // .

        // Delimiters
        LPAREN,    // (
        RPAREN,    // )
        LBRACKET,  // [
        RBRACKET,  // ]
        LBRACE,    // {
        RBRACE,    // }
        COMMA,     // ,
        COLON,     // :
        SEMICOLON, // ;

        // Special
        IDENTIFIER,
        NEWLINE,
        EOF_TOKEN
    };


    inline const std::unordered_map<int, std::string> &tokenTypeNames()
    {
        static const std::unordered_map<int, std::string> map = {
            {(int)TokenType::NUMBER, "NUMBER"},
            {(int)TokenType::STRING, "STRING"},
            {(int)TokenType::TRUE_KW, "TRUE"},
            {(int)TokenType::FALSE_KW, "FALSE"},
            {(int)TokenType::NONE_KW, "NONE"},
            {(int)TokenType::FN, "FN"},
            {(int)TokenType::GIVE, "GIVE"},
            {(int)TokenType::IF, "IF"},
            {(int)TokenType::ELIF, "ELIF"},
            {(int)TokenType::ELSE, "ELSE"},
            {(int)TokenType::FOR, "FOR"},
            {(int)TokenType::WHILE, "WHILE"},
            {(int)TokenType::IN, "IN"},
            {(int)TokenType::BRING, "BRING"},
            {(int)TokenType::FROM, "FROM"},
            {(int)TokenType::AS, "AS"},
            {(int)TokenType::AND, "AND"},
            {(int)TokenType::OR, "OR"},
            {(int)TokenType::NOT, "NOT"},
            {(int)TokenType::IS, "IS"},
            {(int)TokenType::EQ, "EQ"},
            {(int)TokenType::NE, "NE"},
            {(int)TokenType::GT, "GT"},
            {(int)TokenType::LT, "LT"},
            {(int)TokenType::GE, "GE"},
            {(int)TokenType::LE, "LE"},
            {(int)TokenType::OF, "OF"},
            {(int)TokenType::PLUS, "PLUS"},
            {(int)TokenType::MINUS, "MINUS"},
            {(int)TokenType::STAR, "STAR"},
            {(int)TokenType::SLASH, "SLASH"},
            {(int)TokenType::PLUS_PLUS, "PLUS_PLUS"},
            {(int)TokenType::MINUS_MINUS, "MINUS_MINUS"},
            {(int)TokenType::EQUAL, "EQUAL"},
            {(int)TokenType::EQUAL_EQUAL, "EQUAL_EQUAL"},
            {(int)TokenType::BANG, "BANG"},
            {(int)TokenType::BANG_EQUAL, "BANG_EQUAL"},
            {(int)TokenType::GREATER, "GREATER"},
            {(int)TokenType::LESS, "LESS"},
            {(int)TokenType::GREATER_EQUAL, "GREATER_EQUAL"},
            {(int)TokenType::LESS_EQUAL, "LESS_EQUAL"},
            {(int)TokenType::ARROW, "ARROW"},
            {(int)TokenType::DOT, "DOT"},
            {(int)TokenType::LPAREN, "LPAREN"},
            {(int)TokenType::RPAREN, "RPAREN"},
            {(int)TokenType::LBRACKET, "LBRACKET"},
            {(int)TokenType::RBRACKET, "RBRACKET"},
            {(int)TokenType::LBRACE, "LBRACE"},
            {(int)TokenType::RBRACE, "RBRACE"},
            {(int)TokenType::COMMA, "COMMA"},
            {(int)TokenType::COLON, "COLON"},
            {(int)TokenType::SEMICOLON, "SEMICOLON"},
            {(int)TokenType::IDENTIFIER, "IDENTIFIER"},
            {(int)TokenType::NEWLINE, "NEWLINE"},
            {(int)TokenType::EOF_TOKEN, "EOF"},
        };
        return map;
    }

    inline std::string tokenTypeToString(TokenType type)
    {
        auto &names = tokenTypeNames();
        auto it = names.find((int)type);
        if (it != names.end())
            return it->second;
        return "UNKNOWN";
    }


    struct Token
    {
        TokenType type;
        std::string value;
        int line;

        Token(TokenType type, std::string value, int line)
            : type(type), value(std::move(value)), line(line) {}
    };

} // namespace xell
