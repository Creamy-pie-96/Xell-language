#include "xobject.hpp"
#include "../lib/errors/error.hpp"
#include <sstream>
#include <cmath>
#include <cstring>
#include <cassert>
#include <atomic>

namespace xell
{

    // ========================================================================
    // Global allocation counter (debug / test only)
    // ========================================================================

    static std::atomic<int64_t> g_liveAllocs{0};

    int64_t XObject::liveAllocations() { return g_liveAllocs.load(std::memory_order_relaxed); }
    void XObject::resetAllocationCounter() { g_liveAllocs.store(0, std::memory_order_relaxed); }

    // ========================================================================
    // xtype_name — human-readable type tag
    // ========================================================================

    const char *xtype_name(XType t)
    {
        switch (t)
        {
        case XType::NONE:
            return "none";
        case XType::INT:
            return "int";
        case XType::FLOAT:
            return "float";
        case XType::COMPLEX:
            return "complex";
        case XType::BOOL:
            return "bool";
        case XType::STRING:
            return "string";
        case XType::LIST:
            return "list";
        case XType::TUPLE:
            return "tuple";
        case XType::SET:
            return "set";
        case XType::FROZEN_SET:
            return "frozen_set";
        case XType::MAP:
            return "map";
        case XType::FUNCTION:
            return "function";
        case XType::ENUM:
            return "enum";
        case XType::BYTES:
            return "bytes";
        case XType::GENERATOR:
            return "generator";
        }
        return "unknown";
    }

    // ========================================================================
    // XSet methods
    // ========================================================================

    void XSet::add(const XObject &elem) { table.set(elem, true); }
    bool XSet::remove(const XObject &elem) { return table.remove(elem); }
    bool XSet::has(const XObject &elem) const { return table.has(elem); }
    size_t XSet::size() const { return table.size(); }
    bool XSet::empty() const { return table.empty(); }
    void XSet::clear() { table.clear(); }
    std::vector<XObject> XSet::elements() const { return table.keys(); }

    // ========================================================================
    // XMap methods
    // ========================================================================

    void XMap::set(const XObject &key, XObject value) { table.set(key, std::move(value)); }
    XObject *XMap::get(const XObject &key) { return table.get(key); }
    const XObject *XMap::get(const XObject &key) const { return table.get(key); }
    bool XMap::has(const XObject &key) const { return table.has(key); }
    bool XMap::remove(const XObject &key) { return table.remove(key); }

    // String key convenience
    void XMap::set(const std::string &key, XObject value)
    {
        XObject k = XObject::makeString(key);
        table.set(k, std::move(value));
    }
    XObject *XMap::get(const std::string &key)
    {
        XObject k = XObject::makeString(key);
        return table.get(k);
    }
    const XObject *XMap::get(const std::string &key) const
    {
        XObject k = XObject::makeString(key);
        return table.get(k);
    }
    bool XMap::has(const std::string &key) const
    {
        XObject k = XObject::makeString(key);
        return table.has(k);
    }

    size_t XMap::size() const { return table.size(); }
    bool XMap::empty() const { return table.empty(); }
    void XMap::clear() { table.clear(); }

    // ========================================================================
    // Payload allocation helpers (raw new/delete, tracked)
    // ========================================================================

    static XData *allocData(XType type, void *payload)
    {
        g_liveAllocs.fetch_add(1, std::memory_order_relaxed);
        return new XData(type, payload);
    }

    // GeneratorState destructor — defined here because XObject is now complete
    GeneratorState::~GeneratorState()
    {
        if (worker.joinable())
        {
            // If generator is abandoned, we need to let it finish
            {
                std::lock_guard<std::mutex> lk(mtx);
                phase = DONE; // signal generator to exit
            }
            cv.notify_all();
            worker.join();
        }
        delete yieldedValue;
    }

