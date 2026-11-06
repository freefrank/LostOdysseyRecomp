// Native versions of small, hot math functions of the game: particle distribution lookups, a bone's world matrix and
// some of the collision tests. Each one gives the same results as the recompiled code as GCC builds it for the Switch,
// bit for bit: the guest's loads, stores and order of operations, single precision, its fmadds/fmsubs/fnmsubs as one
// fused double operation rounded to single (as the recompiled code computes them), negations where the guest has them,
// and its comparisons, NaN cases included. The one difference left is which payload comes out when two different NaNs
// meet in a fused operation (GCC picks the operand order there). The bone matrix's VMX dot products run in
// flush-to-zero mode like the recompiled VMX code.
// Left to the recompiled code: the VMX matrix inverse (sub_822BC650), where GCC fuses multiplies and adds in pairs of
// its own choosing, and the rotation matrix, actor LocalToWorld, box transforms and line-box test (sub_8229E728,
// sub_822D89B0, sub_822AECB0, sub_822C3058, sub_826D0018), whose exact native versions were no faster than GCC's build.
// None of the callers reads a register these functions leave changed, f1 included, other than an r3 they return
// (checked over every direct call site).
// LO_NATIVE_MATH=0 keeps the recompiled code; 2 runs both on each call and logs the first differences.
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>

// The arithmetic below is written operation by operation; the compiler must not fuse a multiply and an add the guest
// rounds separately.
#if defined(__clang__)
#pragma clang fp contract(off)
#else
#pragma GCC optimize("fp-contract=off")
#endif

extern "C" PPC_FUNC(__imp__sub_822C73E8);
extern "C" PPC_FUNC(__imp__sub_822B0A00);
extern "C" PPC_FUNC(__imp__sub_826C8FC0);
extern "C" PPC_FUNC(__imp__sub_82744880);
extern "C" PPC_FUNC(__imp__sub_82796B88);
extern "C" PPC_FUNC(__imp__sub_822B1C00);

namespace
{
    constexpr uint32_t kZero = 0x82000E50;           // 0.0f
    constexpr uint32_t kOne = 0x8218958C;            // 1.0f
    constexpr uint32_t kGuestIdentityMatrix = 0x8323A520;
    constexpr size_t kMatrixSize = 64;

    // 0: recompiled code. 1: native. 2: both, compared.
    int NativeMathMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_MATH");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    uint32_t LoadU32(uint8_t* base, uint32_t address)
    {
        return ByteSwap(*reinterpret_cast<uint32_t*>(base + address));
    }

    void StoreU32(uint8_t* base, uint32_t address, uint32_t value)
    {
        *reinterpret_cast<uint32_t*>(base + address) = ByteSwap(value);
    }

