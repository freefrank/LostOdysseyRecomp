#include <gpu/dlss_ngx.h>
#include <gpu/temporal_frame_inputs.h>
#include <plume_d3d12.h>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <memory>
#include <stdexcept>

namespace plume { std::unique_ptr<RenderInterface> CreateD3D12Interface(); }
namespace {
using namespace plume;
constexpr uint32_t kOutputWidth=1280, kOutputHeight=720;
[[noreturn]] void Fail(const char* message) { throw std::runtime_error(message); }
int Skip(const char* message) { std::printf("SKIP: %s\n",message); return 77; }
bool FiniteHalf(uint16_t value) { return (value&0x7c00u)!=0x7c00u; }
gpu::temporal::TextureRegion Region(RenderTexture* texture, uint32_t width, uint32_t height) {
    return {texture,{width,height},0,0,width,height};
}
void Clear(RenderCommandList& command, RenderFramebuffer& framebuffer, RenderTexture& texture, RenderColor color) {
    command.barriers(RenderBarrierStage::GRAPHICS,RenderTextureBarrier(&texture,RenderTextureLayout::COLOR_WRITE));
    command.setFramebuffer(&framebuffer); command.clearColor(0,color);
}
}

int main() {
    try {
        std::setvbuf(stdout,nullptr,_IONBF,0);
        gpu::dlss::Controller controller(std::filesystem::temp_directory_path()/"lo-ngx-d3d12-fixture",
            std::filesystem::current_path());
        auto renderInterface=plume::CreateD3D12Interface();
        if (!renderInterface) return Skip("D3D12 interface unavailable");
        auto device=renderInterface->createDevice();
        if (!device) return Skip("D3D12 device unavailable");
        auto& native=*static_cast<plume::D3D12Device*>(device.get());
        struct Drain { gpu::dlss::Controller& controller; ~Drain(){controller.ShutdownAfterGpuDrain();} } drain{controller};
        controller.ProbeOnce(native);
        if (controller.Report().state!=gpu::dlss::ProbeState::Available)
            return Skip(controller.Report().reason.c_str());
        constexpr uint64_t kEpoch=1;
        const auto sizing=controller.QueryOutputSizing(native,{kEpoch,kOutputWidth,kOutputHeight});
        auto queue=device->createCommandQueue(RenderCommandListType::DIRECT);
        auto prefix=queue ? queue->createCommandList() : nullptr;
        auto isolated=queue ? queue->createCommandList() : nullptr;
        auto continuation=queue ? queue->createCommandList() : nullptr;
        auto fence=device->createCommandFence();
        if (!queue || !prefix || !isolated || !continuation || !fence)
            return Skip("D3D12 fixture queue resources unavailable");
        uint64_t serial=0;
        for (const auto quality : {gpu::upscaling::DlssQuality::Quality,gpu::upscaling::DlssQuality::Dlaa}) {
            const auto& mode=sizing.modes[gpu::upscaling::DlssQualityIndex(quality)];
            if (mode.state!=gpu::upscaling::SizingState::Ready || !mode.optimal.width || !mode.optimal.height)
                return Skip(quality==gpu::upscaling::DlssQuality::Dlaa ?
                    "D3D12 DLAA sizing unavailable" : "D3D12 SR sizing unavailable");
            const uint32_t renderWidth=mode.optimal.width, renderHeight=mode.optimal.height;
            auto color=device->createTexture(RenderTextureDesc::Texture2D(renderWidth,renderHeight,1,
                RenderFormat::R16G16B16A16_FLOAT,RenderTextureFlag::RENDER_TARGET));
            auto depth=device->createTexture(RenderTextureDesc::Texture2D(renderWidth,renderHeight,1,
                RenderFormat::R32_FLOAT,RenderTextureFlag::RENDER_TARGET));
            auto motion=device->createTexture(RenderTextureDesc::Texture2D(renderWidth,renderHeight,1,
                RenderFormat::R16G16_FLOAT,RenderTextureFlag::RENDER_TARGET));
            auto output=device->createTexture(RenderTextureDesc::Texture2D(kOutputWidth,kOutputHeight,1,
                RenderFormat::R16G16B16A16_FLOAT,
                RenderTextureFlag::RENDER_TARGET|RenderTextureFlag::UNORDERED_ACCESS));
            const uint32_t pitchPixels=((kOutputWidth*8+255)/256)*256/8;
            auto readback=device->createBuffer(RenderBufferDesc::ReadbackBuffer(uint64_t(pitchPixels)*kOutputHeight*8));
            if (!color || !depth || !motion || !output || !readback)
                return Skip("D3D12 fixture textures unavailable");
            const RenderTexture* colorAttachment[]={color.get()},*depthAttachment[]={depth.get()},
                *motionAttachment[]={motion.get()},*outputAttachment[]={output.get()};
            auto colorFb=device->createFramebuffer(RenderFramebufferDesc(colorAttachment,1));
            auto depthFb=device->createFramebuffer(RenderFramebufferDesc(depthAttachment,1));
            auto motionFb=device->createFramebuffer(RenderFramebufferDesc(motionAttachment,1));
            auto outputFb=device->createFramebuffer(RenderFramebufferDesc(outputAttachment,1));
            if (!colorFb || !depthFb || !motionFb || !outputFb) return Skip("D3D12 fixture framebuffers unavailable");
            gpu::dlss::SrConfig config{};
            config.renderExtent={renderWidth,renderHeight}; config.outputExtent={kOutputWidth,kOutputHeight};
            config.quality=quality; config.colorSpace=gpu::dlss::SrColorSpace::Linear;
            config.deviceEpoch=kEpoch; config.depthInverted=false;
            gpu::temporal::TemporalFrameInputs inputs{};
            inputs.plan.deviceEpoch=kEpoch;
            inputs.plan.consumer=gpu::upscaling::TemporalConsumer::DlssSr;
            inputs.renderFrameId=++serial;
            inputs.color=Region(color.get(),renderWidth,renderHeight);
            inputs.depth=Region(depth.get(),renderWidth,renderHeight);
            inputs.motion=Region(motion.get(),renderWidth,renderHeight);
            inputs.motionInvalidity=Region(motion.get(),renderWidth,renderHeight);
            inputs.currentInputsComplete=true; inputs.resetHistory=true;
            inputs.motionState=gpu::temporal::MotionState::ResetInitialization;
            inputs.colorEncoding=gpu::temporal::ColorEncoding::HdrLinear;
            inputs.depthConvention=gpu::temporal::DepthConvention::Forward;
            prefix->begin();
            Clear(*prefix,*colorFb,*color,RenderColor(.25f,.5f,.125f,1));
            Clear(*prefix,*depthFb,*depth,RenderColor(.5f,0,0,0));
            Clear(*prefix,*motionFb,*motion,RenderColor(0,0,0,0));
            Clear(*prefix,*outputFb,*output,RenderColor(0,0,0,0));
            RenderTextureBarrier reads[]={
                {color.get(),RenderTextureLayout::SHADER_READ},
                {depth.get(),RenderTextureLayout::SHADER_READ},
                {motion.get(),RenderTextureLayout::SHADER_READ}};
            prefix->barriers(RenderBarrierStage::ALL,nullptr,0,reads,3);
            prefix->barriers(RenderBarrierStage::ALL,RenderTextureBarrier(output.get(),RenderTextureLayout::GENERAL));
            prefix->end();
            const auto attempt=controller.RecordIsolated(*static_cast<D3D12CommandList*>(isolated.get()),
                config,inputs,*static_cast<D3D12Texture*>(output.get()));
            if (attempt.status!=gpu::dlss::SrStatus::Executable || !attempt.useId)
                Fail("D3D12 NGX Create/Evaluate failed");
            continuation->begin();
            continuation->barriers(RenderBarrierStage::COPY,
                RenderTextureBarrier(output.get(),RenderTextureLayout::COPY_SOURCE));
            continuation->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),
                RenderFormat::R16G16B16A16_FLOAT,kOutputWidth,kOutputHeight,1,pitchPixels),
                RenderTextureCopyLocation::Subresource(output.get()));
            continuation->end();
            const RenderCommandList* lists[]={prefix.get(),isolated.get(),continuation.get()};
            auto& nativeFence=*static_cast<D3D12CommandFence*>(fence.get());
            const UINT64 submittedFenceValue=nativeFence.fenceValue;
            queue->executeCommandLists(lists,3,nullptr,0,nullptr,0,fence.get());
            if (!controller.Report().srEvaluated) Fail("D3D12 NGX evaluation not reported");
            controller.OnBatchSubmitted(attempt.useId,serial);
            queue->waitForCommandFence(fence.get());
            if (!nativeFence.d3d || nativeFence.d3d->GetCompletedValue()<submittedFenceValue)
                Fail("D3D12 NGX output fence did not complete");
            controller.ReleaseCompletedThrough(serial);
            const auto* pixel=static_cast<const uint16_t*>(readback->map());
            if (!pixel) Fail("D3D12 NGX readback map failed");
            for (unsigned channel=0;channel<4;++channel)
                if (!FiniteHalf(pixel[channel])) Fail("D3D12 NGX output pixel is nonfinite");
            const bool changed=(pixel[0]|pixel[1]|pixel[2]|pixel[3])!=0;
            std::printf("D3D12_NGX mode=%s render=%ux%u output=%ux%u first_half=%04x,%04x,%04x,%04x\n",
                quality==gpu::upscaling::DlssQuality::Dlaa?"dlaa":"quality",
                renderWidth,renderHeight,kOutputWidth,kOutputHeight,
                pixel[0],pixel[1],pixel[2],pixel[3]);
            readback->unmap();
            if (!changed) Fail("D3D12 NGX left zero output sentinel unchanged");
            controller.ReleaseFeatureAfterGpuDrain();
            if (!controller.NeedsFeatureRecreate(config)) Fail("D3D12 NGX feature was not retired");
        }
        std::puts("D3D12 NGX Quality SR and DLAA executed");
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr,"FAIL: %s\n",error.what());
        return 1;
    }
}
