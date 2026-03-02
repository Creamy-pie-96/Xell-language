#pragma once

// =============================================================================
// Map builtins — delete_key, get, merge, entries, from_entries
// =============================================================================
//
// NOTE: keys, values, has, set, contains, is_empty, size, remove
//       are already registered in builtins_collection / builtins_list /
//       builtins_string. This file adds map-specific operations only.
// =============================================================================

#include "builtin_registry.hpp"

namespace xell
{

    inline void registerMapBuiltins(BuiltinTable &t)
    {
        // delete_key(map, key) → remove a key, returns the removed value or none
        // Note: named delete_key to avoid conflict with C++ "delete" keyword.
        // The 'remove' builtin already handles map key removal.
        t["delete_key"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("delete_key", 2, (int)args.size(), line);
            if (!args[0].isMap())
                throw TypeError("delete_key() expects a map as first argument", line);
            auto &map = args[0].asMapMut();
            const XObject *val = map.get(args[1]);
            XObject result = val ? val->clone() : XObject::makeNone();
            map.remove(args[1]);
            return result;
        };

        // get(map, key[, default]) → get value or default if missing
        t["get"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 2 || args.size() > 3)
                throw ArityError("get", 2, (int)args.size(), line);
            if (!args[0].isMap())
                throw TypeError("get() expects a map as first argument", line);
            const auto &map = args[0].asMap();
            const XObject *val = map.get(args[1]);
            if (val)
                return *val;
            return args.size() == 3 ? args[2] : XObject::makeNone();
        };

        // merge(map1, map2) → new map with all keys from both, map2 wins on conflict
        t["merge"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("merge", 2, (int)args.size(), line);
            if (!args[0].isMap() || !args[1].isMap())
                throw TypeError("merge() expects two maps", line);
            XMap result;
            // Copy all from map1
            for (auto it = args[0].asMap().begin(); it.valid(); it.next())
                result.set(it.key(), it.value());
            // Copy/overwrite from map2
            for (auto it = args[1].asMap().begin(); it.valid(); it.next())
                result.set(it.key(), it.value());
            return XObject::makeMap(std::move(result));
        };

        // entries(map) → list of [key, value] pairs
        t["entries"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("entries", 1, (int)args.size(), line);
            if (!args[0].isMap())
                throw TypeError("entries() expects a map", line);
            XList result;
            for (auto it = args[0].asMap().begin(); it.valid(); it.next())
            {
                XList pair;
                pair.push_back(it.key());
                pair.push_back(it.value());
                result.push_back(XObject::makeList(std::move(pair)));
            }
            return XObject::makeList(std::move(result));
        };

        // from_entries(list) → build map from list of [key, value] pairs
        t["from_entries"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("from_entries", 1, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("from_entries() expects a list of [key, value] pairs", line);
            XMap result;
            for (const auto &item : args[0].asList())
            {
                if (!item.isList() || item.asList().size() != 2)
                    throw TypeError("from_entries() each element must be a [key, value] pair", line);
                const auto &pair = item.asList();
                if (!isHashable(pair[0]))
                    throw HashError("map key must be hashable, got " +
                                        std::string(xtype_name(pair[0].type())),
                                    line);
                result.set(pair[0], pair[1]);
            }
            return XObject::makeMap(std::move(result));
        };
    }

} // namespace xell
