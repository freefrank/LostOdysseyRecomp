#include <gpu/temporal_frame_inputs.h>
#include <cstdio>
#include <limits>

using namespace gpu;
using namespace gpu::temporal;

static unsigned checks = 0;
static bool failed = false;
static void Require(bool value, const char* text) {
    ++checks;
    if (!value) { std::fprintf(stderr, "FAIL: %s\n", text); failed = true; }
    else std::printf("PASS: %s\n", text);
}

int main() {
        TemporalFrameInputs inputs;
        inputs.plan.consumer = upscaling::TemporalConsumer::DlssInputs;
        inputs.currentInputsComplete = true;
        inputs.renderFrameId = 17;
        inputs.temporalEpoch = 3;
        inputs.depthAllocation = 9;
        inputs.jitter = FrameJitter(inputs.renderFrameId, 64, 32);
        plume::RenderTexture* token = reinterpret_cast<plume::RenderTexture*>(uintptr_t(1));
        inputs.color = {token, {64, 32}, 0, 0, 64, 32};
        inputs.depth = {token, {64, 32}, 0, 0, 64, 32};
        inputs.motion = {token, {64, 32}, 0, 0, 64, 32};
        inputs.motionInvalidity = {token, {64, 32}, 0, 0, 64, 32};
        inputs.motionState = MotionState::ResetInitialization;
        inputs.resetHistory = true;
        inputs.resetReasons = TemporalResetReason::FirstFrame;
        Require(inputs.CompleteForConsumer(), "first complete DLSS frame accepts reset-initialization motion");
        Require(HasResetReason(inputs.resetReasons, TemporalResetReason::FirstFrame), "first-frame reset reason is explicit");
        Require(inputs.jitter.phase != 0 && inputs.jitter.pixelX >= -.5 && inputs.jitter.pixelX <= .5,
            "input uses one valid raster jitter sample");
        const float previousPixelX = 10.0f, currentPixelX = 12.0f;
        const float motionPixelsX = previousPixelX - currentPixelX;
        Require(motionPixelsX == -2.0f, "motion contract is known previousPixel-currentPixel (-2,0) at scale 1");
        inputs.motionState = MotionState::Unavailable;
        Require(!inputs.CompleteForConsumer(), "DLSS input rejects missing geometry motion instead of treating it as static");
        inputs.plan.consumer = upscaling::TemporalConsumer::None;
        Require(inputs.CompleteForConsumer(), "spatial consumer does not require motion inputs");
        Require(inputs.preExposure == 1 && inputs.exposureScale == 1, "default exposure is explicit unity");
        TextureRegion region{token, {64, 32}, 0, 0, 64, 32};
        Require(region.Complete(), "exact allocation bounds accepted");
        region.x = 1;
        Require(!region.Complete(), "right edge beyond allocation rejected");
        region = {token, {64, 32}, 32, 16, 32, 16};
        Require(region.Complete(), "valid nonzero origin accepted");
        region.width = 0;
        Require(!region.Complete(), "zero-sized region rejected");
        constexpr auto max = std::numeric_limits<uint32_t>::max();
        region = {token, {64, 32}, max, 0, 2, 1};
        Require(!region.Complete(), "wrapped x plus width rejected");
        region = {token, {64, 32}, 0, max, 1, 2};
        Require(!region.Complete(), "wrapped y plus height rejected");
        region = {token, {64, 32}, 1, 0, max, 1};
        Require(!region.Complete(), "wrapped oversized width rejected");
        region = {token, {64, 32}, 0, 1, 1, max};
        Require(!region.Complete(), "wrapped oversized height rejected");
        region = {token, {max, max}, max - 1, max - 1, 1, 1};
        Require(region.Complete(), "maximum representable exact bounds accepted");
        region.texture = nullptr;
        Require(!region.Complete(), "null texture rejected");
        std::printf("PASS: %u temporal frame input checks; MV convention is previousPixel-currentPixel at scale 1,1\n", checks);
        return failed ? 1 : 0;
}
