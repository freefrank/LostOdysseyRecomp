#include <gpu/controller_atlas.h>
#include <hid/controller_atlas_glyphs.h>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <vector>
namespace controller_atlas = gpu::controller_atlas;

namespace hid {
inline bool family = false;
inline uint32_t reads = 0;
bool UsesPlayStationPrompts() { ++reads; return family; }
}
namespace video {
inline bool stopped = false;
bool GpuWorkStopped() { return stopped; }
}
inline uint32_t destroyedTextures = 0;
inline uint32_t recordedAtlasLogs = 0;
enum class RenderFormat { UNKNOWN, BC3_UNORM, R8G8B8A8_UNORM };
enum class RenderTextureLayout { UNKNOWN, COPY_DEST, SHADER_READ };
enum class RenderBarrierStage { COPY, GRAPHICS };
struct RenderTexture { uint32_t id; explicit RenderTexture(uint32_t id) : id(id) {} ~RenderTexture() { ++destroyedTextures; } };
struct RenderTextureDesc {
    static RenderTextureDesc Texture2D(uint32_t width, uint32_t height, uint32_t mips, RenderFormat format) {
        assert(width == 256 && height == 128 && mips == 1 && format == RenderFormat::R8G8B8A8_UNORM);
        return {};
    }
};
struct Device {
    uint32_t creates = 0;
    bool fail = false;
    std::unique_ptr<RenderTexture> createTexture(RenderTextureDesc) {
        ++creates;
        return fail ? nullptr : std::make_unique<RenderTexture>(creates);
    }
};
struct Footprint { void* ring = nullptr; uint32_t pitch = 0; uint64_t offset = 0; };
struct RenderTextureCopyLocation {
    static Footprint Subresource(RenderTexture*) { return {}; }
    static Footprint PlacedFootprint(void* ring, RenderFormat format, uint32_t width, uint32_t height,
        uint32_t depth, uint32_t pitch, uint64_t offset) {
        assert(format == RenderFormat::R8G8B8A8_UNORM && width == 256 && height == 128 && depth == 1);
        return {ring, pitch, offset};
    }
};
struct CommandList {
    uint32_t copies = 0;
    Footprint last;
    void copyTextureRegion(Footprint, Footprint source) { ++copies; last = source; }
};
namespace binding { struct Texture { RenderFormat format = RenderFormat::UNKNOWN; bool ps = false; };
    enum class TextureKind { GuestUpload }; }
struct HostTexture {
    enum class AtlasState : uint8_t { None, Pending, Ready, Failed };
    AtlasState atlasState = AtlasState::None;
    std::vector<uint8_t> atlasPendingRgba;
    std::unique_ptr<HostTexture> atlasPlayStation;
    bool controllerAtlasRecognized = false;
    bool controllerAtlasPlayStationVariant = false;
    controller_atlas::Identity controllerAtlasIdentity = controller_atlas::Identity::Unknown;
    RenderFormat format = RenderFormat::BC3_UNORM;
    uint32_t width = 256, height = 128, bindingWidth = 256, bindingHeight = 128;
    std::unique_ptr<RenderTexture> texture = std::make_unique<RenderTexture>(0);
};
#define LOG_WARNING(...) do {} while (0)
#define LOG_INFO(...) do { ++recordedAtlasLogs; } while (0)
struct TextureSetCache { using Key = std::array<RenderTexture*, 32>; };
struct GuestKey { uint32_t address, width, height;
    bool operator<(const GuestKey& other) const { return address < other.address; }
};
struct GpuSlot { std::vector<std::unique_ptr<HostTexture>> retiredTextures; };
struct Harness {
    uint64_t frame = 0;
    uint32_t drawsThisFrame = 0, debugDraw = 0;
    std::string debugCaptureDir;
    uint64_t controllerAtlasTraceFrame = ~0ull;
    uint32_t controllerAtlasTraceCount = 0;
    struct ControllerAtlasTraceCandidate {
        RenderTexture* image = nullptr;
        uint32_t bank = 0, slot = 0, guestAddress = 0;
        bool playStation = false;
        controller_atlas::Identity identity = controller_atlas::Identity::Unknown;
        const char* stage = nullptr;
    };
    uint64_t controllerAtlasFamilyFrame = ~0ull;
    bool controllerAtlasPlayStationFamily = false;
    uint64_t uploadOffset = 0;
    bool failUpload = false, rollover = false;
    uint32_t uploads = 0;
    std::vector<uint8_t> lastUpload;
    void* uploadRing = reinterpret_cast<void*>(1);
    CommandList first, second;
    CommandList* commandList = &first;
    Device dev;
    Device* device = &dev;
    std::mutex framePlanMutex;
    struct { uint64_t geometryEpoch = 0; } committedPlan;
    std::set<uint64_t> failedPlanEpochs;
    GpuSlot gpu;
    std::map<GuestKey, std::unique_ptr<HostTexture>> textures;
    GpuSlot& Gpu() { return gpu; }
    uint64_t Upload(const void* data, size_t size, uint32_t alignment) {
        assert(data && size == size_t(256) * 128 * 4 && alignment == 512);
        ++uploads;
        if (failUpload || video::GpuWorkStopped()) return UINT64_MAX;
        lastUpload.assign(static_cast<const uint8_t*>(data), static_cast<const uint8_t*>(data) + size);
        if (rollover) { commandList = &second; uploadRing = reinterpret_cast<void*>(2); rollover = false; }
        return uploadOffset;
    }
    void Transition(HostTexture& tex, RenderTextureLayout, RenderBarrierStage) { assert(tex.texture); }
    void DescribeBindingTexture(binding::Texture* b, const HostTexture& tex, binding::TextureKind, uint64_t) {
        if (b) { b->format = tex.format; b->ps = tex.controllerAtlasPlayStationVariant; }
    }
/* PRODUCTION_SELECT */
/* PRODUCTION_TRACE */
/* PRODUCTION_PLAN */
/* PRODUCTION_INVALIDATE */
    bool RecycleAfterFence(bool signaled) {
        if (!signaled) return false;
        gpu.retiredTextures.clear();
        return true;
    }
};

