#include <gpu/frame_generation_present.h>

#include <cstdint>
#include <cstdio>
#include <memory>

namespace {
int failures = 0;
void Check(bool value, const char* name) {
    if (!value) { std::printf("FAIL: %s\n", name); ++failures; }
}

using namespace gpu;
using namespace gpu::frame_generation;

temporal::TextureRegion Region(uintptr_t value, uint32_t width, uint32_t height) {
    return {reinterpret_cast<plume::RenderTexture*>(value), {width, height}, 0, 0, width, height};
}

TextureLease Lease(uintptr_t value, uint32_t width, uint32_t height,
                   std::shared_ptr<void> lifetime = std::make_shared<int>(1)) {
    return {Region(value, width, height), std::move(lifetime), uint64_t(value)};
}

PresentFrame ReadyFrame(std::shared_ptr<void> hudOwner = std::make_shared<int>(5)) {
    PresentFrame frame{};
    frame.temporal.plan.frameGeneration = upscaling::FrameGeneration::Dlss2x;
    frame.temporal.plan.consumer = upscaling::TemporalConsumer::None;
    frame.temporal.renderFrameId = 42;
    frame.temporal.temporalEpoch = 7;
    frame.temporal.currentInputsComplete = true;
    frame.temporal.depthConvention = temporal::DepthConvention::Reversed;
    frame.temporal.motionState = temporal::MotionState::Tracked;
    frame.temporal.color = Region(0x100, 960, 540);
    frame.temporal.depth = Region(0x200, 960, 540);
    frame.temporal.motion = Region(0x300, 960, 540);
    frame.temporal.motionInvalidity = Region(0x400, 960, 540);
    frame.sourceColor = Lease(0x100, 960, 540);
    frame.depth = Lease(0x200, 960, 540);
    frame.motion = Lease(0x300, 960, 540);
    frame.motionInvalidity = Lease(0x400, 960, 540);
    frame.hudlessColor = Lease(0x500, 1920, 1080, std::move(hudOwner));
    frame.uiColorAndAlpha = Lease(0x600, 1920, 1080);
    frame.presentationColor = Lease(0x700, 1920, 1080);
    frame.ui = UiSeparation::ColorAndAlpha;
    return frame;
}
}

int main() {
    PresentLeaseQueue queue;

    auto off = ReadyFrame();
    off.temporal.plan.frameGeneration = upscaling::FrameGeneration::Off;
    Check(!queue.PublishReady(std::move(off)), "FG-off frame cannot publish");

    auto unknownUi = ReadyFrame();
    unknownUi.ui = UiSeparation::Unavailable;
    Check(!queue.PublishReady(std::move(unknownUi)), "unknown UI boundary cannot publish");

    auto missingUi = ReadyFrame();
    missingUi.uiColorAndAlpha = {};
    Check(!queue.PublishReady(std::move(missingUi)), "separated UI mode requires an owned UI image");

    auto badDepth = ReadyFrame();
    badDepth.depth = Lease(0x201, 960, 540);
    Check(!queue.PublishReady(std::move(badDepth)), "temporal resource identity mismatch is rejected");

    auto hudOwner = std::make_shared<int>(9);
    std::weak_ptr<int> hudWeak = hudOwner;
    auto frame = ReadyFrame(hudOwner);
    auto lease = queue.PublishReady(std::move(frame));
    Check(bool(lease) && queue.LiveCount() == 1 && queue.InFlightCount() == 0,
          "ready separated frame publishes one lease");
    hudOwner.reset();
    Check(!hudWeak.expired(), "published lease pins producer resources");
    Check(!queue.MarkSubmitted(lease, 0), "zero completion serial is rejected");
    Check(queue.MarkSubmitted(lease, 7) && queue.InFlightCount() == 1,
          "provider submission arms retirement serial");
    queue.CancelUnsubmitted();
    Check(queue.LiveCount() == 1, "mode switch cannot cancel submitted lease");
    queue.RetireCompleted(6);
    Check(queue.LiveCount() == 1, "lease survives incomplete provider watermark");
    lease.reset();
    queue.RetireCompleted(7);
    Check(queue.LiveCount() == 0 && hudWeak.expired(),
          "completed provider watermark releases pinned resources");

    auto noUi = ReadyFrame();
    noUi.ui = UiSeparation::NoUi;
    noUi.uiColorAndAlpha = {};
    auto noUiLease = queue.PublishReady(std::move(noUi));
    Check(bool(noUiLease), "explicit UI-free frame is publishable");
    const auto noUiId = noUiLease ? noUiLease->id : 0;
    noUiLease.reset();
    Check(queue.DiscardUnsubmitted(noUiId) && queue.LiveCount() == 0,
          "unsubmitted lease can be discarded without a wait");

    auto submitted = queue.PublishReady(ReadyFrame());
    Check(bool(submitted) && queue.MarkSubmitted(submitted, 11), "second submitted lease");
    submitted.reset();
    queue.DrainAfterProvider();
    Check(queue.LiveCount() == 0, "explicit provider drain releases all remaining leases");

    if (!failures) std::puts("PASS: frame-generation present lease contract");
    return failures ? 1 : 0;
}
