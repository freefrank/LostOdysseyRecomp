#include <stdafx.h>
#include "temporal_inputs.h"
#include <cstdlib>
#include <charconv>

extern std::atomic<uint32_t> g_presentedSwaps;
extern "C" PPC_FUNC(__imp__sub_8274C478);
extern "C" PPC_FUNC(__imp__sub_8274CCB8);
extern "C" PPC_FUNC(__imp__sub_82301D28);

namespace temporal_inputs
{
bool Enabled()
{
    static const bool enabled = [] {
        const char* path = std::getenv("LO_TEMPORAL_PROBE_FILE");
        return path && *path;
    }();
    return enabled;
}
namespace
{
// Each path has its own allowance so a busy view path cannot exhaust primitive
// evidence. This records only the first 128 calls to each path in this process.
constexpr uint32_t Limit = 128;
std::mutex probeMutex;
uint32_t counts[3]{};

uint32_t StartSwap()
{
    static const uint32_t threshold = [] {
        const char* text = std::getenv("LO_TEMPORAL_PROBE_START_SWAP");
        uint32_t value = 0;
        if (!text || !*text) return value;
        const char* end = text + std::strlen(text);
        const auto parsed = std::from_chars(text, end, value);
        return parsed.ec == std::errc{} && parsed.ptr == end ? value : 0u;
    }();
    return threshold;
}

bool Read(uint8_t* base, uint32_t object, uint32_t offset, void* output, size_t size)
{
    if (!base || object < 0x10000 || (object & 3) ||
        uint64_t(object) + offset + size > 0x100000000ull) return false;
#if defined(_WIN32)
    // Unlike a range-only pointer test, this tolerates unmapped/guarded guest
    // pages without dereferencing them. It also checks the whole matrix span.
    SIZE_T copied = 0;
    return ReadProcessMemory(GetCurrentProcess(), base + uint64_t(object) + offset,
        output, size, &copied) && copied == size;
#else
    // No unguarded fallback for diagnostic reads on unsupported hosts.
    return false;
#endif
}

uint32_t Word(const uint8_t* bytes)
{
    return (uint32_t(bytes[0]) << 24) | (uint32_t(bytes[1]) << 16) |
        (uint32_t(bytes[2]) << 8) | bytes[3];
}

bool Pointer(uint8_t* base, uint32_t object, uint32_t offset, uint32_t& value)
{
    uint8_t bytes[4]{};
    if (!Read(base, object, offset, bytes, sizeof(bytes))) return false;
    value = Word(bytes);
    return true;
}

void Matrix(FILE* file, uint8_t* base, const char* name, uint32_t object, uint32_t offset)
{
    uint8_t bytes[64]{};
    const bool valid = Read(base, object, offset, bytes, sizeof(bytes));
    std::fprintf(file, " %s_addr=0x%llx %s_valid=%u", name,
        static_cast<unsigned long long>(uint64_t(object) + offset), name, unsigned(valid));
    if (!valid) return;
    std::fprintf(file, " %s_bits=", name);
    for (uint32_t i = 0; i < 16; ++i)
        std::fprintf(file, "%s%08x", i ? "," : "", Word(bytes + i * 4));
}

void Descriptor(FILE* file, uint8_t* base, const char* name, uint32_t shader, uint32_t offset)
{
    uint8_t bytes[4]{};
    const bool valid = Read(base, shader, offset, bytes, sizeof(bytes));
    std::fprintf(file, " %s_descriptor_valid=%u", name, unsigned(valid));
    if (valid)
        std::fprintf(file, " %s_index=%u %s_bound=%u %s_upload_float4_count=4", name,
            (unsigned(bytes[0]) << 8) | bytes[1], name,
            (unsigned(bytes[2]) << 8) | bytes[3], name);
}

void Record(const PPCContext& ctx, uint8_t* base, unsigned kind, uint32_t constructedView = 0)
{
    if (!Enabled()) return;
    // This is only an observation gate for reaching a useful scene. It does
    // not identify the guest frame associated with these CPU-side matrices.
    const uint32_t observedSwap = g_presentedSwaps.load(std::memory_order_relaxed);
    if (observedSwap < StartSwap()) return;
    std::lock_guard lock(probeMutex);
    if (counts[kind] >= Limit) return;
    static FILE* file = [] {
        // Append protects previous evidence; the session header distinguishes runs.
        FILE* opened = std::fopen(std::getenv("LO_TEMPORAL_PROBE_FILE"), "ab");
        if (opened)
        {
            std::fprintf(opened, "\ntemporal_probe_v2 session_clock=%lld max_calls_per_path=128 start_swap=%u bits=big_endian_u32_memory_order same_gpu_frame_proven=0 descriptor_uploads=predicted_not_observed\n",
                static_cast<long long>(std::chrono::steady_clock::now().time_since_epoch().count()), StartSwap());
            std::fflush(opened);
        }
        return opened;
    }();
    ++counts[kind];
    if (!file) return;
    std::fprintf(file, "path=%s phase=%s sample=%u cpu_presented_swaps=%u lr=%08x r3=%08x r4=%08x r5=%08x r6=%08x r7=%08x r8=%08x",
        kind == 0 ? "8274C478" : kind == 1 ? "8274CCB8" : "82301D28",
        kind == 2 ? "post_view_constructor" : "cpu_function_entry", counts[kind],
        observedSwap, uint32_t(ctx.lr),
        ctx.r3.u32, ctx.r4.u32, ctx.r5.u32, ctx.r6.u32, ctx.r7.u32, ctx.r8.u32);
    if (kind == 0)
    {
        const uint32_t shader = ctx.r3.u32, view = ctx.r7.u32;
        uint32_t state = 0;
        const bool stateValid = Pointer(base, view, 4, state);
        std::fprintf(file, " shader=%08x view=%08x state_pointer_valid=%u state=%08x", shader, view, unsigned(stateValid), state);
        Descriptor(file, base, "current_vp", shader, 0x68);
        Descriptor(file, base, "previous_vp", shader, 0x6C);
        Matrix(file, base, "current_vp", view, 0x100);
        Matrix(file, base, "previous_vp", view, 0x660);
        // These source matrices are deliberately named by offset: their exact
        // view/projection convention has not been established by runtime data.
        Matrix(file, base, "view_40", view, 0x40);
        Matrix(file, base, "view_80", view, 0x80);
        if (stateValid)
        {
            Matrix(file, base, "state_d0", state, 0xD0);
            Matrix(file, base, "state_150", state, 0x150);
        }
    }
    else if (kind == 1)
    {
        const uint32_t policy = ctx.r3.u32, primitive = ctx.r6.u32;
        uint32_t shader = 0;
        const bool shaderValid = Pointer(base, policy, 0xC, shader);
        std::fprintf(file, " policy=%08x primitive=%08x shader_pointer_valid=%u shader=%08x", policy, primitive, unsigned(shaderValid), shader);
        if (shaderValid) Descriptor(file, base, "previous_local_to_world", shader, 0x70);
        Matrix(file, base, "current_local_to_world", primitive, 0x20);
        Matrix(file, base, "previous_local_to_world", primitive, 0xA0);
    }
    else
    {
        // saved r3 is the constructor destination; live ctx contains its return
        // state. Do not inspect unverified caller stack arguments or old history.
        uint32_t family = 0, state = 0;
        const bool familyValid = Pointer(base, constructedView, 0, family);
        const bool stateValid = Pointer(base, constructedView, 4, state);
        std::fprintf(file, " view=%08x family_pointer_valid=%u family=%08x state_pointer_valid=%u state=%08x previous_sampled=0",
            constructedView, unsigned(familyValid), family, unsigned(stateValid), state);
        Matrix(file, base, "current_vp", constructedView, 0x100);
        Matrix(file, base, "view_40", constructedView, 0x40);
        Matrix(file, base, "view_80", constructedView, 0x80);
    }
    std::fprintf(file, "\n");
    std::fflush(file);
}
}
}

PPC_FUNC(sub_8274C478)
{
    // Entry values precede a guest virtual call, so they predict the later
    // uploads at 8274C4FC/8274C538; they do not observe their completion.
    try { temporal_inputs::Record(ctx, base, 0); } catch (...) {}
    __imp__sub_8274C478(ctx, base);
}

PPC_FUNC(sub_8274CCB8)
{
    try { temporal_inputs::Record(ctx, base, 1); } catch (...) {}
    __imp__sub_8274CCB8(ctx, base);
}

PPC_FUNC(sub_82301D28)
{
    const uint32_t view = ctx.r3.u32;
    __imp__sub_82301D28(ctx, base);
    try { temporal_inputs::Record(ctx, base, 2, view); } catch (...) {}
}
