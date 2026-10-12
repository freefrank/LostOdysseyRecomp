#include <stdafx.h>
#include <chrono>
#include <mutex>
#include <string>
#include <unordered_map>
#include <os/logger.h>

// #375 diagnostics: when a stream (field or battle BGM, event audio, jingles)
// cannot start, the game keeps the previous BGM and the log shows nothing.
// sub_82B5F610(sound system, type, path, ...) plays every stream. It returns 0
// when none of the six stream slots is free (sound system + 3580, 20 bytes
// each, see sub_82321720 kind 3), and marks the slot 255 when the stream
// manager cannot open the stream (sub_82B6DBD0 returned -1). Record either
// case with what occupies the slots. The success path only reads one byte.

extern "C" PPC_FUNC(__imp__sub_82B5F610);

namespace
{
constexpr uint32_t SoundSystem = 0x832D268C;
constexpr uint32_t StreamManager = 0x832D2DE0;
constexpr uint32_t StreamSlots = 3580;
constexpr uint32_t SlotSize = 20;
constexpr uint32_t SlotCount = 6;
constexpr uint32_t ChannelPath = 127; // channel objects are 320 bytes

bool GuestPointer(uint32_t p) { return p >= 0x10000 && p < 0xF0000000u; }

std::string GuestString(uint8_t* base, uint32_t p, size_t max)
{
    std::string s;
    for (size_t i = 0; GuestPointer(p) && i < max; ++i)
    {
        const char c = char(PPC_LOAD_U8(p + i));
        if (!c) break;
        s.push_back(c >= 32 && c < 127 ? c : '?');
    }
    return s;
}

// Occupied slots as state/substate (sub_82B62610: substate 5 starting, 10/11
// playing, 12 ended) with the stream manager channel's state and file name.
std::string DescribeSlots(uint8_t* base, uint32_t system)
{
    std::string out;
    const uint32_t manager = PPC_LOAD_U32(StreamManager);
    const uint32_t channels = GuestPointer(manager) ? PPC_LOAD_U32(manager + 4) : 0;
    const uint32_t stride = GuestPointer(manager) ? PPC_LOAD_U16(manager + 12) : 0;
    for (uint32_t i = 0; i < SlotCount; ++i)
    {
        const uint32_t slot = system + StreamSlots + i * SlotSize;
        const uint8_t state = PPC_LOAD_U8(slot);
        if (!state) continue;
        out += fmt::format(" [{} state={} sub={} flags={:#x}", i, state, PPC_LOAD_U8(slot + 9), PPC_LOAD_U8(slot + 5));
        if (GuestPointer(channels) && stride)
        {
            const uint32_t channel = channels + i * stride;
            const std::string path = GuestString(base, channel + ChannelPath, 96);
            out += fmt::format(" channel={:#x} {}", PPC_LOAD_U8(channel + 10), path.substr(path.find_last_of('\\') + 1));
        }
        out += "]";
    }
    return out.empty() ? " none" : out;
}
}

PPC_FUNC(sub_82B5F610)
{
    const uint32_t system = ctx.r3.u32, type = ctx.r4.u32, path = ctx.r5.u32;
    __imp__sub_82B5F610(ctx, base);
    const uint32_t handle = ctx.r3.u32;
    if (!GuestPointer(system))
        return;
    const char* reason = nullptr;
    if (!handle)
        reason = "no free stream slot";
    else if (PPC_LOAD_U8(system + StreamSlots + (handle & 0xFF) * SlotSize) == 0xFF)
        reason = "stream open failed";
    if (!reason)
        return;
    // A caller may retry a failed start every frame: report each reason and
    // path again only after 10 s, so later failures still reach the log.
    const std::string name = GuestString(base, path, 200);
    const auto now = std::chrono::steady_clock::now();
    {
        static std::mutex mutex;
        static std::unordered_map<std::string, std::chrono::steady_clock::time_point> reported;
        std::lock_guard lock(mutex);
        if (reported.size() > 256)
            reported.clear();
        const auto [it, added] = reported.try_emplace(std::string(reason) + '|' + name, now);
        if (!added && now - it->second < std::chrono::seconds(10))
            return;
        it->second = now;
    }
    LOG_WARNING("stream start failed ({}): type={} handle={:#x} {}; slots:{}", reason, type, handle, name,
        DescribeSlots(base, system));
}
