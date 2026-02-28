#include "lexer.hpp"
#include "../lib/errors/error.hpp"
#include <sstream>
#include <unordered_map>

namespace xell
{

    // ---- Map-based keyword table (add new keywords here) ------------------------

    static const std::unordered_map<std::string, TokenType> &keywordMap()
    {
        static const std::unordered_map<std::string, TokenType> map = {
            // Control flow
            {"fn", TokenType::FN},
            {"give", TokenType::GIVE},
            {"if", TokenType::IF},
            {"elif", TokenType::ELIF},
            {"else", TokenType::ELSE},
            {"for", TokenType::FOR},
            {"while", TokenType::WHILE},
            {"in", TokenType::IN},

            // Import
            {"bring", TokenType::BRING},
            {"from", TokenType::FROM},
            {"as", TokenType::AS},

            // Literals
            {"true", TokenType::TRUE_KW},
            {"false", TokenType::FALSE_KW},
            {"none", TokenType::NONE_KW},

            // Logical
            {"and", TokenType::AND},
            {"or", TokenType::OR},
            {"not", TokenType::NOT},

            // Comparison keywords
            {"is", TokenType::IS},
            {"eq", TokenType::EQ},
            {"ne", TokenType::NE},
            {"gt", TokenType::GT},
            {"lt", TokenType::LT},
            {"ge", TokenType::GE},
            {"le", TokenType::LE},

            // Utility
            {"of", TokenType::OF},
        };
        return map;
    }

    // ---- Constructor ------------------------------------------------------------

    Lexer::Lexer(const std::string &source)
        : source_(source), pos_(0), line_(1), nestingDepth_(0) {}

    // ---- Character helpers ------------------------------------------------------

    char Lexer::current() const
    {
        if (isAtEnd())
            return '\0';
        return source_[pos_];
    }

    char Lexer::peek(int offset) const
    {
        size_t idx = pos_ + offset;
        if (idx >= source_.size())
            return '\0';
        return source_[idx];
    }

    void Lexer::advance()
    {
        if (!isAtEnd())
        {
            if (source_[pos_] == '\n')
                line_++;
            pos_++;
        }
    }

    bool Lexer::isAtEnd() const
    {
        return pos_ >= source_.size();
    }

    bool Lexer::isAlpha(char c)
    {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
    }

    bool Lexer::isDigit(char c)
    {
        return c >= '0' && c <= '9';
    }

    bool Lexer::isAlphaNumeric(char c)
    {
        return isAlpha(c) || isDigit(c);
    }

    // ---- Whitespace & comment skipping -----------------------------------------

    void Lexer::skipWhitespaceAndComments()
    {
        while (!isAtEnd())
        {
            char c = current();

            // Skip spaces and tabs (NOT newlines — those are significant)
            if (c == ' ' || c == '\t' || c == '\r')
            {
                advance();
                continue;
            }

            // Single-line comment: #
            if (c == '#')
            {
                skipSingleLineComment();
                continue;
            }

            // Multi-line comment: -->
            if (c == '-' && peek(1) == '-' && peek(2) == '>')
            {
                skipMultiLineComment();
                continue;
            }

            break;
        }
    }

    void Lexer::skipSingleLineComment()
    {
        while (!isAtEnd() && current() != '\n')
        {
            advance();
        }
        // Don't consume the \n — let the main loop handle it as a NEWLINE token
    }

    void Lexer::skipMultiLineComment()
    {
        advance(); // -
        advance(); // -
        advance(); // >

        while (!isAtEnd())
        {
            if (current() == '<' && peek(1) == '-' && peek(2) == '-')
            {
                advance(); // <
                advance(); // -
                advance(); // -
                return;
            }
            advance();
        }
        // Unterminated multi-line comment — hit EOF
    }

    // ---- Token readers ----------------------------------------------------------

