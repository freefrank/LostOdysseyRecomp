#include <gpu/sr_hybrid_motion_gpu.h>
#include <gpu/sr_hybrid_mask.h>
#include <gpu/temporal_history.h>
#include <plume_vulkan.h>
#include <array>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

using namespace plume;
using namespace gpu;
namespace {
unsigned checks=0;
void Check(bool ok,const char* what) {++checks;if(!ok)throw std::runtime_error(what);}
void VkCheck(VkResult result,const char* what) {Check(result==VK_SUCCESS,what);}
float Half(uint16_t v) {
    const float sign=(v&0x8000)?-1.f:1.f;
    const unsigned exponent=(v>>10)&31, fraction=v&1023;
    if(exponent==31) return std::numeric_limits<float>::quiet_NaN();
    return sign * (exponent ? std::ldexp(1.f+float(fraction)/1024,int(exponent)-15) : std::ldexp(float(fraction),-24));
}
class Fixture {
    std::unique_ptr<RenderInterface> api_;
    std::unique_ptr<RenderDevice> device_;
    std::unique_ptr<RenderCommandQueue> queue_;
    std::unique_ptr<RenderCommandList> cmd_;
    std::unique_ptr<RenderCommandFence> fence_;
    std::unique_ptr<RenderBuffer> upload_, readback_, maskReadback_;
    std::unique_ptr<RenderTexture> depth_, velocity_, invalidity_, color_;
    temporal::SrHybridMotionGPU hybrid_;
    uint64_t serial_=0,frame_=0;
    static constexpr uint32_t W=8,H=8,PitchBytes=256;
    void Submit() {
        cmd_->end();
        const auto* device=static_cast<VulkanDevice*>(device_.get());
        const auto* queue=static_cast<VulkanCommandQueue*>(queue_.get());
        const auto* cmd=static_cast<VulkanCommandList*>(cmd_.get());
        const auto* fence=static_cast<VulkanCommandFence*>(fence_.get());
        VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};submit.commandBufferCount=1;submit.pCommandBuffers=&cmd->vk;
        VkCheck(vkResetFences(device->vk,1,&fence->vk),"reset fence");
        VkCheck(vkQueueSubmit(queue->queue->vk,1,&submit,fence->vk),"checked queue submit");
        VkCheck(vkWaitForFences(device->vk,1,&fence->vk,VK_TRUE,UINT64_MAX),"checked fence completion");
    }
    std::unique_ptr<RenderTexture> Texture(RenderFormat format,uint32_t width=W) {
        auto p=device_->createTexture(RenderTextureDesc::Texture2D(width,H,1,format,RenderTextureFlag::NONE));
        Check(bool(p),"fixture texture");return p;
    }
    void Fill(RenderTexture* image,RenderFormat format,unsigned pixelBytes,const void* pixels,uint32_t width=W) {
        auto* mapped=static_cast<uint8_t*>(upload_->map());Check(mapped,"upload map");
        std::memset(mapped,0,PitchBytes*H);
        for(unsigned y=0;y<H;++y)std::memcpy(mapped+y*PitchBytes,static_cast<const uint8_t*>(pixels)+y*width*pixelBytes,width*pixelBytes);
        upload_->unmap();cmd_->begin();
        cmd_->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(image,RenderTextureLayout::COPY_DEST));
        cmd_->copyTextureRegion(RenderTextureCopyLocation::Subresource(image),
            RenderTextureCopyLocation::PlacedFootprint(upload_.get(),format,width,H,1,PitchBytes/pixelBytes));
        cmd_->barriers(RenderBarrierStage::ALL,RenderTextureBarrier(image,RenderTextureLayout::SHADER_READ));Submit();
    }
    static temporal::Matrix Identity() {temporal::Matrix m{};m[0]=m[5]=m[10]=m[15]=1;return m;}
    temporal::Camera Camera(temporal::Matrix matrix,double sign=1,uint32_t width=W) {
        auto camera=temporal::Camera::Create(matrix,{0,0,double(width),H,sign});Check(bool(camera),"fixture camera");return *camera;
    }
    void CopyResult(const temporal::MotionFrameView& output,uint32_t width=W) {
        cmd_->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(output.velocity,RenderTextureLayout::COPY_SOURCE));
        cmd_->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback_.get(),RenderFormat::R16G16_FLOAT,width,H,1,PitchBytes/4),RenderTextureCopyLocation::Subresource(output.velocity));
        cmd_->barriers(RenderBarrierStage::ALL,RenderTextureBarrier(output.velocity,RenderTextureLayout::SHADER_READ));
        cmd_->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(output.reactive,RenderTextureLayout::COPY_SOURCE));
        cmd_->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(maskReadback_.get(),RenderFormat::R8_UNORM,width,H,1,PitchBytes),RenderTextureCopyLocation::Subresource(output.reactive));
        cmd_->barriers(RenderBarrierStage::ALL,RenderTextureBarrier(output.reactive,RenderTextureLayout::SHADER_READ));
    }
