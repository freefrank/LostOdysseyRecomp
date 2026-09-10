#pragma once

#include "shader/position_evidence.h"
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <map>
#include <mutex>
#include <new>
#include <tuple>
#include <type_traits>

namespace gpu::taa_collection::summary_collection {

using Key = std::tuple<uint64_t, uint64_t, uint32_t, uint32_t, int, uint32_t,
    uint32_t, uint32_t, position_evidence::Summary, uint32_t>;
using Family = std::tuple<uint64_t, uint64_t, int, uint32_t, uint32_t, uint32_t,
    position_evidence::Summary, uint32_t>;
struct Entry { uint32_t count = 0, sent = 0; uint64_t order = 0; };
inline constexpr size_t Capacity = 2048;

inline int Priority(const Key& key) {
    const auto& [vs, ps, w, h, slot, candidates, flags, rejection, position, guards] = key;
    if ((flags & 3) == 3 && !(flags & 4)) {
        if (slot < 0 && position.kind == 1 && !position.issues && guards == 31) return -1;
        if (slot < 0 && candidates) return 0;
        if (slot >= 0 && rejection) return 1;
        if (flags & 1) return 2;
    }
    return (flags & 1) ? 3 : 4;
}

// std::map keeps its existing ordering and eviction behavior, but every node
// comes from this fixed pool. It has no upstream allocator or heap fallback.
// Entries <= 2048; low-priority dimension families <= 1024. The remaining slots
// cover the containers' implementation sentinel nodes with headroom.
class NodePool {
public:
    static constexpr size_t NodeBytes = 256, Nodes = 4096;
    NodePool() {
        storage_.fill(std::byte{}); // Commit/touch the fixed storage during startup.
        for (size_t i = 0; i < Nodes; ++i) free_[i] = uint16_t(i);
    }
    void* Take() {
        if (!available_) throw std::bad_alloc();
        return storage_.data() + size_t(free_[--available_]) * NodeBytes;
    }
    void Put(void* pointer) noexcept {
        const auto offset = static_cast<std::byte*>(pointer) - storage_.data();
        free_[available_++] = uint16_t(size_t(offset) / NodeBytes);
    }
    size_t Available() const { return available_; }
private:
    alignas(std::max_align_t) std::array<std::byte, NodeBytes * Nodes> storage_;
    std::array<uint16_t, Nodes> free_{};
    size_t available_ = Nodes;
};

template<class T> struct PoolAllocator {
    using value_type = T;
    using is_always_equal = std::false_type;
    NodePool* pool = nullptr;
    PoolAllocator() = default;
    explicit PoolAllocator(NodePool& arena) : pool(&arena) {}
    template<class U> PoolAllocator(const PoolAllocator<U>& other) : pool(other.pool) {}
    T* allocate(size_t count) {
        static_assert(sizeof(T) <= NodePool::NodeBytes);
        static_assert(alignof(T) <= alignof(std::max_align_t));
        if (!pool || count != 1) throw std::bad_alloc();
        return static_cast<T*>(pool->Take());
    }
    void deallocate(T* pointer, size_t) noexcept { pool->Put(pointer); }
    template<class U> bool operator==(const PoolAllocator<U>& other) const { return pool == other.pool; }
};

class Queue {
    template<class K, class V> using Map = std::map<K, V, std::less<K>,
        PoolAllocator<std::pair<const K, V>>>;
public:
    enum class Observation { Queued, Counted, Dropped, Disabled, Busy };
    Queue() : entries_(PoolAllocator<std::pair<const Key, Entry>>(pool_)),
        dimensions_(PoolAllocator<std::pair<const Family, unsigned>>(pool_)) {}
    Queue(const Queue&) = delete;
    Queue& operator=(const Queue&) = delete;

    Observation TryObserve(std::mutex& mutex, const std::atomic<int>& consent, const Key& key) noexcept {
        if (consent.load(std::memory_order_relaxed) != 1) return Observation::Disabled;
        try {
            std::unique_lock lock(mutex, std::try_to_lock);
            if (!lock) return Observation::Busy;
            if (consent.load(std::memory_order_relaxed) != 1) return Observation::Disabled;
            return Observe(key);
        } catch (...) { return Observation::Dropped; }
    }

    Observation Observe(const Key& key) {
        auto it = entries_.find(key);
        bool inserted = false;
        if (it == entries_.end()) {
            const auto& [vs, ps, w, h, slot, candidates, flags, rejection, position, guards] = key;
            const int priority = Priority(key);
            const Family family{vs, ps, slot, candidates, flags, rejection, position, guards};
            if (priority >= 2) {
                auto variants = dimensions_.find(family);
                if (entries_.size() >= Capacity / 2 || (variants != dimensions_.end() && variants->second >= 4))
                    return Observation::Dropped;
            }
            if (entries_.size() >= Capacity) {
                auto victim = std::find_if(entries_.begin(), entries_.end(),
                    [&](const auto& item) { return Priority(item.first) > priority; });
                if (victim == entries_.end()) return Observation::Dropped;
                entries_.erase(victim);
            }
            if (priority >= 2) ++dimensions_[family];
            it = entries_.emplace(key, Entry{0, 0, ++order_}).first;
            inserted = true;
        }
        if (it->second.count < 1000000000) ++it->second.count;
        return inserted ? Observation::Queued : Observation::Counted;
    }

    void Reset() { entries_.clear(); dimensions_.clear(); order_ = 0; }
    auto& Entries() { return entries_; }
    const auto& Entries() const { return entries_; }
    size_t Families() const { return dimensions_.size(); }
    size_t AvailableNodes() const { return pool_.Available(); }

private:
    NodePool pool_;
    Map<Key, Entry> entries_;
    Map<Family, unsigned> dimensions_;
    uint64_t order_ = 0;
};
}