    Token Lexer::readNumber()
    {
        int startLine = line_;
        std::string num;

        while (!isAtEnd() && isDigit(current()))
        {
            num += current();
            advance();
        }

        // Decimal point followed by digits → floating point
        if (!isAtEnd() && current() == '.' && isDigit(peek(1)))
        {
            num += '.';
            advance(); // consume '.'
            while (!isAtEnd() && isDigit(current()))
            {
                num += current();
                advance();
            }
        }

        return Token(TokenType::NUMBER, num, startLine);
    }

    Token Lexer::readString()
    {
        int startLine = line_;
        advance(); // consume opening "

        std::string str;
        while (!isAtEnd() && current() != '"')
        {
            if (current() == '\\' && peek(1) == '"')
            {
                str += '"';
                advance();
                advance();
            }
            else if (current() == '\\' && peek(1) == 'n')
            {
                str += '\n';
                advance();
                advance();
            }
            else if (current() == '\\' && peek(1) == 't')
            {
                str += '\t';
                advance();
                advance();
            }
            else if (current() == '\\' && peek(1) == '\\')
            {
                str += '\\';
                advance();
                advance();
            }
            else
            {
                str += current();
                advance();
            }
        }

        if (isAtEnd())
        {
            throw LexerError("Unterminated string literal", startLine);
        }

        advance(); // consume closing "
        return Token(TokenType::STRING, str, startLine);
    }

    Token Lexer::readIdentifierOrKeyword()
    {
        int startLine = line_;
        std::string word;

        while (!isAtEnd() && isAlphaNumeric(current()))
        {
            word += current();
            advance();
        }

        TokenType type = lookupKeyword(word);
        return Token(type, word, startLine);
    }

    TokenType Lexer::lookupKeyword(const std::string &word)
    {
        auto &kw = keywordMap();
        auto it = kw.find(word);
        if (it != kw.end())
            return it->second;
        return TokenType::IDENTIFIER;
    }

    // ---- Main tokenize loop -----------------------------------------------------

