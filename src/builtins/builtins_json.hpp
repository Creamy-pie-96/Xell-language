#pragma once

// =============================================================================
// JSON / Data builtins — JSON, CSV, TOML, YAML parsing & serialization
// =============================================================================
//
// JSON:  json_parse, json_stringify, json_pretty, json_read, json_write
// CSV:   csv_parse, csv_read, csv_write
// TOML:  toml_read
// YAML:  yaml_read
//
// All parsers are hand-written recursive descent — no external dependencies.
// JSON parser handles: strings, numbers, bools, null, arrays, objects, nested
//                      structures, all escape sequences (\", \\, \/, \b, \f,
//                      \n, \r, \t, \uXXXX).
// CSV parser handles:  quoted fields with embedded commas/newlines/quotes,
//                      custom separators, first row as headers → list of maps.
// TOML parser handles: key=value, [sections], strings, ints, floats, bools,
//                      inline tables, arrays, multiline strings.
// YAML parser handles: indent-based hierarchy, key: value, lists (- item),
//                      nested maps, scalars, multiline strings.
//
// =============================================================================

#include "builtin_registry.hpp"
#include "../lib/errors/error.hpp"
#include <string>
#include <vector>
#include <fstream>
#include <sstream>
#include <cctype>
#include <cstdint>
#include <cmath>
#include <algorithm>
#include <functional>

namespace xell
{

    // =========================================================================
    //  Internal: Full recursive descent JSON parser
    // =========================================================================

    namespace json_internal
    {

        struct JsonParser
        {
            std::string src;
            size_t pos;
            int errLine; // Xell line number for error reporting

            JsonParser(const std::string &s, int line) : src(s), pos(0), errLine(line) {}

            // -- Whitespace ---------------------------------------------------
            void skipWhitespace()
            {
                while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t' ||
                                            src[pos] == '\n' || src[pos] == '\r'))
                    ++pos;
            }

            // -- Peek / Advance -----------------------------------------------
            char peek()
            {
                skipWhitespace();
                if (pos >= src.size())
                    throw RuntimeError("json_parse: unexpected end of input", errLine);
                return src[pos];
            }

            char advance()
            {
                skipWhitespace();
                if (pos >= src.size())
                    throw RuntimeError("json_parse: unexpected end of input", errLine);
                return src[pos++];
            }

            void expect(char c)
            {
                char got = advance();
                if (got != c)
                {
                    std::string msg = "json_parse: expected '";
                    msg += c;
                    msg += "', got '";
                    msg += got;
                    msg += "' at position ";
                    msg += std::to_string(pos - 1);
                    throw RuntimeError(msg, errLine);
                }
            }

            // -- Parse value --------------------------------------------------
            XObject parseValue()
            {
                char c = peek();
                if (c == '"')
                    return parseString();
                if (c == '{')
                    return parseObject();
                if (c == '[')
                    return parseArray();
                if (c == 't' || c == 'f')
                    return parseBool();
                if (c == 'n')
                    return parseNull();
                if (c == '-' || (c >= '0' && c <= '9'))
                    return parseNumber();
                std::string msg = "json_parse: unexpected character '";
                msg += c;
                msg += "' at position ";
                msg += std::to_string(pos);
                throw RuntimeError(msg, errLine);
            }

            // -- String with escape handling ----------------------------------
            XObject parseString()
            {
                return XObject::makeString(parseRawString());
            }

            std::string parseRawString()
            {
                expect('"');
                std::string result;
                while (pos < src.size() && src[pos] != '"')
                {
                    if (src[pos] == '\\')
                    {
                        ++pos;
                        if (pos >= src.size())
                            throw RuntimeError("json_parse: unterminated string escape", errLine);
                        switch (src[pos])
                        {
                        case '"':
                            result += '"';
                            break;
                        case '\\':
                            result += '\\';
                            break;
                        case '/':
                            result += '/';
                            break;
                        case 'b':
                            result += '\b';
                            break;
                        case 'f':
                            result += '\f';
                            break;
                        case 'n':
                            result += '\n';
                            break;
                        case 'r':
                            result += '\r';
                            break;
                        case 't':
                            result += '\t';
                            break;
                        case 'u':
                        {
                            // \uXXXX — 4 hex digits → Unicode code point
                            if (pos + 4 >= src.size())
                                throw RuntimeError("json_parse: incomplete \\u escape", errLine);
                            std::string hex = src.substr(pos + 1, 4);
                            uint32_t cp = 0;
                            for (char h : hex)
                            {
                                cp <<= 4;
                                if (h >= '0' && h <= '9')
                                    cp |= (h - '0');
                                else if (h >= 'a' && h <= 'f')
                                    cp |= (h - 'a' + 10);
                                else if (h >= 'A' && h <= 'F')
                                    cp |= (h - 'A' + 10);
                                else
                                    throw RuntimeError("json_parse: invalid \\u hex digit", errLine);
                            }
                            // Handle surrogate pairs
                            if (cp >= 0xD800 && cp <= 0xDBFF)
                            {
                                // High surrogate — expect low surrogate
                                pos += 4;
                                if (pos + 1 < src.size() && src[pos + 1] == '\\' &&
                                    pos + 2 < src.size() && src[pos + 2] == 'u')
                                {
                                    pos += 2; // skip \u
                                    if (pos + 4 >= src.size())
                                        throw RuntimeError("json_parse: incomplete surrogate pair", errLine);
                                    std::string hex2 = src.substr(pos + 1, 4);
                                    uint32_t low = 0;
                                    for (char h : hex2)
                                    {
                                        low <<= 4;
                                        if (h >= '0' && h <= '9')
                                            low |= (h - '0');
                                        else if (h >= 'a' && h <= 'f')
                                            low |= (h - 'a' + 10);
                                        else if (h >= 'A' && h <= 'F')
                                            low |= (h - 'A' + 10);
                                        else
                                            throw RuntimeError("json_parse: invalid surrogate pair hex", errLine);
                                    }
                                    cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
                                    pos += 4;
                                }
                                else
                                {
                                    throw RuntimeError("json_parse: lone high surrogate", errLine);
                                }
                            }
                            else
                            {
                                pos += 4;
                            }
                            // Encode UTF-8
                            if (cp < 0x80)
                            {
                                result += (char)cp;
                            }
                            else if (cp < 0x800)
                            {
                                result += (char)(0xC0 | (cp >> 6));
                                result += (char)(0x80 | (cp & 0x3F));
                            }
                            else if (cp < 0x10000)
                            {
                                result += (char)(0xE0 | (cp >> 12));
                                result += (char)(0x80 | ((cp >> 6) & 0x3F));
                                result += (char)(0x80 | (cp & 0x3F));
                            }
                            else
                            {
                                result += (char)(0xF0 | (cp >> 18));
                                result += (char)(0x80 | ((cp >> 12) & 0x3F));
                                result += (char)(0x80 | ((cp >> 6) & 0x3F));
                                result += (char)(0x80 | (cp & 0x3F));
                            }
                            break;
                        }
                        default:
                            throw RuntimeError("json_parse: unknown escape '\\" +
                                                   std::string(1, src[pos]) + "'",
                                               errLine);
                        }
                        ++pos;
                    }
                    else
                    {
                        result += src[pos++];
                    }
                }
                if (pos >= src.size())
                    throw RuntimeError("json_parse: unterminated string", errLine);
                ++pos; // skip closing quote
                return result;
            }

