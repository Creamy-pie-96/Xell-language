#pragma once

// =============================================================================
// Casting builtins — Int, Float, String, Complex, Bool, List, Tuple, Set,
//                    iSet, ~List, ~Tuple, ~Set, ~iSet, number, auto_cast
// =============================================================================
//
// Provides type-casting functions for Xell. Regular versions convert values
// to the target container type (elements stay as strings when coming from a
// string). Smart-cast (~) versions auto-detect the best type for each element.
//
// String splitting for container conversion uses comma ',' and '.' as separators.
// =============================================================================

#include "builtin_registry.hpp"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace xell
{

    // ========================================================================
    // Helpers
    // ========================================================================

    /// Trim leading/trailing whitespace from a string
    static inline std::string trimStr(const std::string &s)
    {
        size_t start = s.find_first_not_of(" \t\n\r");
        if (start == std::string::npos)
            return "";
        size_t end = s.find_last_not_of(" \t\n\r");
        return s.substr(start, end - start + 1);
    }

    /// Split a string by comma and fullstop separators
    static inline std::vector<std::string> splitForContainer(const std::string &s)
    {
        std::vector<std::string> result;
        std::string current;
        int bracketDepth = 0; // track [] {} () <> nesting

        for (size_t i = 0; i < s.size(); i++)
        {
            char c = s[i];
            if (c == '[' || c == '(' || c == '{' || c == '<')
                bracketDepth++;
            else if (c == ']' || c == ')' || c == '}' || c == '>')
                bracketDepth--;

            // Only split on comma at top level (not inside nested structures)
            if ((c == ',') && bracketDepth == 0)
            {
                std::string trimmed = trimStr(current);
                if (!trimmed.empty())
                    result.push_back(trimmed);
                current.clear();
            }
            else
            {
                current += c;
            }
        }
        std::string trimmed = trimStr(current);
        if (!trimmed.empty())
            result.push_back(trimmed);
        return result;
    }

    /// Parse an integer string with support for decimal, 0x/0X, 0o/0O, 0b/0B.
    static inline bool parseIntegerString(const std::string &raw, int64_t &out)
    {
        std::string s = trimStr(raw);
        if (s.empty())
            return false;

        bool hasSign = (s[0] == '+' || s[0] == '-');
        size_t digitsStart = hasSign ? 1 : 0;
        if (digitsStart >= s.size())
            return false;

        int base = 10;
        if (digitsStart + 1 < s.size() && s[digitsStart] == '0')
        {
            char p = s[digitsStart + 1];
            if (p == 'x' || p == 'X')
            {
                base = 16;
                s.erase(digitsStart, 2);
            }
            else if (p == 'o' || p == 'O')
            {
                base = 8;
                s.erase(digitsStart, 2);
            }
            else if (p == 'b' || p == 'B')
            {
                base = 2;
                s.erase(digitsStart, 2);
            }
        }

        if (hasSign && s.size() == 1)
            return false;
        if (!hasSign && s.empty())
            return false;

        try
        {
            size_t pos = 0;
            long long v = std::stoll(s, &pos, base);
            if (pos != s.size())
                return false;
            out = static_cast<int64_t>(v);
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    /// Parse a real number string (int or float) into double.
    static inline bool parseRealNumberString(const std::string &raw, double &out)
    {
        int64_t i;
        if (parseIntegerString(raw, i))
        {
            out = static_cast<double>(i);
            return true;
        }

        std::string s = trimStr(raw);
        if (s.empty())
            return false;
        try
        {
            size_t pos = 0;
            double d = std::stod(s, &pos);
            if (pos != s.size())
                return false;
            out = d;
            return true;
        }
        catch (...)
        {
            return false;
        }
    }

    /// Parse complex forms like "3i", "2+3i", "2-3i", "10.11+2i".
    static inline bool parseComplexString(const std::string &raw, XComplex &out)
    {
        std::string s = trimStr(raw);
        if (s.size() < 2)
            return false;
        char last = s.back();
        if (last != 'i' && last != 'I')
            return false;

        std::string body = s.substr(0, s.size() - 1);
        if (body.empty())
            return false;

        // Pure imaginary: "3i", "-2.5i", "+i", "-i"
        {
            std::string imagPart = body;
            if (imagPart == "+")
                imagPart = "1";
            else if (imagPart == "-")
                imagPart = "-1";

            double imagOnly = 0.0;
            if (parseRealNumberString(imagPart, imagOnly))
            {
                out = XComplex{0.0, imagOnly};
                return true;
            }
        }

        // Real + imaginary: find split at last +/- not part of exponent.
        size_t splitAt = std::string::npos;
        for (size_t i = 1; i < body.size(); ++i)
        {
            char c = body[i];
            if ((c == '+' || c == '-') && body[i - 1] != 'e' && body[i - 1] != 'E')
                splitAt = i;
        }
        if (splitAt == std::string::npos)
            return false;

        std::string realPart = body.substr(0, splitAt);
        std::string imagPart = body.substr(splitAt);
        if (imagPart == "+")
            imagPart = "1";
        else if (imagPart == "-")
            imagPart = "-1";

        double real = 0.0;
        double imag = 0.0;
        if (!parseRealNumberString(realPart, real) || !parseRealNumberString(imagPart, imag))
            return false;

        out = XComplex{real, imag};
        return true;
    }

    /// Parse number/complex string. Returns int, float, or complex.
    static inline bool parseNumericValue(const std::string &raw, XObject &out)
    {
        XComplex c;
        if (parseComplexString(raw, c))
        {
            out = XObject::makeComplex(c);
            return true;
        }

        int64_t i;
        if (parseIntegerString(raw, i))
        {
            out = XObject::makeInt(i);
            return true;
        }

        double d;
        if (parseRealNumberString(raw, d))
        {
            out = XObject::makeFloat(d);
            return true;
        }

        return false;
    }

    /// Auto-cast a string to the best XObject type
    static inline XObject autocastValue(const std::string &s, int line)
    {
        std::string value = trimStr(s);

        // Boolean
        if (value == "true")
            return XObject::makeBool(true);
        if (value == "false")
            return XObject::makeBool(false);
        if (value == "none")
            return XObject::makeNone();

        // Try int/float/complex
        if (!value.empty() && (value[0] == '-' || value[0] == '+' || (value[0] >= '0' && value[0] <= '9')))
        {
            XObject numeric;
            if (parseNumericValue(value, numeric))
            {
                return numeric;
            }
        }

        // Try nested container detection: [1,2,3] → list
        if (value.size() >= 2 && value.front() == '[' && value.back() == ']')
        {
            std::string inner = value.substr(1, value.size() - 2);
            auto parts = splitForContainer(inner);
            XList list;
            for (const auto &part : parts)
                list.push_back(autocastValue(part, line));
            return XObject::makeList(std::move(list));
        }

        // Try tuple: (1,2,3)
        if (value.size() >= 2 && value.front() == '(' && value.back() == ')')
        {
            std::string inner = value.substr(1, value.size() - 2);
            auto parts = splitForContainer(inner);
            XTuple tup;
            for (const auto &part : parts)
                tup.push_back(autocastValue(part, line));
            return XObject::makeTuple(std::move(tup));
        }

        // Try set: {1,2,3}
        if (value.size() >= 2 && value.front() == '{' && value.back() == '}')
        {
            std::string inner = value.substr(1, value.size() - 2);
            auto parts = splitForContainer(inner);
            XSet set;
            for (const auto &part : parts)
            {
                XObject val = autocastValue(part, line);
                if (isHashable(val))
                    set.add(val);
            }
            return XObject::makeSet(std::move(set));
        }

        // Default: keep as string
        return XObject::makeString(value);
    }

    /// Convert an XObject to a list of elements (for container conversions)
    static inline std::vector<XObject> toElementList(const XObject &obj, int line)
    {
        if (obj.isList())
            return obj.asList();
        if (obj.isTuple())
        {
            auto &tup = obj.asTuple();
            return std::vector<XObject>(tup.begin(), tup.end());
        }
        if (obj.isSet())
            return obj.asSet().elements();
        if (obj.isFrozenSet())
            return obj.asFrozenSet().elements();
        if (obj.isString())
        {
            auto parts = splitForContainer(obj.asString());
            std::vector<XObject> result;
            for (const auto &p : parts)
                result.push_back(XObject::makeString(p));
            return result;
        }
        // Single value → wrap in a single-element vector
        return {obj};
    }

    /// Smart-cast version: convert elements to best types
    static inline std::vector<XObject> toElementListSmart(const XObject &obj, int line)
    {
        if (obj.isString())
        {
            auto parts = splitForContainer(obj.asString());
            std::vector<XObject> result;
            for (const auto &p : parts)
                result.push_back(autocastValue(p, line));
            return result;
        }
        if (obj.isList())
        {
            std::vector<XObject> result;
            for (const auto &elem : obj.asList())
            {
                if (elem.isString())
                    result.push_back(autocastValue(elem.asString(), line));
                else
                    result.push_back(elem);
            }
            return result;
        }
        if (obj.isTuple())
        {
            std::vector<XObject> result;
            for (const auto &elem : obj.asTuple())
            {
                if (elem.isString())
                    result.push_back(autocastValue(elem.asString(), line));
                else
                    result.push_back(elem);
            }
            return result;
        }
        if (obj.isSet())
        {
            std::vector<XObject> result;
            for (const auto &elem : obj.asSet().elements())
            {
                if (elem.isString())
                    result.push_back(autocastValue(elem.asString(), line));
                else
                    result.push_back(elem);
            }
            return result;
        }
        if (obj.isFrozenSet())
        {
            std::vector<XObject> result;
            for (const auto &elem : obj.asFrozenSet().elements())
            {
                if (elem.isString())
                    result.push_back(autocastValue(elem.asString(), line));
                else
                    result.push_back(elem);
            }
            return result;
        }
        return {obj};
    }

    // ========================================================================
    // Registration
    // ========================================================================

    inline void registerCastingBuiltins(BuiltinTable &t)
    {
        // ---- Capitalized aliases for existing type converters ----

        // Int() — alias for int()
        t["Int"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("Int", 1, (int)args.size(), line);
            if (args[0].isInt())
                return args[0];
            if (args[0].isFloat())
                return XObject::makeInt(static_cast<int64_t>(args[0].asFloat()));
            if (args[0].isString())
            {
                XObject parsed;
                if (!parseNumericValue(args[0].asString(), parsed))
                    throw ConversionError("cannot convert '" + args[0].asString() + "' to Int", line);
                if (parsed.isInt())
                    return parsed;
                if (parsed.isFloat())
                    return XObject::makeInt(static_cast<int64_t>(parsed.asFloat()));
                if (parsed.isComplex())
                {
                    if (parsed.asComplex().imag != 0.0)
                        throw ConversionError("cannot convert complex with non-zero imaginary part to Int", line);
                    return XObject::makeInt(static_cast<int64_t>(parsed.asComplex().real));
                }
                throw ConversionError("cannot convert '" + args[0].asString() + "' to Int", line);
            }
            if (args[0].isBool())
                return XObject::makeInt(args[0].asBool() ? 1 : 0);
            if (args[0].isComplex())
            {
                if (args[0].asComplex().imag != 0.0)
                    throw ConversionError("cannot convert complex with non-zero imaginary part to Int", line);
                return XObject::makeInt(static_cast<int64_t>(args[0].asComplex().real));
            }
            throw ConversionError("cannot convert " + std::string(xtype_name(args[0].type())) + " to Int", line);
        };

        // Float() — alias for float()
        t["Float"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("Float", 1, (int)args.size(), line);
            if (args[0].isFloat())
                return args[0];
            if (args[0].isInt())
                return XObject::makeFloat(static_cast<double>(args[0].asInt()));
            if (args[0].isString())
            {
                XObject parsed;
                if (!parseNumericValue(args[0].asString(), parsed))
                    throw ConversionError("cannot convert '" + args[0].asString() + "' to Float", line);
                if (parsed.isInt())
                    return XObject::makeFloat(static_cast<double>(parsed.asInt()));
                if (parsed.isFloat())
                    return parsed;
                if (parsed.isComplex())
                {
                    if (parsed.asComplex().imag != 0.0)
                        throw ConversionError("cannot convert complex with non-zero imaginary part to Float", line);
                    return XObject::makeFloat(parsed.asComplex().real);
                }
                throw ConversionError("cannot convert '" + args[0].asString() + "' to Float", line);
            }
            if (args[0].isBool())
                return XObject::makeFloat(args[0].asBool() ? 1.0 : 0.0);
            if (args[0].isComplex())
            {
                if (args[0].asComplex().imag != 0.0)
                    throw ConversionError("cannot convert complex with non-zero imaginary part to Float", line);
                return XObject::makeFloat(args[0].asComplex().real);
            }
            throw ConversionError("cannot convert " + std::string(xtype_name(args[0].type())) + " to Float", line);
        };

        // String() — alias for str()
        t["String"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("String", 1, (int)args.size(), line);
            return XObject::makeString(args[0].toString());
        };

        // Complex() — convert to complex (1 or 2 args)
        t["Complex"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() == 1)
            {
                if (args[0].isComplex())
                    return args[0];
                if (args[0].isNumeric())
                    return XObject::makeComplex(args[0].asNumber(), 0.0);
                if (args[0].isString())
                {
                    // Try parsing complex from string
                    XObject result;
                    if (!parseNumericValue(args[0].asString(), result))
                        throw ConversionError("cannot convert '" + args[0].asString() + "' to Complex", line);
                    if (result.isComplex())
                        return result;
                    if (result.isNumeric())
                        return XObject::makeComplex(result.asNumber(), 0.0);
                    throw ConversionError("cannot convert '" + args[0].asString() + "' to Complex", line);
                }
                if (args[0].isBool())
                    return XObject::makeComplex(args[0].asBool() ? 1.0 : 0.0, 0.0);
                throw ConversionError("cannot convert " + std::string(xtype_name(args[0].type())) + " to Complex", line);
            }
            if (args.size() == 2)
            {
                if (!args[0].isNumeric() || !args[1].isNumeric())
                    throw TypeError("Complex() expects two numbers (real, imag)", line);
                return XObject::makeComplex(args[0].asNumber(), args[1].asNumber());
            }
            throw ArityError("Complex", 2, (int)args.size(), line);
        };

        // Bool() — convert to bool
        t["Bool"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("Bool", 1, (int)args.size(), line);
            return XObject::makeBool(args[0].truthy());
        };

        // number() — alias for num(), auto-detect int or float from string
        t["number"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("number", 1, (int)args.size(), line);
            if (args[0].isNumber())
                return args[0];
            if (args[0].isComplex())
                return args[0]; // complex is numeric
            if (args[0].isString())
            {
                XObject parsed;
                if (parseNumericValue(args[0].asString(), parsed))
                    return parsed;
                throw ConversionError("cannot convert '" + args[0].asString() + "' to number", line);
            }
            if (args[0].isBool())
                return XObject::makeInt(args[0].asBool() ? 1 : 0);
            throw ConversionError("cannot convert " + std::string(xtype_name(args[0].type())) + " to number", line);
        };

        // auto() — auto-cast to the best type
        t["auto"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("auto", 1, (int)args.size(), line);
            // If already a non-string type, return as-is
            if (!args[0].isString())
                return args[0];
            return autocastValue(args[0].asString(), line);
        };

        // ---- Container casting functions ----

        // List() — convert to list
        t["List"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("List", 1, (int)args.size(), line);
            auto elems = toElementList(args[0], line);
            return XObject::makeList(XList(std::move(elems)));
        };

        // Tuple() — convert to tuple
        t["Tuple"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("Tuple", 1, (int)args.size(), line);
            auto elems = toElementList(args[0], line);
            return XObject::makeTuple(XTuple(std::move(elems)));
        };

        // Set() — convert to set
        t["Set"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("Set", 1, (int)args.size(), line);
            auto elems = toElementList(args[0], line);
            XSet s;
            for (auto &elem : elems)
            {
                if (!isHashable(elem))
                    throw HashError("set elements must be hashable, got " +
                                        std::string(xtype_name(elem.type())),
                                    line);
                s.add(elem);
            }
            return XObject::makeSet(std::move(s));
        };

        // iSet() — convert to immutable/frozen set
        t["iSet"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("iSet", 1, (int)args.size(), line);
            auto elems = toElementList(args[0], line);
            XSet s;
            for (auto &elem : elems)
            {
                if (!isHashable(elem))
                    throw HashError("frozen set elements must be hashable, got " +
                                        std::string(xtype_name(elem.type())),
                                    line);
                s.add(elem);
            }
            return XObject::makeFrozenSet(std::move(s));
        };

        // ---- Smart-cast container functions (auto-cast elements) ----

        // ~List() — convert to list with auto-cast elements
        t["~List"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("~List", 1, (int)args.size(), line);
            auto elems = toElementListSmart(args[0], line);
            return XObject::makeList(XList(std::move(elems)));
        };

        // ~Tuple() — convert to tuple with auto-cast elements
        t["~Tuple"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("~Tuple", 1, (int)args.size(), line);
            auto elems = toElementListSmart(args[0], line);
            return XObject::makeTuple(XTuple(std::move(elems)));
        };

        // ~Set() — convert to set with auto-cast elements
        t["~Set"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("~Set", 1, (int)args.size(), line);
            auto elems = toElementListSmart(args[0], line);
            XSet s;
            for (auto &elem : elems)
            {
                if (!isHashable(elem))
                    throw HashError("set elements must be hashable, got " +
                                        std::string(xtype_name(elem.type())),
                                    line);
                s.add(elem);
            }
            return XObject::makeSet(std::move(s));
        };

        // ~iSet() — convert to frozen set with auto-cast elements
        t["~iSet"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("~iSet", 1, (int)args.size(), line);
            auto elems = toElementListSmart(args[0], line);
            XSet s;
            for (auto &elem : elems)
            {
                if (!isHashable(elem))
                    throw HashError("frozen set elements must be hashable, got " +
                                        std::string(xtype_name(elem.type())),
                                    line);
                s.add(elem);
            }
            return XObject::makeFrozenSet(std::move(s));
        };
    }

} // namespace xell
