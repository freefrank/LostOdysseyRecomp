#pragma once

#include "fsr_alpha_propagation_policy.h"
#include "fsr_alpha_replay_gpu.h"
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <utility>
#include <vector>

#ifdef LO_GPU_PLUME
namespace gpu::fsr_alpha {

inline bool AuditedPostprocessPs(uint64_t hash) {
    return hash == 0x7c260eacff1d681dull || hash == 0x53dd5d081c7945cfull ||
        hash == 0xee90000c755c0472ull || hash == 0xb4b4d54a7a2d6b96ull;
}

struct MaskLease {
    std::unique_ptr<plume::RenderTexture> texture;
    std::unique_ptr<plume::RenderFramebuffer> clearFramebuffer;
    std::shared_ptr<FrameLease> rawSource;
    std::shared_ptr<MaskLease> parent;
    plume::RenderTextureLayout layout = plume::RenderTextureLayout::UNKNOWN;
};

struct ResolveVersion {
    uint64_t frame = 0, epoch = 0, sourceAllocation = 0, destinationAllocation = 0;
    uint64_t writeOrdinal = 0;
    uint32_t address = 0, format = 0, width = 0, height = 0;
    MaskRect validRect{};
    FrameIdentity rawIdentity{};
    uint32_t rawDraws = 0;
    std::shared_ptr<MaskLease> mask;
    explicit operator bool() const { return mask && mask->texture && writeOrdinal; }
};

inline bool CanRelabelReusedCopy(const ResolveVersion& old, const FrameView& raw,
    uint64_t frame, uint64_t epoch, uint64_t sourceAllocation,
    uint64_t destinationAllocation, uint32_t width, uint32_t height, MaskRect rect) {
    if (!old || !raw || !(old.rawIdentity == raw.identity)) return false;
    const CopyReuseSignature previous{old.frame, old.epoch, old.sourceAllocation,
        old.destinationAllocation, old.width, old.height, old.validRect,
        old.rawIdentity.colorAllocation, old.rawIdentity.depthAllocation, old.rawDraws};
    const CopyReuseSignature current{frame, epoch, sourceAllocation, destinationAllocation,
        width, height, rect, raw.identity.colorAllocation, raw.identity.depthAllocation, raw.auditedDraws};
    return previous == current;
}

struct ResolveEvent {
    ResolveVersion version;
    FrameView rawAtResolve;
    const char* reason = "unavailable";
    bool copied = false, reused = false;
    explicit operator bool() const { return bool(version); }
};

struct FetchCandidate {
    ResolveVersion version;
    std::shared_ptr<MaskLease> mask;
    plume::RenderTexture* returnedColor = nullptr;
    uint32_t cropWidth = 0, cropHeight = 0;
    const char* reason = "unavailable";
    explicit operator bool() const { return version && mask && mask->texture && returnedColor; }
    bool Matches(plume::RenderTexture* finallyBound) const {
        return *this && returnedColor == finallyBound;
    }
};

// A frame-local bridge for the first proven raw-alpha color resolve. Versions
// are keyed by the actual guest destination and color allocation, not by a
// mutable ResolvedSurface pointer or an address alone.
class PropagationGPU {
    plume::RenderDevice* device_ = nullptr;
    uint64_t frame_ = ~0ull, epoch_ = 0;
    std::vector<FrameView> raw_;
    std::map<std::pair<uint32_t, uint32_t>, ResolveVersion> resolved_;
    std::map<std::pair<uint32_t, uint32_t>, const char*> unavailable_;
    std::map<uint64_t, const char*> unavailableRawSources_;

