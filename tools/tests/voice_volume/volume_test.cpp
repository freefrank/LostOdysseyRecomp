#include <stdafx.h>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
#include <unordered_map>

PPC_FUNC(sub_82B6C650);
PPC_FUNC(sub_82B71E68);

namespace
{
constexpr uint32_t Channel = 0x1000, Primary = 0x2000, Secondary = 0x2100;
constexpr uint32_t System = 0x3000, SoundSystemPointer = 0x832D268C;
constexpr uint32_t StreamVoicesPointer = 0x832D2DA0;
constexpr uint32_t Voices = 0x83300000, VoiceTable = 0x83300100;
std::array<uint8_t, 0x4000> guest{};
std::unordered_map<uint32_t, uint8_t> highGuest;
uint32_t voicePercent = 100;
unsigned checks = 0, retailUpdates = 0, retailGains = 0;
double sceneGain = 1;
struct Capture { uint32_t voice = 0, argument = 0; double category = 0, output = 0; } captured;

void Check(bool result, const char* reason)
{
    ++checks;
    if (!result) { std::fprintf(stderr, "FAIL: %s\n", reason); std::exit(1); }
}
void Close(double actual, double expected, const char* reason)
{
    ++checks;
    if (std::abs(actual - expected) > 1e-6)
    {
        std::fprintf(stderr, "FAIL: %s: %.9f != %.9f\n", reason, actual, expected);
        std::exit(1);
    }
}
void Store8(uint32_t address, uint8_t value)
{
    if (address < guest.size()) guest[address] = value;
    else highGuest[address] = value;
}
void Store16(uint32_t address, uint16_t value)
{
    Store8(address, uint8_t(value >> 8)); Store8(address + 1, uint8_t(value));
}
void Store32(uint32_t address, uint32_t value)
{
    for (unsigned i = 0; i != 4; ++i) Store8(address + i, uint8_t(value >> (24 - 8 * i)));
}

// Guest options are 0..128. Retail caches master * category >> 7 for each
// category; Voice is the runtime's independent 0..100 setting.
void Options(uint8_t master, uint8_t music, uint8_t effects, uint32_t voice)
{
    Store8(System + 616, master);
    Store8(System + 621, uint8_t((unsigned(master) * effects) >> 7));
    Store8(System + 622, uint8_t((unsigned(master) * effects) >> 7));
    Store8(System + 623, uint8_t((unsigned(master) * music) >> 7));
    voicePercent = voice;
}
void Source(uint32_t voice, std::initializer_list<uint8_t> speakers)
{
    Store8(voice + 28, uint8_t(speakers.size()));
    unsigned index = 0;
    for (uint8_t speaker : speakers) Store8(voice + 46 + index++, speaker);
}
void Reset()
{
    guest.fill(0); highGuest.clear();
    Store32(SoundSystemPointer, System);
    Store32(StreamVoicesPointer, Voices); Store32(Voices + 8, VoiceTable);
    Store32(VoiceTable, Primary); Store32(VoiceTable + 4, Secondary);
    Store8(Channel + 11, 35);
    Store16(Channel + 100, 1); Store16(Channel + 124, 2);
    constexpr char path[] = "bin/xenon/snd/stream/rt089.xwv";
    std::memcpy(guest.data() + Channel + 127, path, sizeof(path));
    // Ready RT089 snapshot: localized primary is mono FC; companion base is
    // FL, FR, LFE, BL, BR. Speaker-map entries are indices, not bit masks.
    Source(Primary, {2}); Source(Secondary, {0, 1, 3, 4, 5});
    Options(128, 128, 128, 100);
}
void Volume(uint32_t voice, double expected, const char* reason, double curve = 1,
    uint32_t resolvedPrimary = Primary)
{
    const unsigned updatesBefore = retailUpdates, gainsBefore = retailGains;
    sceneGain = curve;
    PPCContext ctx;
    ctx.r3.u32 = Channel; ctx.r4.u32 = voice;
    sub_82B6C650(ctx, guest.data());
    Check(retailUpdates == updatesBefore + 1 && retailGains == gainsBefore + 1,
        "production update calls retail update and the production category wrapper once");
    Check(captured.voice == (voice ? voice : resolvedPrimary), "retail update retains its selected voice");
    Check(captured.argument == voice, "production wrapper preserves the original retail voice argument");
    Close(captured.category, expected, reason);
    Close(captured.output, expected * curve, reason);
}
}