    double LoadDouble(uint8_t* base, uint32_t address)
    {
        const uint64_t value = ByteSwap(*reinterpret_cast<uint64_t*>(base + address));
        double result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    float LoadFloat(uint8_t* base, uint32_t address)
    {
        const uint32_t value = LoadU32(base, address);
        float result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    void StoreFloat(uint8_t* base, uint32_t address, float value)
    {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        StoreU32(base, address, bits);
    }

    float32x4_t LoadRow(const uint8_t* p)
    {
        return vreinterpretq_f32_u8(vrev32q_u8(vld1q_u8(p)));
    }

    void StoreRow(uint8_t* p, float32x4_t row)
    {
        vst1q_u8(p, vrev32q_u8(vreinterpretq_u8_f32(row)));
    }

    // fneg where the guest has one. GCC would otherwise fold a negation into the operation before it (an fnmadd or
    // fmsub, which changes the sign of an exact zero) or after it (a - -b as a + b, which changes the sign of a NaN).
    template<typename T>
    T Negate(T value)
    {
        asm("" : "+w"(value));
        value = -value;
        asm("" : "+w"(value));
        return value;
    }

    // The fused PowerPC operations as the recompiled code computes them on the Switch, operands in the guest's order
    // (fmadds d,a,c,b is a * c + b, fnmsubs is -(a * c - b)): one fused double operation (the product of two singles is exact in double),
    // rounded to single. That rounds twice, so it can differ from the Xbox's single rounding by an ulp in rare ties;
    // these match the recompiled code instead. The negating ones negate the double result, as it does.
    float Madd(float a, float c, float b) { return float(std::fma(double(a), double(c), double(b))); }
    float Msub(float a, float c, float b) { return float(std::fma(double(a), double(c), -double(b))); }
    float Nmsub(float a, float c, float b) { return float(Negate(std::fma(double(a), double(c), -double(b)))); }

    bool Overlaps(uint32_t a, uint32_t aSize, uint32_t b, uint32_t bSize)
    {
        return a < b + bSize && b < a + aSize;
    }

    // fctiwz as recompiled: saturates above, INT_MIN for NaN and below.
    int32_t ToIntTruncated(double value)
    {
        if (value > double(INT_MAX))
            return INT_MAX;
        return value > double(INT32_MIN) && value < double(INT32_MAX) ? int32_t(value) : INT32_MIN;
    }

    // divw; the AArch64 answers for the cases PowerPC leaves undefined.
    int32_t DivideWord(int32_t a, int32_t b)
    {
        if (b == 0)
            return 0;
        if (a == INT32_MIN && b == -1)
            return INT32_MIN;
        return a / b;
    }

    // Mode 2: what the native code stored at `out`, against what the recompiled code stores there.
    void CheckBytes(const char* name, uint8_t* base, uint32_t out, const uint8_t* native, size_t size, uint32_t& calls, uint32_t& differences)
    {
        ++calls;
        if (std::memcmp(base + out, native, size) != 0 && ++differences <= 20)
        {
            for (size_t i = 0; i < size; i += 4)
            {
                uint32_t a, b;
                std::memcpy(&a, native + i, 4);
                std::memcpy(&b, base + out + i, 4);
                if (a != b)
                {
                    a = ByteSwap(a), b = ByteSwap(b);
                    const bool nans = (a & 0x7FFFFFFF) > 0x7F800000 && (b & 0x7FFFFFFF) > 0x7F800000;
                    const bool ulp = (a ^ b) < 0x80000000 && (a - b == 1 || b - a == 1);
                    LOG_ERROR("[math] {} differs at +{}: native {:08X}, guest {:08X}{}", name, i, a, b,
                        nans ? " (NaN payload)" : ulp ? " (1 ulp)" : "");
                    break;
                }
            }
        }
        if ((calls & 0xFFFFF) == 0)
            LOG_ERROR("[math] {} checked {} calls, {} differ", name, calls, differences);
    }

    // Runs `native` into a copy of the `size` bytes at `out`, then the guest's function, and compares.
    template<typename Native>
    void RunChecked(const char* name, PPCContext& ctx, uint8_t* base, uint32_t out, size_t size, PPCFunc* guest, Native native,
        uint32_t& calls, uint32_t& differences)
    {
        uint8_t saved[64];
        std::memcpy(saved, base + out, size);
        native();
        uint8_t result[64];
        std::memcpy(result, base + out, size);
        std::memcpy(base + out, saved, size);
        guest(ctx, base);
        CheckBytes(name, base, out, result, size, calls, differences);
    }
}

// FRawDistribution's lookup table (particles): r3 = table, f1 = time, r5 = out, r6 = values to write.
// Table: +1 operation, +2 entry size and +3 values per entry (bytes), +4 values, +8 their count, +16 time scale,
// +20 time bias. Operation 2 picks a random value between two curves, 3 picks one of the two at random; both draw from
// the game's random generator at 0x8331367C, as many times and in the same order as the guest.
namespace
{
    constexpr uint32_t kDistributionSeed = 0x8331367C;
    constexpr uint32_t kSeedMultiplier = 0x0BB38435;
    constexpr uint32_t kSeedIncrement = 0x3619636B;
    constexpr uint32_t kHalf = 0x82189798;

    struct Entries
    {
        uint32_t first, second; // second is 0 when there is no next entry
        float alpha;
    };

    // sub_822C78D8: the entries on each side of the time and how far between them it is.
    Entries FindEntries(uint8_t* base, uint32_t table, double time, float zero)
    {
        Entries result{ 0, 0, zero };
        const uint32_t values = LoadU32(base, table + 4);
        const float bias = LoadFloat(base, table + 20);
        const float scale = LoadFloat(base, table + 16);
        if (!(time > double(bias)) || scale == zero)
        {
            result.first = values + 8;
            return result;
        }

        const float offset = float(time - double(bias));
        const uint32_t stride = uint32_t(base[table + 3]) * base[table + 2];
        const int32_t last = DivideWord(int32_t(LoadU32(base, table + 8)) - 2, int32_t(stride)) - 1;
        int32_t index = ToIntTruncated(double(offset / scale));
        if (index < last)
        {
            result.second = values + ((uint32_t(index) + 1) * stride + 2) * 4;
            result.alpha = Nmsub(float(index), scale, offset) / scale;
        }
        else
        {
            index = last;
        }
        result.first = values + (uint32_t(index) * stride + 2) * 4;
        return result;
    }

    // The guest's random float in [0, 1): 23 random mantissa bits under 1.0, minus 1 (its fctiwz of a value in [1, 2)).
    float NextRandom(uint32_t& seed)
    {
        seed = seed * kSeedMultiplier + kSeedIncrement;
        const uint32_t bits = (seed & 0x7FFFFF) | 0x3F800000;
        float value;
        std::memcpy(&value, &bits, sizeof(value));
        return value - 1.0f;
    }

    float Interpolated(uint8_t* base, const Entries& e, uint32_t index)
    {
        const float a = LoadFloat(base, e.first + index * 4);
        if (e.second == 0)
            return a;
        return Madd(LoadFloat(base, e.second + index * 4) - a, e.alpha, a);
    }

    void DistributionValues(PPCContext& ctx, uint8_t* base)
    {
        ctx.fpscr.disableFlushMode();
        const uint32_t table = ctx.r3.u32;
        const uint32_t out = ctx.r5.u32;
        const int32_t count = ctx.r6.s32;
        const Entries e = FindEntries(base, table, ctx.f1.f64, LoadFloat(base, kZero));

        const uint8_t operation = base[table + 1];
        const uint32_t stride = base[table + 3];
        uint32_t seed = LoadU32(base, kDistributionSeed);
        uint32_t pick = 0;
        if (operation == 3)
        {
            if (NextRandom(seed) > LoadFloat(base, kHalf))
                pick = 1;
            StoreU32(base, kDistributionSeed, seed);
        }

        for (int32_t i = 0; i < count; ++i)
        {
            float value = Interpolated(base, e, stride * pick + uint32_t(i));
            if (operation == 2)
            {
                const float high = Interpolated(base, e, stride + uint32_t(i));
                const float random = NextRandom(seed);
                StoreU32(base, kDistributionSeed, seed);
                value = Madd(random, high - value, value);
            }
            StoreFloat(base, out + uint32_t(i) * 4, value);
        }
    }

    void CheckDistributionValues(PPCContext& ctx, uint8_t* base)
    {
        static uint32_t calls, differences;
        const uint32_t out = ctx.r5.u32;
        const uint32_t count = ctx.r6.s32 > 0 ? ctx.r6.u32 : 0;
        if (count > 16)
        {
            __imp__sub_822C73E8(ctx, base);
            return;
        }

        const uint32_t seed = LoadU32(base, kDistributionSeed);
        uint8_t saved[64];
        std::memcpy(saved, base + out, count * 4);
        DistributionValues(ctx, base);
        uint8_t native[64];
        std::memcpy(native, base + out, count * 4);
        const uint32_t nativeSeed = LoadU32(base, kDistributionSeed);
        std::memcpy(base + out, saved, count * 4);
        StoreU32(base, kDistributionSeed, seed);

        __imp__sub_822C73E8(ctx, base);
        if (LoadU32(base, kDistributionSeed) != nativeSeed && differences < 20)
            LOG_ERROR("[math] sub_822C73E8 random seed differs: native {:08X}, guest {:08X}", nativeSeed, LoadU32(base, kDistributionSeed));
        CheckBytes("sub_822C73E8", base, out, native, count * 4, calls, differences);
    }
}

PPC_FUNC(sub_822C73E8)
{
    const int mode = NativeMathMode();
    if (mode == 0)
        __imp__sub_822C73E8(ctx, base);
    else if (mode == 2)
        CheckDistributionValues(ctx, base);
    else
        DistributionValues(ctx, base);
}

// A bone's world matrix: r3 = out, r4 = skeletal mesh component, r5 = bone. SpaceBases[bone] * LocalToWorld (+112),
// through the master component's pose and the bone map when there is a master; identity for a bone out of range.
// The guest's vmsum4fp128 dot products add (x + y) + (z + w) of the products, as below.
namespace
{
    constexpr uint32_t kComponentLocalToWorld = 112;
    constexpr uint32_t kComponentSpaceBases = 708;
    constexpr uint32_t kComponentSpaceBaseCount = 712;
    constexpr uint32_t kComponentMaster = 744;
    constexpr uint32_t kComponentBoneMap = 748;
    constexpr uint32_t kComponentBoneMapCount = 752;

    void BoneMatrix(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t component = ctx.r4.u32;
        const uint32_t bone = ctx.r5.u32;
        const uint32_t master = LoadU32(base, component + kComponentMaster);
        uint32_t matrix = 0;
        bool found = false;
        if (master != 0)
        {
            if (bone < LoadU32(base, component + kComponentBoneMapCount))
            {
                const int32_t index = int32_t(LoadU32(base, LoadU32(base, component + kComponentBoneMap) + bone * 4));
                if (index != -1 && index < int32_t(LoadU32(base, master + kComponentSpaceBaseCount)))
                {
                    matrix = LoadU32(base, master + kComponentSpaceBases) + uint32_t(index) * kMatrixSize;
                    found = true;
                }
            }
        }
        else
        {
            const uint32_t count = LoadU32(base, component + kComponentSpaceBaseCount);
            if (count != 0 && bone < count)
            {
                matrix = LoadU32(base, component + kComponentSpaceBases) + bone * kMatrixSize;
                found = true;
            }
        }

        uint8_t* out = base + ctx.r3.u32;
        if (!found)
        {
            std::memmove(out, base + kGuestIdentityMatrix, kMatrixSize);
            return;
        }

        ctx.fpscr.enableFlushMode();
        float32x4_t a[4], b[4];
        for (size_t i = 0; i < 4; ++i)
        {
            a[i] = LoadRow(base + component + kComponentLocalToWorld + i * 16);
            b[i] = LoadRow(base + matrix + i * 16);
        }
        for (size_t i = 0; i < 4; ++i)
        {
            const float32x4_t xy = vaddq_f32(vmulq_laneq_f32(a[0], b[i], 0), vmulq_laneq_f32(a[1], b[i], 1));
            const float32x4_t zw = vaddq_f32(vmulq_laneq_f32(a[2], b[i], 2), vmulq_laneq_f32(a[3], b[i], 3));
            StoreRow(out + i * 16, vaddq_f32(xy, zw));
        }
    }
}

PPC_FUNC(sub_822B0A00)
{
    const int mode = NativeMathMode();
    if (mode == 0)
    {
        __imp__sub_822B0A00(ctx, base);
        return;
    }
    if (mode == 2)
    {
        static uint32_t calls, differences;
        RunChecked("sub_822B0A00", ctx, base, ctx.r3.u32, kMatrixSize, __imp__sub_822B0A00, [&]() { BoneMatrix(ctx, base); },
            calls, differences);
        return;
    }
    BoneMatrix(ctx, base);
}

// The extent check against a box: r3 = box center, r4 = extent, r5 = line end, r6 = line direction (its zero
// components skip the slab test), r7 = reciprocal direction. Returns 1 when the slabs overlap within [0, 1].
namespace
{
    int32_t SlabsOverlap(uint8_t* base, uint32_t center, uint32_t extent, uint32_t end, uint32_t direction, uint32_t reciprocal)
    {
        const float zero = LoadFloat(base, kZero);
        float low = zero;
        float high = LoadFloat(base, kOne);
        for (uint32_t axis = 0; axis < 3; ++axis)
        {
            const float delta = LoadFloat(base, end + axis * 4) - LoadFloat(base, center + axis * 4);
            const float size = LoadFloat(base, extent + axis * 4);
            if (LoadFloat(base, direction + axis * 4) == zero)
            {
                if (std::fabs(delta) > size)
                    return 0;
                continue;
            }

            const float inverse = LoadFloat(base, reciprocal + axis * 4);
            const float scaled = inverse * delta;
            const float near = Nmsub(std::fabs(inverse), size, Negate(scaled));
            const float far = Msub(std::fabs(inverse), size, scaled);
            if (near > low)
                low = near;
            if (far < high)
                high = far;
            if (high < low)
                return 0;
        }
        return 1;
    }
}

PPC_FUNC(sub_826C8FC0)
{
    const int mode = NativeMathMode();
    if (mode == 0)
    {
        __imp__sub_826C8FC0(ctx, base);
        return;
    }

    ctx.fpscr.disableFlushMode();
    const int32_t native = SlabsOverlap(base, ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32);
    if (mode == 2)
    {
        static uint32_t calls, differences;
        __imp__sub_826C8FC0(ctx, base);
        ++calls;
        if (ctx.r3.s32 != native && ++differences <= 20)
            LOG_ERROR("[math] sub_826C8FC0 differs: native {}, guest {}", native, ctx.r3.s32);
        return;
    }
    ctx.r3.s64 = native;
}

// A separating-axis test of a convex shape: r3 = its vertices (TArray<FVector>), r4 = the axis, f1 = the other
// shape's position on it, f2 = a margin. Returns 0 when f1 is outside the vertices' range widened by the margin.
// Otherwise, when a side of the range is nearer than the current time (r7, compared after scaling it by the axis
// length), the time becomes that distance and the normal (r8) the axis, negated for the low side.
// The guest unrolls the projection by four and sums the products of the first two vertices of each group in other
// orders than the rest; so does this.
namespace
{
    constexpr uint32_t kLargest = 0x82000D5C;  // 3.4e38
    constexpr uint32_t kSmallest = 0x82000D60; // -3.4e38
    constexpr uint32_t kOneDouble = 0x82000F28;

    int32_t AxisRange(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t vertices = LoadU32(base, ctx.r3.u32);
        const int32_t count = int32_t(LoadU32(base, ctx.r3.u32 + 4));
        const uint32_t axis = ctx.r4.u32;
        const float nx = LoadFloat(base, axis), ny = LoadFloat(base, axis + 4), nz = LoadFloat(base, axis + 8);
        float low = LoadFloat(base, kLargest);
        float high = LoadFloat(base, kSmallest);
        const auto add = [&](float d)
            {
                const float belowLow = d - low;
                const float aboveHigh = d - high;
                low = belowLow >= 0.0f ? low : d;
                high = aboveHigh >= 0.0f ? d : high;
            };
        const auto x = [&](int32_t i) { return LoadFloat(base, vertices + uint32_t(i) * 12); };
        const auto y = [&](int32_t i) { return LoadFloat(base, vertices + uint32_t(i) * 12 + 4); };
        const auto z = [&](int32_t i) { return LoadFloat(base, vertices + uint32_t(i) * 12 + 8); };

        int32_t i = 0;
        if (count >= 4)
        {
            const uint32_t groups = ((uint32_t(count) - 4) >> 2) + 1;
            for (uint32_t g = 0; g < groups; ++g, i += 4)
            {
                add(Madd(y(i), ny, Madd(x(i), nx, z(i) * nz)));
                add(Madd(y(i + 1), ny, Madd(z(i + 1), nz, x(i + 1) * nx)));
                add(Madd(x(i + 2), nx, Madd(y(i + 2), ny, z(i + 2) * nz)));
                add(Madd(x(i + 3), nx, Madd(y(i + 3), ny, z(i + 3) * nz)));
            }
        }
        for (; i < count; ++i)
            add(Madd(x(i), nx, Madd(y(i), ny, z(i) * nz)));

        const double position = ctx.f1.f64;
        const float lowEdge = float(double(low) - ctx.f2.f64);
        const float highEdge = float(double(high) + ctx.f2.f64);
        if (position < double(lowEdge) || position > double(highEdge))
            return 0;

        const float toLow = float(position - double(lowEdge));
        const float toHigh = float(double(highEdge) - position);
        const float lengthSquared = Madd(nx, nx, Madd(ny, ny, nz * nz));
        const float inverse = float(LoadDouble(base, kOneDouble) / std::sqrt(double(lengthSquared)));
        const uint32_t time = ctx.r7.u32;
        const uint32_t normal = ctx.r8.u32;
        const float limit = LoadFloat(base, time) / inverse;
        if (toLow < limit)
        {
            StoreFloat(base, time, toLow * inverse);
            StoreFloat(base, normal, Negate(nx) * inverse);
            StoreFloat(base, normal + 4, Negate(ny) * inverse);
            StoreFloat(base, normal + 8, Negate(nz) * inverse);
        }
        if (toHigh < limit)
        {
            StoreFloat(base, normal, nx * inverse);
            StoreFloat(base, normal + 4, ny * inverse);
            StoreFloat(base, normal + 8, nz * inverse);
            StoreFloat(base, time, toHigh * inverse);
        }
        return 1;
    }
}

PPC_FUNC(sub_82744880)
{
    const int mode = NativeMathMode();
    const uint32_t axis = ctx.r4.u32, time = ctx.r7.u32, normal = ctx.r8.u32;
    if (mode == 0 || Overlaps(normal, 12, axis, 12) || Overlaps(time, 4, axis, 12) || Overlaps(time, 4, normal, 12))
    {
        __imp__sub_82744880(ctx, base);
        return;
    }

    ctx.fpscr.disableFlushMode();
    if (mode == 2)
    {
        static uint32_t calls, differences;
        uint8_t saved[16], native[16];
        std::memcpy(saved, base + time, 4);
        std::memcpy(saved + 4, base + normal, 12);
        const int32_t result = AxisRange(ctx, base);
        std::memcpy(native, base + time, 4);
        std::memcpy(native + 4, base + normal, 12);
        std::memcpy(base + time, saved, 4);
        std::memcpy(base + normal, saved + 4, 12);
        __imp__sub_82744880(ctx, base);
        ++calls;
        if ((ctx.r3.s32 != result || std::memcmp(base + time, native, 4) != 0 || std::memcmp(base + normal, native + 4, 12) != 0) &&
            ++differences <= 20)
            LOG_ERROR("[math] sub_82744880 differs: native {} time {:08X}, guest {} time {:08X}", result, LoadU32(base, time), ctx.r3.s32,
                ByteSwap(*reinterpret_cast<uint32_t*>(native)));
        return;
    }
    ctx.r3.s64 = AxisRange(ctx, base);
}

// A separating-axis test of a triangle (r3, r4, r5) moving along a line, on the axis r6: f1 and f2 are the line's
// start and end on the axis, f3 the margin. Parallel to the axis it only checks the start against the range; otherwise
// it narrows the entry time (r10, normal into the pointer at the caller's r1 + 92) and the exit time (the pointer at
// r1 + 84). Loads and stores go through memory in the guest's order.
namespace
{
    constexpr uint32_t kParallel = 0x82000E80; // 1e-8, a double

    int32_t TriangleAxis(PPCContext& ctx, uint8_t* base)
    {
        const uint32_t a = ctx.r3.u32, b = ctx.r4.u32, c = ctx.r5.u32, axis = ctx.r6.u32;
        const float nx = LoadFloat(base, axis), ny = LoadFloat(base, axis + 4), nz = LoadFloat(base, axis + 8);
        const float span = float(ctx.f2.f64 - ctx.f1.f64);
        const float db = Madd(nx, LoadFloat(base, b), Madd(LoadFloat(base, b + 4), ny, LoadFloat(base, b + 8) * nz));
        const float dc = Madd(nx, LoadFloat(base, c), Madd(LoadFloat(base, c + 4), ny, LoadFloat(base, c + 8) * nz));
        const float da = Madd(nx, LoadFloat(base, a), Madd(LoadFloat(base, a + 4), ny, LoadFloat(base, a + 8) * nz));
        const float bc = db - dc;
        float low = bc >= 0.0f ? dc : db;
        float high = bc >= 0.0f ? db : dc;
        const float aLow = da - low;
        const float aHigh = da - high;
        low = aLow >= 0.0f ? low : da;
        high = aHigh >= 0.0f ? da : high;
        const float lowEdge = float(double(low) - ctx.f3.f64);
        const float highEdge = float(double(high) + ctx.f3.f64);
        const double start = ctx.f1.f64;

        if (double(std::fabs(span)) < LoadDouble(base, kParallel))
            return !(start < double(lowEdge)) && !(start > double(highEdge));

        const float toLow = float(double(lowEdge) - start);
        const float toHigh = float(double(highEdge) - start);
        const float inverse = LoadFloat(base, kOne) / span;
        const float zero = LoadFloat(base, kZero);
        float enter, exit;
        uint32_t normal[3];
        if (span > zero)
        {
            enter = toLow * inverse;
            exit = toHigh * inverse;
            const float negated[3] = { Negate(nx), Negate(LoadFloat(base, axis + 4)), Negate(LoadFloat(base, axis + 8)) };
            std::memcpy(normal, negated, sizeof(normal));
        }
        else
        {
            enter = toHigh * inverse;
            exit = toLow * inverse;
            for (uint32_t i = 0; i < 3; ++i)
                std::memcpy(&normal[i], base + axis + i * 4, 4), normal[i] = ByteSwap(normal[i]);
        }

        const uint32_t enterTime = ctx.r10.u32;
        if (enter > LoadFloat(base, enterTime))
        {
            const uint32_t enterNormal = LoadU32(base, ctx.r1.u32 + 92);
            StoreFloat(base, enterTime, enter);
            for (uint32_t i = 0; i < 3; ++i)
                StoreU32(base, enterNormal + i * 4, normal[i]);
        }
        const uint32_t exitTime = LoadU32(base, ctx.r1.u32 + 84);
        const float entered = LoadFloat(base, enterTime);
        const float current = LoadFloat(base, exitTime);
        const float exited = current - exit >= 0.0f ? exit : current;
        StoreFloat(base, exitTime, exited);
        return !(exited < entered) && !(exited < zero);
    }
}

PPC_FUNC(sub_82796B88)
{
    const int mode = NativeMathMode();
    if (mode == 0)
    {
        __imp__sub_82796B88(ctx, base);
        return;
    }

    ctx.fpscr.disableFlushMode();
    if (mode == 2)
    {
        // The three outputs, saved and put back in reverse order so overlapping ones end up as they were.
        static uint32_t calls, differences;
        const uint32_t outputs[3] = { ctx.r10.u32, LoadU32(base, ctx.r1.u32 + 92), LoadU32(base, ctx.r1.u32 + 84) };
        const uint32_t sizes[3] = { 4, 12, 4 };
        uint8_t saved[3][12], native[3][12];
        for (size_t i = 0; i < 3; ++i)
            std::memcpy(saved[i], base + outputs[i], sizes[i]);
        const int32_t result = TriangleAxis(ctx, base);
        for (size_t i = 0; i < 3; ++i)
            std::memcpy(native[i], base + outputs[i], sizes[i]);
        for (size_t i = 3; i-- > 0;)
            std::memcpy(base + outputs[i], saved[i], sizes[i]);
        __imp__sub_82796B88(ctx, base);
        bool same = ctx.r3.s32 == result;
        for (size_t i = 0; i < 3; ++i)
            same = same && std::memcmp(base + outputs[i], native[i], sizes[i]) == 0;
        ++calls;
        if (!same && ++differences <= 20)
            LOG_ERROR("[math] sub_82796B88 differs: native {}, guest {}, enter time {:08X}/{:08X}", result, ctx.r3.s32,
                ByteSwap(*reinterpret_cast<uint32_t*>(native[0])), LoadU32(base, outputs[0]));
        return;
    }
    ctx.r3.s64 = TriangleAxis(ctx, base);
}

// A line against a triangle (r5, r6, r7) for the line check at r4 (Start +32, End +44, End - Start +56, the result
// at +16 with its time at +32): the triangle's plane, the line's distances to it, the hit point and the three edge
// tests. On a hit nearer than the result's time it stores the normal (+80) and the time, then makes the guest's
// virtual call (slot 488 of the object at *r4, argument r8) from a frame like the guest's and stores its answer at
// the result's +40. Returns 1 then, else 0.
namespace
{
    constexpr uint32_t kNormalEpsilon = 0x82003904;
    constexpr uint32_t kPlaneBehind = 0x82000D70; // -0.001
    constexpr uint32_t kPlaneMargin = 0x83245678; // 0.001, game data
    constexpr uint32_t kHitActorSlot = 488;
    constexpr uint32_t kTriangleFrame = 160;

    struct TriangleHit
    {
        bool hit;
        float normal[3];
        float time;
    };

    TriangleHit LineTriangle(uint8_t* base, uint32_t check, const uint32_t vertices[3])
    {
        TriangleHit result{};
        const auto at = [&](uint32_t v, uint32_t axis) { return LoadFloat(base, vertices[v] + axis * 4); };
        const float e1x = at(0, 0) - at(2, 0), e1y = at(0, 1) - at(2, 1), e1z = at(0, 2) - at(2, 2);
        const float e2x = at(1, 0) - at(2, 0), e2y = at(1, 1) - at(2, 1), e2z = at(1, 2) - at(2, 2);
        const float cy = Msub(e2z, e1x, e1z * e2x);
        const float cz = Msub(e1y, e2x, e2y * e1x);
        const float cx = Msub(e2y, e1z, e2z * e1y);
        const float lengthSquared = Madd(cx, cx, Madd(cz, cz, cy * cy));

        float nx, ny, nz;
        if (lengthSquared == LoadFloat(base, kOne))
        {
            nx = cx, ny = cy, nz = cz;
        }
        else if (lengthSquared < LoadFloat(base, kNormalEpsilon))
        {
            nx = ny = nz = LoadFloat(base, kZero);
        }
        else
        {
            const float inverse = float(LoadDouble(base, kOneDouble) / std::sqrt(double(lengthSquared)));
            nx = inverse * cx, ny = cy * inverse, nz = cz * inverse;
        }

        const auto dot = [&](float x, float y, float z) { return Madd(x, nx, Madd(z, nz, y * ny)); };
        const float plane = dot(at(0, 0), at(0, 1), at(0, 2));
        const float end = dot(LoadFloat(base, check + 44), LoadFloat(base, check + 48), LoadFloat(base, check + 52)) - plane;
        const float start = dot(LoadFloat(base, check + 32), LoadFloat(base, check + 36), LoadFloat(base, check + 40)) - plane;
        const float behind = LoadFloat(base, kPlaneBehind);
        if (start > behind && end > behind)
            return result;
        const float margin = LoadFloat(base, kPlaneMargin);
        if (start < margin && end < margin)
            return result;

        const float time = Negate(start / (end - start));
        if (!(time < LoadFloat(base, LoadU32(base, check + 16) + 32)))
            return result;

        float hit[3];
        for (uint32_t axis = 0; axis < 3; ++axis)
            hit[axis] = LoadFloat(base, check + 32 + axis * 4) + LoadFloat(base, check + 56 + axis * 4) * time;
        for (uint32_t k = 0; k < 3; ++k)
        {
            const uint32_t b = (k + 1) % 3;
            const float ax = at(k, 0), ay = at(k, 1), az = at(k, 2);
            const float ex = at(b, 0) - ax, ez = at(b, 2) - az, ey = at(b, 1) - ay;
            const float sideZ = Msub(ey, nx, ny * ex);
            const float sideY = Msub(nz, ex, ez * nx);
            const float sideX = Msub(ez, ny, ey * nz);
            const float hitSide = Madd(sideX, hit[0], Madd(sideZ, hit[2], sideY * hit[1]));
            const float vertexSide = Madd(ax, sideX, Madd(az, sideZ, ay * sideY));
            if (!(hitSide - vertexSide < margin))
                return result;
        }

        result.hit = true;
        result.normal[0] = nx, result.normal[1] = ny, result.normal[2] = nz;
        result.time = time;
        return result;
    }

    void StoreTriangleHit(PPCContext& ctx, uint8_t* base, uint32_t check, const TriangleHit& hit)
    {
        for (uint32_t axis = 0; axis < 3; ++axis)
            StoreFloat(base, check + 80 + axis * 4, hit.normal[axis]);
        StoreFloat(base, LoadU32(base, check + 16) + 32, hit.time);

        const uint32_t stack = ctx.r1.u32;
        StoreU32(base, stack - kTriangleFrame, stack);
        const uint32_t object = LoadU32(base, LoadU32(base, check));
        const uint32_t function = LoadU32(base, LoadU32(base, object) + kHitActorSlot);
        ctx.r1.u64 = stack - kTriangleFrame;
        ctx.r3.u64 = object;
        ctx.r4.u64 = ctx.r8.u64;
        ctx.r11.u64 = function;
        ctx.r12.u64 = ctx.lr;
        ctx.lr = 0x822B1EFC;
        PPC_CALL_INDIRECT_FUNC(function);
        ctx.r1.u64 = ctx.r1.u32 + kTriangleFrame;
        StoreU32(base, LoadU32(base, check + 16) + 40, ctx.r3.u32);
        ctx.r3.u64 = 1;
    }
}

PPC_FUNC(sub_822B1C00)
{
    const int mode = NativeMathMode();
    if (mode == 0)
    {
        __imp__sub_822B1C00(ctx, base);
        return;
    }

    ctx.fpscr.disableFlushMode();
    const uint32_t check = ctx.r4.u32;
    const uint32_t vertices[3] = { ctx.r5.u32, ctx.r6.u32, ctx.r7.u32 };
    const TriangleHit hit = LineTriangle(base, check, vertices);
    if (mode == 2)
    {
        static uint32_t calls, differences;
        __imp__sub_822B1C00(ctx, base);
        bool same = ctx.r3.s32 == int32_t(hit.hit);
        if (same && hit.hit)
        {
            same = LoadFloat(base, LoadU32(base, check + 16) + 32) == hit.time;
            for (uint32_t axis = 0; axis < 3; ++axis)
                same = same && LoadFloat(base, check + 80 + axis * 4) == hit.normal[axis];
        }
        ++calls;
        if (!same && ++differences <= 20)
            LOG_ERROR("[math] sub_822B1C00 differs: native {} time {}, guest {}", int32_t(hit.hit), hit.time, ctx.r3.s32);
        return;
    }

    if (!hit.hit)
    {
        ctx.r3.u64 = 0;
        return;
    }
    StoreTriangleHit(ctx, base, check, hit);
}

#endif
