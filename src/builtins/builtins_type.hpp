#pragma once

// =============================================================================
// Type builtins — type, str, num
// =============================================================================

#include "builtin_registry.hpp"

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

        t["str"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("str", 1, (int)args.size(), line);
            return XObject::makeString(args[0].toString());
        };

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
                    double d = std::stod(args[0].asString());
                    return XObject::makeNumber(d);
                }
                catch (...)
                {
                    throw ConversionError("cannot convert '" + args[0].asString() + "' to number", line);
                }
            }
            if (args[0].isBool())
                return XObject::makeNumber(args[0].asBool() ? 1.0 : 0.0);
            throw ConversionError("cannot convert " + std::string(xtype_name(args[0].type())) + " to number", line);
        };
    }

} // namespace xell
