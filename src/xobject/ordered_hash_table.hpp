#pragma once

// =============================================================================
// OrderedHashTable — Insertion-order-preserving hash table with separate chaining
// =============================================================================
//
// Design goals (per Xell specification):
//   - Insertion-order iteration (like Python 3.7+ dict)
//   - Separate chaining for collision handling (linked via pool indices)
//   - NO tombstones: deleted entries are fully unlinked and recycled
//   - O(1) average insert / lookup / delete
//   - Generic template: Key, Value, Hash functor, KeyEqual functor
//
// Memory layout:
//
//   pool_ : vector<Node>   — node storage (arena), reused via free list
//   buckets_ : vector<int32_t> — bucket heads (index into pool_, -1 = empty)
//
//   Each Node stores:
//     key, value        — the data
//     hash              — cached hash value
//     order_prev/next   — doubly-linked list for insertion order
//     chain_next        — singly-linked list for bucket chain (or free list)
//
//   On delete: node is unlinked from both the order list and bucket chain,
//   then added to the free list (via chain_next). Key/value are reset to
//   release resources. No tombstones, no bloat.
//
// =============================================================================

#include <vector>
#include <cstdint>
#include <cstddef>
#include <utility>

namespace xell
{

    template <typename Key, typename Value, typename Hash, typename KeyEqual>
    class OrderedHashTable
    {
    public:
        // ---- Node: one entry in the pool ----
        struct Node
        {
            Key key;
            Value value;
            size_t hash = 0;
            int32_t order_prev = -1; // previous in insertion order
            int32_t order_next = -1; // next in insertion order
            int32_t chain_next = -1; // next in bucket chain (or free list link)
        };

    private:
        std::vector<Node> pool_;         // node storage arena
        std::vector<int32_t> buckets_;   // bucket[hash % capacity] → first node index
        int32_t order_head_ = -1;        // first node in insertion order
        int32_t order_tail_ = -1;        // last node in insertion order
        int32_t free_head_ = -1;         // head of recycled-node free list
        size_t live_count_ = 0;          // number of live entries

        Hash hasher_;
        KeyEqual equal_;

        static constexpr size_t INITIAL_BUCKETS = 8;
        // Load factor 75% using integer arithmetic: count * 4 > buckets * 3
        static constexpr size_t LF_NUM = 3;
        static constexpr size_t LF_DEN = 4;

        // ---- Internal helpers ----

        int32_t allocNode()
        {
            if (free_head_ != -1)
            {
                int32_t idx = free_head_;
                free_head_ = pool_[idx].chain_next;
                return idx;
            }
            pool_.push_back(Node{});
            return static_cast<int32_t>(pool_.size() - 1);
        }

        void recycleNode(int32_t idx)
        {
            // Reset key/value to release any held resources (e.g. XObject refcounts)
            pool_[idx].key = Key{};
            pool_[idx].value = Value{};
            pool_[idx].hash = 0;
            pool_[idx].order_prev = -1;
            pool_[idx].order_next = -1;
            // Link into free list via chain_next
            pool_[idx].chain_next = free_head_;
            free_head_ = idx;
        }

        void rehash(size_t new_bucket_count)
        {
            buckets_.assign(new_bucket_count, -1);
            // Re-insert all live nodes into new buckets (order list is unchanged)
            int32_t cur = order_head_;
            while (cur != -1)
            {
                size_t b = pool_[cur].hash % new_bucket_count;
                pool_[cur].chain_next = buckets_[b];
                buckets_[b] = cur;
                cur = pool_[cur].order_next;
            }
        }

        bool needsGrow() const
        {
            return (live_count_ + 1) * LF_DEN > buckets_.size() * LF_NUM;
        }

    public:
        OrderedHashTable() = default;

        // ================================================================
        // set — Insert or update a key-value pair.
        // Returns true if this was a new insertion, false if an update.
        // ================================================================
        bool set(const Key &key, const Value &value)
        {
            if (buckets_.empty())
                buckets_.assign(INITIAL_BUCKETS, -1);

            size_t h = hasher_(key);
            size_t b = h % buckets_.size();

            // Search bucket chain for existing key
            int32_t cur = buckets_[b];
            while (cur != -1)
            {
                if (pool_[cur].hash == h && equal_(pool_[cur].key, key))
                {
                    pool_[cur].value = value; // update existing
                    return false;
                }
                cur = pool_[cur].chain_next;
            }

            // Grow if load factor exceeded (before allocating node)
            if (needsGrow())
            {
                rehash(buckets_.size() * 2);
                b = h % buckets_.size(); // recalculate bucket after rehash
            }

            // Allocate and populate new node
            int32_t idx = allocNode();
            pool_[idx].key = key;
            pool_[idx].value = value;
            pool_[idx].hash = h;

            // Prepend to bucket chain
            pool_[idx].chain_next = buckets_[b];
            buckets_[b] = idx;

            // Append to insertion-order list (tail)
            pool_[idx].order_prev = order_tail_;
            pool_[idx].order_next = -1;
            if (order_tail_ != -1)
                pool_[order_tail_].order_next = idx;
            else
                order_head_ = idx; // first element
            order_tail_ = idx;

            live_count_++;
            return true; // new insertion
        }

