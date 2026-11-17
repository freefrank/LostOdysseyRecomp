#include "gpu/android_tv.h"
#include "gpu/upscaling_plan.h"
#include <cstdlib>
#include <iostream>

void require(bool condition) { if (!condition) std::abort(); }
int main() {
    using namespace gpu;
    require(android_tv::Snapshot().height == 0);
    for (int height : {720, 1080}) {
        android_tv::SetOutput(true, height);
        auto tv = android_tv::Snapshot();
        auto generation = tv.generation;
        require(tv.height == uint32_t(height));
        android_tv::SetOutput(true, height);
        require(android_tv::Snapshot().generation == generation); // repeated callbacks do no work
        auto output = upscaling::ResolveOutputRegion({uint32_t(height * 16 / 9), uint32_t(height)},
            android_tv::Aspect(tv, aspect_ratio::Mode::Ultrawide));
        require(output.width == uint32_t(height * 16 / 9) && output.height == uint32_t(height));
        require(!upscaling::Letterboxed(output));
        android_tv::SetOutput(false, height);
        require(android_tv::Snapshot().height == 0);
        require(android_tv::Aspect(android_tv::Snapshot(), aspect_ratio::Mode::Auto) == aspect_ratio::Mode::Auto);
    }
    android_tv::SetOutput(true, 2160);
    require(android_tv::Snapshot().height == 0);
    std::cout << "Android TV: atomic dock/undock, existing frame-region sizing and aspect restore PASS\n";
}
