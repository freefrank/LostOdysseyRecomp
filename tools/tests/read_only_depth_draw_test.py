"""Compile production native-draw emission with the real query tracker.

No game execution or hardware timing is claimed. --renderer accepts upstream's
renderer.cpp to demonstrate the failing-before no-op draw regression.
"""
import argparse
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PRELUDE = r'''
#include "gpu/occlusion_queries.h"
#include <cstdio>
#include <stdexcept>
using namespace gpu;
constexpr uint32_t REG_RB_DEPTHCONTROL = 0x2200;
static unsigned checks=0;
static void Check(bool ok, const char* why) {
    ++checks;
    if(!ok) throw std::runtime_error(why);
}
struct Fixture {
    occlusion::Tracker occlusion{occlusion::Mode::Strict};
    struct { uint32_t mode=0, modeMask=0; } drops;
    uint32_t modeControl=5, depthControl=0, recorded=0, readOnlyDepthDrawsSkipped=0;
    uint32_t edramHandoffs=0, pendingClearChecks=0, afterDrawBookkeeping=0;
    bool skipReadOnlyDepthDraws=true, useIndices=false;
    uint32_t indexCount=6;
    int32_t baseVertex=2;
    int preparedIndexBuffer=1;
    uint64_t preparedIndexOffset=0;
    std::vector<uint32_t> indices=std::vector<uint32_t>(6);
    enum class RenderFormat { R32_UINT };
    struct RenderBufferReference { RenderBufferReference(int,uint64_t) {} };
    struct RenderIndexBufferView { RenderIndexBufferView(RenderBufferReference,uint32_t,RenderFormat) {} };
    struct Commands {
        uint32_t& recorded;
        void setIndexBuffer(const RenderIndexBufferView*) {}
        void drawIndexedInstanced(uint32_t n,uint32_t instances,uint32_t first,int32_t base,uint32_t start) {
            Check(n==6 && instances==1 && first==0 && base==2 && start==0,"indexed command arguments preserved");
            ++recorded;
        }
        void drawInstanced(uint32_t n,uint32_t instances,uint32_t base,uint32_t start) {
            Check(n==6 && instances==1 && base==2 && start==0,"non-indexed command arguments preserved");
            ++recorded;
        }
    } commands{recorded};
    Commands* commandList=&commands;
    uint32_t Reg(uint32_t reg) {
        Check(reg==REG_RB_DEPTHCONTROL,"only depth/stencil state is read by this gate");
        return depthControl;
    }
    void Draw() {
        if(modeControl==4 || modeControl==5) occlusion.DrawSeen();
        /* PRODUCTION_MODE_GATE */
        // Model the earlier renderer preparation and later bookkeeping. An
        // early return in the production mode gate must fail these invariants.
        ++edramHandoffs; ++pendingClearChecks;
        const uint32_t before=recorded;
        /* PRODUCTION_DRAW_EMISSION */
        ++afterDrawBookkeeping;
        if(occlusion.Active() && recorded!=before) occlusion.DrawMeasured({1,0,1.0});
    }
};
'''
TESTS = r'''
int main() {
    Fixture first;
    first.Draw();
    Check(first.recorded==0 && first.readOnlyDepthDrawsSkipped==1,"depth-only draw with no writes must be omitted");
    Check(first.edramHandoffs==1 && first.pendingClearChecks==1 && first.afterDrawBookkeeping==1,
        "omitting the native draw must preserve EDRAM preparation and later bookkeeping");
    first.depthControl=2; // Depth testing alone reads the attachment.
    first.Draw();
    Check(first.recorded==0 && first.readOnlyDepthDrawsSkipped==2,"read-only depth test is not an attachment write");
    for(bool indexed : {false,true}) for(bool active : {false,true}) {
        // Do not accumulate measured parts during the exhaustive state sweep.
        for(uint32_t mode=0;mode<8;++mode) {
            Fixture f;
            f.modeControl=mode;
            f.useIndices=indexed;
            for(uint32_t depth=0;depth<65536;++depth) {
                if(active) f.occlusion.Begin(0x1020);
                f.depthControl=depth;
                const uint32_t before=f.recorded;
                const uint32_t handoffs=f.edramHandoffs, after=f.afterDrawBookkeeping;
                f.Draw();
                const bool valid=mode==4 || mode==5;
                const bool effect=mode==4 || (depth&1) || (depth&4) || active;
                Check(f.recorded-before==unsigned(valid&&effect),"all color/depth/stencil/query effects must be retained");
                Check(f.edramHandoffs-handoffs==unsigned(valid) && f.afterDrawBookkeeping-after==unsigned(valid),
                    "preparation and bookkeeping are independent of native draw elision");
            }
        }
    }
    for(const auto mode : {occlusion::Mode::Fast,occlusion::Mode::Strict,occlusion::Mode::History}) {
        Fixture f;
        f.occlusion.SetMode(mode);
        const auto begin=f.occlusion.Begin(0x1020,occlusion::HistoryOwner(0x823cd744,1));
        f.Draw();
        Check(f.recorded==1 && f.readOnlyDepthDrawsSkipped==0,"read-only query geometry is recorded in every policy");
        const auto early=f.occlusion.End(0x1000);
        const std::vector<uint64_t> samples{7};
        const auto completed=f.occlusion.Complete(1,samples);
        if(mode!=occlusion::Mode::Fast) {
            Check(!early && completed.size()==1,"strict/history query waits for the measured draw");
            const auto count=completed[0].record.zpassA+completed[0].record.zpassB-begin.record.zpassA-begin.record.zpassB;
            Check(count==7,"query sample count is unchanged");
        }
    }
    Fixture disabled;
    disabled.skipReadOnlyDepthDraws=false;
    disabled.Draw();
    Check(disabled.recorded==1 && disabled.readOnlyDepthDrawsSkipped==0,"opt-out retains the original draw");
    Fixture color;
    color.modeControl=4;
    color.Draw();
    Check(color.recorded==1,"color draws are never inferred to be invisible");
    Fixture stencil;
    stencil.depthControl=1; // Even all KEEP stencil state takes the conservative path.
    stencil.Draw();
    Check(stencil.recorded==1,"all stencil-enabled draws remain untouched");
    std::printf("PASS: %u checks; native indexed/non-indexed commands, bookkeeping, real queries, opt-out\n",checks);
}
'''


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--cxx", default="g++")
    p.add_argument("--sanitize", action="store_true")
    p.add_argument("--compile-only", action="store_true", help="Link a cross-compiled fixture without executing it")
    p.add_argument("--renderer", type=Path, default=ROOT / "LostOdysseyRecomp/gpu/renderer.cpp")
    a = p.parse_args()
    source = a.renderer.read_text(encoding="utf-8")
    begin = "                if (modeControl != 4 && modeControl != 5)"
    end = "                RefreshAnisotropicFiltering();"
    if source.count(begin) != 1 or source.count(end) != 1:
        raise RuntimeError("DrawImpl extraction boundaries changed")
    gate = source[source.index(begin):source.index(end)]
    draw_begin = "                recordProbe.Enter(record_timing::Phase::Draw);"
    draw_end = "                recordProbe.Enter(record_timing::Phase::Occlusion);"
    if source.count(draw_begin) != 1:
        raise RuntimeError("Native draw extraction boundary changed")
    emission_start = source.index(draw_begin) + len(draw_begin)
    emission = source[emission_start:source.index(draw_end, emission_start)]
    if "skipReadOnlyDepthDraws" in emission:
        # The actual production code must retain every surrounding operation.
        for required in ("color = AcquireColorTarget(", "RenderFramebuffer* framebuffer =", "SetConstantBuffer("):
            if required not in source[:emission_start]:
                raise RuntimeError("Native draw elision moved ahead of renderer preparation")
        if "return;" in emission:
            raise RuntimeError("Native draw elision must not return before bookkeeping")
    a.out.mkdir(parents=True, exist_ok=True)
    cpp = a.out / "read_only_depth_draw.cpp"
    exe = a.out / "read_only_depth_draw"
    cpp.write_text(PRELUDE.replace("/* PRODUCTION_MODE_GATE */", gate)
                   .replace("/* PRODUCTION_DRAW_EMISSION */", emission) + TESTS, encoding="utf-8")
    cmd = [a.cxx, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-Wno-missing-field-initializers", "-I", str(ROOT / "LostOdysseyRecomp"), str(cpp), "-o", str(exe)]
    if a.sanitize:
        cmd += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(cmd, check=True)
    if not a.compile_only:
        subprocess.run([str(exe)], check=True)


if __name__ == "__main__":
    main()
