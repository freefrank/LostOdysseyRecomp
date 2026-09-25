#!/usr/bin/env python3
"""Temporary transport: apply exact source edits to the isolated review branch.
Removed after validation. Never modifies saves, assets, submodule contents or main.
"""
from pathlib import Path
import hashlib


def edit(path, sha, changes):
    p = Path(path)
    raw = p.read_bytes()
    actual = hashlib.sha1(b'blob ' + str(len(raw)).encode() + b'\0' + raw).hexdigest()
    if actual != sha:
        raise RuntimeError(f'{path}: expected {sha}, found {actual}')
    text = raw.decode('utf-8')
    for old, new in changes:
        if text.count(old) != 1:
            raise RuntimeError(f'{path}: context count {text.count(old)} for {old[:90]!r}')
        text = text.replace(old, new, 1)
    p.write_bytes(text.encode('utf-8'))

edit('LostOdysseyRecomp/gpu/temporal_frame_inputs.h', '5035eacd0373c08e3433dd791472cec38890d3ca', [
    ('ResetInitialization = 1, Tracked = 2 };', 'ResetInitialization = 1, Tracked = 2, Hybrid = 3 };'),
    ('KnownDepthConvention(depthConvention) && motionState != MotionState::Unavailable &&',
     'KnownDepthConvention(depthConvention) && motionState != MotionState::Unavailable &&\n             uint32_t(motionState) <= uint32_t(MotionState::Hybrid) &&\n             (motionState != MotionState::Hybrid || plan.frameGeneration == upscaling::FrameGeneration::Off) &&'),
])

edit('LostOdysseyRecomp/gpu/temporal_history.h', '525b5803bab309f147885e3ac3981cec42b17bae', [
    ('#include "motion_frame.h"', '#include "motion_frame.h"\n#include "sr_hybrid_motion_gpu.h"'),
    ('    MotionFrameView motionView_{};', '    MotionFrameView motionView_{};\n    SrHybridMotionGPU hybridMotion_;\n    bool hybridAttempted_ = false;'),
    ('    void RecordExternalRead() { aa_.RecordExternalUse(); }',
     '    void RecordExternalRead() { aa_.RecordExternalUse(); hybridMotion_.RecordConsumerUse(aa_.RecordedSerial()); }\n    bool HybridResourceFailed() const { return hybridAttempted_ && !motionVectorValid_ && hybridMotion_.ResourceFailed(); }'),
    ('const MotionFrameView* motion = nullptr, TemporalResetReason reset = TemporalResetReason::None) {',
     'const MotionFrameView* motion = nullptr, TemporalResetReason reset = TemporalResetReason::None, bool allowHybrid = false) {'),
    ('        captureFailure_=InputCaptureFailure::None;\n        if(frameResourceFailure_',
     '        captureFailure_=InputCaptureFailure::None; hybridAttempted_=false;\n        if(frameResourceFailure_'),
    ('            if(!ContinuousHistoryCamera(*current.camera,*previous.camera))',
     '            if(!previous.camera || !ContinuousHistoryCamera(*current.camera,*previous.camera))'),
    ('        if(upscaling::RequiresMotionDepth(plan.consumer,plan.frameGeneration)&&!motionVectorValid_)', '''        // Only production SR opts in. Probe/legacy TAA and future FG keep their
        // existing contracts. A stale ready geometry view cannot be papered over.
        if (allowHybrid && upscaling::MatchesSrProvider(plan.requestedUpscaler,plan.consumer) &&
            plan.frameGeneration == upscaling::FrameGeneration::Off &&
            !(motion && motion->ready && !motionVectorValid_)) {
            hybridAttempted_ = true;
            const bool hybridReset = (reset|automatic) != TemporalResetReason::None ||
                !previous.camera || (motion && motion->ready && motion->state == MotionState::ResetInitialization);
            const auto composed = hybridMotion_.Render(device_,commands,depth_[frame_%2].texture.get(),
                *current.camera,previous.camera ? &*previous.camera : nullptr,
                motionVectorValid_ ? &motionView_ : nullptr,frame_,epoch_,current.allocation,width_,height_,
                jitter.pixelX,jitter.pixelY,hybridReset,aa_.RecordedSerial()+1);
            if (composed.ready) { motionView_=composed; motionVectorValid_=true; }
            // An optional composer failure may retain already-valid geometric
            // motion; it may never turn missing resources into complete inputs.
        }
        if(upscaling::RequiresMotionDepth(plan.consumer,plan.frameGeneration)&&!motionVectorValid_)'''),
    ('        aa_.RecordExternalUse(); return true;',
     '        aa_.RecordExternalUse(); hybridMotion_.RecordConsumerUse(aa_.RecordedSerial()); return true;'),
    ('aa_.ReleaseCompleted();retired_.clear();',
     'hybridMotion_.ReleaseCompletedThrough(aa_.RecordedSerial());aa_.ReleaseCompleted();retired_.clear();'),
    ('        aa_.ReleaseCompletedThrough(serial);',
     '        aa_.ReleaseCompletedThrough(serial);\n        hybridMotion_.ReleaseCompletedThrough(serial);'),
])

