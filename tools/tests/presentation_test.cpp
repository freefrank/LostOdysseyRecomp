#include <gpu/presentation.h>
#include <plume_render_interface.h>
#include <stdafx.h>
#include <cmath>
#include "presentation_capture.h"
#include "hdr_transfer/gpu_test.h"
namespace plume
{
#if LO_PLATFORM_MACOS
std::unique_ptr<RenderInterface> CreateMetalInterface();
#else
std::unique_ptr<RenderInterface> CreateD3D12Interface();
std::unique_ptr<RenderInterface> CreateVulkanInterface();
#endif
}
// Record two distinct compositions before submitting either. This catches
// descriptor reuse and premature framebuffer destruction in the FG UI path.
static int TestSeparatedUi(plume::RenderDevice* device)
{
    using namespace plume;
    constexpr uint32_t width = 3, height = 1, rowPixels = 64;
    auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
    if (!queue) return 1;
    auto commands = queue->createCommandList();
    auto fence = device->createCommandFence();
    gpu::Presentation presentation;
    if (!queue || !commands || !fence || !presentation.Init(device)) return 1;
    auto texture = [&](bool target) {
        return device->createTexture(RenderTextureDesc::Texture2D(width, height, 1,
            RenderFormat::R8G8B8A8_UNORM,
            target ? RenderTextureFlag::RENDER_TARGET : RenderTextureFlag::NONE));
    };
    auto scene = texture(false), uiA = texture(false), uiB = texture(false);
    auto targetA = texture(true), targetB = texture(true);
    auto upload = device->createBuffer(RenderBufferDesc::UploadBuffer(512 * 3));
    auto readA = device->createBuffer(RenderBufferDesc::ReadbackBuffer(256));
    auto readB = device->createBuffer(RenderBufferDesc::ReadbackBuffer(256));
    if (!scene || !uiA || !uiB || !targetA || !targetB || !upload || !readA || !readB) return 1;
    // Blue scene, red/green straight-alpha overlays at 0, 128/255 and 1.
    auto* data = static_cast<uint32_t*>(upload->map());
    if (!data) return 1;
    for (uint32_t x = 0; x < width; ++x) {
        const uint32_t alpha = x == 0 ? 0 : x == 1 ? 128 : 255;
        data[x] = 0xffff0000;
        data[128 + x] = (alpha << 24) | 0x000000ff;
        data[256 + x] = (alpha << 24) | 0x0000ff00;
    }
    upload->unmap();
    commands->begin();
    RenderTexture* inputs[] = {scene.get(), uiA.get(), uiB.get()};
    for (uint32_t i = 0; i < 3; ++i) {
        commands->barriers(RenderBarrierStage::COPY,
            RenderTextureBarrier(inputs[i], RenderTextureLayout::COPY_DEST));
        auto location = RenderTextureCopyLocation::PlacedFootprint(upload.get(),
            RenderFormat::R8G8B8A8_UNORM, width, height, 1, rowPixels, i * 512);
        commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(inputs[i]), location);
    }
    auto leaseA = presentation.DrawSeparatedUi(commands.get(), scene.get(), uiA.get(),
        targetA.get(), width, height, false);
    auto leaseB = presentation.DrawSeparatedUi(commands.get(), scene.get(), uiB.get(),
        targetB.get(), width, height, false);
    if (!leaseA || !leaseB) return 1;
    RenderTexture* targets[] = {targetA.get(), targetB.get()};
    RenderBuffer* readbacks[] = {readA.get(), readB.get()};
    for (uint32_t i = 0; i < 2; ++i) {
        commands->barriers(RenderBarrierStage::COPY,
            RenderTextureBarrier(targets[i], RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readbacks[i],
            RenderFormat::R8G8B8A8_UNORM, width, height, 1, rowPixels),
            RenderTextureCopyLocation::Subresource(targets[i]));
    }
    commands->end();
    const RenderCommandList* lists[] = {commands.get()};
    queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
    queue->waitForCommandFence(fence.get());
    leaseA.reset();
    leaseB.reset();
    const uint32_t expected[2][3] = {
        {0xffff0000, 0xff7f0080, 0xff0000ff},
        {0xffff0000, 0xff7f8000, 0xff00ff00}};
    bool pass = true;
    for (uint32_t i = 0; i < 2; ++i) {
        const auto* pixels = static_cast<const uint32_t*>(readbacks[i]->map());
        if (!pixels) return 1;
        for (uint32_t x = 0; x < width; ++x) {
            for (uint32_t shift = 0; shift < 32; shift += 8)
                pass &= std::abs(int((pixels[x] >> shift) & 255) -
                    int((expected[i][x] >> shift) & 255)) <= 1;
            printf("Separated UI frame=%u pixel=%u actual=%08x expected=%08x\n",
                i, x, pixels[x], expected[i][x]);
        }
        readbacks[i]->unmap();
    }
    printf("Separated UI alpha and concurrent resource leases: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}
#ifdef _WIN32
static int TestHdrSwapchain(plume::RenderDevice* device, bool vulkan)
{
    using namespace plume;
    // A hidden native window exercises DXGI negotiation without changing the
    // user's desktop HDR setting or presenting a test image.
    HWND window = CreateWindowExW(0, L"STATIC", L"HDR swapchain test", WS_POPUP,
        0, 0, 320, 180, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    if (!window) return 1;
    bool pass = false;
    {
        auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
        RenderSwapChainDesc desc(window, RenderFormat::R16G16B16A16_FLOAT, 3);
        desc.outputMode = RenderOutputMode::HDR_LINEAR;
        auto swapchain = queue ? queue->createSwapChain(desc) : nullptr;
        if (swapchain && !swapchain->isEmpty()) {
            const auto state = swapchain->getDisplayState();
            const auto repeat = swapchain->getDisplayState();
            pass = repeat.encoding == state.encoding && state.headroom >= 1.0f &&
                (!state.hdrActive || state.hdrSupported);
            if (!vulkan) pass &= swapchain->getFormat() == RenderFormat::R16G16B16A16_FLOAT &&
                state.encoding == RenderOutputEncoding::SCRGB && state.linearUnitNits == 80.0f;
            else pass &= !state.hdrStateKnown && (state.encoding == RenderOutputEncoding::SDR || state.hdrTransport);
            printf("HDR swapchain: Vulkan=%d encoding=%u active=%d transport=%d known=%d headroom=%f peak_nits=%f %s\n",
                vulkan, unsigned(state.encoding), state.hdrActive, state.hdrTransport, state.hdrStateKnown,
                state.headroom, state.peakNits, pass ? "PASS" : "FAIL");
            if (vulkan) {
                _putenv_s("LO_HDR_OUTPUT", "sdr");
                pass &= swapchain->resize() && swapchain->getDisplayState().encoding == RenderOutputEncoding::SDR;
                _putenv_s("LO_HDR_OUTPUT", "");
                pass &= swapchain->resize() && swapchain->getDisplayState().encoding == state.encoding;
                printf("Vulkan HDR -> SDR -> HDR swapchain renegotiation: %s\n",pass ? "PASS" : "FAIL");
            }
        }
    }
    DestroyWindow(window);
    return pass ? 0 : 1;
}
#endif

static int TestHdr(plume::RenderDevice* device)
{
    using namespace plume;
    auto queue = device->createCommandQueue(RenderCommandListType::DIRECT);
    if (!queue) return 1;
    auto commands = queue->createCommandList();
    auto fence = device->createCommandFence();
    gpu::Presentation presentation;
    if (!commands || !fence || !presentation.Init(device, RenderFormat::R16G16B16A16_FLOAT)) return 1;
    const uint16_t samples[] = {0, 0x3800, 0x3c00, 0x4000, 0x4400}; // gamma-encoded 0, .5, 1, 2, 4
    bool pass = true;
    for (unsigned mode = 0; mode < 4; ++mode) {
        const bool downsample = mode == 3;
        const uint32_t width = downsample ? 8u : 5u, height = downsample ? 8u : 1u;
        const uint32_t outputWidth = downsample ? 1u : width, outputHeight = 1;
        auto source = device->createTexture(RenderTextureDesc::Texture2D(width, height, 1, RenderFormat::R16G16B16A16_FLOAT));
        auto target = device->createTexture(RenderTextureDesc::Texture2D(outputWidth, outputHeight, 1,
            RenderFormat::R16G16B16A16_FLOAT, RenderTextureFlag::RENDER_TARGET));
        auto upload = device->createBuffer(RenderBufferDesc::UploadBuffer(256 * height));
        auto readback = device->createBuffer(RenderBufferDesc::ReadbackBuffer(256));
        if (!source || !target || !upload || !readback) return 1;
        auto* input = static_cast<uint16_t*>(upload->map());
        if (!input) return 1;
        for (uint32_t y = 0; y < height; ++y) for (uint32_t x = 0; x < width; ++x) {
            const auto sample = downsample ? uint16_t(0x4000) : samples[x];
            for (unsigned c = 0; c < 3; ++c) input[y * 128 + x * 4 + c] = sample;
            input[y * 128 + x * 4 + 3] = 0x3c00;
        }
        upload->unmap();
        const auto transform = gpu::hdr::MakeOutput(true, mode == 1, mode != 2, 200, 1000);
        presentation.SetOutputTransform(transform);
        commands->begin();
        commands->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(source.get(), RenderTextureLayout::COPY_DEST));
        commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(source.get()),
            RenderTextureCopyLocation::PlacedFootprint(upload.get(), RenderFormat::R16G16B16A16_FLOAT, width, height, 1, 32));
        presentation.Draw(commands.get(), source.get(), target.get(), width, height, outputWidth, outputHeight,
            gpu::PresentationOptions{gpu::Antialiasing::Off, gpu::ScalingFilter::Bilinear, false, true});
        commands->barriers(RenderBarrierStage::COPY, RenderTextureBarrier(target.get(), RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),
            RenderFormat::R16G16B16A16_FLOAT, outputWidth, outputHeight, 1, 32), RenderTextureCopyLocation::Subresource(target.get()));
        commands->end();
        const RenderCommandList* lists[] = {commands.get()};
        queue->executeCommandLists(lists, 1, nullptr, 0, nullptr, 0, fence.get());
        queue->waitForCommandFence(fence.get());
        const auto* pixels = static_cast<const uint16_t*>(readback->map());
        if (!pixels) return 1;
        for (uint32_t x = 0; x < outputWidth; ++x) {
            const float encoded = gpu::hdr::DecodeHalf(downsample ? uint16_t(0x4000) : samples[x]);
            const float linear = float(hdr_reference::Decode(encoded,true));
            const float expected = gpu::hdr::MapLinear({linear, linear, linear}, transform.peakRatio)[0] * transform.scale;
            const float actual = gpu::hdr::DecodeHalf(pixels[x * 4]);
            pass &= std::abs(actual - expected) <= std::max(0.005f, expected * 0.002f);
            if (mode != 2 && encoded > 1) pass &= actual > transform.scale;
            printf("HDR mode=%u sample=%u actual=%f expected=%f\n", mode, x, actual, expected);
        }
        readback->unmap();
        // These short-lived test targets may reuse the same CPU address on the
        // next iteration. Retire their framebuffer before destroying the image.
        presentation.ForgetTargets();
    }
    printf("HDR FP16 output, EDR normalization, SDR fallback and downsampling: %s\n", pass ? "PASS" : "FAIL");
    return pass ? 0 : 1;
}

