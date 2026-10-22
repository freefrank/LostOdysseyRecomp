#pragma once
#include <atomic>
#include <cstdint>

namespace gpu::presentation_progress {
enum class Stage : uint32_t {
    Idle, Present, Prepare, Reconcile, ResizeDrain, Resize, Source, Fence, RendererFence,
    Acquire, Record, Submit, SubmitQueueLock, SubmitNative, QueuePresent, Capture
};

inline const char* Name(Stage stage)
{
    switch (stage) {
    case Stage::Idle: return "idle";
    case Stage::Present: return "present";
    case Stage::Prepare: return "prepare";
    case Stage::Reconcile: return "reconcile";
    case Stage::ResizeDrain: return "resize-drain";
    case Stage::Resize: return "resize";
    case Stage::Source: return "source";
    case Stage::Fence: return "present-fence";
    case Stage::RendererFence: return "renderer-fence";
    case Stage::Acquire: return "acquire";
    case Stage::Record: return "record";
    case Stage::Submit: return "submit";
    case Stage::SubmitQueueLock: return "submit-queue-lock";
    case Stage::SubmitNative: return "submit-native";
    case Stage::QueuePresent: return "queue-present";
    case Stage::Capture: return "capture";
    }
    return "unknown";
}

inline constexpr uint32_t NoImage = UINT32_MAX;
struct Snapshot {
    Stage stage = Stage::Idle;
    uint64_t submitted = 0, completed = 0, pendingFence = 0;
    uint64_t swapchainGeneration = 0;
    uint32_t image = NoImage;
    bool loading = false, vsync = false;
};

// One command-thread writer, arbitrary observers. All observations are atomic;
// fields may span adjacent updates, and are not a GPU completion query. The
// watchdog only reads these values: no locks, driver calls or waiting there.
class Progress {
public:
    static_assert(std::atomic<uint64_t>::is_always_lock_free);
    static_assert(std::atomic<uint32_t>::is_always_lock_free);
    static_assert(std::atomic<Stage>::is_always_lock_free);
    class Scope {
    public:
        Scope(Progress& progress, Stage stage) : progress_(progress),
            previous_(progress.stage_.load(std::memory_order_relaxed)) { Set(stage); }
        ~Scope() { Set(previous_); }
        Scope(const Scope&) = delete;
        Scope& operator=(const Scope&) = delete;
        void Set(Stage stage) { progress_.stage_.store(stage, std::memory_order_relaxed); }
    private:
        Progress& progress_;
        Stage previous_;
    };

    void Policy(bool loading, bool vsync) {
        policy_.store(uint32_t(loading) | (uint32_t(vsync) << 1), std::memory_order_relaxed);
    }
    void Image(uint32_t image) { image_.store(image, std::memory_order_relaxed); }
    void SwapchainChanged() {
        swapchainGeneration_.store(swapchainGeneration_.load(std::memory_order_relaxed) + 1,
            std::memory_order_relaxed);
    }
    void Submitted(uint64_t serial) { submitted_.store(serial, std::memory_order_relaxed); }
    void Completed() { completed_.store(submitted_.load(std::memory_order_relaxed), std::memory_order_relaxed); }
    void ResetSubmissions() {
        submitted_.store(0, std::memory_order_relaxed);
        completed_.store(0, std::memory_order_relaxed);
        Image(NoImage);
    }
    Snapshot Read() const {
        Snapshot result;
        result.stage = stage_.load(std::memory_order_relaxed);
        result.submitted = submitted_.load(std::memory_order_relaxed);
        result.completed = completed_.load(std::memory_order_relaxed);
        result.pendingFence = result.submitted > result.completed ? result.submitted : 0;
        result.swapchainGeneration = swapchainGeneration_.load(std::memory_order_relaxed);
        result.image = image_.load(std::memory_order_relaxed);
        const auto policy = policy_.load(std::memory_order_relaxed);
        result.loading = (policy & 1) != 0;
        result.vsync = (policy & 2) != 0;
        return result;
    }
private:
    std::atomic<Stage> stage_{Stage::Idle};
    std::atomic<uint64_t> submitted_{0}, completed_{0}, swapchainGeneration_{0};
    std::atomic<uint32_t> image_{NoImage}, policy_{0};
};
} // namespace gpu::presentation_progress
