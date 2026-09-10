#pragma once
#include "temporal_math.h"
#include <vector>
#include <string>
#include <cstring>
#include <bit>

namespace gpu::taa_collection {
struct SparseFrame {
    uint64_t frame=0, epoch=0, consentEpoch=0;
    double seconds=0;
    uint32_t width=0,height=0,flags=0;
    float jitter[4]{};
    std::optional<temporal::Camera> current,previous;
    std::array<float,576> depth{};
};
bool WantSparse();
uint64_t ConsentEpoch() noexcept;
void SubmitSparse(SparseFrame frame);

// Packet v1: 16-byte header, then 32 fixed 4824-byte frames. All little endian.
// Frame: u64 frame/epoch, f64 monotonic seconds, u32 width/height/flags,
// float4 current/previous jitter, 2 float4x4 VP, 2 float4 raster conventions,
// u32 reserved, 576 records (half2 MV previous-current pixels, float reversed depth).
inline constexpr size_t SparseStride=4824, SparseBytes=16+32*SparseStride;
inline uint16_t Half(float f) {
    uint32_t b=std::bit_cast<uint32_t>(f),s=(b>>16)&0x8000,m=b&0x7fffff;int e=int((b>>23)&255)-127+15;
    if(((b>>23)&255)==255)return uint16_t(s|(m?0x7e00:0x7c00));
    if(e>=31)return uint16_t(s|0x7c00);
    if(e<=0){if(e < -10)return uint16_t(s);m|=0x800000;unsigned shift=14-e;uint32_t q=m>>shift,rem=m&((1u<<shift)-1),mid=1u<<(shift-1);return uint16_t(s+q+(rem>mid||(rem==mid&&(q&1))));}
    uint32_t q=m>>13,rem=m&8191;q+=(rem>4096||(rem==4096&&(q&1)));return uint16_t(s+(uint32_t(e)<<10)+q);
}
inline std::string SparseRaw(const std::vector<SparseFrame>& frames) {
    if(frames.size()!=32)return {};
    std::string out="LOMV";
    auto put=[&](auto v){out.append(reinterpret_cast<const char*>(&v),sizeof(v));};
    put(uint32_t(1));put(uint32_t(32));put(uint32_t(SparseStride));
    for(const auto& f:frames) {
        put(f.frame);put(f.epoch);put(f.seconds);put(f.width);put(f.height);put(f.flags);
        for(float j:f.jitter)put(j);
        for(auto camera:{f.current,f.previous})for(unsigned i=0;i<16;++i)put(camera?float(camera->VP()[i]):0.f);
        for(auto camera:{f.current,f.previous}) {
            put(camera?float(camera->Raster().ndcYSign):1.f);
            put(camera?float(camera->Raster().halfPixelNdcX):0.f);put(camera?float(camera->Raster().halfPixelNdcY):0.f);put(0.f);
        }
        put(uint32_t(0));
        for(unsigned y=0;y<18;++y)for(unsigned x=0;x<32;++x) {
            float d=f.depth[y*32+x];uint16_t mx=0x7e00,my=0x7e00;
            double px=std::floor((x+.5)*f.width/32)+.5-f.jitter[0],py=std::floor((y+.5)*f.height/18)+.5-f.jitter[1];
            if((f.flags&1)&&f.current&&f.previous) {
                auto r=temporal::Reproject({px,py,d},*f.current,*f.previous);
                if(r){mx=Half(float(r.previous.x-px));my=Half(float(r.previous.y-py));}
            }
            put(mx);put(my);put(d);
        }
    }
    return out;
}
// Lossless byte XOR against the preceding frame, then zero/literal runs (1..128).
// Compression is bounded even for noise; no floating point quantization beyond RG16F MV.
inline std::string CompressSparse(const std::string& raw) {
    if(raw.size()!=SparseBytes)return {};
    std::string delta=raw;
    for(size_t i=16+SparseStride;i<raw.size();++i)delta[i]=raw[i]^raw[i-SparseStride];
    std::string out="LOZ1";
    for(size_t i=0;i<delta.size();) {
        bool zero=delta[i]==0;size_t n=1;while(n<128&&i+n<delta.size()&&(delta[i+n]==0)==zero)++n;
        out+=char((zero?128:0)|(n-1));if(!zero)out.append(delta,i,n);i+=n;
    }
    // Pathological alternating literals/zeros can expand; bounded raw fallback.
    if(out.size()>=raw.size()+4)return std::string("LOR1")+raw;
    return out;
}
}
