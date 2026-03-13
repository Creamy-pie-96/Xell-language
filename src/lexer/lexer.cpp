#include "lexer.hpp"
#include "../lib/errors/error.hpp"
#include <cstdint>
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
            {"break", TokenType::BREAK},
            {"continue", TokenType::CONTINUE},
            {"try", TokenType::TRY},
            {"catch", TokenType::CATCH},
            {"finally", TokenType::FINALLY},
            {"throw", TokenType::THROW},
            {"incase", TokenType::INCASE},
            {"let", TokenType::LET},
            {"be", TokenType::BE},
            {"loop", TokenType::LOOP},
            {"do", TokenType::DO},

            // Import / module
            {"bring", TokenType::BRING},
            {"from", TokenType::FROM},
            {"as", TokenType::AS},
            {"module", TokenType::MODULE},
            {"export", TokenType::EXPORT},
            {"requires", TokenType::REQUIRES},

            // Enum
            {"enum", TokenType::ENUM},

            // OOP
            {"struct", TokenType::STRUCT},
            {"class", TokenType::CLASS},
            {"inherits", TokenType::INHERITS},
            {"immutable", TokenType::IMMUTABLE},
            {"private", TokenType::PRIVATE},
            {"protected", TokenType::PROTECTED},
            {"public", TokenType::PUBLIC},
            {"static", TokenType::STATIC},
            {"interface", TokenType::INTERFACE},
            {"implements", TokenType::IMPLEMENTS},
            {"abstract", TokenType::ABSTRACT},
            {"mixin", TokenType::MIXIN},
            {"with", TokenType::WITH},
            // Generator
            {"yield", TokenType::YIELD},

            // Async
            {"async", TokenType::ASYNC},
            {"await", TokenType::AWAIT},

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
            // Pattern matching
            {"belong", TokenType::BELONG},
            {"bind", TokenType::BIND},
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

    bool Lexer::isHexDigit(char c)
    {
        return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
    }

    bool Lexer::isAlphaNumeric(char c)
    {
        return isAlpha(c) || isDigit(c);
    }

    // Helper: Remove common leading whitespace from multi-line strings
    // Also strips leading/trailing blank lines
    static std::string dedentString(const std::string &str)
    {
        // Split into lines
        std::vector<std::string> lines;
        std::istringstream iss(str);
        std::string line;
        while (std::getline(iss, line))
            lines.push_back(line);

        if (lines.empty())
            return "";

        // Remove leading/trailing blank lines
        size_t start = 0, end = lines.size();
        while (start < end && lines[start].empty())
            start++;
        while (end > start && lines[end - 1].empty())
            end--;

        if (start >= end)
            return ""; // All blank

        // Find minimum indentation (ignoring blank lines)
        size_t minIndent = std::string::npos;
        for (size_t i = start; i < end; i++)
        {
            if (lines[i].empty())
                continue; // Skip blank lines
            size_t indent = 0;
            while (indent < lines[i].size() && (lines[i][indent] == ' ' || lines[i][indent] == '\t'))
                indent++;
            if (indent < minIndent)
                minIndent = indent;
        }

        if (minIndent == std::string::npos || minIndent == 0)
            minIndent = 0;

        // Remove common indentation from all non-blank lines and rebuild
        std::string result;
        for (size_t i = start; i < end; i++)
        {
            if (lines[i].empty())
                result += "\n";
            else
            {
                size_t offset = std::min(minIndent, lines[i].size());
                result += lines[i].substr(offset) + "\n";
            }
        }

        // Remove trailing newline that we added
        if (!result.empty() && result.back() == '\n')
            result.pop_back();

        return result;
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

        // Check for 0x, 0o, 0b prefixes
        if (current() == '0' && !isAtEnd())
        {
            char next = peek(1);
            if (next == 'x' || next == 'X')
            {
                // Hexadecimal: 0x1F, 0xFF
                num += '0';
                advance(); // consume '0'
                num += current();
                advance(); // consume 'x'/'X'
                if (isAtEnd() || !isHexDigit(current()))
                    throw LexerError("Expected hex digit after '0x'", startLine);
                while (!isAtEnd() && (isHexDigit(current()) || current() == '_'))
                {
                    if (current() != '_')
                        num += current();
                    advance();
                }
                // Imaginary suffix
                if (!isAtEnd() && current() == 'i' && !isAlphaNumeric(peek(1)))
                {
                    advance();
                    return Token(TokenType::IMAGINARY, num, startLine);
                }
                return Token(TokenType::NUMBER, num, startLine);
            }
            if (next == 'o' || next == 'O')
            {
                // Octal: 0o77
                num += '0';
                advance(); // consume '0'
                num += current();
                advance(); // consume 'o'/'O'
                if (isAtEnd() || !(current() >= '0' && current() <= '7'))
                    throw LexerError("Expected octal digit after '0o'", startLine);
                while (!isAtEnd() && ((current() >= '0' && current() <= '7') || current() == '_'))
                {
                    if (current() != '_')
                        num += current();
                    advance();
                }
                // Imaginary suffix
                if (!isAtEnd() && current() == 'i' && !isAlphaNumeric(peek(1)))
                {
                    advance();
                    return Token(TokenType::IMAGINARY, num, startLine);
                }
                return Token(TokenType::NUMBER, num, startLine);
            }
            if (next == 'b' || next == 'B')
            {
                // Binary: 0b1010
                num += '0';
                advance(); // consume '0'
                num += current();
                advance(); // consume 'b'/'B'
                if (isAtEnd() || !(current() == '0' || current() == '1'))
                    throw LexerError("Expected binary digit after '0b'", startLine);
                while (!isAtEnd() && (current() == '0' || current() == '1' || current() == '_'))
                {
                    if (current() != '_')
                        num += current();
                    advance();
                }
                // Imaginary suffix
                if (!isAtEnd() && current() == 'i' && !isAlphaNumeric(peek(1)))
                {
                    advance();
                    return Token(TokenType::IMAGINARY, num, startLine);
                }
                return Token(TokenType::NUMBER, num, startLine);
            }
        }

        // Decimal number (with _ separator support)
        while (!isAtEnd() && (isDigit(current()) || current() == '_'))
        {
            if (current() != '_')
                num += current();
            advance();
        }

        // Decimal point followed by digits → floating point
        if (!isAtEnd() && current() == '.' && isDigit(peek(1)))
        {
            num += '.';
            advance(); // consume '.'
            while (!isAtEnd() && (isDigit(current()) || current() == '_'))
            {
                if (current() != '_')
                    num += current();
                advance();
            }
        }

        // Scientific notation: uppercase E only (lowercase e is Euler's constant)
        // e.g. 1E10, 3.14E-2, 6.022E+23
        if (!isAtEnd() && current() == 'E')
        {
            num += 'E';
            advance(); // consume 'E'
            if (!isAtEnd() && (current() == '+' || current() == '-'))
            {
                num += current();
                advance(); // consume sign
            }
            if (isAtEnd() || !isDigit(current()))
                throw LexerError("Expected digit after exponent 'E'", startLine);
            while (!isAtEnd() && (isDigit(current()) || current() == '_'))
            {
                if (current() != '_')
                    num += current();
                advance();
            }
        }

        // Imaginary suffix 'i' → IMAGINARY token (for complex numbers: 2i, 3.14i)
        if (!isAtEnd() && current() == 'i' && !isAlphaNumeric(peek(1)))
        {
            advance(); // consume 'i'
            return Token(TokenType::IMAGINARY, num, startLine);
        }

        return Token(TokenType::NUMBER, num, startLine);
    }

    Token Lexer::readString(char quoteChar)
    {
        int startLine = line_;

        // Check for triple-quoted multi-line string: """...""" or '''...'''
        bool isTripleQuote = (peek(1) == quoteChar && peek(2) == quoteChar);
        if (isTripleQuote)
        {
            advance(); // consume first quote
            advance(); // consume second quote
            advance(); // consume third quote

            std::string str;
            while (!isAtEnd())
            {
                if (current() == quoteChar && peek(1) == quoteChar && peek(2) == quoteChar)
                {
                    advance();
                    advance();
                    advance(); // consume closing triple-quote
                    // Apply dedent only to """ (double-quoted) strings, not ''' (single-quoted)
                    if (quoteChar == '"')
                        str = dedentString(str);
                    return Token(TokenType::STRING, str, startLine);
                }
                if (current() == '\n')
                    line_++;
                str += current();
                advance();
            }
            std::string qstr(3, quoteChar);
            throw LexerError("Unterminated multi-line string literal (expected " + qstr + ")", startLine);
        }

        // Regular string: "..." or '...'
        advance(); // consume opening quote

        // Helper: convert hex char to int value (-1 if invalid)
        auto hexVal = [](char c) -> int
        {
            if (c >= '0' && c <= '9')
                return c - '0';
            if (c >= 'a' && c <= 'f')
                return 10 + c - 'a';
            if (c >= 'A' && c <= 'F')
                return 10 + c - 'A';
            return -1;
        };

        // Helper: encode a Unicode code point as UTF-8
        auto encodeUtf8 = [](uint32_t cp, std::string &out)
        {
            if (cp <= 0x7F)
            {
                out += static_cast<char>(cp);
            }
            else if (cp <= 0x7FF)
            {
                out += static_cast<char>(0xC0 | (cp >> 6));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            else if (cp <= 0xFFFF)
            {
                out += static_cast<char>(0xE0 | (cp >> 12));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
            else if (cp <= 0x10FFFF)
            {
                out += static_cast<char>(0xF0 | (cp >> 18));
                out += static_cast<char>(0x80 | ((cp >> 12) & 0x3F));
                out += static_cast<char>(0x80 | ((cp >> 6) & 0x3F));
                out += static_cast<char>(0x80 | (cp & 0x3F));
            }
        };

        std::string str;
        while (!isAtEnd() && current() != quoteChar)
        {
            if (current() == '\\')
            {
                advance(); // consume backslash
                if (isAtEnd())
                    throw LexerError("Unterminated escape in string literal", startLine);
                char esc = current();
                advance(); // consume escape character
                switch (esc)
                {
                case '"':
                    str += '"';
                    break;
                case '\'':
                    str += '\'';
                    break;
                case '\\':
                    str += '\\';
                    break;
                case 'n':
                    str += '\n';
                    break;
                case 't':
                    str += '\t';
                    break;
                case 'r':
                    str += '\r';
                    break;
                case '0':
                    str += '\0';
                    break;
                case 'a':
                    str += '\a';
                    break;
                case 'b':
                    str += '\b';
                    break;
                case 'f':
                    str += '\f';
                    break;
                case 'v':
                    str += '\v';
                    break;
                case 'x':
                {
                    // \xHH — hex byte escape (2 hex digits)
                    if (isAtEnd())
                        throw LexerError("Incomplete \\x escape in string", startLine);
                    int h1 = hexVal(current());
                    if (h1 < 0)
                        throw LexerError("Invalid hex digit in \\x escape", startLine);
                    advance();
                    if (isAtEnd())
                        throw LexerError("Incomplete \\x escape in string", startLine);
                    int h2 = hexVal(current());
                    if (h2 < 0)
                        throw LexerError("Invalid hex digit in \\x escape", startLine);
                    advance();
                    str += static_cast<char>((h1 << 4) | h2);
                    break;
                }
                case 'u':
                {
                    // \uXXXX — Unicode escape (4 hex digits)
                    uint32_t cp = 0;
                    for (int i = 0; i < 4; ++i)
                    {
                        if (isAtEnd())
                            throw LexerError("Incomplete \\u escape in string", startLine);
                        int d = hexVal(current());
                        if (d < 0)
                            throw LexerError("Invalid hex digit in \\u escape", startLine);
                        cp = (cp << 4) | d;
                        advance();
                    }
                    encodeUtf8(cp, str);
                    break;
                }
                case 'U':
                {
                    // \UXXXXXXXX — Unicode escape (8 hex digits)
                    uint32_t cp = 0;
                    for (int i = 0; i < 8; ++i)
                    {
                        if (isAtEnd())
                            throw LexerError("Incomplete \\U escape in string", startLine);
                        int d = hexVal(current());
                        if (d < 0)
                            throw LexerError("Invalid hex digit in \\U escape", startLine);
                        cp = (cp << 4) | d;
                        advance();
                    }
                    if (cp > 0x10FFFF)
                        throw LexerError("\\U escape out of Unicode range", startLine);
                    encodeUtf8(cp, str);
                    break;
                }
                default:
                    // Unknown escape — keep as-is (backslash + char)
                    str += '\\';
                    str += esc;
                    break;
                }
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

        advance(); // consume closing quote
        return Token(TokenType::STRING, str, startLine);
    }

    // Raw string: r"..." or r'...' — backslashes are treated as literal characters
    Token Lexer::readRawString(char quoteChar)
    {
        int startLine = line_;
        advance(); // consume the 'r'
        advance(); // consume the opening quote

        std::string str;
        while (!isAtEnd() && current() != quoteChar)
        {
            str += current();
            advance();
        }

        if (isAtEnd())
        {
            throw LexerError("Unterminated raw string literal", startLine);
        }

        advance(); // consume closing quote
        return Token(TokenType::RAW_STRING, str, startLine);
    }

    // Byte string: b"..." or b'...' — supports \xHH escape sequences
    Token Lexer::readByteString(char quoteChar)
    {
        int startLine = line_;
        advance(); // consume the 'b'
        advance(); // consume the opening quote

        std::string bytes;
        while (!isAtEnd() && current() != quoteChar)
        {
            if (current() == '\\')
            {
                advance(); // consume backslash
                if (isAtEnd())
                    throw LexerError("Unterminated byte string literal", startLine);
                char esc = current();
                if (esc == 'x' || esc == 'X')
                {
                    // \xHH — two hex digits
                    advance();
                    if (isAtEnd())
                        throw LexerError("Incomplete \\x escape in byte string", startLine);
                    char h1 = current();
                    advance();
                    if (isAtEnd())
                        throw LexerError("Incomplete \\x escape in byte string", startLine);
                    char h2 = current();
                    advance();
                    auto hexVal = [](char c) -> int
                    {
                        if (c >= '0' && c <= '9')
                            return c - '0';
                        if (c >= 'a' && c <= 'f')
                            return 10 + c - 'a';
                        if (c >= 'A' && c <= 'F')
                            return 10 + c - 'A';
                        return -1;
                    };
                    int v1 = hexVal(h1), v2 = hexVal(h2);
                    if (v1 < 0 || v2 < 0)
                        throw LexerError("Invalid hex digit in \\x escape", startLine);
                    bytes += static_cast<char>((v1 << 4) | v2);
                }
                else if (esc == '0')
                {
                    bytes += '\0';
                    advance();
                }
                else if (esc == 'n')
                {
                    bytes += '\n';
                    advance();
                }
                else if (esc == 't')
                {
                    bytes += '\t';
                    advance();
                }
                else if (esc == '\\')
                {
                    bytes += '\\';
                    advance();
                }
                else if (esc == '"')
                {
                    bytes += '"';
                    advance();
                }
                else
                {
                    // Unknown escape — keep as-is
                    bytes += '\\';
                    bytes += esc;
                    advance();
                }
            }
            else
            {
                bytes += current();
                advance();
            }
        }

        if (isAtEnd())
            throw LexerError("Unterminated byte string literal", startLine);

        advance(); // consume closing quote
        return Token(TokenType::BYTE_STRING, bytes, startLine);
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

            // --- String literal (double quote) ---
            if (c == '"')
            {
                tokens.push_back(readString('"'));
                continue;
            }

            // --- String literal (single quote) ---
            if (c == '\'')
            {
                tokens.push_back(readString('\''));
                continue;
            }

            // --- Identifier or keyword (also handles raw string r"..."/ r'...' and byte string b"..."/ b'...') ---
            if (isAlpha(c))
            {
                // Check for raw string: r"..." or r'...'
                if (c == 'r' && (peek(1) == '"' || peek(1) == '\''))
                {
                    tokens.push_back(readRawString(peek(1)));
                    continue;
                }
                // Check for byte string: b"..." or b'...'
                if (c == 'b' && (peek(1) == '"' || peek(1) == '\''))
                {
                    tokens.push_back(readByteString(peek(1)));
                    continue;
                }
                tokens.push_back(readIdentifierOrKeyword());
                continue;
            }

            // --- Operators and delimiters ---
            // (using a dispatch approach — each character handled explicitly)

            // + and ++ and +=
            if (c == '+')
            {
                if (peek(1) == '+')
                {
                    tokens.emplace_back(TokenType::PLUS_PLUS, "++", tokenLine);
                    advance();
                    advance();
                }
                else if (peek(1) == '=')
                {
                    tokens.emplace_back(TokenType::PLUS_EQUAL, "+=", tokenLine);
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
                else if (peek(1) == '=')
                {
                    tokens.emplace_back(TokenType::MINUS_EQUAL, "-=", tokenLine);
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
                if (peek(1) == '*' && peek(2) == '=')
                {
                    tokens.emplace_back(TokenType::STAR_STAR_EQUAL, "**=", tokenLine);
                    advance();
                    advance();
                    advance();
                }
                else if (peek(1) == '*')
                {
                    tokens.emplace_back(TokenType::STAR_STAR, "**", tokenLine);
                    advance();
                    advance();
                }
                else if (peek(1) == '=')
                {
                    tokens.emplace_back(TokenType::STAR_EQUAL, "*=", tokenLine);
                    advance();
                    advance();
                }
                else
                {
                    tokens.emplace_back(TokenType::STAR, "*", tokenLine);
                    advance();
                }
                continue;
            }

            if (c == '/')
            {
                if (peek(1) == '=')
                {
                    tokens.emplace_back(TokenType::SLASH_EQUAL, "/=", tokenLine);
                    advance();
                    advance();
                }
                else
                {
                    tokens.emplace_back(TokenType::SLASH, "/", tokenLine);
                    advance();
                }
                continue;
            }

            if (c == '%')
            {
                if (peek(1) == '=')
                {
                    tokens.emplace_back(TokenType::PERCENT_EQUAL, "%=", tokenLine);
                    advance();
                    advance();
                }
                else
                {
                    tokens.emplace_back(TokenType::PERCENT, "%", tokenLine);
                    advance();
                }
                continue;
            }

            // = and == and =>
            if (c == '=')
            {
                if (peek(1) == '=')
                {
                    tokens.emplace_back(TokenType::EQUAL_EQUAL, "==", tokenLine);
                    advance();
                    advance();
                }
                else if (peek(1) == '>')
                {
                    tokens.emplace_back(TokenType::FAT_ARROW, "=>", tokenLine);
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

            // > / >= / >> / >>=
            if (c == '>')
            {
                if (peek(1) == '>' && peek(2) == '=')
                {
                    tokens.emplace_back(TokenType::RSHIFT_EQUAL, ">>=", tokenLine);
                    advance();
                    advance();
                    advance();
                }
                else if (peek(1) == '>')
                {
                    tokens.emplace_back(TokenType::RSHIFT, ">>", tokenLine);
                    advance();
                    advance();
                }
                else if (peek(1) == '=')
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

            // < / <= / << / <<=
            if (c == '<')
            {
                if (peek(1) == '<' && peek(2) == '=')
                {
                    tokens.emplace_back(TokenType::LSHIFT_EQUAL, "<<=", tokenLine);
                    advance();
                    advance();
                    advance();
                }
                else if (peek(1) == '<')
                {
                    tokens.emplace_back(TokenType::LSHIFT, "<<", tokenLine);
                    advance();
                    advance();
                }
                else if (peek(1) == '=')
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
                if (peek(1) == '.' && peek(2) == '.')
                {
                    tokens.emplace_back(TokenType::ELLIPSIS, "...", tokenLine);
                    advance();
                    advance();
                    advance();
                }
                else
                {
                    tokens.emplace_back(TokenType::DOT, ".", tokenLine);
                    advance();
                }
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

            // |> / || / |= / |
            if (c == '|')
            {
                if (peek(1) == '>')
                {
                    tokens.emplace_back(TokenType::PIPE, "|>", tokenLine);
                    advance();
                    advance();
                }
                else if (peek(1) == '|')
                {
                    tokens.emplace_back(TokenType::PIPE_PIPE, "||", tokenLine);
                    advance();
                    advance();
                }
                else if (peek(1) == '=')
                {
                    tokens.emplace_back(TokenType::PIPE_EQUAL, "|=", tokenLine);
                    advance();
                    advance();
                }
                else
                {
                    tokens.emplace_back(TokenType::BAR, "|", tokenLine);
                    advance();
                }
                continue;
            }

            // & / && / &=
            if (c == '&')
            {
                if (peek(1) == '&')
                {
                    tokens.emplace_back(TokenType::AMP_AMP, "&&", tokenLine);
                    advance();
                    advance();
                }
                else if (peek(1) == '=')
                {
                    tokens.emplace_back(TokenType::AMP_EQUAL, "&=", tokenLine);
                    advance();
                    advance();
                }
                else
                {
                    tokens.emplace_back(TokenType::AMP, "&", tokenLine);
                    advance();
                }
                continue;
            }

            // ^ / ^=
            if (c == '^')
            {
                if (peek(1) == '=')
                {
                    tokens.emplace_back(TokenType::CARET_EQUAL, "^=", tokenLine);
                    advance();
                    advance();
                }
                else
                {
                    tokens.emplace_back(TokenType::CARET, "^", tokenLine);
                    advance();
                }
                continue;
            }

            // @ (decorator)
            if (c == '@')
            {
                tokens.emplace_back(TokenType::AT, "@", tokenLine);
                advance();
                continue;
            }

            // ~ (smart-cast prefix)
            if (c == '~')
            {
                tokens.emplace_back(TokenType::TILDE, "~", tokenLine);
                advance();
                continue;
            }

            // $ (shell command: collect rest of line as command string)
            if (c == '$')
            {
                int cmdLine = tokenLine;
                advance(); // consume $
                std::string cmd;
                while (pos_ < source_.size() && source_[pos_] != '\n')
                {
                    cmd += source_[pos_];
                    ++pos_;
                }
                // Trim trailing whitespace
                while (!cmd.empty() && (cmd.back() == ' ' || cmd.back() == '\r' || cmd.back() == '\t'))
                    cmd.pop_back();
                tokens.emplace_back(TokenType::SHELL_CMD, cmd, cmdLine);
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
