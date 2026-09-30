#pragma once
#include "temporal_math.h"
#include "temporal_gpu_timing.h"
#include <cstdint>
#include <memory>
#include <string>
namespace plume { struct RenderDevice; struct RenderCommandList; struct RenderTexture; }
namespace gpu
{
// The output attachment selects the pipeline format independently of sampled
// color inputs. Default preserves the format selected by Init.
enum class TemporalColorStorage { Default, Rgba8, Rgba16Float };
struct TemporalAAInputs
{
    // Stable coverage mode: currentColor/currentDepth remain jittered raster inputs;
    // output AND historyColor use the unjittered display grid. historyDepth remains
    // raw previous raster depth, addressed separately with previousJitter. Current
    // point samples integrate Halton coverage without recursively filtering history
    // for a stationary camera. Jitter must lie strictly inside (-.5,.5) pixels.
    // False retains the original jittered color/depth history pairing.
    bool stableGrid=false;
    // Opt-in longer accumulation and verified local two-surface coverage exchange
    // for stationary geometric MV. Stable-grid only; no geometry identity proof.
    // Animated shading still relies on the existing color policy.
    bool stabilizeStationaryGeometry=false;
    float stationaryHistoryWeight=31.f/33.f; // [0,.995]; independent of moving history.
    float stationaryMotionMin=.002f, stationaryMotionMax=.125f; // Render pixels.
    bool stationaryCoverage=true; // Independent coverage fallback within stabilization.
    // Experimental fallback for static silhouettes containing three or more
    // locally matched depth layers. Default off pending scene validation.
    bool stationaryMultiSurface=false;
    // If a moving pixel's cubic history footprint includes an extra depth
    // layer, retry only its smaller bilinear core with the usual depth checks.
    bool movingBilinearFallback=false;
    // Experimental debug control: snap only near-zero stable history addressing.
    // The original MV still controls weighting and support validation. Default off.
    bool snapStationaryMotion=false;
    // For valid zero geometric motion only, clamp outlying history instead of
    // discarding it. Moving/reactive/depth-invalid pixels retain existing policy.
    bool stationaryColorClip=false;
    // All textures are single-sample 2D, exact declared dimensions, mip zero.
    // Sampled colors may independently use RGBA8_UNORM or RGBA16_FLOAT.
    // outputStorage must describe the actual output attachment; depths are
    // R32_FLOAT host reverse depth (0 clear). No implicit color-space conversion.
    // Optional current reactive mask: R8_UNORM or R32_FLOAT, >0 rejects history.
    // Optional explicit motion vector: RG16_FLOAT (vx, vy in render pixels).
    plume::RenderTexture *currentColor=nullptr, *currentDepth=nullptr;
    plume::RenderTexture *historyColor=nullptr, *historyDepth=nullptr;
    plume::RenderTexture *motionVector=nullptr, *motionDepths=nullptr;
    bool motionVectorDebug=false;
    bool motionVectorValid=false;
    // stableGrid=false: current jittered raster grid, paired with currentDepth.
    // stableGrid=true: stable display color; retain raw currentDepth separately.
    plume::RenderTexture *output=nullptr, *reactiveMask=nullptr;
    TemporalColorStorage outputStorage=TemporalColorStorage::Default;
    uint32_t width=0, height=0, historyWidth=0, historyHeight=0;
    const temporal::Camera *currentCamera=nullptr, *previousCamera=nullptr;
    // Cameras cover the full respective texture at origin 0,0. Matrices must
    // EXCLUDE the explicit jitter below, but contain all other projection terms.
    // Jitter is actual raster displacement in pixels: +X right, +Y down.
    // Input color AND corresponding depth must have been rendered with this jitter.
    double currentJitterX=0, currentJitterY=0, previousJitterX=0, previousJitterY=0;
    bool historyValid=false;
    // Safe default without trustworthy moving-object/reactive coverage.
    bool rejectAllHistory=true;
    // Optional local color-reactive policy: reject reprojected RGB outside the
    // current 3x3 range by more than one RGBA8 quantization step. This is not an
    // object motion/reactive oracle; in-range animated changes can still persist.
    bool rejectOutOfNeighborhoodHistory=false;
    // Offline diagnostics only; never feed this output into color history.
    // Red=accepted history, green=color-reactive rejection, black=other rejection.
    // Overrides color/alpha output (including reset); normal rendering leaves false.
    bool diagnosticAcceptance=false;
    // With diagnosticAcceptance: geometric rejection reasons blue=reactive,
    // yellow=current/replay depth, black=missing primary, purple=third surface,
    // cyan=other. Default keeps the established red/green/black contract.
    bool diagnosticRejectionReasons=false;
    float historyWeight=.85f; // [0,.95], after 3x3 current RGB neighborhood clamp.
    // Camera reprojection pairs radius-one near/far support. Geometric motion
    // accepts its replayed surface plus one silhouette/background surface in the
    // cubic footprint; a missing primary surface or third depth layer rejects.
    // Depth agreement is
    // previous depth: |actual-predicted| <= absolute + relative*max(actual,predicted).
    // Both are NONLINEAR host-depth units, NOT world-distance or view-Z tolerances.
    // No universal disocclusion acceptance is implied; caller must calibrate inputs.
    float depthAbsoluteThreshold=1e-5f, depthRelativeThreshold=.01f;
};

struct TemporalDisplayInputs
{
    // Display-only output must not become history with the original jittered depth.
    // Exact-size RGBA8_UNORM or RGBA16_FLOAT textures, no aliasing. A zero-jitter
    // FP16-to-RGBA8 draw quantizes only this display output, never the history.
    plume::RenderTexture *jitteredColor=nullptr, *output=nullptr;
    uint32_t width=0, height=0;
    double jitterX=0, jitterY=0; // Actual raster displacement of this accumulation.
    TemporalColorStorage outputStorage=TemporalColorStorage::Default;
};

class TemporalAA
{
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    void EnableGpuTiming(bool enabled);
    const temporal::GpuPassTimingStats& ResolveTiming() const;
    const temporal::GpuPassTimingStats& DisplayTiming() const;
    TemporalAA();
    ~TemporalAA();
    bool Init(plume::RenderDevice* device);
    // Select default output storage: false=RGBA8 UNORM, true=RGBA16 FLOAT.
    // Per-draw outputStorage may override it; both pipeline variants remain alive
    // for the instance so a live switch cannot invalidate recorded commands.
    bool Init(plume::RenderDevice* device, bool hdrColor);
    const std::string& LastError() const;
    // Caller transitions every bound input to SHADER_READ and output to COLOR_WRITE
    // BEFORE recording; no implicit UNKNOWN/source-layout assumption or barrier here.
    // Output remains COLOR_WRITE. Caller transitions it for subsequent use.
    // All input/output textures must outlive execution; no aliasing output with inputs.
    // No history ownership, frame identity, jitter generation, or queue submission.
    // False records no draw and leaves output unchanged. Reset stores current RGBA
    // in the requested output format and permits absent history/depth/cameras. Plume cannot query texture
    // descriptions, so dimensions/formats/layout preconditions are caller obligations.
    bool Resolve(plume::RenderCommandList*, const TemporalAAInputs&);
    // Stable display p samples accumulation at p+jitter with separable Catmull-Rom
    // reconstruction, bounded by source footprint extrema. Border extension clamps
    // to edge texel centers; no undefined
    // samples. Zero jitter is exact RGBA Load; nonzero jitter reconstructs alpha.
    // History/depth are untouched. Same explicit layouts/fence contracts as Resolve:
    // input SHADER_READ, output remains COLOR_WRITE, false records no draw.
    bool ReconstructDisplay(plume::RenderCommandList*, const TemporalDisplayInputs&);
    // Only after ALL earlier Resolve/ReconstructDisplay submissions finish their
    // fences (or are discarded).
    // Distinct descriptor/framebuffer allocations are kept until this call; destructor
    // and re-Init have the same completion requirement. Single recording thread only.
    void ReleaseCompleted();
    // Snapshot after recording a batch; release only after that batch's fence completes.
    uint64_t RecordedSerial() const;
    // Mark external commands using owned resources (e.g. depth copies) on the same fence timeline.
    void RecordExternalUse();
    void ReleaseCompletedThrough(uint64_t serial);
};
}
