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
            // Return 0 (shell "success" exit code) so that shell-style
            // operators work correctly:
            //   print("a") && print("b")  →  both execute (0 = success → continue)
            //   print("a") || print("b")  →  only first   (0 = success → skip fallback)
            return XObject::makeNumber(0);
        };
    }

} // namespace xell