uint8_t fixture::Load8(uint32_t address)
{
    if (address < guest.size()) return guest[address];
    const auto found = highGuest.find(address);
    return found == highGuest.end() ? 0 : found->second;
}
uint16_t fixture::Load16(uint32_t address)
{
    return uint16_t((uint16_t(Load8(address)) << 8) | Load8(address + 1));
}
uint32_t fixture::Load32(uint32_t address)
{
    uint32_t value = 0;
    for (unsigned i = 0; i != 4; ++i) value = (value << 8) | Load8(address + i);
    return value;
}
namespace apu { uint32_t VoiceVolume() { return voicePercent; } }

// These are the only retail stubs. The type/category mapping follows
// sub_82B71D90, and the cached gain byte is returned as sub_82B71E68 does.
// Source-layout classification belongs exclusively to production code.
extern "C" PPC_FUNC(__imp__sub_82B71E68)
{
    ++retailGains;
    const uint8_t type = fixture::Load8(ctx.r3.u32 + 11);
    uint32_t gain = 128;
    const uint32_t system = fixture::Load32(SoundSystemPointer);
    if (type <= 4) gain = fixture::Load8(system + 622);
    else if (type == 17 || type == 19) gain = fixture::Load8(system + 623);
    else if (type == 35) gain = fixture::Load8(system + 621);
    ctx.r3.u32 = gain; // retail clobbers the argument register
    ctx.f1.f64 = double(gain) / 128.0;
}
extern "C" PPC_FUNC(__imp__sub_82B6C650)
{
    ++retailUpdates;
    const uint32_t argument = ctx.r4.u32;
    uint32_t voice = argument;
    if (!voice)
    {
        // Retail sub_82B6B840 selects the one-based primary handle. This is
        // guest pointer lookup only; all layout/gain decisions stay in the hook.
        const uint32_t manager = fixture::Load32(StreamVoicesPointer);
        const uint32_t table = manager ? fixture::Load32(manager + 8) : 0;
        const uint16_t handle = fixture::Load16(ctx.r3.u32 + 100);
        if (table && handle) voice = fixture::Load32(table + handle * 4 - 4);
    }
    sub_82B71E68(ctx, base); // exercise the real category hook inside the real update hook
    captured = {voice, argument, ctx.f1.f64, ctx.f1.f64 * sceneGain};
}