int main() {
    Harness harness;
    auto source = std::make_unique<HostTexture>();
    auto* origin = source.get();
    origin->controllerAtlasRecognized = true;
    origin->controllerAtlasIdentity = controller_atlas::Identity::FontIconPage;
    origin->atlasState = HostTexture::AtlasState::Pending;
    origin->atlasPendingRgba.resize(size_t(256) * 128 * 4);
    for (size_t pixel = 0; pixel < size_t(256) * 128; ++pixel) {
        origin->atlasPendingRgba[pixel * 4] = 70;
        origin->atlasPendingRgba[pixel * 4 + 1] = 100;
        origin->atlasPendingRgba[pixel * 4 + 2] = 130;
        origin->atlasPendingRgba[pixel * 4 + 3] = uint8_t(1 + pixel % 255);
    }
    const auto originalRgba = origin->atlasPendingRgba;
    harness.textures.emplace(GuestKey{0x100000, 256, 128}, std::move(source));
    binding::Texture binding{};
    assert(harness.SelectControllerAtlas(origin, &binding, 0) == origin);
    assert(harness.SelectControllerAtlas(origin, &binding, 0) == origin);
    assert(hid::reads == 1 && harness.uploads == 0 && !binding.ps);
    hid::family = true; ++harness.frame;
    harness.rollover = true;
    auto* ps = harness.SelectControllerAtlas(origin, &binding, 0);
    assert(ps != origin && binding.ps && ps->texture && ps->texture->id == 1);
    assert(ps->controllerAtlasIdentity == origin->controllerAtlasIdentity);
    assert(harness.uploads == 1 && harness.dev.creates == 1);
    assert(harness.lastUpload.size() == originalRgba.size() && harness.lastUpload != originalRgba);
    auto expected = originalRgba;
    assert(hid::prompts::atlas::PatchPlayStationAtlasRgba(expected.data(), 256 * 4,
        expected.data(), 256 * 4, 256, 128));
    assert(harness.lastUpload == expected);
    const auto inside = [](hid::prompts::atlas::Rect rect, size_t x, size_t y) {
        return x >= size_t(rect.x) && x < size_t(rect.x + rect.width) &&
               y >= size_t(rect.y) && y < size_t(rect.y + rect.height);
    };
    unsigned systemAlphaChanges[2]{};
    for (size_t y = 0; y < 128; ++y)
        for (size_t x = 0; x < 256; ++x) {
            const size_t offset = (y * 256 + x) * 4;
            bool system = false, patchRegion = false;
            for (size_t i = 0; i < hid::prompts::atlas::SystemCells.size(); ++i)
                if (inside(hid::prompts::atlas::SystemCells[i].rect, x, y)) {
                    system = true;
                    systemAlphaChanges[i] += harness.lastUpload[offset + 3] != originalRgba[offset + 3];
                }
            for (auto cell : hid::prompts::atlas::FaceCells) patchRegion |= inside(cell.artwork, x, y);
            for (auto cell : hid::prompts::atlas::ShoulderCells) patchRegion |= inside(cell.artwork, x, y);
            if (!system) assert(harness.lastUpload[offset + 3] == originalRgba[offset + 3]);
            if (!system && !patchRegion)
                assert(std::equal(originalRgba.begin() + offset, originalRgba.begin() + offset + 4,
                    harness.lastUpload.begin() + offset));
        }
    for (unsigned count : systemAlphaChanges) assert(count > 50);
    for (auto cell : hid::prompts::atlas::SystemCells) {
        const size_t center = (size_t(cell.rect.y + 18) * 256 + cell.rect.x + 18) * 4;
        assert(harness.lastUpload[center + 3] == 255);
    }
    for (unsigned channel = 0; channel < 4; ++channel)
        assert(harness.lastUpload[(size_t(20) * 256 + 200) * 4 + channel] ==
            originalRgba[(size_t(20) * 256 + 200) * 4 + channel]);
    assert(harness.first.copies == 0 && harness.second.copies == 1);
    assert(harness.second.last.ring == reinterpret_cast<void*>(2) && harness.second.last.pitch == 256);
    assert(origin->atlasPendingRgba.empty());
    assert(harness.SelectControllerAtlas(origin, &binding, 0) == ps && harness.uploads == 1);
    hid::family = false; ++harness.frame;
    assert(harness.SelectControllerAtlas(origin, &binding, 0) == origin && !binding.ps);
    hid::family = true; ++harness.frame;
    assert(harness.SelectControllerAtlas(origin, &binding, 0) == ps && harness.uploads == 1);
    assert(hid::reads == 4);
    const auto destroyedBefore = destroyedTextures;
    harness.InvalidateRange(0x100020, 1);
    assert(harness.textures.empty() && harness.gpu.retiredTextures.size() == 1);
    assert(!harness.RecycleAfterFence(false) && destroyedTextures == destroyedBefore);
    assert(harness.RecycleAfterFence(true) && destroyedTextures == destroyedBefore + 2);
    HostTexture failed;
    failed.controllerAtlasRecognized = true;
    failed.atlasState = HostTexture::AtlasState::Pending;
    failed.atlasPendingRgba.resize(size_t(256) * 128 * 4);
    harness.failUpload = true;
    ++harness.frame;
    assert(harness.SelectControllerAtlas(&failed, &binding, 0) == &failed && !binding.ps);
    assert(failed.atlasState == HostTexture::AtlasState::Failed && failed.atlasPendingRgba.empty());
    const auto failedUploads = harness.uploads;
    assert(harness.SelectControllerAtlas(&failed, &binding, 0) == &failed && harness.uploads == failedUploads);
    HostTexture allocationFailed;
    allocationFailed.atlasState = HostTexture::AtlasState::Pending;
    allocationFailed.atlasPendingRgba.resize(size_t(256) * 128 * 4);
    harness.dev.fail = true;
    assert(harness.SelectControllerAtlas(&allocationFailed, &binding, 0) == &allocationFailed);
    assert(allocationFailed.atlasState == HostTexture::AtlasState::Failed && harness.uploads == failedUploads);
    HostTexture patchFailed;
    patchFailed.atlasState = HostTexture::AtlasState::Pending;
    harness.dev.fail = false;
    assert(harness.SelectControllerAtlas(&patchFailed, &binding, 0) == &patchFailed);
    assert(patchFailed.atlasState == HostTexture::AtlasState::Failed && harness.uploads == failedUploads);
    HostTexture stopped;
    stopped.atlasState = HostTexture::AtlasState::Pending;
    stopped.atlasPendingRgba.resize(size_t(256) * 128 * 4);
    harness.failUpload = true;
    video::stopped = true;
    ++harness.frame;
    RenderTexture* originalAtStop = stopped.texture.get();
    const auto drawsAtStop = harness.drawsThisFrame;
    HostTexture* stoppedResult = harness.SelectControllerAtlas(&stopped, &binding, 0);
    // Mirrors the production GetTexture caller's if (!tex) / PlanSuppressed
    // branch, whose exact source ordering is checked by the generator.
    if (stoppedResult || !harness.PlanSuppressed()) ++harness.drawsThisFrame;
    assert(stoppedResult == nullptr && harness.drawsThisFrame == drawsAtStop);
    assert(stopped.texture.get() == originalAtStop); // source remains owned
    video::stopped = false;
    harness.failUpload = false;
    assert(harness.SelectControllerAtlas(&stopped, &binding, 0) == &stopped);

    // Extracted production trace method: an aborted draw never calls it;
    // superseded bindings cannot be reported as recorded.
    RenderTexture image1{11}, image2{12};
    TextureSetCache::Key bindings[3]{};
    bindings[0][5] = &image1;
    bindings[0][9] = &image2;
    const std::vector<Harness::ControllerAtlasTraceCandidate> candidates{
        {&image1, 0, 5, 0x1000, true, controller_atlas::Identity::FontIconPage, "ps"},
        {&image1, 0, 9, 0x2000, false, controller_atlas::Identity::IconPage0, "vs"}, // overwritten by image2
        {&image2, 0, 9, 0x3000, false, controller_atlas::Identity::IconPage0, "vs"}};
    assert(recordedAtlasLogs == 0);
    ++harness.debugDraw;
    harness.debugCaptureDir = "F1";
    // Both drawIndexedInstanced and drawInstanced lead here in production.
    ++harness.drawsThisFrame;
    harness.TraceControllerAtlasRecorded(candidates, bindings, 1, 2);
    assert(recordedAtlasLogs == 2 && harness.controllerAtlasTraceCount == 2);
    for (unsigned i = 0; i < 40; ++i)
        harness.TraceControllerAtlasRecorded(candidates, bindings, 1, 2);
    assert(recordedAtlasLogs == 64 && harness.controllerAtlasTraceCount == 64);
    ++harness.frame;
    harness.TraceControllerAtlasRecorded(candidates, bindings, 1, 2);
    assert(recordedAtlasLogs == 66 && harness.controllerAtlasTraceCount == 2);
    std::cout << "production select/trace/invalidate: stopped GPU suppresses draw, only final recorded bindings log (all slots, capped); prior patch/family/rollover/fence cases passed\n";
}
