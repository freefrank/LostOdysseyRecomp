// The collision octree's line checks without the recompiled code's per-node overhead. sub_822AD008 (the octree's line
// check) walks the tree with two recursive functions:
// - sub_822B1200, a zero-extent line: the node's primitives, then its children in the order the ray meets them (the
//   parametric traversal: t at the node's six planes in f1-f6, the child's centre computed by sub_822ADAA8, the next
//   child by sub_82767670, mirrored axes from the check's +172/+176);
// - sub_822AD6C8, a line with an extent (box): the node's primitives, then each child the box overlaps
//   (sub_822ADE58) whose bounds, grown by the extent, the line crosses (sub_826C8FC0).
// For each primitive both use the collision tag (prim +380 against the global at 0x832350B4) so a primitive in several
// nodes is tested once, the check's flags, the owner chains, and the primitive's own line check (vtable +364) into a
// result on the stack, which is then copied to a block from the check's memory stack and pushed on the result list.
// In a busy street these two were about 3% of Guest Main by themselves.
// Here the traversal, the flag tests (sub_822ADB48, sub_822D7BB8), the child bounds and the next-child choice run
// natively, in the guest's order, with its loads, stores and single-precision arithmetic (sums and products rounded
// to single, fmadds as one fused double operation rounded to single). The calls to other game code are the guest's,
// with its arguments and stack: the virtual calls (+324, +364, +380), the box test sub_826C8FC0, the memory stack
// sub_824E8530 and the copy sub_82B7A0B0. Each level has its guest frame (back chain, result and box test inputs at
// the guest's offsets), so callees and stack walks see the same stack; the line check's child bounds, which only the
// children read, stay on the host.
// Registers: these functions return nothing. r1, r4, f1-f6 and lr are left as the guest leaves them (their caller,
// sub_822AD008, passes r4 and f1 on); r3 and other scratch registers can hold different leftovers, which it does not
// read.
// LO_NATIVE_COLLISION=0 keeps the recompiled code; 2 runs the native code, undoes its tags and results, runs the
// game's code and compares the result lists (the primitives' own line checks then run twice).
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <cstdlib>
#include <cstring>
#include <vector>

#if defined(__clang__)
#pragma clang fp contract(off)
#else
#pragma GCC optimize("fp-contract=off")
#endif

extern "C" PPC_FUNC(__imp__sub_822B1200);
extern "C" PPC_FUNC(__imp__sub_822AD6C8);

namespace
{
    // Constants the guest loads.
    constexpr uint32_t kZero = 0x82000E50;           // 0.0f
    constexpr uint32_t kOne = 0x8218958C;            // 1.0f
    constexpr uint32_t kHalf = 0x82189798;           // 0.5f
    constexpr uint32_t kFloatMax = 0x82000E0C;       // FLT_MAX
    constexpr uint32_t kFloatLowest = 0x82000D64;    // -FLT_MAX
    constexpr uint32_t kCollisionTag = 0x832350B4;
    constexpr uint32_t kTraceFlags = 0x832C9938;     // sub_822ADB48's global

    // Octree node: primitives (TArray at +0, count +4), children (8 nodes of 16 bytes) at +12.
    constexpr uint32_t kNodePrimitives = 0, kNodePrimitiveCount = 4, kNodeChildren = 12;

    // Primitive (component).
    constexpr uint32_t kPrimFlags80 = 80, kPrimOwner = 76, kPrimTag = 380, kPrimOrigin = 388, kPrimExtent = 400;
    constexpr uint32_t kPrimFlags440 = 440, kPrimFlags448 = 448, kPrimFlags600 = 600;
    constexpr uint32_t kPrimLineCheckSlot = 364;

    // Owner (actor).
    constexpr uint32_t kOwnerFlags112 = 112, kOwnerFlags116 = 116, kOwnerChain = 92, kOwnerShouldTraceSlot = 380;

    // The check.
    constexpr uint32_t kCheckResults = 12, kCheckMemStack = 16, kCheckEnd = 20, kCheckStart = 32, kCheckDirection = 56;
    constexpr uint32_t kCheckOneOverDirection = 68, kCheckExtent = 80, kCheckFlags = 92, kCheckSource = 96;
    constexpr uint32_t kCheckFilter = 100, kCheckBox = 108, kCheckBest = 156, kCheckMirrorOrigin = 160;
    constexpr uint32_t kCheckMirrorAxes = 172, kCheckMirrorIndex = 176;
    constexpr uint32_t kFilterAcceptSlot = 324;

    // Check flags.
    constexpr uint32_t kFlagZeroExtentFilter = 0x100, kFlagStopAtAny = 0x200, kFlagFindNearest = 0x400;
    constexpr uint32_t kFlagSkipOwnerTest = 0x80000;

