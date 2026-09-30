// Production status-log format. Real DescribeDlssRuntime plus NoteDlssRuntime.
// No NGX image, GPU submit, or game launch.
#include "gpu/dlss_status_log.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
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
using gpu::frame_plan::DescribeDlssRuntime;
using gpu::frame_plan::DlssEffectPhase;
using gpu::frame_plan::DlssEffectReason;
using gpu::frame_plan::DlssExecutionObservation;
using gpu::frame_plan::DlssExecutionOutcome;
using gpu::frame_plan::EmitSparseRepeat;
using gpu::frame_plan::ExecutionLogChanged;
using gpu::frame_plan::NoteDlssRuntime;
using gpu::frame_plan::PlannerInput;
using gpu::frame_plan::PlannerState;
using gpu::frame_plan::ResetDlssStatusLog;
using gpu::upscaling::BackendDeviceSnapshot;
using gpu::upscaling::DlssQuality;
using gpu::upscaling::OutputSizing;
using gpu::upscaling::SizingState;
using gpu::upscaling::Upscaler;

struct LogFile {
    FILE* file = nullptr;
    explicit LogFile(const std::filesystem::path& path)
    {
        file = std::fopen(path.string().c_str(), "wb+");
        Require(file != nullptr, "open status log");
        os::logger::g_file = file;
    }
    ~LogFile()
    {
        if (os::logger::g_file == file) os::logger::g_file = nullptr;
        if (file) std::fclose(file);
    }
    std::string Text()
    {
        std::fflush(file);
        const long pos = std::ftell(file);
        Require(pos >= 0, "log position");
        std::fseek(file, 0, SEEK_SET);
        std::string text(static_cast<size_t>(pos), '\0');
        if (pos) Require(std::fread(text.data(), 1, static_cast<size_t>(pos), file) == static_cast<size_t>(pos), "read status log");
        std::fseek(file, 0, SEEK_END);
        return text;
    }
};

int Count(const std::string& text, const std::string& needle)
{
    int count = 0;
    for (size_t at = 0; (at = text.find(needle, at)) != std::string::npos; at += needle.size()) ++count;
    return count;
}
bool Has(const std::string& text, const std::string& needle) { return text.find(needle) != std::string::npos; }
std::string Between(const std::string& text, const std::string& marker)
{
    const auto at = text.rfind(marker);
    if (at == std::string::npos) return {};
    const auto end = text.find('\n', at);
    return text.substr(at, end == std::string::npos ? std::string::npos : end - at);
}

