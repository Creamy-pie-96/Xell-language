#pragma once

// =============================================================================
// Utility builtins — assert, format
// =============================================================================

#include "builtin_registry.hpp"
#include <sstream>
#include <iomanip>
#include <cmath>

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

        // format(template, args...) — Python-style string formatting
        // Supports: {} for positional, {0} for indexed, {:.2f} for format specs
        t["format"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.empty())
                throw ArityError("format", 1, 0, line);
            if (!args[0].isString())
                throw TypeError("format() first argument must be a string", line);

            const std::string &tmpl = args[0].asString();
            std::string result;
            size_t argIdx = 1; // next positional argument index (1-based in args vector)
            size_t i = 0;

            while (i < tmpl.size())
            {
                if (tmpl[i] == '{')
                {
                    if (i + 1 < tmpl.size() && tmpl[i + 1] == '{')
                    {
                        result += '{'; // escaped {{
                        i += 2;
                        continue;
                    }

                    // Find closing }
                    size_t j = tmpl.find('}', i + 1);
                    if (j == std::string::npos)
                    {
                        result += tmpl.substr(i);
                        break;
                    }

                    std::string spec = tmpl.substr(i + 1, j - i - 1);
                    i = j + 1;

                    // Parse: optional index, optional :format
                    size_t colonPos = spec.find(':');
                    std::string indexPart = (colonPos != std::string::npos) ? spec.substr(0, colonPos) : spec;
                    std::string fmtPart = (colonPos != std::string::npos) ? spec.substr(colonPos + 1) : "";

                    // Determine which argument to use
                    size_t targetIdx;
                    if (indexPart.empty())
                    {
                        targetIdx = argIdx++;
                    }
                    else
                    {
                        try
                        {
                            targetIdx = std::stoul(indexPart) + 1;
                        } // 0-based to 1-based
                        catch (...)
                        {
                            targetIdx = argIdx++;
                        }
                    }

                    if (targetIdx >= args.size())
                    {
                        result += "<missing>";
                        continue;
                    }

                    const XObject &val = args[targetIdx];

                    // Apply format spec
                    if (fmtPart.empty())
                    {
                        result += val.toString();
                    }
                    else
                    {
                        // Parse format spec: [fill][align][width][.precision][type]
                        // Simplified: support .Nf for float formatting
                        if (!fmtPart.empty() && fmtPart[0] == '.' && val.isNumeric())
                        {
                            // .Nf format
                            std::string precStr;
                            size_t k = 1;
                            while (k < fmtPart.size() && std::isdigit(fmtPart[k]))
                                precStr += fmtPart[k++];
                            int precision = precStr.empty() ? 6 : std::stoi(precStr);
                            if (val.isComplex())
                            {
                                // Format complex as (real+imagi) with precision
                                std::ostringstream oss;
                                oss << std::fixed << std::setprecision(precision);
                                auto c = val.asComplex();
                                oss << "(" << c.real;
                                if (c.imag >= 0)
                                    oss << "+";
                                oss << c.imag << "i)";
                                result += oss.str();
                            }
                            else
                            {
                                std::ostringstream oss;
                                oss << std::fixed << std::setprecision(precision) << val.asNumber();
                                result += oss.str();
                            }
                        }
                        else
                        {
                            result += val.toString();
                        }
                    }
                }
                else if (tmpl[i] == '}' && i + 1 < tmpl.size() && tmpl[i + 1] == '}')
                {
                    result += '}'; // escaped }}
                    i += 2;
                }
                else
                {
                    result += tmpl[i++];
                }
            }

            return XObject::makeString(result);
        };
    }

} // namespace xell
