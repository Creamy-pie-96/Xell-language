#pragma once

// =============================================================================
// Collection builtins — len, push, pop, keys, values, range, set, has
// =============================================================================

#include "builtin_registry.hpp"

namespace xell
{

    inline void registerCollectionBuiltins(BuiltinTable &t)
    {
        t["len"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("len", 1, (int)args.size(), line);
            auto &obj = args[0];
            if (obj.isString())
                return XObject::makeNumber((double)obj.asString().size());
            if (obj.isList())
                return XObject::makeNumber((double)obj.asList().size());
            if (obj.isMap())
                return XObject::makeNumber((double)obj.asMap().size());
            throw TypeError("len() expects a string, list, or map", line);
        };

        t["push"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("push", 2, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("push() expects a list as first argument", line);
            args[0].asListMut().push_back(args[1]);
            return XObject::makeNone();
        };

        t["pop"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("pop", 1, (int)args.size(), line);
            if (!args[0].isList())
                throw TypeError("pop() expects a list", line);
            auto &list = args[0].asListMut();
            if (list.empty())
                throw IndexError("pop from empty list", line);
            XObject last = list.back();
            list.pop_back();
            return last;
        };

        t["keys"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("keys", 1, (int)args.size(), line);
            if (!args[0].isMap())
                throw TypeError("keys() expects a map", line);
            XList result;
            for (const auto &pair : args[0].asMap().entries)
            {
                result.push_back(XObject::makeString(pair.first));
            }
            return XObject::makeList(std::move(result));
        };

        t["values"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("values", 1, (int)args.size(), line);
            if (!args[0].isMap())
                throw TypeError("values() expects a map", line);
            XList result;
            for (const auto &pair : args[0].asMap().entries)
            {
                result.push_back(pair.second);
            }
            return XObject::makeList(std::move(result));
        };

        t["range"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            double start = 0, end = 0, step = 1;
            if (args.size() == 1)
            {
                if (!args[0].isNumber())
                    throw TypeError("range() expects numeric arguments", line);
                end = args[0].asNumber();
            }
            else if (args.size() == 2)
            {
                if (!args[0].isNumber() || !args[1].isNumber())
                    throw TypeError("range() expects numeric arguments", line);
                start = args[0].asNumber();
                end = args[1].asNumber();
            }
            else if (args.size() == 3)
            {
                if (!args[0].isNumber() || !args[1].isNumber() || !args[2].isNumber())
                    throw TypeError("range() expects numeric arguments", line);
                start = args[0].asNumber();
                end = args[1].asNumber();
                step = args[2].asNumber();
            }
            else
            {
                throw ArityError("range", 1, (int)args.size(), line);
            }
            if (step == 0)
                throw TypeError("range() step cannot be zero", line);

            XList result;
            if (step > 0)
            {
                for (double i = start; i < end; i += step)
                    result.push_back(XObject::makeNumber(i));
            }
            else
            {
                for (double i = start; i > end; i += step)
                    result.push_back(XObject::makeNumber(i));
            }
            return XObject::makeList(std::move(result));
        };

        t["set"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("set", 3, (int)args.size(), line);
            if (args[0].isList())
            {
                if (!args[1].isNumber())
                    throw TypeError("list index must be a number", line);
                int idx = (int)args[1].asNumber();
                auto &list = args[0].asListMut();
                if (idx < 0)
                    idx += (int)list.size();
                if (idx < 0 || idx >= (int)list.size())
                    throw IndexError("list index " + std::to_string(idx) + " out of range", line);
                list[idx] = args[2];
                return XObject::makeNone();
            }
            if (args[0].isMap())
            {
                if (!args[1].isString())
                    throw TypeError("map key must be a string", line);
                args[0].asMapMut().set(args[1].asString(), args[2]);
                return XObject::makeNone();
            }
            throw TypeError("set() expects a list or map as first argument", line);
        };

        t["has"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("has", 2, (int)args.size(), line);
            if (!args[0].isMap())
                throw TypeError("has() expects a map as first argument", line);
            if (!args[1].isString())
                throw TypeError("has() expects a string key as second argument", line);
            return XObject::makeBool(args[0].asMap().has(args[1].asString()));
        };
    }

} // namespace xell