    void XObject::freePayload(XType type, void *payload)
    {
        if (!payload)
            return;

        switch (type)
        {
        case XType::NONE:
            break; // no payload
        case XType::INT:
            delete static_cast<int64_t *>(payload);
            break;
        case XType::FLOAT:
            delete static_cast<double *>(payload);
            break;
        case XType::COMPLEX:
            delete static_cast<XComplex *>(payload);
            break;
        case XType::BOOL:
            delete static_cast<bool *>(payload);
            break;
        case XType::STRING:
            delete static_cast<std::string *>(payload);
            break;
        case XType::LIST:
            delete static_cast<XList *>(payload);
            break;
        case XType::TUPLE:
            delete static_cast<XTuple *>(payload);
            break;
        case XType::SET:
            delete static_cast<XSet *>(payload);
            break;
        case XType::FROZEN_SET:
            delete static_cast<XSet *>(payload);
            break;
        case XType::MAP:
            delete static_cast<XMap *>(payload);
            break;
        case XType::FUNCTION:
            delete static_cast<XFunction *>(payload);
            break;
        case XType::ENUM:
            delete static_cast<XEnum *>(payload);
            break;
        case XType::BYTES:
            delete static_cast<XBytes *>(payload);
            break;
        case XType::GENERATOR:
            delete static_cast<XGenerator *>(payload);
            break;
        }
    }

    // ========================================================================
    // Factory methods
    // ========================================================================

    XObject XObject::makeNone()
    {
        return XObject(allocData(XType::NONE, nullptr));
    }

    XObject XObject::makeInt(int64_t value)
    {
        int64_t *p = new int64_t(value);
        return XObject(allocData(XType::INT, p));
    }

    XObject XObject::makeFloat(double value)
    {
        double *p = new double(value);
        return XObject(allocData(XType::FLOAT, p));
    }

    XObject XObject::makeNumber(double value)
    {
        // Backward compat: creates FLOAT
        double *p = new double(value);
        return XObject(allocData(XType::FLOAT, p));
    }

    XObject XObject::makeComplex(double real, double imag)
    {
        XComplex *p = new XComplex(real, imag);
        return XObject(allocData(XType::COMPLEX, p));
    }

    XObject XObject::makeComplex(const XComplex &c)
    {
        XComplex *p = new XComplex(c);
        return XObject(allocData(XType::COMPLEX, p));
    }

    XObject XObject::makeBool(bool value)
    {
        bool *p = new bool(value);
        return XObject(allocData(XType::BOOL, p));
    }

    XObject XObject::makeString(const std::string &value)
    {
        std::string *p = new std::string(value);
        return XObject(allocData(XType::STRING, p));
    }

    XObject XObject::makeString(std::string &&value)
    {
        std::string *p = new std::string(std::move(value));
        return XObject(allocData(XType::STRING, p));
    }

    XObject XObject::makeList()
    {
        XList *p = new XList();
        return XObject(allocData(XType::LIST, p));
    }

    XObject XObject::makeList(XList &&elements)
    {
        XList *p = new XList(std::move(elements));
        return XObject(allocData(XType::LIST, p));
    }

    XObject XObject::makeTuple(XTuple &&elements)
    {
        XTuple *p = new XTuple(std::move(elements));
        return XObject(allocData(XType::TUPLE, p));
    }

    XObject XObject::makeSet()
    {
        XSet *p = new XSet();
        return XObject(allocData(XType::SET, p));
    }

    XObject XObject::makeSet(XSet &&set)
    {
        XSet *p = new XSet(std::move(set));
        return XObject(allocData(XType::SET, p));
    }

    XObject XObject::makeFrozenSet()
    {
        XSet *p = new XSet();
        return XObject(allocData(XType::FROZEN_SET, p));
    }

    XObject XObject::makeFrozenSet(XSet &&set)
    {
        XSet *p = new XSet(std::move(set));
        return XObject(allocData(XType::FROZEN_SET, p));
    }

    XObject XObject::makeMap()
    {
        XMap *p = new XMap();
        return XObject(allocData(XType::MAP, p));
    }

    XObject XObject::makeMap(XMap &&map)
    {
        XMap *p = new XMap(std::move(map));
        return XObject(allocData(XType::MAP, p));
    }

    XObject XObject::makeFunction(const std::string &name,
                                  const std::vector<std::string> &params,
                                  const std::vector<std::unique_ptr<Stmt>> *body,
                                  Environment *closureEnv)
    {
        XFunction *p = new XFunction(name, params, body, closureEnv);
        return XObject(allocData(XType::FUNCTION, p));
    }

    XObject XObject::makeEnum(XEnum &&enumDef)
    {
        XEnum *p = new XEnum(std::move(enumDef));
        return XObject(allocData(XType::ENUM, p));
    }