    // FCheckResult: 72 bytes; +32 is its time.
    constexpr uint32_t kResultSize = 72, kResultTime = 32;

    // The guest frames.
    constexpr uint32_t kLineFrame = 352, kLineResult = 112, kLineBounds = 96, kLineArgument = 84;
    constexpr uint32_t kBoxFrame = 400, kBoxPrimExtent = 80, kBoxCenter = 96, kBoxHalf = 108, kBoxGrown = 112;
    constexpr uint32_t kBoxResult = 128, kBoxOverlapList = 224;

    // Return addresses of the guest's calls.
    constexpr uint32_t kLineFilter = 0x822B1388, kLineBoxTest = 0x822B13AC, kLineShouldTrace = 0x822B1420;
    constexpr uint32_t kLineCheck = 0x822B1498, kLineAlloc = 0x822B14B0, kLineCopy = 0x822B14C8;
    constexpr uint32_t kLineReturnAfterChildren = 0x822B1900, kLineReturn = 0x822B194C;
    constexpr uint32_t kBoxPrimTest = 0x822AD784, kBoxShouldTrace = 0x822AD858, kBoxCheck = 0x822AD8D0;
    constexpr uint32_t kBoxAlloc = 0x822AD8E8, kBoxCopy = 0x822AD900, kBoxChildTest = 0x822ADA4C;
    constexpr uint32_t kBoxEntry = 0x822AD6D0, kBoxTraceFlags = 0x822AD7FC, kBoxOverlaps = 0x822AD95C;
    constexpr uint32_t kBoxChild = 0x822ADA6C;

    // 0: recompiled code. 1: native. 2: both, compared.
    int NativeCollisionMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_COLLISION");
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

