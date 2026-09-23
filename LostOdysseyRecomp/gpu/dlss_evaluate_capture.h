#pragma once

#if defined(LO_GPU_PLUME)
#include "dlss_sr.h"
#include "temporal_frame_inputs.h"
#include <plume_vulkan.h>
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace gpu::dlss::capture {
// Capture entries are owned by the renderer page and its GPU slot. NGX borrows
// an entry only while recording; neither the controller nor an archive owns it.
struct Image {
    std::unique_ptr<plume::RenderBuffer> buffer;
    uint32_t width = 0, height = 0, storageWidth = 0, storageHeight = 0;
    uint32_t x = 0, y = 0, texelBytes = 0;
    VkFormat format = VK_FORMAT_UNDEFINED;
    bool recorded = false, available = false;
    std::string reason = "evaluate_not_called";
    size_t Bytes() const { return size_t(width) * height * texelBytes; }
};

struct Parameters {
    float jitterX = 0, jitterY = 0, mvScaleX = 0, mvScaleY = 0, preExposure = 0, exposureScale = 0;
    uint32_t colorX = 0, colorY = 0, depthX = 0, depthY = 0, mvX = 0, mvY = 0;
    uint32_t outputX = 0, outputY = 0, renderWidth = 0, renderHeight = 0;
    bool reset = false, featureCreated = false, inputHistoryReset = false;
};

inline uint32_t TexelBytes(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM: return 4;
    case VK_FORMAT_R16G16B16A16_SFLOAT: return 8;
    default: return 0;
    }
}
inline const char* FormatName(VkFormat format) {
    switch (format) {
    case VK_FORMAT_R8G8B8A8_UNORM: return "RGBA8_UNORM";
    case VK_FORMAT_R16G16B16A16_SFLOAT: return "RGBA16_FLOAT";
    default: return "unsupported";
    }
}
inline float Half(uint16_t h) {
    const float value = float(h & 1023) / 1024.0f;
    const int e = (h >> 10) & 31;
    const float magnitude = e == 31 ? (value ? NAN : INFINITY) :
        e == 0 ? std::ldexp(value, -14) : std::ldexp(1.0f + value, e - 15);
    return (h & 0x8000) ? -magnitude : magnitude;
}
inline uint8_t PreviewChannel(const uint8_t* texel, VkFormat format, uint32_t channel) {
    if (format == VK_FORMAT_R8G8B8A8_UNORM) return texel[channel];
    const uint16_t half = uint16_t(texel[channel * 2]) | (uint16_t(texel[channel * 2 + 1]) << 8);
    const float v = Half(half);
    return uint8_t(std::clamp(std::isnan(v) ? 0.0f : v, 0.0f, 1.0f) * 255.0f + 0.5f);
}
inline bool WritePreview(const std::filesystem::path& path, const uint8_t* raw, const Image& image) {
    const uint32_t pitch = (image.width * 3 + 3) & ~3u;
    const uint32_t bytes = pitch * image.height + 54;
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) return false;
    auto u16 = [&](uint16_t v) { file.put(char(v)); file.put(char(v >> 8)); };
    auto u32 = [&](uint32_t v) { u16(uint16_t(v)); u16(uint16_t(v >> 16)); };
    file.put('B'); file.put('M'); u32(bytes); u32(0); u32(54);
    u32(40); u32(image.width); u32(image.height); u16(1); u16(24);
    u32(0); u32(bytes - 54); u32(2835); u32(2835); u32(0); u32(0);
    for (uint32_t y = image.height; y-- > 0;) {
        for (uint32_t x = 0; x < image.width; ++x) {
            const auto* p = raw + (size_t(y) * image.width + x) * image.texelBytes;
            file.put(char(PreviewChannel(p, image.format, 2)));
            file.put(char(PreviewChannel(p, image.format, 1)));
            file.put(char(PreviewChannel(p, image.format, 0)));
        }
        for (uint32_t i = image.width * 3; i < pitch; ++i) file.put(0);
    }
    file.close(); return !file.fail();
}
inline void CopyImage(VkCommandBuffer command, const plume::VulkanTexture& texture, const Image& image) {
    VkImageMemoryBarrier barrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.srcQueueFamilyIndex = barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = texture.vk;
    barrier.subresourceRange = texture.imageSubresourceRange;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
    VkBufferImageCopy copy{};
    copy.imageSubresource.aspectMask = texture.imageSubresourceRange.aspectMask;
    copy.imageSubresource.mipLevel = texture.imageSubresourceRange.baseMipLevel;
    copy.imageSubresource.baseArrayLayer = texture.imageSubresourceRange.baseArrayLayer;
    copy.imageSubresource.layerCount = 1;
    copy.imageOffset = {int32_t(image.x), int32_t(image.y), 0};
    copy.imageExtent = {image.width, image.height, 1};
    vkCmdCopyImageToBuffer(command, texture.vk, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        static_cast<plume::VulkanBuffer*>(image.buffer.get())->vk, 1, &copy);
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0, 0, nullptr, 0, nullptr, 1, &barrier);
}

