#include "../../LostOdysseyRecomp/gpu/temporal_evidence.h"
#include "../../LostOdysseyRecomp/gpu/taa_collection_format.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

using gpu::position_evidence::Analyze;
int checks=0;
void Check(bool ok,const char* why) {++checks;if(!ok)throw std::runtime_error(why);}
std::string Wrap(std::string body) {
    return "void main() {\nfloat4 oPointSize = 0.0;\nxeVfetchBase = uint(int(floor(r0.x)) * 8) * 4u;\n"
        "r4.xyzw = XeVF_32_32_32_32_FLOAT(vfetch95, XeVfetchOffset(95u) + xeVfetchBase + 0u, true, false).xyzw;\n"+
        body+"\nif ((xeFlags & 8u) == 0u) { oPos.xy += xeHalfPixelOffset * oPos.w; }\n}";
}
std::string Matrix(int slot=7) {
    return "xePV.xyzw = r4.wwww * XeConst("+std::to_string(slot+3)+").xyzw;\nr0.xyzw = xePV.xyzw;\n"
        "xePV.xyzw = r4.zzzz * XeConst("+std::to_string(slot+2)+").xyzw + r0.xyzw;\nr0.xyzw = xePV.xyzw;\n"
        "xePV.xyzw = r4.yyyy * XeConst("+std::to_string(slot+1)+").xyzw + r0.xyzw;\nr0.xyzw = xePV.xyzw;\n"
        "xePV.xyzw = r4.xxxx * XeConst("+std::to_string(slot)+").xyzw + r0.xyzw;\noPos.xyzw = xePV.xyzw;\n";
}
int main(int argc,char** argv) {
    if(argc==3 && std::string_view(argv[1])=="--fixture") {
        const auto p=Analyze(Wrap(Matrix()));
        std::ofstream out(argv[2]);
        out<<gpu::taa_collection::RequestStart("vulkan","Position Evidence Fixture","0")<<
            gpu::taa_collection::RecordJson(0xe8ec18f1d3eac4df,0x9f93020766683e78,3840,2160,-1,20,27,2,44,p,31)<<"]}";
        return out.good()?0:1;
    }
    if(argc>1) {
        for(int i=1;i<argc;++i) {
            std::ifstream f(argv[i],std::ios::binary);if(!f)throw std::runtime_error("missing input");
            const auto s=Analyze(std::string(std::istreambuf_iterator<char>(f),{}));
            std::cout<<argv[i]<<" kind="<<s.kind<<" slot="<<s.slot<<" issues="<<s.issues<<" outputs="<<s.outputs<<'\n';
        }
        return 0;
    }
    for(int slot:{0,4,7,8,230,233,252}) {
        const auto s=Analyze(Wrap(Matrix(slot)));Check(s.kind==1 && s.slot==slot && !s.issues,"matrix family");
    }
    Check(Analyze(Wrap("oPos.xyzw = max(r4.xyzw, r4.xyzw);\n")).kind==2,"direct geometry");
    auto s=Analyze(Wrap(Matrix(0)+"r2.xyzw = XeConst(7).xyzw;\n"));
    Check(s.kind==1 && s.slot==0,"unused camera c7 must not select c7");
    s=Analyze(Wrap(Matrix()+"o0.xyzw = xePV.xyzw;\n"));Check(s.kind==1 && s.outputs==1,"shared projected interpolator");
    s=Analyze(Wrap(Matrix()+"o0.xy = r4.xy;\n"));Check(s.outputs==0,"zero-initialized unwritten interpolator components");
    s=Analyze(Wrap(Matrix()+"r2.xyzw = XeConst(8).xyzw;\no2.xyzw = r2.xyzw;\n"));Check(s.outputs==4,"other matrix output use");
    s=Analyze(Wrap("if (p0) {\n"+Matrix()+"}\n"));Check(s.kind==0 && (s.issues&2),"branch unknown");
    s=Analyze(Wrap("r2.xyzw = XeConst(7+a0).xyzw;\n"+Matrix()));Check(s.kind==0 && (s.issues&1),"dynamic constant unknown");
    s=Analyze(Wrap(Matrix()+"oPos.x = r4.x;\n"));Check(s.kind==0,"partial export unknown");
    s=Analyze(Wrap(Matrix()+"oPos += r4;\n"));Check(s.kind==0,"compound export unknown");
    s=Analyze(Wrap("r4.xyzw = XeConst(7).xyzw;\n"+Matrix()));Check(s.kind==0 && (s.issues&16),"matrix-dependent scalar unknown");
    std::string bad=Matrix();bad.replace(bad.find("XeConst(9).xyzw"),14,"XeConst(9).yxzw");
    Check(Analyze(Wrap(bad)).kind==0,"incorrect matrix swizzle");
    Check(Analyze(Wrap("oPos.xyzw = rcp(r4.xyzw);\n")).kind==0,"nonlinear output");
    Check(Analyze(Wrap("oPos.xyzw = somethingUnknown(r4);\n")).kind==0,"unsupported output");
    Check(Analyze("other translator dialect").kind==0,"dialect boundary");
    std::array<uint32_t,1024> constants{};gpu::temporal::SceneAnchor anchor;
    for(unsigned i=0;i<16;++i)anchor.vpBits[i]=constants[7*4+i]=std::bit_cast<uint32_t>(float(i+1));
    anchor.viewport={0,0,3840,2160};anchor.depthAllocation=42;
    const auto proof=Analyze(Wrap(Matrix()));
    using gpu::temporal::PositionGuards;
    Check(PositionGuards(proof,true,&anchor,42,anchor.viewport,constants.data())==31,"independent guards all match");
    Check(!(PositionGuards(proof,true,&anchor,99,anchor.viewport,constants.data())&8),"depth mismatch");
    auto vp=anchor.viewport;vp.x=1;
    Check(!(PositionGuards(proof,true,&anchor,42,vp,constants.data())&4),"full viewport differs despite compatible flag");
    constants[28]^=1;
    Check(!(PositionGuards(proof,true,&anchor,42,anchor.viewport,constants.data())&2),"bitwise camera mismatch");
    constants[28]=0x7fc00000;
    Check(!(PositionGuards(proof,true,&anchor,42,anchor.viewport,constants.data())&16),"nonfinite matrix");
    const auto noProof=Analyze(Wrap("oPos.xyzw = r4.xyzw;\n"));
    Check(!(PositionGuards(noProof,true,&anchor,42,anchor.viewport,constants.data())&2),"residual candidate not position proof");
    std::cout<<checks<<" position evidence and independent-guard checks passed\n";
}