edit('LostOdysseyRecomp/gpu/renderer.cpp', '458d68d98dea8cfc0ed506548256a2821aa2ed0d', [
    ('        if (recoverablePending) {',
     '        if (recoverablePending && !(inputs.motionState == temporal::MotionState::Hybrid && inputs.CompleteForConsumer())) {'),
    ('                                        qualifiedEncoding, &motionView)) {',
     '''                                        qualifiedEncoding, &motionView,
                                        srTimeReset ? temporal::TemporalResetReason::FrameDiscontinuity : temporal::TemporalResetReason::None,
                                        dlssSrRequested && motionOptions.enabled && motionOptions.replay && motionOptions.consume &&
                                        temporal::SrHybridMotionEnabled() && !motionInitFailed &&
                                        !(motionReplay && motionReplay->ResourceFailedThisFrame()))) {'''),
    ('                                        const bool resourceFailed = motionInitFailed ||\n                                            (motionReplay && motionReplay->ResourceFailedThisFrame());',
     '                                        const bool resourceFailed = motionInitFailed || temporalHistory->HybridResourceFailed() ||\n                                            (motionReplay && motionReplay->ResourceFailedThisFrame());'),
    ('                                    const auto selected = SelectDlssSceneCopyInputs(inputs);',
     '''                                    if (motionOptions.log && inputs.motionState == temporal::MotionState::Hybrid && frame % 120 == 0)
                                        LOG_INFO("SR hybrid MV: frame={} geometry_view_ready={} reset={} input={}x{} confidence_mask=1",
                                            frame, motionView.ready, inputs.resetHistory, inputs.plan.width, inputs.plan.height);
                                    const auto selected = SelectDlssSceneCopyInputs(inputs);'''),
])

edit('LostOdysseyRecomp/gpu/dlss_ngx.cpp', '7c1d307e403db2bfbbfa84b05b645dda92706903', [
    ('#include "dlss_evaluate_capture.h"', '#include "dlss_evaluate_capture.h"\n#include "sr_hybrid_mask.h"'),
    ('#include <algorithm>', '#include <algorithm>\n#include <cstdio>'),
    ('            if (valid) { mode.optimal = {optimalWidth, optimalHeight}; mode.minimum = {minWidth, minHeight}; mode.maximum = {maxWidth, maxHeight}; }',
     '''            // Retain raw extents even on error, for diagnostics only. Consumers
            // must still require Ready; never invent a DLAA 1:1 vendor result.
            mode.optimal = {optimalWidth, optimalHeight}; mode.minimum = {minWidth, minHeight}; mode.maximum = {maxWidth, maxHeight};
            std::fprintf(stderr, "DLSS sizing: mode=%u output=%ux%u optimal=%ux%u minimum=%ux%u maximum=%ux%u raw_ngx=0x%08x state=%u\\n",
                unsigned(quality), key.outputWidth, key.outputHeight, optimalWidth, optimalHeight,
                minWidth, minHeight, maxWidth, maxHeight, unsigned(optimalResult), unsigned(mode.state));'''),
    ('        !inputs.CompleteForConsumer() || !temporal::MatchesDepthConvention(inputs.depthConvention, config.depthInverted) ||',
     '        !inputs.CompleteForConsumer() || !temporal::ValidSrHybridMask(inputs,sessionDevice_) ||\n        !temporal::MatchesDepthConvention(inputs.depthConvention, config.depthInverted) ||'),
    ('    auto outputResource = ImageResource(output, true);',
     '''    auto outputResource = ImageResource(output, true);
    const bool hybridMotion = inputs.motionState == temporal::MotionState::Hybrid;
    NVSDK_NGX_Resource_VK hybridBiasResource{};
    if (hybridMotion) hybridBiasResource = ImageResource(
        *static_cast<const plume::VulkanTexture*>(inputs.motionInvalidity.texture),false);'''),
    ('    evaluate.pInMotionVectors = &motionResource;',
     '''    evaluate.pInMotionVectors = &motionResource;
    // Confidence biases toward current color. It is neither material alpha nor
    // an SDK guarantee that temporal history will be completely rejected.
    evaluate.pInBiasCurrentColorMask = hybridMotion ? &hybridBiasResource : nullptr;
    evaluate.InBiasCurrentColorSubrectBase = {0,0};'''),
    ('    // Transparency and exposure resources remain null. Auto exposure is only',
     '    // Transparency and exposure resources remain null; hybrid confidence is\n    // independently bound above. Auto exposure is only'),
])

