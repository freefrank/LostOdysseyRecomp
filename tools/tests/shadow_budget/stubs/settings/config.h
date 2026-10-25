#pragma once
#include <cstdint>
namespace settings {
struct Config {
    uint32_t dynamicShadows = 1, depthOfFieldPercent = 100;
    bool motionBlur = true, bloom = true;
};
const Config& GetConfig();
}
