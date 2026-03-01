#pragma once

// =============================================================================
// Hash Algorithms — Pure, standalone hash functions for Xell
// =============================================================================
//
// This module provides raw hash algorithms that operate on byte sequences.
// They are used internally by XObjectHash for set/map hashing, and are also
// exposed to Xell users via the `hash()` builtin + importable `bring` modules.
//
// Algorithms:
//   - FNV-1a   (default)  : Fast, excellent distribution, simple
//   - DJB2                : Classic, simple, good for strings
//   - MurmurHash3         : 64-bit finalizer, excellent avalanche, fast
//   - SipHash-2-4         : Keyed PRF, DoS-resistant, used by Python/Rust
//
// Specialization helpers:
//   - hash_int(int64_t)   : Canonical integer hashing
//   - hash_float(double)  : IEEE 754-aware float hashing (normalizes ±0, NaN)
//   - hash_string(str)    : Convenience for std::string
//
// Seeded variants:
//   - fnv1a_seeded(data, len, seed)
//   - djb2_seeded(data, len, seed)
//
// =============================================================================

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <string>

namespace xell
{
    namespace hash
    {

        // ====================================================================
        // FNV-1a (Fowler–Noll–Vo) — 64-bit
        // ====================================================================
        // Excellent avalanche properties, fast, widely used.
        // Reference: http://www.isthe.com/chongo/tech/comp/fnv/

        constexpr size_t FNV_OFFSET_BASIS = 14695981039346656037ULL;
        constexpr size_t FNV_PRIME = 1099511628211ULL;

        inline size_t fnv1a(const void *data, size_t len)
        {
            const uint8_t *bytes = static_cast<const uint8_t *>(data);
            size_t h = FNV_OFFSET_BASIS;
            for (size_t i = 0; i < len; i++)
            {
                h ^= static_cast<size_t>(bytes[i]);
                h *= FNV_PRIME;
            }
            return h;
        }

        // Seeded FNV-1a — seed mixed into initial state
        inline size_t fnv1a_seeded(const void *data, size_t len, size_t seed)
        {
            const uint8_t *bytes = static_cast<const uint8_t *>(data);
            // Mix seed into the offset basis via XOR + multiply
            size_t h = FNV_OFFSET_BASIS ^ seed;
            h *= FNV_PRIME;
            for (size_t i = 0; i < len; i++)
            {
                h ^= static_cast<size_t>(bytes[i]);
                h *= FNV_PRIME;
            }
            return h;
        }

        // ====================================================================
        // DJB2 — Daniel J. Bernstein's hash
        // ====================================================================
        // Simple and fast, good for string hashing.
        // Reference: http://www.cse.yorku.ca/~oz/hash.html

        inline size_t djb2(const void *data, size_t len)
        {
            const uint8_t *bytes = static_cast<const uint8_t *>(data);
            size_t h = 5381;
            for (size_t i = 0; i < len; i++)
            {
                h = ((h << 5) + h) + static_cast<size_t>(bytes[i]); // h * 33 + c
            }
            return h;
        }

        // Seeded DJB2 — seed replaces the initial basis
        inline size_t djb2_seeded(const void *data, size_t len, size_t seed)
        {
            const uint8_t *bytes = static_cast<const uint8_t *>(data);
            size_t h = 5381 ^ seed;
            for (size_t i = 0; i < len; i++)
            {
                h = ((h << 5) + h) + static_cast<size_t>(bytes[i]);
            }
            return h;
        }

        // ====================================================================
        // MurmurHash3 — 64-bit finalizer (fmix64)
        // ====================================================================
        // The finalizer/mixer from Austin Appleby's MurmurHash3.
        // Provides excellent avalanche on a pre-mixed 64-bit value.
        // Useful for hashing integers and as a secondary mixer.
        // Reference: https://github.com/aappleby/smhasher/blob/master/src/MurmurHash3.cpp
        //
        // For full data streams, we apply fmix64 on a per-block FNV-like accumulation
        // to get a MurmurHash3-quality output with arbitrary-length input.

        inline uint64_t murmur3_fmix64(uint64_t k)
        {
            k ^= k >> 33;
            k *= 0xff51afd7ed558ccdULL;
            k ^= k >> 33;
            k *= 0xc4ceb9fe1a85ec53ULL;
            k ^= k >> 33;
            return k;
        }

