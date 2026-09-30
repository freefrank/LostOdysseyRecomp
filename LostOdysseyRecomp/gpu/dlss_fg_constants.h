#pragma once
#include "frame_generation_camera.h"
#include <sl_consts.h>

namespace gpu::dlss_fg {
using frame_generation::DepthRemap;
namespace detail = frame_generation::detail;
inline bool BuildConstants(const temporal::TemporalFrameInputs& inputs,
    const temporal::Matrix* previousVP, sl::Constants& result, DepthRemap* remap = nullptr,
    const temporal::Viewport* previousRaster = nullptr) {
    framegen::Camera camera;
    if (!frame_generation::BuildCamera(inputs,previousVP,camera,remap,previousRaster)) return false;
    const auto matrix=[](const framegen::Matrix& from,sl::float4x4& to) {
        for (unsigned r=0;r<4;++r) to.setRow(r,{from[4*r],from[4*r+1],from[4*r+2],from[4*r+3]});
    };
    matrix(camera.viewToClip,result.cameraViewToClip); matrix(camera.clipToView,result.clipToCameraView);
    matrix(camera.clipToPrevious,result.clipToPrevClip); matrix(camera.previousToClip,result.prevClipToClip);
    result.cameraPos={camera.position[0],camera.position[1],camera.position[2]};
    result.cameraRight={camera.right[0],camera.right[1],camera.right[2]};
    result.cameraUp={camera.up[0],camera.up[1],camera.up[2]};
    result.cameraFwd={camera.forward[0],camera.forward[1],camera.forward[2]};
    result.cameraNear=camera.nearPlane; result.cameraFar=camera.farPlane;
    result.cameraFOV=camera.fovRadians; result.cameraAspectRatio=camera.aspect;
    result.jitterOffset={camera.jitterX,camera.jitterY};
    result.mvecScale={1.0f/inputs.motion.width,1.0f/inputs.motion.height};
    result.cameraPinholeOffset={0,0};
    result.depthInverted=sl::eTrue; result.cameraMotionIncluded=sl::eTrue;
    result.motionVectors3D=sl::eFalse; result.motionVectorsJittered=sl::eFalse;
    result.motionVectorsDilated=sl::eFalse; result.orthographicProjection=sl::eFalse;
    result.reset=previousVP ? sl::eFalse : sl::eTrue;
    return true;
}
} // namespace gpu::dlss_fg