int main()
{
    Reset();
    Options(128, 0, 128, 100);
    Volume(Primary, 1, "Music 0 retains the localized mono-FC dialogue");
    Volume(0, 1, "Music 0 retains dialogue through the retail primary lookup");
    Volume(Secondary, 1, "Music 0 retains the retail effects-category RT base");

    Options(128, 128, 128, 0);
    Volume(Primary, 0, "Voice 0 mutes only the actual mono-FC source");
    Volume(0, 0, "Voice 0 also mutes the primary selected by a slider update");
    Volume(Secondary, 1, "Voice 0 retains the five-channel companion base");

    Options(128, 128, 0, 65);
    Volume(Primary, .65, "Effects 0 retains the independent Voice gain");
    Volume(0, .65, "Effects 0 retains Voice through the retail primary lookup");
    Volume(Secondary, 0, "Effects 0 retains the retail base mute");
    Options(64, 0, 32, 37);
    Volume(Primary, .185, "Voice uses master without multiplying the retail effects gain", .4);
    Volume(Secondary, .125, "base retains master times retail effects gain and event curve", .4);

    // Every default slider matches the retail category, including master
    // attenuation and a nontrivial existing scene curve.
    for (uint8_t master : std::array<uint8_t, 3>{128, 64, 31})
    {
        Reset(); Options(master, 128, 128, 100);
        Volume(Primary, double(master) / 128, "default Voice matches retail gain", .625);
        Volume(Secondary, double(master) / 128, "default base matches retail gain", .625);
    }

    // The actual full-surround sidecars found for RT035/RT115 and others are
    // cannot be assumed dialogue-only. Localization cannot override the category.
    Reset(); Source(Primary, {0, 1, 2, 3, 4, 5});
    Options(128, 0, 64, 0);
    Volume(Primary, .5, "full-six-channel language source keeps retail effects gain");
    Volume(0, .5, "primary lookup retains a full-six-channel source's retail category");
    Volume(Secondary, .5, "full-six-channel sidecar leaves companion gain retail");
    Options(128, 128, 0, 100);
    Volume(Primary, 0, "full-six-channel language source still follows Effects 0");

    Reset(); Store16(Channel + 124, 0); Options(128, 0, 96, 0);
    Volume(Primary, .75, "unpaired mono-FC source retains retail category");
    Reset(); Source(Primary, {0}); Options(128, 0, 32, 0);
    Volume(Primary, .25, "paired mono-FL source retains retail category");

    // The ready snapshot can appear after an earlier volume update. Stale
    // speaker-map bytes must not classify a source whose count is still zero.
    Reset(); Options(128, 0, 64, 0);
    Store8(Primary + 28, 0);
    Volume(Primary, .5, "loading count-zero source is not mistaken for dialogue");
    Volume(0, .5, "primary lookup before the source is ready retains retail category");
    Volume(Secondary, .5, "loading primary does not change the base gain");
    Source(Primary, {2});
    Volume(Primary, 0, "ready mono-FC source is recognized on the next update");
    Volume(Secondary, .5, "dialogue classification does not leak into companion update");
    Volume(0, 0, "ready mono-FC primary lookup uses Voice without changing retail r4");
    PPCContext outside;
    outside.r3.u32 = Channel;
    sub_82B71E68(outside, guest.data());
    Close(outside.f1.f64, .5, "dialogue classification ends after the update returns");
    Store8(Primary + 28, 0);
    Volume(Primary, .5, "source returning to count zero does not retain prior classification");
    Volume(0, .5, "primary lookup does not retain classification after source unload");

    Source(Primary, {2});
    Store16(Channel + 100, 0);
    Volume(0, .5, "missing primary handle safely retains retail category", 1, 0);
    Store16(Channel + 100, 3);
    Volume(0, .5, "unassigned primary handle safely retains retail category", 1, 0);
    Store16(Channel + 100, 1); Store32(StreamVoicesPointer, 0);
    Volume(0, .5, "missing voice manager safely retains retail category", 1, 0);
    Store32(StreamVoicesPointer, Voices); Store32(Voices + 8, 0);
    Volume(0, .5, "missing voice table safely retains retail category", 1, 0);
    Store32(Voices + 8, VoiceTable); Store32(VoiceTable, 0);
    Volume(0, .5, "empty primary handle entry safely retains retail category", 1, 0);
    Store32(VoiceTable, Primary);
    Volume(0, 0, "primary lookup recovers after its handle entry becomes ready");

    // A separate retail BGM stream remains controlled by Music, even when
    // Effects and Voice are zero. The hook does not reclassify every RT as BGM.
    Reset(); Store16(Channel + 124, 0); Store8(Channel + 11, 19);
    Source(Primary, {0, 1}); Options(64, 96, 0, 0);
    Volume(Primary, .375, "retail type-19 BGM retains Music and master gain", .7);
    Options(64, 0, 128, 100);
    Volume(Primary, 0, "Music 0 still mutes actual retail BGM");

    std::printf("voice volume: %u checks passed through production update/category hooks\n", checks);
}
