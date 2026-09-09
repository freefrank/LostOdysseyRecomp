#include "../../LostOdysseyRecomp/gpu/temporal_collection.h"
#include <fstream>
#include <iostream>
#include <cstdlib>
using namespace gpu::taa_collection;
void check(bool ok){if(!ok)std::abort();}
int main(){
    check(Half(1)==0x3c00&&Half(-1)==0xbc00&&Half(0)==0&&Half(65504)==0x7bff);
    check(Half(std::bit_cast<float>(0x7fc00000u))==0x7e00);
    std::vector<SparseFrame> frames;
    for(unsigned i=0;i<32;++i){SparseFrame f;f.frame=100+i;f.width=1280;f.height=720;f.flags=3;f.seconds=i/60.;
        gpu::temporal::Matrix m{};m[0]=m[5]=m[10]=m[15]=1;m[12]=i*2./1280;
        f.current=gpu::temporal::Camera::Create(m,{0,0,1280,720});m[12]-=2./1280;f.previous=gpu::temporal::Camera::Create(m,{0,0,1280,720});
        f.jitter[0]=i%2?.25f:-.25f;f.depth.fill(.5f);frames.push_back(f);
    }
    auto raw=SparseRaw(frames);check(raw.size()==SparseBytes);
    uint16_t motion;memcpy(&motion,raw.data()+16+216,2);check(motion==Half(-1));
    auto compressed=CompressSparse(raw);check(compressed.size()<raw.size());
    std::ofstream("out/v0.5.0/performance-fix/temporal-fixture.raw",std::ios::binary).write(raw.data(),raw.size());
    std::ofstream("out/v0.5.0/performance-fix/temporal-fixture.bin",std::ios::binary).write(compressed.data(),compressed.size());
    std::cout<<"camera translation/jitter-independent MV, half encoding, packet size passed; fixture "<<raw.size()<<" -> "<<compressed.size()<<" bytes\n";
}
