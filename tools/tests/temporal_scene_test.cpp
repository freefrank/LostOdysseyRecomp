#include <gpu/temporal_scene.h>
#include <cstdio>
#include <cstdlib>
using namespace gpu::temporal;
static void Check(bool result, const char* message)
{
    if (!result) { std::fprintf(stderr, "FAIL: %s\n", message); std::exit(1); }
}
int main()
{
    SceneAnchor camera{};
    camera.vpBits[0] = camera.vpBits[5] = camera.vpBits[10] = camera.vpBits[15] = std::bit_cast<uint32_t>(1.f);
    camera.viewport = {0, 0, 1280, 720}; camera.depthAllocation = 42;
    const SceneResolve depth{10, 100, 0x9000, 4102, 1280, 720, true};
    const SceneResolve color{10, 110, 0xa000, 6, 1280, 720, true};
    SceneObservation s;
    auto begin = [&] { s.Reset(10); s.ObserveCamera(camera); };
    begin(); s.ObserveCamera(camera); s.ObserveDepth(99, depth);
    Check(!s.Ready() && !s.Depth().ordinal, "shadow allocation cannot supply scene depth");
    s.ObserveDepth(42, depth);
    Check(s.ObserveColor(color) && s.Draws() == 2, "ordered same-frame scene accepted");
    s.Reset(11);
    Check(!s.Ready() && !s.Draws() && !s.Depth().ordinal, "frame reset forgets previous resources");
    begin(); auto stale = depth; stale.frame = 9; s.ObserveDepth(42, stale);
    Check(!s.ObserveColor(color), "stale depth rejected");
    begin(); auto partial = depth; partial.fullExtent = false; s.ObserveDepth(42, partial);
    Check(!s.ObserveColor(color), "partial depth cannot inherit untouched pixels");
    begin(); s.ObserveDepth(42, depth); auto early = color; early.ordinal = 99;
    Check(!s.ObserveColor(early), "color from before scene depth rejected");
    begin(); s.ObserveDepth(42, depth); auto staleColor = color; staleColor.frame = 9;
    Check(!s.ObserveColor(staleColor), "old color not promoted by newer frame");
    begin(); s.ObserveDepth(42, depth); auto cropped = color; cropped.width = 640;
    Check(!s.ObserveColor(cropped), "cropped fetch cannot stand in for full scene");
    begin(); s.ObserveDepth(42, depth); Check(s.ObserveColor(color), "first copy accepted");
    Check(!s.ObserveColor(color), "multiple candidate compositions rejected");
    begin(); auto moved = camera; moved.vpBits[12] = std::bit_cast<uint32_t>(2.f); s.ObserveCamera(moved);
    s.ObserveDepth(42, depth); Check(!s.ObserveColor(color), "two cameras in same allocation rejected");
    begin(); auto resized = camera; resized.depthAllocation = 43; s.ObserveCamera(resized);
    s.ObserveDepth(43, depth); Check(!s.ObserveColor(color), "replacement allocation rejected");
    begin(); s.ObserveDepth(42, depth); s.ObserveDepth(42, depth);
    Check(!s.ObserveColor(color), "overwritten selected depth rejected");
    begin(); Check(!s.ObserveColor(color), "composition without observed depth rejected");
    s.Reset(10); auto invalid = camera; invalid.vpBits = {}; s.ObserveCamera(invalid);
    s.ObserveDepth(42, depth); Check(!s.ObserveColor(color), "singular camera rejected");
    std::puts("PASS: temporal scene ordering, allocation, ambiguity and reset cases");
}
