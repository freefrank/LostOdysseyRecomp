#include <stdafx.h>
#include "gpu/frame_plan.h"
#include "gpu/dlss_status_log.h"
#include "gpu/video.h"
#include <string_view>
#include "settings/config.h"
#include "kernel/memory.h"
#include "cpu/ppc_context.h"

extern "C" PPC_FUNC(__imp__sub_82290AB8);
extern "C" PPC_FUNC(__imp__sub_827B6E48);
extern "C" PPC_FUNC(__imp__sub_827BA640);
extern "C" PPC_FUNC(__imp__sub_823BC138);

namespace gpu::frame_plan
{
    bool wire::CatalogStage::Write(uint32_t index, uint32_t value, SurfaceRole& committedRole, uint32_t& committedSurface, uint32_t& committedColor)
    {
        if (index == CatalogBase) { active = value == CatalogMagic; role = SurfaceRole::Unknown; surfaceInfo = colorInfo = 0; return false; }
        if (!active || index < CatalogBase || index > CatalogBase + 4) return false;
        if (index == CatalogBase + 1) role = SurfaceRole(value);
        else if (index == CatalogBase + 2) surfaceInfo = value;
        else if (index == CatalogBase + 3) colorInfo = value;
        else if (index == CatalogBase + 4) {
            active = false;
            if (value == CatalogMagic && surfaceInfo) { committedRole = role; committedSurface = surfaceInfo; committedColor = colorInfo; return true; }
        }
        return false;
    }

    namespace
    {
        std::atomic<uint64_t> drawable{ (uint64_t(1280) << 32) | 720 };
        PlannerState planner;
        struct StatusIdentity {
            upscaling::Upscaler upscaler = upscaling::Upscaler::Off;
            upscaling::DlssQuality quality = upscaling::DlssQuality::Quality;
            upscaling::TemporalConsumer consumer = upscaling::TemporalConsumer::None;
            uint32_t width = 0, height = 0, outputWidth = 0, outputHeight = 0;
            uint64_t requestSignature = 0, geometryEpoch = 0, deviceEpoch = 0, sizingRevision = 0;
            bool inputProbe = false;
            bool operator==(const StatusIdentity&) const = default;
        };
        StatusIdentity loggedStatusPlan{};
        StatusIdentity StatusIdentityOf(const FramePlan& plan)
        {
            return {plan.requestedUpscaler, plan.dlssQuality, plan.consumer, plan.width, plan.height,
                plan.output.width, plan.output.height, plan.requestSignature, plan.geometryEpoch,
                plan.deviceEpoch, plan.sizingRevision, plan.inputProbe};
        }
        std::atomic<uint64_t> failedEpoch{~0ull};
        std::atomic<uint32_t> failedFallbackHeight{720};
        upscaling::SizingCache sizingCache;
        thread_local FramePlan cpuPlan{};
        thread_local std::optional<FramePlan> renderPlan;
        CommandTags tags;
        std::mutex deviceMutex;
        uint32_t readyDevice = 0;
        uint64_t streamGeneration = 0;
        uint64_t emittedSerial = 0, emittedEpoch = 0, emittedSignature = 0, emittedSizingRevision = 0, emittedGeneration = 0;
        uint32_t LoadWord(uint32_t address)
        {
            return g_memory.base && address >= 0x10000
                ? __builtin_bswap32(*reinterpret_cast<const uint32_t*>(g_memory.base + address)) : 0;
        }
    }