    XObject XObject::makeBytes(XBytes &&bytes)
    {
        XBytes *p = new XBytes(std::move(bytes));
        return XObject(allocData(XType::BYTES, p));
    }

    XObject XObject::makeBytes(const std::string &data)
    {
        XBytes *p = new XBytes(data);
        return XObject(allocData(XType::BYTES, p));
    }

    XObject XObject::makeBytes(std::vector<uint8_t> &&data)
    {
        XBytes *p = new XBytes(std::move(data));
        return XObject(allocData(XType::BYTES, p));
    }

    XObject XObject::makeGenerator(XGenerator &&gen)
    {
        XGenerator *p = new XGenerator(std::move(gen));
        return XObject(allocData(XType::GENERATOR, p));
    }

    // ========================================================================
    // Default constructor → none
    // ========================================================================

    XObject::XObject()
        : data_(allocData(XType::NONE, nullptr)) {}

    // ========================================================================
    // Private: construct from raw XData
    // ========================================================================

    XObject::XObject(XData *data)
        : data_(data) {}

    // ========================================================================
    // Ref counting
    // ========================================================================

    void XObject::retain()
    {
        if (data_)
            data_->refCount.fetch_add(1, std::memory_order_relaxed);
    }

    void XObject::release()
    {
        if (!data_)
            return;

        if (data_->refCount.fetch_sub(1, std::memory_order_acq_rel) == 1)
        {
            // Old value was 1 → now 0, we own the last reference
            freePayload(data_->type, data_->payload);
            delete data_;
            g_liveAllocs.fetch_sub(1, std::memory_order_relaxed);
        }
        data_ = nullptr;
    }

    // ========================================================================
    // Big Five
    // ========================================================================

    XObject::~XObject()
    {
        release();
    }

    XObject::XObject(const XObject &other)
        : data_(other.data_)
    {
        retain();
    }

    XObject &XObject::operator=(const XObject &other)
    {
        if (this != &other)
        {
            release();
            data_ = other.data_;
            retain();
        }
        return *this;
    }

    XObject::XObject(XObject &&other) noexcept
        : data_(other.data_)
    {
        other.data_ = nullptr;
    }

    XObject &XObject::operator=(XObject &&other) noexcept
    {
        if (this != &other)
        {
            release();
            data_ = other.data_;
            other.data_ = nullptr;
        }
        return *this;
    }

    // ========================================================================
    // Type queries
    // ========================================================================

    XType XObject::type() const { return data_ ? data_->type : XType::NONE; }
    bool XObject::isNone() const { return type() == XType::NONE; }
    bool XObject::isInt() const { return type() == XType::INT; }
    bool XObject::isFloat() const { return type() == XType::FLOAT; }
    bool XObject::isComplex() const { return type() == XType::COMPLEX; }
    bool XObject::isNumber() const { return type() == XType::INT || type() == XType::FLOAT; }
    bool XObject::isNumeric() const { return type() == XType::INT || type() == XType::FLOAT || type() == XType::COMPLEX; }
    bool XObject::isBool() const { return type() == XType::BOOL; }
    bool XObject::isString() const { return type() == XType::STRING; }
    bool XObject::isList() const { return type() == XType::LIST; }
    bool XObject::isTuple() const { return type() == XType::TUPLE; }
    bool XObject::isSet() const { return type() == XType::SET; }
    bool XObject::isFrozenSet() const { return type() == XType::FROZEN_SET; }
    bool XObject::isMap() const { return type() == XType::MAP; }
    bool XObject::isFunction() const { return type() == XType::FUNCTION; }
    bool XObject::isEnum() const { return type() == XType::ENUM; }
    bool XObject::isBytes() const { return type() == XType::BYTES; }
    bool XObject::isGenerator() const { return type() == XType::GENERATOR; }

    // ========================================================================
    // Payload access (unchecked — caller must verify type)
    // ========================================================================

    int64_t XObject::asInt() const
    {
        return *static_cast<int64_t *>(data_->payload);
    }

    double XObject::asFloat() const
    {
        return *static_cast<double *>(data_->payload);
    }

    const XComplex &XObject::asComplex() const
    {
        return *static_cast<XComplex *>(data_->payload);
    }

