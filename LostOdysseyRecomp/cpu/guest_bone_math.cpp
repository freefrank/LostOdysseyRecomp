// Native versions of the bone-matrix loops of UE3's skeletal animation, the hottest game code in the first battle.
// USkeletalMeshComponent::ComposeSkeleton (sub_822B98C8, plus sub_822BA730) and the skinning-matrix loops of
// sub_822C3E40 multiply one FMatrix per bone. The recompiled loops copy both matrices to the stack through memory,
// build the columns and run 16 dot products; here it's one load, the multiply and one store per matrix.
// The mid-asm hooks return true when they did the work; LO_NATIVE_BONES=0 keeps the recompiled loops, for A/B runs.
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <cmath>
#include <cstdlib>
#include <cstring>

#if defined(__aarch64__)
#include <arm_neon.h>

namespace
{
    constexpr size_t kMatrixSize = 64;
    constexpr uint32_t kGuestIdentityMatrix = 0x8323A520; // FMatrix::Identity

    // 0: recompiled code. 1: native. 2: sub_822B8D68 runs both and logs where they differ.
    int NativeBonesMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_BONES");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    bool NativeBonesEnabled()
    {
        return NativeBonesMode() != 0;
    }

    uint32_t LoadU32(uint8_t* base, uint32_t address)
    {
        return ByteSwap(*reinterpret_cast<uint32_t*>(base + address));
    }

    float32x4_t LoadRow(const uint8_t* p)
    {
        return vreinterpretq_f32_u8(vrev32q_u8(vld1q_u8(p)));
    }

    void StoreRow(uint8_t* p, float32x4_t row)
    {
        vst1q_u8(p, vrev32q_u8(vreinterpretq_u8_f32(row)));
    }

    void LoadMatrix(float32x4_t m[4], const uint8_t* p)
    {
        for (size_t i = 0; i < 4; ++i)
            m[i] = LoadRow(p + i * 16);
    }

    // out = a * b, row vectors, as FMatrix::operator*. Rounding can differ from the guest's dot products by an ulp.
    void MultiplyAndStore(uint8_t* out, const float32x4_t a[4], const float32x4_t b[4])
    {
        for (size_t i = 0; i < 4; ++i)
        {
            float32x4_t row = vmulq_laneq_f32(b[0], a[i], 0);
            row = vfmaq_laneq_f32(row, b[1], a[i], 1);
            row = vfmaq_laneq_f32(row, b[2], a[i], 2);
            row = vfmaq_laneq_f32(row, b[3], a[i], 3);
            StoreRow(out + i * 16, row);
        }
    }

    // sub_822BA730: quaternion (+0), translation (+16), scale (+28) to a matrix.
    void AtomToMatrix(float32x4_t m[4], const uint8_t* atom)
    {
        const float32x4_t rotation = LoadRow(atom);
        const float32x4_t translation = LoadRow(atom + 16);
        const float x = vgetq_lane_f32(rotation, 0);
        const float y = vgetq_lane_f32(rotation, 1);
        const float z = vgetq_lane_f32(rotation, 2);
        const float w = vgetq_lane_f32(rotation, 3);
        const float scale = vgetq_lane_f32(translation, 3);

        const float x2 = x * 2.0f, y2 = y * 2.0f, z2 = z * 2.0f;
        const float xx = x * x2, yy = y * y2, zz = z * z2;
        const float yz = y * z2, wx = w * x2;
        const float xy = x * y2, wz = w * z2;
        const float xz = x * z2, wy = w * y2;

        const float row0[4] = { 1.0f - (zz + yy), wz + xy, xz - wy, 0.0f };
        const float row1[4] = { xy - wz, 1.0f - (zz + xx), wx + yz, 0.0f };
        const float row2[4] = { wy + xz, yz - wx, 1.0f - (yy + xx), 0.0f };

        const uint32x4_t keepWZero = { ~0u, ~0u, ~0u, 0u };
        m[0] = vmulq_n_f32(vld1q_f32(row0), scale);
        m[1] = vmulq_n_f32(vld1q_f32(row1), scale);
        m[2] = vmulq_n_f32(vld1q_f32(row2), scale);
        m[3] = vsetq_lane_f32(1.0f, vreinterpretq_f32_u32(vandq_u32(vreinterpretq_u32_f32(translation), keepWZero)), 3);
    }
}

