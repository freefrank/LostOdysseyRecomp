#pragma once
#include <cstdint>
#include <memory>
#include "hdr_output.h"
namespace plume
{
struct RenderDevice;
struct RenderCommandList;
struct RenderTexture;
enum class RenderFormat;
} // namespace plume
namespace gpu
{
enum class Antialiasing : uint32_t { Off, FXAA, SMAA, TAA };
enum class ScalingFilter : uint32_t { Bilinear, Bicubic };
struct PresentationOptions
{
    Antialiasing antialiasing = Antialiasing::Off;
    ScalingFilter scalingFilter = ScalingFilter::Bilinear;
    bool expandRgbRange = false;
    // Extended gamma-2.2 scene including the original UI/fade suffix.
    bool hdrScene = false;
    // Normalized rectangle in the complete source image. Calibration bypasses
    // the scene highlight shoulder and writes known HDR levels at the final pass.
    bool hdrCalibration = false;
    float calibrationRect[4] = {};
    // Optional frozen extended-gamma scene, cropped to its valid extent. The
    // owner retains it through the present fence. Left is an SDR luminance
    // preview; right uses the same highlight mapping as normal HDR gameplay.
    plume::RenderTexture* calibrationScene = nullptr;
    bool calibrationExpandRgbRange = false;
    // Apply the guest's display gamma ramp at the final pass, before the RGB
    // range expansion and any HDR output conversion (#179). Last, so positional
    // initializers of the fields above keep their meaning.
    bool displayGammaRamp = false;
    // The frozen calibration scene gets the same ramp as gameplay.
    bool calibrationDisplayGammaRamp = false;
    // Player brightness (black level) and gamma, applied after the RGB range
    // expansion (settings::Config::displayBrightness / displayGamma). Only a
    // draw that sets displayAdjust, brightnessPreview or hdrCalibration loads
    // these values; other draws leave the current curve in place.
    int displayBrightness = 0;
    uint32_t displayGamma = 100;
    // The final pass applies the curve to the game image.
    bool displayAdjust = false;
    // calibrationRect compares the game image (left) with the curve applied
    // (right): calibrationScene when set, otherwise a grey test pattern.
    // calibrationSceneExtended marks an FP16 extended-gamma scene.
    bool brightnessPreview = false;
    bool calibrationSceneExtended = false;
    // With brightnessPreview: calibrationScene holds two images side by side,
    // the left one for the left tile and the right one for the right tile, and
    // neither gets the curve (the DLSS 5 neural rendering page).
    bool calibrationSplitScene = false;
};
// Owned by the presentation thread. Resources stay alive until the present fence.
class Presentation
{
    struct Impl;
    std::unique_ptr<Impl> impl;

  public:
    struct UiCompositionLease;
    Presentation();
    ~Presentation();
    bool Init(plume::RenderDevice *device);
    bool Init(plume::RenderDevice *device, plume::RenderFormat swapchainFormat);
    void SetOutputTransform(const hdr::OutputTransform& transform);
    // Draw and ProcessSceneColor keep one framebuffer per caller-owned target,
    // keyed by texture address and size. Call this, with no recorded draw of this
    // instance pending, before targets are replaced without a size change (swap
    // chain resize or recreation).
    void ForgetTargets();
    // Process an opaque, display-encoded pre-UI scene at its actual resolution.
    // Source: sampleable RGBA8 UNORM, at least width x height (top-left crop).
    // Target: distinct, caller-owned RGBA8 UNORM render target, exactly width x
    // height. Alpha is written as 1, matching the existing presentation AA.
    // hdr: source and target are instead R16G16B16A16_FLOAT extended-gamma
    // scenes; the passes keep values above 1 and decide edges on the SDR range.
    // Records transitions; both textures finish in SHADER_READ. Invalid basic
    // arguments return false without recording commands; texture properties are
    // caller preconditions because RenderTexture exposes no description query.
    // Use a separate instance from final presentation. ALL prior commands using
    // this instance must complete before another call, resize or destruction;
    // alternatively use independent instances and targets per in-flight slot.
    // Caller must restore graphics pipeline, framebuffer, descriptors, viewport
    // and scissor before resuming guest draws, and retain textures through fence.
    bool ProcessSceneColor(plume::RenderCommandList *commands, plume::RenderTexture *source,
                           plume::RenderTexture *target, uint32_t width, uint32_t height,
                           Antialiasing antialiasing, bool hdr = false);
    // Synthesize an extended-gamma FP16 frame from the final SDR frame (any
    // upscaler, UI included) and the pre-upscale FP16 scene: per channel
    // gain = max(hdr, 1) sampled bilinearly, weighted by the pixel's own
    // brightness so only near-clipping pixels are expanded. Returns an owned
    // output-size texture in SHADER_READ, valid until the next present fence,
    // or nullptr. hdrValid is the scene's valid extent inside hdrAllocation.
    plume::RenderTexture* ComposeHdrGain(plume::RenderCommandList *commands, plume::RenderTexture *sdr,
                                         plume::RenderTexture *hdr, uint32_t width, uint32_t height,
                                         uint32_t hdrValidWidth, uint32_t hdrValidHeight,
                                         uint32_t hdrAllocationWidth, uint32_t hdrAllocationHeight);
    // Composite an independently produced straight-alpha UI image over a
    // HUD-less output-resolution scene. This performs no AA or scaling and is
    // intended for the final real/generated-frame presentation stage. Source
    // images finish SHADER_READ; target finishes COLOR_WRITE for the caller's
    // normal present transition. All three images must cover width x height.
    // Keep the returned lease, source/target images, and this Presentation alive
    // until the recorded GPU work completes; do not reinitialize Presentation
    // while a recorded composition is pending. An empty lease means no draw was
    // recorded. Each call has independent descriptors and framebuffer.
    [[nodiscard]] std::shared_ptr<UiCompositionLease> DrawSeparatedUi(
        plume::RenderCommandList *commands, plume::RenderTexture *hudless,
        plume::RenderTexture *uiColorAndAlpha, plume::RenderTexture *target,
        uint32_t width, uint32_t height, bool toSwapchain = true);
    // Present the scene + subsequently composited UI without applying AA again.
    // Same ownership/layout contract as Draw (source ends COPY_SOURCE).
    void DrawComposited(plume::RenderCommandList *commands, plume::RenderTexture *source,
                        plume::RenderTexture *target, uint32_t sourceWidth, uint32_t sourceHeight,
                        uint32_t outputWidth, uint32_t outputHeight, ScalingFilter scalingFilter,
                        bool expandRgbRange = false, bool displayGammaRamp = false);
    void Draw(plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *,
              uint32_t, uint32_t, uint32_t, uint32_t, const PresentationOptions &, bool toSwapchain = true);
    void Draw(plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *,
              uint32_t, uint32_t, uint32_t, uint32_t, Antialiasing);
    void Draw(plume::RenderCommandList *commands, plume::RenderTexture *source, plume::RenderTexture *target,
              uint32_t sourceWidth, uint32_t sourceHeight, uint32_t outputWidth, uint32_t outputHeight, bool antialias);
};
} // namespace gpu
