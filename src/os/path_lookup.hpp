#pragma once

// =============================================================================
// path_lookup.hpp — PATH-based external command resolution for Xell
// =============================================================================
// Provides isExecutableOnPath() and buildCommandString() for running external
// programs (git, code, python, etc.) directly from Xell code — just like bash.
//
// Used by the interpreter as a last-resort fallback after all Xell-native
// resolution (user functions, structs/classes, builtins, modules) fails.
//
// The lexer's keyword table is used as a guard list — `if`, `for`, `fn`, etc.
// are never treated as external commands.
// =============================================================================

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif

#include "../interpreter/xobject.hpp"

namespace xell
{

    // ─── Xell keywords that must never be treated as external commands ────

    inline const std::unordered_set<std::string> &xellKeywords()
    {
        static const std::unordered_set<std::string> kw = {
            // Control flow
            "fn",
            "give",
            "if",
            "elif",
            "else",
            "for",
            "while",
            "in",
            "break",
            "continue",
            "try",
            "catch",
            "finally",
            "incase",
            "let",
            "be",
            "loop",
            // Import / module
            "bring",
            "from",
            "as",
            "module",
            "export",
            "requires",
            // Enum
            "enum",
            // OOP
            "struct",
            "class",
            "inherits",
            "immutable",
            "private",
            "protected",
            "public",
            "static",
            "interface",
            "implements",
            "abstract",
            "mixin",
            "with",
            // Generator / async
            "yield",
            "async",
            "await",
            // Literals
            "true",
            "false",
            "none",
            // Logical
            "and",
            "or",
            "not",
            // Comparison keywords
            "is",
            "eq",
            "ne",
            "gt",
            "lt",
            "ge",
            "le",
            // Utility
            "of",
            // Common builtins that shouldn't be hijacked
            "print",
            "input",
            "typeof",
            "len",
            "range",
            "assert",
            "map",
            "filter",
            "reduce",
            "any",
            "all",
            "push",
            "pop",
            "keys",
            "values",
            "has",
        };
        return kw;
    }

    inline bool isXellKeyword(const std::string &name)
    {
        return xellKeywords().count(name) > 0;
    }

    // ─── PATH lookup with caching ────────────────────────────────────────

    inline bool isExecutableOnPath(const std::string &cmd)
    {
        // Cache results to avoid repeated filesystem access
        static std::unordered_map<std::string, bool> cache;

        auto it = cache.find(cmd);
        if (it != cache.end())
            return it->second;

        bool found = false;

#ifdef _WIN32
        // Windows: use SearchPathA to find .exe, .cmd, .bat, etc.
        char buf[MAX_PATH];
        // Try common extensions
        const char *extensions[] = {".exe", ".cmd", ".bat", ".com", nullptr};
        for (const char **ext = extensions; *ext && !found; ++ext)
        {
            if (SearchPathA(NULL, cmd.c_str(), *ext, MAX_PATH, buf, NULL) > 0)
                found = true;
        }
#else
        // Unix: walk $PATH directories, check X_OK
        const char *pathEnv = std::getenv("PATH");
        if (pathEnv)
        {
            std::string paths(pathEnv);
            size_t pos = 0;
            while (pos < paths.size())
            {
                size_t sep = paths.find(':', pos);
                if (sep == std::string::npos)
                    sep = paths.size();

                std::string dir = paths.substr(pos, sep - pos);
                std::string full = dir + "/" + cmd;

                if (access(full.c_str(), X_OK) == 0)
                {
                    found = true;
                    break;
                }
                pos = sep + 1;
            }
        }
#endif

        cache[cmd] = found;
        return found;
    }

    // ─── Build a shell command string from function name + arguments ─────

    inline std::string buildCommandString(const std::string &cmd,
                                          const std::vector<XObject> &args)
    {
        std::string result = cmd;

        for (const auto &arg : args)
        {
            result += ' ';

            if (arg.isString())
            {
                const std::string &s = arg.asString();
                // Shell-quote if the string contains spaces or special chars
                bool needsQuote = false;
                for (char c : s)
                {
                    if (c == ' ' || c == '\t' || c == '"' || c == '\'' ||
                        c == '(' || c == ')' || c == '&' ||
                        c == '|' || c == ';' || c == '<' || c == '>' ||
                        c == '*' || c == '?' || c == '#' ||
#ifdef _WIN32
                        c == '^' || c == '%' ||
#else
                        c == '\\' || c == '$' || c == '`' || c == '!' ||
                        c == '{' || c == '}' || c == '[' || c == ']' || c == '~' ||
#endif
                        false)
                    {
                        needsQuote = true;
                        break;
                    }
                }

                if (needsQuote)
                {
#ifdef _WIN32
                    // Windows cmd.exe: use double quotes, escape inner double quotes
                    result += '"';
                    for (char c : s)
                    {
                        if (c == '"')
                            result += "\\\"";
                        else if (c == '%')
                            result += "%%";
                        else
                            result += c;
                    }
                    result += '"';
#else
                    // Unix: use single quotes (safest), escape existing single quotes
                    result += '\'';
                    for (char c : s)
                    {
                        if (c == '\'')
                            result += "'\\''";
                        else
                            result += c;
                    }
                    result += '\'';
#endif
                }
                else
                {
                    result += s;
                }
            }
            else if (arg.isInt())
            {
                result += std::to_string(arg.asInt());
            }
            else if (arg.isFloat())
            {
                result += std::to_string(arg.asFloat());
            }
            else if (arg.isBool())
            {
                result += arg.asBool() ? "true" : "false";
            }
            else if (arg.isNone())
            {
                // Skip none args
            }
            else
            {
                // For complex objects, convert to string representation
                result += arg.toString();
            }
        }

        return result;
    }

} // namespace xell