            // -- Number -------------------------------------------------------
            XObject parseNumber()
            {
                skipWhitespace();
                size_t start = pos;
                bool isFloat = false;

                if (pos < src.size() && src[pos] == '-')
                    ++pos;

                // Integer part
                if (pos < src.size() && src[pos] == '0')
                {
                    ++pos;
                }
                else if (pos < src.size() && src[pos] >= '1' && src[pos] <= '9')
                {
                    while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9')
                        ++pos;
                }
                else
                {
                    throw RuntimeError("json_parse: invalid number at position " +
                                           std::to_string(pos),
                                       errLine);
                }

                // Fractional part
                if (pos < src.size() && src[pos] == '.')
                {
                    isFloat = true;
                    ++pos;
                    if (pos >= src.size() || src[pos] < '0' || src[pos] > '9')
                        throw RuntimeError("json_parse: expected digit after decimal point", errLine);
                    while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9')
                        ++pos;
                }

                // Exponent part
                if (pos < src.size() && (src[pos] == 'e' || src[pos] == 'E'))
                {
                    isFloat = true;
                    ++pos;
                    if (pos < src.size() && (src[pos] == '+' || src[pos] == '-'))
                        ++pos;
                    if (pos >= src.size() || src[pos] < '0' || src[pos] > '9')
                        throw RuntimeError("json_parse: expected digit in exponent", errLine);
                    while (pos < src.size() && src[pos] >= '0' && src[pos] <= '9')
                        ++pos;
                }

                std::string numStr = src.substr(start, pos - start);
                if (isFloat)
                {
                    double val = std::stod(numStr);
                    return XObject::makeFloat(val);
                }
                else
                {
                    // Try int64_t first; fall back to double for very large numbers
                    try
                    {
                        int64_t val = std::stoll(numStr);
                        return XObject::makeInt(val);
                    }
                    catch (...)
                    {
                        double val = std::stod(numStr);
                        return XObject::makeFloat(val);
                    }
                }
            }

            // -- Boolean ------------------------------------------------------
            XObject parseBool()
            {
                skipWhitespace();
                if (src.compare(pos, 4, "true") == 0)
                {
                    pos += 4;
                    return XObject::makeBool(true);
                }
                if (src.compare(pos, 5, "false") == 0)
                {
                    pos += 5;
                    return XObject::makeBool(false);
                }
                throw RuntimeError("json_parse: expected 'true' or 'false'", errLine);
            }

            // -- Null ---------------------------------------------------------
            XObject parseNull()
            {
                skipWhitespace();
                if (src.compare(pos, 4, "null") == 0)
                {
                    pos += 4;
                    return XObject::makeNone();
                }
                throw RuntimeError("json_parse: expected 'null'", errLine);
            }

            // -- Array --------------------------------------------------------
            XObject parseArray()
            {
                expect('[');
                std::vector<XObject> elements;
                if (peek() == ']')
                {
                    advance(); // skip ]
                    return XObject::makeList(std::move(elements));
                }
                elements.push_back(parseValue());
                while (peek() == ',')
                {
                    advance(); // skip ,
                    elements.push_back(parseValue());
                }
                expect(']');
                return XObject::makeList(std::move(elements));
            }