    void PublishDrawable(uint32_t width, uint32_t height)
    {
        if (width && height) drawable.store((uint64_t(width) << 32) | height, std::memory_order_release);
    }
    void BeginCpuFrame()
    {
        const uint64_t extent = drawable.load(std::memory_order_acquire);
        const auto config = settings::GetConfig();
        const auto output = upscaling::ResolveOutputRegion({uint32_t(extent >> 32), uint32_t(extent)});
        const auto device = video::BackendDeviceState();
        std::optional<upscaling::OutputSizing> sizing;
        if ((config.upscaler == upscaling::Upscaler::Dlss || config.upscaler == upscaling::Upscaler::Fsr) && device.deviceReady && device.backend == backend::Backend::Vulkan)
            sizing = sizingCache.LookupOrRequestSizing({device.deviceEpoch, output.width, output.height,
                config.upscaler, output.x, output.y});
        cpuPlan = planner.Begin({uint32_t(config.internalResolution), config.antialiasing, config.scalingQuality,
            config.upscaler, config.dlssQuality, output, device, sizing ? &*sizing : nullptr,
            getenv("LO_RESOLVE_READBACK") != nullptr, getenv("LO_DLSS_INPUT_PROBE") && std::string_view(getenv("LO_DLSS_INPUT_PROBE")) == "1",
            config.fsrQuality});
        const auto identity = StatusIdentityOf(cpuPlan);
        const bool planChanged = identity != loggedStatusPlan;
        loggedStatusPlan = identity;
        if (planChanged)
            NoteDlssRuntime(CurrentDlssEffect());
    }
    FramePlan CpuPlan() { return cpuPlan; }
    DlssEffectSnapshot CurrentDlssEffect()
    {
        const auto device = upscaling::PublishedDeviceCapability();
        const auto observed = planner.Observe();
        std::optional<upscaling::OutputSizing> sizing;
        if (observed.hasPlan && observed.plan.requestedUpscaler == upscaling::Upscaler::Dlss &&
            observed.plan.output.width && observed.plan.output.height)
            sizing = sizingCache.Peek({device.deviceEpoch, observed.plan.output.width, observed.plan.output.height,
                upscaling::Upscaler::Dlss, observed.plan.output.x, observed.plan.output.y});
        return DescribeDlssRuntime(device, observed, sizing ? &*sizing : nullptr);
    }
    std::optional<FramePlan> CurrentProducerPlan()
    {
        if (renderPlan && renderPlan->cpuSerial) return renderPlan;
        if (cpuPlan.cpuSerial) return cpuPlan;
        return std::nullopt;
    }
    void ReportFailedEpoch(uint64_t geometryEpoch, uint32_t fallbackHeight)
    {
        failedFallbackHeight.store(std::max(720u, fallbackHeight), std::memory_order_release);
        failedEpoch.store(geometryEpoch, std::memory_order_release);
    }
    void ReportPlanFailure(const PlanFailure& failure)
    {
        planner.ReportFailure(failure);
    }
    std::optional<UpscalerExecutionObservation> CurrentUpscalerExecution()
    {
        const auto device = upscaling::PublishedDeviceCapability();
        const auto observed = planner.Observe();
        if (!observed.hasPlan || !observed.execution || !device.Available(observed.plan.requestedUpscaler) ||
            observed.plan.deviceEpoch != device.deviceEpoch ||
            observed.execution->plan.requestSignature != observed.plan.requestSignature ||
            observed.execution->plan.geometryEpoch != observed.plan.geometryEpoch) return {};
        return observed.execution;
    }
    void ReportUpscalerExecution(const UpscalerExecutionObservation& observation)
    {
        if (observation.actualProvider == upscaling::Upscaler::Dlss) { ReportDlssExecution(observation); return; }
        if (!planner.ReportUpscalerExecution(observation) || !ExecutionLogChanged(observation)) return;
        LOG_INFO("FSR: frame={} outcome={} input={}x{} output={}x{} quality={} serial={} reason={}",
            observation.renderFrame, observation.outcome == DlssExecutionOutcome::Submitted ? "submitted" : "fallback",
            observation.plan.width, observation.plan.height, observation.plan.output.width, observation.plan.output.height,
            uint32_t(observation.plan.fsrQuality), observation.submissionSerial, DlssEffectReasonName(observation.reason));
    }
    void ReportDlssExecution(const DlssExecutionObservation& observation)
    {
        if (!planner.ReportExecution(observation)) return;
        if (!ExecutionLogChanged(observation)) return;
        NoteDlssRuntime(CurrentDlssEffect());
    }
    void NoteCurrentDlssStatus()
    {
        NoteDlssRuntime(CurrentDlssEffect());
    }
    std::optional<upscaling::SizingKey> TakeSizingRequest() { return sizingCache.TakeSizingRequest(); }
    void PublishSizing(upscaling::OutputSizing sizing) { sizingCache.PublishSizing(std::move(sizing)); }
    void ResetSizing(uint64_t deviceEpoch) { sizingCache.ResetSizing(deviceEpoch); }
    void TagReservedCommand(uint32_t ring, uint32_t commandAddress)
    {
        const auto plan = CurrentProducerPlan();
        if (ring == 0x8336A7A4 && commandAddress && plan) tags.Store(commandAddress, *plan);
    }
    bool BeginRenderCommand(uint32_t commandAddress)
    {
        renderPlan = tags.Take(commandAddress);
        return renderPlan.has_value();
    }
    void BeginTaggedRenderCommand(uint32_t commandAddress, uint32_t currentStack)
    {
        if (BeginRenderCommand(commandAddress)) gpu::frame_plan::EnsureCurrentPlanQueued(LoadWord(0x83302A38), currentStack);
    }
    void EndRenderCommand() { renderPlan.reset(); }
    std::optional<FramePlan> RenderPlan() { return renderPlan; }
    void DeviceStreamReady(uint32_t device)
    {
        if (!device || !LoadWord(device + 0x30) || !LoadWord(device + 0x34)) return;
        std::lock_guard lock(deviceMutex);
        readyDevice = device;
        ++streamGeneration;
        emittedGeneration = 0;
    }
    void DeviceStreamReadyFromContext(uint32_t device, const PPCContext& context)
    {
        (void)context;
        DeviceStreamReady(device);
    }
    void DeviceStreamDestroyed()
    {
        std::lock_guard lock(deviceMutex);
        readyDevice = 0;
        ++streamGeneration;
    }
    void EnsureCurrentPlanQueued(uint32_t device, uint32_t currentStack)
    {
        const auto plan = CurrentProducerPlan();
        PPCContext* current = GetPPCContext();
        if (!plan || !current) return;
        PPCContext context = *current;
        if (currentStack) context.r1.u32 = currentStack;
        QueuePlanOnDevice(context, device, *plan);
    }
    bool QueuePlanOnDevice(PPCContext& context, uint32_t device, const FramePlan& plan)
    {
        { std::lock_guard lock(deviceMutex);
            if (device != readyDevice || !LoadWord(device + 0x30) || !LoadWord(device + 0x34) ||
                (emittedGeneration == streamGeneration && emittedSerial == plan.cpuSerial && emittedEpoch == plan.geometryEpoch &&
                 emittedSignature == plan.requestSignature && emittedSizingRevision == plan.sizingRevision)) return false; }
        const auto words = wire::EncodePlan(plan);
        if (!gpu::frame_plan::EmitPrivatePacket(device, wire::PlanBase, words)) return false;
        // The display target exists before the world target table is populated;
        // publish it alongside the plan so pre-world Canvas/movie draws cannot
        // allocate a stale native mapping.
        QueueMainDisplayCatalog(context, device);
        { std::lock_guard lock(deviceMutex); emittedSerial = plan.cpuSerial; emittedEpoch = plan.geometryEpoch;
            emittedSignature = plan.requestSignature; emittedSizingRevision = plan.sizingRevision; emittedGeneration = streamGeneration; }
        return true;
    }
    bool QueueCatalogOnDevice(PPCContext& context, uint32_t device, SurfaceRole role, uint32_t surfaceInfo, uint32_t colorInfo)
    {
        { std::lock_guard lock(deviceMutex); if (device != readyDevice || !surfaceInfo) return false; }
        const uint32_t words[] = { wire::CatalogMagic, uint32_t(role), surfaceInfo, colorInfo, wire::CatalogMagic };
        return gpu::frame_plan::EmitPrivatePacket(device, wire::CatalogBase, words);
    }
    bool QueueMainDisplayCatalog(PPCContext& context, uint32_t device)
    {
        const uint32_t surface = LoadWord(0x83302A34);
        if (!surface || !LoadWord(surface + 0x18)) return false;
        return QueueCatalogOnDevice(context, device, SurfaceRole::Scene, LoadWord(surface + 0x18), LoadWord(surface + 0x1C));
    }

