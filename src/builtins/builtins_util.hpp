#pragma once

// =============================================================================
// Utility builtins — assert
// =============================================================================

#include "builtin_registry.hpp"

namespace xell
{

    inline void registerUtilBuiltins(BuiltinTable &t)
    {
        t["assert"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.empty() || args.size() > 2)
                throw ArityError("assert", 1, (int)args.size(), line);
            if (!args[0].truthy())
            {
                std::string msg = args.size() == 2 ? args[1].toString() : "assertion failed";
                throw AssertionError(msg, line);
            }
            return XObject::makeNone();
        };
    }

} // namespace xell
