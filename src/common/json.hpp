#pragma once

// =============================================================================
// Standalone JSON Core — parser, serializer, and value type
// =============================================================================
//
// Completely independent of XObject, GCHeap, and runtime interpreter.
// Conforms to RFC 8259:
//   - All escape sequences (\", \\, \/, \b, \f, \n, \r, \t, \uXXXX)
//   - UTF-16 surrogate pairs (\uD800..\uDBFF + \uDC00..\uDFFF) to UTF-8
//   - Rejection of lone surrogates and invalid escapes
//   - Strict number validation (no leading zeros, digit after decimal,
//     digit after exponent, rejection of NaN/Infinity)
//   - Configurable recursion depth limit (default 500)
//   - Error reporting with line and column numbers
//   - Compact or indented output with sorted keys
//
// =============================================================================

#include <string>
#include <vector>
#include <map>
#include <variant>
#include <stdexcept>
#include <cstdint>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <algorithm>

namespace xell
{
namespace json
{

    class Value;
    using Array = std::vector<Value>;
    using Object = std::map<std::string, Value>;

    enum class Type
    {
        Null,
        Bool,
        Int,
        Float,
        String,
        Array,
        Object
    };

    class ParseError : public std::runtime_error
    {
    public:
        size_t line;
        size_t column;
        ParseError(const std::string &msg, size_t ln, size_t col)
            : std::runtime_error(msg + " at line " + std::to_string(ln) + ", column " + std::to_string(col)),
              line(ln), column(col) {}
    };

    class Value
    {
    public:
        using Data = std::variant<std::monostate, bool, int64_t, double, std::string, Array, Object>;
        Data data;

        Value() : data(std::monostate{}) {}
        Value(std::nullptr_t) : data(std::monostate{}) {}
        Value(bool b) : data(b) {}
        Value(int i) : data(static_cast<int64_t>(i)) {}
        Value(int64_t i) : data(i) {}
        Value(double d) : data(d) {}
        Value(const char *s) : data(std::string(s)) {}
        Value(std::string s) : data(std::move(s)) {}
        Value(Array a) : data(std::move(a)) {}
        Value(Object o) : data(std::move(o)) {}

        Type type() const
        {
            if (isNull()) return Type::Null;
            if (isBool()) return Type::Bool;
            if (isInt()) return Type::Int;
            if (isFloat()) return Type::Float;
            if (isString()) return Type::String;
            if (isArray()) return Type::Array;
            return Type::Object;
        }

        bool isNull() const { return std::holds_alternative<std::monostate>(data); }
        bool isBool() const { return std::holds_alternative<bool>(data); }
        bool isInt() const { return std::holds_alternative<int64_t>(data); }
        bool isFloat() const { return std::holds_alternative<double>(data); }
        bool isNumber() const { return isInt() || isFloat(); }
        bool isString() const { return std::holds_alternative<std::string>(data); }
        bool isArray() const { return std::holds_alternative<Array>(data); }
        bool isObject() const { return std::holds_alternative<Object>(data); }

        bool asBool() const { return std::get<bool>(data); }
        int64_t asInt() const { return std::get<int64_t>(data); }
        double asFloat() const
        {
            if (isInt()) return static_cast<double>(asInt());
            return std::get<double>(data);
        }
        const std::string &asString() const { return std::get<std::string>(data); }
        std::string &asString() { return std::get<std::string>(data); }
        const Array &asArray() const { return std::get<Array>(data); }
        Array &asArray() { return std::get<Array>(data); }
        const Object &asObject() const { return std::get<Object>(data); }
        Object &asObject() { return std::get<Object>(data); }

        const Value &operator[](const std::string &key) const
        {
            static const Value nullVal;
            if (!isObject()) return nullVal;
            const auto &obj = asObject();
            auto it = obj.find(key);
            return (it != obj.end()) ? it->second : nullVal;
        }

        Value &operator[](const std::string &key)
        {
            if (!isObject()) data = Object{};
            return std::get<Object>(data)[key];
        }

