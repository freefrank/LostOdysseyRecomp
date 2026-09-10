#pragma once
#include "temporal_aa.h"
#include "temporal_scene.h"

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
        bool completed=false,stableGrid=false;
    };
    plume::RenderDevice* device_=nullptr;
    TemporalAA aa_;
    std::shared_ptr<taa_collection::SparseDepthGPU> sparse_;
    std::array<Image,2> depth_,history_;
    Image source_,display_;
    std::vector<std::unique_ptr<plume::RenderTexture>> retired_;
    std::array<Frame,2> frames_;
    uint32_t width_=0,height_=0;
    uint64_t frame_=~0ull,epoch_=0;
    bool valid_=false,reused_=false;
    bool diagnosticsEnabled_=false;
    HistoryReuseDiagnostic diagnostics_;
    static void Transition(plume::RenderCommandList* commands,Image& image,plume::RenderTextureLayout layout) {
        if(image.layout!=layout) {commands->barriers(plume::RenderBarrierStage::ALL,plume::RenderTextureBarrier(image.texture.get(),layout));image.layout=layout;}
    }
    bool Allocate(Image& image,plume::RenderFormat format) {
        if(image.texture)retired_.push_back(std::move(image.texture));
        image.layout=plume::RenderTextureLayout::UNKNOWN;
        image.texture=device_->createTexture(plume::RenderTextureDesc::Texture2D(width_,height_,1,format,plume::RenderTextureFlag::RENDER_TARGET));
        return bool(image.texture);
    }
    static bool SameRaster(const Camera& a,const Camera& b) {
        const auto& x=a.Raster();const auto& y=b.Raster();
        return x.x==y.x&&x.y==y.y&&x.width==y.width&&x.height==y.height&&x.ndcYSign==y.ndcYSign&&x.halfPixelNdcX==y.halfPixelNdcX&&x.halfPixelNdcY==y.halfPixelNdcY;
    }