    bool EmitPrivatePacket(uint32_t device, uint32_t registerBase, std::span<const uint32_t> words)
    {
        if (!device || words.empty() || words.size() > 64) return false;
        PPCContext* current = GetPPCContext();
        if (!current) return false;
        thread_local uint32_t scratch = 0;
        if (!scratch) scratch = g_pageAllocator.Alloc(g_pageAllocator.virtualRegion, 0x1000, 0x1000);
        if (!scratch || !g_memory.base) return false;
        for (uint32_t i = 0; i < words.size(); ++i)
            *reinterpret_cast<uint32_t*>(g_memory.base + scratch + i * 4) = __builtin_bswap32(words[i]);
        PPCContext packet = *current;
        packet.r1.u32 = scratch + 0x800;
        // PPC stack frames link to their caller at 0(r1); retain the live
        // frame's backlink while the SDK writer uses our private scratch frame.
        *reinterpret_cast<uint32_t*>(g_memory.base + packet.r1.u32) = __builtin_bswap32(current->r1.u32);
        packet.r3.u32 = device;
        packet.r4.u64 = ~0ull << (64 - words.size());
        packet.r5.u32 = registerBase;
        packet.r6.u32 = scratch;
        sub_823C1BD8(packet, g_memory.base);
        return true;
    }

}

