#pragma once
#include "reference.h"
#include <gpu/display_gamma.h>

namespace hdr_transfer_fixture {
using namespace plume;
using hdr_reference::Color;
inline uint16_t Half(float f) {
    // Round-to-nearest-even, including the subnormal range used by dark ramps.
    uint32_t bits=std::bit_cast<uint32_t>(f);
    const uint32_t sign=(bits>>16)&0x8000;
    int exponent=int((bits>>23)&255)-127+15;
    uint32_t mantissa=bits&0x7fffff;
    if (exponent<=0) {
        if (exponent < -10) return uint16_t(sign);
        mantissa|=0x800000;
        unsigned shift=unsigned(14-exponent);
        return uint16_t(sign+((mantissa+((1u<<(shift-1))-1)+((mantissa>>shift)&1))>>shift));
    }
    if (exponent>=31) return uint16_t(sign|0x7bff);
    uint32_t packed=(uint32_t(exponent)<<10)|(mantissa>>13);
    if ((mantissa&8191)>4096 || ((mantissa&8191)==4096 && (packed&1))) ++packed;
    return uint16_t(sign|packed);
}
inline void SetRamp(bool guest) {
    using namespace gpu::display_gamma;
    OnRegisterWrite(RegRwMode,0);
    OnRegisterWrite(RegWriteEnableMask,7);
    OnRegisterWrite(RegRwIndex,0);
    for (unsigned i=0;i<256;++i) {
        const uint32_t v=hdr_reference::GuestEntry(i,!guest);
        OnRegisterWrite(Reg30Color,v|(v<<10)|(v<<20));
    }
}
struct Harness {
    RenderDevice* device;
    std::unique_ptr<gpu::Presentation> presentation;
    RenderFormat initializedFormat=RenderFormat::UNKNOWN;
    std::unique_ptr<RenderCommandQueue> queue;
    std::unique_ptr<RenderCommandList> commands;
    std::unique_ptr<RenderCommandFence> fence;
    explicit Harness(RenderDevice* d):device(d),queue(d->createCommandQueue(RenderCommandListType::DIRECT)) {
        if (queue) { commands=queue->createCommandList(); fence=d->createCommandFence(); }
    }
    bool Submit() {
        commands->end();
        const RenderCommandList* lists[]={commands.get()};
        queue->executeCommandLists(lists,1,nullptr,0,nullptr,0,fence.get());
        queue->waitForCommandFence(fence.get());
        return true;
    }
    // Off-screen render targets exercise the exact production Draw call and
    // runtime DXC compilation. No WSI, panel, or HDR display is simulated here.
    std::vector<Color> Run(const std::vector<Color>& samples, const gpu::hdr::OutputTransform& output,
                          RenderFormat format, gpu::PresentationOptions options,
                          bool gain=false, bool preview=false, bool overlay=false,
                          const std::vector<Color>* gainPixels=nullptr) {
        const uint32_t width=uint32_t(samples.size()), height=1;
        const uint32_t sourcePitch=(width*8+255)&~255u, targetPitch=(width*(format==RenderFormat::R16G16B16A16_FLOAT?8:4)+255)&~255u;
        if (!queue || !commands || !fence) return {};
        if (!presentation || initializedFormat!=format) {
            presentation=std::make_unique<gpu::Presentation>();
            if (!presentation->Init(device,format)) return {};
            initializedFormat=format;
        }
        presentation->SetOutputTransform(output);
        auto source=device->createTexture(RenderTextureDesc::Texture2D(width,height,1,RenderFormat::R16G16B16A16_FLOAT));
        auto gainSource=gain?device->createTexture(RenderTextureDesc::Texture2D(width,height,1,RenderFormat::R16G16B16A16_FLOAT)):nullptr;
        auto target=device->createTexture(RenderTextureDesc::Texture2D(width,height,1,format,RenderTextureFlag::RENDER_TARGET));
        auto upload=device->createBuffer(RenderBufferDesc::UploadBuffer(sourcePitch));
        auto gainUpload=gainPixels?device->createBuffer(RenderBufferDesc::UploadBuffer(sourcePitch)):nullptr;
        auto readback=device->createBuffer(RenderBufferDesc::ReadbackBuffer(targetPitch));
        if (!source || !target || !upload || !readback) return {};
        auto* data=static_cast<uint16_t*>(upload->map());
        if (!data) return {};
        for (uint32_t x=0;x<width;++x) {
            for (unsigned c=0;c<3;++c) data[4*x+c]=Half(float(samples[x][c]));
            data[4*x+3]=0x3c00;
        }
        upload->unmap();
        if (gainPixels) {
            if (!gainUpload || gainPixels->size()!=samples.size()) return {};
            auto* hdr=static_cast<uint16_t*>(gainUpload->map());
            if (!hdr) return {};
            for (uint32_t x=0;x<width;++x) {
                for (unsigned c=0;c<3;++c) hdr[4*x+c]=Half(float((*gainPixels)[x][c]));
                hdr[4*x+3]=0x3c00;
            }
            gainUpload->unmap();
        }
        commands->begin();
        commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(source.get(),RenderTextureLayout::COPY_DEST));
        commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(source.get()),
            RenderTextureCopyLocation::PlacedFootprint(upload.get(),RenderFormat::R16G16B16A16_FLOAT,width,height,1,sourcePitch/8));
        RenderTexture* input=source.get();
        if (gain) {
            // On sub-white pixels the real ComposeHdrGain leaves SDR unchanged.
            if (!gainSource) return {};
            commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(gainSource.get(),RenderTextureLayout::COPY_DEST));
            commands->copyTextureRegion(RenderTextureCopyLocation::Subresource(gainSource.get()),
                RenderTextureCopyLocation::PlacedFootprint(gainPixels?gainUpload.get():upload.get(),RenderFormat::R16G16B16A16_FLOAT,width,height,1,sourcePitch/8));
            input=presentation->ComposeHdrGain(commands.get(),source.get(),gainSource.get(),width,height,width,height,width,height);
            if (!input) return {};
        }
        if (preview) {
            options.calibrationScene=input;
            options.calibrationRect[0]=options.calibrationRect[1]=0;
            options.calibrationRect[2]=options.calibrationRect[3]=1;
        }
        presentation->Draw(commands.get(),input,target.get(),width,height,width,height,options);
        if (overlay) {
            presentation->DrawOverlay(commands.get(),input,target.get(),0,0,width,height,width,height,1);
        }
        commands->barriers(RenderBarrierStage::COPY,RenderTextureBarrier(target.get(),RenderTextureLayout::COPY_SOURCE));
        commands->copyTextureRegion(RenderTextureCopyLocation::PlacedFootprint(readback.get(),format,width,height,1,
            targetPitch/(format==RenderFormat::R16G16B16A16_FLOAT?8:4)),RenderTextureCopyLocation::Subresource(target.get()));
        Submit();
        std::vector<Color> result(width);
        const auto* bytes=static_cast<const uint8_t*>(readback->map());
        if (!bytes) return {};
        for (uint32_t x=0;x<width;++x) for (unsigned c=0;c<3;++c) {
            if (format==RenderFormat::R16G16B16A16_FLOAT)
                result[x][c]=gpu::hdr::DecodeHalf(reinterpret_cast<const uint16_t*>(bytes)[x*4+c]);
            else if (format==RenderFormat::R10G10B10A2_UNORM)
                result[x][c]=double((reinterpret_cast<const uint32_t*>(bytes)[x]>>(c*10))&1023)/1023;
            else result[x][c]=double(bytes[x*4+c])/255;
        }
        if (!output.linear) if (const char* dump=std::getenv("LO_HDR_FIXTURE_SDR_DUMP")) {
            static unsigned serial=0;
            std::ofstream file(std::string(dump)+"-"+std::to_string(serial++)+".bin",std::ios::binary);
            file.write(reinterpret_cast<const char*>(bytes),std::streamsize(width*4));
        }
        readback->unmap();
        presentation->ForgetTargets();
        return result;
    }
};
inline int Test(RenderDevice* device) {
    Harness test(device);
    const auto original=hdr_reference::Samples();
    bool pass=true;
    unsigned comparisons=0,failures=0,cases=0;
    double maxDiffuseError=0;
    const auto check=[&](const std::vector<Color>& actual,const std::vector<Color>& expected,
                         RenderFormat format,const char* name) {
        ++cases;
        if (actual.size()!=expected.size()) { std::fprintf(stderr,"%s: missing GPU result\n",name); pass=false; return; }
        for (size_t i=0;i<expected.size();++i) for (unsigned c=0;c<3;++c) {
            ++comparisons;
            const double tolerance=format==RenderFormat::R16G16B16A16_FLOAT ? std::max(2e-7,std::abs(expected[i][c])*.0015)
                : format==RenderFormat::R10G10B10A2_UNORM ? 1.1/1023 : 1.1/255;
            const double error=std::abs(actual[i][c]-expected[i][c]);
            if (!std::isfinite(actual[i][c]) || error>tolerance) {
                if (failures++<12) std::fprintf(stderr,"%s pixel=%zu channel=%u actual=%.10g reference=%.10g tolerance=%.3g\n",
                    name,i,c,actual[i][c],expected[i][c],tolerance);
                pass=false;
            }
        }
    };
    for (bool guest : {false,true}) for (bool extended : {false,true}) {
        SetRamp(guest);
        auto samples=original;
        // SDR fallback/video inputs have no extended highlights.
        if (!extended) for (auto& color:samples) for (auto& v:color) v=std::min(v,1.0);
        // CPU expected values use the texture's actual FP16 values, not the
        // unquantized synthetic inputs. FP16 encode is checked separately below.
        for (auto& color:samples) for (auto& v:color) v=gpu::hdr::DecodeHalf(Half(float(v)));
        for (bool pq : {false,true}) for (double white : {80.,203.,400.}) for (double peak : {400.,1000.,10000.}) {
            const auto format=pq?RenderFormat::R10G10B10A2_UNORM:RenderFormat::R16G16B16A16_FLOAT;
            const auto output=gpu::hdr::MakeOutput(true,false,true,float(white),float(peak),pq);
            gpu::PresentationOptions options;
            options.hdrScene=extended; options.displayGammaRamp=guest;
            auto actual=test.Run(samples,output,format,options);
            std::vector<Color> expected;
            for (auto c:samples) expected.push_back(hdr_reference::Output(hdr_reference::Adjust(c,guest,false),true,extended,output.scale,output.peakRatio,pq));
            check(actual,expected,format,extended?"extended scene":"SDR/video on HDR chain");
            if (!pq && actual.size()==samples.size()) for (size_t i=0;i<512;++i) {
                const auto c=hdr_reference::Adjust(samples[i],guest,false);
                for (unsigned ch=0;ch<3;++ch) maxDiffuseError=std::max(maxDiffuseError,
                    std::abs(actual[i][ch]/output.scale-hdr_reference::Srgb(c[ch])));
            }
        }
        // The range/user curves must stay before the single output decode.
        for (int brightness : {-10,0,10}) for (unsigned gamma : {80u,100u,130u}) {
            gpu::PresentationOptions options;
            options.hdrScene=extended; options.displayGammaRamp=guest;
            options.displayAdjust=true; options.displayBrightness=brightness; options.displayGamma=gamma;
            options.expandRgbRange=true;
            const auto output=gpu::hdr::MakeOutput(true,false,true,203,1000);
            auto actual=test.Run(samples,output,RenderFormat::R16G16B16A16_FLOAT,options);
            std::vector<Color> expected;
            for (auto c:samples) {
                if (guest) for (auto& v:c) v=hdr_reference::Lut(v,true,0,100);
                for (auto& v:c) v=std::max((v-16./255)*(255./219),0.0);
                if (!extended) for (auto& v:c) v=std::min(v,1.0);
                if (brightness || gamma!=100) for (auto& v:c) v=hdr_reference::Lut(v,false,brightness,gamma);
                expected.push_back(hdr_reference::Output(c,true,extended,output.scale,output.peakRatio,false));
            }
            check(actual,expected,RenderFormat::R16G16B16A16_FLOAT,"range + user curve + guest LUT");
        }
    }
    SetRamp(false);
    // Actual output transformation changes on the same Presentation object:
    // SDR -> active scRGB -> inactive scRGB -> PQ -> SDR, with black anchored.
    std::vector<Color> diffuse(original.begin(),original.begin()+512);
    for (auto& c:diffuse) for (auto& v:c) v=gpu::hdr::DecodeHalf(Half(float(v)));
    for (unsigned mode : {0u,1u,2u,3u,0u}) {
        const bool linear=mode!=0,pq=mode==3,active=mode==1||pq;
        const auto format=pq?RenderFormat::R10G10B10A2_UNORM:linear?RenderFormat::R16G16B16A16_FLOAT:RenderFormat::R8G8B8A8_UNORM;
        const auto output=gpu::hdr::MakeOutput(linear,false,active,203,1000,pq);
        gpu::PresentationOptions options; options.hdrScene=active;
        auto actual=test.Run(diffuse,output,format,options);
        std::vector<Color> expected;
        for (auto c:diffuse) expected.push_back(hdr_reference::Output(c,linear,active,output.scale,output.peakRatio,pq));
        check(actual,expected,format,"HDR/SDR transform transitions");
    }
    // EDR normalization, host overlays, and gain reconstruction use the same
    // sub-white display response. Real display headroom is outside this fixture.
    for (unsigned path=0;path<3;++path) {
        const auto output=gpu::hdr::MakeOutput(true,path==0,true,203,1000,path==1);
        const auto format=path==1?RenderFormat::R10G10B10A2_UNORM:RenderFormat::R16G16B16A16_FLOAT;
        gpu::PresentationOptions options; options.hdrScene=path==2;
        auto actual=test.Run(diffuse,output,format,options,path==2,false,path==1);
        std::vector<Color> expected;
        for (auto c:diffuse) expected.push_back(hdr_reference::Output(c,true,path==2,output.scale,output.peakRatio,path==1));
        check(actual,expected,format,path==0?"EDR":path==1?"opaque host UI on PQ":"upscaled SDR + gain");
    }
    {
        auto hdr=original,sdr=original;
        for (size_t i=0;i<hdr.size();++i) for (unsigned c=0;c<3;++c) {
            hdr[i][c]=gpu::hdr::DecodeHalf(Half(float(hdr[i][c])));
            sdr[i][c]=gpu::hdr::DecodeHalf(Half(float(std::min(sdr[i][c],1.0))));
        }
        const auto output=gpu::hdr::MakeOutput(true,false,true,203,1000);
        gpu::PresentationOptions options; options.hdrScene=true;
        auto actual=test.Run(sdr,output,RenderFormat::R16G16B16A16_FLOAT,options,true,false,false,&hdr);
        std::vector<Color> expected;
        for (size_t i=0;i<sdr.size();++i) {
            auto c=sdr[i];
            double t=std::clamp((*std::max_element(c.begin(),c.end())-.8)/.2,0.0,1.0);
            const double w=t*t*(3-2*t);
            for (unsigned ch=0;ch<3;++ch)
                c[ch]=gpu::hdr::DecodeHalf(Half(float(c[ch]*(1+w*(std::max(hdr[i][ch],1.0)-1)))));
            expected.push_back(hdr_reference::Output(c,true,true,output.scale,output.peakRatio,false));
        }
        check(actual,expected,RenderFormat::R16G16B16A16_FLOAT,"upscaler gain with mixed highlights");
    }
    // Spatial AA must neither add another transfer nor lose extended white.
    // Uniform fixtures test transfer isolation; the pre-existing diagonal and
    // crop fixtures in presentation_sdr_gpu separately exercise edge decisions.
    for (auto aa : {gpu::Antialiasing::Off,gpu::Antialiasing::FXAA,gpu::Antialiasing::SMAA})
        for (double value : {.02,.05,.25,.5,2.,4.}) {
            value=gpu::hdr::DecodeHalf(Half(float(value)));
            const std::vector<Color> input(16,Color{value,value,value});
            const auto output=gpu::hdr::MakeOutput(true,false,true,203,1000);
            gpu::PresentationOptions options; options.hdrScene=true; options.antialiasing=aa;
            auto actual=test.Run(input,output,RenderFormat::R16G16B16A16_FLOAT,options);
            std::vector<Color> expected(16,hdr_reference::Output(input[0],true,true,output.scale,output.peakRatio,false));
            check(actual,expected,RenderFormat::R16G16B16A16_FLOAT,"HDR AA transfer isolation");
        }
    // Frozen calibration and brightness previews: each half must use exactly
    // the gameplay transfer; a constant texture avoids resampling ambiguity.
    for (bool hdrPage : {false,true}) for (double value : {.002,.02,.05,.25,.5,1.,2.,4.}) {
        value=gpu::hdr::DecodeHalf(Half(float(value)));
        const std::vector<Color> input(16,Color{value,value,value});
        const auto output=gpu::hdr::MakeOutput(true,false,true,203,1000);
        gpu::PresentationOptions options; options.hdrCalibration=hdrPage;
        options.brightnessPreview=!hdrPage; options.calibrationSceneExtended=true;
        auto actual=test.Run(input,output,RenderFormat::R16G16B16A16_FLOAT,options,false,true);
        std::vector<Color> expected;
        for (unsigned i=0;i<16;++i) {
            auto c=hdr_reference::Output(input[i],true,true,1,output.peakRatio,false);
            if (hdrPage && i<8) for (auto& v:c) v=std::min(v,1.0);
            for (auto& v:c) v*=output.scale;
            expected.push_back(c);
        }
        check(actual,expected,RenderFormat::R16G16B16A16_FLOAT,hdrPage?"HDR peak scene preview":"brightness scene preview");
    }
    std::printf("HDR transfer: %u cases, %u RGB comparisons, %u failures; maximum diffuse linear error %.9g: %s\n",
        cases,comparisons,failures,maxDiffuseError,pass?"PASS":"FAIL");
    return pass?0:1;
}
} // namespace hdr_transfer_fixture
