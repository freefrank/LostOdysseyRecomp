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
const bool diagnosticsEnabled = std::getenv("LO_NATIVE_FRONTEND_STATS") != nullptr;
namespace {
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
Reject Submit(PPCContext& ctx, uint8_t* base, bool indexed)
{
    const uint32_t device = ctx.r3.u32;
    if (!base || !Range(device, 0x33A4) || Load(base, 0x83302A38) != device)
        return Reject::Device;
    if ((base[device + 0x2ABC] & 0x81) || Load(base, device + 0x33A0))
        return Reject::Recorded;
    const uint32_t count = indexed ? ctx.r7.u32 : ctx.r6.u32;
    if (count == 0 || count > 65535) return Reject::Count;
    const DirtyState dirty{Load64(base, device), Load64(base, device + 8),
        Load64(base, device + 16), Load64(base, device + 24),
        Load64(base, device + 32), Load64(base, device + 40)};
    if (const auto reason = QualifyDirty(dirty); reason != Reject::None) return reason;
    MeshDraw draw;
    if (indexed) {
        const uint32_t resource = Load(base, device + 12428);
        if (!Range(resource, 28)) return Reject::IndexResource;
        draw = IndexedDraw(ctx.r4.u32, count, ctx.r6.u32,
            Load(base, resource), Load(base, resource + 24));
    } else {
        draw = AutoDraw(ctx.r4.u32, ctx.r5.u32, count);
    }
    PreparedMesh prepared;
    if (!Prepare(draw, dirty, prepared)) return Reject::Count;
    const auto cursor = Load(base, device + 48);
    const auto limit = std::min(Load(base, device + 52), Load(base, device + 56));
    if (!native_command::CanAppend(cursor, limit,
        std::max(prepared.words, prepared.legacyWordsBound))) return Reject::Capacity;
    std::array<uint32_t, kMaxWords> words;
    // Only this final accepted path allocates a revision. It identifies a
    // producer delta, not a guest allocation, GPU resource or cache-validity key.
    const auto revision = producerRevision.fetch_add(1, std::memory_order_relaxed) + 1;
    if (!Encode(prepared, revision, [&](uint32_t offset) {
        return Load(base, device + offset);
    }, words)) std::abort(); // Internal schema disagreement; never replay a draw.
    // All payload values are initialized before the normal SDK cursor publish.
    // The ordinary live stream retains its original later ring/IB submission.
    std::memcpy(base + cursor + 4, words.data(), prepared.words * 4);
    const uint64_t acknowledged = 0;
    for (uint32_t offset = 0; offset < 40; offset += 8)
        std::memcpy(base + device + offset, &acknowledged, sizeof(acknowledged));
    Store(base, device + 48, cursor + prepared.words * 4);
    // These SDK entries are void. LR, SP and nonvolatile registers are unchanged;
    // no temporary UP fields or resource setter bookkeeping are skipped.
    return Reject::None;
}
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
    if (!gpu::native_frontend::TrySubmit(ctx, base, true))
        __imp__sub_823C6860(ctx, base);
}
// Ordinary DrawVertices: device, primitive, firstVertex, count.
PPC_FUNC(sub_827B56B0)
{
    if (!gpu::native_frontend::TrySubmit(ctx, base, false))
        __imp__sub_827B56B0(ctx, base);
}