        inline size_t murmur3(const void *data, size_t len)
        {
            const uint8_t *bytes = static_cast<const uint8_t *>(data);
            // Process 8-byte blocks
            const size_t nblocks = len / 8;
            uint64_t h = 0; // seed = 0

            const uint64_t c1 = 0x87c37b91114253d5ULL;
            const uint64_t c2 = 0x4cf5ad432745937fULL;

            for (size_t i = 0; i < nblocks; i++)
            {
                uint64_t k;
                std::memcpy(&k, bytes + i * 8, 8);

                k *= c1;
                k = (k << 31) | (k >> 33); // ROTL64(k, 31)
                k *= c2;

                h ^= k;
                h = (h << 27) | (h >> 37); // ROTL64(h, 27)
                h = h * 5 + 0x52dce729;
            }

            // Tail: remaining bytes
            const uint8_t *tail = bytes + nblocks * 8;
            uint64_t k = 0;
            switch (len & 7)
            {
            case 7:
                k ^= static_cast<uint64_t>(tail[6]) << 48;
                [[fallthrough]];
            case 6:
                k ^= static_cast<uint64_t>(tail[5]) << 40;
                [[fallthrough]];
            case 5:
                k ^= static_cast<uint64_t>(tail[4]) << 32;
                [[fallthrough]];
            case 4:
                k ^= static_cast<uint64_t>(tail[3]) << 24;
                [[fallthrough]];
            case 3:
                k ^= static_cast<uint64_t>(tail[2]) << 16;
                [[fallthrough]];
            case 2:
                k ^= static_cast<uint64_t>(tail[1]) << 8;
                [[fallthrough]];
            case 1:
                k ^= static_cast<uint64_t>(tail[0]);
                k *= c1;
                k = (k << 31) | (k >> 33);
                k *= c2;
                h ^= k;
            }

            // Finalization
            h ^= static_cast<uint64_t>(len);
            h = murmur3_fmix64(h);
            return static_cast<size_t>(h);
        }

        // ====================================================================
        // SipHash-2-4 — Keyed pseudorandom function
        // ====================================================================
        // DoS-resistant, cryptographically strong PRF. Used by Python 3.4+,
        // Rust's HashMap, and many other production hash tables.
        // Reference: https://131002.net/siphash/siphash.pdf
        //
        // Default key: deterministic (same per process). For randomized
        // hashing, pass a random 128-bit key via siphash_keyed().

        namespace detail
        {
            inline uint64_t rotl64(uint64_t x, int b) { return (x << b) | (x >> (64 - b)); }

            inline void sipround(uint64_t &v0, uint64_t &v1, uint64_t &v2, uint64_t &v3)
            {
                v0 += v1;
                v1 = rotl64(v1, 13);
                v1 ^= v0;
                v0 = rotl64(v0, 32);
                v2 += v3;
                v3 = rotl64(v3, 16);
                v3 ^= v2;
                v0 += v3;
                v3 = rotl64(v3, 21);
                v3 ^= v0;
                v2 += v1;
                v1 = rotl64(v1, 17);
                v1 ^= v2;
                v2 = rotl64(v2, 32);
            }
        } // namespace detail

