#include "movie_player.h"
#include "original_lengths.h"
#include <modding/mod_api.h>
#include <os/logger.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <deque>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace movie {
namespace {
// The game's videos are 30000/1001 fps. Its update sleeps until this long
// before a frame's time, so a frame arrives up to 5 ms early.
constexpr double kGuestFrameMs = 1001.0 / 30.0;
constexpr double kGuestWakeEarlyMs = 5.0;
// Call the guest update at most this long before it would wake by itself.
constexpr double kGuestSleepMs = 2.0;
// Guest frame arrivals the clock offset is taken from (about two seconds).
constexpr size_t kClockSamples = 60;
constexpr size_t kQueueAhead = 4;
// A stopped session stays readable this long for draws already queued.
constexpr double kRetireMs = 2000.0;
// A replacement this close to the original's length is stretched or squeezed
// evenly to it: a tool that relabels 29.97 fps as 30, or drops a few frames.
constexpr double kFitRange = 0.01;

struct Decoded {
    std::shared_ptr<Nv12Frame> frame;
    int64_t us = 0;
    uint64_t serial = 0;
};
std::atomic<uint64_t> gSerial{0};

// Host time to movie time. The guest delivers a frame no earlier than 5 ms
// before its time, so the smallest recent (arrival - pts) is the offset; late
// arrivals only raise it. A pause raises it too, until the old samples age out:
// the result is clamped to one guest frame past the last one shown.
class Clock {
public:
    void Shown(double ptsMs, double nowMs) {
        offsets_.push_back(nowMs - ptsMs);
        if (offsets_.size() > kClockSamples) offsets_.pop_front();
        minOffset_ = *std::min_element(offsets_.begin(), offsets_.end());
        lastPts_ = ptsMs;
    }
    bool Due(double nowMs) const {
        return lastPts_ < 0 || Estimate(nowMs) >= lastPts_ + kGuestFrameMs - kGuestWakeEarlyMs - kGuestSleepMs;
    }
    // The guest frame on screen arrived up to 5 ms before its time: allow that
    // much before it, so the steps stay even at the game's frame rate.
    int64_t TimeUs(double nowMs) const {
        if (lastPts_ < 0) return 0;
        const double t = std::clamp(Estimate(nowMs), lastPts_ - kGuestWakeEarlyMs, lastPts_ + kGuestFrameMs);
        return int64_t(std::max(t, 0.0) * 1000.0);
    }
private:
    double Estimate(double nowMs) const { return nowMs - minOffset_ - kGuestWakeEarlyMs; }
    std::deque<double> offsets_;
    double minOffset_ = 0.0, lastPts_ = -1.0;
};

class Session {
public:
    Session(std::string name, modding::ResolvedAsset asset) : name_(std::move(name)), asset_(std::move(asset)) {
        thread_ = std::thread([this] { Run(); });
    }
    ~Session() {
        RequestStop();
        if (thread_.joinable()) thread_.join();
    }
    void RequestStop() {
        { std::lock_guard lock(mutex_); stop_ = true; }
        cv_.notify_all();
    }
    bool Failed() const { return failed_.load(std::memory_order_acquire); }
    bool Ready() const { return ready_.load(std::memory_order_acquire); }

    bool FrameAt(int64_t us, DrawFrame& out) {
        std::lock_guard lock(mutex_);
        bool advanced = false;
        while (!queue_.empty() && queue_.front().us <= us) {
            Recycle(std::move(shown_.frame));
            shown_ = std::move(queue_.front());
            queue_.pop_front();
            advanced = true;
        }
        if (advanced) cv_.notify_all();
        // Before the first frame's time the first frame stands in.
        const Decoded* pick = shown_.frame ? &shown_ : queue_.empty() ? nullptr : &queue_.front();
        if (!pick) return false;
        // Past the end of a shorter replacement the guest's own video shows again.
        if (ended_ && queue_.empty() && us > shown_.us + endHoldUs_) return false;
        out.frame = pick->frame;
        out.serial = pick->serial;
        return true;
    }

    Clock clock; // render thread only

private:
    void Recycle(std::shared_ptr<Nv12Frame> frame) {
        if (frame && frame.use_count() == 1 && spare_.size() < 2) spare_.push_back(std::move(frame));
    }