public:
    bool Init(plume::RenderDevice* device,std::shared_ptr<taa_collection::SparseDepthGPU> sparse={}) {
        device_=device;sparse_=std::move(sparse);return aa_.Init(device);
    }
    void Reset() {valid_=false;for(auto& frame:frames_)frame.completed=false;}
    // Frame identity is supplied by renderer, never CPU presented-swap count.
    void BeginFrame(uint64_t frame,uint64_t epoch,bool diagnostics=false) {
        diagnosticsEnabled_=diagnostics;
        if(frame_==frame&&epoch_==epoch)return;
        if(frame_+1!=frame||epoch_!=epoch)Reset();
        frame_=frame;epoch_=epoch;frames_[frame%2]=Frame{};reused_=false;
        diagnostics_={};
    }
    bool CaptureDepth(plume::RenderCommandList* commands,plume::RenderTexture* source,const SceneObservation& scene) {
        const auto& d=scene.Depth();auto& current=frames_[frame_%2];
        if(!commands||!source||!scene.Draws()||scene.Reason()!=SceneObservation::Rejection::None||d.frame!=frame_||!d.ordinal||!d.fullExtent||current.depthOrdinal)return false;
        if(width_!=d.width||height_!=d.height) {
            Reset();width_=d.width;height_=d.height;
            bool ok=width_&&height_&&width_<=16384&&height_<=16384;
            if(!ok)return false;
            for(auto& image:depth_)ok=Allocate(image,plume::RenderFormat::R32_FLOAT)&&ok;
            for(auto& image:history_)ok=Allocate(image,plume::RenderFormat::R8G8B8A8_UNORM)&&ok;
            ok=Allocate(source_,plume::RenderFormat::R8G8B8A8_UNORM)&&ok;
            ok=Allocate(display_,plume::RenderFormat::R8G8B8A8_UNORM)&&ok;
            if(!ok){width_=height_=0;return false;}
        }
        Matrix vp{};for(unsigned i=0;i<16;++i)vp[i]=std::bit_cast<float>(scene.Anchor().vpBits[i]);
        current.camera=Camera::Create(vp,scene.Anchor().viewport);if(!current.camera)return false;
        current.number=frame_;current.epoch=epoch_;current.allocation=scene.Anchor().depthAllocation;current.depthOrdinal=d.ordinal;
        // Caller has explicitly transitioned external source to COPY_SOURCE.
        auto& destination=depth_[frame_%2];Transition(commands,destination,plume::RenderTextureLayout::COPY_DEST);
        commands->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(destination.texture.get()),plume::RenderTextureCopyLocation::Subresource(source));
        Transition(commands,destination,plume::RenderTextureLayout::SHADER_READ);return true;
    }
    // Caller supplies full RGBA8 pre-UI scene in COPY_SOURCE. Output is SHADER_READ.
    // allowHistory is an explicit experiment assertion, NOT inferred scene/MV safety.
    plume::RenderTexture* ResolveColor(plume::RenderCommandList* commands,plume::RenderTexture* source,const SceneObservation& scene,double jx,double jy,bool allowHistory,bool stableGrid=false,bool colorReactive=false) {
        auto& current=frames_[frame_%2];auto& previous=frames_[(frame_+1)%2];
        if(!commands||!source||!scene.Ready()||scene.Frame()!=frame_||!current.camera||current.completed||current.depthOrdinal!=scene.Depth().ordinal||scene.Color().width!=width_||scene.Color().height!=height_) {Reset();return nullptr;}
        current.jx=jx;current.jy=jy;current.colorOrdinal=scene.Color().ordinal;current.stableGrid=stableGrid;
        const bool reuse=valid_&&previous.completed&&previous.stableGrid==stableGrid&&previous.number+1==frame_&&previous.epoch==epoch_&&previous.camera&&previous.allocation==current.allocation&&SameRaster(*current.camera,*previous.camera)&&ContinuousHistoryCamera(*current.camera,*previous.camera);
        if(diagnosticsEnabled_)diagnostics_=InspectHistoryReuse(
            {valid_,previous.completed,stableGrid,previous.stableGrid,allowHistory,frame_,previous.number,epoch_,previous.epoch,current.allocation,previous.allocation},
            &*current.camera,previous.camera?&*previous.camera:nullptr);
        Transition(commands,source_,plume::RenderTextureLayout::COPY_DEST);
        commands->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(source_.texture.get()),plume::RenderTextureCopyLocation::Subresource(source));
        Transition(commands,source_,plume::RenderTextureLayout::SHADER_READ);
        Transition(commands,history_[frame_%2],plume::RenderTextureLayout::COLOR_WRITE);
        Transition(commands,history_[(frame_+1)%2],plume::RenderTextureLayout::SHADER_READ);
        TemporalAAInputs in;in.rejectOutOfNeighborhoodHistory=colorReactive;in.stableGrid=stableGrid;in.currentColor=source_.texture.get();in.currentDepth=depth_[frame_%2].texture.get();in.historyColor=history_[(frame_+1)%2].texture.get();in.historyDepth=depth_[(frame_+1)%2].texture.get();in.output=history_[frame_%2].texture.get();
        in.width=in.historyWidth=width_;in.height=in.historyHeight=height_;in.currentCamera=&*current.camera;in.previousCamera=previous.camera?&*previous.camera:nullptr;
        in.currentJitterX=jx;in.currentJitterY=jy;in.previousJitterX=previous.jx;in.previousJitterY=previous.jy;in.historyValid=reuse;in.rejectAllHistory=!allowHistory;
        if(sparse_&&sparse_->Ready()&&taa_collection::WantSparse()) {
            taa_collection::SparseFrame f;f.frame=frame_;f.epoch=epoch_;f.width=width_;f.height=height_;
            f.current=current.camera;f.previous=previous.camera;
            f.flags=(previous.completed&&previous.number+1==frame_&&previous.epoch==epoch_&&previous.camera&&SameRaster(*current.camera,*previous.camera)?1u:0u)|(reuse?2u:0u)|(allowHistory?4u:0u)|(stableGrid?8u:0u);
            f.jitter[0]=float(jx);f.jitter[1]=float(jy);f.jitter[2]=float(previous.jx);f.jitter[3]=float(previous.jy);
            sparse_->Record(commands,in.currentDepth,std::move(f));
        }
        if(!aa_.Resolve(commands,in)){Reset();return nullptr;}
        Transition(commands,history_[frame_%2],plume::RenderTextureLayout::SHADER_READ);
        if(stableGrid) {current.completed=true;valid_=true;reused_=reuse&&allowHistory;return history_[frame_%2].texture.get();}
        Transition(commands,display_,plume::RenderTextureLayout::COLOR_WRITE);
        if(!aa_.ReconstructDisplay(commands,{history_[frame_%2].texture.get(),display_.texture.get(),width_,height_,jx,jy})){Reset();return nullptr;}
        Transition(commands,display_,plume::RenderTextureLayout::SHADER_READ);
        current.completed=true;valid_=true;reused_=reuse&&allowHistory;return display_.texture.get();
    }
    bool Completed() const {return frames_[frame_%2].completed;}
    bool Reused() const {return reused_;}
    const HistoryReuseDiagnostic& Diagnostics() const {return diagnostics_;}
    // Borrowed diagnostic view, SHADER_READ; restore that layout after a readback.
    plume::RenderTexture* CurrentDepth() const {return depth_[frame_%2].texture.get();}
    void ReleaseCompleted() {if(sparse_)sparse_->ReleaseCompleted();aa_.ReleaseCompleted();retired_.clear();}
};
}
#endif
