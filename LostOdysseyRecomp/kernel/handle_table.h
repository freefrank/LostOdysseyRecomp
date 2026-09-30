#pragma once
#include <cstdint>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <utility>

namespace kernel {
// A handle owns a reference; each API operation obtains its own reference.
// Destructors run outside the table lock (they may call another subsystem).
template<class Object> class HandleTable {
    struct Entry { std::shared_ptr<void> token; std::shared_ptr<Object> object; };
    struct Reference { std::shared_ptr<Object> object; size_t count = 0; };
    std::mutex mutex;
    std::unordered_map<uint32_t, Entry> handles;
    std::unordered_map<uint32_t, std::weak_ptr<Object>> objects;
    std::unordered_map<uint32_t, Reference> references;
public:
    void Insert(uint32_t handle, std::shared_ptr<Object> object, std::shared_ptr<void> token = {}) {
        std::lock_guard lock(mutex);
        handles.emplace(handle, Entry{std::move(token), object});
        if (objects.size() > handles.size() + 256) {
            for (auto it = objects.begin(); it != objects.end();)
                if (it->second.expired()) it = objects.erase(it); else ++it;
        }
        // Canonical object addresses are inserted without a separate token.
        if (!handles.at(handle).token) objects[handle] = object;
    }
    std::shared_ptr<Object> Acquire(uint32_t handle) {
        std::lock_guard lock(mutex);
        const auto it = handles.find(handle);
        return it == handles.end() ? nullptr : it->second.object;
    }
    std::shared_ptr<Object> AcquireObject(uint32_t address) {
        std::lock_guard lock(mutex);
        const auto it = objects.find(address);
        return it == objects.end() ? nullptr : it->second.lock();
    }
    bool Close(uint32_t handle, std::shared_ptr<Object>* closedObject = nullptr) {
        typename decltype(handles)::node_type removed;
        { std::lock_guard lock(mutex); removed = handles.extract(handle); }
        // Report the object from this removal, not an earlier Acquire that may
        // refer to a recycled handle token. Release output references outside the lock.
        if (closedObject) *closedObject = removed.empty() ? nullptr : removed.mapped().object;
        return !removed.empty();
    }
    // The source remains alive even if another thread closes it after Acquire.
    bool Duplicate(uint32_t source, uint32_t destination, std::shared_ptr<void> token, bool closeSource,
        std::shared_ptr<Object>* closedObject = nullptr) {
        typename decltype(handles)::node_type removed;
        {
            std::lock_guard lock(mutex);
            const auto it = handles.find(source);
            if (it == handles.end() || handles.contains(destination)) return false;
            auto object = it->second.object;
            handles.emplace(destination, Entry{std::move(token), std::move(object)});
            if (closeSource) removed = handles.extract(source);
        }
        if (closedObject) *closedObject = removed.empty() ? nullptr : removed.mapped().object;
        return true;
    }
    void ReferenceObject(uint32_t address, const std::shared_ptr<Object>& object) {
        std::lock_guard lock(mutex);
        auto& ref = references[address]; ref.object = object; ++ref.count;
    }
    void DereferenceObject(uint32_t address) {
        typename decltype(references)::node_type removed;
        {
            std::lock_guard lock(mutex);
            const auto it = references.find(address);
            if (it != references.end() && !--it->second.count) removed = references.extract(it);
        }
    }
};
}
