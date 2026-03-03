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

    /// Auto-cast a string to the best XObject type
    static inline XObject autocastValue(const std::string &s, int line)
    {
        // Boolean
        if (s == "true")
            return XObject::makeBool(true);
        if (s == "false")
            return XObject::makeBool(false);
        if (s == "none")
            return XObject::makeNone();

        // Try integer
        if (!s.empty() && (s[0] == '-' || s[0] == '+' || (s[0] >= '0' && s[0] <= '9')))
        {
            // Check for complex: ends with 'i'
            if (s.size() > 1 && s.back() == 'i')
            {
                // Try parsing as complex: could be "3i" or "2+3i" or "2-3i"
                std::string withoutI = s.substr(0, s.size() - 1);
                // Simple imaginary: "3i", "-2.5i"
                try
                {
                    size_t pos;
                    double imag = std::stod(withoutI, &pos);
                    if (pos == withoutI.size())
                        return XObject::makeComplex(0.0, imag);
                }
                catch (...)
                {
                }
                // Complex: "2+3i" or "2-3i"
                size_t plusPos = withoutI.find_last_of('+');
                size_t minusPos = withoutI.find_last_of('-');
                size_t splitAt = std::string::npos;
                if (plusPos != std::string::npos && plusPos > 0)
                    splitAt = plusPos;
                else if (minusPos != std::string::npos && minusPos > 0)
                    splitAt = minusPos;
                if (splitAt != std::string::npos)
                {
                    try
                    {
                        double real = std::stod(withoutI.substr(0, splitAt));
                        double imag = std::stod(withoutI.substr(splitAt));
                        return XObject::makeComplex(real, imag);
                    }
                    catch (...)
                    {
                    }
                }
            }

            // Try integer (no decimal point)
            if (s.find('.') == std::string::npos && s.back() != 'i')
            {
                try
                {
                    size_t pos;
                    int64_t i = std::stoll(s, &pos);
                    if (pos == s.size())
                        return XObject::makeInt(i);
                }
                catch (...)
                {
                }
            }

            // Try float
            try
            {
                size_t pos;
                double d = std::stod(s, &pos);
                if (pos == s.size())
                    return XObject::makeFloat(d);
            }
            catch (...)
            {
            }
        }

        // Try nested container detection: [1,2,3] → list
        if (s.size() >= 2 && s.front() == '[' && s.back() == ']')
        {
            std::string inner = s.substr(1, s.size() - 2);
            auto parts = splitForContainer(inner);
            XList list;
            for (const auto &part : parts)
                list.push_back(autocastValue(part, line));
            return XObject::makeList(std::move(list));
        }

        // Try tuple: (1,2,3)
        if (s.size() >= 2 && s.front() == '(' && s.back() == ')')
        {
            std::string inner = s.substr(1, s.size() - 2);
            auto parts = splitForContainer(inner);
            XTuple tup;
            for (const auto &part : parts)
                tup.push_back(autocastValue(part, line));
            return XObject::makeTuple(std::move(tup));
        }

        // Try set: {1,2,3}
        if (s.size() >= 2 && s.front() == '{' && s.back() == '}')
        {
            std::string inner = s.substr(1, s.size() - 2);
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
        return XObject::makeString(s);
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
                try
                {
                    int64_t i = std::stoll(args[0].asString());
                    return XObject::makeInt(i);
                }
                catch (...)
                {
                    throw ConversionError("cannot convert '" + args[0].asString() + "' to Int", line);
                }
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
                try
                {
                    double d = std::stod(args[0].asString());
                    return XObject::makeFloat(d);
                }
                catch (...)
                {
                    throw ConversionError("cannot convert '" + args[0].asString() + "' to Float", line);
                }
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
                    XObject result = autocastValue(args[0].asString(), line);
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
                const std::string &s = args[0].asString();
                // Try int
                if (s.find('.') == std::string::npos)
                {
                    try
                    {
                        size_t pos;
                        int64_t i = std::stoll(s, &pos);
                        if (pos == s.size())
                            return XObject::makeInt(i);
                    }
                    catch (...)
                    {
                    }
                }
                // Try float
                try
                {
                    size_t pos;
                    double d = std::stod(s, &pos);
                    if (pos == s.size())
                        return XObject::makeFloat(d);
                }
                catch (...)
                {
                }
                throw ConversionError("cannot convert '" + s + "' to number", line);
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
