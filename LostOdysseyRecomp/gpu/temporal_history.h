#pragma once
#include "motion_vector.h"
#include "motion_frame.h"
#include "sr_hybrid_motion_gpu.h"
#include "temporal_frame_inputs.h"
#include "temporal_input_capture_failure.h"
#include "temporal_aa.h"
#include "temporal_scene.h"
#include "taa_live_control.h"
#include "sr_compatibility.h"
#include <cstdlib>
#include <cstdio>

namespace gpu::temporal {
struct HistoryContinuityDepthRange {
    std::array<double,3> depths{.001,.01,.1};
    double lowerBound=0,farWorldW=0,nearWorldW=0;
    bool valid=false;
};
inline HistoryContinuityDepthRange SelectHistoryContinuityDepths(const Camera& current) {
    HistoryContinuityDepthRange out;
    const auto& r=current.Raster();
    // Match the guard's existing raster sample. SceneObservation requires the
    // full zero-origin viewport; retain its half-pixel and NDC-Y conventions.
    const double nx=2*((r.width*.5-r.x)/r.width)-1-r.halfPixelNdcX;
    const double ny=(1-2*((r.height*.5-r.y)/r.height)-r.halfPixelNdcY)/r.ndcYSign;
    const auto farPoint=Transform({nx,ny,1,1},current.InverseVP());
    const auto nearPoint=Transform({nx,ny,0,1},current.InverseVP());
    out.farWorldW=farPoint[3];out.nearWorldW=nearPoint[3];
    // Reversed depth: d=1 is the near endpoint. A missing/unsafe near endpoint
    // cannot establish a usable ray, even if some intermediate point is valid.
    if(!ValidW(nearPoint)||!std::all_of(farPoint.begin(),farPoint.end(),[](double v){return std::isfinite(v);}))return out;
    if(farPoint[3]<=0) {
        // Homogeneous W is affine in depth. Some guest projections put infinity
        // at d~.001, so a fixed .001 probe can lie behind the camera even when
        // current and previous VP are identical. Sample the positive-W interval.
        const double slope=nearPoint[3]-farPoint[3];
        if(!std::isfinite(slope)||slope<=0)return out;
        out.lowerBound=-farPoint[3]/slope;
        if(!std::isfinite(out.lowerBound)||out.lowerBound<0||out.lowerBound>=1)return out;
        for(double& depth:out.depths)depth=out.lowerBound+(1-out.lowerBound)*depth;
    }
    // No pole in [0,1]: keep the original depths exactly. Reproject still applies
    // its current/previous W, depth and bounds guards to every selected point.
    out.valid=true;return out;
}
inline bool ContinuousHistoryCamera(const Camera& current,const Camera& previous) {
    // A practical camera-cut guard, not a guest scene identity oracle. Reject
    // > quarter-screen displacement at center across three valid depth slices.
    const auto depths=SelectHistoryContinuityDepths(current);
    if(!depths.valid)return false;
    const auto& r=current.Raster();
    for(double depth:depths.depths) {
        const auto p=Reproject({r.width*.5,r.height*.5,depth},current,previous);
        if(!p||std::abs(p.previous.x-r.width*.5)>r.width*.25||std::abs(p.previous.y-r.height*.5)>r.height*.25)return false;
    }
    return true;
}
// Read-only diagnostics. These classifications never authorize history reuse.
enum class HistoryReuseRejection : uint32_t {
    InvalidHistory=1u<<0, PreviousIncomplete=1u<<1, StableGridChanged=1u<<2,
    FrameDiscontinuity=1u<<3, EpochChanged=1u<<4, PreviousCameraMissing=1u<<5,
    DepthAllocationChanged=1u<<6, RasterChanged=1u<<7, CameraDiscontinuity=1u<<8,
    HistoryDisabled=1u<<9
};
inline const char* HistoryReuseRejectionName(HistoryReuseRejection reason) {
    switch(reason) {
    case HistoryReuseRejection::InvalidHistory:return "invalid_history";
    case HistoryReuseRejection::PreviousIncomplete:return "previous_incomplete";
    case HistoryReuseRejection::StableGridChanged:return "stable_grid_changed";
    case HistoryReuseRejection::FrameDiscontinuity:return "frame_discontinuity";
    case HistoryReuseRejection::EpochChanged:return "epoch_changed";
    case HistoryReuseRejection::PreviousCameraMissing:return "previous_camera_missing";
    case HistoryReuseRejection::DepthAllocationChanged:return "depth_allocation_changed";
    case HistoryReuseRejection::RasterChanged:return "raster_changed";
    case HistoryReuseRejection::CameraDiscontinuity:return "camera_discontinuity";
    case HistoryReuseRejection::HistoryDisabled:return "history_disabled";
    }
    return "unknown";
}
struct HistoryReuseState {
    bool valid=false,previousCompleted=false,currentStable=false,previousStable=false,allowHistory=false;
    uint64_t currentFrame=0,previousFrame=0,currentEpoch=0,previousEpoch=0;
    uint64_t currentAllocation=0,previousAllocation=0;
};
struct HistoryContinuityProbe {
    double depth=0,deltaXFraction=0,deltaYFraction=0;
    Rejection rejection=Rejection::InvalidSample;
    Sample projected{};
    bool projectedValid=false,quarterScreenRejected=false;
};
struct HistoryReuseDiagnostic {
    bool captured=false,cameraChecksAvailable=false;
    uint32_t rejected=0;
    HistoryReuseState state{};
    std::optional<Camera> currentCamera,previousCamera;
    HistoryContinuityDepthRange depthRange{};
    std::array<HistoryContinuityProbe,3> probes{};
};
inline HistoryReuseDiagnostic InspectHistoryReuse(const HistoryReuseState& state,const Camera* current,const Camera* previous) {
    HistoryReuseDiagnostic out;out.captured=true;out.state=state;
    if(current)out.currentCamera=*current;
    if(previous)out.previousCamera=*previous;
    const auto reject=[&](bool condition,HistoryReuseRejection reason){if(condition)out.rejected|=uint32_t(reason);};
    reject(!state.valid,HistoryReuseRejection::InvalidHistory);
    reject(!state.previousCompleted,HistoryReuseRejection::PreviousIncomplete);
    reject(state.currentStable!=state.previousStable,HistoryReuseRejection::StableGridChanged);
    reject(state.previousFrame+1!=state.currentFrame,HistoryReuseRejection::FrameDiscontinuity);
    reject(state.previousEpoch!=state.currentEpoch,HistoryReuseRejection::EpochChanged);
    reject(!previous,HistoryReuseRejection::PreviousCameraMissing);
    reject(state.previousAllocation!=state.currentAllocation,HistoryReuseRejection::DepthAllocationChanged);
    reject(!state.allowHistory,HistoryReuseRejection::HistoryDisabled);
    if(!current||!previous)return out;
    out.cameraChecksAvailable=true;
    const auto& c=current->Raster();const auto& p=previous->Raster();
    reject(c.x!=p.x||c.y!=p.y||c.width!=p.width||c.height!=p.height||c.ndcYSign!=p.ndcYSign||
        c.halfPixelNdcX!=p.halfPixelNdcX||c.halfPixelNdcY!=p.halfPixelNdcY,HistoryReuseRejection::RasterChanged);
    out.depthRange=SelectHistoryContinuityDepths(*current);
    if(!out.depthRange.valid) {
        reject(true,HistoryReuseRejection::CameraDiscontinuity);return out;
    }
    unsigned index=0;
    for(double depth:out.depthRange.depths) {
        auto& probe=out.probes[index++];probe.depth=depth;
        const Sample sample{c.width*.5,c.height*.5,depth};
        const auto result=Reproject(sample,*current,*previous);probe.rejection=result.rejection;
        // Reproject intentionally returns no coordinates on rejection. Record
        // the unclipped projection as well to explain depth/outside failures.
        const double nx=2*((sample.x-c.x)/c.width)-1-c.halfPixelNdcX;
        const double ny=(1-2*((sample.y-c.y)/c.height)-c.halfPixelNdcY)/c.ndcYSign;
        Vector world=Transform({nx,ny,1-depth,1},current->InverseVP());
        if(ValidW(world)) {
            const double w=world[3];for(double& value:world)value/=w;
            const auto clip=Transform(world,previous->VP());
            if(ValidW(clip)) {
                probe.projected={p.x+(clip[0]/clip[3]+p.halfPixelNdcX+1)*.5*p.width,
                    p.y+(1-(p.ndcYSign*clip[1]/clip[3]+p.halfPixelNdcY))*.5*p.height,1-clip[2]/clip[3]};
                probe.projectedValid=std::isfinite(probe.projected.x)&&std::isfinite(probe.projected.y)&&std::isfinite(probe.projected.depth);
            }
        }
        if(probe.projectedValid) {
            probe.deltaXFraction=(probe.projected.x-sample.x)/c.width;
            probe.deltaYFraction=(probe.projected.y-sample.y)/c.height;
            probe.quarterScreenRejected=std::abs(probe.projected.x-sample.x)>c.width*.25||std::abs(probe.projected.y-sample.y)>c.height*.25;
        }
        reject(!result||probe.quarterScreenRejected,HistoryReuseRejection::CameraDiscontinuity);
    }
    return out;
}
}
#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
#include "temporal_collection_gpu.h"
#include <vector>

