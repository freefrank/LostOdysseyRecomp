#pragma once
// Optional event-only attribution. This never authorizes reuse, completes a
// submission, changes a resource lifetime, or scans the live resource set.
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <type_traits>

namespace gpu::vk_object_trace {
inline bool Enabled() {
    static const bool enabled = [] {
        const char* value = std::getenv("LO_VK_OBJECT_TRACE");
        return value && std::strcmp(value, "1") == 0;
    }();
    return enabled;
}
inline bool Permit() {
    if (!Enabled()) return false;
    static std::atomic<uint32_t> events{0};
    constexpr uint32_t limit = 8192;
    const auto event = events.fetch_add(1, std::memory_order_relaxed);
    if (event == limit)
        std::fputs("VK_OBJECT_TRACE limit=8192 reached; missing events do not establish ownership\n", stderr);
    return event < limit;
}
template<class Handle> unsigned long long Id(Handle handle) {
    if constexpr (std::is_pointer_v<Handle>) return static_cast<unsigned long long>(reinterpret_cast<std::uintptr_t>(handle));
    else return static_cast<unsigned long long>(handle);
}
}