PPC_FUNC(sub_82290AB8)
{
    const uint32_t ring = ctx.r4.u32;
    __imp__sub_82290AB8(ctx, base);
    const uint32_t result = ctx.r3.u32;
    const uint32_t address = result && g_memory.base ? __builtin_bswap32(*reinterpret_cast<uint32_t*>(g_memory.base + result + 4)) : 0;
    gpu::frame_plan::TagReservedCommand(ring, address);
}
PPC_FUNC(sub_827B6E48)
{
    const uint32_t device = ctx.r3.u32, parameter = ctx.r4.u32;
    gpu::frame_plan::DeviceStreamDestroyed();
    __imp__sub_827B6E48(ctx, base);
    if (parameter) gpu::frame_plan::DeviceStreamReadyFromContext(device, ctx);
}
PPC_FUNC(sub_827BA640)
{
    __imp__sub_827BA640(ctx, base);
    gpu::frame_plan::DeviceStreamDestroyed();
}
PPC_FUNC(sub_823BC138)
{
    __imp__sub_823BC138(ctx, base);
    constexpr uint32_t table = 0x8336AD50;
    const uint32_t device = g_memory.base ? __builtin_bswap32(*reinterpret_cast<uint32_t*>(g_memory.base + 0x83302A38)) : 0;
    if (!device || !gpu::frame_plan::CurrentProducerPlan() ||
        __builtin_bswap32(*reinterpret_cast<uint32_t*>(g_memory.base + table + 0x1C8)) != device) return;
    struct Slot { uint32_t offset; gpu::frame_plan::SurfaceRole role; };
    constexpr Slot slots[] = {{0x3C, gpu::frame_plan::SurfaceRole::Scene}, {0x60, gpu::frame_plan::SurfaceRole::Scene},
        {0x84, gpu::frame_plan::SurfaceRole::Scene}, {0xA8, gpu::frame_plan::SurfaceRole::Scene},
        {0xCC, gpu::frame_plan::SurfaceRole::Fixed}, {0xF0, gpu::frame_plan::SurfaceRole::Fixed},
        {0x138, gpu::frame_plan::SurfaceRole::Scene}, {0x15C, gpu::frame_plan::SurfaceRole::Scene},
        {0x180, gpu::frame_plan::SurfaceRole::Scene}, {0x1A4, gpu::frame_plan::SurfaceRole::Scene}};
    struct CatalogKey { uint32_t color, surface; bool operator==(const CatalogKey&) const = default; };
    struct CatalogKeyHash { size_t operator()(const CatalogKey& key) const { return (size_t(key.color) << 16) ^ key.surface; } };
    struct Roles { bool scene = false, fixed = false; };
    static std::unordered_map<CatalogKey, gpu::frame_plan::SurfaceRole, CatalogKeyHash> previous;
    std::unordered_map<CatalogKey, Roles, CatalogKeyHash> current;
    const auto add = [&](gpu::frame_plan::SurfaceRole role, uint32_t surfaceInfo, uint32_t colorInfo) {
        const CatalogKey key{colorInfo & 0xFFF, surfaceInfo & 0x3FFF};
        if (!key.surface) return;
        Roles& roles = current[key];
        roles.scene |= role == gpu::frame_plan::SurfaceRole::Scene;
        roles.fixed |= role == gpu::frame_plan::SurfaceRole::Fixed;
    };
    for (size_t index = 0; index < std::size(slots); ++index) {
        const auto slot = slots[index];
        const uint32_t surface = __builtin_bswap32(*reinterpret_cast<uint32_t*>(g_memory.base + table + slot.offset));
        if (surface) {
            const auto surfaceInfo =
                __builtin_bswap32(*reinterpret_cast<uint32_t*>(g_memory.base + surface + 0x18)),
                colorInfo = __builtin_bswap32(*reinterpret_cast<uint32_t*>(g_memory.base + surface + 0x1C));
            add(slot.role, surfaceInfo, colorInfo);
        }
    }
    const uint32_t mainSurface = __builtin_bswap32(*reinterpret_cast<uint32_t*>(g_memory.base + 0x83302A34));
    if (mainSurface) add(gpu::frame_plan::SurfaceRole::Scene,
        __builtin_bswap32(*reinterpret_cast<uint32_t*>(g_memory.base + mainSurface + 0x18)),
        __builtin_bswap32(*reinterpret_cast<uint32_t*>(g_memory.base + mainSurface + 0x1C)));
    for (auto it = previous.begin(); it != previous.end();) {
        if (current.contains(it->first)) { ++it; continue; }
        if (gpu::frame_plan::QueueCatalogOnDevice(ctx, device, gpu::frame_plan::SurfaceRole::Unknown, it->first.surface, it->first.color))
            it = previous.erase(it);
        else
            ++it;
    }
    for (const auto& [key, roles] : current) {
        const auto old = previous.find(key);
        // A shared alias with two incompatible roles retains its prior mapping;
        // a new ambiguous key is deliberately left uncatalogued.
        if (roles.scene && roles.fixed && old == previous.end()) continue;
        const auto role = roles.scene && roles.fixed ? old->second : roles.scene ? gpu::frame_plan::SurfaceRole::Scene : gpu::frame_plan::SurfaceRole::Fixed;
        if (old == previous.end() || old->second != role) {
            if (gpu::frame_plan::QueueCatalogOnDevice(ctx, device, role, key.surface, key.color)) previous[key] = role;
        }
    }
}

// Mid-assembly queue boundaries preserve the producer's tag lifetime. The
// stream helper receives a copied live PPC context, while r1 refreshes its
// stack anchor at the execution site before a rollover-capable PM4 append.
void FramePlanBeforeExecute(PPCRegister& r1, PPCRegister& r31)
{
    gpu::frame_plan::BeginTaggedRenderCommand(r31.u32, r1.u32);
}
void FramePlanAfterExecute()
{
    gpu::frame_plan::EndRenderCommand();
}
void FramePlanDirectCanvas(PPCRegister& r1, PPCRegister& device)
{
    gpu::frame_plan::EnsureCurrentPlanQueued(device.u32, r1.u32);
}
