#pragma once
#include "temporal_aa.h"
#include "temporal_scene.h"
#ifdef LO_GPU_PLUME
#include <plume_render_interface.h>
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
    std::array<Image,2> depth_,history_;
    Image source_,display_;
    std::vector<std::unique_ptr<plume::RenderTexture>> retired_;
    std::array<Frame,2> frames_;
    uint32_t width_=0,height_=0;
    uint64_t frame_=~0ull,epoch_=0;
    bool valid_=false,reused_=false;
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
    static bool ContinuousCamera(const Camera& current,const Camera& previous) {
        // A practical camera-cut guard, not a guest scene identity oracle. Reject
        // > quarter-screen displacement at center across three fixed depth slices.
        const auto& r=current.Raster();
        for(double depth:{.001,.01,.1}) {
            const auto p=Reproject({r.width*.5,r.height*.5,depth},current,previous);
            if(!p||std::abs(p.previous.x-r.width*.5)>r.width*.25||std::abs(p.previous.y-r.height*.5)>r.height*.25)return false;
        }
        return true;
    }
public:
    bool Init(plume::RenderDevice* device) {device_=device;return aa_.Init(device);}
    void Reset() {valid_=false;for(auto& frame:frames_)frame.completed=false;}
    // Frame identity is supplied by renderer, never CPU presented-swap count.
    void BeginFrame(uint64_t frame,uint64_t epoch) {
        if(frame_==frame&&epoch_==epoch)return;
        if(frame_+1!=frame||epoch_!=epoch)Reset();
        frame_=frame;epoch_=epoch;frames_[frame%2]=Frame{};reused_=false;
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
        const bool reuse=valid_&&previous.completed&&previous.stableGrid==stableGrid&&previous.number+1==frame_&&previous.epoch==epoch_&&previous.camera&&previous.allocation==current.allocation&&SameRaster(*current.camera,*previous.camera)&&ContinuousCamera(*current.camera,*previous.camera);
        Transition(commands,source_,plume::RenderTextureLayout::COPY_DEST);
        commands->copyTextureRegion(plume::RenderTextureCopyLocation::Subresource(source_.texture.get()),plume::RenderTextureCopyLocation::Subresource(source));
        Transition(commands,source_,plume::RenderTextureLayout::SHADER_READ);
        Transition(commands,history_[frame_%2],plume::RenderTextureLayout::COLOR_WRITE);
        Transition(commands,history_[(frame_+1)%2],plume::RenderTextureLayout::SHADER_READ);
        TemporalAAInputs in;in.rejectOutOfNeighborhoodHistory=colorReactive;in.stableGrid=stableGrid;in.currentColor=source_.texture.get();in.currentDepth=depth_[frame_%2].texture.get();in.historyColor=history_[(frame_+1)%2].texture.get();in.historyDepth=depth_[(frame_+1)%2].texture.get();in.output=history_[frame_%2].texture.get();
        in.width=in.historyWidth=width_;in.height=in.historyHeight=height_;in.currentCamera=&*current.camera;in.previousCamera=previous.camera?&*previous.camera:nullptr;
        in.currentJitterX=jx;in.currentJitterY=jy;in.previousJitterX=previous.jx;in.previousJitterY=previous.jy;in.historyValid=reuse;in.rejectAllHistory=!allowHistory;
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
    // Borrowed diagnostic view, SHADER_READ; restore that layout after a readback.
    plume::RenderTexture* CurrentDepth() const {return depth_[frame_%2].texture.get();}
    void ReleaseCompleted() {aa_.ReleaseCompleted();retired_.clear();}
};
}
#endif
