#pragma once

// =============================================================================
// String builtins — split, join, trim, upper, lower, replace, starts_with,
//                   ends_with, contains, index_of, substr, char_at, repeat,
//                   pad_start, pad_end, reverse, count, is_empty, is_numeric,
//                   is_alpha, lines, to_chars, trim_start, trim_end,
//                   replace_first
// =============================================================================

#include "builtin_registry.hpp"
#include <algorithm>
#include <cctype>

namespace xell
{

    inline void registerStringBuiltins(BuiltinTable &t)
    {
        // split(str, sep) → list of strings
        t["split"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("split", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("split() expects two strings", line);

            const std::string &s = args[0].asString();
            const std::string &sep = args[1].asString();
            XList result;

            if (sep.empty())
            {
                // Split into individual characters
                for (char c : s)
                    result.push_back(XObject::makeString(std::string(1, c)));
                return XObject::makeList(std::move(result));
            }

            size_t start = 0, pos;
            while ((pos = s.find(sep, start)) != std::string::npos)
            {
                result.push_back(XObject::makeString(s.substr(start, pos - start)));
                start = pos + sep.size();
            }
            result.push_back(XObject::makeString(s.substr(start)));
            return XObject::makeList(std::move(result));
        };

        // join(list, sep) → string
        t["join"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("join", 2, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("join() expects a list as first argument", line);
            if (!args[1].isString())
                throw TypeError("join() expects a string separator as second argument", line);

            const auto &list = args[0].asList();
            const std::string &sep = args[1].asString();
            std::string result;
            for (size_t i = 0; i < list.size(); ++i)
            {
                if (i > 0)
                    result += sep;
                result += list[i].toString();
            }
            return XObject::makeString(std::move(result));
        };

        // trim(str) → string
        t["trim"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("trim", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("trim() expects a string", line);
            const std::string &s = args[0].asString();
            size_t start = 0, end = s.size();
            while (start < end && std::isspace((unsigned char)s[start]))
                ++start;
            while (end > start && std::isspace((unsigned char)s[end - 1]))
                --end;
            return XObject::makeString(s.substr(start, end - start));
        };

        // trim_start(str) → string
        t["trim_start"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("trim_start", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("trim_start() expects a string", line);
            const std::string &s = args[0].asString();
            size_t start = 0;
            while (start < s.size() && std::isspace((unsigned char)s[start]))
                ++start;
            return XObject::makeString(s.substr(start));
        };

        // trim_end(str) → string
        t["trim_end"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("trim_end", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("trim_end() expects a string", line);
            const std::string &s = args[0].asString();
            size_t end = s.size();
            while (end > 0 && std::isspace((unsigned char)s[end - 1]))
                --end;
            return XObject::makeString(s.substr(0, end));
        };

        // upper(str) → string
        t["upper"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("upper", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("upper() expects a string", line);
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c)
                           { return std::toupper(c); });
            return XObject::makeString(std::move(s));
        };

        // lower(str) → string
        t["lower"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("lower", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("lower() expects a string", line);
            std::string s = args[0].asString();
            std::transform(s.begin(), s.end(), s.begin(),
                           [](unsigned char c)
                           { return std::tolower(c); });
            return XObject::makeString(std::move(s));
        };

        // replace(str, old, new) → string — replace all occurrences
        t["replace"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("replace", 3, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString() || !args[2].isString())
                throw TypeError("replace() expects three strings", line);
            const std::string &s = args[0].asString();
            const std::string &oldStr = args[1].asString();
            const std::string &newStr = args[2].asString();
            if (oldStr.empty())
                return args[0];

            std::string result;
            size_t start = 0, pos;
            while ((pos = s.find(oldStr, start)) != std::string::npos)
            {
                result += s.substr(start, pos - start);
                result += newStr;
                start = pos + oldStr.size();
            }
            result += s.substr(start);
            return XObject::makeString(std::move(result));
        };

        // replace_first(str, old, new) → string — replace only first occurrence
        t["replace_first"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("replace_first", 3, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString() || !args[2].isString())
                throw TypeError("replace_first() expects three strings", line);
            std::string s = args[0].asString();
            const std::string &oldStr = args[1].asString();
            const std::string &newStr = args[2].asString();
            size_t pos = s.find(oldStr);
            if (pos != std::string::npos)
                s.replace(pos, oldStr.size(), newStr);
            return XObject::makeString(std::move(s));
        };

        // starts_with(str, prefix) → bool
        t["starts_with"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("starts_with", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("starts_with() expects two strings", line);
            const std::string &s = args[0].asString();
            const std::string &prefix = args[1].asString();
            return XObject::makeBool(s.size() >= prefix.size() &&
                                     s.compare(0, prefix.size(), prefix) == 0);
        };

        // ends_with(str, suffix) → bool
        t["ends_with"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("ends_with", 2, (int)args.size(), line);
            if (!args[0].isString() || !args[1].isString())
                throw TypeError("ends_with() expects two strings", line);
            const std::string &s = args[0].asString();
            const std::string &suffix = args[1].asString();
            return XObject::makeBool(s.size() >= suffix.size() &&
                                     s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0);
        };

        // index_of(str_or_list, sub_or_val) → int (-1 if not found)
        // Polymorphic: works on strings and lists
        t["index_of"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("index_of", 2, (int)args.size(), line);
            if (args[0].isString())
            {
                if (!args[1].isString())
                    throw TypeError("index_of() on string expects a string argument", line);
                size_t pos = args[0].asString().find(args[1].asString());
                return XObject::makeInt(pos == std::string::npos ? -1 : (int64_t)pos);
            }
            if (args[0].isList())
            {
                const auto &list = args[0].asList();
                for (size_t i = 0; i < list.size(); ++i)
                    if (list[i].equals(args[1]))
                        return XObject::makeInt((int64_t)i);
                return XObject::makeInt(-1);
            }
            throw TypeError("index_of() expects a string or list as first argument", line);
        };

        // substr(str, start, len) → string
        t["substr"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("substr", 3, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("substr() expects a string as first argument", line);
            if (!args[1].isNumber() || !args[2].isNumber())
                throw TypeError("substr() expects numeric start and length", line);
            const std::string &s = args[0].asString();
            int start = (int)args[1].asNumber();
            int len = (int)args[2].asNumber();
            if (start < 0)
                start += (int)s.size();
            if (start < 0)
                start = 0;
            if (start >= (int)s.size())
                return XObject::makeString("");
            if (len < 0)
                len = 0;
            return XObject::makeString(s.substr(start, len));
        };

        // char_at(str, n) → string (single character)
        t["char_at"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("char_at", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("char_at() expects a string as first argument", line);
            if (!args[1].isNumber())
                throw TypeError("char_at() expects a numeric index", line);
            const std::string &s = args[0].asString();
            int idx = (int)args[1].asNumber();
            if (idx < 0)
                idx += (int)s.size();
            if (idx < 0 || idx >= (int)s.size())
                throw IndexError("char_at() index " + std::to_string(idx) + " out of range (size " +
                                     std::to_string(s.size()) + ")",
                                 line);
            return XObject::makeString(std::string(1, s[idx]));
        };

        // repeat(str, n) → string
        t["repeat"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("repeat", 2, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("repeat() expects a string as first argument", line);
            if (!args[1].isNumber())
                throw TypeError("repeat() expects a numeric count", line);
            const std::string &s = args[0].asString();
            int n = (int)args[1].asNumber();
            if (n < 0)
                n = 0;
            std::string result;
            result.reserve(s.size() * n);
            for (int i = 0; i < n; ++i)
                result += s;
            return XObject::makeString(std::move(result));
        };

        // pad_start(str, n, ch) → string
        t["pad_start"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("pad_start", 3, (int)args.size(), line);
            if (!args[0].isString() || !args[2].isString())
                throw TypeError("pad_start() expects strings for first and third args", line);
            if (!args[1].isNumber())
                throw TypeError("pad_start() expects a numeric target length", line);
            std::string s = args[0].asString();
            int n = (int)args[1].asNumber();
            const std::string &ch = args[2].asString();
            if (ch.empty())
                return XObject::makeString(std::move(s));
            while ((int)s.size() < n)
                s = ch + s;
            // Trim to exact length if padding char is multi-char
            if ((int)s.size() > n)
                s = s.substr(s.size() - n);
            return XObject::makeString(std::move(s));
        };

        // pad_end(str, n, ch) → string
        t["pad_end"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("pad_end", 3, (int)args.size(), line);
            if (!args[0].isString() || !args[2].isString())
                throw TypeError("pad_end() expects strings for first and third args", line);
            if (!args[1].isNumber())
                throw TypeError("pad_end() expects a numeric target length", line);
            std::string s = args[0].asString();
            int n = (int)args[1].asNumber();
            const std::string &ch = args[2].asString();
            if (ch.empty())
                return XObject::makeString(std::move(s));
            while ((int)s.size() < n)
                s += ch;
            // Trim to exact length
            if ((int)s.size() > n)
                s = s.substr(0, n);
            return XObject::makeString(std::move(s));
        };

        // reverse(str_or_list) → reversed copy
        // Polymorphic: works on strings and lists
        t["reverse"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("reverse", 1, (int)args.size(), line);
            if (args[0].isString())
            {
                std::string s = args[0].asString();
                std::reverse(s.begin(), s.end());
                return XObject::makeString(std::move(s));
            }
            if (args[0].isList())
            {
                XList list = args[0].asList();
                std::reverse(list.begin(), list.end());
                return XObject::makeList(std::move(list));
            }
            throw TypeError("reverse() expects a string or list", line);
        };

        // count(str_or_list, sub_or_val) → int
        // Polymorphic: works on strings and lists
        t["count"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("count", 2, (int)args.size(), line);
            if (args[0].isString())
            {
                if (!args[1].isString())
                    throw TypeError("count() on string expects a string argument", line);
                const std::string &s = args[0].asString();
                const std::string &sub = args[1].asString();
                if (sub.empty())
                    return XObject::makeInt(0);
                int64_t cnt = 0;
                size_t pos = 0;
                while ((pos = s.find(sub, pos)) != std::string::npos)
                {
                    ++cnt;
                    pos += sub.size();
                }
                return XObject::makeInt(cnt);
            }
            if (args[0].isList())
            {
                int64_t cnt = 0;
                for (const auto &elem : args[0].asList())
                    if (elem.equals(args[1]))
                        ++cnt;
                return XObject::makeInt(cnt);
            }
            throw TypeError("count() expects a string or list as first argument", line);
        };

        // is_empty(str_or_list_or_map) → bool
        t["is_empty"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("is_empty", 1, (int)args.size(), line);
            if (args[0].isString())
                return XObject::makeBool(args[0].asString().empty());
            if (args[0].isList())
                return XObject::makeBool(args[0].asList().empty());
            if (args[0].isTuple())
                return XObject::makeBool(args[0].asTuple().empty());
            if (args[0].isMap())
                return XObject::makeBool(args[0].asMap().size() == 0);
            if (args[0].isSet())
                return XObject::makeBool(args[0].asSet().size() == 0);
            if (args[0].isFrozenSet())
                return XObject::makeBool(args[0].asFrozenSet().size() == 0);
            throw TypeError("is_empty() expects a string, list, tuple, map, set, or frozen_set", line);
        };

        // is_numeric(str) → bool
        t["is_numeric"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("is_numeric", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("is_numeric() expects a string", line);
            const std::string &s = args[0].asString();
            if (s.empty())
                return XObject::makeBool(false);
            try
            {
                size_t pos;
                std::stod(s, &pos);
                return XObject::makeBool(pos == s.size());
            }
            catch (...)
            {
                return XObject::makeBool(false);
            }
        };

        // is_alpha(str) → bool
        t["is_alpha"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("is_alpha", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("is_alpha() expects a string", line);
            const std::string &s = args[0].asString();
            if (s.empty())
                return XObject::makeBool(false);
            for (char c : s)
                if (!std::isalpha((unsigned char)c))
                    return XObject::makeBool(false);
            return XObject::makeBool(true);
        };

        // lines(str) → list of strings
        t["lines"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("lines", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("lines() expects a string", line);
            const std::string &s = args[0].asString();
            XList result;
            std::string current;
            for (size_t i = 0; i < s.size(); ++i)
            {
                if (s[i] == '\r')
                {
                    result.push_back(XObject::makeString(current));
                    current.clear();
                    if (i + 1 < s.size() && s[i + 1] == '\n')
                        ++i; // skip \r\n
                }
                else if (s[i] == '\n')
                {
                    result.push_back(XObject::makeString(current));
                    current.clear();
                }
                else
                {
                    current += s[i];
                }
            }
            result.push_back(XObject::makeString(current)); // last line
            return XObject::makeList(std::move(result));
        };

        // to_chars(str) → list of single-character strings
        t["to_chars"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("to_chars", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("to_chars() expects a string", line);
            const std::string &s = args[0].asString();
            XList result;
            for (char c : s)
                result.push_back(XObject::makeString(std::string(1, c)));
            return XObject::makeList(std::move(result));
        };
    }

} // namespace xell