        const Value &operator[](size_t idx) const
        {
            static const Value nullVal;
            if (!isArray()) return nullVal;
            const auto &arr = asArray();
            return (idx < arr.size()) ? arr[idx] : nullVal;
        }

        Value &operator[](size_t idx)
        {
            if (!isArray()) data = Array{};
            return std::get<Array>(data).at(idx);
        }

        bool contains(const std::string &key) const
        {
            if (!isObject()) return false;
            return asObject().find(key) != asObject().end();
        }
        bool has(const std::string &key) const { return contains(key); }

        std::string getString(const std::string &key, const std::string &def = "") const
        {
            if (!isObject()) return def;
            auto it = asObject().find(key);
            if (it != asObject().end() && it->second.isString()) return it->second.asString();
            return def;
        }

        int64_t getInt(const std::string &key, int64_t def = 0) const
        {
            if (!isObject()) return def;
            auto it = asObject().find(key);
            if (it != asObject().end() && it->second.isInt()) return it->second.asInt();
            return def;
        }

        double getFloat(const std::string &key, double def = 0.0) const
        {
            if (!isObject()) return def;
            auto it = asObject().find(key);
            if (it != asObject().end() && it->second.isNumber()) return it->second.asFloat();
            return def;
        }

        bool getBool(const std::string &key, bool def = false) const
        {
            if (!isObject()) return def;
            auto it = asObject().find(key);
            if (it != asObject().end() && it->second.isBool()) return it->second.asBool();
            return def;
        }
    };

    namespace detail
    {
        inline void encodeUtf8(uint32_t cp, std::string &out)
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
        }

        inline int hexVal(char c)
        {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
            return -1;
        }

        class Parser
        {
        public:
            const std::string &src;
            size_t pos = 0;
            size_t line = 1;
            size_t col = 1;
            size_t maxDepth = 500;
            size_t currentDepth = 0;

            Parser(const std::string &s, size_t depthLimit)
                : src(s), maxDepth(depthLimit) {}

            bool isAtEnd() const { return pos >= src.size(); }

            char peek()
            {
                skipWhitespace();
                if (isAtEnd())
                    throw ParseError("Unexpected end of input", line, col);
                return src[pos];
            }

            char advance()
            {
                if (pos < src.size())
                {
                    char c = src[pos++];
                    if (c == '\n')
                    {
                        line++;
                        col = 1;
                    }
                    else
                    {
                        col++;
                    }
                    return c;
                }
                return '\0';
            }

            void skipWhitespace()
            {
                while (pos < src.size())
                {
                    char c = src[pos];
                    if (c == ' ' || c == '\t' || c == '\r')
                    {
                        pos++;
                        col++;
                    }
                    else if (c == '\n')
                    {
                        pos++;
                        line++;
                        col = 1;
                    }
                    else
                    {
                        break;
                    }
                }
            }

            void expect(char expected)
            {
                skipWhitespace();
                if (isAtEnd())
                    throw ParseError(std::string("Expected '") + expected + "', got end of input", line, col);
                size_t curLine = line;
                size_t curCol = col;
                char got = advance();
                if (got != expected)
                {
                    throw ParseError(std::string("Expected '") + expected + "', got '" + got + "'", curLine, curCol);
                }
            }

            uint32_t readHex4()
            {
                uint32_t cp = 0;
                for (int i = 0; i < 4; ++i)
                {
                    if (pos >= src.size())
                        throw ParseError("Incomplete \\u escape sequence", line, col);
                    size_t curLine = line;
                    size_t curCol = col;
                    char c = advance();
                    int h = hexVal(c);
                    if (h < 0)
                        throw ParseError(std::string("Invalid hex digit '") + c + "' in \\u escape", curLine, curCol);
                    cp = (cp << 4) | static_cast<uint32_t>(h);
                }
                return cp;
            }