    double XObject::asNumber() const
    {
        // Backward compat: returns double for INT or FLOAT
        if (type() == XType::INT)
            return static_cast<double>(asInt());
        return *static_cast<double *>(data_->payload);
    }

    bool XObject::asBool() const
    {
        return *static_cast<bool *>(data_->payload);
    }

    const std::string &XObject::asString() const
    {
        return *static_cast<std::string *>(data_->payload);
    }

    std::string &XObject::asStringMut()
    {
        return *static_cast<std::string *>(data_->payload);
    }

    const XList &XObject::asList() const
    {
        return *static_cast<XList *>(data_->payload);
    }

    XList &XObject::asListMut()
    {
        return *static_cast<XList *>(data_->payload);
    }

    const XTuple &XObject::asTuple() const
    {
        return *static_cast<XTuple *>(data_->payload);
    }

    const XSet &XObject::asSet() const
    {
        return *static_cast<XSet *>(data_->payload);
    }

    XSet &XObject::asSetMut()
    {
        return *static_cast<XSet *>(data_->payload);
    }

    const XSet &XObject::asFrozenSet() const
    {
        return *static_cast<XSet *>(data_->payload);
    }

    const XMap &XObject::asMap() const
    {
        return *static_cast<XMap *>(data_->payload);
    }

    XMap &XObject::asMapMut()
    {
        return *static_cast<XMap *>(data_->payload);
    }

    const XFunction &XObject::asFunction() const
    {
        return *static_cast<XFunction *>(data_->payload);
    }

    const XEnum &XObject::asEnum() const
    {
        return *static_cast<XEnum *>(data_->payload);
    }

    const XBytes &XObject::asBytes() const
    {
        return *static_cast<XBytes *>(data_->payload);
    }

    XBytes &XObject::asBytesMut()
    {
        return *static_cast<XBytes *>(data_->payload);
    }

    const XGenerator &XObject::asGenerator() const
    {
        return *static_cast<XGenerator *>(data_->payload);
    }

    XGenerator &XObject::asGeneratorMut()
    {
        return *static_cast<XGenerator *>(data_->payload);
    }

    // ========================================================================
    // Truthiness
    // ========================================================================

    bool XObject::truthy() const
    {
        switch (type())
        {
        case XType::NONE:
            return false;
        case XType::BOOL:
            return asBool();
        case XType::INT:
            return asInt() != 0;
        case XType::FLOAT:
            return asFloat() != 0.0;
        case XType::COMPLEX:
            return asComplex().real != 0.0 || asComplex().imag != 0.0;
        case XType::STRING:
            return !asString().empty();
        case XType::LIST:
            return !asList().empty();
        case XType::TUPLE:
            return !asTuple().empty();
        case XType::SET:
            return !asSet().empty();
        case XType::FROZEN_SET:
            return !asFrozenSet().empty();
        case XType::MAP:
            return !asMap().empty();
        case XType::FUNCTION:
            return true;
        case XType::ENUM:
            return true; // enums are always truthy
        case XType::BYTES:
            return !asBytes().data.empty();
        case XType::GENERATOR:
            return true; // generators are always truthy
        }
        return false;
    }

    // ========================================================================
    // Deep clone
    // ========================================================================

    XObject XObject::clone() const
    {
        switch (type())
        {
        case XType::NONE:
            return makeNone();
        case XType::INT:
            return makeInt(asInt());
        case XType::FLOAT:
            return makeFloat(asFloat());
        case XType::COMPLEX:
            return makeComplex(asComplex());
        case XType::BOOL:
            return makeBool(asBool());
        case XType::STRING:
            return makeString(asString());
        case XType::LIST:
        {
            XList clonedList;
            clonedList.reserve(asList().size());
            for (const auto &elem : asList())
            {
                clonedList.push_back(elem.clone());
            }
            return makeList(std::move(clonedList));
        }
        case XType::TUPLE:
        {
            XTuple clonedTuple;
            clonedTuple.reserve(asTuple().size());
            for (const auto &elem : asTuple())
            {
                clonedTuple.push_back(elem.clone());
            }
            return makeTuple(std::move(clonedTuple));
        }
        case XType::SET:
        {
            XSet clonedSet;
            for (const auto &elem : asSet().elements())
            {
                clonedSet.add(elem.clone());
            }
            return makeSet(std::move(clonedSet));
        }
        case XType::FROZEN_SET:
        {
            XSet clonedSet;
            for (const auto &elem : asFrozenSet().elements())
            {
                clonedSet.add(elem.clone());
            }
            return makeFrozenSet(std::move(clonedSet));
        }
        case XType::MAP:
        {
            XMap clonedMap;
            for (auto it = asMap().begin(); it.valid(); it.next())
            {
                clonedMap.set(it.key().clone(), it.value().clone());
            }
            return makeMap(std::move(clonedMap));
        }
        case XType::FUNCTION:
        {
            const auto &fn = asFunction();
            return makeFunction(fn.name, fn.params, fn.body, fn.closureEnv);
        }
        case XType::ENUM:
        {
            XEnum cloned = asEnum();
            return makeEnum(std::move(cloned));
        }
        case XType::BYTES:
        {
            XBytes cloned(asBytes().data);
            return makeBytes(std::move(cloned));
        }
        case XType::GENERATOR:
            // Generators are not cloneable (shared state) — return as-is (ref counted)
            return *this;
        }
        return makeNone();
    }

