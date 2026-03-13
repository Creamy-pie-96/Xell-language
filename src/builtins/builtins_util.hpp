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

    inline std::string applyFormatSpec(const XObject &val, const std::string &fmtPart, int line)
    {
        if (fmtPart.empty())
            return val.toString();

        size_t idx = 0;
        char fill = ' ';
        char align = '>';
        int width = 0;
        int precision = -1;
        char type = '\0';

        if (fmtPart.size() >= 2 &&
            (fmtPart[1] == '<' || fmtPart[1] == '>' || fmtPart[1] == '^'))
        {
            fill = fmtPart[0];
            align = fmtPart[1];
            idx = 2;
        }
        else if (!fmtPart.empty() &&
                 (fmtPart[0] == '<' || fmtPart[0] == '>' || fmtPart[0] == '^'))
        {
            align = fmtPart[0];
            idx = 1;
        }

        bool zeroPad = false;
        if (idx < fmtPart.size() && fmtPart[idx] == '0')
        {
            zeroPad = true;
            fill = '0';
            align = '>';
        }

        while (idx < fmtPart.size() && std::isdigit((unsigned char)fmtPart[idx]))
        {
            width = width * 10 + (fmtPart[idx] - '0');
            idx++;
        }

        if (idx < fmtPart.size() && fmtPart[idx] == '.')
        {
            idx++;
            precision = 0;
            while (idx < fmtPart.size() && std::isdigit((unsigned char)fmtPart[idx]))
            {
                precision = precision * 10 + (fmtPart[idx] - '0');
                idx++;
            }
        }

        if (idx < fmtPart.size())
            type = fmtPart[idx++];

        if (idx != fmtPart.size())
            throw ValueError("invalid format specifier '" + fmtPart + "'", line);

        std::string rendered;

        if (type == 'd' || type == 'i')
        {
            if (!val.isNumeric())
                throw TypeError("integer format specifier requires a number", line);
            rendered = std::to_string(val.isInt() ? val.asInt() : (int64_t)val.asNumber());
        }
        else if (type == 'f' || (type == '\0' && precision >= 0 && val.isNumeric()))
        {
            std::ostringstream oss;
            oss << std::fixed << std::setprecision(precision >= 0 ? precision : 6);
            if (val.isComplex())
            {
                auto c = val.asComplex();
                oss << "(" << c.real;
                if (c.imag >= 0)
                    oss << "+";
                oss << c.imag << "i)";
            }
            else
            {
                oss << val.asNumber();
            }
            rendered = oss.str();
        }
        else
        {
            rendered = val.toString();
        }

        if ((int)rendered.size() >= width)
            return rendered;

        int pad = width - (int)rendered.size();
        std::string padding((size_t)pad, fill);

        if (align == '<')
            return rendered + padding;
        if (align == '^')
        {
            int left = pad / 2;
            int right = pad - left;
            return std::string((size_t)left, fill) + rendered + std::string((size_t)right, fill);
        }

        if (zeroPad && !rendered.empty() && (rendered[0] == '+' || rendered[0] == '-'))
            return rendered.substr(0, 1) + padding + rendered.substr(1);
        return padding + rendered;
    }

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

                    result += applyFormatSpec(val, fmtPart, line);
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
