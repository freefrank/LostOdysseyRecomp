// BR-02: device capability is a complete by-value snapshot, and the UI status
// is the last CPU plan rather than the saved upscaler request. No NGX image,
// shader, packaging, or game launch is involved.
#include "gpu/frame_plan.h"

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

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
using gpu::frame_plan::ClassifyDlssMenu;
using gpu::frame_plan::DescribeDlssRuntime;
using gpu::frame_plan::DlssEffectPhase;
using gpu::frame_plan::DlssMenuStatus;
using gpu::frame_plan::PlannerInput;
using gpu::frame_plan::PlannerState;
using gpu::upscaling::BackendDeviceSnapshot;
using gpu::upscaling::DlssQuality;
using gpu::upscaling::OutputSizing;
using gpu::upscaling::PublishDeviceCapability;
using gpu::upscaling::PublishedDeviceCapability;
using gpu::upscaling::SizingCache;
using gpu::upscaling::SizingState;
using gpu::upscaling::TemporalConsumer;
using gpu::upscaling::Upscaler;

BackendDeviceSnapshot Device(Backend backend, uint64_t epoch, bool ready, bool dlss)
{
    return {backend, epoch, ready, dlss};
}

OutputSizing ReadySizing(uint64_t epoch, uint32_t width, uint32_t height, uint32_t inputWidth, uint32_t inputHeight)
{
    OutputSizing sizing;
    sizing.key = {epoch, width, height};
    sizing.revision = 3;
    for (auto& mode : sizing.modes) {
        mode.state = SizingState::Ready;
        mode.optimal = {inputWidth, inputHeight};
        mode.minimum = mode.maximum = mode.optimal;
    }
    return sizing;
}

PlannerInput Input(Upscaler upscaler, const BackendDeviceSnapshot& device, const OutputSizing* sizing)
{
    PlannerInput input;
    input.internalResolution = 0;
    input.antialiasing = 0;
    input.scalingQuality = 1;
    input.upscaler = upscaler;
    input.quality = DlssQuality::Quality;
    input.output = {{2560, 1440}, 0, 0, 2560, 1440};
    input.device = device;
    input.sizing = sizing;
    return input;
}

gpu::frame_plan::DlssEffectSnapshot ReadEffect(PlannerState& planner, SizingCache& cache)
{
    const auto device = PublishedDeviceCapability();
    const auto observed = planner.Observe();
    std::optional<OutputSizing> sizing;
    if (observed.hasPlan && observed.plan.output.width && observed.plan.output.height)
        sizing = cache.Peek({device.deviceEpoch, observed.plan.output.width, observed.plan.output.height});
    return DescribeDlssRuntime(device, observed, sizing ? &*sizing : nullptr);
}

