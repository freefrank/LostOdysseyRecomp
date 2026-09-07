#pragma once
#include "temporal_math.h"
#include <cstdint>
#include <memory>
#include <string>
namespace plume { struct RenderDevice; struct RenderCommandList; struct RenderTexture; }
namespace gpu
{
struct TemporalAAInputs
{
    // Stable coverage mode: currentColor/currentDepth remain jittered raster inputs;
    // output AND historyColor use the unjittered display grid. historyDepth remains
    // raw previous raster depth, addressed separately with previousJitter. Current
    // point samples integrate Halton coverage without recursively filtering history
    // for a stationary camera. Jitter must lie strictly inside (-.5,.5) pixels.
    // False retains the original jittered color/depth history pairing.
    bool stableGrid=false;
    // All textures are single-sample 2D, exact declared dimensions, mip zero.
    // Colors/output: RGBA8_UNORM, depths: R32_FLOAT host reverse depth (0 clear).
    // Optional current reactive mask: R8_UNORM or R32_FLOAT, >0 rejects history.
    plume::RenderTexture *currentColor=nullptr, *currentDepth=nullptr;
    plume::RenderTexture *historyColor=nullptr, *historyDepth=nullptr;
    // stableGrid=false: current jittered raster grid, paired with currentDepth.
    // stableGrid=true: stable display color; retain raw currentDepth separately.
    plume::RenderTexture *output=nullptr, *reactiveMask=nullptr;
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
    float historyWeight=.85f; // [0,.95], after 3x3 current RGB neighborhood clamp.
    // Legacy mode validates every cubic-footprint depth against predicted depth.
    // Stable mode pairs radius-one near/far support and validates each nonzero
    // color tap against those two surfaces on the separate raw depth grid.
    // Depth agreement is
    // previous depth: |actual-predicted| <= absolute + relative*max(actual,predicted).
    // Both are NONLINEAR host-depth units, NOT world-distance or view-Z tolerances.
    // No universal disocclusion acceptance is implied; caller must calibrate inputs.
    float depthAbsoluteThreshold=1e-5f, depthRelativeThreshold=.01f;
};

struct TemporalDisplayInputs
{
    // Display-only output must not become history with the original jittered depth.
    // Both textures: exact-size RGBA8_UNORM, no aliasing.
    plume::RenderTexture *jitteredColor=nullptr, *output=nullptr;
    uint32_t width=0, height=0;
    double jitterX=0, jitterY=0; // Actual raster displacement of this accumulation.
};

class TemporalAA
{
    struct Impl;
    std::unique_ptr<Impl> impl;
public:
    TemporalAA();
    ~TemporalAA();
    bool Init(plume::RenderDevice* device);
    const std::string& LastError() const;
    // Caller transitions every bound input to SHADER_READ and output to COLOR_WRITE
    // BEFORE recording; no implicit UNKNOWN/source-layout assumption or barrier here.
    // Output remains COLOR_WRITE. Caller transitions it for subsequent use.
    // All input/output textures must outlive execution; no aliasing output with inputs.
    // No history ownership, frame identity, jitter generation, or queue submission.
    // False records no draw and leaves output unchanged. Reset copies current RGBA
    // exactly and permits absent history/depth/cameras. Plume cannot query texture
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
};
}