        // SipHash-2-4 with explicit 128-bit key (k0, k1)
        inline size_t siphash_keyed(const void *data, size_t len, uint64_t k0, uint64_t k1)
        {
            const uint8_t *bytes = static_cast<const uint8_t *>(data);

            uint64_t v0 = 0x736f6d6570736575ULL ^ k0;
            uint64_t v1 = 0x646f72616e646f6dULL ^ k1;
            uint64_t v2 = 0x6c7967656e657261ULL ^ k0;
            uint64_t v3 = 0x7465646279746573ULL ^ k1;

            // Process 8-byte blocks
            const size_t nblocks = len / 8;
            for (size_t i = 0; i < nblocks; i++)
            {
                uint64_t m;
                std::memcpy(&m, bytes + i * 8, 8);
                v3 ^= m;
                detail::sipround(v0, v1, v2, v3);
                detail::sipround(v0, v1, v2, v3);
                v0 ^= m;
            }

            // Tail: remaining bytes + length encoding
            uint64_t m = static_cast<uint64_t>(len & 0xff) << 56;
            const uint8_t *tail = bytes + nblocks * 8;
            switch (len & 7)
            {
            case 7:
                m |= static_cast<uint64_t>(tail[6]) << 48;
                [[fallthrough]];
            case 6:
                m |= static_cast<uint64_t>(tail[5]) << 40;
                [[fallthrough]];
            case 5:
                m |= static_cast<uint64_t>(tail[4]) << 32;
                [[fallthrough]];
            case 4:
                m |= static_cast<uint64_t>(tail[3]) << 24;
                [[fallthrough]];
            case 3:
                m |= static_cast<uint64_t>(tail[2]) << 16;
                [[fallthrough]];
            case 2:
                m |= static_cast<uint64_t>(tail[1]) << 8;
                [[fallthrough]];
            case 1:
                m |= static_cast<uint64_t>(tail[0]);
            }

            v3 ^= m;
            detail::sipround(v0, v1, v2, v3);
            detail::sipround(v0, v1, v2, v3);
            v0 ^= m;

            // Finalization
            v2 ^= 0xff;
            detail::sipround(v0, v1, v2, v3);
            detail::sipround(v0, v1, v2, v3);
            detail::sipround(v0, v1, v2, v3);
            detail::sipround(v0, v1, v2, v3);

            return static_cast<size_t>(v0 ^ v1 ^ v2 ^ v3);
        }

        // Default SipHash-2-4 with deterministic key
        // (Stable within a process, good for general hashing)
        inline size_t siphash(const void *data, size_t len)
        {
            // Deterministic default key (inspired by Python's default)
            constexpr uint64_t default_k0 = 0x0706050403020100ULL;
            constexpr uint64_t default_k1 = 0x0f0e0d0c0b0a0908ULL;
            return siphash_keyed(data, len, default_k0, default_k1);
        }

        // ====================================================================
        // Hash combiner — for composite types (tuples, etc.)
        // ====================================================================
        // Based on boost::hash_combine. Mixes two hash values together.

        inline size_t hash_combine(size_t h1, size_t h2)
        {
            h1 ^= h2 + 0x9e3779b97f4a7c15ULL + (h1 << 12) + (h1 >> 4);
            return h1;
        }

        // ====================================================================
        // Type-specialized helpers
        // ====================================================================
        // Direct hashing for native types, avoiding unnecessary memcpy.

        // Integer hash — applies murmur3 fmix64 for excellent distribution
        // Handles the common case of hashing numeric indices, counters, etc.
        inline size_t hash_int(int64_t val)
        {
            uint64_t bits;
            std::memcpy(&bits, &val, sizeof(bits));
            return static_cast<size_t>(murmur3_fmix64(bits));
        }

        // Float hash — IEEE 754-aware
        // Normalizes: +0.0 == -0.0 → same hash, NaN → canonical hash
        inline size_t hash_float(double val)
        {
            // Normalize -0.0 to +0.0 (IEEE 754: they compare equal)
            if (val == 0.0)
                val = 0.0;

            uint64_t bits;
            std::memcpy(&bits, &val, sizeof(bits));

            // Canonical NaN: all NaN bit patterns hash the same
            // NaN exponent = all 1s (0x7FF), mantissa != 0
            if ((bits & 0x7FF0000000000000ULL) == 0x7FF0000000000000ULL &&
                (bits & 0x000FFFFFFFFFFFFFULL) != 0)
            {
                bits = 0x7FF8000000000000ULL; // canonical quiet NaN
            }

            return static_cast<size_t>(murmur3_fmix64(bits));
        }

        // String hash — convenience for std::string using default algorithm
        inline size_t hash_string(const std::string &s)
        {
            return fnv1a(s.c_str(), s.size());
        }

        // String hash with algorithm choice
        inline size_t hash_string(const std::string &s, size_t (*algo)(const void *, size_t))
        {
            return algo(s.c_str(), s.size());
        }

        // ====================================================================
        // Convenience: hash function pointer type
        // ====================================================================

        using HashFn = size_t (*)(const void *, size_t);

    } // namespace hash
} // namespace xell
