#pragma once

// =============================================================================
// XObject — Xell's runtime value type
// =============================================================================
//
// Design:
//   Every Xell value at runtime is an XObject. An XObject is a lightweight
//   handle (single pointer, 8 bytes) to a heap-allocated control block (XData)
//   that holds: reference count, type tag, and a void* to the actual payload.
//
//   Copying an XObject is cheap — just a pointer copy + ref count bump.
//   Destruction decrements the ref count; when it hits zero the payload and
//   control block are freed.
//
// Why not std::variant?
//   std::visit has non-trivial dispatch overhead (jump table or switch).
//   Our approach: one enum check + static_cast. Zero abstraction cost.
//
// Why not std::shared_ptr?
//   shared_ptr's control block is heavier (weak count, deleter, allocator).
//   We use a single atomic<uint32_t> for thread-safe ref counting with
//   minimal overhead — no weak refs, no virtual deleter.
//
// Memory layout:
//
//   XObject  (stack, 8 bytes)
//   ┌──────────┐
//   │  data_*  │──→  XData  (heap)
//   └──────────┘     ┌─────────────────────────┐
//                    │ refCount (atomic uint32) │
//                    │ type     (XType)         │
//                    │ payload  (void*)         │──→ actual data
//                    └─────────────────────────┘
//
// =============================================================================

#include <string>
#include <vector>
#include <memory>
#include <atomic>
#include <cstdint>
#include <cmath>
#include <functional>

// Modular hash algorithms and ordered hash table
#include "../hash/hash_algorithm.hpp"
#include "../xobject/ordered_hash_table.hpp"

namespace xell
{

    // Forward declarations
    class XObject;
    struct Stmt;
    struct Expr;
    class Environment;

    // Forward declarations for hash-table-based types (defined after XObject)
    struct XObjectHash;
    struct XObjectEqual;
    struct XSet;
    struct XMap;

    // ========================================================================
    // XComplex — complex number representation (a + bi)
    // ========================================================================

    struct XComplex
    {
        double real;
        double imag;

        XComplex() : real(0.0), imag(0.0) {}
        XComplex(double r, double i) : real(r), imag(i) {}

        XComplex operator+(const XComplex &o) const { return {real + o.real, imag + o.imag}; }
        XComplex operator-(const XComplex &o) const { return {real - o.real, imag - o.imag}; }
        XComplex operator*(const XComplex &o) const
        {
            return {real * o.real - imag * o.imag, real * o.imag + imag * o.real};
        }
        XComplex operator/(const XComplex &o) const
        {
            double denom = o.real * o.real + o.imag * o.imag;
            return {(real * o.real + imag * o.imag) / denom,
                    (imag * o.real - real * o.imag) / denom};
        }
        XComplex operator-() const { return {-real, -imag}; }
        XComplex conjugate() const { return {real, -imag}; }
        double magnitude() const { return std::sqrt(real * real + imag * imag); }
        bool operator==(const XComplex &o) const { return real == o.real && imag == o.imag; }
        bool operator!=(const XComplex &o) const { return !(*this == o); }
    };

    // ========================================================================
    // XType — the type tag enum
    // ========================================================================

    enum class XType : uint8_t
    {
        NONE = 0,
        INT,     // int64_t
        FLOAT,   // double
        COMPLEX, // XComplex (a + bi)
        BOOL,
        STRING,
        LIST,
        TUPLE,
        SET,
        FROZEN_SET, // immutable set: <1, 2, 3>
        MAP,
        FUNCTION,
    };

    /// Human-readable type name for error messages
    const char *xtype_name(XType t);

    // ========================================================================
    // XList, XMap, XFunction — the compound payload types
    // ========================================================================

    /// An ordered list of XObjects (mutable)
    using XList = std::vector<XObject>;

    /// A tuple of XObjects (immutable — enforced at interpreter level)
    using XTuple = std::vector<XObject>;

