// BR03 CPU status: real PlannerState plus DescribeDlssRuntime. No NGX image,
// shader, GPU submit, or game launch.
#include "gpu/frame_plan.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>

namespace {
int checks = 0;
void Require(bool value, const char* what)
{
    ++checks;
    if (!value) {
        std::fprintf(stderr, "FAIL: %s\n", what);
        std::exit(1);
    }
}

using gpu::backend::Backend;
using gpu::frame_plan::DescribeDlssRuntime;
using gpu::frame_plan::DlssEffectPhase;
using gpu::frame_plan::DlssEffectReason;
using gpu::frame_plan::DlssExecutionObservation;
using gpu::frame_plan::DlssExecutionOutcome;
using gpu::frame_plan::PlannerInput;
using gpu::frame_plan::PlannerState;
using gpu::upscaling::BackendDeviceSnapshot;
using gpu::upscaling::DlssQuality;
using gpu::upscaling::OutputSizing;
using gpu::upscaling::SizingState;
using gpu::upscaling::TemporalConsumer;
using gpu::upscaling::Upscaler;

BackendDeviceSnapshot Device(uint64_t epoch, bool ready = true, bool dlss = true)
{
    BackendDeviceSnapshot device;
    device.backend = Backend::Vulkan;
    device.deviceEpoch = epoch;
    device.deviceReady = ready;
    device.dlssAvailable = dlss;
    return device;
}

OutputSizing Ready(uint64_t epoch, uint32_t outputWidth, uint32_t outputHeight, uint32_t inputWidth, uint32_t inputHeight,
    uint32_t balancedWidth, uint32_t balancedHeight)
{
    OutputSizing sizing;
    sizing.key = {epoch, outputWidth, outputHeight};
    sizing.revision = 1;
    for (auto& mode : sizing.modes) {
        mode.state = SizingState::Ready;
        mode.optimal = mode.minimum = mode.maximum = {inputWidth, inputHeight};
    }
    sizing.modes[1].optimal = sizing.modes[1].minimum = sizing.modes[1].maximum = {balancedWidth, balancedHeight};
    return sizing;
}

PlannerInput Input(const BackendDeviceSnapshot& device, const OutputSizing* sizing, DlssQuality quality)
{
    PlannerInput input;
    input.upscaler = Upscaler::Dlss;
    input.quality = quality;
    input.output = {{sizing ? sizing->key.outputWidth : 2560u, sizing ? sizing->key.outputHeight : 1440u}, 0, 0,
        sizing ? sizing->key.outputWidth : 2560u, sizing ? sizing->key.outputHeight : 1440u};
    input.device = device;
    input.sizing = sizing;
    return input;
}

DlssExecutionObservation Submitted(const gpu::frame_plan::FramePlan& plan, uint64_t renderFrame, uint64_t serial)
{
    DlssExecutionObservation observation;
    observation.plan = plan;
    observation.renderFrame = renderFrame;
    observation.submissionSerial = serial;
    observation.outcome = DlssExecutionOutcome::Submitted;
    observation.reason = DlssEffectReason::None;
    return observation;
}

void CheckNotExecutedAndOrdering()
{
    const auto device = Device(1);
    auto sizing = Ready(1, 2560, 1440, 1707, 960, 1483, 835);
    PlannerState planner;
    const auto plan = planner.Begin(Input(device, &sizing, DlssQuality::Quality));
    Require(plan.consumer == TemporalConsumer::DlssSr, "ready sizing selects DLSS SR");
    auto effect = DescribeDlssRuntime(device, planner.Observe(), &sizing);
    Require(effect.phase == DlssEffectPhase::AwaitingExecution && effect.phase != DlssEffectPhase::Active && !effect.execution,
        "a plan that has not executed is not active");

    const auto newer = planner.Begin(Input(device, &sizing, DlssQuality::Quality));
    Require(newer.cpuSerial > plan.cpuSerial && newer.geometryEpoch == plan.geometryEpoch, "CPU serial may lead the same geometry");
    Require(planner.ReportExecution(Submitted(plan, 4, 11)), "an older CPU serial on the current geometry is accepted");
    auto ahead = newer;
    ahead.cpuSerial = newer.cpuSerial + 1;
    Require(!planner.ReportExecution(Submitted(ahead, 5, 12)), "a CPU serial ahead of the planner is rejected");
    Require(planner.Observe().execution && planner.Observe().execution->plan.cpuSerial == plan.cpuSerial, "the accepted older serial remains");
    effect = DescribeDlssRuntime(device, planner.Observe(), &sizing);
    Require(effect.phase == DlssEffectPhase::Active && effect.execution && effect.execution->submissionSerial == 11,
        "the lagged GPU submission is the active effect");

    auto sameFrame = Submitted(plan, 4, 20);
    sameFrame.outcome = DlssExecutionOutcome::Fallback;
    sameFrame.reason = DlssEffectReason::NoEligibleScene;
    Require(!planner.ReportExecution(sameFrame), "the same render frame does not overwrite");
    auto olderFrame = Submitted(plan, 3, 21);
    Require(!planner.ReportExecution(olderFrame), "an older render frame does not overwrite");
    Require(planner.Observe().execution && planner.Observe().execution->outcome == DlssExecutionOutcome::Submitted &&
        planner.Observe().execution->submissionSerial == 11, "the first submission remains");

    auto missed = Submitted(newer, 6, 0);
    missed.outcome = DlssExecutionOutcome::Fallback;
    missed.reason = DlssEffectReason::NoEligibleScene;
    Require(planner.ReportExecution(missed), "a later frame with no eligible scene is accepted");
    effect = DescribeDlssRuntime(device, planner.Observe(), &sizing);
    Require(effect.phase != DlssEffectPhase::Active && effect.reason == DlssEffectReason::NoEligibleScene,
        "no eligible scene replaces the previous active submission");
}

void CheckQualityEpochAndStop()
{
    const auto device = Device(3);
    auto sizing = Ready(3, 2560, 1440, 1707, 960, 1483, 835);
    PlannerState planner;
    const auto quality = planner.Begin(Input(device, &sizing, DlssQuality::Quality));
    Require(planner.ReportExecution(Submitted(quality, 1, 4)), "quality A submission");
    const auto balanced = planner.Begin(Input(device, &sizing, DlssQuality::Balanced));
    Require(balanced.geometryEpoch != quality.geometryEpoch && balanced.width == 1483 && !planner.Observe().execution,
        "quality B clears A's execution");
    const auto again = planner.Begin(Input(device, &sizing, DlssQuality::Quality));
    Require(again.geometryEpoch != quality.geometryEpoch && again.width == 1707 && !planner.Observe().execution,
        "returning to quality A is a new geometry and does not revive the first submission");
    auto effect = DescribeDlssRuntime(device, planner.Observe(), &sizing);
    Require(effect.phase == DlssEffectPhase::AwaitingExecution && effect.phase != DlssEffectPhase::Active,
        "the second A is waiting for a new submission");

    auto epochTwo = Device(4);
    auto sizingTwo = Ready(4, 2560, 1440, 1707, 960, 1483, 835);
    const auto moved = planner.Begin(Input(epochTwo, &sizingTwo, DlssQuality::Quality));
    Require(moved.deviceEpoch == 4 && !planner.Observe().execution, "a device epoch change drops the old execution");
    effect = DescribeDlssRuntime(epochTwo, planner.Observe(), &sizingTwo);
    Require(effect.phase != DlssEffectPhase::Active && !effect.execution, "the new epoch does not show the old execution");
    Require(planner.ReportExecution(Submitted(moved, 2, 8)), "submission on the new epoch");
    auto hidden = epochTwo;
    hidden.deviceEpoch = 5;
    effect = DescribeDlssRuntime(hidden, planner.Observe(), &sizingTwo);
    Require(!effect.execution && effect.phase != DlssEffectPhase::Active, "a mismatched device epoch hides the execution");
    hidden = epochTwo;
    hidden.deviceReady = false;
    effect = DescribeDlssRuntime(hidden, planner.Observe(), &sizingTwo);
    Require(!effect.execution && effect.phase != DlssEffectPhase::Active && effect.phase != DlssEffectPhase::DeviceUnavailable,
        "a device that is not ready hides the execution");
    hidden = epochTwo;
    hidden.gpuWorkStopped = true;
    effect = DescribeDlssRuntime(hidden, planner.Observe(), &sizingTwo);
    Require(effect.phase == DlssEffectPhase::GpuStopped && effect.reason == DlssEffectReason::GpuWorkStopped &&
        !effect.execution && effect.phase != DlssEffectPhase::Active, "stopped GPU work is not an active submission");
}

void CheckFailureProbeAndSizing()
{
    const auto device = Device(6);
    auto sizing = Ready(6, 2560, 1440, 1707, 960, 1483, 835);
    PlannerState planner;
    const auto plan = planner.Begin(Input(device, &sizing, DlssQuality::Quality));
    Require(planner.ReportFailure({plan.geometryEpoch, plan.requestSignature, plan.legacyHeight,
        gpu::frame_plan::FailureReason::DlssUnavailable}), "matching failure latches");
    Require(planner.ReportExecution(Submitted(plan, 1, 3)), "a success report is stored without clearing the latch");
    auto effect = DescribeDlssRuntime(device, planner.Observe(), &sizing);
    Require(effect.failure && *effect.failure == gpu::frame_plan::FailureReason::DlssUnavailable &&
        effect.phase != DlssEffectPhase::Active && effect.reason == DlssEffectReason::RequestFailure,
        "a latched failure is not shown as submitted");
    const auto fallen = planner.Begin(Input(device, &sizing, DlssQuality::Quality));
    Require(fallen.consumer != TemporalConsumer::DlssSr && planner.Observe().persistentFailure &&
        !planner.Observe().execution, "the failure survives Begin and the old execution does not");
    effect = DescribeDlssRuntime(device, planner.Observe(), &sizing);
    Require(effect.failure && *effect.failure == gpu::frame_plan::FailureReason::DlssUnavailable &&
        effect.phase == DlssEffectPhase::TemporaryFallback && effect.phase != DlssEffectPhase::Active,
        "the next plan keeps the persistent failure");
    Require(!planner.ReportExecution(Submitted(plan, 2, 9)), "a late success for the old geometry does not overwrite the failure");
    effect = DescribeDlssRuntime(device, planner.Observe(), &sizing);
    Require(effect.failure && effect.phase != DlssEffectPhase::Active, "the late success is not active");

    auto probeInput = Input(device, &sizing, DlssQuality::Balanced);
    probeInput.inputProbeRequested = true;
    PlannerState probe;
    const auto probing = probe.Begin(probeInput);
    Require(probing.consumer == TemporalConsumer::DlssInputs, "probe selects DLSS inputs");
    effect = DescribeDlssRuntime(device, probe.Observe(), &sizing);
    Require(effect.phase == DlssEffectPhase::InputProbeOnly && effect.reason == DlssEffectReason::InputProbeOnly &&
        effect.phase != DlssEffectPhase::Active, "probe input is not an SR submission");

    auto failedSizing = sizing;
    failedSizing.modes[0].state = SizingState::Error;
    PlannerState sizingPlanner;
    sizingPlanner.Begin(Input(device, &failedSizing, DlssQuality::Quality));
    effect = DescribeDlssRuntime(device, sizingPlanner.Observe(), &failedSizing);
    Require(effect.phase != DlssEffectPhase::Active && effect.sizingState == SizingState::Error &&
        effect.reason == DlssEffectReason::SizingError, "a sizing error is not active");
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    Require(input.good(), "video source is missing");
    std::ostringstream text;
    text << input.rdbuf();
    return text.str();
}

std::string FunctionBody(const std::string& source, std::string_view signature)
{
    const auto at = source.find(signature);
    if (at == std::string::npos) return {};
    const auto brace = source.find('{', at);
    if (brace == std::string::npos) return {};
    int depth = 0;
    for (size_t i = brace; i < source.size(); ++i) {
        if (source[i] == '{') ++depth;
        else if (source[i] == '}') {
            if (--depth == 0) return source.substr(brace, i - brace + 1);
        }
    }
    return {};
}

void CheckPublicationSource()
{
    const auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const auto video = ReadFile(root / "LostOdysseyRecomp" / "gpu" / "video.cpp");
    const auto owned = FunctionBody(video, "void PublishOwnedDeviceCapability()");
    const auto cleared = FunctionBody(video, "void PublishClearedDeviceCapability()");
    const auto stop = FunctionBody(video, "void StopGpuWork(int32_t nativeResult)");
    Require(owned.find("gpuWorkStopped") != std::string::npos && owned.find("PublishDeviceCapability") != std::string::npos,
        "the device owner publishes whether GPU work has stopped");
    Require(cleared.find("gpuWorkStopped = false") != std::string::npos, "clear resets the stopped bit");
    const auto logged = stop.find("LOG_ERROR");
    const auto published = stop.find("PublishOwnedDeviceCapability");
    Require(logged != std::string::npos && published != std::string::npos && logged < published,
        "StopGpuWork publishes after the optional first-stop log");
    Require(stop.find("if (!g_submissionState.Stopped())") != std::string::npos &&
        stop.find("PublishOwnedDeviceCapability") > stop.find("g_submissionState.Stop"),
        "the publish is not limited to the first log");
}
}

int main()
{
    CheckNotExecutedAndOrdering();
    CheckQualityEpochAndStop();
    CheckFailureProbeAndSizing();
    CheckPublicationSource();
    std::printf("PASS: %d DLSS runtime status checks (planner/classify/publish source; no NGX image or GPU submit)\n", checks);
    return 0;
}
