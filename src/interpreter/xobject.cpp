#include "xobject.hpp"
#include <sstream>
#include <cmath>
#include <cstring>
#include <cassert>

namespace xell
{

    // ========================================================================
    // Global allocation counter (debug / test only)
    // ========================================================================

    static int64_t g_liveAllocs = 0;

    int64_t XObject::liveAllocations() { return g_liveAllocs; }
    void XObject::resetAllocationCounter() { g_liveAllocs = 0; }

    // ========================================================================
    // xtype_name — human-readable type tag
    // ========================================================================

    const char *xtype_name(XType t)
    {
        switch (t)
        {
        case XType::NONE:
            return "none";
        case XType::NUMBER:
            return "number";
        case XType::BOOL:
            return "bool";
        case XType::STRING:
            return "string";
        case XType::LIST:
            return "list";
        case XType::MAP:
            return "map";
        case XType::FUNCTION:
            return "function";
        }
        return "unknown";
    }

    // ========================================================================
    // XMap methods
    // ========================================================================

    void XMap::set(const std::string &key, XObject value)
    {
        auto it = index.find(key);
        if (it != index.end())
        {
            // Key exists — overwrite value in place
            entries[it->second].second = std::move(value);
        }
        else
        {
            // New key — append and record index
            index[key] = entries.size();
            entries.emplace_back(key, std::move(value));
        }
    }

    XObject *XMap::get(const std::string &key)
    {
        auto it = index.find(key);
        if (it == index.end())
            return nullptr;
        return &entries[it->second].second;
    }

    const XObject *XMap::get(const std::string &key) const
    {
        auto it = index.find(key);
        if (it == index.end())
            return nullptr;
        return &entries[it->second].second;
    }

    bool XMap::has(const std::string &key) const
    {
        return index.find(key) != index.end();
    }

    size_t XMap::size() const
    {
        return entries.size();
    }

    // ========================================================================
    // Payload allocation helpers (raw new/delete, tracked)
    // ========================================================================

    static XData *allocData(XType type, void *payload)
    {
        g_liveAllocs++;
        return new XData(type, payload);
    }

    void XObject::freePayload(XType type, void *payload)
    {
        if (!payload)
            return;

        switch (type)
        {
        case XType::NONE:
            break; // no payload
        case XType::NUMBER:
            delete static_cast<double *>(payload);
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
        case XType::MAP:
            delete static_cast<XMap *>(payload);
            break;
        case XType::FUNCTION:
            delete static_cast<XFunction *>(payload);
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

    XObject XObject::makeNumber(double value)
    {
        double *p = new double(value);
        return XObject(allocData(XType::NUMBER, p));
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
                                  const std::vector<std::unique_ptr<Stmt>> *body)
    {
        XFunction *p = new XFunction(name, params, body);
        return XObject(allocData(XType::FUNCTION, p));
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
            data_->refCount++;
    }

    void XObject::release()
    {
        if (!data_)
            return;

        data_->refCount--;
        if (data_->refCount == 0)
        {
            freePayload(data_->type, data_->payload);
            delete data_;
            g_liveAllocs--;
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
    bool XObject::isNumber() const { return type() == XType::NUMBER; }
    bool XObject::isBool() const { return type() == XType::BOOL; }
    bool XObject::isString() const { return type() == XType::STRING; }
    bool XObject::isList() const { return type() == XType::LIST; }
    bool XObject::isMap() const { return type() == XType::MAP; }
    bool XObject::isFunction() const { return type() == XType::FUNCTION; }

    // ========================================================================
    // Payload access (unchecked — caller must verify type)
    // ========================================================================

    double XObject::asNumber() const
    {
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
        case XType::NUMBER:
            return asNumber() != 0.0;
        case XType::STRING:
            return !asString().empty();
        case XType::LIST:
            return !asList().empty();
        case XType::MAP:
            return !asMap().entries.empty();
        case XType::FUNCTION:
            return true;
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
        case XType::NUMBER:
            return makeNumber(asNumber());
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
        case XType::MAP:
        {
            XMap clonedMap;
            for (const auto &[key, val] : asMap().entries)
            {
                clonedMap.set(key, val.clone());
            }
            return makeMap(std::move(clonedMap));
        }
        case XType::FUNCTION:
        {
            const auto &fn = asFunction();
            return makeFunction(fn.name, fn.params, fn.body);
        }
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

        case XType::NUMBER:
        {
            double val = asNumber();
            // Print integers without decimal point
            if (val == std::floor(val) && std::isfinite(val))
            {
                // Check if it fits in a long long for clean printing
                long long intVal = static_cast<long long>(val);
                return std::to_string(intVal);
            }
            // For actual floats, use ostringstream for clean output
            std::ostringstream oss;
            oss << val;
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
                // Strings get quoted in list representation
                if (list[i].isString())
                    oss << "\"" << list[i].asString() << "\"";
                else
                    oss << list[i].toString();
            }
            oss << "]";
            return oss.str();
        }

        case XType::MAP:
        {
            std::ostringstream oss;
            oss << "{";
            const auto &map = asMap();
            for (size_t i = 0; i < map.entries.size(); i++)
            {
                if (i > 0)
                    oss << ", ";
                oss << map.entries[i].first << ": ";
                if (map.entries[i].second.isString())
                    oss << "\"" << map.entries[i].second.asString() << "\"";
                else
                    oss << map.entries[i].second.toString();
            }
            oss << "}";
            return oss.str();
        }

        case XType::FUNCTION:
            return "<fn " + asFunction().name + ">";
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

        // Different types are never equal
        if (type() != other.type())
            return false;

        switch (type())
        {
        case XType::NONE:
            return true; // none == none
        case XType::NUMBER:
            return asNumber() == other.asNumber();
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
        case XType::MAP:
        {
            const auto &a = asMap();
            const auto &b = other.asMap();
            if (a.entries.size() != b.entries.size())
                return false;
            for (const auto &[key, val] : a.entries)
            {
                const XObject *bVal = b.get(key);
                if (!bVal || !val.equals(*bVal))
                    return false;
            }
            return true;
        }
        case XType::FUNCTION:
            // Functions are equal only if they are the same object (already handled above)
            return false;
        }
        return false;
    }

    // ========================================================================
    // Debug: ref count
    // ========================================================================

    uint32_t XObject::refCount() const
    {
        return data_ ? data_->refCount : 0;
    }

} // namespace xell