std::string ReadFile(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    Require(input.good(), "source file required by the publication contract is missing");
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

void CheckDefaultAndMenu()
{
    const auto initial = PublishedDeviceCapability();
    Require(initial.backend == Backend::D3D12 && initial.deviceEpoch == 0 && !initial.deviceReady && !initial.dlssAvailable,
        "unpublished capability is not a ready Vulkan DLSS device");
    const auto running = DescribeDlssRuntime(initial, {}, nullptr);
    Require(running.phase == DlssEffectPhase::Inactive && !gpu::upscaling::IsDlssConsumer(running.consumer),
        "no plan is not an active DLSS consumer");
    Require(ClassifyDlssMenu(running, Upscaler::Dlss, DlssQuality::Quality, Backend::D3D12) == DlssMenuStatus::NeedsVulkanRestart,
        "DLSS on the committed D3D12 device needs a Vulkan restart");
    Require(ClassifyDlssMenu(running, Upscaler::Dlss, DlssQuality::Quality, Backend::Vulkan) == DlssMenuStatus::BackendChangePending,
        "an edited Vulkan backend is not detected until it is the committed device");
}

void CheckSnapshotConsistency()
{
    constexpr uint64_t kCount = 100000;
    std::atomic<uint64_t> published{0};
    std::atomic<bool> failed{false};
    std::thread writer([&] {
        for (uint64_t epoch = 1; epoch <= kCount; ++epoch) {
            PublishDeviceCapability(Device((epoch % 3) == 0 ? Backend::D3D12 : Backend::Vulkan, epoch,
                (epoch % 2) == 0, (epoch % 4) < 2));
            published.store(epoch, std::memory_order_release);
        }
    });
    std::vector<std::thread> readers;
    for (int reader = 0; reader < 4; ++reader) {
        readers.emplace_back([&] {
            uint64_t seen = 0;
            while (seen < kCount && !failed.load(std::memory_order_relaxed)) {
                const auto snapshot = PublishedDeviceCapability();
                seen = std::max(seen, published.load(std::memory_order_acquire));
                if (snapshot.deviceEpoch == 0) continue;
                const bool consistent = snapshot.deviceReady == ((snapshot.deviceEpoch % 2) == 0) &&
                    snapshot.dlssAvailable == ((snapshot.deviceEpoch % 4) < 2) &&
                    snapshot.backend == ((snapshot.deviceEpoch % 3) == 0 ? Backend::D3D12 : Backend::Vulkan);
                if (!consistent) failed.store(true, std::memory_order_relaxed);
            }
        });
    }
    writer.join();
    for (auto& reader : readers) reader.join();
    Require(!failed.load(), "capability readers observe one complete backend/epoch/ready/DLSS generation");
    const auto finalSnapshot = PublishedDeviceCapability();
    Require(finalSnapshot.deviceEpoch == kCount && finalSnapshot.deviceReady == ((kCount % 2) == 0) &&
        finalSnapshot.dlssAvailable == ((kCount % 4) < 2) &&
        finalSnapshot.backend == ((kCount % 3) == 0 ? Backend::D3D12 : Backend::Vulkan),
        "final published generation is intact");
}

void CheckEnableCycle()
{
    constexpr uint64_t kEpoch = 7;
    const auto output = gpu::upscaling::OutputRegion{{2560, 1440}, 0, 0, 2560, 1440};
    const auto capable = Device(Backend::Vulkan, kEpoch, true, true);
    PublishDeviceCapability(capable);
    SizingCache cache;
    PlannerState planner;
    const auto off = planner.Begin(Input(Upscaler::Off, capable, nullptr));
    auto effect = ReadEffect(planner, cache);
    Require(!gpu::upscaling::IsDlssConsumer(off.consumer) && effect.phase == DlssEffectPhase::Inactive,
        "startup with upscaling off does not report DLSS active");
    Require(ClassifyDlssMenu(effect, Upscaler::Dlss, DlssQuality::Quality, Backend::Vulkan) != DlssMenuStatus::Active,
        "selecting DLSS before a DLSS plan exists is not treated as enabled");

    const auto pending = cache.LookupOrRequestSizing({kEpoch, output.width, output.height});
    Require(pending.modes[0].state == SizingState::Pending, "first DLSS enable records sizing without waiting");
    const auto requested = cache.TakeSizingRequest();
    Require(requested && *requested == pending.key, "GPU sizing request is the first-enable key");
    Require(cache.Peek(pending.key) && !cache.TakeSizingRequest(), "UI peek does not arm another NGX request");
    const auto first = planner.Begin(Input(Upscaler::Dlss, capable, &pending));
    effect = ReadEffect(planner, cache);
    Require(!gpu::upscaling::IsDlssConsumer(first.consumer) && effect.phase == DlssEffectPhase::TemporaryFallback &&
        effect.sizingKnown && effect.sizingState == SizingState::Pending && effect.plannedRequest == Upscaler::Dlss,
        "first enable stays a temporary fallback while sizing is pending");
    Require(ClassifyDlssMenu(effect, Upscaler::Dlss, DlssQuality::Quality, Backend::Vulkan) == DlssMenuStatus::TemporaryFallback,
        "pending sizing is not shown as enabled or as a permanent device failure");

    auto ready = ReadySizing(kEpoch, output.width, output.height, 1707, 960);
    cache.PublishSizing(ready);
    PublishDeviceCapability(capable);
    const auto cached = cache.Peek(pending.key);
    Require(cached && cached->modes[0].state == SizingState::Ready, "published sizing replaces the pending entry");
    const auto enabled = planner.Begin(Input(Upscaler::Dlss, capable, &*cached));
    effect = ReadEffect(planner, cache);
    Require(enabled.consumer == TemporalConsumer::DlssSr && enabled.width == 1707 && enabled.height == 960 &&
        effect.phase == DlssEffectPhase::AwaitingExecution && !effect.execution &&
        effect.inputWidth == 1707 && effect.outputWidth == 2560,
        "ready sizing selects the DLSS consumer and waits for a submission");
    Require(ClassifyDlssMenu(effect, Upscaler::Dlss, DlssQuality::Quality, Backend::Vulkan) == DlssMenuStatus::TemporaryFallback,
        "matching quality is not shown as enabled before a submission");
    Require(ClassifyDlssMenu(effect, Upscaler::Dlss, DlssQuality::Performance, Backend::Vulkan) == DlssMenuStatus::TemporaryFallback,
        "a different displayed quality is not reported as the running mode");

    const auto disabled = planner.Begin(Input(Upscaler::Off, capable, nullptr));
    effect = ReadEffect(planner, cache);
    Require(!gpu::upscaling::IsDlssConsumer(disabled.consumer) && disabled.width == 2560 &&
        effect.phase == DlssEffectPhase::Inactive && effect.plannedRequest == Upscaler::Off,
        "turning DLSS off leaves a non-DLSS consumer even though capability remains");
    Require(ClassifyDlssMenu(effect, Upscaler::Off, DlssQuality::Quality, Backend::Vulkan) == DlssMenuStatus::Inactive,
        "an off selection is not displayed as enabled");

    const auto again = cache.Peek(pending.key);
    const auto reenabled = planner.Begin(Input(Upscaler::Dlss, capable, again ? &*again : nullptr));
    effect = ReadEffect(planner, cache);
    Require(reenabled.consumer == TemporalConsumer::DlssSr && effect.phase == DlssEffectPhase::AwaitingExecution &&
        !effect.execution, "turning DLSS on again uses the cached ready sizing and waits for a submission");
}

void CheckFallbackStates()
{
    const auto output = gpu::upscaling::OutputRegion{{2560, 1440}, 0, 0, 2560, 1440};
    const auto d3d = Device(Backend::D3D12, 3, true, false);
    PublishDeviceCapability(d3d);
    PlannerState d3dPlanner;
    SizingCache d3dCache;
    const auto d3dPlan = d3dPlanner.Begin(Input(Upscaler::Dlss, d3d, nullptr));
    auto effect = ReadEffect(d3dPlanner, d3dCache);
    Require(!gpu::upscaling::IsDlssConsumer(d3dPlan.consumer) && effect.phase == DlssEffectPhase::NeedsVulkanRestart,
        "a DLSS request on D3D12 is a restart requirement, not an active consumer");
    Require(ClassifyDlssMenu(effect, Upscaler::Dlss, DlssQuality::Quality, Backend::D3D12) == DlssMenuStatus::NeedsVulkanRestart,
        "the committed D3D12 backend tells the menu to switch to Vulkan and restart");
    Require(ClassifyDlssMenu(effect, Upscaler::Dlss, DlssQuality::Dlaa, Backend::Vulkan) == DlssMenuStatus::BackendChangePending,
        "choosing Vulkan in the editor does not permanently disable DLSS before restart");

    gpu::frame_plan::PlannerObservation staleDlss;
    staleDlss.hasPlan = true;
    staleDlss.plan.requestedUpscaler = Upscaler::Dlss;
    staleDlss.plan.consumer = TemporalConsumer::DlssSr;
    staleDlss.plan.dlssQuality = DlssQuality::Quality;
    staleDlss.plan.deviceEpoch = 3;
    staleDlss.plan.width = 1707;
    staleDlss.plan.height = 960;
    staleDlss.plan.output = output;
    auto ready = ReadySizing(3, output.width, output.height, 1707, 960);
    Require(DescribeDlssRuntime(d3d, staleDlss, &ready).phase == DlssEffectPhase::NeedsVulkanRestart,
        "a stale DLSS consumer on D3D12 is not reported as enabled");

    const auto unavailable = Device(Backend::Vulkan, 4, true, false);
    PublishDeviceCapability(unavailable);
    PlannerState unavailablePlanner;
    SizingCache unavailableCache;
    unavailablePlanner.Begin(Input(Upscaler::Dlss, unavailable, nullptr));
    effect = ReadEffect(unavailablePlanner, unavailableCache);
    Require(effect.phase == DlssEffectPhase::DeviceUnavailable, "Vulkan without DLSS capability is device unavailable");
    Require(ClassifyDlssMenu(effect, Upscaler::Dlss, DlssQuality::Quality, Backend::Vulkan) == DlssMenuStatus::DeviceUnavailable,
        "the menu can say the current Vulkan device is unavailable");
    auto sameEpoch = staleDlss;
    sameEpoch.plan.deviceEpoch = unavailable.deviceEpoch;
    Require(DescribeDlssRuntime(unavailable, sameEpoch, &ready).phase == DlssEffectPhase::DeviceUnavailable,
        "a DLSS consumer bit does not override a negative capability snapshot");

    const auto capable = Device(Backend::Vulkan, 5, true, true);
    PublishDeviceCapability(capable);
    SizingCache cache;
    auto failed = ReadySizing(5, output.width, output.height, 1707, 960);
    failed.modes[0].state = SizingState::Error;
    cache.ResetSizing(5);
    cache.PublishSizing(failed);
    PlannerState failedPlanner;
    const auto peeked = cache.Peek({5, output.width, output.height});
    failedPlanner.Begin(Input(Upscaler::Dlss, capable, peeked ? &*peeked : nullptr));
    effect = ReadEffect(failedPlanner, cache);
    Require(effect.phase == DlssEffectPhase::TemporaryFallback && effect.sizingState == SizingState::Error,
        "a sizing error on a capable device stays a temporary fallback");

    auto readyFive = ReadySizing(5, output.width, output.height, 1707, 960);
    cache.PublishSizing(readyFive);
    const auto readyNow = cache.Peek({5, output.width, output.height});
    const auto active = failedPlanner.Begin(Input(Upscaler::Dlss, capable, readyNow ? &*readyNow : nullptr));
    Require(failedPlanner.ReportFailure({active.geometryEpoch, active.requestSignature, 720,
        gpu::frame_plan::FailureReason::DlssUnavailable}), "matching DLSS failure is latched");
    effect = ReadEffect(failedPlanner, cache);
    Require(effect.phase == DlssEffectPhase::TemporaryFallback && effect.failure &&
        *effect.failure == gpu::frame_plan::FailureReason::DlssUnavailable &&
        effect.consumer == TemporalConsumer::DlssSr, "a latched rejection is visible before the next CPU plan");
    const auto fallen = failedPlanner.Begin(Input(Upscaler::Dlss, capable, readyNow ? &*readyNow : nullptr));
    effect = ReadEffect(failedPlanner, cache);
    Require(!gpu::upscaling::IsDlssConsumer(fallen.consumer) && effect.phase == DlssEffectPhase::TemporaryFallback,
        "the next plan keeps the DLSS request in temporary fallback");

    PublishDeviceCapability(Device(Backend::Vulkan, 5, false, false));
    effect = ReadEffect(failedPlanner, cache);
    Require(effect.phase != DlssEffectPhase::Active && effect.phase != DlssEffectPhase::DeviceUnavailable,
        "cleanup clears readiness without calling the device permanently unavailable");
}

void CheckPlanObservationConsistency()
{
    const auto device = Device(Backend::Vulkan, 9, true, true);
    const auto ready = ReadySizing(9, 2560, 1440, 1707, 960);
    PlannerState planner;
    std::atomic<bool> failed{false};
    std::atomic<int> observations{0};
    std::thread mutator([&] {
        for (int i = 0; i < 20000; ++i) {
            if ((i & 1) == 0) planner.Begin(Input(Upscaler::Dlss, device, &ready));
            else planner.Begin(Input(Upscaler::Off, device, nullptr));
            if ((i % 64) == 0) {
                const auto observed = planner.Observe();
                if (observed.hasPlan)
                    planner.ReportFailure({observed.plan.geometryEpoch, observed.plan.requestSignature, 720,
                        gpu::frame_plan::FailureReason::InvalidInput});
            }
        }
    });
    std::thread observer([&] {
        for (int i = 0; i < 20000; ++i) {
            const auto observed = planner.Observe();
            if (!observed.hasPlan) continue;
            observations.fetch_add(1, std::memory_order_relaxed);
            const auto& plan = observed.plan;
            const bool dlss = plan.consumer == TemporalConsumer::DlssSr && plan.requestedUpscaler == Upscaler::Dlss &&
                plan.width == 1707 && plan.height == 960 && plan.dlssQuality == DlssQuality::Quality;
            const bool off = !gpu::upscaling::IsDlssConsumer(plan.consumer) && plan.requestedUpscaler == Upscaler::Off &&
                plan.width == 2560 && plan.height == 1440;
            const bool rejected = !gpu::upscaling::IsDlssConsumer(plan.consumer) && plan.requestedUpscaler == Upscaler::Dlss &&
                plan.width == 2560 && plan.height == 1440 && plan.dlssQuality == DlssQuality::Quality;
            if (!dlss && !off && !rejected) failed.store(true, std::memory_order_relaxed);
        }
    });
    mutator.join();
    observer.join();
    Require(observations.load() > 0 && !failed.load(), "observed plans keep consumer, request, and size from the same publication");
}

void CheckPublicationSource()
{
    const auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const auto video = ReadFile(root / "LostOdysseyRecomp" / "gpu" / "video.cpp");
    const auto reader = FunctionBody(video, "upscaling::BackendDeviceSnapshot BackendDeviceState()");
    Require(!reader.empty() && reader.find("PublishedDeviceCapability") != std::string::npos, "CPU state reads the published snapshot");
    Require(reader.find("Report") == std::string::npos && reader.find("g_dlssController") == std::string::npos &&
        reader.find("g_device") == std::string::npos && reader.find("g_available") == std::string::npos,
        "CPU state does not read the NGX report or device pointers");
    const auto owner = FunctionBody(video, "void PublishOwnedDeviceCapability()");
    Require(owner.find("Report()") != std::string::npos && owner.find("PublishDeviceCapability") != std::string::npos,
        "only the device owner copies the NGX report into the snapshot");
    const auto sizing = FunctionBody(video, "static void ServicePendingDlssSizing()");
    const auto query = sizing.find("QueryOutputSizing");
    const auto republish = sizing.find("PublishOwnedDeviceCapability");
    Require(query != std::string::npos && republish != std::string::npos && query < republish,
        "first-enable publication happens after the in-flight NGX sizing write");
    const auto reset = FunctionBody(video, "static void ResetGpu()");
    const auto clear = reset.find("PublishClearedDeviceCapability");
    const auto destroy = reset.find("g_dlssController.reset");
    Require(clear != std::string::npos && destroy != std::string::npos && clear < destroy,
        "cleanup publishes a not-ready snapshot before the controller is destroyed");
    const auto init = FunctionBody(video, "bool Init()");
    const auto ready = init.find("g_available = true");
    const auto initPublish = init.find("PublishOwnedDeviceCapability");
    Require(ready != std::string::npos && initPublish != std::string::npos && ready < initPublish,
        "successful initialization publishes after the device is marked ready");

    const auto plan = ReadFile(root / "LostOdysseyRecomp" / "gpu" / "frame_plan.cpp");
    const auto effect = FunctionBody(plan, "DlssEffectSnapshot CurrentDlssEffect()");
    Require(effect.find("PublishedDeviceCapability") != std::string::npos && effect.find("Observe") != std::string::npos &&
        effect.find("Peek") != std::string::npos && effect.find("DescribeDlssRuntime") != std::string::npos &&
        effect.find("Report") == std::string::npos && effect.find("g_dlssController") == std::string::npos,
        "UI effect reads published device, plan, and sizing state only");
    const auto begin = FunctionBody(plan, "void BeginCpuFrame()");
    Require(begin.find("BackendDeviceState") != std::string::npos && begin.find("Report") == std::string::npos,
        "CPU frame planning consumes the device snapshot");
}
}

int main()
{
    CheckDefaultAndMenu();
    CheckSnapshotConsistency();
    CheckEnableCycle();
    CheckFallbackStates();
    CheckPlanObservationConsistency();
    CheckPublicationSource();
    std::printf("PASS: %d DLSS capability snapshot checks (no NGX image, shader, packaging, or game launch)\n", checks);
    return 0;
}