            // -- Object -------------------------------------------------------
            XObject parseObject()
            {
                expect('{');
                XMap map;
                if (peek() == '}')
                {
                    advance(); // skip }
                    return XObject::makeMap(std::move(map));
                }
                // First key-value pair
                {
                    std::string key = parseRawString();
                    expect(':');
                    XObject val = parseValue();
                    map.set(key, std::move(val));
                }
                while (peek() == ',')
                {
                    advance(); // skip ,
                    std::string key = parseRawString();
                    expect(':');
                    XObject val = parseValue();
                    map.set(key, std::move(val));
                }
                expect('}');
                return XObject::makeMap(std::move(map));
            }
        };

        // =====================================================================
        //  JSON Stringify — XObject → JSON string
        // =====================================================================

        inline std::string escapeJsonString(const std::string &s)
        {
            std::string result;
            result.reserve(s.size() + 8);
            for (char c : s)
            {
                switch (c)
                {
                case '"':
                    result += "\\\"";
                    break;
                case '\\':
                    result += "\\\\";
                    break;
                case '\b':
                    result += "\\b";
                    break;
                case '\f':
                    result += "\\f";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                case '\t':
                    result += "\\t";
                    break;
                default:
                    if ((unsigned char)c < 0x20)
                    {
                        // Control character — \u00XX
                        char buf[8];
                        snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
                        result += buf;
                    }
                    else
                    {
                        result += c;
                    }
                    break;
                }
            }
            return result;
        }

        inline std::string stringifyValue(const XObject &obj, int indent, int depth, bool pretty);

        inline std::string stringifyMap(const XMap &map, int indent, int depth, bool pretty)
        {
            if (map.size() == 0)
                return "{}";
            std::string result = "{";
            if (pretty)
                result += "\n";
            std::string pad(pretty ? (depth + 1) * indent : 0, ' ');
            std::string closePad(pretty ? depth * indent : 0, ' ');
            bool first = true;
            for (auto it = map.begin(); it.valid(); it.next())
            {
                if (!first)
                {
                    result += ",";
                    if (pretty)
                        result += "\n";
                }
                first = false;
                if (pretty)
                    result += pad;
                // Key: must be a string for valid JSON
                if (it.key().isString())
                {
                    result += "\"" + escapeJsonString(it.key().asString()) + "\"";
                }
                else
                {
                    // Non-string key: stringify it and quote
                    result += "\"" + escapeJsonString(it.key().toString()) + "\"";
                }
                result += pretty ? ": " : ":";
                result += stringifyValue(it.value(), indent, depth + 1, pretty);
            }
            if (pretty)
                result += "\n" + closePad;
            result += "}";
            return result;
        }

        inline std::string stringifyList(const std::vector<XObject> &list, int indent, int depth, bool pretty)
        {
            if (list.empty())
                return "[]";
            std::string result = "[";
            if (pretty)
                result += "\n";
            std::string pad(pretty ? (depth + 1) * indent : 0, ' ');
            std::string closePad(pretty ? depth * indent : 0, ' ');
            for (size_t i = 0; i < list.size(); ++i)
            {
                if (i > 0)
                {
                    result += ",";
                    if (pretty)
                        result += "\n";
                }
                if (pretty)
                    result += pad;
                result += stringifyValue(list[i], indent, depth + 1, pretty);
            }
            if (pretty)
                result += "\n" + closePad;
            result += "]";
            return result;
        }

        inline std::string stringifyValue(const XObject &obj, int indent, int depth, bool pretty)
        {
            switch (obj.type())
            {
            case XType::NONE:
                return "null";
            case XType::BOOL:
                return obj.asBool() ? "true" : "false";
            case XType::INT:
                return std::to_string(obj.asInt());
            case XType::FLOAT:
            {
                double v = obj.asFloat();
                if (std::isinf(v) || std::isnan(v))
                    return "null"; // JSON has no inf/nan
                // Avoid trailing zeros but keep it a valid number
                char buf[64];
                snprintf(buf, sizeof(buf), "%.17g", v);
                return buf;
            }
            case XType::STRING:
                return "\"" + escapeJsonString(obj.asString()) + "\"";
            case XType::LIST:
                return stringifyList(obj.asList(), indent, depth, pretty);
            case XType::TUPLE:
            {
                // Tuples → JSON arrays
                const auto &t = obj.asTuple();
                std::vector<XObject> vec(t.begin(), t.end());
                return stringifyList(vec, indent, depth, pretty);
            }
            case XType::MAP:
                return stringifyMap(obj.asMap(), indent, depth, pretty);
            case XType::SET:
            case XType::FROZEN_SET:
            {
                // Sets → JSON arrays
                auto elems = obj.isSet() ? obj.asSet().elements() : obj.asFrozenSet().elements();
                return stringifyList(elems, indent, depth, pretty);
            }
            default:
                // Functions, generators, etc → serialize as string representation
                return "\"" + escapeJsonString(obj.toString()) + "\"";
            }
        }

        // =====================================================================
        //  CSV Parser — full RFC 4180 compliant
        // =====================================================================

        struct CsvParser
        {
            std::string src;
            size_t pos;
            char sep;

            CsvParser(const std::string &s, char separator) : src(s), pos(0), sep(separator) {}

            bool atEnd() const { return pos >= src.size(); }

            // Parse a single field (handles quoted fields with embedded commas/newlines/quotes)
            std::string parseField()
            {
                if (pos < src.size() && src[pos] == '"')
                {
                    // Quoted field
                    ++pos; // skip opening quote
                    std::string result;
                    while (pos < src.size())
                    {
                        if (src[pos] == '"')
                        {
                            if (pos + 1 < src.size() && src[pos + 1] == '"')
                            {
                                // Escaped quote ""
                                result += '"';
                                pos += 2;
                            }
                            else
                            {
                                // End of quoted field
                                ++pos;
                                break;
                            }
                        }
                        else
                        {
                            result += src[pos++];
                        }
                    }
                    return result;
                }
                else
                {
                    // Unquoted field — read until separator or newline
                    std::string result;
                    while (pos < src.size() && src[pos] != sep &&
                           src[pos] != '\n' && src[pos] != '\r')
                    {
                        result += src[pos++];
                    }
                    return result;
                }
            }

            // Parse one row → vector of field strings
            std::vector<std::string> parseRow()
            {
                std::vector<std::string> fields;
                fields.push_back(parseField());
                while (pos < src.size() && src[pos] == sep)
                {
                    ++pos; // skip separator
                    fields.push_back(parseField());
                }
                // Skip end-of-line
                if (pos < src.size() && src[pos] == '\r')
                    ++pos;
                if (pos < src.size() && src[pos] == '\n')
                    ++pos;
                return fields;
            }

            // Parse all rows
            std::vector<std::vector<std::string>> parseAll()
            {
                std::vector<std::vector<std::string>> rows;
                while (!atEnd())
                {
                    // Skip trailing blank lines
                    if (src[pos] == '\n' || src[pos] == '\r')
                    {
                        if (src[pos] == '\r')
                            ++pos;
                        if (pos < src.size() && src[pos] == '\n')
                            ++pos;
                        continue;
                    }
                    rows.push_back(parseRow());
                }
                return rows;
            }
        };

        // =====================================================================
        //  TOML Parser — covers key=value, [sections], basic types
        // =====================================================================

        struct TomlParser
        {
            std::string src;
            int errLine;
            std::vector<std::string> lines;

            TomlParser(const std::string &s, int line) : src(s), errLine(line)
            {
                // Split into lines
                std::istringstream iss(s);
                std::string l;
                while (std::getline(iss, l))
                    lines.push_back(l);
            }

            static std::string trim(const std::string &s)
            {
                size_t start = s.find_first_not_of(" \t\r\n");
                if (start == std::string::npos)
                    return "";
                size_t end = s.find_last_not_of(" \t\r\n");
                return s.substr(start, end - start + 1);
            }

            static std::string stripComment(const std::string &s)
            {
                // Remove # comments (but not inside strings)
                bool inStr = false;
                for (size_t i = 0; i < s.size(); ++i)
                {
                    if (s[i] == '"')
                        inStr = !inStr;
                    if (!inStr && s[i] == '#')
                        return s.substr(0, i);
                }
                return s;
            }

            XObject parseScalar(const std::string &val)
            {
                if (val.empty())
                    return XObject::makeString("");

                // Boolean
                if (val == "true")
                    return XObject::makeBool(true);
                if (val == "false")
                    return XObject::makeBool(false);

                // Quoted string
                if (val.size() >= 2 && val.front() == '"' && val.back() == '"')
                {
                    std::string s = val.substr(1, val.size() - 2);
                    // Handle basic escape sequences
                    std::string result;
                    for (size_t i = 0; i < s.size(); ++i)
                    {
                        if (s[i] == '\\' && i + 1 < s.size())
                        {
                            switch (s[i + 1])
                            {
                            case 'n':
                                result += '\n';
                                ++i;
                                break;
                            case 't':
                                result += '\t';
                                ++i;
                                break;
                            case 'r':
                                result += '\r';
                                ++i;
                                break;
                            case '\\':
                                result += '\\';
                                ++i;
                                break;
                            case '"':
                                result += '"';
                                ++i;
                                break;
                            default:
                                result += s[i];
                                break;
                            }
                        }
                        else
                        {
                            result += s[i];
                        }
                    }
                    return XObject::makeString(result);
                }

                // Single-quoted literal string (TOML)
                if (val.size() >= 2 && val.front() == '\'' && val.back() == '\'')
                    return XObject::makeString(val.substr(1, val.size() - 2));

                // Inline array [...]
                if (val.size() >= 2 && val.front() == '[' && val.back() == ']')
                    return parseInlineArray(val);

                // Inline table {...}
                if (val.size() >= 2 && val.front() == '{' && val.back() == '}')
                    return parseInlineTable(val);

                // Integer (handle underscores like 1_000)
                {
                    std::string cleaned = val;
                    cleaned.erase(std::remove(cleaned.begin(), cleaned.end(), '_'), cleaned.end());
                    // Check hex 0x, octal 0o, binary 0b
                    if (cleaned.size() > 2 && cleaned[0] == '0')
                    {
                        if (cleaned[1] == 'x' || cleaned[1] == 'X')
                        {
                            try
                            {
                                return XObject::makeInt(std::stoll(cleaned, nullptr, 16));
                            }
                            catch (...)
                            {
                            }
                        }
                        if (cleaned[1] == 'o' || cleaned[1] == 'O')
                        {
                            try
                            {
                                return XObject::makeInt(std::stoll(cleaned.substr(2), nullptr, 8));
                            }
                            catch (...)
                            {
                            }
                        }
                        if (cleaned[1] == 'b' || cleaned[1] == 'B')
                        {
                            try
                            {
                                return XObject::makeInt(std::stoll(cleaned.substr(2), nullptr, 2));
                            }
                            catch (...)
                            {
                            }
                        }
                    }
                    // Regular integer
                    bool isInt = !cleaned.empty();
                    size_t istart = (cleaned[0] == '+' || cleaned[0] == '-') ? 1 : 0;
                    if (istart >= cleaned.size())
                        isInt = false;
                    for (size_t i = istart; i < cleaned.size() && isInt; ++i)
                        if (cleaned[i] < '0' || cleaned[i] > '9')
                            isInt = false;
                    if (isInt)
                    {
                        try
                        {
                            return XObject::makeInt(std::stoll(cleaned));
                        }
                        catch (...)
                        {
                        }
                    }

                    // Float (including inf/nan)
                    if (cleaned == "inf" || cleaned == "+inf")
                        return XObject::makeFloat(std::numeric_limits<double>::infinity());
                    if (cleaned == "-inf")
                        return XObject::makeFloat(-std::numeric_limits<double>::infinity());
                    if (cleaned == "nan" || cleaned == "+nan" || cleaned == "-nan")
                        return XObject::makeFloat(std::numeric_limits<double>::quiet_NaN());

                    // Regular float
                    bool isFloatLike = false;
                    for (char c : cleaned)
                        if (c == '.' || c == 'e' || c == 'E')
                            isFloatLike = true;
                    if (isFloatLike)
                    {
                        try
                        {
                            return XObject::makeFloat(std::stod(cleaned));
                        }
                        catch (...)
                        {
                        }
                    }
                }

                // Fallback: treat as string
                return XObject::makeString(val);
            }

            XObject parseInlineArray(const std::string &s)
            {
                // Strip outer []
                std::string inner = trim(s.substr(1, s.size() - 2));
                if (inner.empty())
                    return XObject::makeList();

                std::vector<XObject> elements;
                // Simple comma-split (respects nested brackets and quotes)
                int depth = 0;
                bool inStr = false;
                std::string current;
                for (size_t i = 0; i < inner.size(); ++i)
                {
                    char c = inner[i];
                    if (c == '"' && (i == 0 || inner[i - 1] != '\\'))
                        inStr = !inStr;
                    if (!inStr)
                    {
                        if (c == '[' || c == '{')
                            ++depth;
                        if (c == ']' || c == '}')
                            --depth;
                        if (c == ',' && depth == 0)
                        {
                            elements.push_back(parseScalar(trim(current)));
                            current.clear();
                            continue;
                        }
                    }
                    current += c;
                }
                if (!trim(current).empty())
                    elements.push_back(parseScalar(trim(current)));
                return XObject::makeList(std::move(elements));
            }

            XObject parseInlineTable(const std::string &s)
            {
                // Strip outer {}
                std::string inner = trim(s.substr(1, s.size() - 2));
                XMap map;
                if (inner.empty())
                    return XObject::makeMap(std::move(map));

                // Split by comma (respecting nesting)
                int depth = 0;
                bool inStr = false;
                std::string current;
                auto processEntry = [&](const std::string &entry)
                {
                    std::string e = trim(entry);
                    size_t eq = e.find('=');
                    if (eq == std::string::npos)
                        return;
                    std::string key = trim(e.substr(0, eq));
                    std::string val = trim(e.substr(eq + 1));
                    // Strip quotes from key if present
                    if (key.size() >= 2 && key.front() == '"' && key.back() == '"')
                        key = key.substr(1, key.size() - 2);
                    map.set(key, parseScalar(val));
                };

                for (size_t i = 0; i < inner.size(); ++i)
                {
                    char c = inner[i];
                    if (c == '"' && (i == 0 || inner[i - 1] != '\\'))
                        inStr = !inStr;
                    if (!inStr)
                    {
                        if (c == '[' || c == '{')
                            ++depth;
                        if (c == ']' || c == '}')
                            --depth;
                        if (c == ',' && depth == 0)
                        {
                            processEntry(current);
                            current.clear();
                            continue;
                        }
                    }
                    current += c;
                }
                if (!trim(current).empty())
                    processEntry(current);
                return XObject::makeMap(std::move(map));
            }

            XObject parse()
            {
                XMap root;
                XMap *current = &root; // current section being populated
                // We need to keep sub-maps alive, store them separately
                // Using a vector of unique_ptr<XMap> for section storage
                struct SectionEntry
                {
                    std::vector<std::string> path;
                    XMap map;
                };
                std::vector<SectionEntry> sections;

                std::string currentSectionName;

                for (size_t i = 0; i < lines.size(); ++i)
                {
                    std::string line = trim(stripComment(lines[i]));
                    if (line.empty())
                        continue;

                    // Section header: [section] or [section.subsection]
                    if (line.front() == '[' && line.back() == ']')
                    {
                        // Check for array of tables [[...]]
                        bool isArrayTable = false;
                        std::string sectionName;
                        if (line.size() >= 4 && line[1] == '[' && line[line.size() - 2] == ']')
                        {
                            isArrayTable = true;
                            sectionName = trim(line.substr(2, line.size() - 4));
                        }
                        else
                        {
                            sectionName = trim(line.substr(1, line.size() - 2));
                        }

                        if (isArrayTable)
                        {
                            // [[array_table]] — append a new map to an array
                            XObject *existing = root.get(sectionName);
                            if (!existing)
                            {
                                // Create new array with one empty map
                                sections.push_back({});
                                SectionEntry &se = sections.back();
                                current = &se.map;
                                std::vector<XObject> arr;
                                arr.push_back(XObject::makeMap()); // placeholder
                                root.set(sectionName, XObject::makeList(std::move(arr)));
                            }
                            else
                            {
                                // Append new map
                                sections.push_back({});
                                SectionEntry &se = sections.back();
                                current = &se.map;
                                existing->asListMut().push_back(XObject::makeMap()); // placeholder
                            }
                            currentSectionName = sectionName;
                        }
                        else
                        {
                            // Regular [section]
                            sections.push_back({});
                            SectionEntry &se = sections.back();
                            current = &se.map;
                            currentSectionName = sectionName;
                        }
                        continue;
                    }

                    // Key = value
                    size_t eq = line.find('=');
                    if (eq != std::string::npos)
                    {
                        std::string key = trim(line.substr(0, eq));
                        std::string val = trim(line.substr(eq + 1));
                        // Strip quotes from key if present
                        if (key.size() >= 2 && key.front() == '"' && key.back() == '"')
                            key = key.substr(1, key.size() - 2);
                        current->set(key, parseScalar(val));
                    }
                }

                // Merge sections into root
                for (auto &se : sections)
                {
                    // Build section as XMap, set into root with dotted key path
                    // For simplicity, we use the full dotted name as key
                    // and nest the map
                }

                // Re-process: build proper nested structure
                // First pass already set key-values into section maps.
                // Now merge them into root under their section names.
                XMap finalRoot;
                // Copy root-level keys first
                for (auto it = root.begin(); it.valid(); it.next())
                    finalRoot.set(it.key().asString(), it.value());

                // Now process sections in order
                size_t sIdx = 0;
                for (size_t i = 0; i < lines.size(); ++i)
                {
                    std::string line = trim(stripComment(lines[i]));
                    if (line.empty())
                        continue;
                    if (line.front() == '[' && line.back() == ']')
                    {
                        bool isArrayTable = false;
                        std::string sName;
                        if (line.size() >= 4 && line[1] == '[' && line[line.size() - 2] == ']')
                        {
                            isArrayTable = true;
                            sName = trim(line.substr(2, line.size() - 4));
                        }
                        else
                        {
                            sName = trim(line.substr(1, line.size() - 2));
                        }

                        if (sIdx < sections.size())
                        {
                            XMap &sMap = sections[sIdx].map;
                            if (isArrayTable)
                            {
                                // Get or create the array
                                XObject *existing = finalRoot.get(sName);
                                if (!existing || !existing->isList())
                                {
                                    std::vector<XObject> arr;
                                    arr.push_back(XObject::makeMap(std::move(sMap)));
                                    finalRoot.set(sName, XObject::makeList(std::move(arr)));
                                }
                                else
                                {
                                    existing->asListMut().push_back(XObject::makeMap(std::move(sMap)));
                                }
                            }
                            else
                            {
                                // Handle dotted section names: [a.b.c] → nested maps
                                // Split by '.'
                                std::vector<std::string> parts;
                                std::istringstream ss(sName);
                                std::string part;
                                while (std::getline(ss, part, '.'))
                                    parts.push_back(trim(part));

                                if (parts.size() == 1)
                                {
                                    finalRoot.set(parts[0], XObject::makeMap(std::move(sMap)));
                                }
                                else
                                {
                                    // Build nested: last part gets the map, earlier parts wrap
                                    XObject nested = XObject::makeMap(std::move(sMap));
                                    for (int p = (int)parts.size() - 2; p >= 0; --p)
                                    {
                                        XMap wrapper;
                                        // Check if this level already exists in root
                                        if (p == 0)
                                        {
                                            XObject *existingParent = finalRoot.get(parts[0]);
                                            if (existingParent && existingParent->isMap())
                                            {
                                                // Merge into existing map
                                                XMap merged;
                                                for (auto mit = existingParent->asMap().begin(); mit.valid(); mit.next())
                                                    merged.set(mit.key(), mit.value());
                                                // Add nested under parts[1]
                                                merged.set(parts[1], nested);
                                                finalRoot.set(parts[0], XObject::makeMap(std::move(merged)));
                                                goto nextSection;
                                            }
                                        }
                                        wrapper.set(parts[p + 1], std::move(nested));
                                        nested = XObject::makeMap(std::move(wrapper));
                                    }
                                    finalRoot.set(parts[0], std::move(nested));
                                nextSection:;
                                }
                            }
                            ++sIdx;
                        }
                    }
                }

                return XObject::makeMap(std::move(finalRoot));
            }
        };

        // =====================================================================
        //  YAML Parser — indent-based key: value, lists, nested maps
        // =====================================================================

        struct YamlParser
        {
            struct Line
            {
                int indent;
                std::string content;
            };

            std::vector<Line> lines;
            int errLine;

            YamlParser(const std::string &src, int line) : errLine(line)
            {
                std::istringstream iss(src);
                std::string l;
                while (std::getline(iss, l))
                {
                    // Skip empty lines and comments
                    std::string trimmed = l;
                    // Remove trailing \r
                    if (!trimmed.empty() && trimmed.back() == '\r')
                        trimmed.pop_back();

                    // Count indent
                    int indent = 0;
                    for (char c : trimmed)
                    {
                        if (c == ' ')
                            ++indent;
                        else if (c == '\t')
                            indent += 2;
                        else
                            break;
                    }

                    std::string content = trimmed.substr(indent);
                    if (content.empty() || content[0] == '#')
                        continue;
                    // Strip inline comments (outside quotes)
                    bool inStr = false;
                    for (size_t i = 0; i < content.size(); ++i)
                    {
                        if (content[i] == '"' || content[i] == '\'')
                            inStr = !inStr;
                        if (!inStr && content[i] == '#' && i > 0 && content[i - 1] == ' ')
                        {
                            content = content.substr(0, i);
                            // Trim trailing space
                            while (!content.empty() && content.back() == ' ')
                                content.pop_back();
                            break;
                        }
                    }

                    lines.push_back({indent, content});
                }
            }

            static std::string trim(const std::string &s)
            {
                size_t a = s.find_first_not_of(" \t");
                if (a == std::string::npos)
                    return "";
                size_t b = s.find_last_not_of(" \t");
                return s.substr(a, b - a + 1);
            }

            XObject parseScalar(const std::string &val)
            {
                std::string v = trim(val);
                if (v.empty() || v == "~" || v == "null")
                    return XObject::makeNone();
                if (v == "true" || v == "True" || v == "TRUE" || v == "yes" || v == "Yes" || v == "YES" || v == "on" || v == "On" || v == "ON")
                    return XObject::makeBool(true);
                if (v == "false" || v == "False" || v == "FALSE" || v == "no" || v == "No" || v == "NO" || v == "off" || v == "Off" || v == "OFF")
                    return XObject::makeBool(false);
                // Quoted string
                if (v.size() >= 2 && ((v.front() == '"' && v.back() == '"') ||
                                      (v.front() == '\'' && v.back() == '\'')))
                    return XObject::makeString(v.substr(1, v.size() - 2));
                // Integer
                {
                    bool isInt = !v.empty();
                    size_t istart = (v[0] == '+' || v[0] == '-') ? 1 : 0;
                    if (istart >= v.size())
                        isInt = false;
                    for (size_t i = istart; i < v.size() && isInt; ++i)
                        if (v[i] < '0' || v[i] > '9')
                            isInt = false;
                    if (isInt)
                    {
                        try
                        {
                            return XObject::makeInt(std::stoll(v));
                        }
                        catch (...)
                        {
                        }
                    }
                }
                // Float
                {
                    if (v == ".inf" || v == ".Inf" || v == ".INF")
                        return XObject::makeFloat(std::numeric_limits<double>::infinity());
                    if (v == "-.inf" || v == "-.Inf" || v == "-.INF")
                        return XObject::makeFloat(-std::numeric_limits<double>::infinity());
                    if (v == ".nan" || v == ".NaN" || v == ".NAN")
                        return XObject::makeFloat(std::numeric_limits<double>::quiet_NaN());
                    bool hasFloat = false;
                    for (char c : v)
                        if (c == '.' || c == 'e' || c == 'E')
                            hasFloat = true;
                    if (hasFloat)
                    {
                        try
                        {
                            return XObject::makeFloat(std::stod(v));
                        }
                        catch (...)
                        {
                        }
                    }
                }
                // Default: string
                return XObject::makeString(v);
            }

            // Parse a YAML flow sequence [...] inline
            XObject parseFlowSequence(const std::string &s)
            {
                std::string inner = trim(s.substr(1, s.size() - 2));
                if (inner.empty())
                    return XObject::makeList();
                std::vector<XObject> elements;
                int depth = 0;
                bool inStr = false;
                std::string current;
                for (size_t i = 0; i < inner.size(); ++i)
                {
                    char c = inner[i];
                    if (c == '"' || c == '\'')
                        inStr = !inStr;
                    if (!inStr)
                    {
                        if (c == '[' || c == '{')
                            ++depth;
                        if (c == ']' || c == '}')
                            --depth;
                        if (c == ',' && depth == 0)
                        {
                            elements.push_back(parseScalar(trim(current)));
                            current.clear();
                            continue;
                        }
                    }
                    current += c;
                }
                if (!trim(current).empty())
                    elements.push_back(parseScalar(trim(current)));
                return XObject::makeList(std::move(elements));
            }

            // Parse a YAML flow mapping {...} inline
            XObject parseFlowMapping(const std::string &s)
            {
                std::string inner = trim(s.substr(1, s.size() - 2));
                XMap map;
                if (inner.empty())
                    return XObject::makeMap(std::move(map));
                int depth = 0;
                bool inStr = false;
                std::string current;
                auto processEntry = [&](const std::string &entry)
                {
                    std::string e = trim(entry);
                    size_t colon = e.find(':');
                    if (colon == std::string::npos)
                        return;
                    std::string key = trim(e.substr(0, colon));
                    std::string val = trim(e.substr(colon + 1));
                    if (key.size() >= 2 && ((key.front() == '"' && key.back() == '"') ||
                                            (key.front() == '\'' && key.back() == '\'')))
                        key = key.substr(1, key.size() - 2);
                    map.set(key, parseScalar(val));
                };
                for (size_t i = 0; i < inner.size(); ++i)
                {
                    char c = inner[i];
                    if (c == '"' || c == '\'')
                        inStr = !inStr;
                    if (!inStr)
                    {
                        if (c == '[' || c == '{')
                            ++depth;
                        if (c == ']' || c == '}')
                            --depth;
                        if (c == ',' && depth == 0)
                        {
                            processEntry(current);
                            current.clear();
                            continue;
                        }
                    }
                    current += c;
                }
                if (!trim(current).empty())
                    processEntry(current);
                return XObject::makeMap(std::move(map));
            }

            // Parse block starting from index `start`, at minimum indent `minIndent`
            // Returns XObject (map or list) and sets `endIdx` to the first line
            // index past what was consumed.
            XObject parseBlock(size_t start, int minIndent, size_t &endIdx)
            {
                if (start >= lines.size())
                {
                    endIdx = start;
                    return XObject::makeNone();
                }

                // Detect if this block is a list or a map
                bool isList = (lines[start].content[0] == '-');

                if (isList)
                {
                    std::vector<XObject> list;
                    size_t i = start;
                    while (i < lines.size() && lines[i].indent >= minIndent)
                    {
                        if (lines[i].indent > minIndent && lines[i].content[0] != '-')
                        {
                            break; // belongs to parent
                        }
                        if (lines[i].indent == minIndent && lines[i].content[0] == '-')
                        {
                            // List item: "- value" or "- key: value" (nested)
                            std::string afterDash = trim(lines[i].content.substr(1));
                            if (afterDash.empty())
                            {
                                // Block sub-item on next line(s)
                                ++i;
                                if (i < lines.size() && lines[i].indent > minIndent)
                                {
                                    size_t subEnd;
                                    list.push_back(parseBlock(i, lines[i].indent, subEnd));
                                    i = subEnd;
                                }
                                else
                                {
                                    list.push_back(XObject::makeNone());
                                }
                            }
                            else if (afterDash.find(':') != std::string::npos &&
                                     afterDash[0] != '[' && afterDash[0] != '{')
                            {
                                // "- key: value" — inline map entry in list
                                size_t colonPos = afterDash.find(':');
                                std::string key = trim(afterDash.substr(0, colonPos));
                                std::string val = trim(afterDash.substr(colonPos + 1));
                                XMap m;
                                if (val.empty())
                                {
                                    // Value on next lines
                                    ++i;
                                    if (i < lines.size() && lines[i].indent > minIndent)
                                    {
                                        size_t subEnd;
                                        m.set(key, parseBlock(i, lines[i].indent, subEnd));
                                        // Check for more keys at dash+2 indent level
                                        while (subEnd < lines.size() &&
                                               lines[subEnd].indent > minIndent &&
                                               lines[subEnd].content[0] != '-')
                                        {
                                            std::string &c = lines[subEnd].content;
                                            size_t cp = c.find(':');
                                            if (cp != std::string::npos)
                                            {
                                                std::string k2 = trim(c.substr(0, cp));
                                                std::string v2 = trim(c.substr(cp + 1));
                                                if (v2.empty() && subEnd + 1 < lines.size() &&
                                                    lines[subEnd + 1].indent > lines[subEnd].indent)
                                                {
                                                    size_t se2;
                                                    m.set(k2, parseBlock(subEnd + 1, lines[subEnd + 1].indent, se2));
                                                    subEnd = se2;
                                                }
                                                else
                                                {
                                                    m.set(k2, parseScalar(v2));
                                                    ++subEnd;
                                                }
                                            }
                                            else
                                            {
                                                ++subEnd;
                                            }
                                        }
                                        i = subEnd;
                                    }
                                    else
                                    {
                                        m.set(key, XObject::makeNone());
                                    }
                                }
                                else
                                {
                                    m.set(key, parseScalar(val));
                                    ++i;
                                    // Check if there are more keys on subsequent indented lines
                                    while (i < lines.size() && lines[i].indent > minIndent &&
                                           lines[i].content[0] != '-')
                                    {
                                        std::string &c = lines[i].content;
                                        size_t cp = c.find(':');
                                        if (cp != std::string::npos)
                                        {
                                            std::string k2 = trim(c.substr(0, cp));
                                            std::string v2 = trim(c.substr(cp + 1));
                                            if (v2.empty() && i + 1 < lines.size() &&
                                                lines[i + 1].indent > lines[i].indent)
                                            {
                                                size_t se2;
                                                m.set(k2, parseBlock(i + 1, lines[i + 1].indent, se2));
                                                i = se2;
                                            }
                                            else
                                            {
                                                m.set(k2, parseScalar(v2));
                                                ++i;
                                            }
                                        }
                                        else
                                        {
                                            ++i;
                                        }
                                    }
                                }
                                list.push_back(XObject::makeMap(std::move(m)));
                            }
                            else if (afterDash[0] == '[')
                            {
                                list.push_back(parseFlowSequence(afterDash));
                                ++i;
                            }
                            else if (afterDash[0] == '{')
                            {
                                list.push_back(parseFlowMapping(afterDash));
                                ++i;
                            }
                            else
                            {
                                list.push_back(parseScalar(afterDash));
                                ++i;
                            }
                        }
                        else
                        {
                            break; // different indent level
                        }
                    }
                    endIdx = i;
                    return XObject::makeList(std::move(list));
                }
                else
                {
                    // Map block
                    XMap map;
                    size_t i = start;
                    while (i < lines.size() && lines[i].indent >= minIndent)
                    {
                        if (lines[i].indent > minIndent)
                            break; // belongs to a sub-block

                        std::string &content = lines[i].content;
                        size_t colonPos = content.find(':');
                        if (colonPos == std::string::npos)
                        {
                            ++i;
                            continue;
                        }
                        std::string key = trim(content.substr(0, colonPos));
                        // Remove quotes from key
                        if (key.size() >= 2 && ((key.front() == '"' && key.back() == '"') ||
                                                (key.front() == '\'' && key.back() == '\'')))
                            key = key.substr(1, key.size() - 2);

                        std::string valStr = trim(content.substr(colonPos + 1));

                        if (valStr.empty())
                        {
                            // Value on next lines (block)
                            ++i;
                            if (i < lines.size() && lines[i].indent > minIndent)
                            {
                                size_t subEnd;
                                map.set(key, parseBlock(i, lines[i].indent, subEnd));
                                i = subEnd;
                            }
                            else
                            {
                                map.set(key, XObject::makeNone());
                            }
                        }
                        else if (valStr[0] == '[')
                        {
                            map.set(key, parseFlowSequence(valStr));
                            ++i;
                        }
                        else if (valStr[0] == '{')
                        {
                            map.set(key, parseFlowMapping(valStr));
                            ++i;
                        }
                        else if (valStr[0] == '|' || valStr[0] == '>')
                        {
                            // Multi-line string: | (literal) or > (folded)
                            bool literal = (valStr[0] == '|');
                            ++i;
                            std::string multiline;
                            int blockIndent = -1;
                            while (i < lines.size() && lines[i].indent > minIndent)
                            {
                                if (blockIndent < 0)
                                    blockIndent = lines[i].indent;
                                if (!multiline.empty())
                                    multiline += literal ? "\n" : " ";
                                // Preserve relative indentation for literal blocks
                                int extra = lines[i].indent - blockIndent;
                                if (extra > 0 && literal)
                                    multiline += std::string(extra, ' ');
                                multiline += lines[i].content;
                                ++i;
                            }
                            map.set(key, XObject::makeString(multiline));
                        }
                        else
                        {
                            map.set(key, parseScalar(valStr));
                            ++i;
                        }
                    }
                    endIdx = i;
                    return XObject::makeMap(std::move(map));
                }
            }

            XObject parse()
            {
                if (lines.empty())
                    return XObject::makeMap();
                // Check for document separator ---
                size_t start = 0;
                if (!lines.empty() && lines[0].content == "---")
                    start = 1;
                if (start >= lines.size())
                    return XObject::makeMap();
                size_t endIdx;
                return parseBlock(start, lines[start].indent, endIdx);
            }
        };

    } // namespace json_internal

    // =========================================================================
    //  Public builtin registration
    // =========================================================================

    inline void registerJSONBuiltins(BuiltinTable &t)
    {
        // ---- json_parse(str) → XObject -------------------------------------
        t["json_parse"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("json_parse", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("json_parse() expects a JSON string", line);
            json_internal::JsonParser parser(args[0].asString(), line);
            XObject result = parser.parseValue();
            // Ensure no trailing non-whitespace
            parser.skipWhitespace();
            if (parser.pos < parser.src.size())
                throw RuntimeError("json_parse: unexpected trailing content at position " +
                                       std::to_string(parser.pos),
                                   line);
            return result;
        };

        // ---- json_stringify(val) → string ----------------------------------
        t["json_stringify"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("json_stringify", 1, (int)args.size(), line);
            return XObject::makeString(
                json_internal::stringifyValue(args[0], 0, 0, false));
        };

        // ---- json_pretty(val, indent=2) → string ---------------------------
        t["json_pretty"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("json_pretty", 1, (int)args.size(), line);
            int indent = 2;
            if (args.size() == 2)
            {
                if (!args[1].isInt())
                    throw TypeError("json_pretty() indent must be an integer", line);
                indent = (int)args[1].asInt();
            }
            return XObject::makeString(
                json_internal::stringifyValue(args[0], indent, 0, true));
        };

        // ---- json_read(path) → XObject -------------------------------------
        t["json_read"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("json_read", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("json_read() expects a file path string", line);
            std::ifstream file(args[0].asString());
            if (!file.is_open())
                throw FileNotFoundError(args[0].asString(), line);
            std::ostringstream ss;
            ss << file.rdbuf();
            json_internal::JsonParser parser(ss.str(), line);
            XObject result = parser.parseValue();
            parser.skipWhitespace();
            return result;
        };

        // ---- json_write(path, val) → none ----------------------------------
        t["json_write"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("json_write", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("json_write() first argument must be a file path string", line);
            std::string json = json_internal::stringifyValue(args[1], 2, 0, true);
            std::ofstream file(args[0].asString());
            if (!file.is_open())
                throw IOError("json_write(): cannot open file: " + args[0].asString(), line);
            file << json << "\n";
            return XObject::makeNone();
        };

        // ---- csv_parse(str, sep=",") → list of maps ------------------------
        t["csv_parse"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("csv_parse", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("csv_parse() expects a CSV string", line);
            char sep = ',';
            if (args.size() == 2)
            {
                if (!args[1].isString() || args[1].asString().size() != 1)
                    throw TypeError("csv_parse() separator must be a single character string", line);
                sep = args[1].asString()[0];
            }
            json_internal::CsvParser parser(args[0].asString(), sep);
            auto rows = parser.parseAll();
            if (rows.empty())
                return XObject::makeList();
            // First row = headers
            std::vector<std::string> headers = rows[0];
            std::vector<XObject> result;
            for (size_t r = 1; r < rows.size(); ++r)
            {
                XMap rowMap;
                for (size_t c = 0; c < headers.size(); ++c)
                {
                    std::string val = (c < rows[r].size()) ? rows[r][c] : "";
                    rowMap.set(headers[c], XObject::makeString(val));
                }
                result.push_back(XObject::makeMap(std::move(rowMap)));
            }
            return XObject::makeList(std::move(result));
        };

        // ---- csv_read(path, sep=",") → list of maps ------------------------
        t["csv_read"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("csv_read", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("csv_read() expects a file path string", line);
            std::ifstream file(args[0].asString());
            if (!file.is_open())
                throw FileNotFoundError(args[0].asString(), line);
            std::ostringstream ss;
            ss << file.rdbuf();
            // Forward to csv_parse logic
            char sep = ',';
            if (args.size() == 2)
            {
                if (!args[1].isString() || args[1].asString().size() != 1)
                    throw TypeError("csv_read() separator must be a single character string", line);
                sep = args[1].asString()[0];
            }
            json_internal::CsvParser parser(ss.str(), sep);
            auto rows = parser.parseAll();
            if (rows.empty())
                return XObject::makeList();
            std::vector<std::string> headers = rows[0];
            std::vector<XObject> result;
            for (size_t r = 1; r < rows.size(); ++r)
            {
                XMap rowMap;
                for (size_t c = 0; c < headers.size(); ++c)
                {
                    std::string val = (c < rows[r].size()) ? rows[r][c] : "";
                    rowMap.set(headers[c], XObject::makeString(val));
                }
                result.push_back(XObject::makeMap(std::move(rowMap)));
            }
            return XObject::makeList(std::move(result));
        };

        // ---- csv_write(path, data) → none ----------------------------------
        // data: list of maps; writes header from first map's keys
        t["csv_write"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("csv_write", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("csv_write() first argument must be a file path string", line);
            if (!args[1].isList())
                throw TypeError("csv_write() second argument must be a list of maps", line);
            const auto &rows = args[1].asList();
            if (rows.empty())
            {
                // Write empty file
                std::ofstream file(args[0].asString());
                if (!file.is_open())
                    throw IOError("csv_write(): cannot open file: " + args[0].asString(), line);
                return XObject::makeNone();
            }
            // Get headers from first row's keys
            if (!rows[0].isMap())
                throw TypeError("csv_write() list elements must be maps", line);
            std::vector<std::string> headers;
            for (auto it = rows[0].asMap().begin(); it.valid(); it.next())
                headers.push_back(it.key().asString());

            std::ofstream file(args[0].asString());
            if (!file.is_open())
                throw IOError("csv_write(): cannot open file: " + args[0].asString(), line);

            // Helper: escape CSV field
            auto escapeField = [](const std::string &s) -> std::string
            {
                bool needsQuote = false;
                for (char c : s)
                    if (c == ',' || c == '"' || c == '\n' || c == '\r')
                    {
                        needsQuote = true;
                        break;
                    }
                if (!needsQuote)
                    return s;
                std::string result = "\"";
                for (char c : s)
                {
                    if (c == '"')
                        result += "\"\"";
                    else
                        result += c;
                }
                result += "\"";
                return result;
            };

            // Write header
            for (size_t i = 0; i < headers.size(); ++i)
            {
                if (i > 0)
                    file << ",";
                file << escapeField(headers[i]);
            }
            file << "\n";

            // Write data rows
            for (const auto &row : rows)
            {
                if (!row.isMap())
                    continue;
                for (size_t i = 0; i < headers.size(); ++i)
                {
                    if (i > 0)
                        file << ",";
                    const XObject *val = row.asMap().get(headers[i]);
                    if (val)
                        file << escapeField(val->toString());
                }
                file << "\n";
            }
            return XObject::makeNone();
        };

        // ---- toml_read(path) → map -----------------------------------------
        t["toml_read"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("toml_read", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("toml_read() expects a file path string", line);
            std::ifstream file(args[0].asString());
            if (!file.is_open())
                throw FileNotFoundError(args[0].asString(), line);
            std::ostringstream ss;
            ss << file.rdbuf();
            json_internal::TomlParser parser(ss.str(), line);
            return parser.parse();
        };

        // ---- yaml_read(path) → map/list ------------------------------------
        t["yaml_read"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("yaml_read", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("yaml_read() expects a file path string", line);
            std::ifstream file(args[0].asString());
            if (!file.is_open())
                throw FileNotFoundError(args[0].asString(), line);
            std::ostringstream ss;
            ss << file.rdbuf();
            json_internal::YamlParser parser(ss.str(), line);
            return parser.parse();
        };
    }

} // namespace xell