    // ========================================================================
    // toString — string representation for print / interpolation
    // ========================================================================

    std::string XObject::toString() const
    {
        switch (type())
        {
        case XType::NONE:
            return "none";

        case XType::INT:
            return std::to_string(asInt());

        case XType::FLOAT:
        {
            double val = asFloat();
            // Print integers without decimal point
            if (val == std::floor(val) && std::isfinite(val))
            {
                long long intVal = static_cast<long long>(val);
                return std::to_string(intVal);
            }
            std::ostringstream oss;
            oss << val;
            return oss.str();
        }

        case XType::COMPLEX:
        {
            const XComplex &c = asComplex();
            std::ostringstream oss;
            oss << "(";
            // Always print real part
            if (c.real == std::floor(c.real) && std::isfinite(c.real))
                oss << static_cast<long long>(c.real);
            else
                oss << c.real;
            // Print sign + imaginary part
            if (c.imag >= 0.0)
                oss << "+";
            if (c.imag == std::floor(c.imag) && std::isfinite(c.imag))
                oss << static_cast<long long>(c.imag);
            else
                oss << c.imag;
            oss << "i)";
            return oss.str();
        }

        case XType::BOOL:
            return asBool() ? "true" : "false";

        case XType::STRING:
            return asString();

        case XType::LIST:
        {
            std::ostringstream oss;
            oss << "[";
            const auto &list = asList();
            for (size_t i = 0; i < list.size(); i++)
            {
                if (i > 0)
                    oss << ", ";
                if (list[i].isString())
                    oss << "\"" << list[i].asString() << "\"";
                else
                    oss << list[i].toString();
            }
            oss << "]";
            return oss.str();
        }

        case XType::TUPLE:
        {
            std::ostringstream oss;
            oss << "(";
            const auto &tup = asTuple();
            for (size_t i = 0; i < tup.size(); i++)
            {
                if (i > 0)
                    oss << ", ";
                if (tup[i].isString())
                    oss << "\"" << tup[i].asString() << "\"";
                else
                    oss << tup[i].toString();
            }
            if (tup.size() == 1)
                oss << ","; // trailing comma for single-element tuple
            oss << ")";
            return oss.str();
        }

        case XType::SET:
        {
            if (asSet().empty())
                return "set()";
            std::ostringstream oss;
            oss << "{";
            auto elems = asSet().elements();
            for (size_t i = 0; i < elems.size(); i++)
            {
                if (i > 0)
                    oss << ", ";
                if (elems[i].isString())
                    oss << "\"" << elems[i].asString() << "\"";
                else
                    oss << elems[i].toString();
            }
            oss << "}";
            return oss.str();
        }

        case XType::FROZEN_SET:
        {
            if (asFrozenSet().empty())
                return "<>";
            std::ostringstream oss;
            oss << "<";
            auto elems = asFrozenSet().elements();
            for (size_t i = 0; i < elems.size(); i++)
            {
                if (i > 0)
                    oss << ", ";
                if (elems[i].isString())
                    oss << "\"" << elems[i].asString() << "\"";
                else
                    oss << elems[i].toString();
            }
            oss << ">";
            return oss.str();
        }

        case XType::MAP:
        {
            std::ostringstream oss;
            oss << "{";
            size_t i = 0;
            for (auto it = asMap().begin(); it.valid(); it.next(), i++)
            {
                if (i > 0)
                    oss << ", ";
                // Show key: strings without quotes (identifier-like), others as toString
                if (it.key().isString())
                    oss << it.key().asString();
                else
                    oss << it.key().toString();
                oss << ": ";
                // Show value: strings with quotes
                if (it.value().isString())
                    oss << "\"" << it.value().asString() << "\"";
                else
                    oss << it.value().toString();
            }
            oss << "}";
            return oss.str();
        }

        case XType::FUNCTION:
            return "<fn " + asFunction().name + ">";

        case XType::ENUM:
        {
            const auto &e = asEnum();
            std::ostringstream oss;
            oss << "<enum " << e.name << ": ";
            for (size_t i = 0; i < e.memberNames.size(); i++)
            {
                if (i > 0)
                    oss << ", ";
                oss << e.memberNames[i];
            }
            oss << ">";
            return oss.str();
        }

        case XType::BYTES:
        {
            const auto &b = asBytes().data;
            std::ostringstream oss;
            oss << "b\"";
            for (uint8_t byte : b)
            {
                if (byte >= 32 && byte < 127 && byte != '"' && byte != '\\')
                    oss << static_cast<char>(byte);
                else
                {
                    oss << "\\x";
                    const char hex[] = "0123456789abcdef";
                    oss << hex[byte >> 4] << hex[byte & 0xF];
                }
            }
            oss << "\"";
            return oss.str();
        }

        case XType::GENERATOR:
            return "<generator " + asGenerator().fnName + ">";
        }

        return "unknown";
    }

