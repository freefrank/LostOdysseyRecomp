#include <discord/game_state.h>
#include <debug/map_info.h>
#include <debug/menu_animations.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <sys/mman.h>
#endif

namespace
{
bool menuOpen = false, moviePlaying = false, mapBusy = false, enabled = true;
unsigned publishes = 0;
debug_menu::MapInfo map{true, 7, L"Road to Uhra - The Great Gate", L"private_package"};
std::mutex mapMutex;

void Check(bool ok, const char* message)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}

class GuestMemory
{
    static constexpr size_t Size = 0x84000000ull;
public:
    uint8_t* base = nullptr;
    GuestMemory()
    {
#ifdef _WIN32
        base = static_cast<uint8_t*>(VirtualAlloc(nullptr, Size, MEM_RESERVE, PAGE_NOACCESS));
        Check(base != nullptr, "reserve sparse guest address space");
        for (size_t p : {0x100000u, 0x200000u, 0x300000u, 0x400000u, 0x500000u,
                 0x83264000u, 0x83268000u, 0x8326D000u, 0x83315000u, 0x83318000u})
            Check(VirtualAlloc(base + p, 4096, MEM_COMMIT, PAGE_READWRITE) != nullptr,
                "commit fixture guest page");
#else
        base = static_cast<uint8_t*>(mmap(nullptr, Size, PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
        Check(base != MAP_FAILED, "map sparse guest address space");
#endif
    }
    ~GuestMemory()
    {
#ifdef _WIN32
        VirtualFree(base, 0, MEM_RELEASE);
#else
        munmap(base, Size);
#endif
    }
    void Word(uint32_t p, uint32_t value)
    {
        for (unsigned i = 0; i < 4; ++i) base[p + i] = uint8_t(value >> (24 - i * 8));
    }
};
}

// Match the production map accessor's uncontended mutex/copy cost. These stubs
// are fixture state only; no game, Discord client, media or user data is opened.
bool debug_menu::TryGetMapInfo(debug_menu::MapInfo& out)
{
    if (mapBusy) return false;
    std::unique_lock lock(mapMutex, std::try_to_lock);
    if (!lock.owns_lock()) return false;
    out = map;
    return true;
}
namespace settings { bool IsOpen() { return menuOpen; } }
namespace movie { bool GuestPlaying() { return moviePlaying; } }
void discord_rpc::Publish(const Snapshot&) { ++publishes; }
bool discord_rpc::Enabled() noexcept { return enabled; }

int main()
{
    using namespace discord_rpc;
    using namespace discord_rpc::game_state;
    GuestMemory memory;
    auto* base = memory.base;
    memory.Word(0x83315FB4, 0x100000);
    memory.Word(0x83318744, 0x200000);
    memory.Word(0x100000 + 0x2B8, 0x300000);
    memory.Word(0x100000 + 0x2BC, 1);
    memory.Word(0x300000, 0x400000);
    memory.Word(0x400000 + 0x40, 0x500000);
    memory.Word(0x500000 + 0x5C8, 0x08000000);
    memory.Word(0x200000 + 472, 2);
    memory.Word(0x200000 + 476, 2);
    memory.Word(debug_menu::menu_animations::CampMenu, 1);
    base[0x832640B0 + 9] = 2;

    auto current = Sample(base);
    Check(current.disc == 2 && current.mode == Mode::Exploring && current.location ==
        "Road to Uhra - The Great Gate", "field snapshot uses native disc and retail location");
    Check(current.location.find("private_package") == std::string::npos, "package labels stay private");
    mapBusy = true;
    Check(Sample(base, current.location).location == current.location,
        "a contended map reader retains the last location without waiting");
    memory.Word(0x832644EC, 0x600000);
    Check(Sample(base, current.location).location.empty(), "title clears even a contended/stale map name");
    memory.Word(0x832644EC, 0);
    mapBusy = false;
    memory.Word(0x200000 + 472, 3);
    Check(Sample(base).mode == Mode::Battle, "entering battle");
    memory.Word(0x200000 + 472, 2);
    memory.Word(0x200000 + 476, 3);
    Check(Sample(base).mode == Mode::Battle, "battle results and return transition");
    memory.Word(0x200000 + 476, 2);
    memory.Word(0x500000 + 0x5C8, 0);
    Check(Sample(base).mode == Mode::Cutscene, "field event owns player control");
    memory.Word(debug_menu::menu_animations::CampMenu, 7);
    Check(Sample(base).mode == Mode::Menu, "camp and its child tasks override field control");
    memory.Word(debug_menu::menu_animations::CampMenu, 1);
    memory.Word(0x832688E8 + 20, 47);
    Check(Sample(base).mode == Mode::Menu, "native save/load task");
    memory.Word(0x832688E8 + 20, 1);
    menuOpen = true;
    Check(Sample(base).mode == Mode::Menu, "host settings");
    menuOpen = false;
    memory.Word(0x832644EC, 0x600000);
    Check(Sample(base).mode == Mode::Title && Sample(base).location.empty(),
        "title discards a stale field location");
    menuOpen = true;
    Check(Sample(base).mode == Mode::Menu, "settings opened from title");
    moviePlaying = true;
    Check(Sample(base).mode == Mode::Movie, "native movie lifecycle wins over retained scene state");
    moviePlaying = menuOpen = false;
    memory.Word(0x832644EC, 0);
    memory.Word(0x500000 + 0x5C8, 0x08000000);
    base[0x832640B0 + 9] = 5;
    Check(Sample(base).disc == 0, "invalid native disc is omitted");
    base[0x832640B0 + 9] = 2;
    map.name = L"\u4e4c\u62c9 \xD83C\xDF19\xE001";
    Check(Sample(base).location == "\xE4\xB9\x8C\xE6\x8B\x89 \xF0\x9F\x8C\x99",
        "localized map UTF-16 converts to UTF-8 without private font codes");
    map.available = false;
    Check(Sample(base).location.empty(), "unknown or streaming map has no debug fallback");
    map.available = true;
    map.name = L"Road to Uhra - The Great Gate";
    memory.Word(0x100000 + 0x2BC, 5);
    Check(Sample(base).mode == Mode::Exploring, "invalid player list never invents a cutscene");
    memory.Word(0x100000 + 0x2BC, 1);
    Check(Sample(nullptr) == Snapshot{}, "null guest memory is safe");

    using Clock = std::chrono::steady_clock;
    constexpr unsigned SkipIterations = 1'000'000, SampleIterations = 100'000;
    enabled = false;
    Tick(base);
    Check(publishes == 0, "disabled tick performs no guest sampling or publishing");
    enabled = true;
    Tick(base);
    Check(publishes == 1, "first tick publishes immediately");
    const auto skipStart = Clock::now();
    for (unsigned i = 0; i < SkipIterations; ++i) Tick(base);
    const auto skipEnd = Clock::now();
    Check(publishes <= 2, "per-frame calls coalesce to approximately one snapshot per second");
    size_t bytes = 0;
    const auto sampleStart = Clock::now();
    for (unsigned i = 0; i < SampleIterations; ++i) bytes += Sample(base).location.size();
    const auto sampleEnd = Clock::now();
    Check(bytes != 0, "benchmark consumes sampled names");
    std::printf("Discord game-state fixture passed; Tick gate %.1f ns/call (%u calls), "
        "full snapshot %.3f us/sample (%u samples).\n",
        std::chrono::duration<double, std::nano>(skipEnd - skipStart).count() / SkipIterations,
        SkipIterations,
        std::chrono::duration<double, std::micro>(sampleEnd - sampleStart).count() / SampleIterations,
        SampleIterations);
}
