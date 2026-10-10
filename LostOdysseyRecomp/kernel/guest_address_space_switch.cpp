#include <os/platform.h>
#if LO_PLATFORM_SWITCH
#include "guest_address_space_layout.h"
#include <switch.h>
#include <malloc.h>
#include <cstdlib>
#include <cstring>
#include <mutex>

// Nintendo Switch backend (Horizon OS, libnx). Ported from UnleashedRecomp-NX
// (kernel/memory.cpp) and extended with the Xbox 360 physical aliases.
//
// Horizon has no mmap, no shared anonymous memory and no lazy commit. The
// 4 GiB guest window is a virtmem reservation in the ASLR region; guest pages
// are backed explicitly through Commit() by the allocators that hand them out
// (memory.cpp PageAllocator, heap.cpp, image load). A backing chunk is heap
// memory turned into code memory (svcMapProcessCodeMemory) and then mapped
// into the window with svcMapProcessMemory. The same code-memory source can be
// mapped more than once, which gives the physical views their aliases:
//   A  0xA0000000 + x   backing x
//   C  0xC0000000 + x   backing x          (same pages as A)
//   E  0xE0000000 + y   backing y + 0x1000 (A shifted by one 4 KiB page)
// Both syscalls need the homebrew loader to grant them: launch through title
// takeover (hold R while starting a game), not the album applet.
namespace GuestAddressSpace
{
namespace
{
constexpr size_t kPage = 0x1000;
// Commit granularity. Large chunks keep the number of kernel memory blocks
// low; the guest's own allocators are dense, so little is wasted.
constexpr size_t kChunk = 0x200000;
constexpr size_t kChunkCount = kSize / kChunk;
constexpr unsigned kSvcMapProcessMemory = 0x74;
constexpr unsigned kSvcUnmapProcessMemory = 0x75;
constexpr unsigned kSvcMapProcessCodeMemory = 0x77;
constexpr unsigned kSvcUnmapProcessCodeMemory = 0x78;

uint8_t* s_base = nullptr;
VirtmemReservation* s_reservation = nullptr;
Handle s_process = INVALID_HANDLE;
std::mutex s_mutex;
// One flag per 2 MiB chunk of the virtual view and of the A view. C and E are
// committed together with A and never on their own.
bool s_committed[kChunkCount]{};
size_t s_committedBytes = 0;

bool ProcessMemorySyscallsAvailable()
{
    return envIsSyscallHinted(kSvcMapProcessMemory) && envIsSyscallHinted(kSvcUnmapProcessMemory) &&
        envIsSyscallHinted(kSvcMapProcessCodeMemory) && envIsSyscallHinted(kSvcUnmapProcessCodeMemory);
}

struct HandleReceive
{
    Handle session;
    Handle received;
};

void ReceiveOwnProcessHandle(void* arg)
{
    auto& receive = *static_cast<HandleReceive*>(arg);
    void* tls = armGetTls();
    hipcMakeRequestInline(tls);
    s32 index = 0;
    if (R_SUCCEEDED(svcReplyAndReceive(&index, &receive.session, 1, INVALID_HANDLE, UINT64_MAX)))
    {
        const HipcParsedRequest request = hipcParseRequest(tls);
        if (request.meta.num_copy_handles == 1)
            receive.received = request.data.copy_handles[0];
    }
    svcCloseHandle(receive.session);
}

// The process syscalls need a real handle to this process; they reject the
// CUR_PROCESS_HANDLE pseudo-handle. hbloader passes one. Loaders that do not
// (Ryujinx runs an NRO directly) get one the way hbloader does: send the
// pseudo-handle to ourselves over a session, and the kernel copies a real one.
Handle OwnProcessHandle()
{
    if (const Handle own = envGetOwnProcessHandle(); own != INVALID_HANDLE)
        return own;
    Handle server = INVALID_HANDLE, client = INVALID_HANDLE;
    if (R_FAILED(svcCreateSession(&server, &client, 0, 0)))
        return INVALID_HANDLE;
    HandleReceive receive{ server, INVALID_HANDLE };
    Thread thread;
    if (R_FAILED(threadCreate(&thread, ReceiveOwnProcessHandle, &receive, nullptr, 0x1000, 0x20, -2)) ||
        R_FAILED(threadStart(&thread)))
    {
        svcCloseHandle(server);
        svcCloseHandle(client);
        return INVALID_HANDLE;
    }
    hipcMakeRequestInline(armGetTls(), .num_copy_handles = 1).copy_handles[0] = CUR_PROCESS_HANDLE;
    svcSendSyncRequest(client); // The receiver closes the session instead of replying.
    svcCloseHandle(client);
    threadWaitForExit(&thread);
    threadClose(&thread);
    return receive.received;
}

// Backs [offset, offset + size) of the window with fresh zeroed memory and,
// for the A view, maps the same pages at the C and E aliases.
bool MapChunk(size_t offset, size_t size)
{
    void* backing = memalign(kPage, size);
    if (!backing)
    {
        RecordFailure(FailureOperation::CreateBacking, 0, -1, nullptr, size, offset);
        return false;
    }
    std::memset(backing, 0, size);

    void* source = nullptr;
    Result rc = 0;
    virtmemLock();
    source = virtmemFindCodeMemory(size, kPage);
    if (source)
    {
        rc = svcMapProcessCodeMemory(s_process, uintptr_t(source), uintptr_t(backing), size);
        if (R_SUCCEEDED(rc))
            rc = svcSetProcessMemoryPermission(s_process, uintptr_t(source), size, Perm_Rw);
    }
    virtmemUnlock();
    if (!source || R_FAILED(rc))
    {
        RecordFailure(FailureOperation::CreateBacking, rc, -1, source, size, offset);
        if (source && R_SUCCEEDED(rc))
            svcUnmapProcessCodeMemory(s_process, uintptr_t(source), uintptr_t(backing), size);
        free(backing);
        return false;
    }

    auto map = [&](size_t destination, size_t sourceOffset, size_t length, int32_t view) {
        rc = svcMapProcessMemory(s_base + destination, s_process,
            uintptr_t(source) + sourceOffset, length);
        if (R_FAILED(rc))
            RecordFailure(FailureOperation::MapView, rc, view, s_base + destination, length, offset);
        return R_SUCCEEDED(rc);
    };

    bool ok = map(offset, 0, size, offset < kStarts[1] ? 0 : 1);
    if (ok && offset >= kStarts[1] && offset < kStarts[2])
    {
        const size_t physical = offset - kStarts[1];
        ok = map(kStarts[2] + physical, 0, size, 2);
        // E: guest E+y reads backing y+0x1000. The first page of the first
        // chunk has no E address (it would fall into C).
        if (ok)
        {
            const size_t skip = physical == 0 ? kPage : 0;
            ok = map(kStarts[3] + physical + skip - kPage, skip, size - skip, 3);
        }
    }
    // Mapped views are never released: guest memory lives for the whole run.
    return ok;
}
}

uint8_t* Allocate()
{
    ClearFailure();
    if (!ProcessMemorySyscallsAvailable())
    {
        // Typical cause: started from the album applet. Logged by main.cpp.
        RecordFailure(FailureOperation::ReserveAny, 0xFFFFFFFFu, -1, nullptr, kSize);
        return nullptr;
    }
    s_process = OwnProcessHandle();
    if (s_process == INVALID_HANDLE)
    {
        RecordFailure(FailureOperation::ReserveAny, 0xFFFFFFFEu, -1, nullptr, kSize);
        return nullptr;
    }

    virtmemLock();
    s_base = static_cast<uint8_t*>(virtmemFindAslr(kSize, kChunk));
    if (s_base)
        s_reservation = virtmemAddReservation(s_base, kSize);
    virtmemUnlock();
    if (!s_base || !s_reservation)
    {
        RecordFailure(FailureOperation::ReserveAny, 0, -1, s_base, kSize);
        s_base = nullptr;
        return nullptr;
    }
    // The guest null page stays unmapped: chunk 0 is committed from 0x1000.
    return s_base;
}

void Release(uint8_t* base)
{
    // The process exits with its mappings; nothing to undo early.
    (void)base;
}

bool Commit(uint32_t address, uint32_t size)
{
    if (!s_base || size == 0)
        return s_base != nullptr;
    const uint64_t end = uint64_t(address) + size;
    if (end > kStarts[2])
        return false; // C and E are aliases of A and are committed with it.

    std::lock_guard lock(s_mutex);
    for (uint64_t chunk = address / kChunk; chunk * kChunk < end; ++chunk)
    {
        if (s_committed[chunk])
            continue;
        size_t offset = size_t(chunk * kChunk);
        size_t length = kChunk;
        if (offset == 0)
        {
            // Keep the guest null page unmapped so a null access faults.
            offset = kPage;
            length -= kPage;
        }
        if (!MapChunk(offset, length))
            return false;
        s_committed[chunk] = true;
        s_committedBytes += kChunk;
    }
    return true;
}

size_t CommittedBytes()
{
    std::lock_guard lock(s_mutex);
    return s_committedBytes;
}
}
#endif
