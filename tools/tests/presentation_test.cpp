#include <gpu/presentation.h>
#include <plume_render_interface.h>
#include <stdafx.h>
#include <cmath>
#include "presentation_capture.h"
namespace plume
{
std::unique_ptr<RenderInterface> CreateD3D12Interface();
}
int main(int argc, char** argv)
{
    using namespace plume;
    auto api = CreateD3D12Interface();
    auto device = api->createDevice();
    if (argc == 6 && std::string(argv[1]) == "--capture")
        return ReplayPresentationCapture(device.get(), argv[2], std::stoul(argv[3]), std::stoul(argv[4]), argv[5]);
    if (argc != 1) return 2;
    auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
    auto commands = queue->createCommandList();
    auto fence = device->createCommandFence();
    gpu::Presentation presentation;
    if (!presentation.Init(device.get()))
        return 1;
    auto source = device->createTexture(RenderTextureDesc::Texture2D(40, 20, 1, RenderFormat::R8G8B8A8_UNORM));
    auto upload = device->createBuffer(RenderBufferDesc::UploadBuffer(256 * 20));
    auto *data = static_cast<uint32_t *>(upload->map());
    for (unsigned y = 0; y < 20; y++)
        for (unsigned x = 0; x < 40; x++)
            data[y * 64 + x] = x >= 32 || y >= 16 ? 0xffff00ff : (x > y + 8 ? 0xffffffff : 0xff000000);
    upload->unmap();
    auto submit = [&] {
        commands->end();
        const RenderCommandList *lists[] = {commands.get()};
        queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
        queue->waitForCommandFence(fence.get());
    };
    commands->begin();
    commands->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(source.get(), RenderTextureLayout::COPY_DEST));
    commands->copyTextureRegion(
        RenderTextureCopyLocation::Subresource(source.get()),
        RenderTextureCopyLocation::PlacedFootprint(upload.get(), RenderFormat::R8G8B8A8_UNORM, 40, 20, 1, 64));
    submit();
    gpu::Presentation scenePresentation;
    if (!scenePresentation.Init(device.get())) return 1;
    auto render = [&](unsigned w, unsigned h, gpu::Antialiasing aa, unsigned sw = 32, unsigned sh = 16, gpu::ScalingFilter filter = gpu::ScalingFilter::Bilinear, unsigned path = 0) {
        auto target = device->createTexture(
            RenderTextureDesc::Texture2D(w, h, 1, RenderFormat::R8G8B8A8_UNORM, RenderTextureFlag::RENDER_TARGET));
        auto readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(256 * h));
        commands->begin();
        bool recorded = true;
        if (path == 1)
            recorded = w == sw && h == sh && scenePresentation.ProcessSceneColor(commands.get(), source.get(), target.get(), sw, sh, aa);
        else if (path == 2)
            presentation.DrawComposited(commands.get(), source.get(), target.get(), sw, sh, w, h, filter);
        else
            presentation.Draw(commands.get(), source.get(), target.get(), sw, sh, w, h, gpu::PresentationOptions{aa, filter});
        commands->barriers(RenderBarrierStage::COPY,
                           RenderTextureBarrier(target.get(), RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(
            RenderTextureCopyLocation::PlacedFootprint(readback.get(), RenderFormat::R8G8B8A8_UNORM, w, h, 1, 64),
            RenderTextureCopyLocation::Subresource(target.get()));
        submit();
        auto *mapped = static_cast<uint32_t *>(readback->map());
        std::vector<uint32_t> result(w * h);
        for (unsigned y = 0; y < h; y++)
            memcpy(result.data() + w * y, mapped + 64 * y, w * 4);
        readback->unmap();
        if (!recorded) result.clear();
        return result;
    };
    bool pass = true;
    const auto identity = render(32, 16, gpu::Antialiasing::Off);
    for (unsigned y = 0; y < 16; y++)
        for (unsigned x = 0; x < 32; x++)
            pass &= identity[y * 32 + x] == (x > y + 8 ? 0xffffffffu : 0xff000000u);
    printf("Native-resolution identity: %s\n", pass ? "PASS" : "FAIL");
    const auto scaled = render(64, 64, gpu::Antialiasing::Off);
    bool bars = true;
    for (unsigned y = 0; y < 64; y++)
        for (unsigned x = 0; x < 64; x++)
            if (y < 16 || y >= 48)
                bars &= scaled[y * 64 + x] == 0xff000000;
    pass &= bars;
    printf("Aspect ratio / letterboxing: %s\n", bars ? "PASS" : "FAIL");
    const auto filtered = render(32, 16, gpu::Antialiasing::FXAA);
    unsigned changed = 0, intermediate = 0;
    for (size_t i = 0; i < filtered.size(); i++)
    {
        changed += filtered[i] != identity[i];
        unsigned c = filtered[i] & 255;
        intermediate += c > 0 && c < 255;
    }
    pass &=
        changed > 0 && intermediate > 0 && filtered.front() == identity.front() && filtered.back() == identity.back();
    printf("FXAA diagonal: %u changed pixels, %u intermediate pixels; flat regions preserved\n", changed, intermediate);
    const auto smaa = render(32,16,gpu::Antialiasing::SMAA);
    changed=0;intermediate=0;
    bool clean=true;
    for(size_t i=0;i<smaa.size();++i) {
        changed+=smaa[i]!=identity[i];
        unsigned r=smaa[i]&255,g=(smaa[i]>>8)&255,b=(smaa[i]>>16)&255;
        intermediate+=r>0&&r<255;
        clean &= r==g && g==b; // No magenta padded-storage leakage.
    }
    bool smaaPass=changed>0&&intermediate>0&&clean&&smaa.front()==identity.front()&&smaa.back()==identity.back();
    pass &= smaaPass;
    printf("SMAA 1x diagonal: %u changed / %u intermediate; flat regions and padding: %s\n",changed,intermediate,smaaPass?"PASS":"FAIL");
    auto boxed=render(64,64,gpu::Antialiasing::SMAA);
    bars=true;
    for(unsigned y=0;y<64;++y) for(unsigned x=0;x<64;++x)
        if(y<16||y>=48) bars &= boxed[y*64+x]==0xff000000u;
    pass &= bars;
    printf("SMAA letterboxing: %s\n",bars?"PASS":"FAIL");
    auto resizedSmallFrame=render(16,8,gpu::Antialiasing::SMAA,16,8);
    auto resized=render(32,16,gpu::Antialiasing::SMAA);
    bool resize=resized==smaa&&resizedSmallFrame.front()==0xff000000u;
    pass &= resize;
    printf("SMAA valid-extent resize and restoration: %s\n",resize?"PASS":"FAIL");
    auto downsample=render(16,8,gpu::Antialiasing::SMAA);
    pass &= downsample.front()==0xff000000u&&downsample.back()==0xffffffffu;
    const auto nativeHigh=render(32,16,gpu::Antialiasing::Off,32,16,gpu::ScalingFilter::Bicubic);
    bool highIdentity=nativeHigh==identity;
    pass &= highIdentity;
    printf("High-quality native identity with padding: %s\n",highIdentity?"PASS":"FAIL");
    const auto linearUp=render(64,32,gpu::Antialiasing::Off);
    const auto cubicUp=render(64,32,gpu::Antialiasing::Off,32,16,gpu::ScalingFilter::Bicubic);
    bool upDifferent=linearUp!=cubicUp;
    pass &= upDifferent;
    printf("Cubic upscale differs from bilinear: %s\n",upDifferent?"PASS":"FAIL");
    auto refill=[&](auto pattern) {
        auto *pixels=static_cast<uint32_t *>(upload->map());
        for(unsigned yy=0;yy<20;++yy) for(unsigned xx=0;xx<40;++xx)
            pixels[yy*64+xx]=xx>=32||yy>=16?0xffff00ffu:pattern(xx,yy);
        upload->unmap();
        commands->begin();
        commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(source.get(),RenderTextureLayout::COPY_DEST));
        commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(source.get()),
            RenderTextureCopyLocation::PlacedFootprint(upload.get(),RenderFormat::R8G8B8A8_UNORM,40,20,1,64));
        submit();
    };
    // Check that choosing an upscale filter does not move FXAA to output space:
    // direct FXAA+scale must equal scaling the separately rendered native AA frame.
    const auto fxaaThenCubic=render(64,32,gpu::Antialiasing::FXAA,32,16,gpu::ScalingFilter::Bicubic);
    refill([&](unsigned xx,unsigned yy) { return filtered[yy*32+xx]; });
    const auto cubicAfterFxaa=render(64,32,gpu::Antialiasing::Off,32,16,gpu::ScalingFilter::Bicubic);
    bool aaOrder=fxaaThenCubic==cubicAfterFxaa;
    pass &= aaOrder;
    printf("FXAA at input size before cubic scaling: %s\n",aaOrder?"PASS":"FAIL");
    refill([](unsigned xx,unsigned) { return xx<16?0xff404040u:0xffc0c0c0u; });
    auto step=render(64,32,gpu::Antialiasing::Off,32,16,gpu::ScalingFilter::Bicubic);
    bool bounded=true;
    for(auto pixel:step) {
        unsigned r=pixel&255,g=(pixel>>8)&255,b=(pixel>>16)&255;
        bounded &= r>=64&&r<=192&&r==g&&g==b;
    }
    bounded &= step.front()==0xff404040u&&step.back()==0xffc0c0c0u;
    pass &= bounded;
    printf("Cubic flat regions / no overshoot or padding bleed: %s\n",bounded?"PASS":"FAIL");
    refill([](unsigned xx,unsigned yy) { return (xx+yy)%2?0xffffffffu:0xff000000u; });
    auto checkReduction=[&](unsigned rw,unsigned rh) {
        auto reduced=render(rw,rh,gpu::Antialiasing::Off,32,16,gpu::ScalingFilter::Bicubic);
        bool correct=true;
        for(unsigned yy=0;yy<rh;++yy) for(unsigned xx=0;xx<rw;++xx) {
            double left=double(xx)*32/rw,right=double(xx+1)*32/rw;
            double top=double(yy)*16/rh,bottom=double(yy+1)*16/rh,total=0;
            for(unsigned sy=unsigned(top);sy<unsigned(std::ceil(bottom));++sy)
                for(unsigned sx=unsigned(left);sx<unsigned(std::ceil(right));++sx) {
                    double coverX=std::min(right,double(sx+1))-std::max(left,double(sx));
                    double coverY=std::min(bottom,double(sy+1))-std::max(top,double(sy));
                    total+=((sx+sy)%2?255:0)*coverX*coverY;
                }
            double expected=total/((right-left)*(bottom-top));
            unsigned pixel=reduced[yy*rw+xx],r=pixel&255,g=(pixel>>8)&255,b=(pixel>>16)&255;
            correct &= std::abs(double(r)-expected)<=1.0&&r==g&&g==b;
        }
        pass &= correct;
        printf("Checkerboard %ux%u full-footprint reduction: %s\n",rw,rh,correct?"PASS":"FAIL");
    };
    checkReduction(8,4);
    checkReduction(10,5); // Fractional coverage, not fixed tap offsets.
    checkReduction(2,1); // More than 4x: multistage path.
    // Stage a scene AA result, then composite sharp opaque UI strokes into it.
    // Final presentation must preserve the entire composited source at native
    // size, while the scene beneath it must already have received actual AA.
    for (auto aa : {gpu::Antialiasing::FXAA, gpu::Antialiasing::SMAA}) {
        refill([](unsigned xx,unsigned yy) { return xx>yy+8?0xffffffffu:0xff000000u; });
        const auto scene=render(32,16,aa,32,16,gpu::ScalingFilter::Bilinear,1);
        const auto reference=render(32,16,aa);
        // Scene ROI surrounds the diagonal and is disjoint from the UI mask.
        unsigned sceneChanged=0;
        if (scene.size()==identity.size())
            for(unsigned yy=2;yy<14;++yy) for(unsigned xx=8;xx<26;++xx)
                sceneChanged+=scene[yy*32+xx]!=identity[yy*32+xx];
        bool sceneValid=scene.size()==identity.size() && scene==reference && sceneChanged>0;
        if (!sceneValid) { pass=false;printf("Pre-UI scene AA response: FAIL\n");continue; }
        auto composited=scene;
        for(unsigned yy=2;yy<14;++yy) for(unsigned xx=2;xx<7;++xx)
            composited[yy*32+xx]=(xx==2+(yy-2)/3||yy==7)?0xffffffffu:0xff000000u;
        refill([&](unsigned xx,unsigned yy) { return composited[yy*32+xx]; });
        const auto finalImage=render(32,16,aa,32,16,gpu::ScalingFilter::Bicubic,2);
        bool preserved=finalImage==composited;
        // A deliberately incorrect second AA pass must differ: this ensures the
        // fixture actually detects the lettering operation we are excluding.
        const auto doubleAa=render(32,16,aa);
        unsigned uiChangedBySecondPass=0;
        for(unsigned yy=2;yy<14;++yy) for(unsigned xx=2;xx<7;++xx)
            uiChangedBySecondPass+=doubleAa[yy*32+xx]!=composited[yy*32+xx];
        bool detectsSecondPass=uiChangedBySecondPass>0;
        pass &= preserved && detectsSecondPass;
        printf("Pre-UI %s + composited UI bypass / negative control: %s (scene ROI changed=%u, second AA UI mask changed=%u)\n",
               aa==gpu::Antialiasing::FXAA?"FXAA":"SMAA",preserved&&detectsSecondPass?"PASS":"FAIL",sceneChanged,uiChangedBySecondPass);
    }
    return pass ? 0 : 1;
}
