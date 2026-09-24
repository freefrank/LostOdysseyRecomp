#ifdef NDEBUG
#undef NDEBUG // Keep the assertions live in the Release fixture build.
#endif
#include <gpu/fsr_mask_policy.h>
#include <gpu/temporal_upscaler.h>

#include <cassert>
#include <cmath>

int main() {
    using namespace gpu;
    temporal::TemporalFrameInputs in{};
    auto* color = reinterpret_cast<plume::RenderTexture*>(uintptr_t(0x100));
    auto* mask = reinterpret_cast<plume::RenderTexture*>(uintptr_t(0x200));
    in.color = {color, {64, 64}, 0, 0, 64, 64};
    in.renderFrameId = 10; in.temporalEpoch = 11; in.plan.geometryEpoch = 12;
    in.plan.deviceEpoch = 13; in.colorOrdinal = 14;
    in.fsrMask = {{mask, {68, 68}, 2, 2, 64, 64},
        {10, 11, 12, 13, 14, 15, 16, color},
        temporal::FsrMaskSemantic::ConservativeTransparentAlpha, temporal::FsrMaskCoverage::Partial};
    auto qualify = [&] { return fsr::QualifyFsrMask(in); };
    assert(qualify().useReactive && qualify().rejection == fsr::FsrMaskRejection::None);
    in.motionInvalidity = {mask, {64, 64}, 0, 0, 1, 1};
    in.materialInstability = in.motionInvalidity;
    assert(qualify().useReactive); // Neither auxiliary motion input is a reactive source.
    in.fsrMask.provenance.renderFrameId++; assert(qualify().rejection == fsr::FsrMaskRejection::Frame);
    in.fsrMask.provenance.renderFrameId--;
    in.fsrMask.provenance.temporalEpoch++; assert(qualify().rejection == fsr::FsrMaskRejection::TemporalEpoch);
    in.fsrMask.provenance.temporalEpoch--;
    in.fsrMask.provenance.geometryEpoch++; assert(qualify().rejection == fsr::FsrMaskRejection::GeometryEpoch);
    in.fsrMask.provenance.geometryEpoch--;
    in.fsrMask.provenance.deviceEpoch++; assert(qualify().rejection == fsr::FsrMaskRejection::DeviceEpoch);
    in.fsrMask.provenance.deviceEpoch--;
    in.fsrMask.provenance.capturedColor = mask; assert(qualify().rejection == fsr::FsrMaskRejection::CapturedColor);
    in.fsrMask.provenance.capturedColor = color;
    in.fsrMask.provenance.colorOrdinal = 0; assert(qualify().rejection == fsr::FsrMaskRejection::ColorOrdinal);
    in.fsrMask.provenance.colorOrdinal = in.colorOrdinal;
    in.fsrMask.provenance.sourceWriteOrdinal = 0; assert(qualify().rejection == fsr::FsrMaskRejection::SourceIdentity);
    in.fsrMask.provenance.sourceWriteOrdinal = 16;
    in.fsrMask.sceneContribution.width = 63; assert(qualify().rejection == fsr::FsrMaskRejection::Extent);
    in.fsrMask.sceneContribution.width = 64;
    in.fsrMask.sceneContribution.x = 5; assert(qualify().rejection == fsr::FsrMaskRejection::Region);
    in.fsrMask.sceneContribution.x = 2;
    in.fsrMask.coverage = temporal::FsrMaskCoverage::Unavailable;
    assert(qualify().rejection == fsr::FsrMaskRejection::Coverage);
    in.fsrMask.coverage = temporal::FsrMaskCoverage::Partial;
    in.fsrMask.semantic = temporal::FsrMaskSemantic::Unknown;
    assert(qualify().rejection == fsr::FsrMaskRejection::Semantic);
    assert(fsr::kReactiveMax == 0.9f);
}
