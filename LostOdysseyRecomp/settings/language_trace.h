#pragma once

#include "language_selection.h"
#include <os/logger.h>
#include <atomic>
#include <array>

namespace settings::language
{
inline bool TraceEnabled()
{
    static const bool enabled = [] {
        const auto value = std::getenv("LO_TRACE_LANGUAGE");
        return value && std::string_view(value) == "1";
    }();
    return enabled;
}

// One bounded stream shared by all hooks. The logger supplies timestamp/thread;
// seq identifies this observation, never an asynchronous 'last language'.
inline uint32_t TraceSequence()
{
    static std::atomic<uint32_t> sequence{0};
    const auto value = sequence.fetch_add(1, std::memory_order_relaxed);
    if (value == 2048) LOG_INFO("language: trace truncated after 2048 observations");
    return value < 2048 ? value + 1 : 0;
}

inline void TraceLookup(uint8_t* base, uint32_t host, uint32_t object,
                        uint32_t request, uint32_t caller, uint32_t result, bool overridden)
{
    if (!TraceEnabled()) return;
    const std::array key{host, object, request, caller, result, uint32_t(overridden)};
    static thread_local std::array<uint32_t, 6> previous{};
    if (key == previous) return;
    previous = key;
    const auto seq = TraceSequence();
    if (!seq) return;
    uint32_t matchingIds = 0;
    if (result)
        for (uint32_t id = 0; id <= 9; ++id)
            if (PPC_LOAD_U32(Records + id * 4) == result) matchingIds |= 1u << id;
    LOG_INFO("language: seq={} stage=lookup lr={:#x} host={} object={:#x} request={} path={} record={:#x} static_id_mask={:#x}",
        seq, caller, host, object, request, overridden ? "host" : "original", result, matchingIds);
}

// Called only on the guest thread with the same config the menu already uses.
// Avoid chasing an uninitialized global config chain from a resource callback.
inline void TraceConfig(uint8_t* base, uint32_t config, const char* stage)
{
    if (!TraceEnabled()) return;
    const auto seq = TraceSequence();
    if (!seq) return;
    const auto count = uint32_t(PPC_LOAD_U8(Registry + 419));
    const auto index = PPC_LOAD_U32(config + 24);
    const bool valid = ValidVoiceIndex(count, index);
    const auto id = valid ? uint32_t(PPC_LOAD_U16(Registry + 288 + index * 2)) : 0;
    LOG_INFO("language: seq={} stage={} config={:#x} raw_voice_index={} raw_voice_count={} valid={} mapped_voice_id={}",
        seq, stage, config, index, count, valid, id);
}

inline void TraceEffective(uint8_t* base, const char* stage, uint32_t caller, uint32_t result)
{
    if (!TraceEnabled()) return;
    // 82481E78 writes this fixed UTF-16 cache; never dereference result as text.
    const std::array<uint32_t, 5> key{caller, result, PPC_LOAD_U16(0x83318000),
        PPC_LOAD_U16(0x83318002), PPC_LOAD_U16(0x83318004)};
    static thread_local std::array<uint32_t, 5> previous{};
    if (key == previous) return;
    previous = key;
    const auto seq = TraceSequence();
    if (!seq) return;
    LOG_INFO("language: seq={} stage={} lr={:#x} result={:#x} effective_utf16={:04x},{:04x},{:04x}",
        seq, stage, caller, result, key[2], key[3], key[4]);
}
}
