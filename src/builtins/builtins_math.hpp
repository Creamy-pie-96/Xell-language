#pragma once

// =============================================================================
// Math builtins — floor, ceil, round, abs, mod
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
            return XObject::makeNumber(std::floor(args[0].asNumber()));
        };

        t["ceil"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("ceil", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("ceil() expects a number", line);
            return XObject::makeNumber(std::ceil(args[0].asNumber()));
        };

        t["round"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("round", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("round() expects a number", line);
            return XObject::makeNumber(std::round(args[0].asNumber()));
        };

        t["abs"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("abs", 1, (int)args.size(), line);
            if (!args[0].isNumber())
                throw TypeError("abs() expects a number", line);
            return XObject::makeNumber(std::fabs(args[0].asNumber()));
        };

        t["mod"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("mod", 2, (int)args.size(), line);
            if (!args[0].isNumber() || !args[1].isNumber())
                throw TypeError("mod() expects two numbers", line);
            double b = args[1].asNumber();
            if (b == 0.0)
                throw DivisionByZeroError(line);
            return XObject::makeNumber(std::fmod(args[0].asNumber(), b));
        };
    }

} // namespace xell
