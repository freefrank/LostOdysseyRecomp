"""Execute the production color-resolve recording block with command stand-ins.

Checks pixel copies, pending clears, command counts and FG invalidation, without
claiming to run Vulkan or the game. The unchanged renderer fails the zero-barrier
reuse assertion. Use --renderer to check a baseline renderer.cpp.
"""
import argparse
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[2]
PRELUDE = r'''
#include "gpu/resolve_copy_policy.h"
#include "gpu/pending_clear.h"
#include <algorithm>
#include <cstdio>
#include <memory>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
using namespace gpu;
static unsigned checks = 0;
static void Check(bool ok, const char* why) {
    ++checks;
    if (!ok) throw std::runtime_error(why);
}
enum class RenderTextureLayout { UNKNOWN, SHADER_READ, COPY_SOURCE, COPY_DEST };
enum class RenderBarrierStage { COPY, GRAPHICS };
struct RenderBox { int32_t left, top, right, bottom, front, back; };
struct Image {
    uint32_t width, height;
    std::vector<uint32_t> pixels;
    Image(uint32_t w, uint32_t h, uint32_t value) : width(w), height(h), pixels(w*h, value) {}
};
struct HostTexture {
    uint64_t allocationSerial = 0;
    uint32_t width = 32, height = 16, format = 1;
    std::unique_ptr<Image> texture;
    pending_clear::Queue pendingClear;
    RenderTextureLayout layout = RenderTextureLayout::SHADER_READ;
    unsigned writable = 0;
    HostTexture(uint64_t serial, uint32_t value) : allocationSerial(serial), texture(std::make_unique<Image>(width,height,value)) {}
};
struct RenderTextureCopyLocation {
    static Image* Subresource(Image* p) { return p; }
};
struct Commands {
    unsigned copies = 0;
    void copyTextureRegion(Image* dst, Image* src, uint32_t x, uint32_t y, uint32_t, const RenderBox* box) {
        ++copies;
        for (int32_t sy = box->top; sy < box->bottom; ++sy)
            for (int32_t sx = box->left; sx < box->right; ++sx)
                dst->pixels[(y + sy - box->top)*dst->width + x + sx - box->left] = src->pixels[sy*src->width+sx];
    }
};
struct Fixture {
    HostTexture color{1, 7};
    struct { std::unique_ptr<HostTexture> tex = std::make_unique<HostTexture>(2,99); } rs;
    Commands commands;
    Commands* commandList = &commands;
    resolve_copy::ConsecutiveCopies consecutiveResolveCopies;
    bool resolveCopyReuse = true;
    unsigned resolveCopiesSkipped = 0, resolveCopiesRecorded = 0, barriers = 0, clears = 0;
    void MarkFgWritable(HostTexture& tex, const char*) { ++tex.writable; }
    void Transition(HostTexture& tex, RenderTextureLayout layout, RenderBarrierStage) {
        if (!tex.pendingClear.Empty()) {
            for (const auto& op : tex.pendingClear.Take()) {
                Check(op.Whole(), "fixture clear must be whole");
                std::fill(tex.texture->pixels.begin(),tex.texture->pixels.end(),uint32_t(op.color[0]));
                ++clears;
            }
            consecutiveResolveCopies.Invalidate();
        }
        if (layout == RenderTextureLayout::COPY_DEST) MarkFgWritable(tex,"writable_transition");
        if (tex.layout != layout) { ++barriers; tex.layout = layout; }
    }
    bool BlitRegion(HostTexture&, HostTexture&, uint32_t, uint32_t, uint32_t, uint32_t) {
        consecutiveResolveCopies.Invalidate();
        return false; // This test never uses a format conversion.
    }
    bool Resolve(uint32_t x0 = 0, uint32_t y0 = 0, uint32_t w = 32, uint32_t h = 16) {
        const uint32_t texW = rs.tex->width, texH = rs.tex->height;
        consecutiveResolveCopies.BeginResolve();
        /* RECORD_PRODUCTION */
        Check(rs.tex->layout == RenderTextureLayout::SHADER_READ, "resolve leaves readable destination");
        Check(std::string(resolveOperation) == "copy" || std::string(resolveOperation) == "copy_reused", "logical operation retained");
        consecutiveResolveCopies.EndResolve();
        return true;
    }
    void Clear(HostTexture& tex, uint32_t value) {
        const float c[4] = {float(value),0,0,0};
        tex.pendingClear.Record(pending_clear::ColorOp(c),tex.width,tex.height);
    }
};
'''
TESTS = r'''
int main() {
    Fixture f;
    Check(f.Resolve(),"initial resolve");
    Check(f.commands.copies == 1 && f.barriers == 3,"initial copy transitions retained");
    const auto expected = f.rs.tex->texture->pixels;
    f.barriers = 0;
    Check(f.Resolve(),"reused resolve");
    Check(f.barriers == 0,"reused resolve must not transition through transfer layouts");
    Check(f.commands.copies == 1 && f.resolveCopiesSkipped == 1,"reused resolve emits no copy");
    Check(f.rs.tex->texture->pixels == expected && f.rs.tex->writable == 2,"pixels and logical writable invalidation retained");
    f.Clear(f.color,17);
    Check(f.Resolve(),"source pending clear");
    Check(f.clears == 1 && f.commands.copies == 2 && f.rs.tex->texture->pixels[0] == 17,"source pending clear invalidates reuse before copying");
    f.Clear(*f.rs.tex,23);
    Check(f.Resolve(2,3,4,5),"destination pending clear");
    Check(f.clears == 2 && f.rs.tex->texture->pixels[0] == 23 && f.rs.tex->texture->pixels[3*32+2] == 17,"destination clear preserves outside partial resolve");
    f.barriers = 0;
    Check(f.Resolve(2,3,4,5) && f.barriers == 0,"partial resolve reuses without barriers");
    f.consecutiveResolveCopies.Invalidate();
    const auto before = f.commands.copies;
    Check(f.Resolve(2,3,4,5) && f.commands.copies == before+1,"batch or external write invalidation forces copy");
    f.resolveCopyReuse = false;
    Check(f.Resolve(2,3,4,5) && f.commands.copies == before+2,"opt-out records copies");
    f.resolveCopyReuse = true;
    f.rs.tex = std::make_unique<HostTexture>(3,42);
    Check(f.Resolve() && f.rs.tex->texture->pixels[0] == 17,"new allocation cannot reuse");

    std::mt19937 rng(0x5060);
    Fixture random;
    auto reference = random.rs.tex->texture->pixels;
    unsigned repeats = 0;
    for (unsigned i=0;i<4000;++i) {
        if (rng()%5 == 0) {
            const uint32_t value = rng()%100;
            random.Clear(random.color,value);
        }
        if (rng()%7 == 0) {
            const uint32_t value = rng()%100;
            random.Clear(*random.rs.tex,value);
            std::fill(reference.begin(),reference.end(),value);
        }
        const auto& pending = random.color.pendingClear.Ops();
        const uint32_t source = pending.empty() ? random.color.texture->pixels[0] : uint32_t(pending.back().color[0]);
        const uint32_t x = rng()%32, y = rng()%16, w = 1+rng()%(32-x), h = 1+rng()%(16-y);
        for (uint32_t sy=y;sy<y+h;++sy) for(uint32_t sx=x;sx<x+w;++sx) reference[sy*32+sx] = source;
        Check(random.Resolve(x,y,w,h),"random resolve");
        Check(random.rs.tex->texture->pixels == reference,"random partial copies match eager reference");
        random.barriers = 0;
        Check(random.Resolve(x,y,w,h),"random repeat");
        Check(random.barriers == 0 && random.rs.tex->texture->pixels == reference,"repeat preserves all pixels without a barrier");
        ++repeats;
    }
    std::printf("PASS: %u checks; %u repeated resolves eliminate %u redundant destination transitions (command stand-ins)\n",checks,repeats,2*repeats);
}
'''


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--cxx", default="g++")
    p.add_argument("--sanitize", action="store_true")
    p.add_argument("--renderer", type=Path, default=ROOT / "LostOdysseyRecomp/gpu/renderer.cpp")
    a = p.parse_args()
    source = a.renderer.read_text(encoding="utf-8")
    begin = '                const char* resolveOperation = rs.tex->format == color.format ? "copy" : "blit";'
    end = '                rs.frame = frame;'
    if source.count(begin) != 1:
        raise RuntimeError("ResolveOnGpu extraction boundary changed")
    block = source[source.index(begin):source.index(end, source.index(begin))]
    a.out.mkdir(parents=True, exist_ok=True)
    cpp = a.out / "resolve_barrier_reuse.cpp"
    exe = a.out / "resolve_barrier_reuse"
    cpp.write_text(PRELUDE.replace("/* RECORD_PRODUCTION */", block) + TESTS, encoding="utf-8")
    cmd = [a.cxx, "-std=c++20", "-O2", "-Wall", "-Wextra", "-Werror", "-I", str(ROOT / "LostOdysseyRecomp"), str(cpp), "-o", str(exe)]
    if a.sanitize:
        cmd += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
    subprocess.run(cmd, check=True)
    subprocess.run([str(exe)], check=True, env=os.environ.copy())


if __name__ == "__main__":
    main()
