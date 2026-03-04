#pragma once

// =============================================================================
// Hash Algorithms — Pure, standalone hash functions for Xell
// =============================================================================
//
// This module provides raw hash algorithms that operate on byte sequences.
// They are used internally by XObjectHash for set/map hashing, and are also
// exposed to Xell users via the `hash()` / `hashme()` builtins + importable
// `bring` modules.
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
// High-level API:
//   - resolveAlgorithm(name)       : Resolve algorithm name to HashFn pointer
//   - hashRaw(data, len, algo)     : Hash raw bytes with named algorithm
//   - hashTyped(dtype, value, algo): Hash typed value with named algorithm
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
        // SHA-256 — Cryptographic hash for cache invalidation
        // ====================================================================
        // Pure C++ implementation of SHA-256 (FIPS 180-4).
        // Used by the module system for .xelc.hash cache invalidation.
        // Returns a 64-character lowercase hex string.
        //
        // TODO(bytecode): When bytecode/VM is added, consider using this
        // for bytecode integrity verification as well.
        // ====================================================================

        namespace sha256_detail
        {
            inline uint32_t rotr(uint32_t x, int n) { return (x >> n) | (x << (32 - n)); }
            inline uint32_t ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
            inline uint32_t maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
            inline uint32_t sigma0(uint32_t x) { return rotr(x, 2) ^ rotr(x, 13) ^ rotr(x, 22); }
            inline uint32_t sigma1(uint32_t x) { return rotr(x, 6) ^ rotr(x, 11) ^ rotr(x, 25); }
            inline uint32_t gamma0(uint32_t x) { return rotr(x, 7) ^ rotr(x, 18) ^ (x >> 3); }
            inline uint32_t gamma1(uint32_t x) { return rotr(x, 17) ^ rotr(x, 19) ^ (x >> 10); }

            constexpr uint32_t K[64] = {
                0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
                0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
                0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
                0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
                0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
                0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
                0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
                0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
                0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
                0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
                0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
                0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
                0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
                0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
                0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
                0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};
        } // namespace sha256_detail

        /// Compute SHA-256 of arbitrary data, returns 64-char lowercase hex string.
        inline std::string sha256(const void *data, size_t len)
        {
            using namespace sha256_detail;
            const uint8_t *msg = static_cast<const uint8_t *>(data);

            // Initial hash values (first 32 bits of fractional parts of sqrt of first 8 primes)
            uint32_t h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a;
            uint32_t h4 = 0x510e527f, h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;

            // Pre-processing: length in bits
            uint64_t bitLen = static_cast<uint64_t>(len) * 8;

            // Calculate padded length (must be multiple of 64 bytes / 512 bits)
            size_t paddedLen = ((len + 8) / 64 + 1) * 64;
            std::vector<uint8_t> padded(paddedLen, 0);
            std::memcpy(padded.data(), msg, len);
            padded[len] = 0x80; // append bit '1'

            // Append original length in bits as 64-bit big-endian
            for (int i = 0; i < 8; i++)
                padded[paddedLen - 1 - i] = static_cast<uint8_t>(bitLen >> (i * 8));

            // Process each 64-byte block
            for (size_t offset = 0; offset < paddedLen; offset += 64)
            {
                uint32_t w[64];
                for (int i = 0; i < 16; i++)
                {
                    w[i] = (static_cast<uint32_t>(padded[offset + i * 4]) << 24) |
                           (static_cast<uint32_t>(padded[offset + i * 4 + 1]) << 16) |
                           (static_cast<uint32_t>(padded[offset + i * 4 + 2]) << 8) |
                           static_cast<uint32_t>(padded[offset + i * 4 + 3]);
                }
                for (int i = 16; i < 64; i++)
                    w[i] = gamma1(w[i - 2]) + w[i - 7] + gamma0(w[i - 15]) + w[i - 16];

                uint32_t a = h0, b = h1, c = h2, d = h3;
                uint32_t e = h4, f = h5, g = h6, hh = h7;

                for (int i = 0; i < 64; i++)
                {
                    uint32_t t1 = hh + sigma1(e) + ch(e, f, g) + K[i] + w[i];
                    uint32_t t2 = sigma0(a) + maj(a, b, c);
                    hh = g;
                    g = f;
                    f = e;
                    e = d + t1;
                    d = c;
                    c = b;
                    b = a;
                    a = t1 + t2;
                }

                h0 += a;
                h1 += b;
                h2 += c;
                h3 += d;
                h4 += e;
                h5 += f;
                h6 += g;
                h7 += hh;
            }

            // Produce hex string
            uint32_t digest[8] = {h0, h1, h2, h3, h4, h5, h6, h7};
            static const char hex[] = "0123456789abcdef";
            std::string result;
            result.reserve(64);
            for (int i = 0; i < 8; i++)
            {
                for (int j = 28; j >= 0; j -= 4)
                    result += hex[(digest[i] >> j) & 0xf];
            }
            return result;
        }

        /// Convenience: SHA-256 of a std::string
        inline std::string sha256_string(const std::string &s)
        {
            return sha256(s.c_str(), s.size());
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

        // ====================================================================
        // High-level API — resolve algorithm by name, hash typed values
        // ====================================================================
        // These functions provide a clean interface for the builtins layer.
        // They abstract away the raw byte-level details and algorithm dispatch.

        /// Resolve an algorithm name string to a HashFn pointer.
        /// Supported names: "fnv1a" (default), "djb2", "murmur3", "siphash"
        /// Returns nullptr if the name is unknown.
        inline HashFn resolveAlgorithm(const std::string &name)
        {
            if (name == "fnv1a")
                return fnv1a;
            if (name == "djb2")
                return djb2;
            if (name == "murmur3")
                return murmur3;
            if (name == "siphash")
                return siphash;
            return nullptr;
        }

        /// Hash raw bytes with a named algorithm (defaults to "fnv1a").
        inline size_t hashRaw(const void *data, size_t len,
                              const std::string &algo = "fnv1a")
        {
            HashFn fn = resolveAlgorithm(algo);
            if (!fn)
                fn = fnv1a; // fallback
            return fn(data, len);
        }

        /// Hash a typed value with a named algorithm.
        /// dtype: "int", "float", "complex", "string", "bool"
        /// For complex: value is expected to be two doubles packed as {real, imag}.
        inline size_t hashTyped(const std::string &dtype,
                                const void *value, size_t valueSize,
                                const std::string &algo = "fnv1a")
        {
            HashFn fn = resolveAlgorithm(algo);
            if (!fn)
                fn = fnv1a;

            if (dtype == "int" && valueSize == sizeof(int64_t))
            {
                // Use specialized integer hash for fnv1a
                if (fn == fnv1a)
                {
                    int64_t v;
                    std::memcpy(&v, value, sizeof(v));
                    return hash_int(v);
                }
                return fn(value, valueSize);
            }

            if (dtype == "float" && valueSize == sizeof(double))
            {
                if (fn == fnv1a)
                {
                    double d;
                    std::memcpy(&d, value, sizeof(d));
                    return hash_float(d);
                }
                // Normalize -0.0
                double d;
                std::memcpy(&d, value, sizeof(d));
                if (d == 0.0)
                    d = 0.0;
                return fn(&d, sizeof(d));
            }

            if (dtype == "complex" && valueSize == 2 * sizeof(double))
            {
                double parts[2];
                std::memcpy(parts, value, sizeof(parts));
                if (parts[0] == 0.0)
                    parts[0] = 0.0;
                if (parts[1] == 0.0)
                    parts[1] = 0.0;
                size_t h1 = fn(&parts[0], sizeof(double));
                size_t h2 = fn(&parts[1], sizeof(double));
                return hash_combine(h1, h2);
            }

            if (dtype == "string")
                return fn(value, valueSize);

            if (dtype == "bool" && valueSize == 1)
                return fn(value, 1);

            // Fallback: hash raw bytes
            return fn(value, valueSize);
        }

    } // namespace hash
} // namespace xell
