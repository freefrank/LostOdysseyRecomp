// FConvexVolume::IntersectBox (sub_823BDA78: r3 = the volume, its planes at +12 in groups of four as X, Y, Z, W
// vectors and their count at +16; r4 = the box origin, r5 = its extent, three floats each). Returns 1 unless, for a
// plane, origin . normal - w > |extent| . |normal|: the per-primitive frustum test of the game's rendering thread,
// 1.3% of it in the busy street. The recompiled loop moves every vector through the context in memory.
//
// The same arithmetic as the recompiled machine code (b35's, objdump): with flushing on, distance = fmul, fmla, fmla,
// fsub, push-out = fmul, fmla, fmla, then fcmgt. Fused as it fuses them, in its order, so every lane rounds the same.
// Registers: r3 = the result; the recompiled code leaves only volatile ones (r6, r8-r11, vector registers) otherwise.
// LO_NATIVE_FRUSTUM_BOX=0 keeps the recompiled code; 2 runs both and compares (first 20 differences, a count every 2^20).
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <arm_neon.h>
#include <atomic>
#include <cstdlib>
#include <cstring>

extern "C" PPC_FUNC(__imp__sub_823BDA78);

namespace
{
    int NativeFrustumBoxMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_FRUSTUM_BOX");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    // Four big-endian floats at a guest address, lane i = the i-th in memory.
    float32x4_t LoadVector(uint8_t* base, uint32_t address)
    {
        uint8x16_t bytes = vld1q_u8(base + address);
        return vreinterpretq_f32_u8(vrev32q_u8(bytes));
    }

    // The sign bit cleared as vandc clears it (a bit operation, whatever flushing does).
    float32x4_t ClearSign(float32x4_t value)
    {
        return vreinterpretq_f32_u32(vbicq_u32(vreinterpretq_u32_f32(value), vdupq_n_u32(0x80000000u)));
    }

    float LoadFloat(uint8_t* base, uint32_t address)
    {
        const uint32_t bits = PPC_LOAD_U32(address);
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value;
    }

    uint32_t Intersects(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t volume = ctx.r3.u32, originAddress = ctx.r4.u32, extentAddress = ctx.r5.u32;
        const int32_t planes = int32_t(PPC_LOAD_U32(volume + 16));
        if (planes <= 0) return 1;
        uint32_t group = PPC_LOAD_U32(volume + 12);
        const float32x4_t ox = vdupq_n_f32(LoadFloat(base, originAddress));
        const float32x4_t oy = vdupq_n_f32(LoadFloat(base, originAddress + 4));
        const float32x4_t oz = vdupq_n_f32(LoadFloat(base, originAddress + 8));
        const float32x4_t ex = ClearSign(vdupq_n_f32(LoadFloat(base, extentAddress)));
        const float32x4_t ey = ClearSign(vdupq_n_f32(LoadFloat(base, extentAddress + 4)));
        const float32x4_t ez = ClearSign(vdupq_n_f32(LoadFloat(base, extentAddress + 8)));
        ctx.fpscr.enableFlushMode();
        for (int32_t plane = 0; plane < planes; plane += 4, group += 64)
        {
            const float32x4_t px = LoadVector(base, group), py = LoadVector(base, group + 16);
            const float32x4_t pz = LoadVector(base, group + 32), pw = LoadVector(base, group + 48);
            float32x4_t distance = vmulq_f32(ox, px);
            float32x4_t pushOut = vmulq_f32(ex, ClearSign(px));
            distance = vfmaq_f32(distance, oy, py);
            pushOut = vfmaq_f32(pushOut, ey, ClearSign(py));
            distance = vfmaq_f32(distance, oz, pz);
            pushOut = vfmaq_f32(pushOut, ez, ClearSign(pz));
            const uint32x4_t outside = vcgtq_f32(vsubq_f32(distance, pw), pushOut);
            if (vmaxvq_u32(outside)) return 0;
        }
        return 1;
    }

    void Check(PPCContext& ctx, uint8_t* base)
    {
        static std::atomic<uint32_t> calls, differences;
        PPCContext copy = ctx;
        const uint32_t native = Intersects(copy, base);
        const uint32_t volume = ctx.r3.u32;
        __imp__sub_823BDA78(ctx, base);
        const uint32_t n = calls.fetch_add(1) + 1;
        if (ctx.r3.u32 != native && differences.fetch_add(1) < 20)
            LOG_ERROR("[frustum box] sub_823BDA78 differs: volume {:08X} {}/{}", volume, native, ctx.r3.u32);
        if ((n & 0xFFFFF) == 0)
            LOG_ERROR("[frustum box] sub_823BDA78 checked {} calls, {} differ", n, differences.load());
    }
}

PPC_FUNC(sub_823BDA78)
{
    const int mode = NativeFrustumBoxMode();
    if (mode == 0)
    {
        __imp__sub_823BDA78(ctx, base);
        return;
    }
    if (mode == 2)
    {
        Check(ctx, base);
        return;
    }
    ctx.r3.u64 = Intersects(ctx, base);
}