    double LoadFloat(uint8_t* base, uint32_t address)
    {
        const uint32_t value = LoadU32(base, address);
        float result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    void StoreFloat(uint8_t* base, uint32_t address, double value)
    {
        const float single = float(value);
        uint32_t bits;
        std::memcpy(&bits, &single, sizeof(bits));
        StoreU32(base, address, bits);
    }

    // fadds, fmuls, fmadds as the recompiled code computes them.
    double AddSingle(double a, double b) { return double(float(a + b)); }
    double MulSingle(double a, double b) { return double(float(a * b)); }
    double MaddSingle(double a, double c, double b) { return double(float(std::fma(a, c, b))); }

    // Mode 2 records the tags the native code set, to undo them before the guest's run.
    struct TagUndo
    {
        uint32_t address, value;
    };
    thread_local bool g_inGuestRun;

    void SetTag(uint8_t* base, uint32_t primitive, uint32_t tag, std::vector<TagUndo>* undo)
    {
        if (undo)
            undo->push_back({ primitive + kPrimTag, LoadU32(base, primitive + kPrimTag) });
        StoreU32(base, primitive + kPrimTag, tag);
    }

    void CallVirtual(PPCContext& ctx, uint8_t* base, uint32_t object, uint32_t slot, uint32_t returnAddress, uint32_t r12)
    {
        const uint32_t function = LoadU32(base, LoadU32(base, object) + slot);
        ctx.r11.u64 = function;
        ctx.r12.u64 = r12;
        ctx.lr = returnAddress;
        PPC_CALL_INDIRECT_FUNC(function);
    }

    // sub_822ADB48: whether the primitive passes the global trace filter (flags at prim +80).
    bool PassesTraceFlags(uint8_t* base, uint32_t primitive)
    {
        const uint32_t flags = LoadU32(base, kTraceFlags);
        if (flags == 0)
            return true;
        const uint32_t primitiveFlags = LoadU32(base, primitive + kPrimFlags80);
        if ((primitiveFlags & 0x8000000) == 0)
            return true;
        if ((flags & 0x1) && (primitiveFlags & 0x4000000))
            return false;
        if ((flags & 0x2) && (primitiveFlags & 0x2000000))
            return false;
        if ((flags & 0x4) && (primitiveFlags & 0x1000000))
            return false;
        if ((flags & 0x8) && (primitiveFlags & 0x800000))
            return false;
        if ((flags & 0x10) && (primitiveFlags & 0x400000))
            return false;
        if ((flags & 0x20) && (primitiveFlags & 0x20000))
            return false;
        return true;
    }

    // sub_822D7BB8: the zero-extent filter's test of the primitive and its owner.
    bool PassesZeroExtentFilter(uint8_t* base, uint32_t primitive)
    {
        if ((LoadU32(base, primitive + kPrimFlags448) & 0x80000000) == 0)
            return false;
        const uint32_t owner = LoadU32(base, primitive + kPrimOwner);
        if (owner == 0)
            return true;
        const uint32_t ownerFlags = LoadU32(base, owner + kOwnerFlags112);
        if (ownerFlags & 0x80000000)
            return true;
        if ((ownerFlags & 0x4000000) == 0)
            return false;
        return (LoadU32(base, owner + kOwnerFlags116) & 0x800000) == 0;
    }

    // Whether the owner is the check's source, or one is in the other's +92 chain.
    bool RelatedToSource(uint8_t* base, uint32_t owner, uint32_t source)
    {
        if (owner == source)
            return true;
        for (uint32_t actor = source; actor != 0; actor = LoadU32(base, actor + kOwnerChain))
            if (actor == owner)
                return true;
        for (uint32_t actor = owner; actor != 0; actor = LoadU32(base, actor + kOwnerChain))
            if (actor == source)
                return true;
        return false;
    }

    // The empty FCheckResult the guest builds at `result`: Time and the floats after it 0, Item = -1, the rest 0.
    void ClearResult(uint8_t* base, uint32_t result, uint32_t zeroBits)
    {
        StoreU32(base, result + 0, 0);
        StoreU32(base, result + 4, 0);
        for (uint32_t offset = 8; offset < 36; offset += 4)
            StoreU32(base, result + offset, zeroBits);
        StoreU32(base, result + 36, 0xFFFFFFFF);
        for (uint32_t offset = 40; offset < kResultSize; offset += 4)
            StoreU32(base, result + offset, 0);
    }

    // The primitive's line check and, on a hit, the result pushed on the check's list (r3 of the alloc and the copy as
    // the guest passes them). Returns the result block (0 when the allocation failed, as the guest then stores through 0),
    // or -1 when the primitive was not hit.
    uint32_t CheckPrimitive(PPCContext& ctx, uint8_t* base, uint32_t primitive, uint32_t check, uint32_t frame,
        uint32_t resultOffset, uint32_t r12, uint32_t zeroBits, const uint32_t (&returns)[3])
    {
        const uint32_t result = frame + resultOffset;
        ClearResult(base, result, zeroBits);
        ctx.r3.u64 = primitive;
        ctx.r4.u64 = result;
        ctx.r5.u64 = check + kCheckEnd;
        ctx.r6.u64 = check + kCheckStart;
        ctx.r7.u64 = check + kCheckExtent;
        ctx.r8.u64 = LoadU32(base, check + kCheckFlags);
        CallVirtual(ctx, base, primitive, kPrimLineCheckSlot, returns[0], r12);
        if (ctx.r3.u32 != 0)
            return 0xFFFFFFFF;

        ctx.r3.u64 = LoadU32(base, check + kCheckMemStack);
        ctx.r4.s64 = kResultSize;
        ctx.r5.s64 = 8;
        ctx.r12.u64 = r12;
        ctx.lr = returns[1];
        sub_824E8530(ctx, base);
        const uint32_t block = ctx.r3.u32;
        if (block != 0)
        {
            ctx.r4.u64 = result;
            ctx.r5.s64 = kResultSize;
            ctx.r12.u64 = r12;
            ctx.lr = returns[2];
            sub_82B7A0B0(ctx, base);
        }
        StoreU32(base, block, LoadU32(base, check + kCheckResults));
        return block;
    }

    // A node's bounds as sub_822ADAA8 stores them (single precision): centre and half-size.
    struct Bounds
    {
        float x, y, z, half;
    };

    // sub_822ADAA8: the child's bounds (half the parent's half-size, then the centre) from the parent's and the child's
    // index (bit 2 = +x, bit 1 = +y, bit 0 = +z).
    Bounds ChildBounds(const Bounds& parent, uint32_t index, double halfConstant)
    {
        const double half = MulSingle(parent.half, halfConstant);
        const double x = (index & 4) ? 1.0 : -1.0, y = (index & 2) ? 1.0 : -1.0, z = (index & 1) ? 1.0 : -1.0;
        return { float(MaddSingle(x, half, parent.x)), float(MaddSingle(y, half, parent.y)),
            float(MaddSingle(z, half, parent.z)), float(half) };
    }

    // sub_82767670: the exit plane nearest along the ray picks the next child.
    uint32_t NextChild(double tx, uint32_t nx, double ty, uint32_t ny, double tz, uint32_t nz)
    {
        if (tx < ty)
            return tx < tz ? nx : nz;
        return ty < tz ? ny : nz;
    }

    struct LineCheck
    {
        PPCContext& ctx;
        uint8_t* base;
        uint32_t check;
        std::vector<TagUndo>* undo;
        // The guest's constants, loaded once.
        double zero = LoadFloat(base, kZero), one = LoadFloat(base, kOne), halfConstant = LoadFloat(base, kHalf);
        double high = LoadFloat(base, kFloatMax), low = LoadFloat(base, kFloatLowest);
        uint32_t zeroBits = LoadU32(base, kZero);

        // sub_822B1200 for `node`: t = the line's parameters at the node's planes (x0, y0, z0, x1, y1, z1), `parent`
        // its bounds, `stack` the r1 it is called with. The guest passes the bounds on its stack (+96, pointer at
        // +84); only the children read them, so they stay here.
        void Node(uint32_t node, const double (&t)[6], const Bounds& parent, uint32_t stack)
        {
            const uint32_t frame = stack - kLineFrame;
            StoreU32(base, frame, stack);
            const uint32_t r12 = stack - 88;
            if (t[3] < zero || t[4] < zero || t[5] < zero)
            {
                ctx.lr = kLineReturn;
                return;
            }

            double limit;
            const uint32_t best = LoadU32(base, check + kCheckBest);
            if ((LoadU32(base, check + kCheckFlags) & kFlagFindNearest) && best != 0)
                limit = LoadFloat(base, best + kResultTime);
            else
                limit = one;
            if (t[0] > limit || t[1] > limit || t[2] > limit)
            {
                ctx.lr = kLineReturn;
                return;
            }

            for (int32_t i = 0; i < int32_t(LoadU32(base, node + kNodePrimitiveCount)); ++i)
            {
                const uint32_t tag = LoadU32(base, kCollisionTag);
                const uint32_t primitive = LoadU32(base, LoadU32(base, node + kNodePrimitives) + uint32_t(i) * 4);
                if (LoadU32(base, primitive + kPrimTag) == tag)
                    continue;
                const uint32_t owner = LoadU32(base, primitive + kPrimOwner);
                SetTag(base, primitive, tag, undo);
                if (owner == 0)
                    continue;

                const uint32_t flags = LoadU32(base, check + kCheckFlags);
                if ((flags & kFlagZeroExtentFilter) == 0)
                {
                    if (flags & kFlagSkipOwnerTest)
                    {
                        if ((LoadU32(base, primitive + kPrimFlags600) & 0x10000000) == 0)
                            continue;
                    }
                    else
                    {
                        const uint32_t primitiveFlags = LoadU32(base, primitive + kPrimFlags448);
                        if ((primitiveFlags & 0x4000000) == 0 || (primitiveFlags & 0x10000000) == 0)
                            continue;
                        if ((LoadU32(base, owner + kOwnerFlags116) & 0x4000) == 0)
                            continue;
                        if (!PassesTraceFlags(base, primitive))
                            continue;
                    }
                }
                else
                {
                    if ((LoadU32(base, primitive + kPrimFlags440) & 0x40000) == 0)
                        continue;
                    if (!PassesZeroExtentFilter(base, primitive))
                        continue;
                    const uint32_t filter = LoadU32(base, check + kCheckFilter);
                        ctx.r1.u64 = frame;
                    ctx.r3.u64 = filter;
                    ctx.r4.u64 = primitive;
                    CallVirtual(ctx, base, filter, kFilterAcceptSlot, kLineFilter, r12);
                    if (ctx.r3.u32 == 0)
                        continue;
                }

                ctx.r1.u64 = frame;
                ctx.r3.u64 = primitive + kPrimOrigin;
                ctx.r4.u64 = primitive + kPrimExtent;
                ctx.r5.u64 = check + kCheckStart;
                ctx.r6.u64 = check + kCheckDirection;
                ctx.r7.u64 = check + kCheckOneOverDirection;
                ctx.r12.u64 = r12;
                ctx.lr = kLineBoxTest;
                sub_826C8FC0(ctx, base);
                if (ctx.r3.u32 == 0)
                    continue;

                const uint32_t source = LoadU32(base, check + kCheckSource);
                if (RelatedToSource(base, owner, source))
                    continue;
                const uint32_t checkFlags = LoadU32(base, check + kCheckFlags);
                if ((checkFlags & kFlagSkipOwnerTest) == 0)
                {
                    ctx.r3.u64 = owner;
                    ctx.r4.u64 = primitive;
                    ctx.r5.u64 = source;
                    ctx.r6.u64 = checkFlags;
                    CallVirtual(ctx, base, owner, kOwnerShouldTraceSlot, kLineShouldTrace, r12);
                    if (ctx.r3.u32 == 0)
                        continue;
                }

                const uint32_t block = CheckPrimitive(ctx, base, primitive, check, frame, kLineResult, r12, zeroBits,
                    { kLineCheck, kLineAlloc, kLineCopy });
                if (block == 0xFFFFFFFF)
                    continue;
                const uint32_t nearest = LoadU32(base, check + kCheckBest);
                StoreU32(base, check + kCheckResults, block);
                if (nearest == 0 || LoadFloat(base, block + kResultTime) < LoadFloat(base, nearest + kResultTime))
                    StoreU32(base, check + kCheckBest, block);
                if (LoadU32(base, check + kCheckFlags) & kFlagStopAtAny)
                {
                    ctx.r1.u64 = stack;
                    ctx.lr = kLineReturn;
                    return;
                }
            }

            if (LoadU32(base, node + kNodeChildren) == 0)
            {
                ctx.r1.u64 = stack;
                ctx.lr = kLineReturn;
                return;
            }

            double xm = MulSingle(AddSingle(t[0], t[3]), halfConstant);
            double ym = MulSingle(AddSingle(t[1], t[4]), halfConstant);
            double zm = MulSingle(AddSingle(t[2], t[5]), halfConstant);
            const uint32_t mirror = LoadU32(base, check + kCheckMirrorAxes);
            if (mirror != 0)
            {
                if (mirror & 4)
                    xm = LoadFloat(base, check + kCheckMirrorOrigin + 0) < double(parent.x) ? high : low;
                if (mirror & 2)
                    ym = LoadFloat(base, check + kCheckMirrorOrigin + 4) < double(parent.y) ? high : low;
                if (mirror & 1)
                    zm = LoadFloat(base, check + kCheckMirrorOrigin + 8) < double(parent.z) ? high : low;
            }

            // The first child: the entry plane is the largest of t[0..2].
            uint32_t index = 0;
            if (t[0] > t[1] && t[0] > t[2])
            {
                if (ym < t[0]) index = 2;
                if (zm < t[0]) index |= 1;
            }
            else if (!(t[0] > t[1]) && t[1] > t[2])
            {
                if (xm < t[1]) index = 4;
                if (zm < t[1]) index |= 1;
            }
            else
            {
                if (xm < t[2]) index = 4;
                if (ym < t[2]) index |= 2;
            }

            for (;;)
            {
                const Bounds bounds = ChildBounds(parent, index, halfConstant);
                if (index > 7)
                {
                    if (int32_t(index) < 8)
                        continue;
                    break;
                }
                const uint32_t child = LoadU32(base, node + kNodeChildren) +
                    ((LoadU32(base, check + kCheckMirrorIndex) << 4) ^ (index << 4));
                double tx0 = t[0], ty0 = t[1], tz0 = t[2], tx1 = xm, ty1 = ym, tz1 = zm;
                if (index & 4) tx0 = xm, tx1 = t[3];
                if (index & 2) ty0 = ym, ty1 = t[4];
                if (index & 1) tz0 = zm, tz1 = t[5];
                const double childT[6] = { tx0, ty0, tz0, tx1, ty1, tz1 };
                // The registers the guest's call passes (the bounds' address included), for what a child that makes
                // no call leaves behind.
                ctx.r3.u64 = child;
                ctx.r4.u64 = check;
                ctx.r9.u64 = frame + kLineBounds;
                ctx.f1.f64 = tx0, ctx.f2.f64 = ty0, ctx.f3.f64 = tz0, ctx.f4.f64 = tx1, ctx.f5.f64 = ty1, ctx.f6.f64 = tz1;
                ctx.r1.u64 = frame;
                Node(child, childT, bounds, frame);
                if (index == 7)
                {
                    ctx.r1.u64 = stack;
                    ctx.lr = kLineReturn;
                    return;
                }

                // sub_82767670's arguments as the guest passes them: for each axis the exit t and the child beyond it
                // (8 = out of this node).
                const double fx = tx1, fy = ty1, fz = tz1;
                const uint32_t nx = (index & 4) ? 8 : index | 4;
                const uint32_t ny = (index & 2) ? 8 : index | 2;
                const uint32_t nz = (index & 1) ? 8 : index | 1;
                index = NextChild(fx, nx, fy, ny, fz, nz);
                ctx.r3.u64 = index;
                ctx.r4.u64 = nx;
                ctx.r6.u64 = ny;
                ctx.r8.u64 = nz;
                ctx.f1.f64 = fx;
                ctx.f2.f64 = fy;
                ctx.f3.f64 = fz;
                if (int32_t(index) >= 8)
                    break;
            }
            ctx.r1.u64 = stack;
            ctx.lr = kLineReturnAfterChildren;
        }
    };

    struct BoxCheck
    {
        PPCContext& ctx;
        uint8_t* base;
        uint32_t check;
        std::vector<TagUndo>* undo;

        // sub_822AD6C8 for `node` with bounds `bounds` (centre, half-size at +12), called with r1 = `stack`.
        void Node(uint32_t node, uint32_t bounds, uint32_t stack, uint32_t r12)
        {
            const uint32_t frame = stack - kBoxFrame;
            StoreU32(base, frame, stack);
            ctx.r1.u64 = frame;
            ctx.lr = kBoxEntry;

            const int32_t count = int32_t(LoadU32(base, node + kNodePrimitiveCount));
            if (count > 0)
            {
                uint32_t tag = LoadU32(base, kCollisionTag);
                for (int32_t i = 0; i < int32_t(LoadU32(base, node + kNodePrimitiveCount)); ++i)
                {
                    const uint32_t primitive = LoadU32(base, LoadU32(base, node + kNodePrimitives) + uint32_t(i) * 4);
                    if (LoadU32(base, primitive + kPrimTag) == tag)
                        continue;
                    const uint32_t owner = LoadU32(base, primitive + kPrimOwner);
                    SetTag(base, primitive, tag, undo);
                    if (owner == 0)
                        continue;

                    // The primitive's box grown by the check's extent, against the line.
                    const uint32_t extent = frame + kBoxPrimExtent;
                    StoreFloat(base, extent + 0, AddSingle(LoadFloat(base, primitive + kPrimExtent + 0), LoadFloat(base, check + kCheckExtent + 0)));
                    StoreFloat(base, extent + 4, AddSingle(LoadFloat(base, primitive + kPrimExtent + 4), LoadFloat(base, check + kCheckExtent + 4)));
                    StoreFloat(base, extent + 8, AddSingle(LoadFloat(base, primitive + kPrimExtent + 8), LoadFloat(base, check + kCheckExtent + 8)));
                    ctx.r3.u64 = primitive + kPrimOrigin;
                    ctx.r4.u64 = extent;
                    ctx.r5.u64 = check + kCheckStart;
                    ctx.r6.u64 = check + kCheckDirection;
                    ctx.r7.u64 = check + kCheckOneOverDirection;
                    ctx.r12.u64 = r12;
                    ctx.lr = kBoxPrimTest;
                    sub_826C8FC0(ctx, base);
                    if (ctx.r3.u32 == 0)
                        continue;

                    const uint32_t source = LoadU32(base, check + kCheckSource);
                    if (RelatedToSource(base, owner, source))
                        continue;
                    const uint32_t flags = LoadU32(base, check + kCheckFlags);
                    if (flags & kFlagSkipOwnerTest)
                    {
                        if ((LoadU32(base, primitive + kPrimFlags600) & 0x10000000) == 0)
                            continue;
                    }
                    else
                    {
                        ctx.lr = kBoxTraceFlags;
                        if (!PassesTraceFlags(base, primitive))
                            continue;
                        const uint32_t primitiveFlags = LoadU32(base, primitive + kPrimFlags448);
                        if ((primitiveFlags & 0x10000000) == 0)
                            continue;
                        if ((LoadU32(base, owner + kOwnerFlags116) & 0x4000) == 0)
                            continue;
                        if ((primitiveFlags & 0x2000000) == 0)
                            continue;
                        ctx.r3.u64 = owner;
                        ctx.r4.u64 = primitive;
                        ctx.r5.u64 = source;
                        ctx.r6.u64 = flags;
                        CallVirtual(ctx, base, owner, kOwnerShouldTraceSlot, kBoxShouldTrace, r12);
                        if (ctx.r3.u32 == 0)
                        {
                            tag = LoadU32(base, kCollisionTag);
                            continue;
                        }
                    }

                    const uint32_t block = CheckPrimitive(ctx, base, primitive, check, frame, kBoxResult, r12,
                        LoadU32(base, kZero), { kBoxCheck, kBoxAlloc, kBoxCopy });
                    if (block != 0xFFFFFFFF)
                    {
                        StoreU32(base, check + kCheckResults, block);
                        if (LoadU32(base, check + kCheckFlags) & kFlagStopAtAny)
                        {
                            ctx.r1.u64 = stack;
                            return;
                        }
                    }
                    tag = LoadU32(base, kCollisionTag);
                }
            }

            const uint32_t children = LoadU32(base, node + kNodeChildren);
            if (children == 0)
            {
                ctx.r1.u64 = stack;
                return;
            }

            // sub_822ADE58: the children the check's box overlaps, in the guest's order (its arguments and count
            // left in the registers as the guest's call leaves them).
            ctx.lr = kBoxOverlaps;
            ctx.r4.u64 = bounds;
            ctx.r5.u64 = check + kCheckBox;
            ctx.r6.u64 = frame + kBoxOverlapList;
            uint32_t overlapping[8];
            uint32_t overlapCount = 0;
            {
                const uint32_t box = check + kCheckBox;
                const double cx = LoadFloat(base, bounds + 0);
                if (LoadFloat(base, box + 12) > cx)
                {
                    const double cy = LoadFloat(base, bounds + 4);
                    if (LoadFloat(base, box + 16) > cy)
                    {
                        const double cz = LoadFloat(base, bounds + 8);
                        if (LoadFloat(base, box + 20) > cz) overlapping[overlapCount++] = 7;
                        if (!(LoadFloat(base, box + 8) > cz)) overlapping[overlapCount++] = 6;
                    }
                    if (!(LoadFloat(base, box + 4) > cy))
                    {
                        const double cz = LoadFloat(base, bounds + 8);
                        if (LoadFloat(base, box + 20) > cz) overlapping[overlapCount++] = 5;
                        if (!(LoadFloat(base, box + 8) > cz)) overlapping[overlapCount++] = 4;
                    }
                }
                if (!(LoadFloat(base, box + 0) > cx))
                {
                    const double cy = LoadFloat(base, bounds + 4);
                    if (LoadFloat(base, box + 16) > cy)
                    {
                        const double cz = LoadFloat(base, bounds + 8);
                        if (LoadFloat(base, box + 20) > cz) overlapping[overlapCount++] = 3;
                        if (!(LoadFloat(base, box + 8) > cz)) overlapping[overlapCount++] = 2;
                    }
                    if (!(LoadFloat(base, box + 4) > cy))
                    {
                        const double cz = LoadFloat(base, bounds + 8);
                        if (LoadFloat(base, box + 20) > cz) overlapping[overlapCount++] = 1;
                        if (!(LoadFloat(base, box + 8) > cz)) overlapping[overlapCount++] = 0;
                    }
                }
            }

            ctx.r3.u64 = overlapCount;
            const double half = LoadFloat(base, kHalf);
            for (uint32_t k = 0; k < overlapCount; ++k)
            {
                const uint32_t index = overlapping[k];
                const double childHalf = MulSingle(LoadFloat(base, bounds + 12), half);
                const uint32_t center = frame + kBoxCenter, grown = frame + kBoxGrown;
                StoreFloat(base, frame + kBoxHalf, childHalf);
                StoreFloat(base, grown + 0, AddSingle(LoadFloat(base, check + kCheckExtent + 0), childHalf));
                StoreFloat(base, grown + 4, AddSingle(LoadFloat(base, check + kCheckExtent + 4), childHalf));
                StoreFloat(base, grown + 8, AddSingle(LoadFloat(base, check + kCheckExtent + 8), childHalf));
                const double x = (index & 4) ? 1.0 : -1.0, y = (index & 2) ? 1.0 : -1.0, z = (index & 1) ? 1.0 : -1.0;
                StoreFloat(base, center + 0, MaddSingle(x, childHalf, LoadFloat(base, bounds + 0)));
                StoreFloat(base, center + 4, MaddSingle(y, childHalf, LoadFloat(base, bounds + 4)));
                StoreFloat(base, center + 8, MaddSingle(z, childHalf, LoadFloat(base, bounds + 8)));
                ctx.r3.u64 = center;
                ctx.r4.u64 = grown;
                ctx.r5.u64 = check + kCheckStart;
                ctx.r6.u64 = check + kCheckDirection;
                ctx.r7.u64 = check + kCheckOneOverDirection;
                ctx.r12.u64 = r12;
                ctx.lr = kBoxChildTest;
                sub_826C8FC0(ctx, base);
                if (ctx.r3.u32 == 0)
                    continue;

                const uint32_t child = LoadU32(base, node + kNodeChildren) + (index << 4);
                ctx.r3.u64 = child;
                ctx.r4.u64 = check;
                ctx.r5.u64 = center;
                ctx.lr = kBoxChild;
                Node(child, center, frame, kBoxChild);
                ctx.r1.u64 = frame;
                if (LoadU32(base, check + kCheckResults) != 0 && (LoadU32(base, check + kCheckFlags) & kFlagStopAtAny))
                    break;
            }
            ctx.r1.u64 = stack;
        }
    };
}

namespace
{
    // Mode 2: the results a run pushed on the check's list (newest first), down to the head it started from.
    std::vector<uint8_t> NewResults(uint8_t* base, uint32_t check, uint32_t oldHead, uint32_t& nearestIndex,
        uint32_t oldNearest)
    {
        std::vector<uint8_t> results;
        nearestIndex = 0xFFFFFFFF;
        const uint32_t nearest = LoadU32(base, check + kCheckBest);
        uint32_t count = 0;
        for (uint32_t block = LoadU32(base, check + kCheckResults); block != oldHead && count < 4096; ++count)
        {
            if (block == nearest && nearest != oldNearest)
                nearestIndex = count;
            // +0 links the list; the blocks themselves differ between the runs.
            results.insert(results.end(), base + block + 4, base + block + kResultSize);
            if (block == 0)
                break;
            block = LoadU32(base, block);
        }
        return results;
    }