static int TestHdrCalibration(plume::RenderDevice* device)
{
    using namespace plume;
    constexpr uint32_t width=100,height=40,sourcePitch=512,targetPitch=1024;
    auto queue=device->createCommandQueue(RenderCommandListType::DIRECT);
    if (!queue) return 1;
    auto commands=queue->createCommandList(); auto fence=device->createCommandFence();
    auto source=device->createTexture(RenderTextureDesc::Texture2D(width,height,1,RenderFormat::R8G8B8A8_UNORM));
    auto upload=device->createBuffer(RenderBufferDesc::UploadBuffer(sourcePitch*height));
    if (!commands || !fence || !source || !upload) return 1;
    auto* input=static_cast<uint32_t*>(upload->map());
    if (!input) return 1;
    for (unsigned y=0;y<height;++y) for (unsigned x=0;x<width;++x) input[y*128+x]=0xff808080;
    upload->unmap();
    auto scene=device->createTexture(RenderTextureDesc::Texture2D(50,height,1,RenderFormat::R16G16B16A16_FLOAT));
    auto sceneUpload=device->createBuffer(RenderBufferDesc::UploadBuffer(512*height));
    if (!scene || !sceneUpload) return 1;
    auto* sceneInput=static_cast<uint16_t*>(sceneUpload->map());
    if (!sceneInput) return 1;
    for (unsigned y=0;y<height;++y) for (unsigned x=0;x<50;++x) {
        for (unsigned c=0;c<3;++c) sceneInput[y*256+x*4+c]=x<25?0x3800:0x4000; // .5 and 2, extended gamma.
        sceneInput[y*256+x*4+3]=0x3c00;
    }
    sceneUpload->unmap();
    bool pass=true;
    for (unsigned mode=0;mode<6;++mode) {
        const bool scenePreview=mode>=3, pq=mode%3==2;
        const float peak=mode==3?600.0f:1000.0f;
        const auto format=pq ? RenderFormat::R10G10B10A2_UNORM : RenderFormat::R16G16B16A16_FLOAT;
        gpu::Presentation presentation;
        if (!presentation.Init(device,format)) return 1;
        presentation.SetOutputTransform(gpu::hdr::MakeOutput(true,mode%3==1,true,200,peak,pq));
        auto target=device->createTexture(RenderTextureDesc::Texture2D(width,height,1,format,RenderTextureFlag::RENDER_TARGET));
        auto readback=device->createBuffer(RenderBufferDesc::ReadbackBuffer(targetPitch*height));
        if (!target || !readback) return 1;
        commands->begin();
        commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(source.get(),RenderTextureLayout::COPY_DEST));
        commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(source.get()),
            RenderTextureCopyLocation::PlacedFootprint(upload.get(),RenderFormat::R8G8B8A8_UNORM,width,height,1,128));
        commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(scene.get(),RenderTextureLayout::COPY_DEST));
        commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(scene.get()),
            RenderTextureCopyLocation::PlacedFootprint(sceneUpload.get(),RenderFormat::R16G16B16A16_FLOAT,50,height,1,64));
        gpu::PresentationOptions options;
        options.hdrCalibration=true;
        if (scenePreview) options.calibrationScene=scene.get();
        options.calibrationRect[0]=options.calibrationRect[1]=0.1f;
        options.calibrationRect[2]=options.calibrationRect[3]=0.9f;
        presentation.Draw(commands.get(),source.get(),target.get(),width,height,width,height,options);
        commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(target.get(),RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),format,width,height,1,targetPitch/(pq?4:8)),
            RenderTextureCopyLocation::Subresource(target.get()));
        commands->end();
        const RenderCommandList* lists[]={commands.get()};
        queue->executeCommandLists(lists,1,nullptr,0,nullptr,0,fence.get()); queue->waitForCommandFence(fence.get());
        const auto* data=static_cast<const uint8_t*>(readback->map());
        if (!data) return 1;
        const unsigned xs[]={20,scenePreview?40u:30u,60,scenePreview?80u:70u,5};
        const unsigned ys[]={scenePreview?20u:10u,20,scenePreview?20u:10u,20,20};
        const float dark=float(hdr_reference::Srgb(0.5))*200;
        const float light=std::pow(2.0f,2.2f);
        const float mapped=gpu::hdr::MapLinear({light,light,light},peak/200)[0]*200;
        const float expected[]={scenePreview?dark:200,scenePreview?200.0f:180.0f,
            scenePreview?dark:1000,scenePreview?mapped:900,float(hdr_reference::Srgb(128.0/255))*200};
        for (unsigned i=0;i<5;++i) {
            float nits;
            if (pq) nits=gpu::hdr::DecodePq10(reinterpret_cast<const uint32_t*>(data+ys[i]*targetPitch)[xs[i]])[0];
            else nits=gpu::hdr::DecodeHalf(reinterpret_cast<const uint16_t*>(data+ys[i]*targetPitch)[xs[i]*4])*(mode%3==1?200:80);
            pass &= std::abs(nits-expected[i]) < std::max(1.0f,expected[i]*0.012f);
            printf("Calibration mode=%u sample=%u nits=%f expected=%f\n",mode,i,nits,expected[i]);
        }
        readback->unmap();
    }
    printf("HDR calibration pattern, frozen scene, peak adjustment, menu transfer and PQ10: %s\n",pass?"PASS":"FAIL");
    return pass?0:1;
}

