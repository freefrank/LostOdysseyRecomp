// Appended to verbatim generated PPC routines by guest_language_test.py.
// Synthetic tables exercise language semantics, not any particular game scene.
#include <settings/language_selection.h>
#include <cstdio>
#include <stdexcept>
#ifdef _WIN32
#include <windows.h>
#else
#include <sys/mman.h>
#endif
namespace {
constexpr uint32_t Table = 0x8336A5F0, Cache = 0x83318000;
unsigned hostLanguage = 1;
bool overrideEnabled = true;
void Require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
void PutString(uint8_t* base, uint32_t address, const char* value) {
    do { PPC_STORE_U16(address, uint16_t(*value)); address += 2; } while (*value++);
}
void Initialize(uint8_t* base, unsigned host = 1) {
    hostLanguage = host;
    overrideEnabled = true;
    std::memset(base + Table, 0, 424);
    std::memset(base + Cache, 0, 32);
    const char* names[] = {"JPN", "INT", "JPN", "DEU", "FRA", "ESN", "ITA"};
    for (unsigned id = 0; id != 7; ++id) {
        PPC_STORE_U16(Table + id * 14, id);
        PutString(base, Table + id * 14 + 2, names[id]);
        const uint32_t pointer = 0x20000 + id * 32;
        PutString(base, pointer, names[id]);
        PPC_STORE_U32(0x832455F0 + id * 4, pointer);
    }
    PPC_STORE_U8(Table + 416, 7);
    PPC_STORE_U8(Table + 419, 5);
    const unsigned voice[] = {1, 2, 3, 4, 6};
    for (unsigned i = 0; i != 5; ++i) PPC_STORE_U16(Table + 288 + i * 2, voice[i]);
}
uint32_t Call(PPCFunc* function, uint8_t* base, uint32_t r3, uint32_t r4 = 0, uint32_t r5 = 0) {
    PPCContext ctx{};
    ctx.r1.u64 = 0x1F000;
    ctx.r3.u64 = r3; ctx.r4.u64 = r4; ctx.r5.u64 = r5;
    function(ctx, base);
    Require(ctx.r1.u32 == 0x1F000, "native stack balanced");
    return ctx.r3.u32;
}
unsigned Apply(uint8_t* base, unsigned index) {
    auto record = Call(sub_82482038, base, index);
    Call(sub_82481E78, base, record);
    return Call(sub_82481F40, base, 0);
}
}
PPC_FUNC(sub_82481BE8) {
    // Existing production host policy, intentionally unchanged for the audit.
    if (overrideEnabled && settings::language::ResourceOverride(hostLanguage, ctx.r3.u32, ctx.r4.u32)) {
        ctx.r3.u64 = PPC_LOAD_U32(0x832455F0 + hostLanguage * 4);
        return;
    }
    native_lookup(ctx, base);
}
// These are ABI register spill helpers, not language behavior.
PPC_FUNC(__savegprlr_29) {
    PPC_STORE_U32(ctx.r1.u32 - 8, ctx.r12.u32);
    PPC_STORE_U64(ctx.r1.u32 - 16, ctx.r31.u64);
    PPC_STORE_U64(ctx.r1.u32 - 24, ctx.r30.u64);
    PPC_STORE_U64(ctx.r1.u32 - 32, ctx.r29.u64);
}
PPC_FUNC(__savegprlr_28) {
    __savegprlr_29(ctx, base);
    PPC_STORE_U64(ctx.r1.u32 - 40, ctx.r28.u64);
}
PPC_FUNC(__restgprlr_29) {
    ctx.lr = PPC_LOAD_U32(ctx.r1.u32 - 8);
    ctx.r31.u64 = PPC_LOAD_U64(ctx.r1.u32 - 16);
    ctx.r30.u64 = PPC_LOAD_U64(ctx.r1.u32 - 24);
    ctx.r29.u64 = PPC_LOAD_U64(ctx.r1.u32 - 32);
}
PPC_FUNC(__restgprlr_28) {
    ctx.r28.u64 = PPC_LOAD_U64(ctx.r1.u32 - 40);
    __restgprlr_29(ctx, base);
}
// ASCII UTF16 fixture names need no locale collation. Use the game's generated
// ordinal comparator at the CRT collation boundary; every string is non-null.
PPC_FUNC(sub_82BE19E8) { sub_822D03D8(ctx, base); }
int main() {
    uint8_t* base = nullptr;
#ifdef _WIN32
    base = static_cast<uint8_t*>(VirtualAlloc(nullptr, size_t(1) << 32, MEM_RESERVE, PAGE_NOACCESS));
    Require(base != nullptr, "reserve guest address space");
    for (uint32_t address : {0x10000u, 0x20000u, 0x83240000u, 0x83310000u, 0x83360000u})
        Require(VirtualAlloc(base + address, 0x10000, MEM_COMMIT, PAGE_READWRITE), "commit fixture pages");
#else
    base = static_cast<uint8_t*>(mmap(nullptr, size_t(1) << 32, PROT_READ | PROT_WRITE,
        MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE, -1, 0));
    Require(base != MAP_FAILED, "reserve guest address space");
#endif
    try {
        for (unsigned host : {1u, 2u, 5u}) {
            Initialize(base, host);
            const unsigned expected[] = {1, 2, 3, 4, 6};
            for (unsigned index = 0; index != 5; ++index)
                Require(Apply(base, index) == expected[index], "voice index and text language independence");
        }
        Initialize(base);
        Require(baseline_menu::VoiceLanguage(base, 99) == 6, "baseline clamps corrupt selection to Italian");
        Require(current_menu::VoiceLanguage(base, 99) == 0, "current menu rejects corrupt selection");
        PPC_STORE_U8(Table + 419, 0);
        Require(baseline_menu::VoiceCount(base) == 1 && baseline_menu::VoiceLanguage(base, 0) == 1,
            "baseline invents English option from stale empty table");
        Require(current_menu::VoiceCount(base) == 0 && current_menu::VoiceLanguage(base, 0) == 0,
            "current menu preserves empty table state");
        PPC_STORE_U8(Table + 419, 17);
        Require(current_menu::VoiceCount(base) == 0 && current_menu::VoiceLanguage(base, 0) == 0,
            "current menu rejects corrupt count");
        PPC_STORE_U8(Table + 419, 16);
        PPC_STORE_U16(Table + 288 + 15 * 2, 2);
        Require(current_menu::VoiceLanguage(base, 15) == 2, "current menu supports all native slots");
        Initialize(base);
        PPC_STORE_U8(Table + 420, 1);
        PPC_STORE_U16(Table + 320, 5);
        PPC_STORE_U16(Table + 322, 5);
        PPC_STORE_U16(Table + 324, 1);
        const auto spanish = PPC_LOAD_U32(0x832455F0 + 5 * 4);
        const auto alias = Call(sub_82481CD0, base, Table, spanish, 0);
        Require(Call(sub_82481C58, base, Table, alias) == 1, "voice alias maps Spanish to English");
        Require(Call(sub_82482038, base, 5) == 0, "out of range index returns null");
        Require(Call(sub_82482038, base, 0xFFFFFFFF) == 0, "negative index returns null");
        Require(!settings::language::ValidVoiceIndex(5, 99), "host guard blocks invalid voice mutation");
        Require(!settings::language::ValidVoiceIndex(0, 0), "host guard blocks empty list");
        Require(settings::language::ValidVoiceIndex(16, 15), "native full table capacity");
        // Demonstrates a real semantic discrepancy on a synthetic supported-list
        // boundary, NOT evidence that issue #54's actual resources contain it.
        Initialize(base, 5);
        overrideEnabled = false;
        Require(Apply(base, 99) == 2, "native invalid selection uses table default Japanese");
        overrideEnabled = true;
        Require(Apply(base, 99) == 5, "host fallback substitutes unsupported Spanish voice");
        std::puts("PASS: extracted baseline/current menu: stale empty table, invalid index/count, native 16-slot capacity");
        std::puts("PASS: native PPC mapping, independent text/voice, alias, cached reverse ID, invalid index");
        std::puts("REPRO: synthetic invalid index: native effective voice=2; existing host override effective voice=5 (unsupported)");
        std::puts("LIMIT: synthetic tables; no save, scene, playback, locale collation, or issue #54 root-cause verification");
    } catch (const std::exception& e) { std::fprintf(stderr, "FAIL: %s\n", e.what()); return 1; }
#ifdef _WIN32
    VirtualFree(base, 0, MEM_RELEASE);
#else
    munmap(base, size_t(1) << 32);
#endif
}