edit('LostOdysseyRecomp/gpu/fsr_upscaler.cpp', '1940fa20a6722fe2a6cce46235d2d3f7431a3c92', [
    ('#include "fsr_mask_policy.h"', '#include "fsr_mask_policy.h"\n#include "sr_hybrid_mask.h"'),
    ('    auto maskDecision = QualifyNativeMask(inputs);',
     '''    if (!temporal::ValidSrHybridMask(inputs,impl_->device)) {
        logGuardRejection("hybrid_confidence_mask"); return attempt;
    }
    const auto* hybridConfidence = inputs.motionState == temporal::MotionState::Hybrid ?
        static_cast<const plume::VulkanTexture*>(inputs.motionInvalidity.texture) : nullptr;
    auto maskDecision = QualifyNativeMask(inputs);'''),
    ('    dispatch.transparencyAndComposition = {};',
     '''    // Preserve the audited material reactive mask. Hybrid motion confidence
    // separately biases accumulation through the composition input, not alpha.
    dispatch.transparencyAndComposition = hybridConfidence ?
        MakeResource(*hybridConfidence,FFX_RESOURCE_STATE_COMPUTE_READ) : FfxResource{};'''),
])

edit('LostOdysseyRecomp/gpu/upscaling_plan.h', '111a1d459214513e0aeda384eaefa66c84ec1f36', [
    ('class SizingCache {\npublic:', '''class SizingCache {
public:
    using Clock = uint64_t(*)(); // monotonic milliseconds; injectable in CPU tests
    explicit SizingCache(Clock clock = nullptr) : clock_(clock ? clock : &SteadyMilliseconds) {}'''),
    ('    std::array<std::optional<OutputSizing>, 8> entries_{};', '''    std::array<std::optional<OutputSizing>, 8> entries_{};
    struct RetryState { unsigned failures = 0; uint64_t notBefore = 0; };
    std::array<RetryState,8> retries_{};
    Clock clock_;
    static uint64_t SteadyMilliseconds();'''),
])

p = Path('LostOdysseyRecomp/gpu/upscaling_plan.cpp')
raw = p.read_bytes()
assert hashlib.sha1(b'blob '+str(len(raw)).encode()+b'\0'+raw).hexdigest() == '0ff7ed4b47a758531ad6698ead829b50164651b2'
s = raw.decode()
s = s.replace('#include "upscaling_plan.h"', '#include "upscaling_plan.h"\n#include <algorithm>\n#include <chrono>\n#include <limits>', 1)
s = s.replace('OutputSizing SizingCache::LookupOrRequestSizing', '''uint64_t SizingCache::SteadyMilliseconds() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

OutputSizing SizingCache::LookupOrRequestSizing''', 1)
s = s.replace('        entries_ = {};', '        entries_ = {}; retries_ = {};', 1)
old = '''    for (const auto& entry : entries_) {
        if (!entry || entry->key != key) continue;
        if (entry->modes[0].state == SizingState::Pending && (!inFlight_ || *inFlight_ != key)) pending_ = key;
        return *entry;
    }'''
