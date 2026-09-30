#pragma once
// Synthetic, redistributable Xenos microcode. No captured game programs/assets.
#include <gpu/shader/xenos_shader_code.h>
#include <gpu/shader/xenos_translator.h>
#include <array>
#include <cstring>
#include <vector>
namespace motion_fixture {
inline uint32_t Swap(uint32_t x) { return (x >> 24) | ((x >> 8) & 0xff00u) | ((x << 8) & 0xff0000u) | (x << 24); }
struct Program {
    std::vector<std::array<uint32_t,3>> instructions;
    uint32_t sequence = 0;
    template<class T> void Add(const T& x, bool fetch = false) {
        if (fetch) sequence |= 1u << (2 * instructions.size());
        std::array<uint32_t,3> w{}; static_assert(sizeof(T)==12); std::memcpy(w.data(), &x, 12); instructions.push_back(w);
    }
    std::vector<uint32_t> Host() const {
        std::vector<uint32_t> w(3 + 3 * instructions.size());
        xenos::ControlFlowExecInstruction cf{}; cf.address = 1; cf.count = uint32_t(instructions.size());
        cf.sequence = sequence; cf.opcode = xenos::ControlFlowOpcode::ExecEnd;
        std::memcpy(w.data(), &cf, 6); std::memcpy(w.data()+3,instructions.data(),instructions.size()*12); return w;
    }
    std::vector<uint32_t> Guest() const { auto w=Host(); for(auto& x:w)x=Swap(x); return w; }
};
inline xenos::AluInstruction Alu() {
    xenos::AluInstruction a{}; a.scalarOpcode=xenos::AluScalarOpcode::RetainPrev;
    a.vectorWriteMask=15; a.src1Select=a.src2Select=a.src3Select=1; return a;
}
inline Program Vertex(bool skin) {
    using namespace xenos; Program p;
    VertexFetchInstruction vf{}; vf.opcode=FetchOpcode::VertexFetch; vf.dstRegister=1; vf.mustBeOne=1;
    vf.dstSwizzle=0x688; vf.format=uint32_t(VertexFormat::k_32_32_32_32_FLOAT); vf.stride=4; p.Add(vf,true);
    if (skin) {
        auto address=Alu(); address.vectorWriteMask=0; address.scalarOpcode=AluScalarOpcode::MaxAs;
        address.src3Swizzle=0x40; // SCALAR_0 selects r0.x = vertex ID (0,1,2)
        p.Add(address);
        auto weighted=Alu(); weighted.vectorDest=2; weighted.vectorOpcode=AluVectorOpcode::Mul;
        weighted.src1Select=weighted.src2Select=0; weighted.src1Register=8; weighted.src2Register=16;
        weighted.const0Relative=1; weighted.constAddressRegisterRelative=1; p.Add(weighted);
        weighted.vectorOpcode=AluVectorOpcode::Mad; weighted.src1Register=20; weighted.src2Register=17;
        weighted.src3Register=2; p.Add(weighted);
    }
    auto output=Alu(); output.exportData=1; output.vectorDest=uint32_t(ExportRegister::VSPosition);
    output.vectorOpcode=AluVectorOpcode::Add; output.src1Register=1;
    output.src2Select=skin?1:0; output.src2Register=skin?2:4; p.Add(output);
    return p;
}
inline Program Pixel() {
    using namespace xenos; Program p; auto a=Alu(); a.vectorOpcode=AluVectorOpcode::Max;
    a.exportData=1; a.vectorDest=0; a.src1Select=a.src2Select=0; p.Add(a); return p;
}
struct alignas(16) Shared {
    uint32_t bools[8]{}, loops[32]{};
    float ndcScale[4]{1,1,-1,0}, ndcOffset[4]{0,0,1,0}, halfPixel[2]{};
    uint32_t vtxFmt=4,flags=0;
    float alpha[4]{},colorMax[4]{1,1,1,1}; uint32_t rest[196]{};
};
static_assert(sizeof(Shared)==1024);
}
