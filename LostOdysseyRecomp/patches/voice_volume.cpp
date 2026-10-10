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
// sub_82B5C918, which keeps master * category >> 7 at +620 + category.
//
// Category 1 belongs to the streams that sub_82B5F610 starts with kind 0
// (channel type 35 at +11): a cutscene's localized dialogue stream
// (snd\int\stream\be0200.xwv), but also its effects and music streams
// (snd\stream\be0200.xwv, be0200bgm.xwv) and jingles such as s04_item. The
// stream setup sub_82B6DBD0 copies the full path to channel + 127, so the
// dialogue streams are the ones under a language folder (int, jpn, deu, fra,
// ita, kor...) of snd\. Voices inside the effect banks (.xse: battle shouts,
// short event lines) share their bank and its waves' volume with the effects
// and keep following Sound effects.
//
// sub_82B71E68(channel) returns a channel's category gain in f1, which the
// three volume updates (sub_82B6C650, sub_82B6EA80, sub_82B6F708) multiply
// into its volume. For a dialogue stream, return master * Voice volume
// instead: at 100 it matches the retail gain with Sound effects at 100.

extern "C" PPC_FUNC(__imp__sub_82B71E68);

namespace
{
constexpr uint32_t SoundSystem = 0x832D268C;
constexpr uint8_t KindZeroStream = 35;
constexpr uint32_t ChannelPath = 127;
constexpr size_t PathCapacity = 320 - ChannelPath; // the channel object is 320 bytes

bool Separator(char c) { return c == '\\' || c == '/'; }

// "...\snd\<language>\stream\name.xwv"; the shared streams are "...\snd\stream\...".
bool DialoguePath(const char* path)
{
    for (size_t i = 0; i + 5 < PathCapacity && path[i]; ++i)
    {
        if (!Separator(path[i]) || (path[i + 1] | 0x20) != 's' || (path[i + 2] | 0x20) != 'n' ||
            (path[i + 3] | 0x20) != 'd' || !Separator(path[i + 4]))
            continue;
        size_t language = i + 5, end = language;
        while (end < PathCapacity && path[end] && !Separator(path[end])) ++end;
        if (end == language || end + 8 > PathCapacity || !Separator(path[end])) return false;
        static constexpr char stream[] = "stream";
        for (size_t k = 0; k < 6; ++k)
            if ((path[end + 1 + k] | 0x20) != stream[k]) return false;
        return Separator(path[end + 7]);
    }
    return false;
}

// Logs each dialogue stream once per gain, so run logs show what was replaced.
void Log(std::string_view path, float gain, double retail)
{
    static std::mutex mutex;
    static std::array<std::pair<std::string, float>, 8> recent;
    static size_t next = 0;
    std::lock_guard lock(mutex);
    for (const auto& [p, g] : recent)
        if (g == gain && p == path) return;
    recent[next++ % recent.size()] = {std::string(path), gain};
    LOG_INFO("voice volume: {:.2f} instead of {:.2f} for {}", gain, retail, path);
}
}

PPC_FUNC(sub_82B71E68)
{
    const uint32_t channel = ctx.r3.u32;
    __imp__sub_82B71E68(ctx, base);
    if (PPC_LOAD_U8(channel + 11) != KindZeroStream)
        return;
    const char* path = reinterpret_cast<const char*>(base + channel + ChannelPath);
    if (!DialoguePath(path))
        return;
    const uint32_t system = PPC_LOAD_U32(SoundSystem);
    const float master = system ? float(PPC_LOAD_U8(system + 616)) / 128.0f : 1.0f;
    const float gain = master * float(apu::VoiceVolume()) / 100.0f;
    Log(std::string_view(path, strnlen(path, PathCapacity)), gain, ctx.f1.f64);
    ctx.f1.f64 = double(gain);
}
