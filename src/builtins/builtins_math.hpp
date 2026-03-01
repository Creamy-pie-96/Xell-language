#pragma once

// =============================================================================
// Math builtins — floor, ceil, round, abs, mod, pow, sqrt
// =============================================================================

#include "builtin_registry.hpp"
#include <cmath>

namespace xell
{

    inline void registerMathBuiltins(BuiltinTable &t)
    {
        t["floor"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("floor", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("floor() expects a number", line);
            return XObject::makeInt(static_cast<int64_t>(std::floor(args[0].asNumber())));
        };

        t["ceil"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("ceil", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("ceil() expects a number", line);
            return XObject::makeInt(static_cast<int64_t>(std::ceil(args[0].asNumber())));
        };

        t["round"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("round", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("round() expects a number", line);
            return XObject::makeInt(static_cast<int64_t>(std::round(args[0].asNumber())));
        };

        t["abs"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("abs", 1, (int)args.size(), line);
            if (args[0].isInt())
            {
                int64_t val = args[0].asInt();
                return XObject::makeInt(val < 0 ? -val : val);
            }
            if (args[0].isFloat())
                return XObject::makeFloat(std::fabs(args[0].asFloat()));
            if (args[0].isComplex())
                return XObject::makeFloat(args[0].asComplex().magnitude());
            throw TypeError("abs() expects a number", line);
        };

        t["mod"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("mod", 2, (int)args.size(), line);
            if (!args[0].isNumber() || !args[1].isNumber())
                throw TypeError("mod() expects two numbers", line);
            if (args[0].isInt() && args[1].isInt())
            {
                int64_t b = args[1].asInt();
                if (b == 0)
                    throw DivisionByZeroError(line);
                return XObject::makeInt(args[0].asInt() % b);
            }
            double b = args[1].asNumber();
            if (b == 0.0)
                throw DivisionByZeroError(line);
            return XObject::makeFloat(std::fmod(args[0].asNumber(), b));
        };

        t["pow"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("pow", 2, (int)args.size(), line);
            if (!args[0].isNumber() || !args[1].isNumber())
                throw TypeError("pow() expects two numbers", line);
            return XObject::makeFloat(std::pow(args[0].asNumber(), args[1].asNumber()));
        };

        t["sqrt"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("sqrt", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("sqrt() expects a number", line);
            double val = args[0].asNumber();
            if (val < 0.0)
            {
                // sqrt of negative → complex result
                return XObject::makeComplex(0.0, std::sqrt(-val));
            }
            return XObject::makeFloat(std::sqrt(val));
        };
    }

} // namespace xell
