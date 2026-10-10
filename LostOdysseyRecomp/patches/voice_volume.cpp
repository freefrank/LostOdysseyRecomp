#include <stdafx.h>
#include <array>
#include <cstring>
#include <mutex>
#include <string>
#include <string_view>
#include <os/logger.h>
#include <apu/audio.h>

// Voice volume (#394). The retail game has four volume categories in its
// sound system (0x832D268C): sub_82870E38 hands config+8 (Music) to category 3
// and config+12 (Sound effects) to categories 2 and 1, scaled to 0-128 by
// sub_82B5C918, which keeps master * category >> 7 at +620 + category. A
// channel's type at +11 selects its category (sub_82B71D90).
//
// A cutscene's dialogue is the language track of one of its streams: the
// channel playing snd\stream\be0200.xwv (path at channel + 127) has a second
// voice, handle at channel + 124, that sub_82B6D7E0 opens on the same name
// under the language folder (sub_82851800: snd\int\stream\be0200.xwv). Every
// stream under a language folder has a twin of that name in snd\stream (all
// four discs), so dialogue always plays as such a language track. The
// stream volume update sub_82B6C650(channel, voice) runs for both voices and
// multiplies in the channel's category gain from sub_82B71E68(channel). For
// the language track, return master * Voice volume there instead; at 100 it
// matches the retail gain with Sound effects at 100. Voices inside the effect
// banks (.xse: battle shouts, short event lines) share their bank and its
// waves' volume with the effects and keep following Sound effects.

extern "C" PPC_FUNC(__imp__sub_82B6C650);
extern "C" PPC_FUNC(__imp__sub_82B71E68);

namespace
{
constexpr uint32_t SoundSystem = 0x832D268C;
constexpr uint32_t StreamVoices = 0x832D2DA0;
constexpr uint32_t ChannelPath = 127;
constexpr size_t PathCapacity = 320 - ChannelPath; // the channel object is 320 bytes

thread_local bool languageTrack = false;

// The voice for a stream handle, as sub_82B6B840 looks it up.
uint32_t StreamVoice(uint8_t* base, uint32_t handle)
{
    const uint32_t voices = PPC_LOAD_U32(StreamVoices);
    const uint32_t table = voices ? PPC_LOAD_U32(voices + 8) : 0;
    return handle && table ? PPC_LOAD_U32(table + handle * 4 - 4) : 0;
}

// Logs each stream once per gain, so run logs show what was replaced.
void Log(std::string_view path, float gain, double retail)
{
    static std::mutex mutex;
    static std::array<std::pair<std::string, float>, 8> recent;
    static size_t next = 0;
    std::lock_guard lock(mutex);
    for (const auto& [p, g] : recent)
        if (g == gain && p == path) return;
    recent[next++ % recent.size()] = {std::string(path), gain};
    LOG_INFO("voice volume: {:.2f} instead of {:.2f} for the language track of {}", gain, retail, path);
}
}

PPC_FUNC(sub_82B6C650)
{
    const uint32_t voice = ctx.r4.u32;
    languageTrack = voice && voice == StreamVoice(base, PPC_LOAD_U16(ctx.r3.u32 + 124));
    __imp__sub_82B6C650(ctx, base);
    languageTrack = false;
}

PPC_FUNC(sub_82B71E68)
{
    const uint32_t channel = ctx.r3.u32;
    __imp__sub_82B71E68(ctx, base);
    if (!languageTrack)
        return;
    const uint32_t system = PPC_LOAD_U32(SoundSystem);
    const float master = system ? float(PPC_LOAD_U8(system + 616)) / 128.0f : 1.0f;
    const float gain = master * float(apu::VoiceVolume()) / 100.0f;
    const char* path = reinterpret_cast<const char*>(base + channel + ChannelPath);
    Log(std::string_view(path, strnlen(path, PathCapacity)), gain, ctx.f1.f64);
    ctx.f1.f64 = double(gain);
}