    void Run() {
        std::string error;
        Mp4Track track;
        std::unique_ptr<MovieDecoder> decoder;
        auto source = OpenByteSource(asset_, &error);
        if (source && ParseMp4(*source, track, error)) {
            decoder = CreateMovieDecoder(true);
            if (!decoder) error = "no H.264 decoder on this platform";
            else {
                decoder->SetStopFlag(&stop_);
                if (!decoder->Open(track, *source, error)) decoder.reset();
            }
        }
        if (!decoder) {
            if (stop_.load(std::memory_order_relaxed)) return;
            LOG_WARNING("movie: {} keeps the original video, {} not used: {}", name_, modding::DescribeAsset(asset_), error);
            failed_.store(true, std::memory_order_release);
            return;
        }
        double lengthUs = 0.0;
        for (const auto& sample : track.samples)
            lengthUs = std::max(lengthUs, 1e6 * double(sample.pts + sample.duration) / track.timescale);
        double scale = 1.0;
        const double originalUs = 1e3 * OriginalLengthMs(name_);
        // More than a frame off, but within the fit range.
        if (originalUs > 0 && lengthUs > 0 && std::abs(originalUs - lengthUs) > 20000.0 &&
            std::abs(originalUs / lengthUs - 1.0) <= kFitRange)
            scale = originalUs / lengthUs;
        const double frameUs = track.frameDuration ? 1e6 * track.frameDuration / track.timescale : 1e6 / 60.0;
        endHoldUs_ = int64_t(frameUs * scale * 1.5);
        bool first = true;
        for (;;) {
            std::shared_ptr<Nv12Frame> frame;
            {
                std::unique_lock lock(mutex_);
                cv_.wait(lock, [&] { return stop_ || queue_.size() < kQueueAhead; });
                if (stop_) return;
                if (!spare_.empty()) { frame = std::move(spare_.back()); spare_.pop_back(); }
            }
            if (!frame) frame = std::make_shared<Nv12Frame>();
            if (!decoder->Next(*frame, error)) {
                if (stop_.load(std::memory_order_relaxed)) return;
                if (!error.empty()) {
                    failed_.store(true, std::memory_order_release);
                    LOG_WARNING("movie: {} decoding stopped: {}", name_, error);
                }
                std::lock_guard lock(mutex_);
                ended_ = true;
                if (!error.empty()) {
                    queue_.clear(); shown_ = {}; spare_.clear();
                }
                return;
            }
            if (first) {
                first = false;
                LOG_NOTICE("movie: {} replaced by {} ({}x{}, {:.3f} fps, {} {})", name_, modding::DescribeAsset(asset_),
                    track.width, track.height, 1e6 / frameUs, decoder->Name(), decoder->AccelerationName());
                if (scale != 1.0)
                    LOG_NOTICE("movie: {} is {:.3f} s, fitted to the original's {:.3f} s", name_, lengthUs / 1e6, originalUs / 1e6);
            }
            const int64_t us = int64_t(1e6 * double(frame->pts) * scale / track.timescale);
            std::lock_guard lock(mutex_);
            queue_.push_back({std::move(frame), us, ++gSerial});
            ready_.store(true, std::memory_order_release);
        }
    }

