#include "upscaling_plan.h"
#include <algorithm>
#include <chrono>
#include <limits>
#if defined(LO_GPU_PLUME)
#include "temporal_upscaler.h"
#endif

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

uint64_t SizingCache::SteadyMilliseconds() {
    return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
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
        entries_ = {}; retries_ = {};
        pending_.reset();
        inFlight_.reset();
    }
    for (size_t i=0; i<entries_.size(); ++i) {
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
    }
    auto pending = PendingSizing(key);
    for (auto& entry : entries_)
        if (!entry) { entry = pending; break; }
    if (!inFlight_ || *inFlight_ != key) pending_ = key;
    return pending;
}

std::optional<SizingKey> SizingCache::TakeSizingRequest() {
    std::lock_guard lock(mutex_);
    if (!pending_ || inFlight_) return std::nullopt;
    auto request = pending_;
    pending_.reset();
    inFlight_ = request;
    return request;
}

void SizingCache::PublishSizing(OutputSizing sizing) {
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

void SizingCache::ResetSizing(uint64_t deviceEpoch) {
    std::lock_guard lock(mutex_);
    deviceEpoch_ = deviceEpoch;
    entries_ = {}; retries_ = {};
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
OutputSizing SizingService::QueryOutputSizing(TemporalUpscaler& upscaler,
    const plume::VulkanInterface& vulkanInterface, const plume::VulkanDevice& device, const SizingKey& key) {
    return upscaler.QuerySizing(vulkanInterface, device, key);
}
#endif
} // namespace gpu::upscaling
