#include <gpu/render_resolution.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>

namespace {
int checks = 0;
void Require(bool value, const char* name) { ++checks; if (!value) { std::fprintf(stderr, "FAIL: %s\n", name); std::exit(1); } }
}

int main()
{
    using namespace gpu::resolution;
    const Size plan{3440, 1440};
    Require(TargetSizeForRole(TargetRole::Scene, 1280, 720, plan) == plan, "scene maps full plan");
    Require(TargetSizeForRole(TargetRole::Scene, 480, 480, plan) == plan, "scene bypasses square heuristic");
    Require(TargetSizeForRole(TargetRole::Fixed, 880, 896, plan) == Size{}, "fixed remains native");
    Require(TargetSizeForRole(TargetRole::Unknown, 1280, 736, plan) == Size{2560, 1440}, "unknown retains isotropic height scale");
    Require(TargetSizeForRole(TargetRole::Unknown, 880, 896, plan) == Size{2560, 1440}, "unknown near-square retains isotropic height scale");
    const uint32_t requestedHeight = 352, cachedHeight = 640;
    const uint32_t effectiveHeight = std::max(requestedHeight, cachedHeight);
    Require(TargetSizeForRole(TargetRole::Unknown, 640, requestedHeight, plan) == Size{2560, 1440}, "requested nonsquare extent would remap");
    Require(TargetSizeForRole(TargetRole::Unknown, 640, effectiveHeight, plan) == Size{}, "cached square extent keeps its fixed mapping");
    std::printf("target mapping: %d checks passed\n", checks);
}