    /// A user-defined function captured at definition time.
    /// Holds a raw pointer to the AST body (owned by the Program, outlives this).
    /// closureEnv captures the lexical scope for lexical scoping.
    struct XFunction
    {
        std::string name;
        std::vector<std::string> params;
        const std::vector<std::unique_ptr<Stmt>> *body; // non-owning pointer to AST
        Environment *closureEnv;                        // lexical scope at definition

        // Default parameter values (evaluated at call time from AST pointers)
        std::vector<const Expr *> defaults; // nullptr = no default; non-owning AST ptrs

        // Variadic support
        bool isVariadic = false;
        std::string variadicName; // name of the ...rest parameter

        // Lambda single expression (for inline lambdas: x => x * 2)
        const Expr *lambdaSingleExpr = nullptr; // non-owning AST ptr

        // Heap-allocated closure environment for lambdas returned from functions.
        // When set, closureEnv points into this. Shared so copies of the function
        // keep the environment alive.
        std::shared_ptr<Environment> ownedEnv;

        XFunction(std::string name, std::vector<std::string> params,
                  const std::vector<std::unique_ptr<Stmt>> *body,
                  Environment *closureEnv = nullptr)
            : name(std::move(name)), params(std::move(params)),
              body(body), closureEnv(closureEnv) {}
    };

    // ========================================================================
    // XData — the heap-allocated control block
    // ========================================================================

    struct XData
    {
        std::atomic<uint32_t> refCount;
        XType type;
        void *payload; // points to the type-specific data

        XData(XType type, void *payload)
            : refCount(1), type(type), payload(payload) {}

        // No copy/move — always heap allocated, managed by XObject
        XData(const XData &) = delete;
        XData &operator=(const XData &) = delete;
    };

    // ========================================================================
    // XObject — the lightweight value handle
    // ========================================================================

    class XObject
    {
    public:
        // ---- Construction: named factory methods (no implicit conversions) ----

        /// none value (payload is nullptr)
        static XObject makeNone();

        /// integer (heap-allocates an int64_t)
        static XObject makeInt(int64_t value);

        /// float (heap-allocates a double)
        static XObject makeFloat(double value);

        /// number — backward compat: creates FLOAT
        static XObject makeNumber(double value);

        /// complex number (heap-allocates an XComplex)
        static XObject makeComplex(double real, double imag);
        static XObject makeComplex(const XComplex &c);

        /// bool (heap-allocates a bool)
        static XObject makeBool(bool value);

        /// string (heap-allocates a std::string)
        static XObject makeString(const std::string &value);
        static XObject makeString(std::string &&value);

        /// empty list
        static XObject makeList();
        /// list from existing vector
        static XObject makeList(XList &&elements);

        /// tuple from existing vector (immutable after creation)
        static XObject makeTuple(XTuple &&elements);

        /// empty set
        static XObject makeSet();
        /// set from existing XSet
        static XObject makeSet(XSet &&set);

        /// frozen (immutable) set
        static XObject makeFrozenSet();
        static XObject makeFrozenSet(XSet &&set);

        /// empty map
        static XObject makeMap();
        /// map from existing XMap
        static XObject makeMap(XMap &&map);

        /// function (closureEnv = lexical scope at definition site)
        static XObject makeFunction(const std::string &name,
                                    const std::vector<std::string> &params,
                                    const std::vector<std::unique_ptr<Stmt>> *body,
                                    Environment *closureEnv = nullptr);

        // ---- Default constructor → none ----

        XObject();

        // ---- Big Five: ref-counted copy/move ----

        ~XObject();
        XObject(const XObject &other);
        XObject &operator=(const XObject &other);
        XObject(XObject &&other) noexcept;
        XObject &operator=(XObject &&other) noexcept;

        // ---- Type queries ----

        XType type() const;
        bool isNone() const;
        bool isInt() const;
        bool isFloat() const;
        bool isComplex() const;
        bool isNumber() const;  // true for INT or FLOAT (backward compat)
        bool isNumeric() const; // true for INT, FLOAT, or COMPLEX
        bool isBool() const;
        bool isString() const;
        bool isList() const;
        bool isTuple() const;
        bool isSet() const;
        bool isFrozenSet() const;
        bool isMap() const;
        bool isFunction() const;

