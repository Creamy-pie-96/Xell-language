#pragma once

// =============================================================================
// GCHeap — Cycle-collecting garbage collector for Xell
// =============================================================================
//
// Xell uses reference counting for deterministic memory management. This works
// perfectly for acyclic object graphs, but circular references (e.g., closures
// that capture themselves, self-referential containers, singleton patterns)
// cause silent memory leaks because the ref count never reaches zero.
//
// GCHeap implements a CPython-style "trial deletion" cycle collector that runs
// as a backup to the primary reference counting mechanism:
//
//   1. Only CONTAINER types (those that can reference other XObjects) are
//      tracked by the GC. Scalars (int, float, bool, string, bytes, complex,
//      none) are never tracked — they cannot form cycles.
//
//   2. When a container XData is allocated, it is registered on an intrusive
//      doubly-linked list maintained by GCHeap.
//
//   3. Periodically (after THRESHOLD container allocations), the collector
//      runs the trial-deletion algorithm:
//
//        a. Copy each tracked object's refcount into a temporary gc_refs field.
//        b. For each tracked object, visit its children. For each child that
//           is also tracked, decrement the child's gc_refs.
//        c. After this pass, objects with gc_refs > 0 have external references
//           (from the stack, global variables, etc.) — they are ROOTS.
//        d. Transitively mark everything reachable from roots.
//        e. Everything remaining is unreachable garbage — break cycles and free.
//
//   4. The collector is completely transparent to the user. No user-facing API
//      is needed (though a gc_collect() builtin can be exposed later).
//
// Memory overhead: 3 fields per tracked XData (two pointers + one int32).
// CPU overhead: near-zero between collections. Collection itself is O(tracked).
//
// =============================================================================

#include <cstdint>
#include <cstddef>
#include <functional>

namespace xell
{

    // Forward declarations
    struct XData;

    /// Check whether a given XType represents a container type (can hold
    /// references to other XObjects and thus participate in reference cycles).
    bool isGCContainerType(uint8_t type);

    // ========================================================================
    // GCHeap — singleton cycle collector
    // ========================================================================

    class GCHeap
    {
    public:
        /// Get the singleton instance.
        static GCHeap &instance();

        /// Register a container XData on the tracking list.
        /// Called automatically by allocData() for container types.
        void track(XData *data);

        /// Remove an XData from the tracking list.
        /// Called automatically by release() before freeing, or by the collector.
        void untrack(XData *data);

        /// Run one cycle-collection pass. Returns the number of objects freed.
        /// Can be called manually (e.g., from a gc_collect() builtin) or
        /// automatically by track() when the allocation threshold is reached.
        size_t collect();

        /// Number of container objects currently tracked.
        size_t trackedCount() const { return trackedCount_; }

        /// Number of collection cycles that have run.
        size_t collectionsRun() const { return collections_; }

        /// Allocation threshold — collect() is triggered after this many
        /// container allocations since the last collection.
        static constexpr size_t DEFAULT_THRESHOLD = 700;

        /// Set/get the threshold.
        void setThreshold(size_t t) { threshold_ = t; }
        size_t threshold() const { return threshold_; }

        /// Enable/disable automatic collection (for testing).
        void setEnabled(bool e) { enabled_ = e; }
        bool enabled() const { return enabled_; }

    private:
        GCHeap();
        ~GCHeap() = default;

        // Non-copyable
        GCHeap(const GCHeap &) = delete;
        GCHeap &operator=(const GCHeap &) = delete;

        /// Visit all child XData* pointers of a container object.
        /// The callback receives each child's XData* (may be null — skipped).
        void visitChildren(XData *data, const std::function<void(XData *)> &callback);

        // ---- Tracking list (intrusive doubly-linked via gc_next/gc_prev) ----
        XData *head_ = nullptr; // first tracked object (or nullptr)
        XData *tail_ = nullptr; // last tracked object (or nullptr)

        // ---- Counters ----
        size_t trackedCount_ = 0;
        size_t allocsSinceCollect_ = 0;
        size_t collections_ = 0;
        size_t threshold_ = DEFAULT_THRESHOLD;
        bool enabled_ = true;

        // ---- Recursion guard ----
        bool collecting_ = false;
    };

} // namespace xell