BackendDeviceSnapshot Device(uint64_t epoch)
{
    BackendDeviceSnapshot device;
    device.backend = Backend::Vulkan;
    device.deviceEpoch = epoch;
    device.deviceReady = true;
    device.dlssAvailable = true;
    return device;
}
OutputSizing Ready(uint64_t epoch)
{
    OutputSizing sizing;
    sizing.key = {epoch, 2560, 1440};
    sizing.revision = 1;
    for (auto& mode : sizing.modes) {
        mode.state = SizingState::Ready;
        mode.optimal = mode.minimum = mode.maximum = {1707, 960};
    }
    sizing.modes[1].optimal = sizing.modes[1].minimum = sizing.modes[1].maximum = {1483, 835};
    sizing.modes[2].optimal = sizing.modes[2].minimum = sizing.modes[2].maximum = {1280, 720};
    sizing.modes[3].optimal = sizing.modes[3].minimum = sizing.modes[3].maximum = {2560, 1440};
    return sizing;
}
PlannerInput Input(const BackendDeviceSnapshot& device, const OutputSizing* sizing, DlssQuality quality, Upscaler upscaler = Upscaler::Dlss)
{
    PlannerInput input;
    input.upscaler = upscaler;
    input.quality = quality;
    input.output = {{2560, 1440}, 0, 0, 2560, 1440};
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
void RequireTokens(const std::string& line, std::initializer_list<const char*> tokens, const char* what)
{
    for (const char* token : tokens) Require(Has(line, token), what);
}

std::string ReadSource(const std::filesystem::path& path)
{
    std::ifstream input(path, std::ios::binary);
    Require(input.good(), "source missing");
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

void CheckSavedLines(LogFile& log)
{
    auto saved = [](Upscaler upscaler, DlssQuality quality, uint32_t frameRate) {
        settings::Config config;
        config.width = 2560;
        config.height = 1440;
        config.graphicsBackend = settings::GraphicsBackend::Vulkan;
        config.antialiasing = 0;
        config.frameRate = frameRate;
        config.upscaler = upscaler;
        config.dlssQuality = quality;
        settings::LogSettingsSaved(config);
    };
    saved(Upscaler::Dlss, DlssQuality::Quality, 60);
    saved(Upscaler::Dlss, DlssQuality::Balanced, 30);
    saved(Upscaler::Dlss, DlssQuality::Performance, 120);
    saved(Upscaler::Off, DlssQuality::Quality, 60);
    saved(Upscaler::Dlss, DlssQuality::Dlaa, 60);
    const auto text = log.Text();
    Require(Count(text, "settings saved:") == 5, "five successful save lines");
    Require(Has(text, "settings saved: 2560x1440 internal_resolution=0 mode=0 backend=Vulkan(1) AA=0 frame_rate=60 upscaler=Dlss(1) dlss_quality=Quality(0) language=1"),
        "quality save keeps size, backend, AA and names the new fields");
    Require(Has(text, "frame_rate=30 upscaler=Dlss(1) dlss_quality=Balanced(1)"), "balanced save");
    Require(Has(text, "frame_rate=120 upscaler=Dlss(1) dlss_quality=Performance(2)"), "performance save");
    Require(Has(text, "frame_rate=60 upscaler=Off(0) dlss_quality=Quality(0)"), "off save");
    Require(Has(text, "frame_rate=60 upscaler=Dlss(1) dlss_quality=Dlaa(3)"), "DLAA save is not Off");
}

void CheckRuntime(LogFile& log, std::vector<std::string>& examples)
{
    ResetDlssStatusLog();
    const auto before = Count(log.Text(), "dlss status=");
    const auto device = Device(7);
    auto sizing = Ready(7);
    PlannerState planner;
    const auto quality = planner.Begin(Input(device, &sizing, DlssQuality::Quality));
    auto awaiting = DescribeDlssRuntime(device, planner.Observe(), &sizing);
    Require(awaiting.phase == DlssEffectPhase::AwaitingExecution && awaiting.inputWidth == 1707 && awaiting.outputWidth == 2560,
        "quality plan is waiting");
    NoteDlssRuntime(awaiting);
    NoteDlssRuntime(awaiting);
    auto text = log.Text();
    Require(Count(text, "dlss status=") == before + 1, "the same awaiting snapshot logs once");
    auto awaitingLine = Between(text, "dlss status=");
    RequireTokens(awaitingLine, {"status=AwaitingExecution", "requested=Dlss(1)", "quality=Quality(0)", "input=1707x960",
        "output=2560x1440", "reason=AwaitingGpuFrame", "sizing=Ready(1)", "executed_quality=none", "render_frame=none",
        "device_epoch=7"}, "awaiting fields");
    Require(Has(awaitingLine, fmt::format("cpu_serial={}", awaiting.cpuSerial)) &&
        Has(awaitingLine, fmt::format("geometry_epoch={}", awaiting.geometryEpoch)) &&
        Has(awaitingLine, fmt::format("request_signature={:#x}", awaiting.requestSignature)), "awaiting identity");
    examples.push_back(awaitingLine);

    PlannerState off;
    off.Begin(Input(device, &sizing, DlssQuality::Quality, Upscaler::Off));
    NoteDlssRuntime(DescribeDlssRuntime(device, off.Observe(), &sizing));
    NoteDlssRuntime(DescribeDlssRuntime(device, off.Observe(), &sizing));
    text = log.Text();
    const auto offCount = Count(text, "status=Off");
    Require(offCount == 1, "turning upscaler off logs once");
    examples.push_back(Between(text, "dlss status=Off"));

    Require(planner.ReportExecution(Submitted(quality, 41, 9)), "quality submission stored");
    auto submitted = DescribeDlssRuntime(device, planner.Observe(), &sizing);
    Require(submitted.phase == DlssEffectPhase::Active && submitted.execution && submitted.execution->renderFrame == 41, "submitted classification");
    const auto beforeSubmit = Count(log.Text(), "status=Submitted");
    NoteDlssRuntime(submitted);
    const auto again = planner.Begin(Input(device, &sizing, DlssQuality::Quality));
    Require(again.geometryEpoch == quality.geometryEpoch && planner.ReportExecution(Submitted(again, 42, 10)), "same request accepts a newer frame");
    NoteDlssRuntime(DescribeDlssRuntime(device, planner.Observe(), &sizing));
    text = log.Text();
    Require(Count(text, "status=Submitted") == beforeSubmit + 1, "a newer render frame with the same request does not log again");
    auto submittedLine = Between(text, "dlss status=Submitted");
    RequireTokens(submittedLine, {"status=Submitted", "phase=Active", "executed_quality=Quality(0)", "executed_input=1707x960",
        "executed_output=2560x1440", "render_frame=41", "submission_serial=9", "submit=checked_not_gpu_complete"},
        "submitted means checked submit, not GPU completion");
    Require(!Has(submittedLine, "GPU finished") && !Has(submittedLine, "gpu_complete=1"), "submitted line does not claim GPU completion");
    examples.push_back(submittedLine);

    auto fallback = Submitted(quality, 43, 0);
    fallback.outcome = DlssExecutionOutcome::Fallback;
    fallback.reason = DlssEffectReason::NoEligibleScene;
    Require(planner.ReportExecution(fallback), "fallback stored");
    auto fallen = DescribeDlssRuntime(device, planner.Observe(), &sizing);
    Require(fallen.phase == DlssEffectPhase::TemporaryFallback && fallen.reason == DlssEffectReason::NoEligibleScene, "fallback classification");
    NoteDlssRuntime(fallen);
    NoteDlssRuntime(fallen);
    auto recovered = Submitted(quality, 44, 12);
    Require(planner.ReportExecution(recovered), "recovery stored");
    auto restored = DescribeDlssRuntime(device, planner.Observe(), &sizing);
    Require(restored.phase == DlssEffectPhase::Active && restored.execution && restored.execution->renderFrame == 44, "recovery classification");
    NoteDlssRuntime(restored);
    text = log.Text();
    Require(Has(text, "status=Fallback") && Has(text, "reason=NoEligibleScene") && Has(text, "render_frame=43"), "fallback is visible");
    Require(Has(text, "status=Submitted") && Has(text, "render_frame=44") && Has(text, "submission_serial=12"), "recovery is visible");
    examples.push_back(Between(text, "dlss status=Fallback"));

    planner.Begin(Input(device, &sizing, DlssQuality::Balanced));
    NoteDlssRuntime(DescribeDlssRuntime(device, planner.Observe(), &sizing));
    planner.Begin(Input(device, &sizing, DlssQuality::Performance));
    NoteDlssRuntime(DescribeDlssRuntime(device, planner.Observe(), &sizing));
    text = log.Text();
    Require(Has(text, "quality=Balanced(1)") && Has(text, "input=1483x835"), "balanced request");
    Require(Has(text, "quality=Performance(2)") && Has(text, "input=1280x720"), "performance request");

    PlannerState probe;
    auto probeInput = Input(device, &sizing, DlssQuality::Balanced);
    probeInput.inputProbeRequested = true;
    probe.Begin(probeInput);
    NoteDlssRuntime(DescribeDlssRuntime(device, probe.Observe(), &sizing));
    auto stoppedDevice = device;
    stoppedDevice.gpuWorkStopped = true;
    NoteDlssRuntime(DescribeDlssRuntime(stoppedDevice, planner.Observe(), &sizing));
    auto unavailable = device;
    unavailable.dlssAvailable = false;
    PlannerState bare;
    bare.Begin(Input(unavailable, &sizing, DlssQuality::Quality));
    NoteDlssRuntime(DescribeDlssRuntime(unavailable, bare.Observe(), &sizing));
    auto failedSizing = sizing;
    failedSizing.modes[0].state = SizingState::Error;
    PlannerState sizingPlanner;
    sizingPlanner.Begin(Input(device, &failedSizing, DlssQuality::Quality));
    NoteDlssRuntime(DescribeDlssRuntime(device, sizingPlanner.Observe(), &failedSizing));
    text = log.Text();
    Require(Has(text, "status=InputProbeOnly") && Has(text, "reason=InputProbeOnly"), "input probe");
    Require(Has(text, "status=GpuStopped") && Has(text, "reason=GpuWorkStopped"), "stopped GPU work");
    Require(Has(text, "status=DeviceUnavailable") && Has(text, "reason=CapabilityUnavailable"), "missing capability");
    Require(Has(text, "reason=SizingError") && Has(text, "sizing=Error(3)"), "sizing error");
}

void CheckJitter(LogFile& log)
{
    ResetDlssStatusLog();
    const auto device = Device(8);
    auto sizing = Ready(8);
    PlannerState planner;
    const auto plan = planner.Begin(Input(device, &sizing, DlssQuality::Quality));
    Require(planner.ReportExecution(Submitted(plan, 1, 1)), "jitter baseline");
    const auto submitted = DescribeDlssRuntime(device, planner.Observe(), &sizing);
    auto fallbackObs = Submitted(plan, 2, 0);
    fallbackObs.outcome = DlssExecutionOutcome::Fallback;
    fallbackObs.reason = DlssEffectReason::NoEligibleScene;
    Require(planner.ReportExecution(fallbackObs), "jitter fallback");
    const auto fallback = DescribeDlssRuntime(device, planner.Observe(), &sizing);
    ResetDlssStatusLog();
    const auto before = Count(log.Text(), "dlss status=");
    for (int i = 0; i < 40; ++i) NoteDlssRuntime(i % 2 ? fallback : submitted);
    NoteDlssRuntime(fallback);
    NoteDlssRuntime(fallback);
    const auto added = Count(log.Text(), "dlss status=") - before;
    Require(added > 0 && added <= 12, "alternating status stays bounded");
    Require(Has(log.Text(), "jitter_changes="), "jitter keeps a count");
    PlannerState switched;
    switched.Begin(Input(device, &sizing, DlssQuality::Balanced));
    NoteDlssRuntime(DescribeDlssRuntime(device, switched.Observe(), &sizing));
    Require(Has(log.Text(), "quality=Balanced(1)") && Has(log.Text(), "input=1483x835"),
        "a request switch during jitter is still written");
}

void CheckExecutionPrefilter()
{
    ResetDlssStatusLog();
    const auto device = Device(9);
    auto sizing = Ready(9);
    PlannerState planner;
    const auto plan = planner.Begin(Input(device, &sizing, DlssQuality::Quality));
    auto first = Submitted(plan, 4, 11);
    Require(ExecutionLogChanged(first), "first execution needs a status snapshot");
    first.renderFrame = 5;
    first.submissionSerial = 12;
    first.plan.cpuSerial += 1;
    Require(!ExecutionLogChanged(first), "render frame and serials are not part of the dedupe key");
    first.outcome = DlssExecutionOutcome::Fallback;
    first.reason = DlssEffectReason::NoEligibleScene;
    Require(ExecutionLogChanged(first), "fallback changes the execution stamp");
}

void CheckSparseRepeat()
{
    uint32_t repeats = 0;
    Require(EmitSparseRepeat(repeats, true) && repeats == 0, "a new reason logs immediately");
    int logged = 1;
    for (uint32_t i = 1; i <= 300; ++i) {
        const bool emit = EmitSparseRepeat(repeats, false);
        if (i == 64 || i == 256) Require(emit, "checkpoint logs the repeat count");
        else Require(!emit, "steady restores do not log");
        if (emit) ++logged;
    }
    Require(logged == 3 && repeats == 300, "three hundred repeats produce the first line plus two checkpoints");
    Require(EmitSparseRepeat(repeats, true) && repeats == 0, "a reason change logs again");
}

void CheckWiring()
{
    const auto root = std::filesystem::path(__FILE__).parent_path().parent_path().parent_path();
    const auto config = FunctionBody(ReadSource(root / "LostOdysseyRecomp" / "settings" / "config.cpp"), "static bool WriteConfig");
    const auto saved = config.find("LogSettingsSaved");
    const auto moved = config.find("MoveFileExW");
    Require(!config.empty() && saved != std::string::npos && moved != std::string::npos && moved < saved, "save log follows the replace");
    size_t at = 0;
    while ((at = config.find("return false", at)) != std::string::npos) {
        Require(at < saved, "failed save returns before the success log");
        ++at;
    }
    const auto plan = ReadSource(root / "LostOdysseyRecomp" / "gpu" / "frame_plan.cpp");
    const auto begin = FunctionBody(plan, "void BeginCpuFrame()");
    Require(begin.find("if (planChanged)") != std::string::npos && begin.find("NoteDlssRuntime(CurrentDlssEffect())") != std::string::npos &&
        begin.find("Report") == std::string::npos, "CPU planning logs only a changed plan and does not report execution");
    const auto report = FunctionBody(plan, "void ReportDlssExecution");
    const auto changed = report.find("ExecutionLogChanged");
    const auto noted = report.find("NoteDlssRuntime");
    Require(changed != std::string::npos && noted != std::string::npos && changed < noted, "execution log uses the semantic prefilter");
    const auto video = FunctionBody(ReadSource(root / "LostOdysseyRecomp" / "gpu" / "video.cpp"), "void PublishOwnedDeviceCapability()");
    const auto publish = video.find("PublishDeviceCapability");
    const auto deviceNote = video.find("NoteCurrentDlssStatus");
    Require(publish != std::string::npos && deviceNote != std::string::npos && publish < deviceNote &&
        video.find("previous != snapshot") != std::string::npos, "device publish logs only when the snapshot changes");
    const auto menu = ReadSource(root / "LostOdysseyRecomp" / "settings" / "menu.cpp");
    Require(menu.find("NoteDlssRuntime") == std::string::npos && menu.find("LogSettingsSaved") == std::string::npos,
        "status logging is not bound to the menu");
    const auto renderer = ReadSource(root / "LostOdysseyRecomp" / "gpu" / "renderer.cpp");
    Require(renderer.find("EmitSparseRepeat") != std::string::npos &&
        renderer.find("restored scene-copy destination frame={} reason={} repeats={}") != std::string::npos,
        "scene-copy restore log keeps a bounded repeat count");
}
}

int main()
{
    const auto directory = std::filesystem::temp_directory_path() / "lore-dlss-status-log";
    std::filesystem::create_directories(directory);
    LogFile log(directory / "status.log");
    std::vector<std::string> examples;
    CheckSavedLines(log);
    CheckRuntime(log, examples);
    CheckJitter(log);
    CheckExecutionPrefilter();
    CheckSparseRepeat();
    CheckWiring();
    for (const auto& line : examples) std::printf("EXAMPLE %s\n", line.c_str());
    std::printf("PASS: %d DLSS status log checks (logger file, classify/dedupe; no NGX image or GPU submit)\n", checks);
    return 0;
}
