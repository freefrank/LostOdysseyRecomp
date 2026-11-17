#include <stdafx.h>
#include <kernel/memory.h>
#include <settings/config.h>
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>

PPC_FUNC(sub_823BE570);
PPC_FUNC(sub_823BE9D0);
PPC_FUNC(sub_823D5E50);

namespace {
constexpr uint32_t Renderer = 0x1000, Views = 0x4000, Bits = 0x9000;
constexpr uint32_t Primitives = 0x10000, Interactions = 0x180000;
alignas(4) std::array<uint8_t, 0x400000> memory{};
settings::Config config;
std::vector<uint32_t> interactions, forwarded;
uint32_t renderArgument = Renderer;
uint64_t loads = 0, checks = 0, depthCalls = 0;
bool collect = true;
unsigned mutate = 0;

void Check(bool condition, const char* message) {
    ++checks;
    if (!condition) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
uint32_t Primitive(uint32_t i) { return Primitives + i * 0xA0; }
uint32_t Read(uint32_t address) {
    uint32_t value;
    std::memcpy(&value, memory.data() + address, 4);
    return __builtin_bswap32(value);
}
void Write(uint32_t address, uint32_t value) {
    value = __builtin_bswap32(value);
    std::memcpy(memory.data() + address, &value, 4);
}
void Float(uint32_t address, float value) { Write(address, std::bit_cast<uint32_t>(value)); }
void Visible(uint32_t i, bool visible, uint32_t bits = Bits) {
    const uint32_t address = bits + (i / 32) * 4, mask = 1u << (i % 32);
    Write(address, visible ? Read(address) | mask : Read(address) & ~mask);
}
void View(uint32_t renderer, uint32_t views, uint32_t bits, uint32_t count = 1) {
    Write(renderer + 0x30, views); Write(renderer + 0x34, count);
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t view = views + i * 0x6F0;
        Float(view + 0x24, 1); Float(view + 0x28, 1);
        Float(view + 0x80, 1); Float(view + 0x94, 1);
        Float(view + 0x13C, 1); Write(view + 0x25C, bits);
    }
}
uint32_t Interaction(uint32_t primitive, uint32_t flags = 0x10000000) {
    const uint32_t address = Interactions + uint32_t(interactions.size()) * 0x10;
    Write(address + 8, primitive); Write(address + 0xC, flags);
    interactions.push_back(address);
    return address;
}
void Frame() {
    forwarded.clear(); loads = 0;
    PPCContext ctx; ctx.r3.u32 = renderArgument;
    sub_823BE570(ctx, memory.data());
}
void Reset(uint32_t count, uint32_t lights = 1, bool visible = true) {
    config.dynamicShadows = 1; interactions.clear(); Frame();
    memory.fill(0); renderArgument = Renderer;
    View(Renderer, Views, Bits);
    for (uint32_t i = 0; i < count; ++i) {
        Write(Primitive(i) + 0x20, i); Float(Primitive(i) + 0x40, 100.0f - float(i));
        Visible(i, visible);
        for (uint32_t light = 0; light < lights; ++light) Interaction(Primitive(i));
    }
    config.dynamicShadows = 2;
}
std::set<uint32_t> Unique() { return {forwarded.begin(), forwarded.end()}; }
void Observe(const char* name) {
    std::printf("%s interactions=%zu unique=%zu loads=%llu\n", name, forwarded.size(), Unique().size(),
        static_cast<unsigned long long>(loads));
}
void Observations() {
    // Deliberately first: the upstream frame counter starts below the grace length.
    Reset(60, 1, false); Frame(); Observe("never_visible");
    Reset(12); for (uint32_t i = 0; i < 12; ++i) Float(Primitive(i) + 0x40, 100);
    Frame(); Frame(); Observe("equal_sizes");
    Reset(10); interactions.clear();
    for (uint32_t i = 0; i < 10; ++i) {
        Float(Primitive(i) + 0x40, 100.0f - float(i) * 10);
        for (uint32_t light = 0; light < (i == 0 ? 8u : 1u); ++light) Interaction(Primitive(i));
    }
    Frame(); Frame(); Observe("multiple_lights");
    Reset(8, 4); Frame(); Frame(); Observe("cache_four_lights");
}
void Tests() {
    Reset(60, 1, false); Frame();
    Check(forwarded.empty(), "a caster never seen on screen has no off-screen grace");

    Reset(12);
    for (uint32_t i = 0; i < 12; ++i) Float(Primitive(i) + 0x40, 100);
    for (unsigned frame = 0; frame < 8; ++frame) {
        Frame(); Check(Unique().size() == 8, "ties never exceed the eight-caster budget");
        Check(Unique().contains(Primitive(0)) && !Unique().contains(Primitive(11)), "equal-size selection is stable");
    }
    std::reverse(interactions.begin(), interactions.end()); Frame();
    Check(Unique().size() == 8 && Unique().contains(Primitive(0)) && !Unique().contains(Primitive(11)),
        "warm selection does not depend on interaction traversal order");

    Reset(10); interactions.clear();
    for (uint32_t i = 0; i < 10; ++i) {
        Float(Primitive(i) + 0x40, 100.0f - float(i) * 10);
        for (uint32_t light = 0; light < (i == 0 ? 8u : 1u); ++light) Interaction(Primitive(i));
    }
    Frame(); Frame();
    Check(Unique().size() == 8 && forwarded.size() == 15 && Unique().contains(Primitive(7)),
        "eight lights on one caster consume one ranking slot");

    Reset(8, 4); Frame();
    Check(forwarded.size() == 32, "all eligible light interactions of the eight casters survive");
    const auto fourLightLoads = loads;
    interactions.resize(1); Frame();
    Check(forwarded.size() == 1, "a smaller scene does not keep phantom casters");
    if (!std::getenv("LO_SHADOW_CACHE_VERIFY"))
        Check(fourLightLoads < loads * 16, "repeated lights reuse visibility and projected-size reads");

    Reset(1, 3); Frame(); Visible(0, false);
    for (unsigned frame = 1; frame <= 15; ++frame) {
        Frame(); Check(forwarded.size() == 3, "an observed caster retains all lights for 15 off-screen frames");
    }
    Frame(); Check(forwarded.empty(), "off-screen grace ends at frame 16");
    Visible(0, true); Frame(); Check(forwarded.size() == 3, "returning caster regains its shadow");
    Visible(0, false); Write(Primitive(0) + 0x20, 31); Frame();
    Check(forwarded.empty(), "a reused primitive address does not inherit grace from a different index");

    Reset(9); for (uint32_t i = 0; i < 9; ++i) Float(Primitive(i) + 0x40, 100);
    Frame(); Float(Primitive(8) + 0x40, 120); Frame(); Frame();
    Check(!Unique().contains(Primitive(8)), "20 percent size jitter retains incumbent shadows");
    Float(Primitive(8) + 0x40, 126); Frame(); Frame();
    Check(Unique().size() == 8 && Unique().contains(Primitive(8)), "a substantially larger challenger displaces an incumbent");

    Reset(9); Frame();
    // A new bound / transform is sampled on the next InitDynamicShadows.
    Float(Primitive(8) + 0x40, 1000); Frame(); Frame();
    Check(Unique().contains(Primitive(8)), "changed bounds invalidate cached size at the next pass");
    Float(Views + 0x13C, 100); Frame(); Frame();
    Check(Unique().contains(Primitive(8)), "a changed camera is evaluated again");
    Reset(9); Float(Views + 0x10C, 1);
    for (uint32_t i = 0; i < 9; ++i) Float(Primitive(i) + 0x28, 100);
    Frame(); Float(Primitive(8) + 0x28, 0); Frame(); Frame();
    Check(Unique().contains(Primitive(8)), "a changed primitive position invalidates projected size on the next pass");

    Reset(1); Write(Renderer + 0x34, 2);
    View(Renderer, Views, 0, 2); Write(Views + 0x6F0 + 0x25C, Bits);
    Frame(); Check(forwarded.size() == 1, "visibility in any view is sufficient");
    Reset(1, 1, false);
    const uint32_t child = 0x140000;
    Write(Primitive(0) + 0x8C, child); Write(child + 0x20, 7);
    Float(child + 0x40, 5); Float(child + 0x28, 20); Visible(7, true);
    Frame(); Check(forwarded.size() == 1, "an attached visible child keeps its parent's shadow");
    Write(child + 0x90, child); Frame();
    Check(forwarded.size() == 1, "cyclic attachment lists retain the existing 64-child guard");
    Reset(9); Frame(); Write(Primitive(8) + 0x8C, child);
    Float(child + 0x40, 1000); Float(child + 0x28, 20); Frame(); Frame();
    Check(Unique().contains(Primitive(8)), "changed attached bounds participate in the next ranking");

    Reset(1); Float(Primitive(0) + 0x40, std::nanf("")); Frame(); Frame();
    Check(forwarded.size() == 1, "non-finite bounds retain the original sanitized zero-size behavior");
    Reset(1); View(0x3000, 0x6000, 0xB000); renderArgument = 0x3000;
    Frame(); Check(forwarded.empty(), "another renderer cannot reuse the first view's visibility");
    Reset(1); Frame();
    View(Renderer, 0x6000, 0xB000); Frame();
    Check(forwarded.empty(), "replaced views at the same renderer address discard old visibility grace");

    if (std::getenv("LO_SHADOW_CACHE_VERIFY")) {
        for (unsigned change : {1u, 2u, 3u}) {
            Reset(1, 3); mutate = change; Frame(); mutate = 0;
            Check(forwarded.size() == 3, "cache verification fails open after a mid-enumeration input mutation");
            Visible(0, false); Frame();
            Check(forwarded.empty(), "verification failure discards the invalid pass's history");
        }
    }

    Reset(2, 3, false); config.dynamicShadows = 1; Frame();
    Check(forwarded.size() == 6 && loads == 0, "On delegates without inspecting guest shadow state");
    config.dynamicShadows = 0; Frame();
    Check(forwarded.size() == 6 && loads == 0, "Off keeps the original initialization path");
    for (uint32_t mode : {1u, 2u, 0u}) {
        config.dynamicShadows = mode; depthCalls = 0;
        PPCContext ctx; ctx.r3.u64 = 0x123456789ABCDEF0ull;
        sub_823D5E50(ctx, memory.data());
        Check(depthCalls == (mode ? 1u : 0u), "On/Low render depths; Off alone skips the depth function");
        Check(ctx.r3.u64 == (mode ? 0xFEDCBA9876543210ull : 0x123456789ABCDEF0ull), "guest return register is preserved");
    }

    Reset(2, 1, false); Write(interactions[0] + 0xC, 0); Write(Primitive(1) + 0x88, 1); Frame();
    Check(forwarded.size() == 2, "ineligible interactions retain original guest preconditions");
    // The persistent history must stay bounded without dereferencing old pointers.
    Reset(5000, 1, false); Frame(); Frame();
    Check(forwarded.size() == 5000 - 4096, "history stops at 4096 entries and delegates overflow interactions");
    Reset(1); Frame(); Check(forwarded.size() == 1, "normal operation recovers after overflow");
    // A function call outside the synchronous Init scope must not reuse its cache.
    forwarded.clear(); PPCContext direct; direct.r3.u32 = Renderer;
    direct.r4.u32 = interactions.front(); direct.lr = 0x823BE600;
    sub_823BE9D0(direct, memory.data());
    Check(forwarded.size() == 1, "unscoped calls retain original behavior");
    // Independently check the budget and all-or-none interactions under moving
    // bounds, changing visibility, different light counts and traversal order.
    Reset(40); uint32_t random = 73451;
    const auto next = [&] { random = random * 1664525u + 1013904223u; return random; };
    for (unsigned step = 0; step < 300; ++step) {
        interactions.clear();
        std::array<unsigned, 40> lightCounts{};
        for (uint32_t i = 0; i < 40; ++i) {
            Float(Primitive(i) + 0x40, 1.0f + float(next() % 1000));
            Visible(i, (next() % 5) != 0);
            lightCounts[i] = 1 + next() % 4;
            for (unsigned light = 0; light < lightCounts[i]; ++light) Interaction(Primitive(i));
        }
        if (step % 2) std::reverse(interactions.begin(), interactions.end());
        Frame();
        Check(Unique().size() <= 8, "the caster cap holds across animated multi-light scenes");
        for (uint32_t i = 0; i < 40; ++i) {
            const auto count = std::count(forwarded.begin(), forwarded.end(), Primitive(i));
            Check(count == 0 || count == lightCounts[i], "all lights share one decision for a caster in a pass");
        }
    }
    std::printf("PASS: %llu production shadow-budget checks\n", static_cast<unsigned long long>(checks));
}
void Benchmark() {
    Reset(8, 4); View(Renderer, Views, Bits, 4);
    // Shared attached bounds make repeated screen-size work visible without GPU work.
    for (uint32_t i = 0; i < 8; ++i) {
        uint32_t parent = Primitive(i);
        for (uint32_t c = 0; c < 8; ++c) {
            const uint32_t child = 0x140000 + (i * 8 + c) * 0xA0;
            Write(parent + (c ? 0x90 : 0x8C), child); parent = child;
            Write(child + 0x20, 100 + c); Float(child + 0x28, float(c)); Float(child + 0x40, 5);
        }
    }
    for (unsigned i = 0; i < 100; ++i) Frame();
    collect = false;
    constexpr unsigned frames = 20000;
    const auto start = std::chrono::steady_clock::now();
    for (unsigned i = 0; i < frames; ++i) Frame();
    const auto elapsed = std::chrono::steady_clock::now() - start;
    std::printf("frames=%u ns_per_init=%.2f loads_last_init=%llu\n", frames,
        std::chrono::duration<double, std::nano>(elapsed).count() / frames, static_cast<unsigned long long>(loads));
}
}