// sub_822B98C8 at 0x822B9A14, a bone with a parent: Matrix(atom[bone]) * world[parent] into world[bone].
bool NativeComposeBone(PPCRegister& r23, PPCRegister& r24, PPCRegister& r27)
{
    if (!NativeBonesEnabled())
        return false;

    uint8_t* base = g_memory.base;
    const uint32_t bone = r23.u32;
    const uint32_t world = LoadU32(base, r27.u32 + 708);
    const uint32_t bones = LoadU32(base, LoadU32(base, r27.u32 + 640) + 124);
    const uint32_t parent = LoadU32(base, bones + bone * 68 + 56);

    float32x4_t local[4], parentWorld[4];
    AtomToMatrix(local, base + LoadU32(base, r24.u32) + bone * 32);
    LoadMatrix(parentWorld, base + world + parent * kMatrixSize);
    MultiplyAndStore(base + world + bone * kMatrixSize, local, parentWorld);
    return true;
}

namespace
{
    // refToLocal[bone] * bases[index] into out[bone] for every bone of the LOD, identity when the index is invalid.
    // The bone map is only there for a mesh that follows a master component's pose (r31 at the first hook).
    void SkinningMatrices(PPCRegister& r27, PPCRegister& r29, PPCRegister& r30, uint32_t basesOwner, uint32_t boneMap)
    {
        uint8_t* base = g_memory.base;
        const uint32_t lod = r27.u32;
        const uint32_t skeleton = r29.u32;

        for (int32_t i = 0; i < int32_t(LoadU32(base, lod + 28)); ++i)
        {
            const uint32_t list = LoadU32(base, lod + 24);
            const uint32_t bone = ByteSwap(*reinterpret_cast<uint16_t*>(base + list + i * 2));
            const int32_t index = boneMap ? int32_t(LoadU32(base, boneMap + bone * 4)) : int32_t(bone);
            uint8_t* out = base + LoadU32(base, r30.u32) + bone * kMatrixSize;

            if (int32_t(bone) < int32_t(LoadU32(base, skeleton + 176)) && index >= 0 && index < int32_t(LoadU32(base, basesOwner + 712)))
            {
                float32x4_t refToLocal[4], bases[4];
                LoadMatrix(refToLocal, base + LoadU32(base, skeleton + 172) + bone * kMatrixSize);
                LoadMatrix(bases, base + LoadU32(base, basesOwner + 708) + uint32_t(index) * kMatrixSize);
                MultiplyAndStore(out, refToLocal, bases);
            }
            else
            {
                std::memcpy(out, base + kGuestIdentityMatrix, kMatrixSize);
            }
        }
    }
}

// sub_822C3E40 at 0x822C3ED8: the pose comes from the master component in r31, through the bone map at r28+748.
bool NativeSkinningMatricesMaster(PPCRegister& r27, PPCRegister& r28, PPCRegister& r29, PPCRegister& r30, PPCRegister& r31)
{
    if (!NativeBonesEnabled())
        return false;

    SkinningMatrices(r27, r29, r30, r31.u32, LoadU32(g_memory.base, r28.u32 + 748));
    return true;
}

// sub_822C3E40 at 0x822C40F0: the component's own pose.
bool NativeSkinningMatricesOwn(PPCRegister& r27, PPCRegister& r28, PPCRegister& r29, PPCRegister& r30)
{
    if (!NativeBonesEnabled())
        return false;

    SkinningMatrices(r27, r29, r30, r28.u32, 0);
    return true;
}

namespace
{
    constexpr float kLinearAbove = 0.9999f; // the guest's threshold at 0x82000D80
    constexpr float kEpsilon = 1e-5f;       // the guest's constant at 0x82000D90

    float32x4_t Slerp(float32x4_t a, float32x4_t b, float alpha)
    {
        const float dot = vaddvq_f32(vmulq_f32(a, b));
        const float cosOmega = std::fabs(dot);

        float scaleA = 1.0f - alpha;
        float scaleB = alpha;
        if (cosOmega < kLinearAbove)
        {
            const float omega = acosf(cosOmega);
            const float invSin = 1.0f / sinf(omega);
            scaleA = sinf(scaleA * omega) * invSin;
            scaleB = sinf(omega * alpha) * invSin;
        }
        if (dot < 0.0f)
            scaleB = -scaleB;

        return vmlaq_n_f32(vmulq_n_f32(b, scaleB), a, scaleA);
    }