        // ================================================================
        // get — Look up a key. Returns pointer to value, or nullptr.
        // ================================================================
        Value *get(const Key &key)
        {
            if (buckets_.empty())
                return nullptr;

            size_t h = hasher_(key);
            size_t b = h % buckets_.size();

            int32_t cur = buckets_[b];
            while (cur != -1)
            {
                if (pool_[cur].hash == h && equal_(pool_[cur].key, key))
                    return &pool_[cur].value;
                cur = pool_[cur].chain_next;
            }
            return nullptr;
        }

        const Value *get(const Key &key) const
        {
            return const_cast<OrderedHashTable *>(this)->get(key);
        }

        // ================================================================
        // has — Check if a key exists.
        // ================================================================
        bool has(const Key &key) const
        {
            return get(key) != nullptr;
        }

        // ================================================================
        // remove — Delete a key. Returns true if found and removed.
        // No tombstones: node is fully unlinked and recycled.
        // ================================================================
        bool remove(const Key &key)
        {
            if (buckets_.empty())
                return false;

            size_t h = hasher_(key);
            size_t b = h % buckets_.size();

            // Walk bucket chain, tracking previous for unlinking
            int32_t prev_chain = -1;
            int32_t cur = buckets_[b];
            while (cur != -1)
            {
                if (pool_[cur].hash == h && equal_(pool_[cur].key, key))
                {
                    // 1. Unlink from bucket chain
                    if (prev_chain == -1)
                        buckets_[b] = pool_[cur].chain_next;
                    else
                        pool_[prev_chain].chain_next = pool_[cur].chain_next;

                    // 2. Unlink from insertion-order doubly-linked list
                    int32_t op = pool_[cur].order_prev;
                    int32_t on = pool_[cur].order_next;
                    if (op != -1)
                        pool_[op].order_next = on;
                    else
                        order_head_ = on;
                    if (on != -1)
                        pool_[on].order_prev = op;
                    else
                        order_tail_ = op;

                    // 3. Recycle the node (releases resources, adds to free list)
                    recycleNode(cur);
                    live_count_--;
                    return true;
                }
                prev_chain = cur;
                cur = pool_[cur].chain_next;
            }
            return false;
        }

        // ================================================================
        // Size / empty / clear
        // ================================================================
        size_t size() const { return live_count_; }
        bool empty() const { return live_count_ == 0; }

        void clear()
        {
            pool_.clear();
            buckets_.clear();
            order_head_ = order_tail_ = free_head_ = -1;
            live_count_ = 0;
        }

        // ================================================================
        // Iterator — traverses entries in insertion order
        // ================================================================
        class Iterator
        {
            friend class OrderedHashTable;
            const std::vector<Node> *pool_;
            int32_t idx_;
            Iterator(const std::vector<Node> *pool, int32_t idx) : pool_(pool), idx_(idx) {}

        public:
            bool valid() const { return idx_ != -1; }
            const Key &key() const { return (*pool_)[idx_].key; }
            const Value &value() const { return (*pool_)[idx_].value; }
            Value &valueMut() { return const_cast<Node &>((*pool_)[idx_]).value; }
            void next() { idx_ = (*pool_)[idx_].order_next; }

            bool operator!=(const Iterator &other) const { return idx_ != other.idx_; }
        };

        Iterator begin() const { return Iterator(&pool_, order_head_); }
        Iterator end() const { return Iterator(&pool_, -1); }

        // ================================================================
        // Convenience collectors (return vectors in insertion order)
        // ================================================================
        std::vector<Key> keys() const
        {
            std::vector<Key> result;
            result.reserve(live_count_);
            for (auto it = begin(); it.valid(); it.next())
                result.push_back(it.key());
            return result;
        }

        std::vector<Value> values() const
        {
            std::vector<Value> result;
            result.reserve(live_count_);
            for (auto it = begin(); it.valid(); it.next())
                result.push_back(it.value());
            return result;
        }

        std::vector<std::pair<Key, Value>> entries() const
        {
            std::vector<std::pair<Key, Value>> result;
            result.reserve(live_count_);
            for (auto it = begin(); it.valid(); it.next())
                result.emplace_back(it.key(), it.value());
            return result;
        }
    };

} // namespace xell
