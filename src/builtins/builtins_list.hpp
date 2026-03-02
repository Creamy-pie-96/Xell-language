#pragma once

// =============================================================================
// List builtins — shift, unshift, insert, remove_val, sort, sort_desc,
//                 slice, flatten, unique, first, last, zip, sum, min, max,
//                 avg, size
// =============================================================================
//
// NOTE: push, pop, range, len, contains, index_of, reverse, count
//       are already registered in builtins_collection.hpp / builtins_string.hpp.
//       (index_of, reverse, count are polymorphic string+list in builtins_string)
//
// NOTE: Higher-order functions (map, filter, reduce, any, all) require
//       interpreter access and are registered separately in interpreter.cpp.
// =============================================================================

#include "builtin_registry.hpp"
#include <algorithm>
#include <numeric>

namespace xell
{

    inline void registerListBuiltins(BuiltinTable &t)
    {
        // shift(list) → remove and return first item
        t["shift"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("shift", 1, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("shift() expects a list", line);
            auto &list = args[0].asListMut();
            if (list.empty())
                throw IndexError("shift from empty list", line);
            XObject first = list.front();
            list.erase(list.begin());
            return first;
        };

        // unshift(list, val) → insert at beginning
        t["unshift"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("unshift", 2, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("unshift() expects a list as first argument", line);
            args[0].asListMut().insert(args[0].asListMut().begin(), args[1]);
            return XObject::makeNone();
        };

        // insert(list, i, val) → insert at specific index
        t["insert"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("insert", 3, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("insert() expects a list as first argument", line);
            if (!args[1].isNumber())
                throw TypeError("insert() expects a numeric index", line);
            auto &list = args[0].asListMut();
            int idx = (int)args[1].asNumber();
            if (idx < 0)
                idx += (int)list.size();
            if (idx < 0)
                idx = 0;
            if (idx > (int)list.size())
                idx = (int)list.size();
            list.insert(list.begin() + idx, args[2]);
            return XObject::makeNone();
        };

        // remove_val(list, val) → remove first occurrence of value, returns bool
        t["remove_val"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("remove_val", 2, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("remove_val() expects a list as first argument", line);
            auto &list = args[0].asListMut();
            for (auto it = list.begin(); it != list.end(); ++it)
            {
                if (it->equals(args[1]))
                {
                    list.erase(it);
                    return XObject::makeBool(true);
                }
            }
            return XObject::makeBool(false);
        };

        // sort(list) → sort ascending (in-place), returns the list
        t["sort"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("sort", 1, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("sort() expects a list", line);
            auto &list = args[0].asListMut();
            std::sort(list.begin(), list.end(), [line](const XObject &a, const XObject &b)
                      {
                if (!a.isNumeric() || !b.isNumeric()) {
                    // Fall back to string comparison for non-numerics
                    return a.toString() < b.toString();
                }
                return a.asNumber() < b.asNumber(); });
            return args[0];
        };

        // sort_desc(list) → sort descending (in-place), returns the list
        t["sort_desc"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("sort_desc", 1, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("sort_desc() expects a list", line);
            auto &list = args[0].asListMut();
            std::sort(list.begin(), list.end(), [line](const XObject &a, const XObject &b)
                      {
                if (!a.isNumeric() || !b.isNumeric()) {
                    return a.toString() > b.toString();
                }
                return a.asNumber() > b.asNumber(); });
            return args[0];
        };

        // slice(list, start[, end]) → sublist
        t["slice"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 2 || args.size() > 3)
                throw ArityError("slice", 2, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("slice() expects a list as first argument", line);
            if (!args[1].isNumber())
                throw TypeError("slice() expects a numeric start index", line);

            const auto &list = args[0].asList();
            int start = (int)args[1].asNumber();
            int end = (args.size() == 3 && args[2].isNumber())
                          ? (int)args[2].asNumber()
                          : (int)list.size();

            if (start < 0)
                start += (int)list.size();
            if (end < 0)
                end += (int)list.size();
            start = std::max(0, std::min(start, (int)list.size()));
            end = std::max(0, std::min(end, (int)list.size()));

            if (start >= end)
                return XObject::makeList(XList{});

            XList result(list.begin() + start, list.begin() + end);
            return XObject::makeList(std::move(result));
        };

        // flatten(list) → flatten one level deep
        t["flatten"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("flatten", 1, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("flatten() expects a list", line);
            XList result;
            for (const auto &item : args[0].asList())
            {
                if (item.isList())
                    for (const auto &sub : item.asList())
                        result.push_back(sub);
                else
                    result.push_back(item);
            }
            return XObject::makeList(std::move(result));
        };

        // unique(list) → new list with duplicates removed (preserves order)
        t["unique"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("unique", 1, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("unique() expects a list", line);
            XList result;
            for (const auto &item : args[0].asList())
            {
                bool found = false;
                for (const auto &existing : result)
                    if (existing.equals(item))
                    {
                        found = true;
                        break;
                    }
                if (!found)
                    result.push_back(item);
            }
            return XObject::makeList(std::move(result));
        };

        // first(list) → first element
        t["first"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("first", 1, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("first() expects a list", line);
            const auto &list = args[0].asList();
            if (list.empty())
                throw IndexError("first() on empty list", line);
            return list.front();
        };

        // last(list) → last element
        t["last"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("last", 1, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("last() expects a list", line);
            const auto &list = args[0].asList();
            if (list.empty())
                throw IndexError("last() on empty list", line);
            return list.back();
        };

        // zip(list1, list2) → list of [a, b] pairs
        t["zip"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("zip", 2, (int)args.size(), line);
            if (!args[0].isList() || !args[1].isList())
                throw TypeError("zip() expects two lists", line);
            const auto &a = args[0].asList();
            const auto &b = args[1].asList();
            size_t len = std::min(a.size(), b.size());
            XList result;
            for (size_t i = 0; i < len; ++i)
            {
                XList pair;
                pair.push_back(a[i]);
                pair.push_back(b[i]);
                result.push_back(XObject::makeList(std::move(pair)));
            }
            return XObject::makeList(std::move(result));
        };

        // sum(list) → sum of all numbers
        t["sum"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("sum", 1, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("sum() expects a list", line);
            const auto &list = args[0].asList();
            // Check if any are floats/complex
            bool hasFloat = false;
            bool hasComplex = false;
            for (const auto &item : list)
            {
                if (!item.isNumeric())
                    throw TypeError("sum() expects a list of numbers", line);
                if (item.isComplex())
                    hasComplex = true;
                else if (item.isFloat())
                    hasFloat = true;
            }
            if (hasComplex)
            {
                double real = 0, imag = 0;
                for (const auto &item : list)
                {
                    if (item.isComplex())
                    {
                        real += item.asComplex().real;
                        imag += item.asComplex().imag;
                    }
                    else
                    {
                        real += item.asNumber();
                    }
                }
                return XObject::makeComplex(real, imag);
            }
            if (hasFloat)
            {
                double total = 0;
                for (const auto &item : list)
                    total += item.asNumber();
                return XObject::makeFloat(total);
            }
            int64_t total = 0;
            for (const auto &item : list)
                total += item.asInt();
            return XObject::makeInt(total);
        };

        // min(list) or min(a, b) → smallest value
        t["min"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() == 1)
            {
                // min(list)
                if (!args[0].isList())
                    throw TypeError("min() with one argument expects a list", line);
                const auto &list = args[0].asList();
                if (list.empty())
                    throw IndexError("min() on empty list", line);
                const XObject *best = &list[0];
                for (size_t i = 1; i < list.size(); ++i)
                {
                    if (!list[i].isNumeric())
                        throw TypeError("min() expects numeric elements", line);
                    if (list[i].asNumber() < (*best).asNumber())
                        best = &list[i];
                }
                return *best;
            }
            if (args.size() == 2)
            {
                // min(a, b)
                if (!args[0].isNumeric() || !args[1].isNumeric())
                    throw TypeError("min() expects numbers", line);
                return args[0].asNumber() <= args[1].asNumber() ? args[0] : args[1];
            }
            throw ArityError("min", 1, (int)args.size(), line);
        };

        // max(list) or max(a, b) → largest value
        t["max"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() == 1)
            {
                // max(list)
                if (!args[0].isList())
                    throw TypeError("max() with one argument expects a list", line);
                const auto &list = args[0].asList();
                if (list.empty())
                    throw IndexError("max() on empty list", line);
                const XObject *best = &list[0];
                for (size_t i = 1; i < list.size(); ++i)
                {
                    if (!list[i].isNumeric())
                        throw TypeError("max() expects numeric elements", line);
                    if (list[i].asNumber() > (*best).asNumber())
                        best = &list[i];
                }
                return *best;
            }
            if (args.size() == 2)
            {
                // max(a, b)
                if (!args[0].isNumeric() || !args[1].isNumeric())
                    throw TypeError("max() expects numbers", line);
                return args[0].asNumber() >= args[1].asNumber() ? args[0] : args[1];
            }
            throw ArityError("max", 1, (int)args.size(), line);
        };

        // avg(list) → average (always returns float)
        t["avg"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("avg", 1, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("avg() expects a list", line);
            const auto &list = args[0].asList();
            if (list.empty())
                throw IndexError("avg() on empty list", line);
            double total = 0;
            for (const auto &item : list)
            {
                if (!item.isNumeric())
                    throw TypeError("avg() expects a list of numbers", line);
                total += item.asNumber();
            }
            return XObject::makeFloat(total / (double)list.size());
        };

        // size(collection) → int — alias for len, works on all collections
        t["size"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("size", 1, (int)args.size(), line);
            auto &obj = args[0];
            if (obj.isString())
                return XObject::makeInt((int64_t)obj.asString().size());
            if (obj.isList())
                return XObject::makeInt((int64_t)obj.asList().size());
            if (obj.isTuple())
                return XObject::makeInt((int64_t)obj.asTuple().size());
            if (obj.isSet())
                return XObject::makeInt((int64_t)obj.asSet().size());
            if (obj.isFrozenSet())
                return XObject::makeInt((int64_t)obj.asFrozenSet().size());
            if (obj.isMap())
                return XObject::makeInt((int64_t)obj.asMap().size());
            if (obj.isBytes())
                return XObject::makeInt((int64_t)obj.asBytes().data.size());
            throw TypeError("size() expects a collection type", line);
        };
    }

} // namespace xell