    // Mode 2: runs the native code, undoes the collision tags and the results it pushed, runs the game's code and
    // compares the two result lists (contents and which one became the nearest). The primitives' own line checks and
    // the memory stack run twice; the second run's results are the ones kept.
    template<typename Native>
    void RunChecked(const char* name, PPCContext& ctx, uint8_t* base, PPCFunc* guest, Native native, uint32_t& calls,
        uint32_t& differences)
    {
        const uint32_t check = ctx.r4.u32;
        const uint32_t oldHead = LoadU32(base, check + kCheckResults);
        const uint32_t oldNearest = LoadU32(base, check + kCheckBest);
        const PPCContext before = ctx;

        std::vector<TagUndo> undo;
        native(&undo);
        uint32_t nativeNearest;
        const std::vector<uint8_t> nativeResults = NewResults(base, check, oldHead, nativeNearest, oldNearest);
        for (auto it = undo.rbegin(); it != undo.rend(); ++it)
            StoreU32(base, it->address, it->value);
        StoreU32(base, check + kCheckResults, oldHead);
        StoreU32(base, check + kCheckBest, oldNearest);

        ctx = before;
        g_inGuestRun = true;
        guest(ctx, base);
        g_inGuestRun = false;
        uint32_t guestNearest;
        const std::vector<uint8_t> guestResults = NewResults(base, check, oldHead, guestNearest, oldNearest);

        ++calls;
        if ((nativeResults != guestResults || nativeNearest != guestNearest) && ++differences <= 20)
            LOG_ERROR("[collision] {} differs for check {:08X}: native {} results (nearest {}), guest {} (nearest {})", name,
                check, nativeResults.size() / (kResultSize - 4), int32_t(nativeNearest), guestResults.size() / (kResultSize - 4),
                int32_t(guestNearest));
        // A line every 65,536 calls of each function, so a run shows the check is active even with no differences.
        if ((calls & 0xFFFF) == 0)
            LOG_ERROR("[collision] {} checked {} calls, {} differ", name, calls, differences);
    }
}

PPC_FUNC(sub_822B1200)
{
    const int mode = NativeCollisionMode();
    if (mode == 0)
    {
        __imp__sub_822B1200(ctx, base);
        return;
    }

    auto native = [&](std::vector<TagUndo>* undo)
        {
            ctx.fpscr.disableFlushMode();
            const double t[6] = { ctx.f1.f64, ctx.f2.f64, ctx.f3.f64, ctx.f4.f64, ctx.f5.f64, ctx.f6.f64 };
            const uint32_t stack = ctx.r1.u32;
            const uint32_t parent = LoadU32(base, stack + kLineArgument);
            const Bounds bounds{ float(LoadFloat(base, parent + 0)), float(LoadFloat(base, parent + 4)),
                float(LoadFloat(base, parent + 8)), float(LoadFloat(base, parent + 12)) };
            LineCheck line{ ctx, base, ctx.r4.u32, undo };
            line.Node(ctx.r3.u32, t, bounds, stack);
            ctx.r1.u64 = stack;
        };
    if (mode == 2)
    {
        // The game's code recurses through this hook.
        if (g_inGuestRun)
            __imp__sub_822B1200(ctx, base);
        else
        {
            static uint32_t calls, differences;
            RunChecked("sub_822B1200", ctx, base, __imp__sub_822B1200, native, calls, differences);
        }
        return;
    }
    native(nullptr);
}

PPC_FUNC(sub_822AD6C8)
{
    const int mode = NativeCollisionMode();
    if (mode == 0)
    {
        __imp__sub_822AD6C8(ctx, base);
        return;
    }

    auto native = [&](std::vector<TagUndo>* undo)
        {
            ctx.fpscr.disableFlushMode();
            const uint32_t stack = ctx.r1.u32;
            BoxCheck box{ ctx, base, ctx.r4.u32, undo };
            box.Node(ctx.r3.u32, ctx.r5.u32, stack, uint32_t(ctx.lr));
            ctx.r1.u64 = stack;
        };
    if (mode == 2)
    {
        if (g_inGuestRun)
            __imp__sub_822AD6C8(ctx, base);
        else
        {
            static uint32_t calls, differences;
            RunChecked("sub_822AD6C8", ctx, base, __imp__sub_822AD6C8, native, calls, differences);
        }
        return;
    }
    native(nullptr);
}