    std::vector<Token> Lexer::tokenize()
    {
        std::vector<Token> tokens;

        while (!isAtEnd())
        {
            skipWhitespaceAndComments();
            if (isAtEnd())
                break;

            char c = current();
            int tokenLine = line_;

            // --- Newline ---
            if (c == '\n')
            {
                advance();
                // Only emit NEWLINE outside brackets/parens/braces, and collapse consecutive
                if (nestingDepth_ == 0)
                {
                    if (tokens.empty() || tokens.back().type != TokenType::NEWLINE)
                    {
                        tokens.emplace_back(TokenType::NEWLINE, "\\n", tokenLine);
                    }
                }
                continue;
            }

            // --- Number literal ---
            if (isDigit(c))
            {
                tokens.push_back(readNumber());
                continue;
            }

            // --- String literal ---
            if (c == '"')
            {
                tokens.push_back(readString());
                continue;
            }

            // --- Identifier or keyword ---
            if (isAlpha(c))
            {
                tokens.push_back(readIdentifierOrKeyword());
                continue;
            }

            // --- Operators and delimiters ---
            // (using a dispatch approach — each character handled explicitly)

            // + and ++
            if (c == '+')
            {
                if (peek(1) == '+')
                {
                    tokens.emplace_back(TokenType::PLUS_PLUS, "++", tokenLine);
                    advance();
                    advance();
                }
                else
                {
                    tokens.emplace_back(TokenType::PLUS, "+", tokenLine);
                    advance();
                }
                continue;
            }

            // - (minus), -- (decrement), -> (arrow), --> (comment already handled above)
            if (c == '-')
            {
                if (peek(1) == '-' && peek(2) == '>')
                {
                    // Multi-line comment start (shouldn't reach here if skipWhitespaceAndComments ran,
                    // but guard just in case)
                    skipMultiLineComment();
                }
                else if (peek(1) == '-')
                {
                    tokens.emplace_back(TokenType::MINUS_MINUS, "--", tokenLine);
                    advance();
                    advance();
                }
                else if (peek(1) == '>')
                {
                    tokens.emplace_back(TokenType::ARROW, "->", tokenLine);
                    advance();
                    advance();
                }
                else
                {
                    tokens.emplace_back(TokenType::MINUS, "-", tokenLine);
                    advance();
                }
                continue;
            }

            if (c == '*')
            {
                tokens.emplace_back(TokenType::STAR, "*", tokenLine);
                advance();
                continue;
            }

            if (c == '/')
            {
                tokens.emplace_back(TokenType::SLASH, "/", tokenLine);
                advance();
                continue;
            }

            if (c == '%')
            {
                tokens.emplace_back(TokenType::PERCENT, "%", tokenLine);
                advance();
                continue;
            }

            // = and ==
            if (c == '=')
            {
                if (peek(1) == '=')
                {
                    tokens.emplace_back(TokenType::EQUAL_EQUAL, "==", tokenLine);
                    advance();
                    advance();
                }
                else
                {
                    tokens.emplace_back(TokenType::EQUAL, "=", tokenLine);
                    advance();
                }
                continue;
            }

            // ! and !=
            if (c == '!')
            {
                if (peek(1) == '=')
                {
                    tokens.emplace_back(TokenType::BANG_EQUAL, "!=", tokenLine);
                    advance();
                    advance();
                }
                else
                {
                    tokens.emplace_back(TokenType::BANG, "!", tokenLine);
                    advance();
                }
                continue;
            }

            // > and >=
            if (c == '>')
            {
                if (peek(1) == '=')
                {
                    tokens.emplace_back(TokenType::GREATER_EQUAL, ">=", tokenLine);
                    advance();
                    advance();
                }
                else
                {
                    tokens.emplace_back(TokenType::GREATER, ">", tokenLine);
                    advance();
                }
                continue;
            }

            // < and <=
            if (c == '<')
            {
                if (peek(1) == '=')
                {
                    tokens.emplace_back(TokenType::LESS_EQUAL, "<=", tokenLine);
                    advance();
                    advance();
                }
                else
                {
                    tokens.emplace_back(TokenType::LESS, "<", tokenLine);
                    advance();
                }
                continue;
            }

            if (c == '.')
            {
                tokens.emplace_back(TokenType::DOT, ".", tokenLine);
                advance();
                continue;
            }

            // Delimiters with nesting tracking
            if (c == '(')
            {
                tokens.emplace_back(TokenType::LPAREN, "(", tokenLine);
                nestingDepth_++;
                advance();
                continue;
            }
            if (c == ')')
            {
                tokens.emplace_back(TokenType::RPAREN, ")", tokenLine);
                if (nestingDepth_ > 0)
                    nestingDepth_--;
                advance();
                continue;
            }
            if (c == '[')
            {
                tokens.emplace_back(TokenType::LBRACKET, "[", tokenLine);
                nestingDepth_++;
                advance();
                continue;
            }
            if (c == ']')
            {
                tokens.emplace_back(TokenType::RBRACKET, "]", tokenLine);
                if (nestingDepth_ > 0)
                    nestingDepth_--;
                advance();
                continue;
            }
            if (c == '{')
            {
                tokens.emplace_back(TokenType::LBRACE, "{", tokenLine);
                nestingDepth_++;
                advance();
                continue;
            }
            if (c == '}')
            {
                tokens.emplace_back(TokenType::RBRACE, "}", tokenLine);
                if (nestingDepth_ > 0)
                    nestingDepth_--;
                advance();
                continue;
            }

            if (c == ',')
            {
                tokens.emplace_back(TokenType::COMMA, ",", tokenLine);
                advance();
                continue;
            }
            if (c == ':')
            {
                tokens.emplace_back(TokenType::COLON, ":", tokenLine);
                advance();
                continue;
            }
            if (c == ';')
            {
                tokens.emplace_back(TokenType::SEMICOLON, ";", tokenLine);
                advance();
                continue;
            }

            // Unknown character
            throw LexerError("Unexpected character '" + std::string(1, c) + "'", tokenLine);
        }

        // Remove trailing NEWLINE before EOF
        if (!tokens.empty() && tokens.back().type == TokenType::NEWLINE)
        {
            tokens.pop_back();
        }

        tokens.emplace_back(TokenType::EOF_TOKEN, "", line_);
        return tokens;
    }

} // namespace xell
