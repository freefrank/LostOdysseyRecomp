#pragma once
#include <array>
namespace framegen {
using Matrix = std::array<float,16>; // Row-major, row-vector convention.
struct Camera {
    Matrix viewToClip{}, clipToView{}, clipToPrevious{}, previousToClip{};
    std::array<float,3> position{}, right{}, up{}, forward{};
    float nearPlane=0, farPlane=0, fovRadians=0, aspect=0;
    float jitterX=0, jitterY=0;
    // Motion vectors are previous-minus-current, in input-resolution pixels.
    bool depthReversed=true;
};
} // namespace framegen
