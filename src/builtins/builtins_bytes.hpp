#pragma once

// =============================================================================
// Bytes builtins — encode, decode, byte_at, byte_len, bytes, bytes_from
// =============================================================================

#include "builtin_registry.hpp"
#include <algorithm>

namespace xell
{

    inline void registerBytesBuiltins(BuiltinTable &t)
    {
        // bytes(value) — convert string to bytes, or create empty bytes
        t["bytes"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.empty())
                return XObject::makeBytes(std::string(""));
            if (args.size() != 1)
                throw ArityError("bytes", 1, (int)args.size(), line);
            if (args[0].isString())
                return XObject::makeBytes(args[0].asString());
            if (args[0].isBytes())
                return args[0]; // already bytes
            if (args[0].isList())
            {
                // Convert list of ints to bytes
                std::vector<uint8_t> data;
                for (const auto &elem : args[0].asList())
                {
                    if (!elem.isInt())
                        throw TypeError("bytes() from list requires int elements, got " +
                                            std::string(xtype_name(elem.type())),
                                        line);
                    int64_t val = elem.asInt();
                    if (val < 0 || val > 255)
                        throw TypeError("bytes() element out of range (0-255): " +
                                            std::to_string(val),
                                        line);
                    data.push_back(static_cast<uint8_t>(val));
                }
                return XObject::makeBytes(std::move(data));
            }
            throw TypeError("bytes() expects a string, list of ints, or bytes, got " +
                                std::string(xtype_name(args[0].type())),
                            line);
        };

        // encode(string[, encoding]) — encode string to bytes (UTF-8 default)
        t["encode"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("encode", 1, (int)args.size(), line);
            if (!args[0].isString())
                throw TypeError("encode() expects a string, got " +
                                    std::string(xtype_name(args[0].type())),
                                line);
            // Only UTF-8 encoding supported for now
            return XObject::makeBytes(args[0].asString());
        };

        // decode(bytes[, encoding]) — decode bytes to string (UTF-8 default)
        t["decode"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 1 || args.size() > 2)
                throw ArityError("decode", 1, (int)args.size(), line);
            if (!args[0].isBytes())
                throw TypeError("decode() expects bytes, got " +
                                    std::string(xtype_name(args[0].type())),
                                line);
            const auto &data = args[0].asBytes().data;
            return XObject::makeString(std::string(data.begin(), data.end()));
        };

        // byte_at(bytes, index) — get byte value at index (returns int 0-255)
        t["byte_at"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("byte_at", 2, (int)args.size(), line);
            if (!args[0].isBytes())
                throw TypeError("byte_at() expects bytes as first argument", line);
            if (!args[1].isInt() && !args[1].isFloat())
                throw TypeError("byte_at() expects an integer index", line);

            const auto &data = args[0].asBytes().data;
            int idx = static_cast<int>(args[1].asNumber());
            if (idx < 0)
                idx += static_cast<int>(data.size());
            if (idx < 0 || idx >= static_cast<int>(data.size()))
                throw IndexError("byte index " + std::to_string(idx) + " out of range (size " +
                                     std::to_string(data.size()) + ")",
                                 line);
            return XObject::makeInt(static_cast<int64_t>(data[idx]));
        };

        // byte_len(bytes) — get byte length
        t["byte_len"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("byte_len", 1, (int)args.size(), line);
            if (!args[0].isBytes())
                throw TypeError("byte_len() expects bytes, got " +
                                    std::string(xtype_name(args[0].type())),
                                line);
            return XObject::makeInt(static_cast<int64_t>(args[0].asBytes().data.size()));
        };

        // bytes_concat(a, b) — concatenate two byte strings
        t["bytes_concat"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 2)
                throw ArityError("bytes_concat", 2, (int)args.size(), line);
            if (!args[0].isBytes() || !args[1].isBytes())
                throw TypeError("bytes_concat() expects two bytes arguments", line);
            std::vector<uint8_t> result = args[0].asBytes().data;
            const auto &rhs = args[1].asBytes().data;
            result.insert(result.end(), rhs.begin(), rhs.end());
            return XObject::makeBytes(std::move(result));
        };

        // bytes_slice(bytes, start[, end]) — slice bytes
        t["bytes_slice"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 2 || args.size() > 3)
                throw ArityError("bytes_slice", 2, (int)args.size(), line);
            if (!args[0].isBytes())
                throw TypeError("bytes_slice() expects bytes as first argument", line);

            const auto &data = args[0].asBytes().data;
            int start = static_cast<int>(args[1].asNumber());
            int end = (args.size() == 3) ? static_cast<int>(args[2].asNumber()) : static_cast<int>(data.size());

            if (start < 0)
                start += static_cast<int>(data.size());
            if (end < 0)
                end += static_cast<int>(data.size());
            start = std::max(0, std::min(start, static_cast<int>(data.size())));
            end = std::max(0, std::min(end, static_cast<int>(data.size())));

            if (start >= end)
                return XObject::makeBytes(std::vector<uint8_t>{});

            std::vector<uint8_t> slice(data.begin() + start, data.begin() + end);
            return XObject::makeBytes(std::move(slice));
        };

        // to_bytes(value) — alias for bytes()
        t["to_bytes"] = t["bytes"];
    }

} // namespace xell
