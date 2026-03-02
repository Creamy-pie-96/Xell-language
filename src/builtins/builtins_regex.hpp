#pragma once

// =============================================================================
// Regex builtins — regex_match, regex_match_full, regex_find, regex_find_all,
//                  regex_replace, regex_replace_all, regex_split, regex_groups
// =============================================================================

#include "builtin_registry.hpp"
#include <regex>

namespace xell
{

    inline void registerRegexBuiltins(BuiltinTable &t)
    {
        // regex_match(str, pattern) — true if pattern matches anywhere in str
        t["regex_match"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("regex_match", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("regex_match() expects two strings", line);

            try
            {
                std::regex re(args[1].asString());
                return XObject::makeBool(
                    std::regex_search(args[0].asString(), re));
            }
            catch (const std::regex_error &e)
            {
                throw RuntimeError(
                    std::string("regex_match() invalid pattern: ") + e.what(), line);
            }
        };

        // regex_match_full(str, pattern) — true only if entire string matches
        t["regex_match_full"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("regex_match_full", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("regex_match_full() expects two strings", line);

            try
            {
                std::regex re(args[1].asString());
                return XObject::makeBool(
                    std::regex_match(args[0].asString(), re));
            }
            catch (const std::regex_error &e)
            {
                throw RuntimeError(
                    std::string("regex_match_full() invalid pattern: ") + e.what(), line);
            }
        };

        // regex_find(str, pattern) — first match string, or "" if none
        t["regex_find"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("regex_find", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("regex_find() expects two strings", line);

            try
            {
                std::regex re(args[1].asString());
                std::smatch m;
                if (std::regex_search(args[0].asString(), m, re))
                    return XObject::makeString(m[0].str());
                return XObject::makeString("");
            }
            catch (const std::regex_error &e)
            {
                throw RuntimeError(
                    std::string("regex_find() invalid pattern: ") + e.what(), line);
            }
        };

        // regex_find_all(str, pattern) — list of all match strings
        t["regex_find_all"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("regex_find_all", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("regex_find_all() expects two strings", line);

            try
            {
                std::regex re(args[1].asString());
                const std::string &s = args[0].asString();
                std::vector<XObject> results;

                auto begin = std::sregex_iterator(s.begin(), s.end(), re);
                auto end = std::sregex_iterator();
                for (auto it = begin; it != end; ++it)
                    results.push_back(XObject::makeString((*it)[0].str()));

                return XObject::makeList(std::move(results));
            }
            catch (const std::regex_error &e)
            {
                throw RuntimeError(
                    std::string("regex_find_all() invalid pattern: ") + e.what(), line);
            }
        };

        // regex_replace(str, pattern, replacement) — replace first occurrence
        t["regex_replace"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("regex_replace", 3, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString() || !args[2].isString())
                throw TypeError("regex_replace() expects three strings", line);

            try
            {
                std::regex re(args[1].asString());
                // format_first_only → only first match
                std::string result = std::regex_replace(
                    args[0].asString(), re, args[2].asString(),
                    std::regex_constants::format_first_only);
                return XObject::makeString(result);
            }
            catch (const std::regex_error &e)
            {
                throw RuntimeError(
                    std::string("regex_replace() invalid pattern: ") + e.what(), line);
            }
        };

        // regex_replace_all(str, pattern, replacement) — replace all occurrences
        t["regex_replace_all"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("regex_replace_all", 3, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString() || !args[2].isString())
                throw TypeError("regex_replace_all() expects three strings", line);

            try
            {
                std::regex re(args[1].asString());
                std::string result = std::regex_replace(
                    args[0].asString(), re, args[2].asString());
                return XObject::makeString(result);
            }
            catch (const std::regex_error &e)
            {
                throw RuntimeError(
                    std::string("regex_replace_all() invalid pattern: ") + e.what(), line);
            }
        };

        // regex_split(str, pattern) — split string by regex pattern
        t["regex_split"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("regex_split", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("regex_split() expects two strings", line);

            try
            {
                std::regex re(args[1].asString());
                const std::string &s = args[0].asString();
                std::vector<XObject> parts;

                std::sregex_token_iterator begin(s.begin(), s.end(), re, -1);
                std::sregex_token_iterator end;
                for (auto it = begin; it != end; ++it)
                    parts.push_back(XObject::makeString(it->str()));

                return XObject::makeList(std::move(parts));
            }
            catch (const std::regex_error &e)
            {
                throw RuntimeError(
                    std::string("regex_split() invalid pattern: ") + e.what(), line);
            }
        };

        // regex_groups(str, pattern) — list of capture group strings from first match
        t["regex_groups"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("regex_groups", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("regex_groups() expects two strings", line);

            try
            {
                std::regex re(args[1].asString());
                std::smatch m;
                std::vector<XObject> groups;

                if (std::regex_search(args[0].asString(), m, re))
                {
                    // Skip m[0] (full match), return capture groups m[1], m[2], ...
                    for (size_t i = 1; i < m.size(); ++i)
                        groups.push_back(XObject::makeString(m[i].str()));
                }

                return XObject::makeList(std::move(groups));
            }
            catch (const std::regex_error &e)
            {
                throw RuntimeError(
                    std::string("regex_groups() invalid pattern: ") + e.what(), line);
            }
        };
    }

} // namespace xell
