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
#include <functional>
#include <unordered_map>

namespace xell
{

    // Forward declarations
    class XObject;
    struct Stmt;
    class Environment;

    // ========================================================================
    // XType — the type tag enum
    // ========================================================================

    enum class XType : uint8_t
    {
        NONE = 0,
        NUMBER,
        BOOL,
        STRING,
        LIST,
        MAP,
        FUNCTION,
    };

    /// Human-readable type name for error messages
    const char *xtype_name(XType t);

    // ========================================================================
    // XList, XMap, XFunction — the compound payload types
    // ========================================================================

    /// An ordered list of XObjects
    using XList = std::vector<XObject>;

    /// An ordered map (preserves insertion order).
    /// We use a vector of pairs for ordered iteration + a hash map for O(1) lookup.
    struct XMap
    {
        std::vector<std::pair<std::string, XObject>> entries;
        std::unordered_map<std::string, size_t> index; // key → position in entries

        void set(const std::string &key, XObject value);
        XObject *get(const std::string &key);
        const XObject *get(const std::string &key) const;
        bool has(const std::string &key) const;
        size_t size() const;
    };

    /// A user-defined function captured at definition time.
    /// Holds a raw pointer to the AST body (owned by the Program, outlives this).
    /// closureEnv captures the lexical scope for lexical scoping.
    struct XFunction
    {
        std::string name;
        std::vector<std::string> params;
        const std::vector<std::unique_ptr<Stmt>> *body; // non-owning pointer to AST
        Environment *closureEnv;                        // lexical scope at definition

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

        /// number (heap-allocates a double)
        static XObject makeNumber(double value);

        /// bool (heap-allocates a bool)
        static XObject makeBool(bool value);

        /// string (heap-allocates a std::string)
        static XObject makeString(const std::string &value);
        static XObject makeString(std::string &&value);

        /// empty list
        static XObject makeList();
        /// list from existing vector
        static XObject makeList(XList &&elements);

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
        bool isNumber() const;
        bool isBool() const;
        bool isString() const;
        bool isList() const;
        bool isMap() const;
        bool isFunction() const;

        // ---- Payload access (unchecked — caller must verify type first) ----

        double asNumber() const;
        bool asBool() const;
        const std::string &asString() const;
        std::string &asStringMut();
        const XList &asList() const;
        XList &asListMut();
        const XMap &asMap() const;
        XMap &asMapMut();
        const XFunction &asFunction() const;

        // ---- Truthiness (for if/while conditions) ----
        //   none → false
        //   bool → its value
        //   number → false if 0.0
        //   string → false if empty
        //   list → false if empty
        //   map → false if empty
        //   function → true (always)

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

} // namespace xell