    // ========================================================================
    // Equality
    // ========================================================================

    bool XObject::equals(const XObject &other) const
    {
        // Same XData pointer → same object
        if (data_ == other.data_)
            return true;

        // Numeric cross-type equality: int == float, int == complex, float == complex
        if (isNumeric() && other.isNumeric())
        {
            // If either is complex, compare as complex
            if (isComplex() || other.isComplex())
            {
                XComplex a = isComplex() ? asComplex() : XComplex(asNumber(), 0.0);
                XComplex b = other.isComplex() ? other.asComplex() : XComplex(other.asNumber(), 0.0);
                return a == b;
            }
            // Both are int/float — compare as double
            return asNumber() == other.asNumber();
        }

        // Different types are never equal (except numeric cross-type handled above)
        if (type() != other.type())
            return false;

        switch (type())
        {
        case XType::NONE:
            return true; // none == none
        case XType::INT:
            return asInt() == other.asInt();
        case XType::FLOAT:
            return asFloat() == other.asFloat();
        case XType::COMPLEX:
            return asComplex() == other.asComplex();
        case XType::BOOL:
            return asBool() == other.asBool();
        case XType::STRING:
            return asString() == other.asString();
        case XType::LIST:
        {
            const auto &a = asList();
            const auto &b = other.asList();
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); i++)
            {
                if (!a[i].equals(b[i]))
                    return false;
            }
            return true;
        }
        case XType::TUPLE:
        {
            const auto &a = asTuple();
            const auto &b = other.asTuple();
            if (a.size() != b.size())
                return false;
            for (size_t i = 0; i < a.size(); i++)
            {
                if (!a[i].equals(b[i]))
                    return false;
            }
            return true;
        }
        case XType::SET:
        {
            const auto &a = asSet();
            const auto &b = other.asSet();
            if (a.size() != b.size())
                return false;
            for (const auto &elem : a.elements())
            {
                if (!b.has(elem))
                    return false;
            }
            return true;
        }
        case XType::FROZEN_SET:
        {
            const auto &a = asFrozenSet();
            const auto &b = other.asFrozenSet();
            if (a.size() != b.size())
                return false;
            for (const auto &elem : a.elements())
            {
                if (!b.has(elem))
                    return false;
            }
            return true;
        }
        case XType::MAP:
        {
            const auto &a = asMap();
            const auto &b = other.asMap();
            if (a.size() != b.size())
                return false;
            for (auto it = a.begin(); it.valid(); it.next())
            {
                const XObject *bVal = b.get(it.key());
                if (!bVal || !it.value().equals(*bVal))
                    return false;
            }
            return true;
        }
        case XType::FUNCTION:
            // Functions are equal only if they are the same object (already handled above)
            return false;
        case XType::ENUM:
            // Enum equality: same name and same members
            return asEnum().name == other.asEnum().name;
        case XType::BYTES:
            return asBytes().data == other.asBytes().data;
        case XType::GENERATOR:
            // Generators are equal only if same object (already handled above)
            return false;
        }
        return false;
    }

    // ========================================================================
    // Debug: ref count
    // ========================================================================

    uint32_t XObject::refCount() const
    {
        return data_ ? data_->refCount.load(std::memory_order_relaxed) : 0;
    }

    // ========================================================================
    // Hash support
    // ========================================================================

    bool isHashable(const XObject &obj)
    {
        switch (obj.type())
        {
        case XType::NONE:
        case XType::INT:
        case XType::FLOAT:
        case XType::COMPLEX:
        case XType::BOOL:
        case XType::STRING:
            return true;
        case XType::TUPLE:
        {
            // Tuple is hashable only if ALL elements are hashable
            for (const auto &elem : obj.asTuple())
            {
                if (!isHashable(elem))
                    return false;
            }
            return true;
        }
        case XType::FROZEN_SET:
        {
            // Frozen set is hashable only if ALL elements are hashable
            for (const auto &elem : obj.asFrozenSet().elements())
            {
                if (!isHashable(elem))
                    return false;
            }
            return true;
        }
        case XType::BYTES:
            return true; // bytes are immutable and hashable
        default:
            return false; // LIST, SET, MAP, FUNCTION are mutable/non-hashable
        }
    }

    size_t hashXObject(const XObject &obj, hash::HashFn hashFn)
    {
        switch (obj.type())
        {
        case XType::NONE:
        {
            const char marker[] = "__xell_none__";
            return hashFn(marker, sizeof(marker) - 1);
        }
        case XType::INT:
        {
            // Use specialized integer hash for default algo
            if (hashFn == hash::fnv1a)
                return hash::hash_int(obj.asInt());
            int64_t val = obj.asInt();
            return hashFn(&val, sizeof(val));
        }
        case XType::FLOAT:
        {
            if (hashFn == hash::fnv1a)
                return hash::hash_float(obj.asFloat());
            double val = obj.asFloat();
            if (val == 0.0)
                val = 0.0; // normalize -0.0
            return hashFn(&val, sizeof(val));
        }
        case XType::COMPLEX:
        {
            // Hash real and imaginary parts separately and combine
            const XComplex &c = obj.asComplex();
            double real = c.real == 0.0 ? 0.0 : c.real;
            double imag = c.imag == 0.0 ? 0.0 : c.imag;
            size_t h1 = hashFn(&real, sizeof(real));
            size_t h2 = hashFn(&imag, sizeof(imag));
            return hash::hash_combine(h1, h2);
        }
        case XType::BOOL:
        {
            uint8_t b = obj.asBool() ? 1 : 0;
            return hashFn(&b, 1);
        }
        case XType::STRING:
            return hashFn(obj.asString().c_str(), obj.asString().size());
        case XType::TUPLE:
        {
            const char seed[] = "__xell_tuple__";
            size_t h = hashFn(seed, sizeof(seed) - 1);
            for (const auto &elem : obj.asTuple())
            {
                h = hash::hash_combine(h, hashXObject(elem, hashFn));
            }
            return h;
        }
        case XType::FROZEN_SET:
        {
            // Order-independent hash: XOR all element hashes
            // (XOR is commutative + associative → order doesn't matter)
            const char seed[] = "__xell_fset__";
            size_t h = hashFn(seed, sizeof(seed) - 1);
            for (const auto &elem : obj.asFrozenSet().elements())
            {
                h ^= hashXObject(elem, hashFn);
            }
            return h;
        }
        case XType::BYTES:
        {
            const auto &b = obj.asBytes().data;
            return hashFn(b.data(), b.size());
        }
        default:
            throw HashError("cannot hash mutable type '" +
                                std::string(xtype_name(obj.type())) + "'",
                            0);
        }
    }

    size_t XObjectHash::operator()(const XObject &obj) const
    {
        return hashXObject(obj, hash::fnv1a);
    }

    bool XObjectEqual::operator()(const XObject &a, const XObject &b) const
    {
        return a.equals(b);
    }

} // namespace xell