new = '''    for (size_t i=0; i<entries_.size(); ++i) {
        const auto& entry=entries_[i];
        if (!entry || entry->key != key) continue;
        const bool error=key.provider==Upscaler::Dlss && key.outputWidth && key.outputHeight &&
            std::any_of(entry->modes.begin(),entry->modes.end(),[](const auto& m){return m.state==SizingState::Error;});
        const auto& retry=retries_[i];
        // At most three attempts per cached key/epoch, at 1s then 2s backoff.
        // Working quality modes remain usable while a failed DLAA mode retries.
        if ((!inFlight_ || *inFlight_ != key) &&
            (entry->modes[0].state==SizingState::Pending ||
             (error && retry.failures<3 && clock_()>=retry.notBefore))) pending_=key;
        return *entry;
    }'''
assert s.count(old)==1
s=s.replace(old,new,1)
s=s.replace('    auto request = pending_;\n', '    if (!pending_ || inFlight_) return std::nullopt;\n    auto request = pending_;\n',1)
start=s.index('void SizingCache::PublishSizing(')
end=s.index('\nvoid SizingCache::ResetSizing(', start)
s=s[:start]+'''void SizingCache::PublishSizing(OutputSizing sizing) {
    std::lock_guard lock(mutex_);
    if (sizing.key.deviceEpoch != deviceEpoch_) return;
    sizing.revision = ++revision_;
    size_t index=entries_.size();
    for (size_t i=0;i<entries_.size();++i)
        if (entries_[i] && entries_[i]->key==sizing.key) {index=i;break;}
    if (index==entries_.size()) {
        for (size_t i=0;i<entries_.size();++i) if (!entries_[i]) {index=i;break;}
        if (index==entries_.size()) index=0;
        retries_[index]={};
    }
    auto& retry=retries_[index];
    const bool error=sizing.key.provider==Upscaler::Dlss &&
        std::any_of(sizing.modes.begin(),sizing.modes.end(),[](const auto& m){return m.state==SizingState::Error;});
    if (error) {
        retry.failures=std::min(retry.failures+1,3u);
        const uint64_t delay=uint64_t(1000) << (retry.failures-1);
        const uint64_t now=clock_();
        retry.notBefore=now>std::numeric_limits<uint64_t>::max()-delay ?
            std::numeric_limits<uint64_t>::max() : now+delay;
    } else retry={};
    if (inFlight_ && *inFlight_==sizing.key) inFlight_.reset();
    entries_[index]=std::move(sizing);
}
''' + s[end:]
s=s.replace('    entries_ = {};\n    pending_.reset();', '    entries_ = {}; retries_ = {};\n    pending_.reset();',1)
p.write_bytes(s.encode())

p=Path('tools/tests/motion_replay/CMakeLists.txt')
s=p.read_text()
assert 'sr_hybrid_motion_gpu_test' not in s
s+='''
# SR hybrid producer: real Vulkan pixels, plus strict CPU identity/retry contracts.
add_executable(sr_hybrid_motion_gpu_test "${ROOT}/tools/tests/sr_hybrid_motion_gpu_test.cpp" "${CMAKE_CURRENT_LIST_DIR}/platform_stub.cpp")
target_link_libraries(sr_hybrid_motion_gpu_test PRIVATE mv_runtime)
add_test(NAME sr_hybrid_motion_gpu COMMAND sr_hybrid_motion_gpu_test)
add_executable(sr_hybrid_motion_test "${ROOT}/tools/tests/sr_hybrid_motion_test.cpp" "${ROOT}/LostOdysseyRecomp/gpu/upscaling_plan.cpp")
target_include_directories(sr_hybrid_motion_test PRIVATE "${ROOT}/LostOdysseyRecomp")
add_test(NAME sr_hybrid_motion_cpu COMMAND sr_hybrid_motion_test)
'''
p.write_bytes(s.encode())
print('APPLIED: pinned runtime integration; no game assets or user data touched')
