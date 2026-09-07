#include <gpu/render_resolution.h>
#include <gpu/temporal_scene.h>
#include <cstdio>
#include <cstdlib>
#include <cmath>

namespace {
int checks = 0;
void Require(bool ok, const char* message) {
    ++checks;
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
}
int main() {
    using namespace gpu::resolution;
    Require(ResolveInternalSize(0, 1920, 1200) == Size{1920, 1080}, "Auto fits letterboxed output");
    Require(ResolveInternalSize(0, 3440, 1440) == Size{2560, 1440}, "Auto fits ultrawide output");
    Require(ResolveInternalSize(0, 7680, 4320) == Size{3840, 2160}, "Auto capped at 4K");
    Require(ResolveInternalSize(0, 0, 0) == Size{}, "uninitialized output stays native");
    Require(ResolveInternalSize(0, 1366, 768) == Size{1360, 765}, "Auto retains exact 16:9 raster ratio");
    Require(ResolveInternalSize(0, 1080, 1920) == Size{1072, 603}, "portrait excludes output bars");
    Require(ResolveInternalSize(2160, 1280, 720) == Size{3840, 2160}, "manual internal size independent of output");
    Require(Scale(428, 1080) == 642 && Scale(448, 1080) == 672, "logical fetch view excludes scaled storage padding");
    Require(Scale(736, 2160) == 2208 && Scale(720, 2160) == 2160, "EDRAM allocation padding is separate from frontbuffer content");
    Require(TargetHeight(1024, 1024, 2160) == 720, "square shadow target retains original texel resolution");
    Require(TargetHeight(1280, 736, 2160) == 2160, "scene target uses full requested raster resolution");
    Require(TargetHeight(8192, 4096, 2160) == 720, "unrelated oversized target does not exceed device dimensions");
    for (uint32_t h : {720u, 1080u, 1440u, 2160u, 765u}) {
        const Size size = h == 765 ? ResolveInternalSize(0, 1366, 768) : ResolveInternalSize(h, 1, 1);
        Require(Scale(1280, h) == size.width && Scale(720, h) == size.height, "guest frontbuffer maps to requested content");
        // A strip partition must cover the full extent without holes or overlap,
        // even when neither the guest origin nor its width maps to integral pixels.
        uint32_t covered = 0;
        for (uint32_t x = 0; x < 1280; x += 7) covered += Scale(std::min(x + 7, 1280u), h) - Scale(x, h);
        Require(covered == size.width, "fractional-scale adjacent resolve strips partition exactly");

        // Known identity-camera point NDC=(.25,-.5) maps analytically to
        // guest=(800.5,540.5). Keeping guest half-pixel NDC while changing the
        // viewport must reconstruct that same world point at every scale.
        using namespace gpu::temporal;
        const Matrix identity{1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        const Viewport viewport{0,0,double(size.width),double(size.height),1,1./1280,-1./720};
        const auto camera = Camera::Create(identity, viewport);
        Require(camera.has_value(), "physical temporal camera is valid");
        const double scale = double(h) / 720;
        const auto reprojection = Reproject({800.5 * scale, 540.5 * scale, .5}, *camera, *camera);
        Require(bool(reprojection) && std::abs(reprojection.world[0] - .25) < 1e-9 &&
            std::abs(reprojection.world[1] + .5) < 1e-9, "scaled half-pixel reconstructs independent known point");

        SceneAnchor anchor;
        for (unsigned i = 0; i < 16; ++i) anchor.vpBits[i] = std::bit_cast<uint32_t>(float(identity[i]));
        anchor.viewport = viewport; anchor.depthAllocation = 4;
        SceneObservation scene; scene.Reset(17); scene.ObserveCamera(anchor);
        scene.ObserveDepth(4, {17, 1, 0x1000, 0, size.width, size.height, true});
        scene.ObserveColor({17, 2, 0x2000, 6, size.width, size.height, true});
        Require(scene.Ready(), "physical depth and color agree with temporal viewport");
    }
    std::printf("render resolution: %d checks passed\n", checks);
}