            std::string parseString()
            {
                skipWhitespace();
                if (isAtEnd() || src[pos] != '"')
                    throw ParseError("Expected '\"'", line, col);

                size_t startLine = line;
                size_t startCol = col;
                advance(); // consume opening quote

                std::string result;
                while (pos < src.size() && src[pos] != '"')
                {
                    char c = src[pos];
                    if (static_cast<unsigned char>(c) < 0x20)
                    {
                        throw ParseError("Unescaped control character in string", line, col);
                    }
                    if (c == '\\')
                    {
                        advance(); // consume '\\'
                        if (pos >= src.size())
                            throw ParseError("Unterminated escape sequence in string", line, col);

                        size_t escLine = line;
                        size_t escCol = col;
                        char esc = advance();
                        switch (esc)
                        {
                        case '"':  result += '"'; break;
                        case '\\': result += '\\'; break;
                        case '/':  result += '/'; break;
                        case 'b':  result += '\b'; break;
                        case 'f':  result += '\f'; break;
                        case 'n':  result += '\n'; break;
                        case 'r':  result += '\r'; break;
                        case 't':  result += '\t'; break;
                        case 'u':
                        {
                            uint32_t cp = readHex4();
                            if (cp >= 0xD800 && cp <= 0xDBFF)
                            {
                                // High surrogate: expect following \uDC00..\uDFFF
                                if (pos + 1 < src.size() && src[pos] == '\\' && src[pos + 1] == 'u')
                                {
                                    advance(); // '\\'
                                    advance(); // 'u'
                                    uint32_t low = readHex4();
                                    if (low < 0xDC00 || low > 0xDFFF)
                                    {
                                        throw ParseError("Expected low surrogate after high surrogate", escLine, escCol);
                                    }
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                }
                                else
                                {
                                    throw ParseError("Lone high surrogate in string", escLine, escCol);
                                }
                            }
                            else if (cp >= 0xDC00 && cp <= 0xDFFF)
                            {
                                throw ParseError("Lone low surrogate in string", escLine, escCol);
                            }
                            encodeUtf8(cp, result);
                            break;
                        }
                        default:
                            throw ParseError(std::string("Invalid escape sequence '\\") + esc + "'", escLine, escCol);
                        }
                    }
                    else
                    {
                        result += c;
                        advance();
                    }
                }

                if (pos >= src.size())
                    throw ParseError("Unterminated string", startLine, startCol);

                advance(); // consume closing quote
                return result;
            }

