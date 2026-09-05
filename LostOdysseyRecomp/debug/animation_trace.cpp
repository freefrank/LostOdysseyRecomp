#include <stdafx.h>
#include <os/logger.h>

extern std::atomic<uint32_t> g_presentedSwaps;
extern "C" PPC_FUNC(__imp__sub_822B5938);

// Opt-in observation of skeletal pose evaluation, without changing guest state.
PPC_FUNC(sub_822B5938)
{
    const uint32_t component = ctx.r3.u32;
    __imp__sub_822B5938(ctx, base);
    static const bool enabled = getenv("LO_TRACE_ANIMATION") != nullptr;
    if (!enabled) return;
    static std::mutex traceMutex;
    static std::unordered_map<uint32_t, uint32_t> lastFrames;
    std::lock_guard lock(traceMutex);
    const uint32_t frame = g_presentedSwaps.load();
    auto [it, inserted] = lastFrames.try_emplace(component, frame);
    if (!inserted && frame - it->second < 120) return;
    it->second = frame;
    const uint32_t mesh = PPC_LOAD_U32(component + 640);
    const uint32_t atoms = PPC_LOAD_U32(component + 720);
    const uint32_t count = PPC_LOAD_U32(component + 724);
    uint64_t hash = 14695981039346656037ull;
    if (atoms && count <= 512)
        for (uint32_t i = 0; i < count * 32; ++i) hash = (hash ^ base[atoms+i]) * 1099511628211ull;
    LOG_INFO("animation f{} component={:#x} mesh={:#x} tree={:#x} refpose={} animsets={} atoms={:#x}/{} hash={:x}",
        frame, component, mesh, PPC_LOAD_U32(component+648), PPC_LOAD_U32(component+840),
        PPC_LOAD_U32(component+764), atoms, count, hash);
}

extern "C" PPC_FUNC(__imp__sub_8258E0E8);
PPC_FUNC(sub_8258E0E8)
{
    const uint32_t component = ctx.r3.u32;
    const uint64_t name = ctx.r4.u64;
    __imp__sub_8258E0E8(ctx, base);
    static const bool enabled = getenv("LO_TRACE_ANIMATION") != nullptr;
    if (enabled)
    {
        static std::atomic<uint32_t> count{0};
        if (count++ < 400)
            LOG_INFO("find animation f{} component={:#x} name={:016x} sets={} result={:#x}",
                g_presentedSwaps.load(), component, name, PPC_LOAD_U32(component+764), ctx.r3.u32);
    }
}

extern "C" PPC_FUNC(__imp__sub_829B7A98);
PPC_FUNC(sub_829B7A98)
{
    static const bool enabled = getenv("LO_TRACE_ANIMATION") != nullptr;
    const uint32_t caller = uint32_t(ctx.lr);
    std::string path;
    if (enabled && ctx.r4.u32)
        for (uint32_t i = 0; i < 256; ++i) {
            const uint16_t c = PPC_LOAD_U16(ctx.r4.u32 + i * 2);
            if (!c) break;
            path += c < 128 ? char(c) : '?';
        }
    __imp__sub_829B7A98(ctx, base);
    if (enabled)
        LOG_INFO("animation asset f{} caller={:#x} '{}' result={:#x}",
            g_presentedSwaps.load(), caller, path, ctx.r3.u32);
}

extern "C" PPC_FUNC(__imp__sub_82B26518);
PPC_FUNC(sub_82B26518)
{
    static const bool enabled = getenv("LO_TRACE_ANIMATION") != nullptr;
    if (enabled) {
        LOG_INFO("actor resource f{} caller={:#x} this={:#x} input={:#x} actor={} model={} stack={:#x}",
            g_presentedSwaps.load(), uint32_t(ctx.lr), ctx.r3.u32, ctx.r4.u32,
            PPC_LOAD_U8(ctx.r4.u32+4), PPC_LOAD_U32(ctx.r4.u32+8), ctx.r1.u32);
    }
    __imp__sub_82B26518(ctx,base);
}

extern "C" PPC_FUNC(__imp__sub_828BB378);
void ArmGuestWriteWatchpoint(uint32_t address, uint32_t length);
PPC_FUNC(sub_828BB378)
{
    const uint32_t actor = ctx.r3.u32;
    __imp__sub_828BB378(ctx, base);
    static const bool enabled = getenv("LO_TRACE_MODEL") != nullptr;
    if (enabled) {
        LOG_INFO("actor created f{} actor={:#x} id={} model={}", g_presentedSwaps.load(), actor,
            PPC_LOAD_U32(actor+64),PPC_LOAD_U32(actor+72));
        if (g_presentedSwaps >= 1000) ArmGuestWriteWatchpoint(actor+72,4);
    }
}

extern "C" PPC_FUNC(__imp__sub_82AF6290);
PPC_FUNC(sub_82AF6290)
{
    static const bool experiment = getenv("LO_TEST_NORMAL_MODEL") != nullptr;
    const uint32_t data = PPC_LOAD_U32(ctx.r3.u32+0x20);
    const uint32_t model = data ? PPC_LOAD_U32(data+0x80) : 0;
    if (experiment && model == 11) {
        LOG_INFO("model A/B: temporarily constructing Kaim with normal model 0 (saved model 11)");
        PPC_STORE_U32(data+0x80,0);
        __imp__sub_82AF6290(ctx,base);
        PPC_STORE_U32(data+0x80,model);
    } else __imp__sub_82AF6290(ctx,base);
}

extern "C" PPC_FUNC(__imp__sub_829E6AF8);
PPC_FUNC(sub_829E6AF8)
{
    static const bool enabled = getenv("LO_TRACE_ANIMATION") != nullptr;
    if (enabled)
        LOG_INFO("SetBattleCharaResource f{} char={} model={} caller={:#x}",
            g_presentedSwaps.load(), ctx.r4.u32, ctx.r5.u32, uint32_t(ctx.lr));
    __imp__sub_829E6AF8(ctx, base);
}