        // ---- Payload access (unchecked — caller must verify type first) ----

        int64_t asInt() const;
        double asFloat() const;
        const XComplex &asComplex() const;
        double asNumber() const; // returns double for INT or FLOAT (backward compat)
        bool asBool() const;
        const std::string &asString() const;
        std::string &asStringMut();
        const XList &asList() const;
        XList &asListMut();
        const XTuple &asTuple() const;
        const XSet &asSet() const;
        XSet &asSetMut();
        const XSet &asFrozenSet() const; // same payload type but immutable
        const XMap &asMap() const;
        XMap &asMapMut();
        const XFunction &asFunction() const;

        // ---- Truthiness (for if/while conditions) ----
        //   none → false, bool → its value, number → false if 0.0
        //   string → false if empty, list/tuple → false if empty
        //   set/map → false if empty, function → true (always)

        bool truthy() const;

        // ---- Deep clone (creates independent copy of payload) ----

        XObject clone() const;

        // ---- Conversion to string (for print / interpolation) ----

        std::string toString() const;

        // ---- Comparison ----

        bool equals(const XObject &other) const;

        // ---- Debug: ref count (for testing) ----

        uint32_t refCount() const;

        // ---- Debug: global allocation tracking (for leak detection in tests) ----

        static int64_t liveAllocations();
        static void resetAllocationCounter();

    private:
        XData *data_;

        /// Construct from a pre-built XData (takes ownership, refCount already 1)
        explicit XObject(XData *data);

        /// Increment ref count
        void retain();

        /// Decrement ref count, free if zero
        void release();

        /// Free the payload based on type
        static void freePayload(XType type, void *payload);
    };

    // ========================================================================
    // Hash support for XObject
    // ========================================================================

    /// Check if an XObject is of an immutable (hashable) type.
    /// Hashable: none, int, float, complex, bool, string,
    ///           tuple (if all elements hashable), frozen_set (if all elements hashable)
    /// NOT hashable: list, set, map, function
    bool isHashable(const XObject &obj);

    /// Hash a single XObject using a specific algorithm function.
    /// Throws HashError if the object is not hashable.
    size_t hashXObject(const XObject &obj, hash::HashFn hashFn);

    /// Hash functor for XObject — uses FNV-1a (default algorithm).
    struct XObjectHash
    {
        size_t operator()(const XObject &obj) const;
    };

    /// Equality functor for XObject — delegates to XObject::equals().
    struct XObjectEqual
    {
        bool operator()(const XObject &a, const XObject &b) const;
    };

    // ========================================================================
    // XSet — ordered set of unique hashable XObjects
    // ========================================================================

    struct XSet
    {
        using Table = OrderedHashTable<XObject, bool, XObjectHash, XObjectEqual>;
        Table table;

        void add(const XObject &elem);
        bool remove(const XObject &elem);
        bool has(const XObject &elem) const;
        size_t size() const;
        bool empty() const;
        void clear();
        std::vector<XObject> elements() const;
    };

    // ========================================================================
    // XMap — ordered map with XObject keys
    // ========================================================================

    struct XMap
    {
        using Table = OrderedHashTable<XObject, XObject, XObjectHash, XObjectEqual>;
        Table table;

        // XObject key API
        void set(const XObject &key, XObject value);
        XObject *get(const XObject &key);
        const XObject *get(const XObject &key) const;
        bool has(const XObject &key) const;
        bool remove(const XObject &key);

        // String key convenience (backward compat)
        void set(const std::string &key, XObject value);
        XObject *get(const std::string &key);
        const XObject *get(const std::string &key) const;
        bool has(const std::string &key) const;

        size_t size() const;
        bool empty() const;
        void clear();

        Table::Iterator begin() const { return table.begin(); }
    };

} // namespace xell
