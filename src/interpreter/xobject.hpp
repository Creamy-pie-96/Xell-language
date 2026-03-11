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
#include <thread>
#include <mutex>
#include <condition_variable>

// Modular hash algorithms and ordered hash table
#include "../hash/hash_algorithm.hpp"
#include "../xobject/ordered_hash_table.hpp"
#include "../common/access_level.hpp"

// Module object type
// Forward declaration — XModule is defined in module/xmodule.hpp
// (included in xobject.cpp where the full type is needed)
namespace xell
{
    struct XModule;
}

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
        ENUM,       // enum type (stored as map of names → values)
        BYTES,      // raw binary data (vector<uint8_t>)
        GENERATOR,  // lazy generator (thread-based coroutine)
        STRUCT_DEF, // struct type definition (XStructDef)
        INSTANCE,   // struct/class instance (XInstance)
        MODULE,     // module object (XModule)
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

        // Generator/async flags (set by parser/interpreter)
        bool isGenerator = false; // true if function contains yield
        bool isAsync = false;     // true for async fn

        // Type annotations for overload resolution (e.g. "str", "int", "bool")
        std::vector<std::string> typeAnnotations; // one per param, empty = dynamic

        // Overloaded variants of this function (same name, different signature)
        std::vector<XObject> overloads;

        XFunction(std::string name, std::vector<std::string> params,
                  const std::vector<std::unique_ptr<Stmt>> *body,
                  Environment *closureEnv = nullptr)
            : name(std::move(name)), params(std::move(params)),
              body(body), closureEnv(closureEnv) {}
    };

    // ========================================================================
    // XEnum — enumeration with named members and integer values
    // ========================================================================

    struct XEnum
    {
        std::string name;                                 // enum type name
        std::vector<std::string> memberNames;             // ordered member names
        std::unordered_map<std::string, XObject> members; // name → value

        XEnum() = default;
        XEnum(std::string name) : name(std::move(name)) {}
    };

    // ========================================================================
    // XBytes — raw binary data
    // ========================================================================

    struct XBytes
    {
        std::vector<uint8_t> data;

        XBytes() = default;
        explicit XBytes(std::vector<uint8_t> d) : data(std::move(d)) {}
        explicit XBytes(const std::string &s) : data(s.begin(), s.end()) {}
    };

    // ========================================================================
    // XGenerator — lazy generator (thread-based coroutine)
    // ========================================================================
    //
    // Uses a separate thread + mutex/condition_variable to implement
    // suspend/resume semantics for yield. The generator body runs in
    // a worker thread that blocks at each yield point until next() is called.
    // ========================================================================

    struct GeneratorState
    {
        std::mutex mtx;
        std::condition_variable cv;
        enum Phase
        {
            IDLE,
            RUNNING,
            YIELDED,
            DONE
        } phase = IDLE;
        XObject *yieldedValue = nullptr; // heap-allocated to avoid incomplete type
        std::exception_ptr error;        // captures exceptions from generator body
        std::thread worker;
        bool started = false;

        ~GeneratorState();
    };

    struct XGenerator
    {
        std::shared_ptr<GeneratorState> state;
        std::string fnName;

        XGenerator() : state(std::make_shared<GeneratorState>()) {}
    };

    // Forward declarations — full definitions after XObject class
    struct XStructFieldInfo;
    struct XStructMethodInfo;
    struct XStructDef;
    struct XInstance;

    // ========================================================================
    // XData — the heap-allocated control block
    // ========================================================================

    struct XData
    {
        std::atomic<uint32_t> refCount;
        XType type;
        void *payload; // points to the type-specific data

        // ---- GC cycle-collector fields ----
        XData *gc_next = nullptr; // intrusive doubly-linked list
        XData *gc_prev = nullptr;
        int32_t gc_refs = 0;        // temporary: trial-deletion ref count
        bool gc_tracked = false;    // is this on the GC tracking list?
        bool gc_collecting = false; // being freed by GC (skip in release())

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

        /// enum (name → value map)
        static XObject makeEnum(XEnum &&enumDef);

        /// bytes (raw binary data)
        static XObject makeBytes(XBytes &&bytes);
        static XObject makeBytes(const std::string &data);
        static XObject makeBytes(std::vector<uint8_t> &&data);

        /// generator (lazy coroutine)
        static XObject makeGenerator(XGenerator &&gen);

        /// struct type definition
        static XObject makeStructDef(std::shared_ptr<XStructDef> def);

        /// struct/class instance
        static XObject makeInstance(XInstance &&inst);

        /// module object
        static XObject makeModule(std::shared_ptr<XModule> mod);

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
        bool isEnum() const;
        bool isBytes() const;
        bool isGenerator() const;
        bool isStructDef() const;
        bool isInstance() const;
        bool isModule() const;

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
        const XEnum &asEnum() const;
        const XBytes &asBytes() const;
        XBytes &asBytesMut();
        const XGenerator &asGenerator() const;
        XGenerator &asGeneratorMut();
        const XStructDef &asStructDef() const;
        std::shared_ptr<XStructDef> asStructDefShared() const;
        const XInstance &asInstance() const;
        XInstance &asInstanceMut();
        const XModule &asModule() const;
        XModule &asModuleMut();
        std::shared_ptr<XModule> asModuleShared() const;

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

        /// Called by the cycle collector to free a payload without going through
        /// the normal release() path. Public so gc.cpp can use it.
        static void freePayload(XType type, void *payload);

        /// Called by the cycle collector after deleting an XData to keep the
        /// global allocation counter accurate.
        static void notifyGCFreed(size_t count);

        /// Raw access to underlying XData (for GC traversal only).
        XData *rawData() const { return data_; }

    private:
        XData *data_;

        /// Construct from a pre-built XData (takes ownership, refCount already 1)
        explicit XObject(XData *data);

        /// Increment ref count
        void retain();

        /// Decrement ref count, free if zero
        void release();
    };

    // ========================================================================
    // Hash support for XObject
    // ========================================================================

    /// Callback type for hashing instances via __hash__ magic method.
    /// The interpreter sets this so xobject.cpp can call back into it.
    /// Returns true if __hash__ was found and result is set, false otherwise.
    using InstanceHashCallback = bool (*)(const XObject &instance, int64_t &result);

    /// Set/get the global instance hash callback (set by Interpreter on construction).
    void setInstanceHashCallback(InstanceHashCallback cb);
    InstanceHashCallback getInstanceHashCallback();

    /// Check if an XObject is of an immutable (hashable) type.
    /// Hashable: none, int, float, complex, bool, string,
    ///           tuple (if all elements hashable), frozen_set (if all elements hashable),
    ///           frozen instances with __hash__ defined
    /// NOT hashable: list, set, map, function, mutable instances
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

    // ========================================================================
    // XStructDef — a struct type definition (blueprint for instances)
    // ========================================================================

    struct XStructFieldInfo
    {
        std::string name;
        XObject defaultValue;
        AccessLevel access = AccessLevel::PUBLIC;
    };

    struct XStructMethodInfo
    {
        std::string name;
        XObject fnObject; // XObject wrapping an XFunction
        AccessLevel access = AccessLevel::PUBLIC;
        bool isAbstract = false; // true for abstract methods (no body)
    };

    // Property definition (get/set) for runtime
    struct XPropertyInfo
    {
        std::string name;
        XObject getter; // XFunction or None
        XObject setter; // XFunction or None
        AccessLevel access = AccessLevel::PUBLIC;
    };

    struct XStructDef
    {
        std::string name;
        std::vector<XStructFieldInfo> fields;         // ordered field definitions
        std::vector<XStructMethodInfo> methods;       // method definitions
        std::vector<XStructFieldInfo> staticFields;   // static field definitions
        std::vector<XStructMethodInfo> staticMethods; // static method definitions
        std::vector<XPropertyInfo> properties;        // get/set properties
        bool isClass = false;                         // true if defined with `class`, false for `struct`
        bool isInterface = false;                     // true if defined with `interface`
        bool isAbstract = false;                      // true if defined with `abstract`
        bool isMixin = false;                         // true if defined with `mixin`
        // Decorator flags
        bool isDataclass = false;                            // @dataclass: auto-generate __init__/__eq__/__print__
        bool isImmutable = false;                            // @immutable: freeze instances after __init__
        bool isSingleton = false;                            // @singleton: only one instance ever created
        mutable XObject singletonInstance;                   // cached singleton instance (if isSingleton)
        std::vector<std::shared_ptr<XStructDef>> parents;    // parent classes (inheritance chain)
        std::vector<std::shared_ptr<XStructDef>> mixins;     // mixed-in method bundles
        std::vector<std::shared_ptr<XStructDef>> interfaces; // implemented interfaces

        XStructDef() = default;
        XStructDef(std::string name) : name(std::move(name)) {}

        // Look up a method by name, searching own methods then parents (left-to-right DFS)
        const XStructMethodInfo *findMethod(const std::string &methodName) const
        {
            for (const auto &mi : methods)
                if (mi.name == methodName)
                    return &mi;
            for (const auto &parent : parents)
            {
                const XStructMethodInfo *found = parent->findMethod(methodName);
                if (found)
                    return found;
            }
            // Search mixins (lower priority than class/parent)
            for (const auto &mixin : mixins)
            {
                const XStructMethodInfo *found = mixin->findMethod(methodName);
                if (found)
                    return found;
            }
            return nullptr;
        }

        // Like findMethod, but also returns which class in the hierarchy owns the method
        // Returns {method, owningClass} — owningClass is the XStructDef that defined the method
        std::pair<const XStructMethodInfo *, const XStructDef *>
        findMethodWithOwner(const std::string &methodName) const
        {
            for (const auto &mi : methods)
                if (mi.name == methodName)
                    return {&mi, this};
            for (const auto &parent : parents)
            {
                auto result = parent->findMethodWithOwner(methodName);
                if (result.first)
                    return result;
            }
            // Search mixins
            for (const auto &mixin : mixins)
            {
                auto result = mixin->findMethodWithOwner(methodName);
                if (result.first)
                    return result;
            }
            return {nullptr, nullptr};
        }

        // Look up a field default by name, searching own fields then parents
        const XStructFieldInfo *findField(const std::string &fieldName) const
        {
            for (const auto &fi : fields)
                if (fi.name == fieldName)
                    return &fi;
            for (const auto &parent : parents)
            {
                const XStructFieldInfo *found = parent->findField(fieldName);
                if (found)
                    return found;
            }
            // Search mixins (lower priority than own fields and parents)
            for (const auto &mixin : mixins)
            {
                const XStructFieldInfo *found = mixin->findField(fieldName);
                if (found)
                    return found;
            }
            return nullptr;
        }

        // Like findField, but also returns which class in the hierarchy owns the field
        std::pair<const XStructFieldInfo *, const XStructDef *>
        findFieldWithOwner(const std::string &fieldName) const
        {
            for (const auto &fi : fields)
                if (fi.name == fieldName)
                    return {&fi, this};
            for (const auto &parent : parents)
            {
                auto result = parent->findFieldWithOwner(fieldName);
                if (result.first)
                    return result;
            }
            // Search mixins (lower priority than own fields and parents)
            for (const auto &mixin : mixins)
            {
                auto result = mixin->findFieldWithOwner(fieldName);
                if (result.first)
                    return result;
            }
            return {nullptr, nullptr};
        }

        // Check if this class/struct is or inherits from a given name
        bool isOrInherits(const std::string &typeName) const
        {
            if (name == typeName)
                return true;
            for (const auto &parent : parents)
                if (parent->isOrInherits(typeName))
                    return true;
            return false;
        }

        // Look up a static field by name (own then parents)
        const XStructFieldInfo *findStaticField(const std::string &fieldName) const
        {
            for (const auto &fi : staticFields)
                if (fi.name == fieldName)
                    return &fi;
            for (const auto &parent : parents)
            {
                const XStructFieldInfo *found = parent->findStaticField(fieldName);
                if (found)
                    return found;
            }
            return nullptr;
        }

        // Look up a static method by name (own then parents)
        const XStructMethodInfo *findStaticMethod(const std::string &methodName) const
        {
            for (const auto &mi : staticMethods)
                if (mi.name == methodName)
                    return &mi;
            for (const auto &parent : parents)
            {
                const XStructMethodInfo *found = parent->findStaticMethod(methodName);
                if (found)
                    return found;
            }
            return nullptr;
        }

        // Mutable static field lookup — returns mutable pointer for assignment
        XStructFieldInfo *findStaticFieldMut(const std::string &fieldName)
        {
            for (auto &fi : staticFields)
                if (fi.name == fieldName)
                    return &fi;
            for (auto &parent : parents)
            {
                XStructFieldInfo *found = parent->findStaticFieldMut(fieldName);
                if (found)
                    return found;
            }
            return nullptr;
        }

        // Look up a property by name (own then parents)
        const XPropertyInfo *findProperty(const std::string &propName) const
        {
            for (const auto &pi : properties)
                if (pi.name == propName)
                    return &pi;
            for (const auto &parent : parents)
            {
                const XPropertyInfo *found = parent->findProperty(propName);
                if (found)
                    return found;
            }
            return nullptr;
        }

        // Collect all fields from the entire inheritance chain (parents first, then own)
        std::vector<XStructFieldInfo> allFields() const
        {
            std::vector<XStructFieldInfo> result;
            for (const auto &parent : parents)
            {
                auto pfields = parent->allFields();
                for (auto &f : pfields)
                {
                    // Don't add if already present (child overrides parent)
                    bool exists = false;
                    for (const auto &r : result)
                        if (r.name == f.name)
                        {
                            exists = true;
                            break;
                        }
                    if (!exists)
                        result.push_back(std::move(f));
                }
            }
            // Collect fields from mixins (lower priority than parents)
            for (const auto &mixin : mixins)
            {
                auto mfields = mixin->allFields();
                for (auto &f : mfields)
                {
                    bool exists = false;
                    for (const auto &r : result)
                        if (r.name == f.name)
                        {
                            exists = true;
                            break;
                        }
                    if (!exists)
                        result.push_back(std::move(f));
                }
            }
            for (const auto &fi : fields)
            {
                // Child fields override parent fields with same name
                bool found = false;
                for (auto &r : result)
                    if (r.name == fi.name)
                    {
                        r.defaultValue = fi.defaultValue.clone();
                        found = true;
                        break;
                    }
                if (!found)
                {
                    XStructFieldInfo copy;
                    copy.name = fi.name;
                    copy.defaultValue = fi.defaultValue.clone();
                    result.push_back(std::move(copy));
                }
            }
            return result;
        }
    };

    // ========================================================================
    // XInstance — a struct/class instance with field values
    // ========================================================================

    struct XInstance
    {
        std::string typeName;                            // "Point", "Animal", etc.
        std::shared_ptr<XStructDef> structDef;           // reference to the type definition
        std::unordered_map<std::string, XObject> fields; // field name → current value
        bool frozen = false;                             // true if created with ~ prefix

        XInstance() = default;
        XInstance(std::string typeName, std::shared_ptr<XStructDef> def)
            : typeName(std::move(typeName)), structDef(std::move(def)) {}
    };

} // namespace xell
