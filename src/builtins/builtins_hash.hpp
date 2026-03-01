#pragma once

// =============================================================================
// Hash builtins — hash(), is_hashable(), hash_seed()
// =============================================================================
//
// Provides user-facing hash functions. Users can specify the algorithm:
//   hash(value)               — uses FNV-1a (default)
//   hash(value, "djb2")       — uses DJB2
//   hash(value, "murmur3")    — uses MurmurHash3 (64-bit)
//   hash(value, "siphash")    — uses SipHash-2-4
//   is_hashable(value)        — returns true if value can be used as set/map key
//   hash_seed(value, seed)    — FNV-1a with a custom seed (for randomized hashing)
//   hash_seed(value, seed, "djb2") — seeded DJB2
//
// =============================================================================

#include "builtin_registry.hpp"

namespace xell
{

    inline void registerHashBuiltins(BuiltinTable &t)
    {
        // hash(value [, algorithm])
        t["hash"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.empty() || args.size() > 2)
                throw ArityError("hash", 1, (int)args.size(), line);

            if (!isHashable(args[0]))
                throw HashError("cannot hash mutable type '" +
                                    std::string(xtype_name(args[0].type())) + "'",
                                line);

            // Determine algorithm
            hash::HashFn hashFn = hash::fnv1a; // default
            if (args.size() == 2)
            {
                if (!args[1].isString())
                    throw TypeError("hash() algorithm name must be a string", line);
                const std::string &algo = args[1].asString();
                if (algo == "fnv1a")
                    hashFn = hash::fnv1a;
                else if (algo == "djb2")
                    hashFn = hash::djb2;
                else if (algo == "murmur3")
                    hashFn = hash::murmur3;
                else if (algo == "siphash")
                    hashFn = hash::siphash;
                else
                    throw TypeError("unknown hash algorithm: '" + algo +
                                        "' (available: fnv1a, djb2, murmur3, siphash)",
                                    line);
            }

            size_t h = hashXObject(args[0], hashFn);
            return XObject::makeInt(static_cast<int64_t>(h));
        };

        // is_hashable(value)
        t["is_hashable"] = [](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() != 1)
                throw ArityError("is_hashable", 1, (int)args.size(), line);
            return XObject::makeBool(isHashable(args[0]));
        };

        // hash_seed(value, seed [, algorithm])
        // Seeded hashing for randomized / per-session hash tables.
        // seed is a number. Algorithm defaults to FNV-1a.
        t["hash_seed"] = [&t](std::vector<XObject> &args, int line) -> XObject
        {
            if (args.size() < 2 || args.size() > 3)
                throw ArityError("hash_seed", 2, (int)args.size(), line);

            if (!isHashable(args[0]))
                throw HashError("cannot hash mutable type '" +
                                    std::string(xtype_name(args[0].type())) + "'",
                                line);

            if (!args[1].isNumber())
                throw TypeError("hash_seed() seed must be a number", line);

            size_t seed = args[1].isInt()
                              ? static_cast<size_t>(args[1].asInt())
                              : static_cast<size_t>(static_cast<int64_t>(args[1].asNumber()));

            // Determine seeded algorithm
            std::string algo = "fnv1a";
            if (args.size() == 3)
            {
                if (!args[2].isString())
                    throw TypeError("hash_seed() algorithm name must be a string", line);
                algo = args[2].asString();
            }

            // For seeded hashing, we need to get the raw bytes from the XObject
            // and apply the seeded algorithm. We do this type-by-type.
            size_t h = 0;
            const XObject &val = args[0];

            auto applySeeded = [&](const void *data, size_t len) -> size_t
            {
                if (algo == "fnv1a")
                    return hash::fnv1a_seeded(data, len, seed);
                else if (algo == "djb2")
                    return hash::djb2_seeded(data, len, seed);
                else
                    throw TypeError("seeded hashing not supported for algorithm '" + algo +
                                        "' (available: fnv1a, djb2)",
                                    line);
            };

            switch (val.type())
            {
            case XType::NONE:
            {
                const char marker[] = "__xell_none__";
                h = applySeeded(marker, sizeof(marker) - 1);
                break;
            }
            case XType::INT:
            {
                int64_t v = val.asInt();
                h = applySeeded(&v, sizeof(v));
                break;
            }
            case XType::FLOAT:
            {
                double d = val.asFloat();
                if (d == 0.0)
                    d = 0.0;
                h = applySeeded(&d, sizeof(d));
                break;
            }
            case XType::COMPLEX:
            {
                XComplex c = val.asComplex();
                double parts[2] = {c.real, c.imag};
                h = applySeeded(parts, sizeof(parts));
                break;
            }
            case XType::BOOL:
            {
                uint8_t b = val.asBool() ? 1 : 0;
                h = applySeeded(&b, 1);
                break;
            }
            case XType::STRING:
                h = applySeeded(val.asString().c_str(), val.asString().size());
                break;
            case XType::TUPLE:
            {
                // Seed applies to each element, combined together
                const char tseed[] = "__xell_tuple__";
                h = applySeeded(tseed, sizeof(tseed) - 1);
                for (const auto &elem : val.asTuple())
                {
                    // Recursively hash each element with the seeded algo
                    std::vector<XObject> subArgs = {elem, args[1]};
                    if (args.size() == 3)
                        subArgs.push_back(args[2]);
                    XObject subHash = t["hash_seed"](subArgs, line);
                    size_t elemH = static_cast<size_t>(subHash.asInt());
                    h = hash::hash_combine(h, elemH);
                }
                break;
            }
            case XType::FROZEN_SET:
            {
                // XOR-based order-independent seeded hash
                h = 0;
                for (const auto &elem : val.asFrozenSet().elements())
                {
                    std::vector<XObject> subArgs = {elem, args[1]};
                    if (args.size() == 3)
                        subArgs.push_back(args[2]);
                    XObject subHash = t["hash_seed"](subArgs, line);
                    h ^= static_cast<size_t>(subHash.asInt());
                }
                break;
            }
            default:
                throw HashError("cannot hash mutable type '" +
                                    std::string(xtype_name(val.type())) + "'",
                                line);
            }

            return XObject::makeInt(static_cast<int64_t>(h));
        };
    }

} // namespace xell