            Value parseNumber()
            {
                skipWhitespace();
                size_t startLine = line;
                size_t startCol = col;
                size_t startPos = pos;
                bool isFloat = false;

                if (pos < src.size() && src[pos] == '-')
                {
                    advance();
                }

                if (pos >= src.size() || !std::isdigit(static_cast<unsigned char>(src[pos])))
                {
                    throw ParseError("Expected digit in number", startLine, startCol);
                }

                if (src[pos] == '0')
                {
                    advance();
                    // Leading zeros are strictly forbidden (e.g. "01")
                    if (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
                    {
                        throw ParseError("Leading zeros are not permitted in numbers", startLine, startCol);
                    }
                }
                else
                {
                    while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
                    {
                        advance();
                    }
                }

                // Fraction
                if (pos < src.size() && src[pos] == '.')
                {
                    isFloat = true;
                    advance();
                    if (pos >= src.size() || !std::isdigit(static_cast<unsigned char>(src[pos])))
                    {
                        throw ParseError("Expected digit after decimal point", line, col);
                    }
                    while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
                    {
                        advance();
                    }
                }

                // Exponent
                if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E'))
                {
                    isFloat = true;
                    advance();
                    if (pos < src.size() && (src[pos] == '+' || src[pos] == '-'))
                    {
                        advance();
                    }
                    if (pos >= src.size() || !std::isdigit(static_cast<unsigned char>(src[pos])))
                    {
                        throw ParseError("Expected digit in exponent", line, col);
                    }
                    while (pos < src.size() && std::isdigit(static_cast<unsigned char>(src[pos])))
                    {
                        advance();
                    }
                }

                std::string numStr = src.substr(startPos, pos - startPos);
                if (isFloat)
                {
                    double val = std::stod(numStr);
                    if (std::isinf(val) || std::isnan(val))
                    {
                        throw ParseError("Number out of range (infinite/NaN)", startLine, startCol);
                    }
                    return Value(val);
                }
                else
                {
                    try
                    {
                        int64_t val = std::stoll(numStr);
                        return Value(val);
                    }
                    catch (...)
                    {
                        double val = std::stod(numStr);
                        if (std::isinf(val) || std::isnan(val))
                        {
                            throw ParseError("Number out of range (infinite/NaN)", startLine, startCol);
                        }
                        return Value(val);
                    }
                }
            }

            Value parseArray()
            {
                size_t startLine = line;
                size_t startCol = col;
                expect('[');
                if (currentDepth >= maxDepth)
                {
                    throw ParseError("JSON depth limit exceeded", startLine, startCol);
                }
                currentDepth++;

                Array arr;
                skipWhitespace();
                if (pos < src.size() && src[pos] == ']')
                {
                    advance();
                    currentDepth--;
                    return Value(std::move(arr));
                }

                while (true)
                {
                    arr.push_back(parseValue());
                    skipWhitespace();
                    if (pos < src.size() && src[pos] == ']')
                    {
                        advance();
                        break;
                    }
                    if (pos < src.size() && src[pos] == ',')
                    {
                        advance();
                        continue;
                    }
                    throw ParseError("Expected ',' or ']' in array", line, col);
                }

                currentDepth--;
                return Value(std::move(arr));
            }

            Value parseObject()
            {
                size_t startLine = line;
                size_t startCol = col;
                expect('{');
                if (currentDepth >= maxDepth)
                {
                    throw ParseError("JSON depth limit exceeded", startLine, startCol);
                }
                currentDepth++;

                Object obj;
                skipWhitespace();
                if (pos < src.size() && src[pos] == '}')
                {
                    advance();
                    currentDepth--;
                    return Value(std::move(obj));
                }

                while (true)
                {
                    std::string key = parseString();
                    expect(':');
                    Value val = parseValue();
                    obj[key] = std::move(val);

                    skipWhitespace();
                    if (pos < src.size() && src[pos] == '}')
                    {
                        advance();
                        break;
                    }
                    if (pos < src.size() && src[pos] == ',')
                    {
                        advance();
                        continue;
                    }
                    throw ParseError("Expected ',' or '}' in object", line, col);
                }

                currentDepth--;
                return Value(std::move(obj));
            }

            Value parseValue()
            {
                skipWhitespace();
                if (isAtEnd())
                    throw ParseError("Unexpected end of input", line, col);

                char c = src[pos];
                if (c == '"')
                    return Value(parseString());
                if (c == '{')
                    return parseObject();
                if (c == '[')
                    return parseArray();
                if (c == '-' || (c >= '0' && c <= '9'))
                    return parseNumber();
                if (c == 't')
                {
                    size_t curLine = line;
                    size_t curCol = col;
                    if (src.compare(pos, 4, "true") == 0)
                    {
                        advance(); advance(); advance(); advance();
                        return Value(true);
                    }
                    throw ParseError("Expected 'true'", curLine, curCol);
                }
                if (c == 'f')
                {
                    size_t curLine = line;
                    size_t curCol = col;
                    if (src.compare(pos, 5, "false") == 0)
                    {
                        advance(); advance(); advance(); advance(); advance();
                        return Value(false);
                    }
                    throw ParseError("Expected 'false'", curLine, curCol);
                }
                if (c == 'n')
                {
                    size_t curLine = line;
                    size_t curCol = col;
                    if (src.compare(pos, 4, "null") == 0)
                    {
                        advance(); advance(); advance(); advance();
                        return Value(nullptr);
                    }
                    throw ParseError("Expected 'null'", curLine, curCol);
                }

                throw ParseError(std::string("Unexpected character '") + c + "'", line, col);
            }
        };