Memory g_memory;
void* Memory::Translate(size_t address) const {
    if (address > memory.size() - 4 || address % 4) { std::fprintf(stderr, "invalid fixture address: %zx\n", address); std::abort(); }
    ++loads;
    return memory.data() + address;
}
const settings::Config& settings::GetConfig() { return config; }
extern "C" PPC_FUNC(__imp__sub_823BE570) {
    (void)ctx;
    size_t ordinal = 0;
    for (uint32_t interaction : interactions) {
        if (ordinal++ == 1 && mutate) {
            if (mutate == 1) Visible(0, false);
            if (mutate == 2) Float(Primitive(0) + 0x40, 250);
            if (mutate == 3) Float(Views + 0x13C, 2);
        }
        PPCContext draw; draw.r3.u32 = renderArgument; draw.r4.u32 = interaction; draw.lr = 0x823BE600;
        sub_823BE9D0(draw, base);
    }
}
extern "C" PPC_FUNC(__imp__sub_823BE9D0) {
    (void)base;
    if (ctx.r3.u32 != renderArgument || ctx.lr != 0x823BE600) std::abort();
    if (collect) forwarded.push_back(Read(ctx.r4.u32 + 8));
}
extern "C" PPC_FUNC(__imp__sub_823D5E50) { (void)base; ++depthCalls; ctx.r3.u64 = 0xFEDCBA9876543210ull; }
extern "C" PPC_FUNC(__imp__sub_826E1860) { (void)ctx; (void)base; }
extern "C" PPC_FUNC(__imp__sub_823E3900) { (void)ctx; (void)base; }
extern "C" PPC_FUNC(__imp__sub_823E4E70) { (void)ctx; (void)base; }
extern "C" PPC_FUNC(__imp__sub_823E6F48) { (void)ctx; (void)base; }

int main(int argc, char** argv) {
    if (argc == 2 && std::strcmp(argv[1], "--observe") == 0) Observations();
    else if (argc == 2 && std::strcmp(argv[1], "--bench") == 0) Benchmark();
    else Tests();
}
