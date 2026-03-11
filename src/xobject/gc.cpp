#include "gc.hpp"
#include "../interpreter/xobject.hpp"
#include "../interpreter/environment.hpp"
#include "../module/xmodule.hpp"
#include <vector>
#include <algorithm>

namespace xell
{

    // ========================================================================
    // Container type check
    // ========================================================================

    bool isGCContainerType(uint8_t type)
    {
        switch (static_cast<XType>(type))
        {
        case XType::LIST:
        case XType::TUPLE:
        case XType::SET:
        case XType::FROZEN_SET:
        case XType::MAP:
        case XType::FUNCTION:
        case XType::ENUM:
        case XType::INSTANCE:
        case XType::STRUCT_DEF:
        case XType::MODULE:
            return true;
        default:
            return false;
        }
    }

    // ========================================================================
    // GCHeap singleton
    // ========================================================================

    GCHeap &GCHeap::instance()
    {
        static GCHeap heap;
        return heap;
    }

    GCHeap::GCHeap() = default;

    // ========================================================================
    // Tracking list management
    // ========================================================================

    void GCHeap::track(XData *data)
    {
        if (!data || data->gc_tracked)
            return;

        data->gc_tracked = true;
        data->gc_next = nullptr;
        data->gc_prev = tail_;

        if (tail_)
            tail_->gc_next = data;
        else
            head_ = data;
        tail_ = data;

        trackedCount_++;
        allocsSinceCollect_++;

        // Auto-collect when threshold is reached
        if (enabled_ && !collecting_ && allocsSinceCollect_ >= threshold_)
            collect();
    }

    void GCHeap::untrack(XData *data)
    {
        if (!data || !data->gc_tracked)
            return;

        data->gc_tracked = false;

        // Unlink from doubly-linked list
        if (data->gc_prev)
            data->gc_prev->gc_next = data->gc_next;
        else
            head_ = data->gc_next;

        if (data->gc_next)
            data->gc_next->gc_prev = data->gc_prev;
        else
            tail_ = data->gc_prev;

        data->gc_next = nullptr;
        data->gc_prev = nullptr;

        if (trackedCount_ > 0)
            trackedCount_--;
    }

    // ========================================================================
    // Child traversal — visit all XData* children of a container object
    // ========================================================================

    void GCHeap::visitChildren(XData *data, const std::function<void(XData *)> &callback)
    {
        if (!data || !data->payload)
            return;

        switch (data->type)
        {
        case XType::LIST:
        {
            auto *list = static_cast<XList *>(data->payload);
            for (auto &elem : *list)
            {
                XData *child = elem.rawData();
                if (child)
                    callback(child);
            }
            break;
        }

        case XType::TUPLE:
        {
            auto *tup = static_cast<XTuple *>(data->payload);
            for (auto &elem : *tup)
            {
                XData *child = elem.rawData();
                if (child)
                    callback(child);
            }
            break;
        }

        case XType::SET:
        case XType::FROZEN_SET:
        {
            auto *set = static_cast<XSet *>(data->payload);
            for (auto it = set->table.begin(); it.valid(); it.next())
            {
                XData *child = it.key().rawData();
                if (child)
                    callback(child);
            }
            break;
        }

        case XType::MAP:
        {
            auto *map = static_cast<XMap *>(data->payload);
            for (auto it = map->table.begin(); it.valid(); it.next())
            {
                XData *kd = it.key().rawData();
                if (kd)
                    callback(kd);
                XData *vd = it.value().rawData();
                if (vd)
                    callback(vd);
            }
            break;
        }

        case XType::FUNCTION:
        {
            auto *fn = static_cast<XFunction *>(data->payload);
            // Visit closure environment variables
            if (fn->ownedEnv)
            {
                for (auto &kv : fn->ownedEnv->vars())
                {
                    XData *child = kv.second.rawData();
                    if (child)
                        callback(child);
                }
            }
            // Visit overloads
            for (auto &ov : fn->overloads)
            {
                XData *child = ov.rawData();
                if (child)
                    callback(child);
            }
            break;
        }

        case XType::ENUM:
        {
            auto *en = static_cast<XEnum *>(data->payload);
            for (auto &kv : en->members)
            {
                XData *child = kv.second.rawData();
                if (child)
                    callback(child);
            }
            break;
        }

        case XType::INSTANCE:
        {
            auto *inst = static_cast<XInstance *>(data->payload);
            for (auto &kv : inst->fields)
            {
                XData *child = kv.second.rawData();
                if (child)
                    callback(child);
            }
            // Note: inst->structDef is a shared_ptr<XStructDef>, not an XObject.
            // The GC tracks the STRUCT_DEF XData separately if it exists.
            break;
        }

        case XType::STRUCT_DEF:
        {
            auto *defPtr = static_cast<std::shared_ptr<XStructDef> *>(data->payload);
            XStructDef *def = defPtr->get();
            if (!def)
                break;
            // Fields' default values
            for (auto &fi : def->fields)
            {
                XData *child = fi.defaultValue.rawData();
                if (child)
                    callback(child);
            }
            // Methods' function objects
            for (auto &mi : def->methods)
            {
                XData *child = mi.fnObject.rawData();
                if (child)
                    callback(child);
            }
            // Static fields
            for (auto &fi : def->staticFields)
            {
                XData *child = fi.defaultValue.rawData();
                if (child)
                    callback(child);
            }
            // Static methods
            for (auto &mi : def->staticMethods)
            {
                XData *child = mi.fnObject.rawData();
                if (child)
                    callback(child);
            }
            // Properties (getter/setter)
            for (auto &pi : def->properties)
            {
                XData *gd = pi.getter.rawData();
                if (gd)
                    callback(gd);
                XData *sd = pi.setter.rawData();
                if (sd)
                    callback(sd);
            }
            // Singleton instance
            {
                XData *sd = def->singletonInstance.rawData();
                if (sd)
                    callback(sd);
            }
            break;
        }

        case XType::MODULE:
        {
            auto *modPtr = static_cast<std::shared_ptr<XModule> *>(data->payload);
            XModule *mod = modPtr->get();
            if (!mod)
                break;
            // Exported values
            for (auto &kv : mod->exports)
            {
                XData *child = kv.second.rawData();
                if (child)
                    callback(child);
            }
            // Owned environment variables
            if (mod->ownedEnv)
            {
                for (auto &kv : mod->ownedEnv->vars())
                {
                    XData *child = kv.second.rawData();
                    if (child)
                        callback(child);
                }
            }
            break;
        }

        default:
            // Non-container types have no children
            break;
        }
    }

