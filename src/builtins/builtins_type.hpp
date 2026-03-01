#pragma once

// =============================================================================
// Type builtins — type, str, int, float, num, complex, real, imag, conjugate
// =============================================================================

#include "builtin_registry.hpp"
#include <cmath>

namespace xell
{

    inline void registerTypeBuiltins(BuiltinTable &t)
    {
        t["type"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("type", 1, (int)args.size(), line);
            return XObject::makeString(xtype_name(args[0].type()));
        };

        // typeof is an alias for type
        t["typeof"] = t["type"];

        t["str"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("str", 1, (int)args.size(), line);
            return XObject::makeString(args[0].toString());
        };

        // int() — convert to integer
        t["int"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("int", 1, (int)args.size(), line);
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
                    throw ConversionError("cannot convert '" + args[0].asString() + "' to int", line);
                }
            }
            if (args[0].isBool())
                return XObject::makeInt(args[0].asBool() ? 1 : 0);
            throw ConversionError("cannot convert " + std::string(xtype_name(args[0].type())) + " to int", line);
        };

        // float() — convert to float
        t["float"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("float", 1, (int)args.size(), line);
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
                    throw ConversionError("cannot convert '" + args[0].asString() + "' to float", line);
                }
            }
            if (args[0].isBool())
                return XObject::makeFloat(args[0].asBool() ? 1.0 : 0.0);
            throw ConversionError("cannot convert " + std::string(xtype_name(args[0].type())) + " to float", line);
        };

        // num() — backward compat: converts to float (same as float())
        t["num"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("num", 1, (int)args.size(), line);
            if (args[0].isNumber())
                return args[0];
            if (args[0].isString())
            {
                try
                {
                    const std::string &s = args[0].asString();
                    // Try int first
                    if (s.find('.') == std::string::npos)
                    {
                        try
                        {
                            int64_t i = std::stoll(s);
                            return XObject::makeInt(i);
                        }
                        catch (...)
                        {
                        }
                    }
                    double d = std::stod(s);
                    return XObject::makeFloat(d);
                }
                catch (...)
                {
                    throw ConversionError("cannot convert '" + args[0].asString() + "' to number", line);
                }
            }
            if (args[0].isBool())
                return XObject::makeInt(args[0].asBool() ? 1 : 0);
            throw ConversionError("cannot convert " + std::string(xtype_name(args[0].type())) + " to number", line);
        };

        // complex(real, imag) — create a complex number
        t["complex"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() == 1)
            {
                // complex(num) → complex with imag=0
                if (!args[0].isNumeric())
                    throw TypeError("complex() expects a number", line);
                if (args[0].isComplex())
                    return args[0];
                return XObject::makeComplex(args[0].asNumber(), 0.0);
            }
            if (args.size() == 2)
            {
                if (!args[0].isNumber() || !args[1].isNumber())
                    throw TypeError("complex() expects two numbers (real, imag)", line);
                return XObject::makeComplex(args[0].asNumber(), args[1].asNumber());
            }
            throw ArityError("complex", 2, (int)args.size(), line);
        };

        // real(complex) — get real part
        t["real"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("real", 1, (int)args.size(), line);
            if (args[0].isComplex())
                return XObject::makeFloat(args[0].asComplex().real);
            if (args[0].isNumber())
                return XObject::makeFloat(args[0].asNumber());
            throw TypeError("real() expects a number or complex", line);
        };

        // imag(complex) — get imaginary part
        t["imag"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("imag", 1, (int)args.size(), line);
            if (args[0].isComplex())
                return XObject::makeFloat(args[0].asComplex().imag);
            if (args[0].isNumber())
                return XObject::makeFloat(0.0);
            throw TypeError("imag() expects a number or complex", line);
        };

        // conjugate(complex) — complex conjugate
        t["conjugate"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("conjugate", 1, (int)args.size(), line);
            if (args[0].isComplex())
                return XObject::makeComplex(args[0].asComplex().conjugate());
            if (args[0].isNumber())
                return XObject::makeFloat(args[0].asNumber()); // conjugate of real = itself
            throw TypeError("conjugate() expects a number or complex", line);
        };

        // magnitude(complex) — absolute value / magnitude
        t["magnitude"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("magnitude", 1, (int)args.size(), line);
            if (args[0].isComplex())
                return XObject::makeFloat(args[0].asComplex().magnitude());
            if (args[0].isNumber())
                return XObject::makeFloat(std::fabs(args[0].asNumber()));
            throw TypeError("magnitude() expects a number or complex", line);
        };
    }

} // namespace xell