namespace gpu::temporal {
// One recording thread / one ordered queue. Caller retains external sources until
// submitted GPU work completes and calls ReleaseCompleted only after that fence.
// Copies are owned: guest resolve addresses and recycled RT pointers are not history.
class HistoryOwner {
    struct Image {
        std::unique_ptr<plume::RenderTexture> texture;
        plume::RenderTextureLayout layout=plume::RenderTextureLayout::UNKNOWN;
    };
    struct Frame {
        uint64_t number=~0ull,epoch=0,depthOrdinal=0,colorOrdinal=0,allocation=0;
        std::optional<Camera> camera;
        double jx=0,jy=0;
        bool completed=false,stableGrid=false,inputsComplete=false,taaResolved=false;
        ColorEncoding colorEncoding=ColorEncoding::Sdr;
        bool colorEncodingAssumed=false;
        TemporalResetReason inputReset=TemporalResetReason::FirstFrame;
        frame_plan::FramePlan plan{};
        JitterSample jitter{};
    };
    plume::RenderDevice* device_=nullptr;
    TemporalAA aa_;
    std::shared_ptr<taa_collection::SparseDepthGPU> sparse_;
    uint64_t sparseReleaseSerial_=0;
    std::array<Image,2> depth_,history_;
    Image source_,display_;
    MotionFrameView motionView_{};
    SrHybridMotionGPU hybridMotion_;
    bool hybridAttempted_ = false;
    struct RetiredImage { uint64_t serial; std::unique_ptr<plume::RenderTexture> texture; };
    std::vector<RetiredImage> retired_;
    std::array<Frame,2> frames_;
    uint32_t width_=0,height_=0;
    uint32_t storageWidth_=0,storageHeight_=0;
    uint64_t frame_=~0ull,epoch_=0;
    bool valid_=false,reused_=false;
    bool motionVectorValid_=false;
    bool aaInitialized_=false;
    InputCaptureFailure captureFailure_=InputCaptureFailure::None;
    // Reset() may run again mid-frame; only a new BeginFrame clears this cause.
    InputCaptureFailure frameResourceFailure_=InputCaptureFailure::None;
    uint32_t testFailOwnedAllocationAt_=0, testOwnedAllocationAttempt_=0;
    TemporalResetReason pendingReset_=TemporalResetReason::None;
    bool diagnosticsEnabled_=false;
    plume::RenderFormat sourceFormat_=plume::RenderFormat::R8G8B8A8_UNORM;
    plume::RenderFormat historyFormat_=plume::RenderFormat::R8G8B8A8_UNORM;
    HistoryReuseDiagnostic diagnostics_;
    static void Transition(plume::RenderCommandList* commands,Image& image,plume::RenderTextureLayout layout) {
        if(image.layout!=layout) {commands->barriers(plume::RenderBarrierStage::ALL,plume::RenderTextureBarrier(image.texture.get(),layout));image.layout=layout;}
    }
    bool Allocate(Image& image,plume::RenderFormat format) {
        if(image.texture)retired_.push_back({aa_.RecordedSerial(),std::move(image.texture)});
        image.layout=plume::RenderTextureLayout::UNKNOWN;
        // Instance-local failure seam after normal retirement, before the real
        // device allocation. Unused in production unless a fixture arms it.
        if(testFailOwnedAllocationAt_ && ++testOwnedAllocationAttempt_ == testFailOwnedAllocationAt_) {
            testFailOwnedAllocationAt_=0; return false;
        }
        image.texture=device_->createTexture(plume::RenderTextureDesc::Texture2D(width_,height_,1,format,plume::RenderTextureFlag::RENDER_TARGET));
        return bool(image.texture);
    }
    bool SetHistoryStorage(bool fp16) {
        // HDR already owns FP16 source/history. The SDR precision experiment
        // changes only accumulation and display storage, never the copied source.
        const auto format=(fp16||sourceFormat_==plume::RenderFormat::R16G16B16A16_FLOAT)?
            plume::RenderFormat::R16G16B16A16_FLOAT:plume::RenderFormat::R8G8B8A8_UNORM;
        if(format==historyFormat_&&storageWidth_==width_&&storageHeight_==height_&&
            history_[0].texture&&history_[1].texture&&display_.texture)return true;
        std::array<Image,2> nextHistory;Image nextDisplay;
        for(auto& image:nextHistory)if(!Allocate(image,format))return false;
        if(!Allocate(nextDisplay,format))return false;
        // Retain attachments through the last recorded use, including pending
        // framebuffers and external scene-copy draws sharing that fence timeline.
        const auto serial=aa_.RecordedSerial();
        for(auto& image:history_)if(image.texture)retired_.push_back({serial,std::move(image.texture)});
        if(display_.texture)retired_.push_back({serial,std::move(display_.texture)});
        history_=std::move(nextHistory);display_=std::move(nextDisplay);historyFormat_=format;
        storageWidth_=width_;storageHeight_=height_;
        // History allocations changed, while this frame's captured inputs remain
        // valid. Do not erase them between CaptureDepth and ResolveColor.
        valid_=false;for(auto& frame:frames_)frame.completed=frame.taaResolved=false;return true;
    }
    TemporalColorStorage OutputStorage() const {
        return historyFormat_==plume::RenderFormat::R16G16B16A16_FLOAT?
            TemporalColorStorage::Rgba16Float:TemporalColorStorage::Rgba8;
    }
    static bool SameRaster(const Camera& a,const Camera& b) {
        const auto& x=a.Raster();const auto& y=b.Raster();
        return x.x==y.x&&x.y==y.y&&x.width==y.width&&x.height==y.height&&x.ndcYSign==y.ndcYSign&&x.halfPixelNdcX==y.halfPixelNdcX&&x.halfPixelNdcY==y.halfPixelNdcY;
    }
    static bool SameInputConfiguration(const frame_plan::FramePlan& a,const frame_plan::FramePlan& b) {
        return a.geometryEpoch==b.geometryEpoch&&a.width==b.width&&a.height==b.height&&
            a.requestSignature==b.requestSignature&&a.deviceEpoch==b.deviceEpoch&&a.sizingRevision==b.sizingRevision&&
            a.output==b.output&&a.legacyWidth==b.legacyWidth&&a.legacyHeight==b.legacyHeight&&
            a.requestedUpscaler==b.requestedUpscaler&&
            upscaling::SameEffectiveQuality(a.requestedUpscaler,a.dlssQuality,b.dlssQuality,a.fsrQuality,b.fsrQuality)&&
            a.frameGeneration==b.frameGeneration&&a.legacyAA==b.legacyAA&&
            a.effectiveAA==b.effectiveAA&&a.scalingQuality==b.scalingQuality&&a.consumer==b.consumer&&
            a.requiresReadback==b.requiresReadback&&a.inputProbe==b.inputProbe;
    }
public:
    // The instance selects the source domain, which never changes at runtime.
    // SDR may independently retain FP16 history; HDR stays FP16 throughout.
    bool Init(plume::RenderDevice* device,std::shared_ptr<taa_collection::SparseDepthGPU> sparse={},bool hdrColor=false) {
        device_=device;sparse_=std::move(sparse);
        sourceFormat_=historyFormat_=hdrColor?plume::RenderFormat::R16G16B16A16_FLOAT:plume::RenderFormat::R8G8B8A8_UNORM;
        captureFailure_=InputCaptureFailure::None;
        // Input-only DLSS collection owns only current depth/color. TAA shaders,
        // history, and display storage remain lazy until ResolveColor is selected.
        return device_!=nullptr;
    }
    void EnableGpuTiming(bool enabled) {aa_.EnableGpuTiming(enabled);}
    const GpuPassTimingStats& ResolveTiming() const {return aa_.ResolveTiming();}
    const GpuPassTimingStats& DisplayTiming() const {return aa_.DisplayTiming();}
    void Reset() {valid_=false;motionVectorValid_=false;motionView_={};captureFailure_=InputCaptureFailure::None;for(auto& frame:frames_)frame.completed=frame.inputsComplete=frame.taaResolved=false;}
    bool MotionVectorValid() const { return motionVectorValid_; }
    InputCaptureFailure LastInputCaptureFailure() const {
        return frameResourceFailure_ != InputCaptureFailure::None ? frameResourceFailure_ : captureFailure_;
    }
    // Test-only: 1/2 = owned depth buffers, 3 = owned scene color.
    void InjectOwnedCaptureAllocationFailureForTest(uint32_t allocationOrdinal) {
        testFailOwnedAllocationAt_=allocationOrdinal;testOwnedAllocationAttempt_=0;
    }
    uint32_t InputWidth() const { return width_; }
    uint32_t InputHeight() const { return height_; }
    uint64_t InputFrame() const { return frame_; }
    uint64_t InputEpoch() const { return epoch_; }
    uint64_t CapturedDepthOrdinal() const { return frames_[frame_%2].depthOrdinal; }
    uint64_t CapturedDepthAllocation() const { return frames_[frame_%2].allocation; }
    bool HasCapturedCamera() const { return bool(frames_[frame_%2].camera); }
    // Capture-only snapshots of the cameras that own this frame's depth input.
    std::optional<Camera> CurrentCameraForCapture() const {
        const auto& current = frames_[frame_%2];
        return current.number == frame_ && current.epoch == epoch_ ? current.camera : std::nullopt;
    }
    std::optional<Camera> PreviousCameraForCapture() const {
        const auto& previous = frames_[(frame_+1)%2];
        return previous.number + 1 == frame_ && previous.epoch == epoch_ ? previous.camera : std::nullopt;
    }
    uint64_t CapturedColorOrdinal() const { return frames_[frame_%2].colorOrdinal; }
    // External passes sampling our owned depth join THIS owner's submission serial.
    void RecordExternalRead() { aa_.RecordExternalUse(); hybridMotion_.RecordConsumerUse(aa_.RecordedSerial()); }
    bool HybridResourceFailed() const { return hybridAttempted_ && !motionVectorValid_ && hybridMotion_.ResourceFailed(); }
    // Frame identity is supplied by renderer, never CPU presented-swap count.
    void BeginFrame(uint64_t frame,uint64_t epoch,bool diagnostics=false) {
        diagnosticsEnabled_=diagnostics;
        if(frame_==frame&&epoch_==epoch)return;
        if(frame_+1!=frame||epoch_!=epoch) {
            pendingReset_=pendingReset_|(frame_+1!=frame?TemporalResetReason::FrameDiscontinuity:TemporalResetReason::None)|
                (epoch_!=epoch?TemporalResetReason::EpochChanged:TemporalResetReason::None);
            Reset();
        }
        if(frame_!=frame)frameResourceFailure_=InputCaptureFailure::None;
        frame_=frame;epoch_=epoch;frames_[frame%2]=Frame{};reused_=false;
        diagnostics_={};motionView_={};motionVectorValid_=false;captureFailure_=InputCaptureFailure::None;
    }
    bool CaptureDepth(plume::RenderCommandList* commands,plume::RenderTexture* source,const SceneObservation& scene) {
        const auto& d=scene.Depth();auto& current=frames_[frame_%2];
        if(frameResourceFailure_!=InputCaptureFailure::None)return false;
        if(!commands||!source||!scene.Draws()||scene.Reason()!=SceneObservation::Rejection::None||d.frame!=frame_||!d.ordinal||!d.fullExtent||current.depthOrdinal)return false;
        if(width_!=d.width||height_!=d.height) {
            pendingReset_=pendingReset_|TemporalResetReason::ExtentChanged;
            Reset();width_=d.width;height_=d.height;
            bool ok=width_&&height_&&width_<=16384&&height_<=16384;
            if(!ok){frameResourceFailure_=InputCaptureFailure::OwnedExtentInvalid;return false;}
            for(auto& image:depth_)if(!Allocate(image,plume::RenderFormat::R32_FLOAT)) {
                ok=false;
                if(frameResourceFailure_==InputCaptureFailure::None)
                    frameResourceFailure_=InputCaptureFailure::OwnedDepthAllocationFailed;
            }
            if(!Allocate(source_,sourceFormat_)) {
                ok=false;
                if(frameResourceFailure_==InputCaptureFailure::None)
                    frameResourceFailure_=InputCaptureFailure::OwnedColorAllocationFailed;
            }
            if(!ok){width_=height_=0;return false;}
        }
        Matrix vp{};for(unsigned i=0;i<16;++i)vp[i]=std::bit_cast<float>(scene.Anchor().vpBits[i]);
        current.camera=Camera::Create(vp,scene.Anchor().viewport);if(!current.camera)return false;
        current.number=frame_;current.epoch=epoch_;current.allocation=scene.Anchor().depthAllocation;current.depthOrdinal=d.ordinal;
        // Caller has explicitly transitioned external source to COPY_SOURCE.
        auto& destination=depth_[frame_%2];Transition(commands,destination,plume::RenderTextureLayout::COPY_DEST);
        commands->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(destination.texture.get()),plume::RenderTextureCopyLocation::Subresource(source));
        Transition(commands,destination,plume::RenderTextureLayout::SHADER_READ);
        aa_.RecordExternalUse();return true;
    }
    // Input-only consumers use the exact pre-TAA color boundary without causing
    // a legacy resolve. This advances input history independently of taaResolved.
    bool CaptureColorInputs(plume::RenderCommandList* commands, plume::RenderTexture* source,
        const SceneObservation& scene, const frame_plan::FramePlan& plan, const JitterSample& jitter, ColorEncoding encoding,
        const MotionFrameView* motion = nullptr, TemporalResetReason reset = TemporalResetReason::None, bool allowHybrid = false) {
        auto& current=frames_[frame_%2];const auto& previous=frames_[(frame_+1)%2];
        captureFailure_=InputCaptureFailure::None; hybridAttempted_=false;
        if(frameResourceFailure_!=InputCaptureFailure::None) {captureFailure_=frameResourceFailure_;return false;}
        if(!commands) { captureFailure_=InputCaptureFailure::MissingCommands; return false; }
        if(!source) { captureFailure_=InputCaptureFailure::MissingSource; return false; }
        if(!scene.Ready()) { captureFailure_=InputCaptureFailure::SceneNotReady; return false; }
        if(scene.Frame()!=frame_) { captureFailure_=InputCaptureFailure::SceneFrameMismatch; return false; }
        if(!current.camera) { captureFailure_=InputCaptureFailure::MissingCamera; return false; }
        if(current.depthOrdinal!=scene.Depth().ordinal) { captureFailure_=InputCaptureFailure::DepthOrdinalMismatch; return false; }
        if(current.colorOrdinal) { captureFailure_=InputCaptureFailure::ColorAlreadyCaptured; return false; }
        if(scene.Color().width!=width_||scene.Color().height!=height_) { captureFailure_=InputCaptureFailure::SceneColorExtentMismatch; return false; }
        if(!plan.width||!plan.height) { captureFailure_=InputCaptureFailure::PlanExtentInvalid; return false; }
        if(plan.width!=width_||plan.height!=height_) { captureFailure_=InputCaptureFailure::PlanExtentMismatch; return false; }
        // The renderer has already selected a complete scene-copy boundary and
        // this owner has its matching camera/depth. Missing tonemap certification
        // may lower image quality; it need not prevent an SR bring-up attempt.
        // The caller's existing SourceFormat/scene-rectangle copy contract stays
        // in force. RenderTexture is abstract; do not read native backend fields
        // through it or guess a transfer function from an allocation descriptor.
        const bool assumeSdr = sr_compatibility::Enabled() && allowHybrid &&
            encoding == ColorEncoding::Unknown &&
            upscaling::MatchesSrProvider(plan.requestedUpscaler,plan.consumer) &&
            plan.frameGeneration == upscaling::FrameGeneration::Off && !plan.requiresReadback &&
            sourceFormat_ == plume::RenderFormat::R8G8B8A8_UNORM;
        if (assumeSdr) {
            encoding = ColorEncoding::Sdr;
            if (!previous.colorEncodingAssumed || previous.number + 1 != frame_)
                std::fprintf(stderr,
                    "SR compatibility: provider=%u frame=%llu assuming SDR at eligible RGBA8 scene copy %ux%u; LO_SR_COMPAT=0 restores color certification\n",
                    unsigned(plan.requestedUpscaler), static_cast<unsigned long long>(frame_), width_, height_);
        }
        Transition(commands,source_,plume::RenderTextureLayout::COPY_DEST);
        commands->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(source_.texture.get()),plume::RenderTextureCopyLocation::Subresource(source));
        Transition(commands,source_,plume::RenderTextureLayout::SHADER_READ);
        current.colorOrdinal=scene.Color().ordinal; current.jx=jitter.pixelX; current.jy=jitter.pixelY;
        current.plan=plan;current.jitter=jitter;current.colorEncoding=encoding;
        current.colorEncodingAssumed=assumeSdr;
        motionVectorValid_=motion&&motion->ready&&motion->frame==frame_&&motion->epoch==epoch_&&
            motion->depthAllocation==current.allocation&&motion->width==width_&&motion->height==height_&&
            motion->velocity&&motion->reactive;
        motionView_=motionVectorValid_?*motion:MotionFrameView{};
        TemporalResetReason automatic=pendingReset_;pendingReset_=TemporalResetReason::None;
        if(!previous.inputsComplete) automatic=automatic|TemporalResetReason::FirstFrame;
        else {
            if(previous.number+1!=frame_) automatic=automatic|TemporalResetReason::FrameDiscontinuity;
            if(previous.epoch!=epoch_) automatic=automatic|TemporalResetReason::EpochChanged;
            if(previous.allocation!=current.allocation) automatic=automatic|TemporalResetReason::AllocationChanged;
            if(previous.colorEncoding!=encoding || previous.colorEncodingAssumed!=assumeSdr)
                automatic=automatic|TemporalResetReason::ColorEncodingChanged;
            if(previous.plan.consumer!=plan.consumer) automatic=automatic|TemporalResetReason::ConsumerChanged;
            if(previous.plan.width!=plan.width||previous.plan.height!=plan.height) automatic=automatic|TemporalResetReason::ExtentChanged;
            if(!previous.camera || !ContinuousHistoryCamera(*current.camera,*previous.camera)) automatic=automatic|TemporalResetReason::CameraDiscontinuity;
            if(!SameInputConfiguration(previous.plan,plan)) automatic=automatic|TemporalResetReason::PlanConfigurationChanged;
        }
        // Only production SR opts in. Probe/legacy TAA and future FG keep their
        // existing contracts. A stale ready geometry view cannot be papered over.
        if (allowHybrid && upscaling::MatchesSrProvider(plan.requestedUpscaler,plan.consumer) &&
            plan.frameGeneration == upscaling::FrameGeneration::Off &&
            !(motion && motion->ready && !motionVectorValid_)) {
            hybridAttempted_ = true;
            const bool hybridReset = (reset|automatic) != TemporalResetReason::None ||
                !previous.camera || (motion && motion->ready && motion->state == MotionState::ResetInitialization);
            const auto composed = hybridMotion_.Render(device_,commands,depth_[frame_%2].texture.get(),
                *current.camera,previous.camera ? &*previous.camera : nullptr,
                motionVectorValid_ ? &motionView_ : nullptr,frame_,epoch_,current.allocation,width_,height_,
                jitter.pixelX,jitter.pixelY,hybridReset,aa_.RecordedSerial()+1);
            if (composed.ready) { motionView_=composed; motionVectorValid_=true; }
            // An optional composer failure may retain already-valid geometric
            // motion; it may never turn missing resources into complete inputs.
        }
        if(upscaling::RequiresMotionDepth(plan.consumer,plan.frameGeneration)&&!motionVectorValid_)
            automatic=automatic|TemporalResetReason::IncompleteInputs;
        current.inputReset=reset|automatic;
        current.inputsComplete=!upscaling::RequiresMotionDepth(plan.consumer,plan.frameGeneration)||motionVectorValid_;
        aa_.RecordExternalUse(); hybridMotion_.RecordConsumerUse(aa_.RecordedSerial()); return true;
    }
    TemporalFrameInputs CurrentInputs() const {
        const auto& current=frames_[frame_%2]; TemporalFrameInputs result;
        result.plan=current.plan; result.renderFrameId=frame_; result.temporalEpoch=epoch_; result.depthAllocation=current.allocation;
        result.color={source_.texture.get(),{width_,height_},0,0,width_,height_};
        result.depth={depth_[frame_%2].texture.get(),{width_,height_},0,0,width_,height_};
        // CaptureDepth copies the R32 resolve unchanged. SceneObservation's
        // reviewed camera/depth contract is d=1 near, d=0 far (reversed Z).
        result.depthConvention=DepthConvention::Reversed;
        if (current.camera) { result.cameraViewProjection=current.camera->VP(); result.cameraValid=true; }
        result.motion={motionView_.velocity,{width_,height_},0,0,width_,height_};
        result.motionInvalidity={motionView_.reactive,{width_,height_},0,0,width_,height_};
        result.jitter=current.jitter; result.colorEncoding=current.colorEncoding; result.currentInputsComplete=current.inputsComplete;
        result.colorEncodingAssumed=current.colorEncodingAssumed;
        result.motionState=motionView_.state; result.resetReasons=current.inputReset;
        result.resetHistory=result.resetReasons!=TemporalResetReason::None;
        return result;
    }
    // Caller supplies full scene color in SourceFormat() and COPY_SOURCE. Output
    // is SHADER_READ in OutputFormat(); the guest SDR scene-copy still quantizes
    // the sampled result into its original RGBA8 target, outside this owner.
    // allowHistory is an explicit experiment assertion, NOT inferred scene/MV safety.
    plume::RenderTexture* ResolveColor(plume::RenderCommandList* commands,plume::RenderTexture* source,const SceneObservation& scene,double jx,double jy,bool allowHistory,bool stableGrid=false,bool colorReactive=false,const MotionFrameView* motion=nullptr,bool motionDebug=false,const LiveOptions* live=nullptr) {
        auto& current=frames_[frame_%2];auto& previous=frames_[(frame_+1)%2];
        if(!commands||!source||!scene.Ready()||scene.Frame()!=frame_||!current.camera||current.completed||current.depthOrdinal!=scene.Depth().ordinal||scene.Color().width!=width_||scene.Color().height!=height_) {Reset();return nullptr;}
        if(!SetHistoryStorage(live&&live->history_fp16!=0)){Reset();return nullptr;}
        current.jx=jx;current.jy=jy;current.stableGrid=stableGrid;
        const bool reuse=valid_&&previous.completed&&previous.stableGrid==stableGrid&&previous.number+1==frame_&&previous.epoch==epoch_&&previous.camera&&previous.allocation==current.allocation&&SameRaster(*current.camera,*previous.camera)&&ContinuousHistoryCamera(*current.camera,*previous.camera);
        if(diagnosticsEnabled_)diagnostics_=InspectHistoryReuse(
            {valid_,previous.completed,stableGrid,previous.stableGrid,allowHistory,frame_,previous.number,epoch_,previous.epoch,current.allocation,previous.allocation},
            &*current.camera,previous.camera?&*previous.camera:nullptr);
        const JitterSample jitter{0,jx,jy,0,0};
        frame_plan::FramePlan legacyPlan{};legacyPlan.width=width_;legacyPlan.height=height_;legacyPlan.consumer=upscaling::TemporalConsumer::LegacyTaa;
        if(!CaptureColorInputs(commands,source,scene,legacyPlan,jitter,sourceFormat_==plume::RenderFormat::R16G16B16A16_FLOAT?ColorEncoding::HdrLinear:ColorEncoding::Sdr,motion)) {Reset();return nullptr;}
        if(!aaInitialized_&&!aa_.Init(device_,sourceFormat_==plume::RenderFormat::R16G16B16A16_FLOAT)){Reset();return nullptr;}
        aaInitialized_=true;
        Transition(commands,history_[frame_%2],plume::RenderTextureLayout::COLOR_WRITE);
        Transition(commands,history_[(frame_+1)%2],plume::RenderTextureLayout::SHADER_READ);

        motionView_={};
        motionVectorValid_=motion&&motion->ready&&motion->frame==frame_&&motion->epoch==epoch_&&
            motion->depthAllocation==current.allocation&&motion->width==width_&&motion->height==height_&&
            motion->velocity&&motion->depths&&motion->reactive;
        if(motionVectorValid_)motionView_=*motion;
        TemporalAAInputs in;in.rejectOutOfNeighborhoodHistory=colorReactive;in.stableGrid=stableGrid;in.currentColor=source_.texture.get();in.currentDepth=depth_[frame_%2].texture.get();in.historyColor=history_[(frame_+1)%2].texture.get();in.historyDepth=depth_[(frame_+1)%2].texture.get();in.output=history_[frame_%2].texture.get();
        in.outputStorage=OutputStorage();
        in.motionVector=motionView_.velocity;in.motionDepths=motionView_.depths;in.reactiveMask=motionView_.reactive;
        in.motionVectorDebug=motionDebug;
        in.motionVectorValid=motionVectorValid_;
        // Serial zero is the built-in main policy, not a live diagnostic
        // snapshot. Preserve environment diagnostics in that case.
        const bool liveOverride=live&&live->serial!=0;
        static const bool stationaryHistory=[] {const char* value=std::getenv("LO_TAA_STATIONARY_HISTORY");return !value||value[0]!='0';}();
        in.stabilizeStationaryGeometry=stationaryHistory;
        if(live) {
            in.stabilizeStationaryGeometry=stationaryHistory&&live->stationary!=0;in.stationaryCoverage=live->coverage!=0;
            in.snapStationaryMotion=live->snap_stationary!=0;
            in.stationaryColorClip=live->stationary_color_clip!=0;
            in.stationaryMultiSurface=live->stationary_multi_surface!=0;
            in.movingBilinearFallback=live->moving_bilinear_fallback!=0;
            in.historyWeight=live->history_weight;in.stationaryHistoryWeight=live->stationary_weight;
            in.stationaryMotionMin=live->motion_min;in.stationaryMotionMax=live->motion_max;
            in.depthAbsoluteThreshold=live->depth_absolute;in.depthRelativeThreshold=live->depth_relative;
            if(liveOverride)in.motionVectorDebug=false;
        }
        in.width=in.historyWidth=width_;in.height=in.historyHeight=height_;in.currentCamera=&*current.camera;in.previousCamera=previous.camera?&*previous.camera:nullptr;
        in.currentJitterX=jx;in.currentJitterY=jy;in.previousJitterX=reuse?previous.jx:0;in.previousJitterY=reuse?previous.jy:0;in.historyValid=reuse;
        // An explicitly requested geometric frame must not silently become camera
        // history when tracking/coverage/epoch validation failed. nullptr alone
        // selects the established camera-only baseline.
        in.rejectAllHistory=!allowHistory||(motion&&!motionVectorValid_);
        if(sparse_&&!sparseReleaseSerial_&&sparse_->Ready()&&taa_collection::WantSparse()) {
            taa_collection::SparseFrame f;f.frame=frame_;f.epoch=epoch_;f.width=width_;f.height=height_;
            f.current=current.camera;f.previous=previous.camera;
            f.flags=(previous.completed&&previous.number+1==frame_&&previous.epoch==epoch_&&previous.camera&&SameRaster(*current.camera,*previous.camera)?1u:0u)|(reuse?2u:0u)|(allowHistory?4u:0u)|(stableGrid?8u:0u);
            f.jitter[0]=float(jx);f.jitter[1]=float(jy);f.jitter[2]=float(previous.jx);f.jitter[3]=float(previous.jy);
            sparse_->Record(commands,in.currentDepth,std::move(f));
            aa_.RecordExternalUse();sparseReleaseSerial_=aa_.RecordedSerial();
        }
        if(!aa_.Resolve(commands,in)){Reset();return nullptr;}
        Transition(commands,history_[frame_%2],plume::RenderTextureLayout::SHADER_READ);
        // Diagnose the same inputs without feeding diagnostic colors into history.
        static const int acceptanceView=[] {const char* value=std::getenv("LO_TAA_ACCEPTANCE");return value&&value[0]=='2'?2:value&&value[0]=='1'?1:0;}();
        const int diagnosticView=liveOverride?live->acceptance:acceptanceView;
        if(diagnosticView||in.motionVectorDebug||(liveOverride&&live->mv_debug)) {
            in.diagnosticAcceptance=diagnosticView!=0;
            in.motionVectorDebug=!diagnosticView&&(in.motionVectorDebug||(liveOverride&&live->mv_debug));
            in.output=display_.texture.get();
            in.diagnosticRejectionReasons=diagnosticView==2;
            Transition(commands,display_,plume::RenderTextureLayout::COLOR_WRITE);
            if(!aa_.Resolve(commands,in)){Reset();return nullptr;}
            Transition(commands,display_,plume::RenderTextureLayout::SHADER_READ);
            current.completed=current.taaResolved=true;valid_=true;reused_=reuse&&!in.rejectAllHistory;return display_.texture.get();
        }
        if(stableGrid) {current.completed=current.taaResolved=true;valid_=true;reused_=reuse&&!in.rejectAllHistory;return history_[frame_%2].texture.get();}
        Transition(commands,display_,plume::RenderTextureLayout::COLOR_WRITE);
        if(!aa_.ReconstructDisplay(commands,{history_[frame_%2].texture.get(),display_.texture.get(),width_,height_,jx,jy,OutputStorage()})){Reset();return nullptr;}
        Transition(commands,display_,plume::RenderTextureLayout::SHADER_READ);
        current.completed=current.taaResolved=true;valid_=true;reused_=reuse&&!in.rejectAllHistory;return display_.texture.get();
    }
    bool Completed() const {return frames_[frame_%2].completed;}
    bool InputsComplete() const {return frames_[frame_%2].inputsComplete;}
    // Input-only consumers never set taaResolved, but their previous complete
    // color/depth/MV set is still valid evidence for the next reset decision.
    bool ResetInitializationRequired() const {
        const auto& previous=frames_[(frame_+1)%2];
        return !previous.inputsComplete || previous.number+1!=frame_ || previous.epoch!=epoch_;
    }
    bool Reused() const {return reused_;}
    plume::RenderFormat SourceFormat() const {return sourceFormat_;}
    plume::RenderFormat HistoryFormat() const {return historyFormat_;}
    plume::RenderFormat OutputFormat() const {return historyFormat_;}
    const HistoryReuseDiagnostic& Diagnostics() const {return diagnostics_;}
    // Borrowed diagnostic view, SHADER_READ; restore that layout after a readback.
    plume::RenderTexture* CurrentDepth() const {return depth_[frame_%2].texture.get();}
    plume::RenderTexture* CurrentMotionVector() const {return motionVectorValid_?motionView_.velocity:nullptr;}
    plume::RenderTexture* CurrentMotionDepths() const {return motionVectorValid_?motionView_.depths:nullptr;}
    plume::RenderTexture* CurrentReactiveMask() const {return motionVectorValid_?motionView_.reactive:nullptr;}
    void ReleaseCompleted() {if(sparse_)sparse_->ReleaseCompleted();sparseReleaseSerial_=0;hybridMotion_.ReleaseCompletedThrough(aa_.RecordedSerial());aa_.ReleaseCompleted();retired_.clear();}
    uint64_t RecordedSerial() const {return aa_.RecordedSerial();}
    void ReleaseCompletedThrough(uint64_t serial) {
        if(sparse_&&sparseReleaseSerial_&&sparseReleaseSerial_<=serial) {
            sparse_->ReleaseCompleted();sparseReleaseSerial_=0;
        }
        aa_.ReleaseCompletedThrough(serial);
        hybridMotion_.ReleaseCompletedThrough(serial);
        std::erase_if(retired_,[serial](const RetiredImage& image){return image.serial<=serial;});
    }
};
}
#endif
