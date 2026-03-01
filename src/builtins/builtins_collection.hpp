#pragma once

// =============================================================================
// Collection builtins — len, push, pop, keys, values, range, set, has,
//                       add, remove, contains, to_set, to_tuple, to_list,
//                       union_set, intersect, diff
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
            throw TypeError("len() expects a string, list, tuple, set, frozen_set, or map", line);
        };

        t["push"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("push", 2, (int)args.size(), line);
            if (args[0].isFrozenSet())
                throw ImmutabilityError("cannot modify a frozen set — frozen sets are immutable", line);
            if (!args[0].isList())
                throw TypeError("push() expects a list as first argument", line);
            args[0].asListMut().push_back(args[1]);
            return XObject::makeNone();
        };

        t["pop"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("pop", 1, (int)args.size(), line);
            if (args[0].isFrozenSet())
                throw ImmutabilityError("cannot modify a frozen set — frozen sets are immutable", line);
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
            for (auto it = args[0].asMap().begin(); it.valid(); it.next())
            {
                result.push_back(it.key());
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
            for (auto it = args[0].asMap().begin(); it.valid(); it.next())
            {
                result.push_back(it.value());
            }
            return XObject::makeList(std::move(result));
        };

        t["range"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            // Check all args are numeric (int or float)
            bool allInt = true;
            for (size_t i = 0; i < args.size(); ++i)
            {
                if (!args[i].isNumber())
                    throw TypeError("range() expects numeric arguments", line);
                if (args[i].isFloat())
                    allInt = false;
            }

            double start = 0, end = 0, step = 1;
            if (args.size() == 1)
            {
                end = args[0].asNumber();
            }
            else if (args.size() == 2)
            {
                start = args[0].asNumber();
                end = args[1].asNumber();
            }
            else if (args.size() == 3)
            {
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
            if (allInt)
            {
                int64_t istart = (int64_t)start, iend = (int64_t)end, istep = (int64_t)step;
                if (istep > 0)
                {
                    for (int64_t i = istart; i < iend; i += istep)
                        result.push_back(XObject::makeInt(i));
                }
                else
                {
                    for (int64_t i = istart; i > iend; i += istep)
                        result.push_back(XObject::makeInt(i));
                }
            }
            else
            {
                if (step > 0)
                {
                    for (double i = start; i < end; i += step)
                        result.push_back(XObject::makeFloat(i));
                }
                else
                {
                    for (double i = start; i > end; i += step)
                        result.push_back(XObject::makeFloat(i));
                }
            }
            return XObject::makeList(std::move(result));
        };

        t["set"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 3)
                throw ArityError("set", 3, (int)args.size(), line);
            if (args[0].isTuple())
                throw ImmutabilityError("cannot modify a tuple — tuples are immutable", line);
            if (args[0].isFrozenSet())
                throw ImmutabilityError("cannot modify a frozen set — frozen sets are immutable", line);
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
                if (!isHashable(args[1]))
                    throw HashError("map key must be hashable (immutable type), got " +
                                        std::string(xtype_name(args[1].type())),
                                    line);
                args[0].asMapMut().set(args[1], args[2]);
                return XObject::makeNone();
            }
            throw TypeError("set() expects a list or map as first argument", line);
        };

        t["has"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("has", 2, (int)args.size(), line);
            if (args[0].isMap())
                return XObject::makeBool(args[0].asMap().has(args[1]));
            if (args[0].isSet())
                return XObject::makeBool(args[0].asSet().has(args[1]));
            if (args[0].isFrozenSet())
                return XObject::makeBool(args[0].asFrozenSet().has(args[1]));
            throw TypeError("has() expects a map, set, or frozen_set as first argument", line);
        };

        // ---- New collection builtins for set/tuple support ----

        t["add"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("add", 2, (int)args.size(), line);
            if (args[0].isFrozenSet())
                throw ImmutabilityError("cannot modify a frozen set — frozen sets are immutable", line);
            if (!args[0].isSet())
                throw TypeError("add() expects a set as first argument", line);
            if (!isHashable(args[1]))
                throw HashError("set elements must be hashable (immutable), got " +
                                    std::string(xtype_name(args[1].type())),
                                line);
            args[0].asSetMut().add(args[1]);
            return XObject::makeNone();
        };

        t["remove"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("remove", 2, (int)args.size(), line);
            if (args[0].isFrozenSet())
                throw ImmutabilityError("cannot modify a frozen set — frozen sets are immutable", line);
            if (args[0].isSet())
                return XObject::makeBool(args[0].asSetMut().remove(args[1]));
            if (args[0].isMap())
                return XObject::makeBool(args[0].asMapMut().remove(args[1]));
            if (args[0].isList())
            {
                if (!args[1].isNumber())
                    throw TypeError("remove() list index must be a number", line);
                int idx = (int)args[1].asNumber();
                auto &list = args[0].asListMut();
                if (idx < 0)
                    idx += (int)list.size();
                if (idx < 0 || idx >= (int)list.size())
                    throw IndexError("list index " + std::to_string(idx) + " out of range", line);
                XObject removed = list[idx];
                list.erase(list.begin() + idx);
                return removed;
            }
            throw TypeError("remove() expects a list, set, or map", line);
        };

        t["contains"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("contains", 2, (int)args.size(), line);
            if (args[0].isSet())
                return XObject::makeBool(args[0].asSet().has(args[1]));
            if (args[0].isMap())
                return XObject::makeBool(args[0].asMap().has(args[1]));
            if (args[0].isList())
            {
                for (const auto &elem : args[0].asList())
                    if (elem.equals(args[1]))
                        return XObject::makeBool(true);
                return XObject::makeBool(false);
            }
            if (args[0].isTuple())
            {
                for (const auto &elem : args[0].asTuple())
                    if (elem.equals(args[1]))
                        return XObject::makeBool(true);
                return XObject::makeBool(false);
            }
            if (args[0].isFrozenSet())
                return XObject::makeBool(args[0].asFrozenSet().has(args[1]));
            throw TypeError("contains() expects a list, tuple, set, frozen_set, or map", line);
        };

        t["to_set"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("to_set", 1, (int)args.size(), line);
            XSet s;
            if (args[0].isList())
            {
                for (const auto &elem : args[0].asList())
                {
                    if (!isHashable(elem))
                        throw HashError("set elements must be hashable, got " +
                                            std::string(xtype_name(elem.type())),
                                        line);
                    s.add(elem);
                }
                return XObject::makeSet(std::move(s));
            }
            if (args[0].isTuple())
            {
                for (const auto &elem : args[0].asTuple())
                {
                    if (!isHashable(elem))
                        throw HashError("set elements must be hashable, got " +
                                            std::string(xtype_name(elem.type())),
                                        line);
                    s.add(elem);
                }
                return XObject::makeSet(std::move(s));
            }
            if (args[0].isFrozenSet())
            {
                // frozen_set elements are already hashable
                for (const auto &elem : args[0].asFrozenSet().elements())
                    s.add(elem);
                return XObject::makeSet(std::move(s));
            }
            throw TypeError("to_set() expects a list, tuple, or frozen_set", line);
        };

        t["to_tuple"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("to_tuple", 1, (int)args.size(), line);
            if (args[0].isList())
            {
                XTuple tup(args[0].asList().begin(), args[0].asList().end());
                return XObject::makeTuple(std::move(tup));
            }
            if (args[0].isTuple())
                return args[0]; // already a tuple
            if (args[0].isSet())
            {
                auto elems = args[0].asSet().elements();
                XTuple tup(elems.begin(), elems.end());
                return XObject::makeTuple(std::move(tup));
            }
            if (args[0].isFrozenSet())
            {
                auto elems = args[0].asFrozenSet().elements();
                XTuple tup(elems.begin(), elems.end());
                return XObject::makeTuple(std::move(tup));
            }
            throw TypeError("to_tuple() expects a list, tuple, set, or frozen_set", line);
        };

        t["to_list"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("to_list", 1, (int)args.size(), line);
            if (args[0].isList())
                return args[0]; // already a list
            if (args[0].isTuple())
            {
                XList list(args[0].asTuple().begin(), args[0].asTuple().end());
                return XObject::makeList(std::move(list));
            }
            if (args[0].isSet())
            {
                auto elems = args[0].asSet().elements();
                XList list(elems.begin(), elems.end());
                return XObject::makeList(std::move(list));
            }
            if (args[0].isFrozenSet())
            {
                auto elems = args[0].asFrozenSet().elements();
                XList list(elems.begin(), elems.end());
                return XObject::makeList(std::move(list));
            }
            throw TypeError("to_list() expects a list, tuple, set, or frozen_set", line);
        };

        t["union_set"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("union_set", 2, (int)args.size(), line);
            auto getElems = [&](const XObject &o) -> std::vector<XObject>
            {
                if (o.isSet())
                    return o.asSet().elements();
                if (o.isFrozenSet())
                    return o.asFrozenSet().elements();
                throw TypeError("union_set() expects sets or frozen_sets", line);
            };
            XSet result;
            for (const auto &elem : getElems(args[0]))
                result.add(elem);
            for (const auto &elem : getElems(args[1]))
                result.add(elem);
            return XObject::makeSet(std::move(result));
        };

        t["intersect"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("intersect", 2, (int)args.size(), line);
            auto getElems = [&](const XObject &o) -> std::vector<XObject>
            {
                if (o.isSet())
                    return o.asSet().elements();
                if (o.isFrozenSet())
                    return o.asFrozenSet().elements();
                throw TypeError("intersect() expects sets or frozen_sets", line);
            };
            auto hasElem = [&](const XObject &o, const XObject &e) -> bool
            {
                if (o.isSet())
                    return o.asSet().has(e);
                if (o.isFrozenSet())
                    return o.asFrozenSet().has(e);
                return false;
            };
            XSet result;
            for (const auto &elem : getElems(args[0]))
            {
                if (hasElem(args[1], elem))
                    result.add(elem);
            }
            return XObject::makeSet(std::move(result));
        };

        t["diff"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("diff", 2, (int)args.size(), line);
            auto getElems = [&](const XObject &o) -> std::vector<XObject>
            {
                if (o.isSet())
                    return o.asSet().elements();
                if (o.isFrozenSet())
                    return o.asFrozenSet().elements();
                throw TypeError("diff() expects sets or frozen_sets", line);
            };
            auto hasElem = [&](const XObject &o, const XObject &e) -> bool
            {
                if (o.isSet())
                    return o.asSet().has(e);
                if (o.isFrozenSet())
                    return o.asFrozenSet().has(e);
                return false;
            };
            XSet result;
            for (const auto &elem : getElems(args[0]))
            {
                if (!hasElem(args[1], elem))
                    result.add(elem);
            }
            return XObject::makeSet(std::move(result));
        };
    }

} // namespace xell
