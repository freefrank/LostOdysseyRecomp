#include "io_diagnostics.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#elif defined(__linux__)
#include <sys/syscall.h>
#include <unistd.h>
#elif defined(__APPLE__) || defined(__FreeBSD__)
#include <pthread.h>
#endif

namespace io_diagnostics
{
namespace
{
struct Slot
{
    std::mutex mutex;
    // Separate from the protected payload so destruction can always release a
    // reservation without waiting for a concurrent manual snapshot.
    std::atomic<uint64_t> reservation{0};
    Record record{};
};

std::array<Slot, ActiveCapacity> activeSlots;
std::array<Slot, HistoryCapacity> historySlots;
std::atomic<uint64_t> nextRequest{1};
std::atomic<uint64_t> nextObject{1};
std::atomic<uint64_t> nextSequence{1};
std::atomic<size_t> nextActiveSlot{0};
std::atomic<uint64_t> droppedRequests{0};
std::atomic<uint64_t> droppedHistoryEvents{0};
std::atomic<uint64_t> historyOverwrites{0};

uint64_t NowNs() noexcept
{
    return uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
}

uint64_t HostTid() noexcept
{
#if defined(_WIN32)
    return GetCurrentThreadId();
#elif defined(__linux__)
    return uint64_t(syscall(SYS_gettid));
#elif defined(__APPLE__)
    uint64_t tid = 0;
    pthread_threadid_np(nullptr, &tid);
    return tid;
#elif defined(__FreeBSD__)
    return uint64_t(pthread_getthreadid_np());
#else
    // Explicitly unavailable; never substitute a hash or a guest thread ID.
    return 0;
#endif
}

bool TryLock(Slot& slot) noexcept
{
    return slot.mutex.try_lock();
}

void Unlock(Slot& slot) noexcept
{
    slot.mutex.unlock();
}

void CopyOperation(Record& record, const char* operation) noexcept
{
    if (!operation)
        return;
    size_t n = 0;
    while (n + 1 < sizeof(record.operation) && operation[n])
    {
        record.operation[n] = operation[n];
        ++n;
    }
    record.operation[n] = '\0';
}

void CopyPath(Record& record, const std::filesystem::path& path) noexcept
{
    const auto& native = path.native();
    size_t written = 0;
    size_t read = 0;
#if defined(_WIN32)
    // Convert directly into the bounded record; path.string()/u8string() would
    // allocate for every acquisition and may throw on an unrepresentable name.
    while (read < native.size())
    {
        uint32_t cp = uint16_t(native[read]);
        size_t consumed = 1;
        if (cp >= 0xD800 && cp <= 0xDBFF && read + 1 < native.size() &&
            uint16_t(native[read + 1]) >= 0xDC00 && uint16_t(native[read + 1]) <= 0xDFFF)
        {
            cp = 0x10000 + ((cp - 0xD800) << 10) + (uint16_t(native[read + 1]) - 0xDC00);
            consumed = 2;
        }
        else if (cp >= 0xD800 && cp <= 0xDFFF)
            cp = 0xFFFD;
        const size_t bytes = cp < 0x80 ? 1 : cp < 0x800 ? 2 : cp < 0x10000 ? 3 : 4;
        if (written + bytes >= sizeof(record.path))
            break;
        if (bytes == 1)
            record.path[written++] = char(cp);
        else
        {
            record.path[written++] = char((bytes == 2 ? 0xC0 : bytes == 3 ? 0xE0 : 0xF0) |
                (cp >> (6 * (bytes - 1))));
            for (size_t remaining = bytes - 1; remaining > 0; --remaining)
                record.path[written++] = char(0x80 | ((cp >> (6 * (remaining - 1))) & 0x3F));
        }
        read += consumed;
    }
#else
    written = std::min(native.size(), sizeof(record.path) - 1);
    std::memcpy(record.path, native.data(), written);
    read = written;
#endif
    record.path[written] = '\0';
    record.pathTruncated = read < native.size();
}

Record NewRecord(const char* operation, uint32_t handle, uint64_t offset,
    uint32_t length, uint32_t pcr) noexcept
{
    Record record{};
    record.request = nextRequest.fetch_add(1, std::memory_order_relaxed);
    record.hostTid = HostTid();
    record.guestPcr = pcr;
    record.handle = handle;
    record.requestedOffset = offset;
    record.resolvedOffset = UnspecifiedOffset;
    record.length = length;
    record.startedNs = record.updatedNs = NowNs();
    CopyOperation(record, operation);
    return record;
}

void SaveHistory(const Record& record) noexcept
{
    Slot& slot = historySlots[(record.sequence - 1) % HistoryCapacity];
    if (!TryLock(slot))
    {
        droppedHistoryEvents.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    // A preempted writer must not replace a newer event after the ring wraps.
    if (slot.record.sequence >= record.sequence)
        droppedHistoryEvents.fetch_add(1, std::memory_order_relaxed);
    else
    {
        if (slot.record.sequence)
            historyOverwrites.fetch_add(1, std::memory_order_relaxed);
        slot.record = record;
    }
    Unlock(slot);
}

void ObserveOwner(Record& record, const Object& object) noexcept
{
    record.ownerObservationStable = false;
    record.observedOwnerRequest = record.observedOwnerTid = record.observedOwnerSinceNs = 0;
    for (unsigned attempt = 0; attempt < 3; ++attempt)
    {
        const uint64_t request = object.ownerRequest.load(std::memory_order_acquire);
        const uint64_t tid = object.ownerTid.load(std::memory_order_relaxed);
        const uint64_t since = object.ownerSinceNs.load(std::memory_order_relaxed);
        if (request == object.ownerRequest.load(std::memory_order_acquire))
        {
            record.ownerObservationStable = true;
            record.observedOwnerRequest = request;
            record.observedOwnerTid = request ? tid : 0;
            record.observedOwnerSinceNs = request ? since : 0;
            return;
        }
    }
}

void WriteString(FILE* stream, const char* text)
{
    std::fputc('"', stream);
    for (const auto* p = reinterpret_cast<const unsigned char*>(text); *p; ++p)
    {
        if (*p == '"' || *p == '\\')
        {
            std::fputc('\\', stream);
            std::fputc(*p, stream);
        }
        else if (*p < 0x20)
            std::fprintf(stream, "\\u%04x", unsigned(*p));
        else
            std::fputc(*p, stream);
    }
    std::fputc('"', stream);
}

void WriteRecord(FILE* stream, const char* type, const Record& r)
{
    std::fprintf(stream, "{\"type\":\"%s\",\"sequence\":%" PRIu64 ",\"request\":%" PRIu64
        ",\"object_instance\":%" PRIu64 ",\"object_address\":%" PRIuPTR ",\"mutex_address\":%" PRIuPTR
        ",\"host_tid\":%" PRIu64 ",\"guest_pcr\":%" PRIu32 ",\"handle\":%" PRIu32
        ",\"requested_offset\":%" PRIu64 ",\"resolved_offset\":%" PRIu64 ",\"length\":%" PRIu32
        ",\"status\":%" PRIu32 ",\"transferred\":%" PRIu32 ",\"started_ns\":%" PRIu64
        ",\"updated_ns\":%" PRIu64 ",\"lock_acquired_ns\":%" PRIu64,
        type, r.sequence, r.request, r.objectInstance, r.objectAddress, r.mutexAddress,
        r.hostTid, r.guestPcr, r.handle, r.requestedOffset, r.resolvedOffset, r.length,
        r.status, r.transferred, r.startedNs, r.updatedNs, r.lockAcquiredNs);
    std::fprintf(stream, ",\"stage\":\"%s\",\"lock_held\":%s,\"has_result\":%s"
        ",\"owner_observation_stable\":%s,\"observed_owner_request\":%" PRIu64
        ",\"observed_owner_tid\":%" PRIu64 ",\"observed_owner_since_ns\":%" PRIu64
        ",\"snapshot_owner_resolved\":%s,\"snapshot_owner_request\":%" PRIu64
        ",\"snapshot_owner_tid\":%" PRIu64 ",\"snapshot_owner_since_ns\":%" PRIu64
        ",\"owner_changed_while_waiting\":%s,\"path_truncated\":%s,\"operation\":",
        StageName(r.stage), r.lockHeld ? "true" : "false", r.hasResult ? "true" : "false",
        r.ownerObservationStable ? "true" : "false", r.observedOwnerRequest,
        r.observedOwnerTid, r.observedOwnerSinceNs, r.snapshotOwnerResolved ? "true" : "false",
        r.snapshotOwnerRequest, r.snapshotOwnerTid, r.snapshotOwnerSinceNs,
        r.ownerChangedWhileWaiting ? "true" : "false", r.pathTruncated ? "true" : "false");
    WriteString(stream, r.operation);
    std::fputs(",\"path\":", stream);
    WriteString(stream, r.path);
    std::fputs("}\n", stream);
}
}

bool Enabled() noexcept
{
    static const bool enabled = []
    {
        const char* value = std::getenv("LO_IO_DIAGNOSTICS");
        return value && value[0] == '1' && value[1] == '\0';
    }();
    return enabled;
}

Object::Object() noexcept : instance(Enabled() ? nextObject.fetch_add(1, std::memory_order_relaxed) : 0) {}

const char* StageName(Stage stage) noexcept
{
    switch (stage)
    {
    case Stage::ApiEnter: return "ApiEnter";
    case Stage::HandleAcquired: return "HandleAcquired";
    case Stage::IoLockWait: return "IoLockWait";
    case Stage::IoLockAcquired: return "IoLockAcquired";
    case Stage::IoLockReleased: return "IoLockReleased";
    case Stage::TransferDone: return "TransferDone";
    case Stage::CompletionPublished: return "CompletionPublished";
    case Stage::ApiReturn: return "ApiReturn";
    case Stage::Created: return "Created";
    case Stage::CloseBegin: return "CloseBegin";
    case Stage::CloseEnd: return "CloseEnd";
    case Stage::Destroyed: return "Destroyed";
    case Stage::DiscSelectBegin: return "DiscSelectBegin";
    case Stage::DiscSelectEnd: return "DiscSelectEnd";
    }
    return "Unknown";
}

Request::Request(const char* operation, uint32_t handle, uint64_t requestedOffset,
    uint32_t length, uint32_t pcr) noexcept : enabled_(Enabled())
{
    if (!enabled_)
        return;
    record_ = NewRecord(operation, handle, requestedOffset, length, pcr);
    const size_t first = nextActiveSlot.fetch_add(1, std::memory_order_relaxed) % ActiveCapacity;
    for (size_t i = 0; i < ActiveCapacity; ++i)
    {
        const size_t index = (first + i) % ActiveCapacity;
        uint64_t empty = 0;
        if (activeSlots[index].reservation.compare_exchange_strong(empty, record_.request,
            std::memory_order_acq_rel, std::memory_order_relaxed))
        {
            slot_ = index;
            break;
        }
    }
    if (slot_ == ActiveCapacity)
        droppedRequests.fetch_add(1, std::memory_order_relaxed);
    SetStage(Stage::ApiEnter);
}

Request::~Request()
{
    if (!enabled_)
        return;
    // Do not dereference object_: the handle may already have been destroyed.
    SetStage(Stage::ApiReturn);
    if (slot_ != ActiveCapacity)
        activeSlots[slot_].reservation.store(0, std::memory_order_release);
}

void Request::Publish() noexcept
{
    record_.sequence = nextSequence.fetch_add(1, std::memory_order_relaxed);
    record_.updatedNs = NowNs();
    if (slot_ != ActiveCapacity)
    {
        Slot& slot = activeSlots[slot_];
        // Only this request and a manual snapshot can access this payload. A
        // snapshot holds the slot just long enough to copy it into preallocated
        // storage. Do not drop a final IoLockWait/IoLockAcquired publication: a
        // blocked request would otherwise remain at its previous stage forever.
        std::lock_guard lock(slot.mutex);
        slot.record = record_;
    }
    SaveHistory(record_);
}

void Request::Acquired(Object& object, const void* mutex, const std::filesystem::path& path,
    const void* objectAddress) noexcept
{
    if (!enabled_)
        return;
    object_ = &object;
    record_.objectInstance = object.instance;
    record_.objectAddress = reinterpret_cast<uintptr_t>(objectAddress ? objectAddress : &object);
    record_.mutexAddress = reinterpret_cast<uintptr_t>(mutex);
    CopyPath(record_, path);
    SetStage(Stage::HandleAcquired);
}

void Request::SetStage(Stage stage) noexcept
{
    if (!enabled_)
        return;
    record_.stage = stage;
    Publish();
}

void Request::SetResolvedOffset(uint64_t offset) noexcept
{
    if (!enabled_)
        return;
    record_.resolvedOffset = offset;
    Publish();
}

void Request::SetRequestedOffset(uint64_t offset) noexcept
{
    if (!enabled_)
        return;
    record_.requestedOffset = offset;
    Publish();
}

void Request::SetResult(uint32_t status, uint32_t transferred) noexcept
{
    if (!enabled_)
        return;
    record_.status = status;
    record_.transferred = transferred;
    record_.hasResult = true;
    Publish();
}

void Request::LockWaiting() noexcept
{
    if (!enabled_)
        return;
    if (object_)
        ObserveOwner(record_, *object_);
    SetStage(Stage::IoLockWait);
}

void Request::LockAcquired() noexcept
{
    if (!enabled_)
        return;
    record_.lockHeld = true;
    record_.lockAcquiredNs = NowNs();
    if (object_)
    {
        object_->ownerTid.store(record_.hostTid, std::memory_order_relaxed);
        object_->ownerSinceNs.store(record_.lockAcquiredNs, std::memory_order_relaxed);
        object_->ownerRequest.store(record_.request, std::memory_order_release);
    }
    SetStage(Stage::IoLockAcquired);
}

void Request::LockReleased() noexcept
{
    if (!enabled_ || !record_.lockHeld)
        return;
    if (object_)
    {
        uint64_t request = record_.request;
        object_->ownerRequest.compare_exchange_strong(request, 0, std::memory_order_release,
            std::memory_order_relaxed);
    }
    record_.lockHeld = false;
    SetStage(Stage::IoLockReleased);
}

void RecordEvent(Stage stage, const char* operation, const Object* object,
    uint32_t handle, const std::filesystem::path* path, uint32_t pcr, const void* mutex,
    const void* objectAddress) noexcept
{
    if (!Enabled())
        return;
    Record record = NewRecord(operation, handle, UnspecifiedOffset, 0, pcr);
    record.stage = stage;
    record.mutexAddress = reinterpret_cast<uintptr_t>(mutex);
    if (object)
    {
        record.objectInstance = object->instance;
        record.objectAddress = reinterpret_cast<uintptr_t>(objectAddress ? objectAddress : object);
        ObserveOwner(record, *object);
    }
    if (path)
        CopyPath(record, *path);
    record.sequence = nextSequence.fetch_add(1, std::memory_order_relaxed);
    SaveHistory(record);
}

SnapshotData Snapshot()
{
    SnapshotData result;
    result.enabled = Enabled();
    result.capturedNs = NowNs();
    if (!result.enabled)
    {
        result.finishedNs = NowNs();
        return result;
    }
    result.active.reserve(ActiveCapacity);
    result.history.reserve(HistoryCapacity);
    for (Slot& slot : activeSlots)
    {
        if (!TryLock(slot))
        {
            ++result.skippedBusySlots;
            continue;
        }
        const uint64_t reservation = slot.reservation.load(std::memory_order_acquire);
        if (reservation && reservation == slot.record.request)
            result.active.push_back(slot.record);
        else if (reservation)
            ++result.skippedUnpublishedSlots;
        Unlock(slot);
    }
    for (Slot& slot : historySlots)
    {
        if (!TryLock(slot))
        {
            ++result.skippedBusySlots;
            continue;
        }
        if (slot.record.sequence)
            result.history.push_back(slot.record);
        Unlock(slot);
    }
    // A waiter may outlive several owners. Resolve against copied active holder
    // records instead of treating the owner observed before lock() as current.
    for (Record& waiter : result.active)
    {
        if (!waiter.objectInstance)
            continue;
        const Record* owner = nullptr;
        for (const Record& candidate : result.active)
        {
            if (candidate.objectInstance == waiter.objectInstance && candidate.lockHeld &&
                (!owner || candidate.lockAcquiredNs > owner->lockAcquiredNs))
                owner = &candidate;
        }
        if (owner)
        {
            waiter.snapshotOwnerResolved = true;
            waiter.snapshotOwnerRequest = owner->request;
            waiter.snapshotOwnerTid = owner->hostTid;
            waiter.snapshotOwnerSinceNs = owner->lockAcquiredNs;
            waiter.ownerChangedWhileWaiting = waiter.stage == Stage::IoLockWait &&
                waiter.ownerObservationStable && waiter.observedOwnerRequest != owner->request;
        }
    }
    auto bySequence = [](const Record& a, const Record& b) { return a.sequence < b.sequence; };
    std::sort(result.active.begin(), result.active.end(), bySequence);
    std::sort(result.history.begin(), result.history.end(), bySequence);
    result.droppedRequests = droppedRequests.load(std::memory_order_relaxed);
    result.droppedHistoryEvents = droppedHistoryEvents.load(std::memory_order_relaxed);
    result.droppedEvents = result.droppedHistoryEvents;
    result.historyOverwrites = historyOverwrites.load(std::memory_order_relaxed);
    result.finishedNs = NowNs();
    return result;
}

void Dump(const char* path)
{
    const SnapshotData snapshot = Snapshot();
    FILE* stream = path ? std::fopen(path, "wb") : stderr;
    if (!stream)
    {
        std::fprintf(stderr, "LoDumpIoDiagnostics: could not open '%s'\n", path);
        return;
    }
    std::fprintf(stream, "{\"type\":\"summary\",\"enabled\":%s,\"captured_ns\":%" PRIu64
        ",\"finished_ns\":%" PRIu64 ",\"active_capacity\":%zu,\"history_capacity\":%zu"
        ",\"active_count\":%zu,\"history_count\":%zu,\"dropped_requests\":%" PRIu64
        ",\"dropped_events\":%" PRIu64 ",\"dropped_history_events\":%" PRIu64
        ",\"history_overwrites\":%" PRIu64
        ",\"skipped_busy_slots\":%" PRIu64 ",\"skipped_unpublished_slots\":%" PRIu64 "}\n",
        snapshot.enabled ? "true" : "false", snapshot.capturedNs, snapshot.finishedNs,
        ActiveCapacity, HistoryCapacity, snapshot.active.size(), snapshot.history.size(),
        snapshot.droppedRequests, snapshot.droppedEvents, snapshot.droppedHistoryEvents,
        snapshot.historyOverwrites,
        snapshot.skippedBusySlots, snapshot.skippedUnpublishedSlots);
    for (const Record& record : snapshot.active)
        WriteRecord(stream, "active", record);
    for (const Record& record : snapshot.history)
        WriteRecord(stream, "history", record);
    if (path)
        std::fclose(stream);
    else
        std::fflush(stream);
}
}

#if defined(_WIN32)
extern "C" __declspec(dllexport) void LoDumpIoDiagnostics(const char* path)
#else
extern "C" __attribute__((used, visibility("default"))) void LoDumpIoDiagnostics(const char* path)
#endif
{
    try
    {
        io_diagnostics::Dump(path);
    }
    catch (...)
    {
        std::fputs("LoDumpIoDiagnostics: snapshot failed\n", stderr);
    }
}