    static void Transition(plume::RenderCommandList* commands, MaskLease& mask,
        plume::RenderTextureLayout to, plume::RenderBarrierStages stage) {
        commands->barriers(stage, plume::RenderTextureBarrier(mask.texture.get(), to));
        mask.layout = to;
    }
public:
    explicit PropagationGPU(plume::RenderDevice* device) : device_(device) {}
    void BeginFrame(uint64_t frame, uint64_t epoch) {
        if (frame_ == frame && epoch_ == epoch) return;
        frame_ = frame; epoch_ = epoch; Invalidate();
    }
    void Invalidate() {
        raw_.clear(); resolved_.clear(); unavailable_.clear(); unavailableRawSources_.clear();
    }
    void DiscardUnsubmitted() { Invalidate(); }
    void PublishRaw(FrameView view) {
        if (!view || view.identity.renderFrame != frame_ || view.identity.geometryEpoch != epoch_) return;
        if (unavailableRawSources_.contains(view.identity.colorAllocation)) return;
        for (auto& old : raw_) if (old.identity == view.identity) { old = std::move(view); return; }
        raw_.push_back(std::move(view));
    }
    bool HasEvidence() const {
        return !raw_.empty() || !resolved_.empty() || !unavailableRawSources_.empty();
    }
    bool HasRawSource(uint64_t colorAllocation) const {
        return std::any_of(raw_.begin(), raw_.end(), [&](const FrameView& view) {
            return view.identity.colorAllocation == colorAllocation;
        });
    }
    // An RGB write after a raw replay makes that source snapshot stale. Keep
    // the rejection for this frame: a later replay cannot reconstruct the
    // missing contribution from the intervening draw or clear.
    bool InvalidateRawSource(uint64_t colorAllocation) {
        if (!HasRawSource(colorAllocation)) return false;
        unavailableRawSources_[colorAllocation] = "source_written_after_raw";
        raw_.erase(std::remove_if(raw_.begin(), raw_.end(), [&](const FrameView& view) {
            return view.identity.colorAllocation == colorAllocation;
        }), raw_.end());
        return true;
    }
    void MarkUnsupportedPostprocess(uint64_t colorAllocation) {
        if (!colorAllocation) return;
        unavailableRawSources_[colorAllocation] = "unsupported_postprocess";
        raw_.erase(std::remove_if(raw_.begin(), raw_.end(), [&](const FrameView& view) {
            return view.identity.colorAllocation == colorAllocation;
        }), raw_.end());
    }
    ResolveEvent RecordResolve(plume::RenderCommandList* commands, uint64_t currentFrame,
        uint64_t currentEpoch, uint64_t sourceAllocation,
        uint32_t sourceWidth, uint32_t sourceHeight, uint32_t address, uint32_t format,
        uint64_t destinationAllocation, uint64_t writeOrdinal, uint32_t width, uint32_t height,
        MaskRect rect, const char* operation, std::vector<std::shared_ptr<MaskLease>>& batchUses) {
        ResolveEvent out{};
        const auto key = std::make_pair(address, format);
        auto previous = resolved_.find(key);
        const ResolveVersion old = previous == resolved_.end() ? ResolveVersion{} : previous->second;
        resolved_.erase(key);
        unavailable_.erase(key);
        if (frame_ != currentFrame || epoch_ != currentEpoch) {
            out.reason = "owner_frame_epoch_mismatch"; return out;
        }
        if (!device_ || !commands || !sourceAllocation || !destinationAllocation || !writeOrdinal ||
            !width || !height || !Contains({0, 0, width, height}, rect)) {
            out.reason = "invalid_resolve_identity_or_rect"; return out;
        }
        const FrameView* raw = nullptr;
        for (const auto& view : raw_) if (view.identity.colorAllocation == sourceAllocation &&
            view.identity.width == sourceWidth && view.identity.height == sourceHeight &&
            view.identity.renderFrame == currentFrame && view.identity.geometryEpoch == currentEpoch) {
            if (raw && !(raw->identity == view.identity)) {
                out.reason = "ambiguous_raw_identity"; return out;
            }
            raw = &view;
        }
        if (!raw || !*raw || !Contains({0, 0, sourceWidth, sourceHeight}, rect)) {
            const auto blocked = unavailableRawSources_.find(sourceAllocation);
            out.reason = blocked == unavailableRawSources_.end() ?
                "no_matching_raw_source" : blocked->second;
            unavailable_[key] = out.reason;
            return out;
        }
        out.rawAtResolve = *raw;
        ResolveVersion next{};
        next.frame = frame_; next.epoch = epoch_;
        next.sourceAllocation = sourceAllocation;
        next.destinationAllocation = destinationAllocation;
        next.writeOrdinal = writeOrdinal;
        next.address = address; next.format = format;
        next.width = width; next.height = height; next.validRect = rect;
        next.rawIdentity = raw->identity; next.rawDraws = raw->auditedDraws;
        if (operation && std::strcmp(operation, "copy_reused") == 0) {
            if (!CanRelabelReusedCopy(old, *raw, frame_, epoch_, sourceAllocation,
                destinationAllocation, width, height, rect)) {
                out.reason = "copy_reuse_revision_mismatch"; return out;
            }
            next.mask = old.mask;
            out.version = next; out.reason = "available"; out.reused = true;
            resolved_[key] = next;
            return out;
        }
        if (!operation || std::strcmp(operation, "copy") != 0) {
            out.reason = "unsupported_color_blit"; return out;
        }
        auto lease = std::make_shared<MaskLease>();
        lease->rawSource = raw->lease;
        lease->texture = device_->createTexture(plume::RenderTextureDesc::Texture2D(
            width, height, 1, plume::RenderFormat::R8_UNORM, plume::RenderTextureFlag::RENDER_TARGET));
        if (lease->texture) {
            const plume::RenderTexture* attachments[] = {lease->texture.get()};
            lease->clearFramebuffer = device_->createFramebuffer(plume::RenderFramebufferDesc(attachments, 1));
        }
        if (!lease->texture || !lease->clearFramebuffer) {
            out.reason = "mask_allocation_failed"; return out;
        }
        // Register ownership before the first GPU command references the mask.
        batchUses.push_back(lease);
        Transition(commands, *lease, plume::RenderTextureLayout::COLOR_WRITE, plume::RenderBarrierStage::GRAPHICS);
        commands->setFramebuffer(lease->clearFramebuffer.get());
        commands->clearColor(0, plume::RenderColor(0, 0, 0, 0));
        Transition(commands, *lease, plume::RenderTextureLayout::COPY_DEST, plume::RenderBarrierStage::COPY);
        commands->barriers(plume::RenderBarrierStage::COPY,
            plume::RenderTextureBarrier(raw->texture, plume::RenderTextureLayout::COPY_SOURCE));
        raw->lease->layout = plume::RenderTextureLayout::COPY_SOURCE;
        plume::RenderBox box{int32_t(rect.x), int32_t(rect.y), int32_t(rect.x + rect.width),
            int32_t(rect.y + rect.height), 0, 1};
        commands->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(lease->texture.get()),
            plume::RenderTextureCopyLocation::Subresource(raw->texture), rect.x, rect.y, 0, &box);
        commands->barriers(plume::RenderBarrierStage::GRAPHICS,
            plume::RenderTextureBarrier(raw->texture, plume::RenderTextureLayout::COLOR_WRITE));
        raw->lease->layout = plume::RenderTextureLayout::COLOR_WRITE;
        Transition(commands, *lease, plume::RenderTextureLayout::SHADER_READ, plume::RenderBarrierStage::GRAPHICS);
        next.mask = std::move(lease);
        out.version = next; out.reason = "available"; out.copied = true;
        resolved_[key] = next;
        return out;
    }
    FetchCandidate RecordFetchView(plume::RenderCommandList* commands, uint64_t currentFrame,
        uint64_t currentEpoch, uint32_t address, uint32_t format,
        uint64_t destinationAllocation, uint64_t writeOrdinal, plume::RenderTexture* returnedColor,
        uint32_t cropWidth, uint32_t cropHeight, std::vector<std::shared_ptr<MaskLease>>& batchUses) {
        FetchCandidate out{};
        if (frame_ != currentFrame || epoch_ != currentEpoch) {
            out.reason = "owner_frame_epoch_mismatch"; return out;
        }
        auto found = resolved_.find({address, format});
        if (found == resolved_.end()) {
            if (auto reason = unavailable_.find({address, format}); reason != unavailable_.end())
                out.reason = reason->second;
            else out.reason = "resolved_version_unavailable";
            return out;
        }
        if (!found->second || !returnedColor ||
            found->second.destinationAllocation != destinationAllocation ||
            found->second.writeOrdinal != writeOrdinal || found->second.frame != frame_ ||
            found->second.epoch != epoch_) {
            out.reason = "resolved_version_unavailable"; return out;
        }
        auto version = found->second;
        out.version = version; out.returnedColor = returnedColor;
        out.cropWidth = cropWidth; out.cropHeight = cropHeight;
        if (!Contains(version.validRect, {0, 0, cropWidth, cropHeight}) ||
            cropWidth > version.width || cropHeight > version.height) {
            out.reason = "uninitialized_fetch_crop"; return out;
        }
        if (cropWidth == version.width && cropHeight == version.height) {
            out.mask = version.mask; out.reason = "available"; return out;
        }
        if (!commands || !device_) { out.reason = "crop_command_unavailable"; return out; }
        auto crop = std::make_shared<MaskLease>();
        crop->parent = version.mask;
        crop->texture = device_->createTexture(plume::RenderTextureDesc::Texture2D(
            cropWidth, cropHeight, 1, plume::RenderFormat::R8_UNORM));
        if (!crop->texture) { out.reason = "crop_allocation_failed"; return out; }
        batchUses.push_back(crop);
        Transition(commands, *version.mask, plume::RenderTextureLayout::COPY_SOURCE, plume::RenderBarrierStage::COPY);
        Transition(commands, *crop, plume::RenderTextureLayout::COPY_DEST, plume::RenderBarrierStage::COPY);
        plume::RenderBox box{0, 0, int32_t(cropWidth), int32_t(cropHeight), 0, 1};
        commands->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(crop->texture.get()),
            plume::RenderTextureCopyLocation::Subresource(version.mask->texture.get()), 0, 0, 0, &box);
        Transition(commands, *version.mask, plume::RenderTextureLayout::SHADER_READ, plume::RenderBarrierStage::GRAPHICS);
        Transition(commands, *crop, plume::RenderTextureLayout::SHADER_READ, plume::RenderBarrierStage::GRAPHICS);
        out.mask = std::move(crop); out.reason = "available";
        return out;
    }
};
} // namespace gpu::fsr_alpha
#endif
