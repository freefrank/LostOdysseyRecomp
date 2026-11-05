// The render thread's bone matrix upload of a skinned mesh (sub_823B7830: r3 = the mesh object, r4 = the new
// dynamic data). It deletes the old data (virtual call), keeps the new one at +56, then for the LOD's chunks (one or
// two passes, the second for +24) fills each chunk's array of 3x4 bone matrices: per bone, TArray::AddUninitialized
// and the reference-to-local matrix (4x4, 64 bytes) through the chunk's bone map, transposed as 12 floats; then the
// array's +12 gets the chunk's +48, at most 4. In the busy
// street it was 2.8% of the game's rendering thread, which sets Guest Main's pace there.
//
// Native only when no array grows (each chunk's bone count fits its array's Max, as after the first frames): then
// it's the same stores, and the delete through the same frame. Otherwise the recompiled
// function runs. The prologue's comparison of the old and new data branches to the same place either way, so it's
// not repeated. The recompiled lfs/stfs pass each word through double with flushing off: only a signalling NaN
// changes (quieted, bit 22 set; garbage in a mesh's first frames had some), so that's done here too. General registers aren't reproduced: the caller reads none after the call (void, savegprlr frame).
// LO_NATIVE_BONE_UPLOAD=0 keeps the recompiled code; 2 runs it and compares the arrays with the native result (first
// 20 differences, and a count every 2^16 calls).
#include <stdafx.h>
#include <kernel/memory.h>
#include <os/logger.h>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

extern "C" PPC_FUNC(__imp__sub_823B7830);

namespace
{
    int NativeBoneUploadMode()
    {
        static const int mode = []()
            {
                const char* value = std::getenv("LO_NATIVE_BONE_UPLOAD");
                return value ? std::atoi(value) : 1;
            }();
        return mode;
    }

    constexpr uint32_t kChunkStride = 52;
    constexpr uint32_t kLodStride = 320;
    constexpr uint32_t kFrameSize = 208;          // the guest function's frame (stwu r1,-208)
    constexpr uint32_t kDeleteReturn = 0x823B78E0; // after the bctrl of the delete
    // Word j of a bone's 3x4 is word kTranspose[j] of its 4x4.
    constexpr uint32_t kTranspose[12] = {0, 4, 8, 12, 1, 5, 9, 13, 2, 6, 10, 14};

    // One chunk array to fill: its TArray (Data, Num, Max, then the influence field at +12) and the chunk.
    struct Fill
    {
        uint32_t array, chunk;
        int32_t count;
    };

    // The chunk arrays of both passes, or false when one would grow (the recompiled code reallocates).
    bool Plan(uint8_t* base, uint32_t self, uint32_t data, std::vector<Fill>& fills)
    {
        fills.clear();
        const uint32_t lod = PPC_LOAD_U32(data + 28);
        const uint32_t lodInfo = PPC_LOAD_U32(PPC_LOAD_U32(PPC_LOAD_U32(self + 16) + 160) + lod * 4);
        const uint32_t lodData = PPC_LOAD_U32(self + 28) + lod * kLodStride;
        const int32_t chunks = int32_t(PPC_LOAD_U32(lodInfo + 16));
        const uint32_t chunkBase = PPC_LOAD_U32(lodInfo + 12);
        const bool influences = int32_t(PPC_LOAD_U32(data + 44)) > 0;
        const bool second = PPC_LOAD_U32(self + 24) != 0;
        for (int pass = 0; pass < (second ? 2 : 1); ++pass)
        {
            const uint32_t list = PPC_LOAD_U32(lodData + (pass ? (influences ? 308 : 284) : (influences ? 296 : 272)));
            for (int32_t c = 0; c < chunks; ++c)
            {
                const uint32_t chunk = chunkBase + uint32_t(c) * kChunkStride;
                const uint32_t array = PPC_LOAD_U32(list + uint32_t(c) * 4) + 252;
                const int32_t count = int32_t(PPC_LOAD_U32(chunk + 32));
                if (count > 0 && (count > int32_t(PPC_LOAD_U32(array + 8)) || !PPC_LOAD_U32(array)))
                    return false;
                fills.push_back({array, chunk, count});
            }
        }
        return true;
    }

    // The array's +12 as the recompiled code stores it after the bones: the chunk's +48, at most 4.
    uint32_t InfluenceField(uint8_t* base, uint32_t chunk)
    {
        const int32_t value = int32_t(PPC_LOAD_U32(chunk + 48));
        return uint32_t(value > 4 ? 4 : value);
    }

    // A word as lfs then stfs leave it (guest byte order in and out): a signalling NaN comes back quiet.
    uint32_t ThroughDouble(uint32_t raw)
    {
        const uint32_t value = ByteSwap(raw);
        const bool signalling = (value & 0x7F800000u) == 0x7F800000u && (value & 0x003FFFFFu) && !(value & 0x00400000u);
        return signalling ? ByteSwap(value | 0x00400000u) : raw;
    }

    // The 3x4 words of bone i of a chunk, as the recompiled loop stores them.
    void BoneWords(uint8_t* base, uint32_t matrices, uint32_t boneMap, int32_t i, uint32_t* words)
    {
        const uint32_t source = matrices + (uint32_t(PPC_LOAD_U16(boneMap + uint32_t(i) * 2)) << 6);
        uint32_t matrix[16];
        std::memcpy(matrix, base + source, sizeof(matrix));
        for (int j = 0; j < 12; ++j) words[j] = ThroughDouble(matrix[kTranspose[j]]);
    }

