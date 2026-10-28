#include "reference.h"
#include <gpu/hdr_output.h>
#include <cstdio>
#include <cstdlib>

static unsigned checks=0;
static void Require(bool ok, const char* why) {
    ++checks;
    if (!ok) { std::fprintf(stderr,"HDR reference: %s\n",why); std::exit(1); }
}
int main() {
    using namespace hdr_reference;
    // Published ST2084 reference points, separate from production DecodePq.
    Require(std::abs(Pq(1000)-0.751827096247041)<1e-12,"PQ 1000 nits");
    Require(std::abs(Pq(100)-0.508078421517399)<1e-12,"PQ 100 nits");
    Require(Decode(0,true)==0 && Decode(1,true)==1,"black/white anchors");
    Require(Srgb(.04045)==.04045/12.92,"linear toe boundary");
    Require(std::abs(Decode(1.000001,true)-Decode(.999999,true))<5e-6,"white continuity");
    for (double v=0;v<=1;v+=1.0/65536)
        Require(Decode(v,true)==Decode(v,false),"SDR/HDR diffuse response");
    for (double v=1.01;v<65504;v*=1.03)
        Require(Decode(v,true)==std::pow(v,2.2),"unchanged extended highlights");
    for (double white : {80.,203.,400.}) for (double peak : {400.,1000.,10000.}) {
        const double ratio=std::max(white,peak)/white;
        double previous=0;
        for (double v=0;v<=16;v+=1.0/1024) {
            const double l=Decode(v,true);
            const auto expected=Shoulder({l,l*.5,l*.25},ratio);
            const auto actual=gpu::hdr::MapLinear({float(l),float(l*.5),float(l*.25)},float(ratio));
            Require(std::abs(actual[0]-expected[0])<std::max(1e-6,expected[0]*1e-6),"production shoulder vs double oracle");
            Require(actual[0]+std::max(1e-6,previous*2e-7)>=previous && actual[0]<=ratio*(1+2e-7),"monotonic bounded shoulder (FP32 rounding)");
            Require(std::abs(actual[1]*2-actual[0])<1e-6,"shoulder hue ratio");
            previous=actual[0];
        }
    }
    for (bool guest : {false,true}) {
        double oldSum=0,expectedSum=0;
        unsigned oldZero=0,newZero=0;
        for (unsigned i=1;i<=32;++i) {
            const double v=double(i)/255;
            const double s=Adjust({v,v,v},guest,false)[0];
            oldSum+=std::pow(s,2.2); expectedSum+=Srgb(s);
            const auto oldPq=std::lround(Pq(203*std::pow(s,2.2))*1023);
            const auto newPq=std::lround(Pq(203*Srgb(s))*1023);
            oldZero+=oldPq==0; newZero+=newPq==0;
        }
        std::printf("dark ramp 1..32 guest_lut=%d: old/reference luminance=%.6f, PQ10 black codes old=%u reference=%u\n",
            guest,oldSum/expectedSum,oldZero,newZero);
        Require(oldSum<expectedSum*.75,"old gamma hypothesis produces measurable shadow loss");
    }
    std::printf("level 13: old %.9f, reference %.9f, ratio %.6f\n",std::pow(13./255,2.2),Srgb(13./255),std::pow(13./255,2.2)/Srgb(13./255));
    std::printf("HDR independent CPU reference: %u checks PASS\n",checks);
}
