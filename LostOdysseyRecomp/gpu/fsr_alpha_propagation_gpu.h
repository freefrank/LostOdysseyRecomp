#pragma once

#include "fsr_alpha_propagation_policy.h"
#include "fsr_alpha_replay_gpu.h"
#include "temporal_frame_inputs.h"
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
inline bool AuditedPostprocessPair(uint64_t vs, uint64_t ps) {
    return (vs == 0x2f6bbed8149a7804ull &&
            (ps == 0x7c260eacff1d681dull || ps == 0x53dd5d081c7945cfull)) ||
        (vs == 0xd1b241c74103b6bfull && ps == 0xee90000c755c0472ull) ||
        (vs == 0x9b81c55ca39bb529ull && ps == 0xb4b4d54a7a2d6b96ull);
}

struct MaskLease {
    std::unique_ptr<plume::RenderTexture> texture;
    std::unique_ptr<plume::RenderFramebuffer> clearFramebuffer;
    std::shared_ptr<FrameLease> rawSource;
    std::shared_ptr<MaskLease> parent;
    plume::RenderTextureLayout layout = plume::RenderTextureLayout::UNKNOWN;
};

enum class SourceStage : uint8_t { Raw, Downsample, Dof, Bloom, Tonemap };

// A postprocess output is identified by its own color allocation and draw
// revision. It has no raw-scene depth, viewport or jitter identity.
struct SourceMask {
    uint64_t frame = 0, epoch = 0, colorAllocation = 0, revision = 0;
    uint32_t width = 0, height = 0;
    MaskRect validRect{};
    SourceStage stage = SourceStage::Raw;
    std::shared_ptr<MaskLease> mask;
    explicit operator bool() const { return frame && colorAllocation && mask && mask->texture; }
};