    float32x4_t NormalizedOrIdentity(float32x4_t q, bool& scaled)
    {
        const float lengthSquared = vaddvq_f32(vmulq_f32(q, q));
        scaled = false;
        if (lengthSquared < kEpsilon)
        {
            const float identity[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
            return vld1q_f32(identity);
        }
        if (std::fabs(lengthSquared - 1.0f) < kEpsilon)
            return q;

        scaled = true;
        return vmulq_n_f32(q, 1.0f / sqrtf(lengthSquared));
    }
}

extern "C" PPC_FUNC(__imp__sub_822B9630);
extern "C" PPC_FUNC(__imp__sub_822B9778);
extern "C" PPC_FUNC(__imp__sub_822B8D68);

// FQuat::Slerp, shortest path: r3 = out, r4 = a, r5 = b, f1 = alpha. The guest goes through its double-precision
// sin (sub_822A2FE0, called three times) and acos.
PPC_FUNC(sub_822B9630)
{
    if (!NativeBonesEnabled())
    {
        __imp__sub_822B9630(ctx, base);
        return;
    }

    const float32x4_t a = LoadRow(base + ctx.r4.u32);
    const float32x4_t b = LoadRow(base + ctx.r5.u32);
    StoreRow(base + ctx.r3.u32, Slerp(a, b, float(ctx.f1.f64)));
}

// FQuat::Normalize: r3 = quaternion. Returns 1 if it scaled it, 0 if already unit or set to identity (too small).
PPC_FUNC(sub_822B9778)
{
    if (!NativeBonesEnabled())
    {
        __imp__sub_822B9778(ctx, base);
        return;
    }

    uint8_t* quat = base + ctx.r3.u32;
    const float32x4_t q = LoadRow(quat);
    bool scaled;
    const float32x4_t result = NormalizedOrIdentity(q, scaled);
    if (scaled || vaddvq_f32(vmulq_f32(q, q)) < kEpsilon)
        StoreRow(quat, result);
    ctx.r3.u64 = scaled;
}

namespace
{
    constexpr uint32_t kRotationKeySizes = 0x82189554;     // bytes per component, by format (sub_822B9508)
    constexpr uint32_t kRotationKeyComponents = 0x82189538;
    constexpr uint32_t kFixed48Scales = 0x82218268;        // z, then x and y (format 4, sub_826D29F8)
    constexpr uint32_t kGuestIdentityQuat = 0x8323A560;
    constexpr uint32_t kRotationKeysOffset = 24;           // the rotation stream's header
    constexpr uint32_t kKeyTableFormat = 6;                // keys at uneven times, with a table of their frames

    float LoadFloat(uint8_t* base, uint32_t address)
    {
        const uint32_t value = LoadU32(base, address);
        float result;
        std::memcpy(&result, &value, sizeof(result));
        return result;
    }

    uint32_t LoadU16(uint8_t* base, uint32_t address)
    {
        return ByteSwap(*reinterpret_cast<uint16_t*>(base + address));
    }

    // The quaternion's w from x, y and z (sub_822B95D8).
    float32x4_t WithRebuiltW(float32x4_t q)
    {
        const float x = vgetq_lane_f32(q, 0);
        const float y = vgetq_lane_f32(q, 1);
        const float z = vgetq_lane_f32(q, 2);
        const float w = 1.0f - x * x - y * y - z * z;
        return vsetq_lane_f32(w > 0.0f ? sqrtf(w) : 0.0f, q, 3);
    }

    // sub_822B9508: formats 0 (four floats), 1 and 6 (three floats), 4 (11-11-10 fixed point).
    float32x4_t DecodeRotation(uint8_t* base, uint32_t keys, int32_t index, uint32_t format)
    {
        const uint32_t stride = LoadU32(base, kRotationKeySizes + format * 4) * LoadU32(base, kRotationKeyComponents + format * 4);
        const uint32_t key = keys + index * stride + kRotationKeysOffset;
        if (format == 0)
            return LoadRow(base + key);
        if (format != 4)
            return WithRebuiltW(LoadRow(base + key));

        const uint32_t packed = LoadU32(base, key);
        const float scaleZ = LoadFloat(base, kFixed48Scales);
        const float scaleXY = LoadFloat(base, kFixed48Scales + 4);
        const float xyz[4] =
        {
            float(int32_t(packed >> 21) - 1023) * scaleXY,
            float(int32_t((packed >> 10) & 0x7FF) - 1023) * scaleXY,
            float(int32_t(packed & 0x3FF) - 511) * scaleZ,
            0.0f,
        };
        return WithRebuiltW(vld1q_f32(xyz));
    }