    const std::string name_;
    const modding::ResolvedAsset asset_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::deque<Decoded> queue_; // display order
    Decoded shown_;
    std::vector<std::shared_ptr<Nv12Frame>> spare_;
    std::atomic<bool> stop_{false};
    bool ended_ = false;
    int64_t endHoldUs_ = 25000;
    std::atomic<bool> failed_{false};
    std::atomic<bool> ready_{false};
    std::thread thread_;
};

struct Retired {
    uint32_t id;
    double at;
};
std::mutex gMutex;
std::map<uint32_t, std::shared_ptr<Session>> gSessions; // by session id
std::map<uint32_t, uint32_t> gByMovie;                  // guest movie object -> session id
std::vector<Retired> gRetired;
uint32_t gNextId = 0;
std::condition_variable gRetiredWake;
#if !defined(__ANDROID__) && !defined(__SWITCH__) && !defined(__NX__)
std::atomic<uint32_t> gGuestMovie{0};
#endif

std::shared_ptr<Session> Find(uint32_t id) {
    std::lock_guard lock(gMutex);
    const auto it = gSessions.find(id);
    return it == gSessions.end() ? nullptr : it->second;
}

// Started only when a replacement stops. Sleep until its grace period ends;
// normal frames do no cleanup work. The worker stops and joins before the
// session maps are destroyed at shutdown.
class RetiredCleanup {
    bool stop_ = false; // guarded by gMutex
    std::thread thread_{[this] {
        std::unique_lock lock(gMutex);
        for (;;) {
            gRetiredWake.wait(lock, [this] { return stop_ || !gRetired.empty(); });
            if (stop_) break;
            const auto delay = std::chrono::duration<double, std::milli>(
                std::max(0.0, gRetired.front().at + kRetireMs - NowMs()));
            if (gRetiredWake.wait_for(lock, delay, [this] { return stop_; })) break;
            std::vector<std::shared_ptr<Session>> expired;
            const double nowMs = NowMs();
            std::erase_if(gRetired, [&](const Retired& retired) {
                if (nowMs - retired.at < kRetireMs) return false;
                if (const auto session = gSessions.find(retired.id); session != gSessions.end()) {
                    expired.push_back(std::move(session->second));
                    gSessions.erase(session);
                }
                return true;
            });
            lock.unlock();
            expired.clear(); // Joining decode threads must not hold gMutex.
            lock.lock();
        }
    }};
public:
    ~RetiredCleanup() {
        { std::lock_guard lock(gMutex); stop_ = true; }
        gRetiredWake.notify_all();
        thread_.join();
    }
};

void WakeRetiredCleanupLocked() {
    static RetiredCleanup cleanup;
    gRetiredWake.notify_all();
}

// Caller holds gMutex. Pending draws retain the stopped session for kRetireMs.
void RetireLocked(uint32_t guestMovie, double nowMs) {
    if (const auto it = gByMovie.find(guestMovie); it != gByMovie.end()) {
        if (const auto session = gSessions.find(it->second); session != gSessions.end()) session->second->RequestStop();
        gRetired.push_back({it->second, nowMs});
        gByMovie.erase(it);
        WakeRetiredCleanupLocked();
    }
}
}

double NowMs() {
    using namespace std::chrono;
    return duration<double, std::milli>(steady_clock::now().time_since_epoch()).count();
}

void StartReplacement(uint32_t guestMovie, std::string_view name) {
#if !defined(__ANDROID__) && !defined(__SWITCH__) && !defined(__NX__)
    gGuestMovie.store(guestMovie, std::memory_order_relaxed);
#endif
    {
        std::lock_guard lock(gMutex);
        RetireLocked(guestMovie, NowMs());
    }
    if (name.empty() || !modding::Enabled()) return;
    auto asset = modding::Resolve({{modding::AssetKind::Movie, std::string(name)}, {}});
    if (!asset) return;
    auto session = std::make_shared<Session>(std::string(name), std::move(*asset));
    std::lock_guard lock(gMutex);
    const uint32_t id = ++gNextId ? gNextId : ++gNextId;
    gSessions.emplace(id, std::move(session));
    gByMovie[guestMovie] = id;
}

void StopReplacement(uint32_t guestMovie) {
#if !defined(__ANDROID__) && !defined(__SWITCH__) && !defined(__NX__)
    uint32_t active = guestMovie;
    gGuestMovie.compare_exchange_strong(active, 0u, std::memory_order_relaxed);
#endif
    std::lock_guard lock(gMutex);
    RetireLocked(guestMovie, NowMs());
}

#if !defined(__ANDROID__) && !defined(__SWITCH__) && !defined(__NX__)
bool GuestPlaying() {
    return gGuestMovie.load(std::memory_order_relaxed) != 0;
}
#endif

uint32_t ReplacementSession(uint32_t guestMovie) {
    std::lock_guard lock(gMutex);
    if (gByMovie.empty()) return 0;
    const auto it = gByMovie.find(guestMovie);
    if (it == gByMovie.end()) return 0;
    const auto session = gSessions.find(it->second);
    return session != gSessions.end() && !session->second->Failed() ? it->second : 0;
}

bool GuestUpdateDue(uint32_t session, double nowMs) {
    const auto s = Find(session);
    // Keep collecting the guest clock during startup, but retain the guest's
    // original pacing until a replacement picture is actually available.
    return !s || !s->Ready() || s->clock.Due(nowMs);
}

void GuestFrameShown(uint32_t session, double ptsMs, double nowMs) {
    if (const auto s = Find(session)) s->clock.Shown(ptsMs, nowMs);
}

int64_t ReplacementTimeUs(uint32_t session, double nowMs) {
    const auto s = Find(session);
    return s ? s->clock.TimeUs(nowMs) : 0;
}

bool FrameForDraw(uint32_t session, int64_t timeUs, DrawFrame& out) {
    const auto s = Find(session);
    return s && !s->Failed() && s->FrameAt(timeUs, out);
}
}
