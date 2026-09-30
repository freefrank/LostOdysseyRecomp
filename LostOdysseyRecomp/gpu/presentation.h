#pragma once
#include <cstdint>
#include <memory>
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
    // Process an opaque, display-encoded pre-UI scene at its actual resolution.
    // Source: sampleable RGBA8 UNORM, at least width x height (top-left crop).
    // Target: distinct, caller-owned RGBA8 UNORM render target, exactly width x
    // height. Alpha is written as 1, matching the existing presentation AA.
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
                           Antialiasing antialiasing);
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
                        bool expandRgbRange = false);
    void Draw(plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *,
              uint32_t, uint32_t, uint32_t, uint32_t, const PresentationOptions &, bool toSwapchain = true);
    void Draw(plume::RenderCommandList *, plume::RenderTexture *, plume::RenderTexture *,
              uint32_t, uint32_t, uint32_t, uint32_t, Antialiasing);
    void Draw(plume::RenderCommandList *commands, plume::RenderTexture *source, plume::RenderTexture *target,
              uint32_t sourceWidth, uint32_t sourceHeight, uint32_t outputWidth, uint32_t outputHeight, bool antialias);
};
} // namespace gpu