        inline std::string escapeString(const std::string &s)
        {
            std::string out = "\"";
            out.reserve(s.size() + 16);
            for (char c : s)
            {
                switch (c)
                {
                case '"':  out += "\\\""; break;
                case '\\': out += "\\\\"; break;
                case '\b': out += "\\b"; break;
                case '\f': out += "\\f"; break;
                case '\n': out += "\\n"; break;
                case '\r': out += "\\r"; break;
                case '\t': out += "\\t"; break;
                default:
                    if (static_cast<unsigned char>(c) < 0x20)
                    {
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                        out += buf;
                    }
                    else
                    {
                        out += c;
                    }
                    break;
                }
            }
            out += '"';
            return out;
        }

        inline void stringifyInternal(const Value &val, int indent, int depth, std::string &out)
        {
            switch (val.type())
            {
            case Type::Null:
                out += "null";
                break;
            case Type::Bool:
                out += (val.asBool() ? "true" : "false");
                break;
            case Type::Int:
                out += std::to_string(val.asInt());
                break;
            case Type::Float:
            {
                double d = val.asFloat();
                if (std::isnan(d) || std::isinf(d))
                {
                    throw std::invalid_argument("NaN and Infinity cannot be represented in JSON");
                }
                char buf[64];
                snprintf(buf, sizeof(buf), "%.16g", d);
                out += buf;
                break;
            }
            case Type::String:
                out += escapeString(val.asString());
                break;
            case Type::Array:
            {
                const auto &arr = val.asArray();
                if (arr.empty())
                {
                    out += "[]";
                    break;
                }
                out += '[';
                if (indent > 0) out += '\n';
                std::string pad(indent > 0 ? (depth + 1) * indent : 0, ' ');
                std::string closePad(indent > 0 ? depth * indent : 0, ' ');
                for (size_t i = 0; i < arr.size(); ++i)
                {
                    if (i > 0)
                    {
                        out += ',';
                        if (indent > 0) out += '\n';
                    }
                    if (indent > 0) out += pad;
                    stringifyInternal(arr[i], indent, depth + 1, out);
                }
                if (indent > 0)
                {
                    out += '\n';
                    out += closePad;
                }
                out += ']';
                break;
            }
            case Type::Object:
            {
                const auto &obj = val.asObject();
                if (obj.empty())
                {
                    out += "{}";
                    break;
                }
                out += '{';
                if (indent > 0) out += '\n';
                std::string pad(indent > 0 ? (depth + 1) * indent : 0, ' ');
                std::string closePad(indent > 0 ? depth * indent : 0, ' ');
                bool first = true;
                // std::map is automatically sorted by key!
                for (const auto &[k, v] : obj)
                {
                    if (!first)
                    {
                        out += ',';
                        if (indent > 0) out += '\n';
                    }
                    first = false;
                    if (indent > 0) out += pad;
                    out += escapeString(k);
                    out += (indent > 0 ? ": " : ":");
                    stringifyInternal(v, indent, depth + 1, out);
                }
                if (indent > 0)
                {
                    out += '\n';
                    out += closePad;
                }
                out += '}';
                break;
            }
            }
        }
    } // namespace detail

    inline Value parse(const std::string &src, size_t maxDepth = 500)
    {
        detail::Parser p(src, maxDepth);
        p.skipWhitespace();
        if (p.isAtEnd())
        {
            throw ParseError("Unexpected end of input", 1, 1);
        }
        Value val = p.parseValue();
        p.skipWhitespace();
        if (!p.isAtEnd())
        {
            throw ParseError("Unexpected trailing content", p.line, p.col);
        }
        return val;
    }

    inline std::string stringify(const Value &val, int indent = 0)
    {
        std::string out;
        detail::stringifyInternal(val, indent, 0, out);
        return out;
    }

} // namespace json
} // namespace xell
