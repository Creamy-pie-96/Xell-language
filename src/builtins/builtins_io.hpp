#pragma once

// =============================================================================
// IO builtins — print, input, exit
// =============================================================================

#include "builtin_registry.hpp"
#include <string>
#include <vector>
#include <iostream>
#include <cstdlib>

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
            return XObject::makeInt(0);
        };

        // input(prompt) or input() — read a line from stdin
        t["input"] = [&output](std::vector<XObject> &args, int line) -> XObject
        {
            // Optional prompt argument
            if (args.size() > 1)
                throw ArityError("input", 1, (int)args.size(), line);

            if (args.size() == 1)
            {
                if (!args[0].isString())
                    throw TypeError("input() prompt must be a string", line);
                // Print prompt to stdout (not to output buffer)
                std::cout << args[0].asString() << std::flush;
            }

            std::string input_line;
            if (std::getline(std::cin, input_line))
                return XObject::makeString(input_line);
            else
                return XObject::makeString(""); // EOF or error → return empty string
        };

        // exit(code) — terminate the program with an exit code
        t["exit"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            int exit_code = 0;
            if (args.size() == 0)
                exit_code = 0;
            else if (args.size() == 1)
            {
                if (!args[0].isNumber())
                    throw TypeError("exit() code must be a number", line);
                exit_code = args[0].isInt() ? static_cast<int>(args[0].asInt()) : static_cast<int>(args[0].asNumber());
            }
            else
                throw ArityError("exit", 1, (int)args.size(), line);

            std::exit(exit_code);
            return XObject::makeNone(); // never reached
        };
    }

} // namespace xell
