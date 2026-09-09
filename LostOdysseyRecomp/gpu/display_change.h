#pragma once
#include <atomic>
#include <cstdint>
#include <mutex>

namespace gpu::video {
enum class DisplayChangeResult { Pending, Applied, Failed };

// One display transaction. A repeated mode still gets a new ticket;
// a late window/presentation completion cannot complete a newer request.
class DisplayChangeTracker {
    std::mutex mutex;
    uint64_t serial = 0;
    uint32_t width = 0, height = 0, mode = 0;
    DisplayChangeResult result = DisplayChangeResult::Failed;
    std::atomic<uint64_t> presentation{0};
public:
    uint64_t TryBegin(uint32_t w, uint32_t h, uint32_t m) {
        std::lock_guard lock(mutex);
        if (result == DisplayChangeResult::Pending) return 0;
        ++serial; width = w; height = h; mode = m;
        result = DisplayChangeResult::Pending;
        presentation = 0;
        return serial;
    }
    uint64_t Begin(uint32_t w, uint32_t h, uint32_t m) {
        std::lock_guard lock(mutex);
        ++serial; width = w; height = h; mode = m;
        result = DisplayChangeResult::Pending;
        presentation = 0;
        return serial;
    }
    uint64_t WindowTicket(uint32_t w, uint32_t h, uint32_t m) {
        std::lock_guard lock(mutex);
        return result == DisplayChangeResult::Pending && width == w && height == h && mode == m ? serial : 0;
    }
    void WindowComplete(uint64_t ticket, bool success) {
        std::lock_guard lock(mutex);
        if (!ticket || ticket != serial || result != DisplayChangeResult::Pending) return;
        if (success) presentation = ticket;
        else { result = DisplayChangeResult::Failed; presentation = 0; }
    }
    uint64_t PresentationTicket() const { return presentation.load(); }
    void Complete(uint64_t ticket, bool success) {
        if (!ticket) return;
        std::lock_guard lock(mutex);
        if (!ticket || ticket != serial || result != DisplayChangeResult::Pending) return;
        result = success ? DisplayChangeResult::Applied : DisplayChangeResult::Failed;
        presentation = 0;
    }
    DisplayChangeResult Query(uint64_t ticket) {
        std::lock_guard lock(mutex);
        return ticket == serial ? result : DisplayChangeResult::Failed;
    }
    void Reset() {
        std::lock_guard lock(mutex);
        ++serial; presentation = 0; result = DisplayChangeResult::Failed;
    }
};
}