    // sub_826D3B60, including its answer of -1 when nothing fits.
    uint32_t FindKey(uint8_t* base, uint32_t frames, uint32_t frame, uint32_t high)
    {
        uint32_t low = 0;
        high &= 0xFFFF;
        while (int32_t(high - low) > 0)
        {
            const int32_t span = int32_t(high - low);
            const uint32_t middle = (low + span / 2) & 0xFFFF;
            if (span == 1)
                return LoadU16(base, frames + middle * 2) <= frame ? middle : 0;

            if (LoadU16(base, frames + middle * 2) > frame)
                high = middle;
            else
                low = middle;
        }
        return 0xFFFF;
    }

    struct KeyPair
    {
        int32_t first;
        int32_t second;
        float alpha;
    };

    // Keys at the time, when there are at least two. cacheShift: the half of the cache word for this track.
    KeyPair PickKeys(uint8_t* base, int32_t count, uint32_t frames, bool hasFrames, float length, float time, uint32_t cache, uint32_t cacheShift)
    {
        const int32_t last = count - 1;
        KeyPair keys;
        if (!hasFrames)
        {
            const float position = time / (length / float(last));
            keys.first = int32_t(uint16_t(int32_t(std::floor(position))));
            if (keys.first >= last)
                keys.first = last;
            keys.second = keys.first + 1;
            keys.alpha = position - float(keys.first);
            return keys;
        }

        const int32_t frameCount = int32_t(LoadU16(base, frames - 2));
        const float position = time / (length / float(frameCount - 1));
        const uint32_t frame = uint16_t(int32_t(std::floor(position)));

        // The guest takes the frame number at the cached key as the next key's index. Kept as it is: its result is
        // clamped, and the cache word it writes is read back the same way.
        const uint32_t cached = (LoadU32(base, cache) >> cacheShift) & 0xFFFF;
        uint32_t found = 0xFFFF;
        if (int32_t(cached) < count)
        {
            const uint32_t cachedFrame = LoadU16(base, frames + cached * 2);
            if (cachedFrame <= frame)
                found = cachedFrame;
        }
        if (found == 0xFFFF)
            found = FindKey(base, frames, frame, uint32_t(count) + 0xFFFF);

        const uint32_t kept = LoadU32(base, cache) & ~(0xFFFFu << cacheShift);
        *reinterpret_cast<uint32_t*>(base + cache) = ByteSwap(kept | (found << cacheShift));

        keys.first = int32_t(found) < last ? int32_t(found) : last;
        keys.second = keys.first + 1;
        keys.alpha = 0.0f;
        if (keys.second < count)
        {
            const float frame0 = float(LoadU16(base, frames + keys.first * 2));
            const float frame1 = float(LoadU16(base, frames + keys.second * 2));
            keys.alpha = (position - frame0) / (frame1 - frame0);
        }
        else
        {
            keys.second = keys.first;
        }
        return keys;
    }

    struct TrackQuery
    {
        uint32_t positions, rotations;
        int32_t positionCount, rotationCount;
        uint32_t positionFormat, rotationFormat;
        float length, time;
        bool looping;
        uint32_t cache;
    };