struct ResolveVersion {
    uint64_t frame = 0, epoch = 0, sourceAllocation = 0, destinationAllocation = 0;
    uint64_t writeOrdinal = 0;
    uint32_t address = 0, format = 0, width = 0, height = 0;
    MaskRect validRect{};
    FrameIdentity rawIdentity{};
    uint32_t rawDraws = 0;
    uint64_t sourceRevision = 0;
    SourceStage sourceStage = SourceStage::Raw;
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

struct SceneCopyMask {
    uint64_t frame = 0, epoch = 0, colorOrdinal = 0, sourceWriteOrdinal = 0;
    uint64_t sourceAllocation = 0;
    uint32_t width = 0, height = 0;
    plume::RenderTexture* capturedColor = nullptr;
    std::shared_ptr<MaskLease> mask;
    explicit operator bool() const { return frame && colorOrdinal && capturedColor && mask; }
};

// Only the frozen, paired source may enter the borrowed SR handoff. The
// renderer separately carries sceneCopy.mask's lease into the consuming slot.
inline bool AttachSceneCopyMask(const SceneCopyMask& sceneCopy, temporal::TemporalFrameInputs& inputs) {
    inputs.fsrMask = {};
    if (!sceneCopy || !sceneCopy.mask->texture || !sceneCopy.sourceAllocation ||
        !sceneCopy.sourceWriteOrdinal || sceneCopy.frame != inputs.renderFrameId ||
        sceneCopy.epoch != inputs.temporalEpoch || sceneCopy.colorOrdinal != inputs.colorOrdinal ||
        sceneCopy.capturedColor != inputs.color.texture ||
        sceneCopy.width != inputs.color.width || sceneCopy.height != inputs.color.height) return false;
    auto& mask = inputs.fsrMask;
    mask.sceneContribution = {sceneCopy.mask->texture.get(), {sceneCopy.width, sceneCopy.height},
        0, 0, sceneCopy.width, sceneCopy.height};
    mask.provenance = {sceneCopy.frame, sceneCopy.epoch, inputs.plan.geometryEpoch,
        inputs.plan.deviceEpoch, sceneCopy.colorOrdinal, sceneCopy.sourceAllocation,
        sceneCopy.sourceWriteOrdinal, inputs.color.texture};
    mask.semantic = temporal::FsrMaskSemantic::ConservativeTransparentAlpha;
    mask.coverage = temporal::FsrMaskCoverage::Partial;
    return true;
}

// The DTO remains borrowed; retain the exact producer image in the consuming
// GPU batch before recording a barrier or descriptor that references it.
inline bool RetainSceneCopyMaskForBatch(temporal::TemporalFrameInputs& inputs,
    const std::shared_ptr<MaskLease>& lease, std::vector<std::shared_ptr<MaskLease>>& batchUses) {
    if (!lease || !lease->texture ||
        inputs.fsrMask.sceneContribution.texture != lease->texture.get()) {
        inputs.fsrMask = {};
        return false;
    }
    batchUses.push_back(lease);
    return true;
}

// A frame-local bridge for the first proven raw-alpha color resolve. Versions
// are keyed by the actual guest destination and color allocation, not by a
// mutable ResolvedSurface pointer or an address alone.
class PropagationGPU {
    plume::RenderDevice* device_ = nullptr;
    uint64_t frame_ = ~0ull, epoch_ = 0;
    std::vector<FrameView> raw_;
    std::map<uint64_t, SourceMask> sources_;
    SceneCopyMask sceneCopy_{};
    std::map<std::pair<uint32_t, uint32_t>, ResolveVersion> resolved_;
    std::map<std::pair<uint32_t, uint32_t>, const char*> unavailable_;
    std::map<uint64_t, const char*> unavailableRawSources_;
    std::map<uint64_t, ClearBackground> clearBackgrounds_;

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
        raw_.clear(); sources_.clear(); sceneCopy_ = {};
        resolved_.clear(); unavailable_.clear(); unavailableRawSources_.clear(); clearBackgrounds_.clear();
    }
    void DiscardUnsubmitted() { Invalidate(); }
    void PublishRaw(FrameView view) {
        if (!view || view.identity.renderFrame != frame_ || view.identity.geometryEpoch != epoch_) return;
        InvalidateClearBackground(view.identity.colorAllocation, "raw_draw");
        if (unavailableRawSources_.contains(view.identity.colorAllocation)) return;
        // A later audited raw write supersedes a prior full-screen stage on
        // this same guest color allocation. Resolves must consume the newest
        // producer, not the older stage just because it has higher priority.
        sources_.erase(view.identity.colorAllocation);
        for (auto& old : raw_) if (old.identity == view.identity) { old = std::move(view); return; }
        raw_.push_back(std::move(view));
    }
    void PublishPostprocess(SourceMask source) {
        if (!source || source.frame != frame_ || source.epoch != epoch_ ||
            !source.width || !source.height ||
             !Contains({0, 0, source.width, source.height}, source.validRect)) return;
        InvalidateClearBackground(source.colorAllocation, "postprocess_draw");
        raw_.erase(std::remove_if(raw_.begin(), raw_.end(), [&](const FrameView& view) {
            return view.identity.colorAllocation == source.colorAllocation;
        }), raw_.end());
        unavailableRawSources_.erase(source.colorAllocation);
        sources_[source.colorAllocation] = std::move(source);
    }
    const SourceMask* CurrentSource(uint64_t colorAllocation, uint64_t frame, uint64_t epoch) const {
        if (frame != frame_ || epoch != epoch_) return nullptr;
        const auto found = sources_.find(colorAllocation);
        return found != sources_.end() && found->second.frame == frame && found->second.epoch == epoch ?
            &found->second : nullptr;
    }
    bool RecordFullColorClear(uint64_t frame, uint64_t epoch, uint64_t allocation,
        uint32_t width, uint32_t height, uint64_t ordinal,
        MaskRect affectedRect, const char* kind) {
        if (frame != frame_ || epoch != epoch_ || !allocation || !width || !height ||
            !kind || affectedRect != MaskRect{0, 0, width, height}) return false;
        const bool replacedSource = sources_.contains(allocation) ||
            std::any_of(raw_.begin(), raw_.end(), [&](const FrameView& view) {
                return view.identity.colorAllocation == allocation;
            });
        clearBackgrounds_[allocation] = {frame, epoch, allocation, ordinal,
            width, height, affectedRect, kind, nullptr};
        // The clear supersedes this allocation's old source, but resolved
        // snapshots already copied from it retain their independent leases.
        sources_.erase(allocation);
        raw_.erase(std::remove_if(raw_.begin(), raw_.end(), [&](const FrameView& view) {
            return view.identity.colorAllocation == allocation;
        }), raw_.end());
        // A first clear has no accumulated replay to forbid. Preserve an
        // earlier writer rejection even if HandleFsrAlphaRgbWriter already
        // removed the source immediately before this clear is recorded.
        if (replacedSource) unavailableRawSources_.try_emplace(allocation, "color_cleared_after_raw");
        return true;
    }
    const ClearBackground* RecentClear(uint64_t allocation) const {
        const auto found = clearBackgrounds_.find(allocation);
        return found == clearBackgrounds_.end() ? nullptr : &found->second;
    }
    void InvalidateClearBackground(uint64_t allocation, const char* writer) {
        const auto found = clearBackgrounds_.find(allocation);
        if (found != clearBackgrounds_.end() && !found->second.invalidatedBy)
            found->second.invalidatedBy = writer;
    }
    bool FreezeForSceneCopy(const FetchCandidate& fetched, plume::RenderTexture* actualSource,
        plume::RenderTexture* capturedColor, uint64_t frame, uint64_t epoch,
        uint64_t colorOrdinal, uint32_t width, uint32_t height) {
        sceneCopy_ = {};
        if (frame != frame_ || epoch != epoch_ || !colorOrdinal || !capturedColor ||
            !fetched.Matches(actualSource) || fetched.cropWidth != width ||
            fetched.cropHeight != height ||
            !Contains(fetched.version.validRect, {0, 0, width, height})) return false;
        sceneCopy_ = {frame, epoch, colorOrdinal, fetched.version.writeOrdinal,
            fetched.version.sourceAllocation, width, height, capturedColor, fetched.mask};
        return true;
    }
    SceneCopyMask SceneCopy() const { return sceneCopy_; }
    bool HasEvidence() const {
        return !raw_.empty() || !sources_.empty() || !resolved_.empty() || !unavailableRawSources_.empty();
    }
    bool HasRawSource(uint64_t colorAllocation) const {
        return std::any_of(raw_.begin(), raw_.end(), [&](const FrameView& view) {
            return view.identity.colorAllocation == colorAllocation;
        });
    }
    bool HasCurrentSource(uint64_t colorAllocation) const {
        return HasRawSource(colorAllocation) || sources_.contains(colorAllocation);
    }
    // An RGB write after a raw replay makes that source snapshot stale. Keep
    // the rejection for this frame: a later replay cannot reconstruct the
    // missing contribution from the intervening draw or clear.
    bool InvalidateRawSource(uint64_t colorAllocation) {
        InvalidateClearBackground(colorAllocation, "rgb_writer");
        if (!HasCurrentSource(colorAllocation)) return false;
        unavailableRawSources_[colorAllocation] = "source_written_after_raw";
        sources_.erase(colorAllocation);
        raw_.erase(std::remove_if(raw_.begin(), raw_.end(), [&](const FrameView& view) {
            return view.identity.colorAllocation == colorAllocation;
        }), raw_.end());
        return true;
    }
    void MarkUnsupportedPostprocess(uint64_t colorAllocation) {
        if (!colorAllocation) return;
        InvalidateClearBackground(colorAllocation, "unsupported_postprocess");
        unavailableRawSources_[colorAllocation] = "unsupported_postprocess";
        sources_.erase(colorAllocation);
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
        const SourceMask* stage = CurrentSource(sourceAllocation, currentFrame, currentEpoch);
        if (stage && (stage->width != sourceWidth || stage->height != sourceHeight)) {
            out.reason = "postprocess_source_rect_unavailable";
            unavailable_[key] = out.reason;
            return out;
        }
        const MaskRect validCopy = stage ? IntersectCopyValidRect(stage->validRect, rect) : rect;
        if (stage && !Contains({0, 0, sourceWidth, sourceHeight}, validCopy)) {
            out.reason = "postprocess_source_rect_unavailable";
            unavailable_[key] = out.reason;
            return out;
        }
        const FrameView* raw = nullptr;
        for (const auto& view : raw_) if (!stage &&
            view.identity.colorAllocation == sourceAllocation &&
            view.identity.width == sourceWidth && view.identity.height == sourceHeight &&
            view.identity.renderFrame == currentFrame && view.identity.geometryEpoch == currentEpoch) {
            if (raw && !(raw->identity == view.identity)) {
                out.reason = "ambiguous_raw_identity"; return out;
            }
            raw = &view;
        }
        if ((!stage && (!raw || !*raw)) ||
            !Contains({0, 0, sourceWidth, sourceHeight}, rect)) {
            const auto blocked = unavailableRawSources_.find(sourceAllocation);
            out.reason = blocked == unavailableRawSources_.end() ?
                "no_matching_raw_source" : blocked->second;
            unavailable_[key] = out.reason;
            return out;
        }
        if (raw) out.rawAtResolve = *raw;
        ResolveVersion next{};
        next.frame = frame_; next.epoch = epoch_;
        next.sourceAllocation = sourceAllocation;
        next.destinationAllocation = destinationAllocation;
        next.writeOrdinal = writeOrdinal;
        next.address = address; next.format = format;
        next.width = width; next.height = height; next.validRect = validCopy;
        if (raw) {
            next.rawIdentity = raw->identity; next.rawDraws = raw->auditedDraws;
            next.sourceRevision = raw->auditedDraws;
        } else {
            next.sourceRevision = stage->revision;
            next.sourceStage = stage->stage;
        }
        if (operation && std::strcmp(operation, "copy_reused") == 0) {
            const bool sameSource = raw ? CanRelabelReusedCopy(old, *raw, frame_, epoch_,
                sourceAllocation, destinationAllocation, width, height, rect) :
                old && old.frame == frame_ && old.epoch == epoch_ &&
                old.sourceAllocation == sourceAllocation && old.destinationAllocation == destinationAllocation &&
                old.sourceRevision == stage->revision && old.sourceStage == stage->stage &&
                old.width == width && old.height == height && old.validRect == validCopy;
            if (!sameSource) {
                out.reason = "copy_reuse_revision_mismatch"; return out;
            }
            next.mask = old.mask;
            out.version = next; out.reason = "available"; out.reused = true;
            resolved_[key] = next;
            return out;
        }
        // Renderer::BlitRegion uses src.Load(pixel) and the identical source and
        // destination pixel coordinates. Only its format conversion changes.
        if (!operation || (std::strcmp(operation, "copy") != 0 &&
            std::strcmp(operation, "blit") != 0)) {
            out.reason = "unsupported_color_blit"; return out;
        }
        auto lease = std::make_shared<MaskLease>();
        if (raw) lease->rawSource = raw->lease;
        else lease->parent = stage->mask;
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
        auto* sourceTexture = raw ? raw->texture : stage->mask->texture.get();
        commands->barriers(plume::RenderBarrierStage::COPY,
            plume::RenderTextureBarrier(sourceTexture, plume::RenderTextureLayout::COPY_SOURCE));
        if (raw) raw->lease->layout = plume::RenderTextureLayout::COPY_SOURCE;
        else stage->mask->layout = plume::RenderTextureLayout::COPY_SOURCE;
        plume::RenderBox box{int32_t(validCopy.x), int32_t(validCopy.y),
            int32_t(validCopy.x + validCopy.width), int32_t(validCopy.y + validCopy.height), 0, 1};
        commands->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(lease->texture.get()),
            plume::RenderTextureCopyLocation::Subresource(sourceTexture), validCopy.x, validCopy.y, 0, &box);
        commands->barriers(plume::RenderBarrierStage::GRAPHICS,
            plume::RenderTextureBarrier(sourceTexture, raw ?
                plume::RenderTextureLayout::COLOR_WRITE : plume::RenderTextureLayout::SHADER_READ));
        if (raw) raw->lease->layout = plume::RenderTextureLayout::COLOR_WRITE;
        else stage->mask->layout = plume::RenderTextureLayout::SHADER_READ;
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
