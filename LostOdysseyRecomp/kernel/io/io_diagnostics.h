#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <vector>

namespace io_diagnostics
{
inline constexpr size_t ActiveCapacity = 256;
inline constexpr size_t HistoryCapacity = 2048;
inline constexpr uint64_t UnspecifiedOffset = UINT64_MAX;

// LO_IO_DIAGNOSTICS is read once. Only the exact value "1" enables recording.
bool Enabled() noexcept;

struct Object
{
    Object() noexcept;

    const uint64_t instance;
    std::atomic<uint64_t> ownerRequest{0};
    std::atomic<uint64_t> ownerTid{0};
    std::atomic<uint64_t> ownerSinceNs{0};
};

enum class Stage : uint8_t
{
    ApiEnter,
    HandleAcquired,
    IoLockWait,
    IoLockAcquired,
    IoLockReleased,
    TransferDone,
    CompletionPublished,
    ApiReturn,
    Created,
    CloseBegin,
    CloseEnd,
    Destroyed,
    DiscSelectBegin,
    DiscSelectEnd,
};

const char* StageName(Stage stage) noexcept;

// Plain, owned data: no pointer in a record is ever dereferenced by Snapshot.
// Addresses are diagnostic values only; instance distinguishes address reuse.
struct Record
{
    uint64_t sequence;
    uint64_t request;
    uint64_t objectInstance;
    uintptr_t objectAddress;
    uintptr_t mutexAddress;
    uint64_t hostTid;
    uint32_t guestPcr;
    uint32_t handle;
    uint64_t requestedOffset;
    uint64_t resolvedOffset;
    uint32_t length;
    uint32_t status;
    uint32_t transferred;
    uint64_t startedNs;
    uint64_t updatedNs;
    uint64_t lockAcquiredNs;
    uint64_t observedOwnerRequest;
    uint64_t observedOwnerTid;
    uint64_t observedOwnerSinceNs;
    uint64_t snapshotOwnerRequest;
    uint64_t snapshotOwnerTid;
    uint64_t snapshotOwnerSinceNs;
    Stage stage;
    bool lockHeld;
    bool hasResult;
    bool ownerObservationStable;
    bool snapshotOwnerResolved;
    bool ownerChangedWhileWaiting;
    bool pathTruncated;
    char operation[48];
    char path[1024];
};

class Request
{
public:
    Request(const char* operation, uint32_t handle, uint64_t requestedOffset,
        uint32_t length, uint32_t pcr) noexcept;
    ~Request();
    Request(const Request&) = delete;
    Request& operator=(const Request&) = delete;

    void Acquired(Object& object, const void* mutex, const std::filesystem::path& path,
        const void* objectAddress = nullptr) noexcept;
    void SetStage(Stage stage) noexcept;
    void SetRequestedOffset(uint64_t offset) noexcept;
    // Call only after resolving the file position while holding its I/O mutex.
    void SetResolvedOffset(uint64_t offset) noexcept;
    void SetResult(uint32_t status, uint32_t transferred = 0) noexcept;
    void LockWaiting() noexcept;
    void LockAcquired() noexcept;
    // Call immediately before the actual unlock, while Object is still alive.
    void LockReleased() noexcept;

private:
    void Publish() noexcept;

    bool enabled_ = false;
    size_t slot_ = ActiveCapacity;
    Object* object_ = nullptr;
    Record record_;
};

void RecordEvent(Stage stage, const char* operation, const Object* object,
    uint32_t handle, const std::filesystem::path* path = nullptr, uint32_t pcr = 0,
    const void* mutex = nullptr, const void* objectAddress = nullptr) noexcept;

struct SnapshotData
{
    bool enabled = false;
    uint64_t capturedNs = 0;
    uint64_t finishedNs = 0;
    uint64_t droppedRequests = 0;
    uint64_t droppedEvents = 0;
    uint64_t droppedHistoryEvents = 0;
    uint64_t historyOverwrites = 0;
    uint64_t skippedBusySlots = 0;
    uint64_t skippedUnpublishedSlots = 0;
    std::vector<Record> active;
    std::vector<Record> history;
};

// Manual, best-effort observation over [capturedNs, finishedNs], not a global
// stop-the-world transaction. Busy slots are skipped, including in a debugger.
// A resolved owner comes from the latest copied active holder of that instance;
// an unresolved owner is unknown, not proof that the mutex is unlocked.
SnapshotData Snapshot();
}

// Suitable for: (gdb) call LoDumpIoDiagnostics("/tmp/lo-io.jsonl")
// nullptr writes to stderr. Recording never opens or flushes a log file.
extern "C" void LoDumpIoDiagnostics(const char* path);
