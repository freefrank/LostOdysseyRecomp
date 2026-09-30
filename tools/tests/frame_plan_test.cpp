#include <gpu/frame_plan.h>
#include <gpu/movie_clear.h>
#include <settings/config.h>
#include <cstdio>
#include <cstdlib>
#include <thread>

namespace {
int checks = 0;
void Require(bool value, const char* what) { ++checks; if (!value) { std::fprintf(stderr, "FAIL: %s\n", what); std::exit(1); } }
}

int main()
{
    using namespace gpu::frame_plan;
    const settings::Config defaults;
    Require(defaults.upscaler == gpu::upscaling::Upscaler::Off && defaults.dlssQuality == gpu::upscaling::DlssQuality::Quality &&
        defaults.antialiasing == 0 && defaults.internalResolution == 0 && defaults.scalingQuality == 1,
        "P1 config defaults preserve legacy preferences");
    Require(Choose(3, 7, 0, 2560, 1080).width == 2560 && Choose(3, 7, 0, 2560, 1080).output.drawable.width == 2560, "2560x1080 plan retains native raster and output");
    Require(Choose(4, 8, 0, 3440, 1440).height == 1440, "3440x1440 plan retains native raster");
    Require(Choose(5, 9, 1080, 3440, 1440).width == 2580, "manual height preserves wide aspect with integer rounding");
    Require(Choose(6, 10, 0, 3440, 1440, true).requiresReadback, "readback is native before plan publication");
    Require(Reduced({7, 11, 3440, 1440}, 1080).height == 1080, "failure retry preserves plan aspect");
    Require(Reduced({8, 12, 1720, 720}, 720).height == 720, "lowest retry height does not loop below supported limit");
    FailureState failures;
    uint64_t retryEpoch = 0;
    auto retry = AdvanceCpuPlan(failures, 1, retryEpoch, ~0ull, 720, 0, 3440, 1440, false);
    Require(retry.geometryEpoch == 1 && retry.width == 3440 && retry.height == 1440, "first valid plan has nonzero epoch");
    retry = AdvanceCpuPlan(failures, 2, retryEpoch, retry.geometryEpoch, 1080, 0, 3440, 1440, false);
    Require(retry.geometryEpoch == 2 && retry.width == 2580 && retry.height == 1080, "first failure reduces the original request");
    retry = AdvanceCpuPlan(failures, 3, retryEpoch, retry.geometryEpoch, 810, 0, 3440, 1440, false);
    Require(retry.geometryEpoch == 3 && retry.width == 1935 && retry.height == 810, "repeated failure keeps decreasing the request cap");
    retry = AdvanceCpuPlan(failures, 4, retryEpoch, retry.geometryEpoch, 720, 0, 3440, 1440, false);
    Require(retry.geometryEpoch == 4 && retry.width == 1720 && retry.height == 720, "failure reaches supported minimum once");
    retry = AdvanceCpuPlan(failures, 5, retryEpoch, retry.geometryEpoch, 720, 0, 3440, 1440, false);
    Require(retry.failed && retry.geometryEpoch == 4 && retry.width == 1720 && retry.height == 720, "lowest failure remains terminal for its request");
    retry = AdvanceCpuPlan(failures, 6, retryEpoch, retry.geometryEpoch, 720, 0, 2560, 1080, false);
    Require(!retry.failed && retry.geometryEpoch == 5 && retry.width == 2560 && retry.height == 1080, "request change clears terminal failure and cap");
    Require(gpu::movie_clear::ScaleBoundary(163.63636f, 3440, 1280) == 440, "movie bar boundary is float-scaled then rounded once");

    CommandTags tags;
    tags.Store(0x1000, {11, 12, 2560, 1080});
    Require(tags.Take(0x1000)->width == 2560, "tag follows its reserved command");
    Require(!tags.Take(0x1000), "tag is consumed exactly once");
    tags.Store(0x8336A7A4, {12, 13, 2560, 1080});
    Require(tags.Take(0x8336A7A4)->cpuSerial == 12, "production ring object address tags normally");

    wire::PlanStage stage;
    Require(!stage.Write(wire::PlanBase, wire::Magic), "plan begin stages split packet");
    stage.Write(wire::PlanBase + 1, 13); stage.Write(wire::PlanBase + 2, 0);
    stage.Write(wire::PlanBase + 3, 14); stage.Write(wire::PlanBase + 4, 0);
    stage.Write(wire::PlanBase + 5, 3440); stage.Write(wire::PlanBase + 6, 1440);
    Require(stage.Write(wire::PlanBase + 7, wire::Magic)->width == 3440, "legacy private registers commit after split writes");
    FramePlan full{21, 22, 1114, 626};
    full.requestSignature = 23; full.deviceEpoch = 24; full.sizingRevision = 25;
    full.output = {{2560, 1440}, 10, 20, 2540, 1400};
    full.legacyWidth = 2560; full.legacyHeight = 1440;
    full.requestedUpscaler = gpu::upscaling::Upscaler::Dlss;
    full.dlssQuality = gpu::upscaling::DlssQuality::Balanced;
    full.consumer = gpu::upscaling::TemporalConsumer::DlssInputs;
    full.inputProbe = true;
    full.legacyAA = 3;
    const auto words = wire::EncodePlan(full);
    Require(words.size() == wire::PlanWordCount && words.size() <= wire::PlanWordCapacity, "versioned packet stays in private register range");
    wire::PlanStage fullStage;
    std::optional<FramePlan> decoded;
    for (uint32_t index = 0; index < words.size(); ++index) decoded = fullStage.Write(wire::PlanBase + index, words[index]);
    Require(decoded && *decoded == full, "versioned packet commits the complete snapshot");
    auto Decode = [](const auto& packet) {
        wire::PlanStage reader;
        std::optional<FramePlan> result;
        for (uint32_t i = 0; i < packet.size(); ++i) result = reader.Write(wire::PlanBase + i, packet[i]);
        return result;
    };
    auto v2 = words; v2[1] = 2;
    Require(Decode(v2) == full, "known v2 full packet decodes with legacy flags");
    for (auto consumer : {gpu::upscaling::TemporalConsumer::None, gpu::upscaling::TemporalConsumer::LegacyTaa,
             gpu::upscaling::TemporalConsumer::DlssInputs, gpu::upscaling::TemporalConsumer::DlssSr,
             gpu::upscaling::TemporalConsumer::FsrSr}) {
        for (auto quality : {gpu::upscaling::FsrQuality::Quality, gpu::upscaling::FsrQuality::Balanced,
                 gpu::upscaling::FsrQuality::Performance, gpu::upscaling::FsrQuality::NativeAA}) {
            auto candidate = full;
            candidate.requestedUpscaler = gpu::upscaling::Upscaler::Fsr;
            candidate.consumer = consumer;
            candidate.fsrQuality = quality;
            candidate.frameGeneration = gpu::upscaling::FrameGeneration::Dlss2x;
            Require(Decode(wire::EncodePlan(candidate)) == candidate, "v3 consumer/FSR quality/FG roundtrip");
        }
    }
    for (auto provider : {gpu::upscaling::Upscaler::Off, gpu::upscaling::Upscaler::Dlss,
             gpu::upscaling::Upscaler::Fsr}) {
        auto candidate = full;
        candidate.requestedUpscaler = provider;
        candidate.consumer = gpu::upscaling::TemporalConsumer::FsrSr;
        candidate.legacyAA = 0;
        Require(Decode(wire::EncodePlan(candidate)) == candidate,
            "v3 FSR consumer bit cannot alias legacy AA for any provider");
    }
    auto malformed = words; malformed[1] = 4;
    Require(!Decode(malformed), "unknown version rejected");
    malformed = words; malformed[22] = (malformed[22] & ~3u) | 3u;
    Require(!Decode(malformed), "unknown provider rejected");
    malformed = words; malformed[22] |= (3u << 4) | (1u << 21);
    Require(!Decode(malformed), "unknown consumer rejected");
    malformed = words; malformed[22] |= 1u << 25;
    Require(!Decode(malformed), "unknown flag rejected");
    malformed = v2; malformed[22] |= 1u << 21;
    Require(!Decode(malformed), "new v3 bits cannot be interpreted as v2 legacy AA");
    malformed = v2; malformed[22] = (malformed[22] & ~3u) | 2u;
    Require(!Decode(malformed), "v2 cannot advertise FSR");
    Require(FullRequestSignature(full, 0, {1114, 626}) != FullRequestSignature(full, 1080, {1114, 626}) &&
        FullRequestSignature(full, 0, {1114, 626}) != FullRequestSignature(full, 0, {960, 540}),
        "production request signature includes legacy mode and recommended input");
    const PlanFailure matching{full.geometryEpoch, full.requestSignature, 720, FailureReason::DlssOutOfMemory};
    Require(MatchesPlanFailure(full, matching) && !MatchesPlanFailure(full, {full.geometryEpoch + 1, full.requestSignature, 720, FailureReason::DlssOutOfMemory}),
        "production failure matcher rejects stale geometry");
    const auto sourceDecision = ResolvePresentationDecision(&full, false, 0, 0);
    Require(sourceDecision.requestedAA == 0 && !sourceDecision.bypassAA && ResolvePresentationDecision(&full, true, 0, 0).bypassAA,
        "presentation uses effective source AA and only actual renderer coverage bypasses it");
    Require(ResolvePresentationDecision(nullptr, false, 2, 1).requestedAA == 2, "unknown presentation source uses explicit legacy fallback");
    gpu::upscaling::OutputSizing ready{}; ready.key={9,1280,720}; ready.revision=7;
    for(auto& mode:ready.modes){mode.state=gpu::upscaling::SizingState::Ready;mode.optimal={640,360};}
    PlannerState producer;
    PlannerInput input{0,3,1,gpu::upscaling::Upscaler::Dlss,gpu::upscaling::DlssQuality::Quality,{{1280,720},0,0,1280,720},{gpu::backend::Backend::Vulkan,9,true,true},&ready,false,true};
    const auto first=producer.Begin(input); const auto second=producer.Begin(input);
    Require(first.width==640&&first.height==360&&second.cpuSerial>first.cpuSerial,"production planner applies ready sizing across serials");
    bool accepted=false;
    std::thread gpuWorker([&] { accepted=producer.ReportFailure({first.geometryEpoch,first.requestSignature,720,FailureReason::DlssUnavailable}); });
    gpuWorker.join();
    Require(accepted,"older matching GPU plan failure is accepted after a newer CPU serial");
    const auto fallback=producer.Begin(input);
    Require(!fallback.failed&&fallback.width==fallback.legacyWidth&&!fallback.inputProbe,"production planner disables DLSS for the next legacy plan");
    const auto stableFallback=producer.Begin(input);
    Require(!stableFallback.failed&&stableFallback.geometryEpoch==fallback.geometryEpoch&&stableFallback.cpuSerial>fallback.cpuSerial,
        "unchanged DLSS fallback keeps a fresh stable geometry epoch across serials");
    input.internalResolution=1080; const auto changed=producer.Begin(input);
    Require(!changed.failed,"new config request clears prior failure latch");
    input.readback=true; const auto readbackPlan=producer.Begin(input);
    Require(readbackPlan.requiresReadback&&!readbackPlan.inputProbe,"readback never selects probe low-resolution input");
    PlannerState bounded;
    PlannerInput varied=input; varied.readback=false; varied.upscaler=gpu::upscaling::Upscaler::Off;
    for(uint32_t i=0;i<12;++i){varied.internalResolution=i&1?720:1080;bounded.Begin(varied);}
    Require(!bounded.ReportFailure({9999,9999,720,FailureReason::Unknown}),"unknown failure is rejected after more than eight requests");
    PlannerState legacyOom;
    PlannerInput legacy{}; legacy.internalResolution=2160;legacy.output={{3840,2160},0,0,3840,2160};
    auto high=legacyOom.Begin(legacy); Require(legacyOom.ReportFailure({high.geometryEpoch,high.requestSignature,1080,FailureReason::DlssOutOfMemory}),"legacy OOM accepted");
    auto reduced=legacyOom.Begin(legacy); Require(reduced.geometryEpoch!=high.geometryEpoch&&reduced.height==1080,"legacy OOM gets fresh reduced epoch");
    Require(legacyOom.ReportFailure({reduced.geometryEpoch,reduced.requestSignature,720,FailureReason::DlssOutOfMemory}),"reduced OOM accepted");
    auto floor=legacyOom.Begin(legacy); Require(floor.height==720,"legacy OOM reaches 720 floor");
    Require(legacyOom.ReportFailure({floor.geometryEpoch,floor.requestSignature,720,FailureReason::DlssOutOfMemory}),"floor failure accepted");
    auto terminal=legacyOom.Begin(legacy); Require(terminal.height==720&&terminal.geometryEpoch==floor.geometryEpoch&&terminal.failed,"legacy OOM terminal floor remains stable");
    PlannerState resetMailbox;
    PlannerInput identityOne{}; identityOne.internalResolution=1080; identityOne.output={{1920,1080},0,0,1920,1080};
    const auto firstIdentity=resetMailbox.Begin(identityOne);
    identityOne.internalResolution=720; const auto failedIdentity=resetMailbox.Begin(identityOne);
    Require(resetMailbox.ReportFailure({failedIdentity.geometryEpoch,failedIdentity.requestSignature,720,FailureReason::InvalidInput}),
        "mailbox accepts failure before a request identity change");
    identityOne.internalResolution=0; const auto clearedIdentity=resetMailbox.Begin(identityOne);
    Require(resetMailbox.ReportFailure({clearedIdentity.geometryEpoch,clearedIdentity.requestSignature,720,FailureReason::InvalidInput}),
        "cleared mailbox restarts its write index before accepting a new failure");
    Require(firstIdentity.requestSignature != failedIdentity.requestSignature && failedIdentity.requestSignature != clearedIdentity.requestSignature,
        "distinct identities reach the latch-clear path");
    PlannerState requestedDlssLegacy;
    PlannerInput dlssLegacy{}; dlssLegacy.internalResolution=1080; dlssLegacy.antialiasing=3;
    dlssLegacy.upscaler=gpu::upscaling::Upscaler::Dlss; dlssLegacy.output={{1920,1080},0,0,1920,1080};
    const auto ordinaryLegacy=requestedDlssLegacy.Begin(dlssLegacy);
    Require(ordinaryLegacy.consumer==gpu::upscaling::TemporalConsumer::None&&ordinaryLegacy.height==1080,
        "requested DLSS without ready input sizing produces an actual legacy plan");
    Require(requestedDlssLegacy.ReportFailure({ordinaryLegacy.geometryEpoch,ordinaryLegacy.requestSignature,810,FailureReason::DlssOutOfMemory}),
        "ordinary requested-DLSS legacy OOM is accepted");
    const auto ordinaryReduced=requestedDlssLegacy.Begin(dlssLegacy);
    Require(ordinaryReduced.height==810&&ordinaryReduced.geometryEpoch!=ordinaryLegacy.geometryEpoch&&
        ordinaryReduced.requestSignature==ordinaryLegacy.requestSignature,
        "ordinary requested-DLSS legacy OOM reduces with a fresh epoch and stable request signature");
    PlannerState dlssFallback;
    gpu::upscaling::OutputSizing probeSizing{}; probeSizing.key={9,1920,1080}; probeSizing.revision=1;
    for(auto& mode:probeSizing.modes){mode.state=gpu::upscaling::SizingState::Ready;mode.optimal={960,540};}
    PlannerInput dlssProbe=dlssLegacy; dlssProbe.device={gpu::backend::Backend::Vulkan,9,true,true};
    dlssProbe.sizing=&probeSizing; dlssProbe.inputProbeRequested=true;
    const auto probePlan=dlssFallback.Begin(dlssProbe);
    Require(probePlan.consumer==gpu::upscaling::TemporalConsumer::DlssInputs,"ready probe starts with DLSS inputs");
    Require(dlssFallback.ReportFailure({probePlan.geometryEpoch,probePlan.requestSignature,720,FailureReason::DlssOutOfMemory}),
        "DLSS input failure is accepted before legacy fallback");
    const auto fallbackLegacy=dlssFallback.Begin(dlssProbe);
    Require(!fallbackLegacy.failed&&fallbackLegacy.consumer==gpu::upscaling::TemporalConsumer::None&&fallbackLegacy.height==1080&&
        fallbackLegacy.geometryEpoch!=probePlan.geometryEpoch&&fallbackLegacy.requestSignature==probePlan.requestSignature,
        "DLSS failure disables DLSS for a fresh legacy fallback without changing request identity");
    Require(dlssFallback.ReportFailure({fallbackLegacy.geometryEpoch,fallbackLegacy.requestSignature,810,FailureReason::DlssOutOfMemory}),
        "actual legacy fallback OOM is accepted");
    const auto fallbackReduced=dlssFallback.Begin(dlssProbe);
    Require(!fallbackReduced.failed&&fallbackReduced.consumer==gpu::upscaling::TemporalConsumer::None&&fallbackReduced.height==810&&
        fallbackReduced.geometryEpoch!=fallbackLegacy.geometryEpoch&&fallbackReduced.requestSignature==probePlan.requestSignature,
        "actual legacy fallback OOM reduces with a fresh epoch and stable request signature");
    const auto fallbackContinued=dlssFallback.Begin(dlssProbe);
    const auto fallbackContinuedAgain=dlssFallback.Begin(dlssProbe);
    Require(fallbackContinued.consumer==gpu::upscaling::TemporalConsumer::None&&fallbackContinued.height==810&&
        fallbackContinued.geometryEpoch==fallbackReduced.geometryEpoch&&fallbackContinued.requestSignature==probePlan.requestSignature&&
        fallbackContinuedAgain.consumer==gpu::upscaling::TemporalConsumer::None&&fallbackContinuedAgain.height==810&&
        fallbackContinuedAgain.geometryEpoch==fallbackReduced.geometryEpoch&&fallbackContinuedAgain.requestSignature==probePlan.requestSignature,
        "DLSS-disabled request keeps the reduced legacy plan across later serials");
    Require(dlssFallback.ReportFailure({fallbackContinuedAgain.geometryEpoch,fallbackContinuedAgain.requestSignature,720,FailureReason::DlssOutOfMemory}),
        "continued legacy plan accepts a later OOM");
    const auto fallbackFloor=dlssFallback.Begin(dlssProbe);
    const auto fallbackFloorContinued=dlssFallback.Begin(dlssProbe);
    Require(fallbackFloor.consumer==gpu::upscaling::TemporalConsumer::None&&fallbackFloor.height==720&&
        fallbackFloor.geometryEpoch!=fallbackReduced.geometryEpoch&&fallbackFloorContinued.consumer==gpu::upscaling::TemporalConsumer::None&&
        fallbackFloorContinued.height==720&&fallbackFloorContinued.geometryEpoch==fallbackFloor.geometryEpoch&&
        fallbackFloorContinued.requestSignature==probePlan.requestSignature,
        "subsequent legacy OOM reaches a stable floor without re-enabling DLSS");
    PlannerInput recoveredDlss=dlssProbe; recoveredDlss.quality=gpu::upscaling::DlssQuality::Balanced;
    const auto recoveredProbe=dlssFallback.Begin(recoveredDlss);
    Require(recoveredProbe.consumer==gpu::upscaling::TemporalConsumer::DlssInputs&&recoveredProbe.inputProbe&&recoveredProbe.legacyHeight==1080&&
        recoveredProbe.requestSignature!=probePlan.requestSignature&&recoveredProbe.geometryEpoch!=fallbackFloor.geometryEpoch,
        "a same-geometry new request clears old retry state and can probe DLSS again");
    PlannerState sameExtent;
    PlannerInput manual720{}; manual720.internalResolution=720; manual720.output={{1280,720},0,0,1280,720};
    const auto oldExtent=sameExtent.Begin(manual720);
    Require(sameExtent.ReportFailure({oldExtent.geometryEpoch,oldExtent.requestSignature,720,FailureReason::InvalidInput}),
        "same-extent old request failure is accepted");
    manual720.internalResolution=0; const auto autoExtent=sameExtent.Begin(manual720);
    Require(autoExtent.width==oldExtent.width&&autoExtent.height==oldExtent.height&&autoExtent.geometryEpoch!=oldExtent.geometryEpoch&&
        autoExtent.requestSignature!=oldExtent.requestSignature&&sameExtent.ReportFailure({autoExtent.geometryEpoch,autoExtent.requestSignature,720,FailureReason::InvalidInput}),
        "same-extent new request gets a fresh epoch outside the renderer failed set");
    input.readback=false; input.quality=gpu::upscaling::DlssQuality::Balanced; const auto qualityChanged=producer.Begin(input);
    Require(qualityChanged.geometryEpoch!=fallback.geometryEpoch,"quality change receives a fresh geometry epoch");
    const auto letterbox = gpu::upscaling::ResolveOutputRegion({1280, 800});
    Require(letterbox.x == 0 && letterbox.y == 40 && letterbox.width == 1280 && letterbox.height == 720 &&
        gpu::upscaling::ResolveOutputRegion({2560, 1080}).width == 2560, "output region excludes bars while preserving wide output");
    gpu::upscaling::SizingCache cache;
    const gpu::upscaling::SizingKey key{3, 1280, 720};
    Require(cache.LookupOrRequestSizing(key).modes[0].state == gpu::upscaling::SizingState::Pending && cache.TakeSizingRequest() == key, "sizing miss requests exact output without waiting");
    gpu::upscaling::OutputSizing sized{}; sized.key = key; sized.modes[0].state = gpu::upscaling::SizingState::Ready; sized.modes[0].optimal = {960, 540};
    cache.PublishSizing(sized);
    Require(cache.LookupOrRequestSizing(key).modes[0].optimal.width == 960, "sizing cache publishes exact-key results");
    cache.ResetSizing(4);
    cache.PublishSizing(sized); // stale epoch is ignored rather than reviving old output dimensions.
    Require(cache.LookupOrRequestSizing({4, 1280, 720}).modes[0].state == gpu::upscaling::SizingState::Pending,
        "sizing cache rejects stale device-epoch results");
    const gpu::upscaling::SizingKey cacheFirst{5, 1280, 720}, latest{5, 1920, 1080};
    cache.LookupOrRequestSizing(cacheFirst); cache.LookupOrRequestSizing(latest);
    Require(cache.TakeSizingRequest() == latest, "latest sizing miss replaces an older pending request");
    auto latestResult = sized; latestResult.key = latest; latestResult.modes[0].optimal = {1280, 720};
    cache.PublishSizing(latestResult);
    Require(cache.LookupOrRequestSizing(latest).modes[0].state == gpu::upscaling::SizingState::Ready &&
        cache.LookupOrRequestSizing({4, 1280, 720}).modes[0].state == gpu::upscaling::SizingState::Error,
        "published latest request completes and old epoch cannot reset the cache");
    const gpu::upscaling::SizingKey fsrKey{5, 1920, 1080, gpu::upscaling::Upscaler::Fsr};
    Require(cache.LookupOrRequestSizing(fsrKey).modes[0].state == gpu::upscaling::SizingState::Pending &&
        cache.TakeSizingRequest() == fsrKey && cache.Peek(latest)->modes[0].state == gpu::upscaling::SizingState::Ready,
        "FSR sizing request and NGX key stay separate");
    gpu::upscaling::OutputSizing unsupported{}; unsupported.key = fsrKey;
    for (auto& mode : unsupported.modes) mode.state = gpu::upscaling::SizingState::Unavailable;
    cache.PublishSizing(unsupported);
    Require(cache.LookupOrRequestSizing(fsrKey).modes[0].state == gpu::upscaling::SizingState::Unavailable &&
        !cache.TakeSizingRequest(), "unsupported provider is cached without repeat requests");
    auto areaKey = latest; areaKey.outputY = 40;
    Require(cache.LookupOrRequestSizing(areaKey).modes[0].state == gpu::upscaling::SizingState::Pending &&
        cache.TakeSizingRequest() == areaKey && cache.Peek(fsrKey).has_value(), "content origin isolates sizing cache");
    PlannerState providerPlanner;
    auto dlssInput = input; dlssInput.readback = false; dlssInput.inputProbeRequested = false;
    dlssInput.quality = gpu::upscaling::DlssQuality::Quality; dlssInput.internalResolution = 0;
    const auto submittedPlan = providerPlanner.Begin(dlssInput);
    Require(submittedPlan.consumer == gpu::upscaling::TemporalConsumer::DlssSr &&
        providerPlanner.ReportExecution({submittedPlan, 1, 11, DlssExecutionOutcome::Submitted}), "DLSS submission accepted");
    auto fsrInput = dlssInput; fsrInput.upscaler = gpu::upscaling::Upscaler::Fsr;
    fsrInput.fsrQuality = gpu::upscaling::FsrQuality::Balanced;
    const auto fsrFallback = providerPlanner.Begin(fsrInput);
    Require(fsrFallback.consumer != gpu::upscaling::TemporalConsumer::DlssSr &&
        fsrFallback.requestSignature != submittedPlan.requestSignature && !providerPlanner.Observe().execution &&
        !providerPlanner.ReportExecution({submittedPlan, 2, 12, DlssExecutionOutcome::Submitted}),
        "old DLSS submission cannot satisfy unsupported FSR request");
    fsrInput.fsrQuality = gpu::upscaling::FsrQuality::Performance;
    Require(providerPlanner.Begin(fsrInput).requestSignature != fsrFallback.requestSignature,
        "FSR quality affects request identity without NGX sizing");
    std::printf("frame plan: %d checks passed\n", checks);
}
