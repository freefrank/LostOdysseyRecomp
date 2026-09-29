#pragma once
#include "native_command_stream.h"
#include <array>
#include <bit>
#include <cstdint>
#include <span>

namespace gpu::native_frontend
{
// One value-owned semantic state delta and draw at the original execution
// position. No pointers, transient handles, or first-consumption retirement:
// the command is safe to replay with the caller's unchanged state inherited.
constexpr uint32_t kMesh = 0x80004C4D;
constexpr uint32_t kVersion = 1;
constexpr uint32_t kHeaderWords = 9;
constexpr uint32_t kMaxWords = 2500;
extern const bool meshEnabled;
extern const bool diagnosticsEnabled;
void ProducerOutcomeSnapshot(std::span<uint64_t> destination);

enum class Group : uint32_t
{
    VertexConstants, PixelConstants, Pipeline, ProgramControl, Targets,
    Raster, Fetch, PointRaster, DrawControl, PolygonOffset, BoolLoop, Count
};
constexpr uint32_t kGroupCount = uint32_t(Group::Count);
struct GroupLayout
{
    uint32_t guestOffset, registerFirst, fields, wordsPerField;
};
// SDK dirty masks name these semantic groups, not arbitrary register runs.
// Compatibility register locations are confined to the two front-end adapters.
constexpr std::array<GroupLayout, kGroupCount> kGroups{{
    {1920, 0x4000, 64, 16}, {6016, 0x4400, 64, 16},
    {10548, 0x2200, 12, 1}, {10528, 0x2180, 5, 1},
    {10368, 0x2000, 16, 1}, {10444, 0x2100, 21, 1},
    {1152, 0x4800, 32, 6}, {10596, 0x2280, 21, 1},
    {10680, 0x2300, 38, 1}, {10832, 0x2380, 8, 1},
    {10112, 0x4900, 1, 40}
}};
constexpr uint64_t HighMask(uint32_t bits)
{
    return bits == 64 ? ~uint64_t(0) : (~uint64_t(0) << (64 - bits));
}
constexpr uint32_t MaskWords(uint32_t fields)
{
    return fields == 1 ? 0 : fields <= 32 ? 1 : 2;
}
struct DirtyState
{
    uint64_t vertex = 0, pixel = 0, main = 0, fetchRaster = 0, misc = 0;
    uint64_t derived = 0;
};
struct MeshDraw
{
    uint32_t initiator = 0, dmaBase = 0, dmaSize = 0, baseVertex = 0;
    bool Indexed() const { return ((initiator >> 6) & 3) == 0; }
};
enum class Reject : uint32_t
{
    None, Device, Recorded, Count, ShaderPrepare, DerivedPrepare,
    StreamPrepare, IndexResource, Capacity, CountReasons
};
constexpr const char* RejectName(Reject reason)
{
    switch (reason) {
    case Reject::Device: return "device";
    case Reject::Recorded: return "recorded_or_special";
    case Reject::Count: return "count_split_or_zero";
    case Reject::ShaderPrepare: return "shader_prepare";
    case Reject::DerivedPrepare: return "derived_state_prepare";
    case Reject::StreamPrepare: return "stream_event_prepare";
    case Reject::IndexResource: return "index_resource";
    case Reject::Capacity: return "capacity_or_rollover";
    default: return "none";
    }
}
// The rejected paths have additional guest writes/events. Never partially
// clear their dirty masks and then replay the original call.
constexpr Reject QualifyDirty(const DirtyState& state)
{
    if (state.main & (uint64_t(15) << 17)) return Reject::ShaderPrepare;
    if (state.main & state.derived) return Reject::DerivedPrepare;
    if (state.misc & (uint64_t(63) << 49)) return Reject::StreamPrepare;
    return Reject::None;
}
constexpr std::array<uint64_t, kGroupCount> Masks(const DirtyState& d)
{
    return {d.vertex, d.pixel,
        (d.main << 52) & HighMask(12), (d.main << 47) & HighMask(5),
        (d.main << 6) & HighMask(16), (d.main << 22) & HighMask(21),
        d.fetchRaster << 32, (d.fetchRaster << 9) & HighMask(21),
        (d.misc << 26) & HighMask(38), (d.misc << 18) & HighMask(8),
        (d.misc & (uint64_t(1) << 56)) ? HighMask(1) : 0};
}
struct PreparedMesh
{
    MeshDraw draw;
    std::array<uint64_t, kGroupCount> masks{};
    uint32_t groups = 0, words = 0, legacyWordsBound = 0;
};
inline bool Prepare(const MeshDraw& draw, const DirtyState& dirty, PreparedMesh& result)
{
    if (QualifyDirty(dirty) != Reject::None || !(draw.initiator >> 16)) return false;
    const auto source = (draw.initiator >> 6) & 3;
    if (source != 0 && source != 2) return false;
    PreparedMesh staged;
    staged.draw = draw;
    staged.masks = Masks(dirty);
    staged.words = kHeaderWords;
    staged.legacyWordsBound = draw.Indexed() ? 7 : 5;
    for (uint32_t i = 0; i < kGroupCount; ++i) {
        if (!staged.masks[i]) continue;
        const auto& layout = kGroups[i];
        const auto fields = uint32_t(std::popcount(staged.masks[i]));
        staged.groups |= 1u << i;
        staged.words += MaskWords(layout.fields) + fields * layout.wordsPerField;
        // A safe upper bound for original headers and alignment. Requiring
        // both encodings to fit also excludes original SDK allocation effects.
        staged.legacyWordsBound += fields * (layout.wordsPerField + 4);
        if (i == uint32_t(Group::Fetch)) staged.legacyWordsBound += 4;
    }
    if (staged.words > kMaxWords) return false;
    result = staged;
    return true;
}
constexpr uint32_t IndexPhysicalAddress(uint32_t address)
{
    // Exact SDK alias conversion, including its E-region carry correction.
    return (address & 0x1fffffffu) + (((address >> 20) + 512u) & 0x1000u);
}
constexpr MeshDraw IndexedDraw(uint32_t primitive, uint32_t count,
    uint32_t startIndex, uint32_t resourceFlags, uint32_t resourceData)
{
    const bool index32 = (resourceFlags >> 31) != 0;
    const uint32_t address = resourceData + (startIndex << (index32 ? 2 : 1));
    return {(count << 16) | (primitive & 63) | (index32 ? 2048u : 0u),
        IndexPhysicalAddress(address),
        (count << (index32 ? 1 : 0)) | (std::rotl(resourceFlags, 1) & 0xc0000000u),
        0}; // sub_823C6860 writes zero to VGT_INDX_OFFSET; r5 is not consumed.
}
constexpr MeshDraw AutoDraw(uint32_t primitive, uint32_t firstVertex, uint32_t count)
{
    return {(count << 16) | (primitive & 63) | 128u, 0, 0, firstVertex};
}
// Caller preflights all address ranges and destination capacity first. Only
// owned CPU words are created here; no guest state or renderer side effects.
template<class ReadGuest>
bool Encode(const PreparedMesh& prepared, uint64_t producerRevision,
    ReadGuest&& read, std::span<uint32_t> output)
{
    if (output.size() < prepared.words || prepared.words < kHeaderWords) return false;
    output[0] = native_command::GuestWord(kMesh);
    output[1] = prepared.words;
    output[2] = (kVersion << 16) | prepared.groups;
    output[3] = prepared.draw.initiator;
    output[4] = prepared.draw.dmaBase;
    output[5] = prepared.draw.dmaSize;
    output[6] = prepared.draw.baseVertex;
    output[7] = uint32_t(producerRevision);
    output[8] = uint32_t(producerRevision >> 32);
    size_t at = kHeaderWords;
    for (uint32_t group = 0; group < kGroupCount; ++group) {
        const auto mask = prepared.masks[group];
        if (!mask) continue;
        const auto& layout = kGroups[group];
        const auto maskWords = MaskWords(layout.fields);
        if (maskWords) output[at++] = uint32_t(mask >> 32);
        if (maskWords == 2) output[at++] = uint32_t(mask);
        native_command::VisitRuns(mask, [&](uint32_t first, uint32_t count, uint32_t) {
            const uint32_t start = first * layout.wordsPerField;
            const uint32_t end = start + count * layout.wordsPerField;
            for (uint32_t word = start; word < end; ++word)
                output[at++] = read(layout.guestOffset + word * 4);
        });
    }
    return at == prepared.words;
}
struct DeltaView
{
    uint64_t mask = 0;
    std::span<const uint32_t> values;
};
struct DecodedMesh
{
    MeshDraw draw;
    uint64_t producerRevision = 0;
    std::array<DeltaView, kGroupCount> deltas{};
};
// Full validation precedes every observable write. The caller supplies the
// native payload after the guest-endian discriminator (starting at wordCount).
inline bool Decode(std::span<const uint32_t> body, DecodedMesh& result)
{
    if (body.size() < kHeaderWords - 1 || body.size() + 1 > kMaxWords ||
        body[0] != body.size() + 1 || (body[1] >> 16) != kVersion ||
        (body[1] & 0xffffu) >= (1u << kGroupCount)) return false;
    DecodedMesh staged;
    staged.draw = {body[2], body[3], body[4], body[5]};
    const auto source = (body[2] >> 6) & 3;
    if ((source != 0 && source != 2) || !(body[2] >> 16) ||
        (body[2] & 0x0000f740u)) return false;
    if (source == 2 && (body[3] || body[4] || (body[2] & 2048u))) return false;
    staged.producerRevision = uint64_t(body[6]) | (uint64_t(body[7]) << 32);
    size_t at = kHeaderWords - 1;
    for (uint32_t group = 0; group < kGroupCount; ++group) {
        if (!(body[1] & (1u << group))) continue;
        const auto& layout = kGroups[group];
        const auto maskWords = MaskWords(layout.fields);
        if (body.size() - at < maskWords) return false;
        uint64_t mask = maskWords ? uint64_t(body[at++]) << 32 : HighMask(1);
        if (maskWords == 2) mask |= body[at++];
        if (!mask || (mask & ~HighMask(layout.fields))) return false;
        const size_t count = size_t(std::popcount(mask)) * layout.wordsPerField;
        if (body.size() - at < count) return false;
        staged.deltas[group] = {mask, body.subspan(at, count)};
        at += count;
    }
    if (at != body.size()) return false;
    result = staged;
    return true;
}
// State deltas and the base-vertex write are unpredicated; the draw/DMA writes
// are predicated separately by the command processor, exactly like the SDK.
template<class ApplyValue>
void ApplyDeltas(const DecodedMesh& mesh, ApplyValue&& apply)
{
    for (uint32_t group = 0; group < kGroupCount; ++group) {
        const auto& delta = mesh.deltas[group];
        if (!delta.mask) continue;
        const auto& layout = kGroups[group];
        native_command::VisitRuns(delta.mask, [&](uint32_t first, uint32_t count, uint32_t packed) {
            for (uint32_t i = 0; i < count * layout.wordsPerField; ++i)
                apply(layout.registerFirst + first * layout.wordsPerField + i,
                    delta.values[packed * layout.wordsPerField + i]);
        });
        if (group == uint32_t(Group::Fetch)) {
            // No-rollover SDK tail: 00025000 00000000 00025000 00000000.
            apply(0x5000, 0); apply(0x5001, 0x25000); apply(0x5002, 0);
        }
    }
    apply(0x2102, mesh.draw.baseVertex);
}
}
