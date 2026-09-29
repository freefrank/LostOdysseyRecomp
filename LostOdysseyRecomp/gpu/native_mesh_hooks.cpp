#include <stdafx.h>
#include "gpu/native_mesh.h"
#include <atomic>
#include <cstdlib>

extern "C" PPC_FUNC(__imp__sub_823C6860);
extern "C" PPC_FUNC(__imp__sub_827B56B0);

namespace gpu::native_frontend
{
const bool meshEnabled = [] {
    const char* value = std::getenv("LO_NATIVE_FRONTEND");
    return value && std::string_view(value) == "mesh";
}();
// A separate process-level opt-in preserves the first mesh experiment as an
// A/B control. The SDK still owns shader/derived preparation on this path.
const bool preparedTailEnabled = [] {
    const char* value = std::getenv("LO_NATIVE_FRONTEND_PREPARED");
    return meshEnabled && value && std::string_view(value) == "1";
}();
const bool diagnosticsEnabled = std::getenv("LO_NATIVE_FRONTEND_STATS") != nullptr;
namespace {
struct PreparedCall {
    uint8_t* base;
    uint32_t device, stack;
    bool indexed;
};
thread_local const PreparedCall* preparedCall = nullptr;
struct PreparedScope {
    const PreparedCall* previous = preparedCall;
    explicit PreparedScope(const PreparedCall& call) { preparedCall = &call; }
    ~PreparedScope() { preparedCall = previous; }
};
std::atomic<uint64_t> producerRevision{0};
std::array<std::atomic<uint64_t>, uint32_t(Reject::CountReasons)> outcomes{};
uint32_t Load(const uint8_t* base, uint32_t address)
{
    uint32_t value;
    std::memcpy(&value, base + address, 4);
    return native_command::GuestWord(value);
}
uint64_t Load64(const uint8_t* base, uint32_t address)
{
    return (uint64_t(Load(base, address)) << 32) | Load(base, address + 4);
}
void Store(uint8_t* base, uint32_t address, uint32_t value)
{
    const auto guest = native_command::GuestWord(value);
    std::memcpy(base + address, &guest, 4);
}
bool Range(uint32_t address, uint32_t bytes)
{
    return address >= 0x10000 && uint64_t(address) + bytes <= 0x100000000ull;
}
Reject DeviceReason(const uint8_t* base, uint32_t device)
{
    if (!base || !Range(device, 0x33A4) || Load(base, 0x83302A38) != device)
        return Reject::Device;
    if ((base[device + 0x2ABC] & 0x81) || Load(base, device + 0x33A0))
        return Reject::Recorded;
    return Reject::None;
}
constexpr uint32_t kPreparedWordCapacity = [] {
    uint32_t words = kHeaderWords;
    for (uint32_t i = 2; i < kGroupCount; ++i)
        words += MaskWords(kGroups[i].fields) + kGroups[i].fields * kGroups[i].wordsPerField;
    return words + 1; // optional stream-control value
}();
static_assert(kPreparedWordCapacity < 1024);
template<bool Tail>
Reject Emit(uint8_t* base, uint32_t device, bool indexed, uint32_t primitive,
    uint32_t first, uint32_t count, const DirtyState& dirty)
{
    if (count == 0 || count > 65535) return Reject::Count;
    if (const auto reason = QualifyDirty(dirty); reason != Reject::None) return reason;
    MeshDraw draw;
    if (indexed) {
        const uint32_t resource = Load(base, device + 12428);
        if (!Range(resource, 28)) return Reject::IndexResource;
        draw = IndexedDraw(primitive, count, first,
            Load(base, resource), Load(base, resource + 24));
    } else {
        draw = AutoDraw(primitive, first, count);
    }
    PreparedMesh prepared;
    if (!Prepare(draw, dirty, prepared)) return Reject::Count;
    const auto cursor = Load(base, device + 48);
    const auto limit = std::min(Load(base, device + 52), Load(base, device + 56));
    if (!native_command::CanAppend(cursor, limit,
        std::max(prepared.words, prepared.legacyWordsBound))) return Reject::Capacity;
    // The prepared tail has no ALU banks: avoid a needless 10-KiB stack
    // allocation/probe for its at-most-397-word state/draw record.
    std::array<uint32_t, Tail ? kPreparedWordCapacity : kMaxWords> words;
    if (prepared.words > words.size()) return Reject::Count;
    // Only this final accepted path allocates a revision. It identifies a
    // producer delta, not a guest allocation, GPU resource or cache-validity key.
    const auto revision = producerRevision.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!Encode(prepared, revision, [&](uint32_t offset) {
        return Load(base, device + offset);
    }, words)) std::abort(); // Internal schema disagreement; never replay a draw.
    if constexpr (Tail) words[0] = native_command::GuestWord(kPreparedMesh);
    // All payload values are initialized before the normal SDK cursor publish.
    // The ordinary live stream retains its original later ring/IB submission.
    std::memcpy(base + cursor + 4, words.data(), prepared.words * 4);
    const uint64_t acknowledged = 0;
    // At the post-preparation boundary the original prefix already handled
    // its initial ALU banks. A preparation helper may have dirtied them again
    // for a future call; the original tail does not acknowledge those banks.
    for (uint32_t offset = Tail ? 16u : 0u; offset < 40; offset += 8)
        std::memcpy(base + device + offset, &acknowledged, sizeof(acknowledged));
    Store(base, device + 48, cursor + prepared.words * 4);
    // These SDK entries are void. LR, SP and nonvolatile registers are unchanged;
    // no temporary UP fields or resource setter bookkeeping are skipped.
    return Reject::None;
}
Reject Submit(PPCContext& ctx, uint8_t* base, bool indexed)
{
    const uint32_t device = ctx.r3.u32;
    if (const auto reason = DeviceReason(base, device); reason != Reject::None) return reason;
    const DirtyState dirty{Load64(base, device), Load64(base, device + 8),
        Load64(base, device + 16), Load64(base, device + 24),
        Load64(base, device + 32), Load64(base, device + 40)};
    return Emit<false>(base, device, indexed, ctx.r4.u32,
        indexed ? ctx.r6.u32 : ctx.r5.u32, indexed ? ctx.r7.u32 : ctx.r6.u32, dirty);
}
}
// This scope authorizes only the matching invocation/frame, never an arbitrary
// caller of a generated implementation or a nested SDK entry on the same device.
void ExecuteOriginalWithPreparedTail(PPCContext& ctx, uint8_t* base, bool indexed)
{
    const auto original = indexed ? __imp__sub_823C6860 : __imp__sub_827B56B0;
    if (!preparedTailEnabled) { original(ctx, base); return; }
    const PreparedCall call{base, ctx.r3.u32, ctx.r1.u32, indexed};
    PreparedScope scope(call);
    original(ctx, base);
}
bool SubmitPrepared(uint32_t stack, uint32_t device, bool indexed,
    uint32_t primitive, uint32_t first, uint32_t count,
    uint64_t main, uint64_t fetchRaster, uint64_t misc)
{
    if (!preparedTailEnabled || !preparedCall || preparedCall->indexed != indexed ||
        preparedCall->device != device ||
        stack != preparedCall->stack - (indexed ? 240u : 208u)) return false;
    auto* base = preparedCall->base;
    if (DeviceReason(base, device) != Reject::None) return false;
    // Both helpers (when needed) already ran in the canonical SDK prefix.
    // Use their LIVE returned mask, not the old device+16 dirty word. Still
    // reject an unconsumed shader mask rather than deleting its guard.
    const DirtyState dirty{0, 0, main, fetchRaster, misc, 0};
    return Emit<true>(base, device, indexed, primitive, first, count, dirty) == Reject::None;
}
void ProducerOutcomeSnapshot(std::span<uint64_t> destination)
{
    for (size_t i = 0; i < std::min(destination.size(), outcomes.size()); ++i)
        destination[i] = outcomes[i].load(std::memory_order_relaxed);
}
bool TrySubmit(PPCContext& ctx, uint8_t* base, bool indexed)
{
    if (!meshEnabled) return false;
    const auto reason = Submit(ctx, base, indexed);
    if (diagnosticsEnabled) outcomes[uint32_t(reason)].fetch_add(1, std::memory_order_relaxed);
    return reason == Reject::None;
}
}

