// The animation tree's child weights without a virtual call per child. sub_822B3600 (UAnimNodeBlendBase, passing the
// node's total weight down) walks the node's children (array at +180, 64 bytes each, count at +184) and calls the node's
// vtable slot +348 for each child with an animation node. That slot is almost always sub_822B3688, 14 instructions:
// child.Weight (+12) * weight into +16, and the same product added to the child node's NodeTotalWeight (+148). In a
// busy street the call overhead made sub_822B3688 1.6% of Guest Main. When the slot holds sub_822B3688 its arithmetic
// runs here, in the same order; any other override is called as the guest would.
// LO_NATIVE_ANIM_NODES=0 keeps the recompiled code; 2 runs the game's code after the native one and logs differences.
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

extern "C" PPC_FUNC(__imp__sub_822B3600);
extern "C" PPC_FUNC(__imp__sub_822B3688);

namespace
{
    constexpr uint32_t kChildWeightFunction = 0x822B3688;
    constexpr uint32_t kChildWeightSlot = 348;
    constexpr uint32_t kSkeletalComponent = 60;
    constexpr uint32_t kChildren = 180;
    constexpr uint32_t kChildCount = 184;
    constexpr uint32_t kChildSize = 64;
    constexpr uint32_t kChildAnim = 8;
    constexpr uint32_t kChildWeight = 12;
    constexpr uint32_t kChildTotalWeight = 16;
    constexpr uint32_t kNodeTotalWeight = 148;
    constexpr uint32_t kFrameSize = 128; // sub_822B3600's stack frame, for the calls it makes

    // 0: recompiled code. 1: native. 2: native, then the guest's sub_822B3688, comparing what each stored.
    int NativeAnimNodesMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_ANIM_NODES");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    uint32_t LoadU32(uint8_t* base, uint32_t address)
    {
        return ByteSwap(*reinterpret_cast<uint32_t*>(base + address));
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
        *reinterpret_cast<uint32_t*>(base + address) = ByteSwap(bits);
    }

    uint32_t FloatBits(float value)
    {
        uint32_t bits;
        std::memcpy(&bits, &value, sizeof(bits));
        return bits;
    }

    struct ChildWeights
    {
        uint32_t anim;
        float total, nodeTotal;
    };

    // sub_822B3688 for the child at `child` (node r3, index r4, weight f1, a double): fmuls, then fmadds, as the
    // recompiled code computes them (in double, rounded to single). The guest reads the child array and the child again
    // after its first store; nothing it stores is one of those fields.
    ChildWeights ComputeChildWeights(uint8_t* base, uint32_t child, double weight, bool store)
    {
        ChildWeights result;
        const float childWeight = LoadFloat(base, child + kChildWeight);
        result.total = float(double(childWeight) * weight);
        if (store)
            StoreFloat(base, child + kChildTotalWeight, result.total);

        result.anim = LoadU32(base, child + kChildAnim);
        result.nodeTotal = float(std::fma(double(childWeight), weight, double(LoadFloat(base, result.anim + kNodeTotalWeight))));
        if (store)
            StoreFloat(base, result.anim + kNodeTotalWeight, result.nodeTotal);
        return result;
    }

    uint32_t ChildAddress(uint8_t* base, uint32_t node, uint32_t index)
    {
        return LoadU32(base, node + kChildren) + index * kChildSize;
    }

    void CheckChildWeights(PPCContext& ctx, uint8_t* base, uint32_t node, uint32_t index)
    {
        static uint32_t calls, differences;

        const uint32_t child = ChildAddress(base, node, index);
        const ChildWeights native = ComputeChildWeights(base, child, ctx.f1.f64, false);
        ctx.r3.u64 = node;
        ctx.r4.u64 = index;
        __imp__sub_822B3688(ctx, base);

        const float total = LoadFloat(base, child + kChildTotalWeight);
        const float nodeTotal = LoadFloat(base, native.anim + kNodeTotalWeight);
        ++calls;
        if ((FloatBits(total) != FloatBits(native.total) || FloatBits(nodeTotal) != FloatBits(native.nodeTotal)) && ++differences <= 40)
            LOG_ERROR("[anim nodes] sub_822B3688 differs: node {:08X} child {} weight {} total {}/{} node total {}/{}", node, index,
                ctx.f1.f64, native.total, total, native.nodeTotal, nodeTotal);
        if ((calls & 0xFFFFF) == 0)
            LOG_ERROR("[anim nodes] sub_822B3688 checked {} calls, {} differ", calls, differences);
    }
}

PPC_FUNC(sub_822B3688)
{
    const int mode = NativeAnimNodesMode();
    if (mode == 0)
    {
        __imp__sub_822B3688(ctx, base);
        return;
    }

    ctx.fpscr.disableFlushMode();
    if (mode == 2)
        CheckChildWeights(ctx, base, ctx.r3.u32, ctx.r4.u32);
    else
        ComputeChildWeights(base, ChildAddress(base, ctx.r3.u32, ctx.r4.u32), ctx.f1.f64, true);
}

// r3 = node, f2 = its weight. Leaves r3, r4 and f1 as the guest does: those of the last call, which are the node, the
// child's index and f2 when it was sub_822B3688.
PPC_FUNC(sub_822B3600)
{
    const int mode = NativeAnimNodesMode();
    if (mode == 0)
    {
        __imp__sub_822B3600(ctx, base);
        return;
    }

    ctx.fpscr.disableFlushMode();
    const uint32_t node = ctx.r3.u32;
    if (LoadU32(base, node + kSkeletalComponent) == 0)
        return;

    const uint64_t returnAddress = ctx.lr;
    const double weight = ctx.f2.f64;
    int32_t lastNative = -1;
    for (int32_t i = 0; i < int32_t(LoadU32(base, node + kChildCount)); ++i)
    {
        const uint32_t child = ChildAddress(base, node, uint32_t(i));
        if (LoadU32(base, child + kChildAnim) == 0)
            continue;

        const uint32_t function = LoadU32(base, LoadU32(base, node) + kChildWeightSlot);
        if (function == kChildWeightFunction)
        {
            if (mode == 2)
            {
                ctx.f1.f64 = weight;
                CheckChildWeights(ctx, base, node, uint32_t(i));
            }
            else
            {
                ComputeChildWeights(base, child, weight, true);
            }
            lastNative = i;
            continue;
        }

        // Another override: called from a frame like the guest's, back chain included.
        lastNative = -1;
        const uint32_t stack = ctx.r1.u32;
        *reinterpret_cast<uint32_t*>(base + stack - kFrameSize) = ByteSwap(stack);
        ctx.r1.u64 = stack - kFrameSize;
        ctx.r3.u64 = node;
        ctx.r4.u64 = uint32_t(i);
        ctx.f1.f64 = weight;
        ctx.r11.u64 = function;
        ctx.r12.u64 = returnAddress;
        ctx.lr = 0x822B3668;
        PPC_CALL_INDIRECT_FUNC(function);
        ctx.r1.u64 = ctx.r1.u32 + kFrameSize;
    }

    if (lastNative >= 0)
    {
        ctx.r3.u64 = node;
        ctx.r4.u64 = uint32_t(lastNative);
        ctx.f1.f64 = weight;
    }
}
