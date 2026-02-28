#pragma once

// =============================================================================
// IO builtins — print
// =============================================================================

#include "builtin_registry.hpp"
#include <string>
#include <vector>

namespace xell
{

    /// Register IO builtins.
    /// @param output  Reference to the interpreter's captured output vector.
    inline void registerIOBuiltins(BuiltinTable &t, std::vector<std::string> &output)
    {
        t["print"] = [&output](std::vector<XObject> &args, int /*line*/) -> XObject
        {
            std::string line;
            for (size_t i = 0; i < args.size(); i++)
            {
                if (i > 0)
                    line += " ";
                line += args[i].toString();
            }
            output.push_back(line);
            return XObject::makeNone();
        };
    }

} // namespace xell
