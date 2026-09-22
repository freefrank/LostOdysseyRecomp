#include "upscaling_plan.h"
#include "dlss_ngx.h"

namespace gpu::upscaling {
namespace {
std::mutex g_deviceCapabilityMutex;
BackendDeviceSnapshot g_deviceCapability{};

OutputSizing PendingSizing(const SizingKey& key) {
    OutputSizing sizing;
    sizing.key = key;
    for (auto& mode : sizing.modes) mode.state = SizingState::Pending;
    return sizing;
}
}

OutputSizing SizingCache::LookupOrRequestSizing(const SizingKey& key) {
    std::lock_guard lock(mutex_);
    if (key.deviceEpoch < deviceEpoch_) {
        auto stale = PendingSizing(key);
        for (auto& mode : stale.modes) mode.state = SizingState::Error;
        return stale;
    }
    if (key.deviceEpoch > deviceEpoch_) {
        deviceEpoch_ = key.deviceEpoch;
        entries_ = {};
        pending_.reset();
        inFlight_.reset();
    }
    for (const auto& entry : entries_) {
        if (!entry || entry->key != key) continue;
        if (entry->modes[0].state == SizingState::Pending && (!inFlight_ || *inFlight_ != key)) pending_ = key;
        return *entry;
    }
    auto pending = PendingSizing(key);
    for (auto& entry : entries_)
        if (!entry) { entry = pending; break; }
    if (!inFlight_ || *inFlight_ != key) pending_ = key;
    return pending;
}

std::optional<SizingKey> SizingCache::TakeSizingRequest() {
    std::lock_guard lock(mutex_);
    auto request = pending_;
    pending_.reset();
    inFlight_ = request;
    return request;
}

void SizingCache::PublishSizing(OutputSizing sizing) {
    std::lock_guard lock(mutex_);
    if (sizing.key.deviceEpoch != deviceEpoch_) return;
    sizing.revision = ++revision_;
    for (auto& entry : entries_)
        if (entry && entry->key == sizing.key) { entry = std::move(sizing); if (inFlight_ && *inFlight_ == entry->key) inFlight_.reset(); return; }
    for (auto& entry : entries_)
        if (!entry) { entry = std::move(sizing); if (inFlight_ && *inFlight_ == entry->key) inFlight_.reset(); return; }
    entries_[0] = std::move(sizing);
    if (inFlight_ && *inFlight_ == entries_[0]->key) inFlight_.reset();
}

void SizingCache::ResetSizing(uint64_t deviceEpoch) {
    std::lock_guard lock(mutex_);
    deviceEpoch_ = deviceEpoch;
    entries_ = {};
    pending_.reset();
    inFlight_.reset();
}

std::optional<OutputSizing> SizingCache::Peek(const SizingKey& key) {
    std::lock_guard lock(mutex_);
    if (key.deviceEpoch != deviceEpoch_) return std::nullopt;
    for (const auto& entry : entries_)
        if (entry && entry->key == key) return *entry;
    return std::nullopt;
}

void PublishDeviceCapability(BackendDeviceSnapshot snapshot) {
    std::lock_guard lock(g_deviceCapabilityMutex);
    g_deviceCapability = snapshot;
}

BackendDeviceSnapshot PublishedDeviceCapability() {
    std::lock_guard lock(g_deviceCapabilityMutex);
    return g_deviceCapability;
}

#if defined(LO_GPU_PLUME)
OutputSizing SizingService::QueryOutputSizing(dlss::Controller& controller,
    const plume::VulkanInterface& vulkanInterface, const plume::VulkanDevice& device, const SizingKey& key) {
    return controller.QueryOutputSizing(vulkanInterface, device, key);
}
#endif
} // namespace gpu::upscaling
