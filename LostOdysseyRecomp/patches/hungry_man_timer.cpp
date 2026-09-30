#include <stdafx.h>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <mutex>

extern "C" PPC_FUNC(__imp__sub_82846128);

namespace
{
// gr1_0_scrw frame 0 checks whether the food errand is active and its counter
// is at most 300, then increments that counter. This is the only 0x1039
// increment in the script. Match the loaded bytecode so other 0x0B commands
// retain their original behavior.
constexpr uint32_t CounterPc = 0x53C;
constexpr uint8_t CounterCode[] = {
    0x02, 0x37, 0x10, 0x77, 0x80, 0x00, 0x3F, 0x05,
    0x02, 0x39, 0x10, 0x78, 0x80, 0x05, 0x3F, 0x05,
    0x0B, 0x39, 0x10,
};
constexpr double OriginalUpdatesPerSecond = 30.0;

struct CounterState
{
    uint32_t frame = 0;
    uint32_t code = 0;
    double partialUpdates = 0;
};

std::mutex counterMutex;
CounterState counterState;
}

PPC_FUNC(sub_82846128)
{
    const uint32_t frame = ctx.r4.u32;
    const uint32_t pc = PPC_LOAD_U32(frame + 0x69C);
    if (pc != CounterPc || PPC_LOAD_U32(frame) != 0 ||
        PPC_LOAD_U32(frame + 0x490) != 0x496C)
    {
        __imp__sub_82846128(ctx, base);
        return;
    }

    const uint32_t code = PPC_LOAD_U32(frame + 0x494);
    if (!code || std::memcmp(base + code + CounterPc - 16, CounterCode, sizeof(CounterCode)) != 0)
    {
        __imp__sub_82846128(ctx, base);
        return;
    }

    PPCRegister elapsed{};
    elapsed.u64 = PPC_LOAD_U64(0x83315ED0);
    const double delta = elapsed.f64;
    if (!std::isfinite(delta) || delta < 0)
    {
        __imp__sub_82846128(ctx, base);
        return;
    }

    uint32_t updates = 0;
    {
        std::lock_guard lock(counterMutex);
        if (counterState.frame != frame || counterState.code != code)
            counterState = {frame, code, 0};

        // Count 30-FPS update equivalents only when this opcode runs. Using
        // time since its previous visit would also count other script work.
        counterState.partialUpdates += std::min(delta * OriginalUpdatesPerSecond, 301.0);
        updates = static_cast<uint32_t>(std::min(std::floor(counterState.partialUpdates + 1e-6), 301.0));
        counterState.partialUpdates = std::max(0.0, counterState.partialUpdates - updates);
    }

    if (updates == 0)
    {
        PPC_STORE_U32(frame + 0x69C, pc + 3);
        ctx.r3.s64 = 0;
        return;
    }

    const uint64_t opcodeTable = ctx.r3.u64;
    const uint64_t scriptFrame = ctx.r4.u64;
    for (uint32_t i = 0; i < updates; ++i)
    {
        if (i != 0) PPC_STORE_U32(frame + 0x69C, pc);
        ctx.r3.u64 = opcodeTable;
        ctx.r4.u64 = scriptFrame;
        __imp__sub_82846128(ctx, base);
    }
}