    // sub_822B8D68: a bone's atom (quaternion, translation, scale) at a time in an animation sequence.
    void SampleTrack(uint8_t* base, const TrackQuery& q, uint8_t* atom)
    {
        const float one[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        float32x4_t translation = vld1q_f32(one);
        const auto storeAtom = [&](float32x4_t rotation)
            {
                StoreRow(atom, rotation);
                StoreRow(atom + 16, vsetq_lane_f32(1.0f, translation, 3));
            };
        const auto loadPosition = [&](int32_t index) { return LoadRow(base + q.positions + index * 12); };

        if (q.positionCount == 0 || q.rotationCount == 0)
        {
            storeAtom(LoadRow(base + kGuestIdentityQuat));
            return;
        }

        const auto decodeSingle = [&](int32_t index)
            {
                return q.rotationCount > 1 ? DecodeRotation(base, q.rotations, index, q.rotationFormat)
                                           : WithRebuiltW(LoadRow(base + q.rotations));
            };

        if (!(q.time > 0.0f))
        {
            translation = loadPosition(0);
            storeAtom(decodeSingle(0));
            return;
        }
        if (!(q.time < q.length))
        {
            translation = loadPosition(q.looping ? 0 : q.positionCount - 1);
            storeAtom(decodeSingle(q.looping ? 0 : q.rotationCount - 1));
            return;
        }

        if (q.positionCount == 1)
        {
            translation = loadPosition(0);
        }
        else
        {
            const uint32_t frames = q.positions + q.positionCount * 12 + 2;
            KeyPair keys = PickKeys(base, q.positionCount, frames, q.positionFormat == kKeyTableFormat, q.length, q.time, q.cache, 0);
            if (keys.second == q.positionCount)
                keys.second = q.looping ? 0 : keys.first;
            const float32x4_t a = loadPosition(keys.first);
            const float32x4_t b = loadPosition(keys.second);
            translation = vaddq_f32(vmulq_n_f32(vsubq_f32(b, a), keys.alpha), a);
        }

        if (q.rotationCount == 1)
        {
            storeAtom(WithRebuiltW(LoadRow(base + q.rotations)));
            return;
        }

        const uint32_t frames = q.rotations + q.rotationCount * 12 + 26;
        KeyPair keys = PickKeys(base, q.rotationCount, frames, q.rotationFormat == kKeyTableFormat, q.length, q.time, q.cache, 16);
        if (keys.second == q.rotationCount)
            keys.second = q.looping ? 0 : keys.first;
        const float32x4_t a = DecodeRotation(base, q.rotations, keys.first, q.rotationFormat);
        const float32x4_t b = DecodeRotation(base, q.rotations, keys.second, q.rotationFormat);
        bool scaled;
        storeAtom(NormalizedOrIdentity(Slerp(a, b, keys.alpha), scaled));
    }

    void CheckSampleTrack(PPCContext& ctx, uint8_t* base, const TrackQuery& q)
    {
        static uint32_t calls, differences;

        const bool usesCache = q.positionFormat == kKeyTableFormat || q.rotationFormat == kKeyTableFormat;
        const uint32_t cacheBefore = usesCache ? LoadU32(base, q.cache) : 0;
        const uint32_t atom = ctx.r3.u32;
        alignas(16) uint8_t native[32];
        SampleTrack(base, q, native);
        const uint32_t cacheNative = usesCache ? LoadU32(base, q.cache) : 0;
        if (usesCache)
            *reinterpret_cast<uint32_t*>(base + q.cache) = ByteSwap(cacheBefore);

        __imp__sub_822B8D68(ctx, base);

        const uint32_t cacheGuest = usesCache ? LoadU32(base, q.cache) : 0;
        float worst = 0.0f;
        for (uint32_t i = 0; i < 8; ++i)
        {
            const uint32_t word = ByteSwap(*reinterpret_cast<uint32_t*>(native + i * 4));
            float nativeValue;
            std::memcpy(&nativeValue, &word, sizeof(nativeValue));
            const float difference = std::fabs(LoadFloat(base, atom + i * 4) - nativeValue);
            if (!(difference <= worst))
                worst = difference;
        }

        ++calls;
        if ((cacheNative != cacheGuest || !(worst < 0.002f)) && ++differences <= 40)
            LOG_WARNING("[bones] sub_822B8D68 differs by {}: time {}/{} keys {}/{} formats {}/{} cache {:08X}/{:08X}", worst, q.time, q.length,
                q.positionCount, q.rotationCount, q.positionFormat, q.rotationFormat, cacheNative, cacheGuest);
        if ((calls & 0x3FFFF) == 0)
            LOG_WARNING("[bones] sub_822B8D68 checked {} calls, {} differ", calls, differences);
    }
}

PPC_FUNC(sub_822B8D68)
{
    const uint32_t rotationFormat = ctx.r9.u32;
    const bool decoded = rotationFormat == 0 || rotationFormat == 1 || rotationFormat == 4 || rotationFormat == kKeyTableFormat;
    if (!NativeBonesEnabled() || !decoded)
    {
        __imp__sub_822B8D68(ctx, base);
        return;
    }

    TrackQuery query;
    query.positions = ctx.r4.u32;
    query.positionCount = ctx.r5.s32;
    query.rotations = ctx.r6.u32;
    query.rotationCount = ctx.r7.s32;
    query.positionFormat = ctx.r8.u32;
    query.rotationFormat = rotationFormat;
    query.length = float(ctx.f1.f64);
    query.time = float(ctx.f2.f64);
    query.looping = LoadU32(base, ctx.r1.u32 + 92) != 0;
    query.cache = LoadU32(base, ctx.r1.u32 + 100);

    if (NativeBonesMode() == 2)
        CheckSampleTrack(ctx, base, query);
    else
        SampleTrack(base, query, base + ctx.r3.u32);
}

#else

bool NativeComposeBone(PPCRegister&, PPCRegister&, PPCRegister&) { return false; }
bool NativeSkinningMatricesMaster(PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&) { return false; }
bool NativeSkinningMatricesOwn(PPCRegister&, PPCRegister&, PPCRegister&, PPCRegister&) { return false; }

#endif
