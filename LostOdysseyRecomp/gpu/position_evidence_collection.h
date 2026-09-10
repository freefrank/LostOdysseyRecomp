#pragma once
#include "collection_worker.h"
#include "shader_source_collection.h"
#include "shader/position_evidence.h"
#include <array>
#include <atomic>
#include <memory>
#include <mutex>
#include <span>

namespace gpu::position_evidence {

// Optional CPU diagnostics. The producer never waits for an analyzer or allocates
// storage. The detached worker owns its entire state and analyzer; it must not
// capture a renderer, device, logger, or any other application-lifetime object.
class Collection {
    struct Result {
        uint64_t hash = 0;
        Summary summary;
        bool occupied = false, ready = false;
    };
    struct State {
        std::atomic<bool> active{true};
        std::mutex mutex;
        taa_collection::shader_sources::Queue pending;
        std::array<Result, taa_collection::shader_sources::MaxTrackedPrograms> results{};
        State() { pending.Initialize(); }
        size_t Find(uint64_t hash) const noexcept {
            size_t slot = static_cast<size_t>(hash ^ (hash >> 32)) % results.size();
            for (size_t i = 0; i < results.size(); ++i) {
                if (!results[slot].occupied || results[slot].hash == hash) return slot;
                slot = (slot + 1) % results.size();
            }
            return results.size();
        }
    };
    std::shared_ptr<State> state_;
    std::unique_ptr<taa_collection::CollectionWorker> worker_;
public:
    template<class Analyzer> explicit Collection(Analyzer analyzer) : state_(std::make_shared<State>()) {
        worker_ = std::make_unique<taa_collection::CollectionWorker>(
            [state=state_, analyze=std::move(analyzer)](taa_collection::CollectionWorker::Control& control) {
                while (!control.Stopped()) {
                    taa_collection::shader_sources::Batch batch;
                    {
                        std::lock_guard lock(state->mutex);
                        batch = state->pending.Pending(0);
                    }
                    if (batch.programs.empty()) { control.Wait(std::chrono::milliseconds(250)); continue; }
                    for (const auto& program : batch.programs) {
                        if (control.Stopped()) return;
                        Summary summary;
                        try { summary = analyze(std::span<const uint8_t>(*program.bytes)); }
                        catch (...) { summary.issues = 4; }
                        if (control.Stopped()) return;
                        std::lock_guard lock(state->mutex);
                        const auto slot = state->Find(program.key.second);
                        if (slot < state->results.size()) {
                            auto& result = state->results[slot];
                            result.summary = summary;
                            result.ready = true;
                        }
                    }
                    std::lock_guard lock(state->mutex);
                    state->pending.Acknowledge(batch);
                }
            });
    }
    ~Collection() { Stop(); }
    Collection(const Collection&) = delete;
    Collection& operator=(const Collection&) = delete;

    void Stop() noexcept {
        state_->active.store(false, std::memory_order_relaxed);
        if (worker_) worker_->Stop();
    }

    bool TryGet(uint64_t hash, const uint32_t* words, size_t count, Summary& output) noexcept {
        output = {};
        if (!state_->active.load(std::memory_order_relaxed)) return false;
        bool notify = false;
        try {
            std::unique_lock lock(state_->mutex, std::try_to_lock);
            if (!lock || !state_->active.load(std::memory_order_relaxed)) return false;
            const auto slot = state_->Find(hash);
            if (slot == state_->results.size()) return false;
            auto& result = state_->results[slot];
            if (result.occupied && result.ready) { output = result.summary; return true; }
            const auto observation = state_->pending.Observe(true, hash, words, count);
            if (observation == taa_collection::shader_sources::Queue::Observation::Queued) {
                result.hash = hash;
                result.occupied = true;
                notify = true;
            }
        } catch (...) { return false; }
        if (notify) worker_->Notify();
        return false;
    }
};
}