    void Store(uint8_t* base, uint32_t data, const std::vector<Fill>& fills)
    {
        const uint32_t matrices = PPC_LOAD_U32(data + 4);
        for (const Fill& fill : fills)
        {
            PPC_STORE_U32(fill.array + 4, 0);
            if (fill.count > 0)
            {
                const uint32_t destination = PPC_LOAD_U32(fill.array);
                const uint32_t boneMap = PPC_LOAD_U32(fill.chunk + 28);
                // The matrices are scattered by the bone map: fetching four bones ahead hides most of each miss
                // (half of this function's time was the first load of a matrix).
                constexpr int32_t kAhead = 4;
                for (int32_t i = 0; i < fill.count; ++i)
                {
                    if (i + kAhead < fill.count)
                    {
                        const uint8_t* next = base + matrices + uint32_t(PPC_LOAD_U16(boneMap + uint32_t(i + kAhead) * 2)) * 64;
                        __builtin_prefetch(next);
                        __builtin_prefetch(next + 32);
                    }
                    BoneWords(base, matrices, boneMap, i, reinterpret_cast<uint32_t*>(base + destination + uint32_t(i) * 48));
                }
                PPC_STORE_U32(fill.array + 4, uint32_t(fill.count));
            }
            PPC_STORE_U32(fill.array + 12, InfluenceField(base, fill.chunk));
        }
    }

    // The old data's virtual delete (vtable slot 0, r4 = 1), from a frame like the guest's.
    void DeleteOld(PPCContext& ctx, uint8_t* base, uint32_t old)
    {
        const uint32_t stack = ctx.r1.u32;
        PPC_STORE_U32(stack - kFrameSize, stack);
        ctx.r1.u64 = stack - kFrameSize;
        const uint32_t function = PPC_LOAD_U32(PPC_LOAD_U32(old));
        ctx.r3.u64 = old;
        ctx.r4.u64 = 1;
        ctx.r11.u64 = function;
        ctx.lr = kDeleteReturn;
        PPC_CALL_INDIRECT_FUNC(function);
        ctx.r1.u64 = stack;
    }

    void Check(PPCContext& ctx, uint8_t* base, uint32_t self, const std::vector<Fill>& fills)
    {
        static uint32_t calls, differences;
        const uint32_t data = ctx.r4.u32;
        const uint32_t matrices = PPC_LOAD_U32(data + 4);
        // Expected words, Num and influence field of every array, before the recompiled code writes them.
        std::vector<uint32_t> expected;
        for (const Fill& fill : fills)
        {
            const uint32_t boneMap = PPC_LOAD_U32(fill.chunk + 28);
            for (int32_t i = 0; i < fill.count; ++i)
            {
                uint32_t words[12];
                BoneWords(base, matrices, boneMap, i, words);
                expected.insert(expected.end(), words, words + 12);
            }
            expected.push_back(fill.count > 0 ? uint32_t(fill.count) : 0);
            expected.push_back(InfluenceField(base, fill.chunk));
        }
        __imp__sub_823B7830(ctx, base);
        ++calls;
        size_t at = 0;
        bool same = true;
        std::string first;
        for (size_t f = 0; f < fills.size(); ++f)
        {
            const Fill& fill = fills[f];
            const uint32_t destination = PPC_LOAD_U32(fill.array);
            for (int32_t i = 0; i < fill.count; ++i)
                for (int j = 0; j < 12; ++j, ++at)
                {
                    uint32_t got;
                    std::memcpy(&got, base + destination + uint32_t(i) * 48 + j * 4, 4);
                    if (got != expected[at] && same)
                    {
                        same = false;
                        first = fmt::format("array {} bone {} word {} {:08X}/{:08X}", f, i, j, ByteSwap(expected[at]), ByteSwap(got));
                    }
                }
            if ((PPC_LOAD_U32(fill.array + 4) != expected[at] || PPC_LOAD_U32(fill.array + 12) != expected[at + 1]) && same)
            {
                same = false;
                first = fmt::format("array {} num {}/{} field {}/{}", f, expected[at], PPC_LOAD_U32(fill.array + 4), expected[at + 1],
                    PPC_LOAD_U32(fill.array + 12));
            }
            at += 2;
        }
        if (!same && ++differences <= 20)
            LOG_ERROR("[bone upload] sub_823B7830 differs: object {:08X} data {:08X} arrays {}: {}", self, data, fills.size(), first);
        if ((calls & 0xFFFF) == 0)
            LOG_ERROR("[bone upload] sub_823B7830 checked {} calls, {} differ", calls, differences);
    }
}

PPC_FUNC(sub_823B7830)
{
    const int mode = NativeBoneUploadMode();
    static thread_local std::vector<Fill> fills;
    const uint32_t self = ctx.r3.u32, data = ctx.r4.u32;
    if (mode == 0 || !Plan(base, self, data, fills))
    {
        __imp__sub_823B7830(ctx, base);
        return;
    }
    if (mode == 2)
    {
        Check(ctx, base, self, fills);
        return;
    }
    if (const uint32_t old = PPC_LOAD_U32(self + 56))
        DeleteOld(ctx, base, old);
    PPC_STORE_U32(self + 56, data);
    Store(base, data, fills);
    PPC_STORE_U32(self + 40, 0xFFFFFFFFu);
}