// Ordinary DrawIndexedVertices: device, primitive, unused-r5, startIndex, count.
PPC_FUNC(sub_823C6860)
{
    if (gpu::native_frontend::TrySubmit(ctx, base, true)) return;
    if (gpu::native_frontend::preparedTailEnabled)
        gpu::native_frontend::ExecuteOriginalWithPreparedTail(ctx, base, true);
    else
        __imp__sub_823C6860(ctx, base);
}
// Ordinary DrawVertices: device, primitive, firstVertex, count.
PPC_FUNC(sub_827B56B0)
{
    if (gpu::native_frontend::TrySubmit(ctx, base, false)) return;
    if (gpu::native_frontend::preparedTailEnabled)
        gpu::native_frontend::ExecuteOriginalWithPreparedTail(ctx, base, false);
    else
        __imp__sub_827B56B0(ctx, base);
}

// Configured immediately after optional shader and derived-state preparation,
// before scalar/fetch/stream dirty writers. A successful tail handoff branches
// to the ORIGINAL SDK epilogue, preserving its saved LR/SP/nonvolatile state.
bool NativePreparedIndexed(PPCRegister& stack, PPCRegister& device,
    PPCRegister& primitive, PPCRegister& first, PPCRegister& count,
    PPCRegister& main, PPCRegister& fetchRaster, PPCRegister& misc)
{
    return gpu::native_frontend::SubmitPrepared(stack.u32, device.u32, true,
        primitive.u32, first.u32, count.u32, main.u64, fetchRaster.u64, misc.u64);
}
bool NativePreparedAuto(PPCRegister& stack, PPCRegister& device,
    PPCRegister& primitive, PPCRegister& first, PPCRegister& count,
    PPCRegister& main, PPCRegister& fetchRaster, PPCRegister& misc)
{
    return gpu::native_frontend::SubmitPrepared(stack.u32, device.u32, false,
        primitive.u32, first.u32, count.u32, main.u64, fetchRaster.u64, misc.u64);
}