public:
    Fixture() {
        for(auto format:{xenos::ShaderBinaryFormat::Spirv,xenos::ShaderBinaryFormat::Dxil})
            for(bool pixel:{false,true}) {
                auto shader=xenos::CompileHlsl(temporal::kSrHybridMotionShader,pixel?"pixel":"vertex",pixel?"ps_6_0":"vs_6_0",format);
                if(!shader.ok)std::fprintf(stderr,"%s\n",shader.errors.c_str());
                Check(shader.ok,"production shader compiles for SPIR-V and DXIL");
            }
        api_=CreateVulkanInterface();Check(bool(api_),"Vulkan interface");device_=api_->createDevice();Check(bool(device_),"Vulkan device");
        std::printf("DEVICE=%s\n",device_->getDescription().name.c_str());
        queue_=device_->createCommandQueue(RenderCommandListType::DIRECT);Check(bool(queue_),"queue");
        cmd_=queue_->createCommandList();fence_=device_->createCommandFence();
        upload_=device_->createBuffer(RenderBufferDesc::UploadBuffer(PitchBytes*H));
        readback_=device_->createBuffer(RenderBufferDesc::ReadbackBuffer(PitchBytes*H));
        maskReadback_=device_->createBuffer(RenderBufferDesc::ReadbackBuffer(PitchBytes*H));
        Check(cmd_&&fence_&&upload_&&readback_&&maskReadback_,"command and transfer resources");
        depth_=Texture(RenderFormat::R32_FLOAT);velocity_=Texture(RenderFormat::R16G16_FLOAT);
        invalidity_=Texture(RenderFormat::R8_UNORM);color_=Texture(RenderFormat::R8G8B8A8_UNORM);
        std::array<float,W*H> depth;depth.fill(.5f);Fill(depth_.get(),RenderFormat::R32_FLOAT,4,depth.data());
        std::array<uint32_t,W*H> mv;mv.fill(0x3c00bc00u);Fill(velocity_.get(),RenderFormat::R16G16_FLOAT,4,mv.data());
        std::array<uint8_t,W*H> mask{};for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x)mask[y*W+x]=x<4?0:255;
        Fill(invalidity_.get(),RenderFormat::R8_UNORM,1,mask.data());
        std::array<uint32_t,W*H> color;color.fill(0xff808080u);Fill(color_.get(),RenderFormat::R8G8B8A8_UNORM,4,color.data());
    }
    void Pixels(bool geometry,bool reset,float dx,float dy,double jitterX=0,double jitterY=0,double sign=1) {
        auto current=Camera(Identity(),sign);auto previousMatrix=Identity();previousMatrix[12]=dx;previousMatrix[13]=dy;
        auto previous=Camera(previousMatrix,sign);++frame_;
        temporal::MotionFrameView geo{velocity_.get(),nullptr,invalidity_.get(),frame_,1,7,W,H,true,temporal::MotionState::Tracked};
        cmd_->begin();
        auto output=hybrid_.Render(device_.get(),cmd_.get(),depth_.get(),current,&previous,geometry?&geo:nullptr,
            frame_,1,7,W,H,jitterX,jitterY,reset,++serial_);
        Check(output.ready && output.state==temporal::MotionState::Hybrid,"real hybrid GPU output");
        CopyResult(output);hybrid_.RecordConsumerUse(++serial_);Submit();hybrid_.ReleaseCompletedThrough(serial_);
        const auto* motion=static_cast<const uint16_t*>(readback_->map());const auto* mask=static_cast<const uint8_t*>(maskReadback_->map());
        Check(motion&&mask,"readback maps");
        // Interior samples stay inside both viewports for every tested jitter.
        for(unsigned y=2;y<6;++y)for(unsigned x=3;x<6;++x) {
            const bool exact=geometry&&x<4&&!reset;
            const float expectedX=reset?0:exact?-1:dx*W*.5f;
            const float expectedY=reset?0:exact?1:float(-dy*H*.5*sign);
            Check(std::abs(Half(motion[y*PitchBytes/2+x*2])-expectedX)<.002f,"backward X/current-jitter cancellation");
            Check(std::abs(Half(motion[y*PitchBytes/2+x*2+1])-expectedY)<.002f,"backward Y/current-jitter cancellation");
            Check(mask[y*PitchBytes+x]==(exact?0:255),"geometry confidence versus camera approximation");
        }
        readback_->unmap();maskReadback_->unmap();
        auto bad=geo;bad.epoch=2;cmd_->begin();
        Check(!hybrid_.Render(device_.get(),cmd_.get(),depth_.get(),current,&previous,&bad,frame_,1,7,W,H,0,0,false,++serial_).ready,"stale GPU view is rejected");Submit();
    }
    void PoolAndInvalidDepth() {
        auto camera=Camera(Identity());cmd_->begin();std::array<RenderTexture*,8> outputs{};
        for(unsigned i=0;i<outputs.size();++i) {
            auto output=hybrid_.Render(device_.get(),cmd_.get(),depth_.get(),camera,&camera,nullptr,++frame_,1,7,W,H,0,0,false,++serial_);
            Check(output.ready,"in-flight batch allocated");outputs[i]=output.velocity;
            for(unsigned j=0;j<i;++j)Check(outputs[j]!=outputs[i],"no in-flight image alias");
        }
        Check(!hybrid_.Render(device_.get(),cmd_.get(),depth_.get(),camera,&camera,nullptr,++frame_,1,7,W,H,0,0,false,++serial_).ready,"bounded pool falls back without a wait");
        Check(hybrid_.BatchCount()==8 && !hybrid_.ResourceFailed(),"pool bound is transient, not device failure");Submit();hybrid_.ReleaseCompletedThrough(serial_);
        std::array<float,W*H> invalid;invalid.fill(std::numeric_limits<float>::quiet_NaN());
        Fill(depth_.get(),RenderFormat::R32_FLOAT,4,invalid.data());cmd_->begin();
        auto output=hybrid_.Render(device_.get(),cmd_.get(),depth_.get(),camera,&camera,nullptr,++frame_,1,7,W,H,0,0,false,++serial_);
        Check(output.ready,"pool recovers after completed fence");CopyResult(output);Submit();hybrid_.ReleaseCompletedThrough(serial_);
        const auto* motion=static_cast<const uint16_t*>(readback_->map());const auto* mask=static_cast<const uint8_t*>(maskReadback_->map());Check(motion&&mask,"invalid-depth readback");
        for(unsigned y=0;y<H;++y)for(unsigned x=0;x<W;++x) {
            Check(Half(motion[y*PitchBytes/2+x*2])==0 && Half(motion[y*PitchBytes/2+x*2+1])==0 && mask[y*PitchBytes+x]==255,"NaN depth is uncertain zero, never valid stationary");
        }
        readback_->unmap();maskReadback_->unmap();invalid.fill(.5f);Fill(depth_.get(),RenderFormat::R32_FLOAT,4,invalid.data());
    }
    void HistoryOwnerRecovery() {
        temporal::HistoryOwner owner;Check(owner.Init(device_.get()),"real HistoryOwner initialized");
        auto matrix=Identity();frame_plan::FramePlan plan{};plan.width=W;plan.height=H;plan.deviceEpoch=1;plan.geometryEpoch=1;
        plan.requestedUpscaler=upscaling::Upscaler::Fsr;plan.consumer=upscaling::TemporalConsumer::FsrSr;
        auto frame=[&](uint64_t number,bool hybrid,bool expectedComplete,bool expectedReset,uint64_t epoch=1) {
            owner.BeginFrame(number,epoch);temporal::SceneObservation scene;scene.Reset(number);
            temporal::SceneAnchor anchor{};anchor.viewport={0,0,W,H};anchor.depthAllocation=9;
            for(unsigned i=0;i<16;++i)anchor.vpBits[i]=std::bit_cast<uint32_t>(float(matrix[i]));
            scene.ObserveCamera(anchor);scene.ObserveDepth(9,{number,1,0,0,W,H,true});
            Check(scene.ObserveColor({number,2,0,0,W,H,true}),"synthetic qualified pre-UI scene");
            cmd_->begin();cmd_->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(depth_.get(),RenderTextureLayout::COPY_SOURCE));
            Check(owner.CaptureDepth(cmd_.get(),depth_.get(),scene),"actual owner depth capture");
            cmd_->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(color_.get(),RenderTextureLayout::COPY_SOURCE));
            Check(owner.CaptureColorInputs(cmd_.get(),color_.get(),scene,plan,{},temporal::ColorEncoding::Sdr,nullptr,
                temporal::TemporalResetReason::None,hybrid),"actual owner color capture");
            auto inputs=owner.CurrentInputs();Check(inputs.CompleteForConsumer()==expectedComplete,"owner missing-geometry recovery");
            if(expectedComplete) {
                Check(inputs.motionState==temporal::MotionState::Hybrid && inputs.resetHistory==expectedReset,"owner reset and Hybrid state");
                Check(temporal::ValidSrHybridMask(inputs,static_cast<VulkanDevice*>(device_.get())),"native confidence region qualification");
                auto stale=inputs;stale.motionInvalidity.width--;Check(!temporal::ValidSrHybridMask(stale,static_cast<VulkanDevice*>(device_.get())),"native confidence extent rejection");
            }
            cmd_->barriers(RenderBarrierStage::ALL,RenderTextureBarrier(depth_.get(),RenderTextureLayout::SHADER_READ));
            cmd_->barriers(RenderBarrierStage::ALL,RenderTextureBarrier(color_.get(),RenderTextureLayout::SHADER_READ));
            Submit();owner.ReleaseCompletedThrough(owner.RecordedSerial());
        };
        frame(1,false,false,true);frame(2,true,true,true);frame(3,true,true,false);
        frame(4,false,false,true);frame(5,true,true,true);frame(6,true,true,false);
        frame(8,true,true,true);frame(9,true,true,true,2);
        plan.requestedUpscaler=upscaling::Upscaler::Dlss;plan.consumer=upscaling::TemporalConsumer::DlssSr;
        frame(10,true,true,true,2);frame(11,true,true,false,2);
        owner.ReleaseCompleted();
    }
};
}
int main() {
    try {
        Fixture f;
        for(unsigned phase=0;phase<32;++phase) {
            const auto jitter=temporal::FrameJitter(phase+1,8,8);
            f.Pixels(false,false,0,0,jitter.pixelX,jitter.pixelY);
            f.Pixels(false,false,-.5f,0,jitter.pixelX,jitter.pixelY);
        }
        f.Pixels(true,false,-.5f,0);f.Pixels(true,true,-.5f,0);f.Pixels(false,false,0,.25f,0,0,-1);
        f.PoolAndInvalidDepth();f.HistoryOwnerRecovery();
        std::printf("PASS: %u hybrid Vulkan/owner checks; no NGX execution or game quality claim\n",checks);return 0;
    } catch(const std::exception& e) {std::fprintf(stderr,"FAIL after %u checks: %s\n",checks,e.what());return 1;}
}