    // ========================================================================
    // collect() — the cycle-detection and collection algorithm
    // ========================================================================

    size_t GCHeap::collect()
    {
        if (collecting_)
            return 0; // re-entrant guard

        collecting_ = true;
        allocsSinceCollect_ = 0;

        // ================================================================
        // Phase 1: Copy refcounts to gc_refs for all tracked objects
        // ================================================================
        for (XData *p = head_; p; p = p->gc_next)
            p->gc_refs = static_cast<int32_t>(p->refCount.load(std::memory_order_relaxed));

        // ================================================================
        // Phase 2: Subtract internal references
        //   For each tracked object, visit its children. If a child is also
        //   tracked, decrement the child's gc_refs. After this pass, gc_refs
        //   reflects only EXTERNAL references (from outside the tracked set).
        // ================================================================
        for (XData *p = head_; p; p = p->gc_next)
        {
            visitChildren(p, [](XData *child)
                          {
                if (child->gc_tracked)
                    child->gc_refs--; });
        }

        // ================================================================
        // Phase 3: Find roots (gc_refs > 0 → externally referenced)
        // ================================================================
        std::vector<XData *> roots;
        for (XData *p = head_; p; p = p->gc_next)
        {
            if (p->gc_refs > 0)
            {
                roots.push_back(p);
                p->gc_refs = -1; // sentinel: "reachable"
            }
        }

        // ================================================================
        // Phase 4: Transitively mark everything reachable from roots
        // ================================================================
        while (!roots.empty())
        {
            XData *root = roots.back();
            roots.pop_back();

            visitChildren(root, [&](XData *child)
                          {
                if (child->gc_tracked && child->gc_refs != -1)
                {
                    child->gc_refs = -1; // mark reachable
                    roots.push_back(child);
                } });
        }

        // ================================================================
        // Phase 5: Collect unreachable objects (gc_refs != -1)
        // ================================================================
        std::vector<XData *> unreachable;
        for (XData *p = head_; p; p = p->gc_next)
        {
            if (p->gc_refs != -1)
                unreachable.push_back(p);
        }

        if (unreachable.empty())
        {
            collecting_ = false;
            collections_++;
            return 0;
        }

        // ================================================================
        // Phase 6: Free unreachable objects
        //   1. Remove from tracking list and mark as gc_collecting
        //      (so release() calls triggered during payload destruction
        //       don't double-free).
        //   2. Destroy payloads (this triggers XObject destructors on
        //      children, but gc_collecting prevents re-entry).
        //   3. Delete the XData control blocks.
        // ================================================================
        size_t freed = unreachable.size();

        for (XData *data : unreachable)
        {
            untrack(data);
            data->gc_collecting = true;
        }

        // Destroy payloads — child XObject destructors will call release(),
        // which checks gc_collecting and safely no-ops for garbage objects.
        for (XData *data : unreachable)
        {
            if (data->payload)
            {
                XObject::freePayload(data->type, data->payload);
                data->payload = nullptr;
            }
        }

        // Delete control blocks and update the global allocation counter
        for (XData *data : unreachable)
            delete data;

        XObject::notifyGCFreed(freed);

        collecting_ = false;
        collections_++;
        return freed;
    }

} // namespace xell