int main(int argc, char** argv)
{
    using namespace plume;
#if LO_PLATFORM_MACOS
    auto api = CreateMetalInterface();
    const char* backend = "Metal";
#else
    const bool vulkan=argc>1 && std::string(argv[1])=="--vulkan";
    if(vulkan){--argc;++argv;}
    auto api = vulkan ? CreateVulkanInterface() : CreateD3D12Interface();
    const char* backend = vulkan ? "Vulkan" : "D3D12";
#endif
    if(!api)return 2;
    auto device = api->createDevice();
    if(!device)return 2;
    printf("Backend: %s on %s\n",backend,device->getDescription().name.c_str());
#ifdef _WIN32
    if (argc == 2 && std::string(argv[1]) == "--hdr-swapchain-only")
        return TestHdrSwapchain(device.get(),vulkan);
#endif
    if (argc == 2 && std::string(argv[1]) == "--hdr-only") return TestHdr(device.get());
    if (argc == 2 && std::string(argv[1]) == "--hdr-transfer-only") return hdr_transfer_fixture::Test(device.get());
    if (argc == 2 && std::string(argv[1]) == "--hdr-calibration-only") return TestHdrCalibration(device.get());
    if (argc == 2 && std::string(argv[1]) == "--separated-ui-only")
        return TestSeparatedUi(device.get());
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
        // The fence has completed; discard cached framebuffers before this
        // render's caller-owned target is destroyed and its address is reused.
        presentation.ForgetTargets();
        scenePresentation.ForgetTargets();
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