struct Entry {
    uint32_t attemptIndex = 0;
    std::optional<uint32_t> evaluateIndex;
    uint64_t page = 0, renderFrame = 0, resolveOrdinal = 0, drawOrdinal = 0;
    temporal::TemporalFrameInputs inputs{};
    SrConfig config{};
    Parameters sdk{};
    Image input, output;
    bool omitted = false, evaluated = false, vendorSuccess = false;
    bool isolatedAccepted = false, isolatedIncluded = false, compositeSucceeded = false, adopted = false;
    bool checkedSubmit = false, completed = false;
    uint64_t submissionSerial = 0;
    std::optional<int32_t> vendorResult, createResult;
    std::string stage = "before_validation", reason;

    static void Setup(Image& dst, const plume::VulkanTexture& texture, uint32_t x, uint32_t y,
        uint32_t width, uint32_t height) {
        dst.x = x; dst.y = y; dst.width = width; dst.height = height;
        dst.storageWidth = texture.desc.width; dst.storageHeight = texture.desc.height;
        dst.format = texture.imageFormat; dst.texelBytes = TexelBytes(dst.format);
    }
    bool Allocate(plume::RenderDevice& device, const plume::VulkanTexture& color,
        const plume::VulkanTexture& scratch, uint32_t colorX, uint32_t colorY,
        uint32_t renderWidth, uint32_t renderHeight, uint32_t outputX, uint32_t outputY,
        uint32_t outputWidth, uint32_t outputHeight) {
        Setup(input, color, colorX, colorY, renderWidth, renderHeight);
        Setup(output, scratch, outputX, outputY, outputWidth, outputHeight);
        auto valid = [](const Image& image) {
            return image.texelBytes && image.width && image.height && image.x <= image.storageWidth &&
                image.y <= image.storageHeight && image.width <= image.storageWidth - image.x &&
                image.height <= image.storageHeight - image.y;
        };
        if (!valid(input) || !valid(output)) { reason = "capture_unsupported_format_or_region"; return false; }
        input.buffer = device.createBuffer(plume::RenderBufferDesc::ReadbackBuffer(input.Bytes()));
        output.buffer = device.createBuffer(plume::RenderBufferDesc::ReadbackBuffer(output.Bytes()));
        if (!input.buffer || !output.buffer) {
            input.buffer.reset(); output.buffer.reset(); reason = "capture_allocation_failed"; return false;
        }
        input.reason = output.reason = "not_submitted";
        return true;
    }
    void Before(VkCommandBuffer command, const plume::VulkanTexture& color) {
        if (!input.buffer) return;
        if (input.x != sdk.colorX || input.y != sdk.colorY || input.width != sdk.renderWidth ||
            input.height != sdk.renderHeight) { reason = "capture_sdk_color_rect_mismatch"; return; }
        CopyImage(command, color, input); input.recorded = true;
    }
    void After(VkCommandBuffer command, const plume::VulkanTexture& scratch) {
        if (!output.buffer) return;
        if (output.x != sdk.outputX || output.y != sdk.outputY ||
            output.width != config.outputExtent.width || output.height != config.outputExtent.height) {
            reason = "capture_sdk_output_rect_mismatch"; return;
        }
        CopyImage(command, scratch, output); output.recorded = true;
    }
    bool ExportImage(Image& image, const std::filesystem::path& dir, const char* name) {
        if (!image.recorded || !isolatedIncluded || !checkedSubmit || !completed) {
            image.reason = !isolatedAccepted ? "discarded_isolated_list" :
                !checkedSubmit ? "submit_not_confirmed" : !completed ? "completion_not_confirmed" : "copy_not_recorded";
            return false;
        }
        if (!image.buffer) { image.reason = reason.empty() ? "capture_allocation_failed" : reason; return false; }
        auto* bytes = static_cast<const uint8_t*>(image.buffer->map());
        if (!bytes) { image.reason = "map_failed"; return false; }
        const auto rawPath = dir / (std::string(name) + ".bin");
        const auto previewPath = dir / (std::string(name) + "-preview.bmp");
        std::ofstream raw(rawPath, std::ios::binary | std::ios::trunc);
        if (raw) raw.write(reinterpret_cast<const char*>(bytes), std::streamsize(image.Bytes()));
        raw.close();
        const bool saved = !raw.fail() && WritePreview(previewPath, bytes, image);
        image.buffer->unmap();
        if (!saved) {
            std::error_code ec; std::filesystem::remove(rawPath, ec); std::filesystem::remove(previewPath, ec);
            image.reason = "write_failed";
        } else { image.available = true; image.reason = "readback"; }
        return saved;
    }
};
} // namespace gpu::dlss::capture
namespace gpu::dlss { struct EvaluateCapture : capture::Entry {}; }
namespace gpu::dlss::capture {

inline const char* ResetName(temporal::TemporalResetReason reason) {
    using R = temporal::TemporalResetReason;
    switch (reason) {
    case R::FirstFrame: return "first_frame";
    case R::FrameDiscontinuity: return "frame_discontinuity";
    case R::EpochChanged: return "epoch_changed";
    case R::AllocationChanged: return "allocation_changed";
    case R::ExtentChanged: return "extent_changed";
    case R::ColorEncodingChanged: return "color_encoding_changed";
    case R::ConsumerChanged: return "consumer_changed";
    case R::IncompleteInputs: return "incomplete_inputs";
    case R::CameraDiscontinuity: return "camera_discontinuity";
    case R::PlanConfigurationChanged: return "plan_configuration_changed";
    default: return "unknown";
    }
}
inline const char* FrameFallbackName(frame_plan::DlssEffectReason reason) {
    using R = frame_plan::DlssEffectReason;
    switch (reason) {
    case R::None: return "none";
    case R::AwaitingGpuFrame: return "awaiting_gpu_frame";
    case R::DeviceNotReady: return "device_not_ready";
    case R::NeedsVulkanRestart: return "needs_vulkan_restart";
    case R::CapabilityUnavailable: return "capability_unavailable";
    case R::SizingPending: return "sizing_pending";
    case R::SizingUnavailable: return "sizing_unavailable";
    case R::SizingError: return "sizing_error";
    case R::InputProbeOnly: return "input_probe_only";
    case R::NoEligibleScene: return "no_eligible_scene";
    case R::MotionPipelinePending: return "motion_pipeline_pending";
    case R::UnknownColorEncoding: return "unknown_color_encoding";
    case R::FeatureReconfigurePending: return "feature_reconfigure_pending";
    case R::PromotionUnavailable: return "promotion_unavailable";
    case R::RequestFailure: return "request_failure";
    case R::GpuWorkStopped: return "gpu_work_stopped";
    }
    return "unknown_frame_fallback";
}
inline void JsonString(std::ostream& file, const std::string& value) {
    file << '"';
    for (unsigned char c : value) {
        if (c == '"' || c == '\\') file << '\\' << char(c);
        else if (c < 32) file << '?'; else file << char(c);
    }
    file << '"';
}
inline void JsonImage(std::ostream& file, const Image& image) {
    file << "{\"available\":" << (image.available ? "true" : "false") << ",\"reason\":";
    JsonString(file, image.reason);
    file << ",\"format\":\"" << FormatName(image.format) << "\",\"vk_format\":" << int(image.format)
         << ",\"channels\":\"RGBA\",\"component_type\":\"" << (image.texelBytes == 8 ? "float16" : "unorm8")
         << "\",\"endianness\":\"little\",\"content_rect\":[" << image.x << ',' << image.y << ',' << image.width << ',' << image.height
         << "],\"storage\":[" << image.storageWidth << ',' << image.storageHeight << "],\"tight_row_stride\":"
         << uint64_t(image.width) * image.texelBytes << ",\"raw_bytes\":" << image.Bytes() << '}';
}
struct Page {
    static constexpr uint32_t kMaxSaved = 4;
    static constexpr size_t kMaxBytes = 128u << 20;
    uint32_t number = 0, frame = 0, attempts = 0, evaluates = 0, reserved = 0, saved = 0, omitted = 0;
    size_t reservedBytes = 0;
    uint64_t temporalEpochEnd = 0;
    bool gapResetBeforeInputs = false, resetAtFrameEnd = false;
    bool closed = false;
    bool captureFailed = false;
    std::string fallbackReason = "no_eligible_scene";
    frame_plan::FramePlan framePlan{};
    std::vector<std::shared_ptr<Entry>> entries;
    bool CanReserve(uint64_t bytes) const {
        return reserved < kMaxSaved && bytes <= kMaxBytes && reservedBytes <= kMaxBytes - bytes;
    }
    std::shared_ptr<EvaluateCapture> NewAttempt(plume::RenderDevice& device, const temporal::TemporalFrameInputs& inputs,
        const SrConfig& config, const plume::VulkanTexture& color, const plume::VulkanTexture& scratch,
        uint64_t resolveOrdinal, uint64_t drawOrdinal) {
        auto e = std::make_shared<EvaluateCapture>();
        e->attemptIndex = ++attempts; e->page = number; e->renderFrame = frame;
        e->resolveOrdinal = resolveOrdinal; e->drawOrdinal = drawOrdinal;
        e->inputs = inputs; e->config = config;
        entries.push_back(e);
        const uint64_t bytes = uint64_t(inputs.color.width) * inputs.color.height * TexelBytes(color.imageFormat) +
            uint64_t(scratch.desc.width) * scratch.desc.height * TexelBytes(scratch.imageFormat);
        if (!CanReserve(bytes)) {
            e->omitted = true; e->reason = "capture_limit"; ++omitted; return e;
        }
        if (e->Allocate(device, color, scratch, inputs.color.x, inputs.color.y, inputs.color.width,
            inputs.color.height, 0, 0, scratch.desc.width, scratch.desc.height)) {
            ++reserved; reservedBytes += size_t(bytes);
        }
        return e;
    }
    void Called(const std::shared_ptr<Entry>& e) { e->evaluateIndex = ++evaluates; }
    void CancelReservation(const std::shared_ptr<Entry>& e) {
        if (e->evaluated) return;
        if (e->omitted) { --omitted; e->omitted = false; }
        if (!e->input.buffer) return;
        reservedBytes -= e->input.Bytes() + e->output.Bytes();
        --reserved;
        e->input.buffer.reset(); e->output.buffer.reset();
        e->input.reason = e->output.reason = "evaluate_not_called";
    }
    bool Export(const std::filesystem::path& dir) {
        bool ok = !captureFailed;
        for (const auto& e : entries) {
            if (!e->evaluated || e->omitted) continue;
            const auto basename = [e](const char* name) {
                std::string n = std::to_string(*e->evaluateIndex);
                return std::string("dlss-") + name + "-" + std::string(3 - std::min<size_t>(3, n.size()), '0') + n;
            };
            if (e->vendorSuccess && e->isolatedIncluded && e->checkedSubmit) {
                const bool inputSaved = e->ExportImage(e->input, dir, basename("input").c_str());
                const bool outputSaved = e->ExportImage(e->output, dir, basename("output").c_str());
                if (inputSaved && outputSaved) ++saved;
                ok = inputSaved && outputSaved && ok;
            } else {
                e->input.reason = !e->isolatedAccepted ? "discarded_isolated_list" :
                    !e->checkedSubmit ? "submit_not_confirmed" : "vendor_failed";
                e->output.reason = !e->vendorSuccess ? "vendor_failed" : e->input.reason;
                if (e->vendorSuccess && !e->omitted) ok = false;
            }
        }
        std::ofstream file(dir / "dlss-evaluations.json", std::ios::trunc);
        if (!file) return false;
        file << "{\"schema\":1,\"page\":" << number << ",\"render_frame\":" << frame
             << ",\"frame_plan\":{\"cpu_serial\":" << framePlan.cpuSerial
             << ",\"request_signature\":" << framePlan.requestSignature
             << ",\"geometry_epoch\":" << framePlan.geometryEpoch
             << ",\"device_epoch\":" << framePlan.deviceEpoch
             << ",\"requested_upscaler\":" << uint32_t(framePlan.requestedUpscaler)
             << ",\"consumer\":" << uint32_t(framePlan.consumer)
             << ",\"sizing_revision\":" << framePlan.sizingRevision << '}'
             << ",\"attempt_count\":" << attempts << ",\"evaluate_count\":" << evaluates
             << ",\"saved\":" << saved << ",\"omitted\":" << omitted
             << ",\"capture_failed\":" << (captureFailed ? "true" : "false")
             << ",\"truncated\":" << (omitted ? "true" : "false")
             << ",\"max_saved_evaluations\":" << kMaxSaved << ",\"max_readback_bytes\":" << kMaxBytes
             << ",\"gap_reset_before_inputs\":" << (gapResetBeforeInputs ? "true" : "false")
             << ",\"reset_at_frame_end\":" << (resetAtFrameEnd ? "true" : "false")
             << ",\"temporal_epoch_after_frame\":" << temporalEpochEnd << ",\"zero_evaluate_reason\":";
        JsonString(file, evaluates ? "" : fallbackReason);
        file << ",\"evaluations\":[";
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& e = *entries[i]; const auto& p = e.inputs.plan; const auto& s = e.sdk;
            if (i) file << ',';
            file << "{\"attempt_index\":" << e.attemptIndex << ",\"evaluate_index\":";
            if (e.evaluateIndex) file << *e.evaluateIndex; else file << "null";
            file << ",\"page\":" << e.page << ",\"render_frame\":" << e.renderFrame
                 << ",\"scene_color_resolve_ordinal\":" << e.resolveOrdinal << ",\"scene_copy_draw_ordinal\":" << e.drawOrdinal
                 << ",\"plan\":{\"cpu_serial\":" << p.cpuSerial << ",\"request_signature\":" << p.requestSignature
                 << ",\"geometry_epoch\":" << p.geometryEpoch << ",\"device_epoch\":" << p.deviceEpoch
                 << ",\"requested_upscaler\":" << uint32_t(p.requestedUpscaler) << ",\"quality\":" << uint32_t(p.dlssQuality)
                 << ",\"consumer\":" << uint32_t(p.consumer) << ",\"input\":[" << p.width << ',' << p.height
                 << "],\"output\":[" << p.output.width << ',' << p.output.height << "]}"
                 << ",\"sr_config\":{\"render\":[" << e.config.renderExtent.width << ',' << e.config.renderExtent.height
                 << "],\"output\":[" << e.config.outputExtent.width << ',' << e.config.outputExtent.height
                  << "],\"quality\":" << uint32_t(e.config.quality) << ",\"device_epoch\":" << e.config.deviceEpoch
                  << ",\"color_space\":" << uint32_t(e.config.colorSpace) << ",\"color_encoding\":" << uint32_t(e.inputs.colorEncoding)
                  << ",\"color_space_name\":\"" << (e.config.colorSpace == SrColorSpace::Linear ? "linear" :
                      e.config.colorSpace == SrColorSpace::DisplayEncoded ? "display_encoded" : "unknown")
                  << "\",\"color_encoding_name\":\"" << (e.inputs.colorEncoding == temporal::ColorEncoding::HdrLinear ? "hdr_linear" :
                      e.inputs.colorEncoding == temporal::ColorEncoding::Sdr ? "sdr" : "unknown") << '"'
                 << ",\"depth_inverted\":" << (e.config.depthInverted ? "true" : "false")
                 << ",\"auto_exposure\":" << (e.config.autoExposure ? "true" : "false") << '}';
            file << std::setprecision(9) << ",\"sdk\":{\"jitter_input_pixels\":[" << s.jitterX << ',' << s.jitterY
                 << "],\"reset\":" << (s.reset ? "true" : "false") << ",\"feature_created\":" << (s.featureCreated ? "true" : "false")
                 << ",\"input_history_reset\":" << (s.inputHistoryReset ? "true" : "false")
                 << ",\"reset_reason_bits\":" << uint32_t(e.inputs.resetReasons) << ",\"reset_reasons\":[";
            bool first = true;
            for (uint32_t bit = 0; bit < 10; ++bit) {
                auto reason = temporal::TemporalResetReason(1u << bit);
                if (temporal::HasResetReason(e.inputs.resetReasons, reason)) {
                    if (!first) file << ','; first = false; JsonString(file, ResetName(reason));
                }
            }
            file << "],\"temporal_epoch\":" << e.inputs.temporalEpoch << ",\"render_subrect\":[" << s.renderWidth << ',' << s.renderHeight
                 << "],\"color_base\":[" << s.colorX << ',' << s.colorY << "],\"depth_base\":[" << s.depthX << ',' << s.depthY
                 << "],\"mv_base\":[" << s.mvX << ',' << s.mvY << "],\"output_base\":[" << s.outputX << ',' << s.outputY
                 << "],\"mv_scale\":[" << s.mvScaleX << ',' << s.mvScaleY << "],\"depth_convention\":" << uint32_t(e.inputs.depthConvention)
                 << ",\"pre_exposure\":" << s.preExposure << ",\"exposure_scale\":" << s.exposureScale << '}';
            file << ",\"stage\":"; JsonString(file, e.stage);
            file << ",\"reason\":"; JsonString(file, e.reason);
            file << ",\"evaluate_called\":" << (e.evaluated ? "true" : "false") << ",\"vendor_result\":";
            if (e.vendorResult) file << *e.vendorResult; else file << "null";
            file << ",\"create_result\":";
            if (e.createResult) file << *e.createResult; else file << "null";
            file << ",\"vendor_success\":" << (e.vendorSuccess ? "true" : "false")
                 << ",\"isolated_accepted\":" << (e.isolatedAccepted ? "true" : "false")
                 << ",\"isolated_included\":" << (e.isolatedIncluded ? "true" : "false")
                 << ",\"composite_succeeded\":" << (e.compositeSucceeded ? "true" : "false")
                 << ",\"adopted\":" << (e.adopted ? "true" : "false")
                 << ",\"checked_submit\":" << (e.checkedSubmit ? "true" : "false")
                 << ",\"submission_serial\":" << e.submissionSerial << ",\"gpu_completed\":" << (e.completed ? "true" : "false")
                 << ",\"omitted\":" << (e.omitted ? "true" : "false")
                 << ",\"scratch_alpha_adopted\":false,\"preview\":\"RGB clamped to 0..1 UNORM8; alpha ignored; float storage alone does not imply HDR\""
                 << ",\"input\":"; JsonImage(file, e.input);
            file << ",\"output\":"; JsonImage(file, e.output); file << '}';
        }
        file << "]}\n"; file.close();
        return ok && !file.fail();
    }
};

// The fixture substitutes only `vendor`; the production and synthetic paths
// share these exact pre/post recording boundaries and a single vendor call.
template<class Vendor>
int32_t InvokeEvaluate(VkCommandBuffer command, Entry* entry, const plume::VulkanTexture& color,
    const plume::VulkanTexture& scratch, const Parameters& parameters, Vendor&& vendor,
    bool (*success)(int32_t)) {
    if (entry) { entry->sdk = parameters; entry->stage = "evaluate"; entry->Before(command, color); entry->evaluated = true; }
    const int32_t result = vendor();
    if (entry) {
        entry->vendorResult = result;
        entry->vendorSuccess = success(result);
        if (entry->vendorSuccess) entry->After(command, scratch);
        else entry->reason = "vendor_failed";
    }
    return result;
}
} // namespace gpu::dlss::capture
#endif
