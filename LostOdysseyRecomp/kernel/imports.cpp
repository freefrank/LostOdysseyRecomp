#include <stdafx.h>
#include <cpu/ppc_context.h>
#include <cpu/guest_thread.h>
#include "function.h"
#include "xbox.h"
#include "heap.h"
#include "memory.h"
#include "xam.h"
#include "xdm.h"
#include "xex_loader.h"
#include "guest_printf.h"
#include <os/logger.h>
#include <csetjmp>

// Kernel HLE for xboxkrnl.exe / xam.xex imports. Reference behaviour: Xenia
// (BSD-3) kernel/xboxkrnl; structure follows UnleashedRecomp (GPLv3).
// Anything not implemented here gets a logging stub from
// tools/gen_import_stubs.py (kernel/imports_stubs.cpp).

// ---------------------------------------------------------------------------
// Dispatcher objects
// ---------------------------------------------------------------------------

struct Event final : KernelObject, HostObject<XKEVENT>
{
    bool manualReset;
    std::atomic<bool> signaled;

    Event(XKEVENT* header) : manualReset(!header->Type), signaled(!!header->SignalState) {}
    Event(bool manualReset, bool initialState) : manualReset(manualReset), signaled(initialState) {}

    uint32_t Wait(uint32_t timeout) override
    {
        if (timeout == 0)
        {
            if (manualReset)
                return signaled ? STATUS_SUCCESS : STATUS_TIMEOUT;

            bool expected = true;
            return signaled.compare_exchange_strong(expected, false) ? STATUS_SUCCESS : STATUS_TIMEOUT;
        }

        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
        while (true)
        {
            if (manualReset)
            {
                if (signaled)
                    return STATUS_SUCCESS;
            }
            else
            {
                bool expected = true;
                if (signaled.compare_exchange_weak(expected, false))
                    return STATUS_SUCCESS;
            }

            if (timeout == INFINITE)
            {
                signaled.wait(false);
            }
            else
            {
                if (std::chrono::steady_clock::now() >= deadline)
                    return STATUS_TIMEOUT;
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        }
    }

    bool Set()
    {
        signaled = true;
        signaled.notify_all();
        return TRUE;
    }

    bool Reset()
    {
        signaled = false;
        return TRUE;
    }
};

static std::atomic<uint32_t> g_keSetEventGeneration;

struct Semaphore final : KernelObject, HostObject<XKSEMAPHORE>
{
    std::atomic<uint32_t> count;
    uint32_t maximumCount;

    Semaphore(XKSEMAPHORE* semaphore) : count(semaphore->Header.SignalState), maximumCount(semaphore->Limit) {}
    Semaphore(uint32_t count, uint32_t maximumCount) : count(count), maximumCount(maximumCount) {}

    uint32_t Wait(uint32_t timeout) override
    {
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
        while (true)
        {
            uint32_t current = count.load();
            if (current != 0)
            {
                if (count.compare_exchange_weak(current, current - 1))
                    return STATUS_SUCCESS;
                continue;
            }

            if (timeout == 0)
                return STATUS_TIMEOUT;

            if (timeout == INFINITE)
            {
                count.wait(0);
            }
            else
            {
                if (std::chrono::steady_clock::now() >= deadline)
                    return STATUS_TIMEOUT;
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        }
    }

    void Release(uint32_t releaseCount, uint32_t* previousCount)
    {
        if (previousCount != nullptr)
            *previousCount = count;

        count += releaseCount;
        count.notify_all();
    }
};

// Mutant (NtCreateMutant): recursive, owner tracked by guest thread block.
struct Mutant final : KernelObject, HostObject<XDISPATCHER_HEADER>
{
    std::atomic<uint32_t> owner{ 0 };
    uint32_t recursion = 0;

    Mutant(XDISPATCHER_HEADER*) {}
    Mutant(bool initialOwner)
    {
        if (initialOwner)
        {
            owner = g_ppcContext->r13.u32;
            recursion = 1;
        }
    }

    uint32_t Wait(uint32_t timeout) override
    {
        uint32_t self = g_ppcContext->r13.u32;
        auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
        while (true)
        {
            uint32_t expected = 0;
            if (owner.compare_exchange_weak(expected, self) || expected == self)
            {
                recursion++;
                return STATUS_SUCCESS;
            }

            if (timeout == 0)
                return STATUS_TIMEOUT;
            if (timeout == INFINITE)
                owner.wait(expected);
            else
            {
                if (std::chrono::steady_clock::now() >= deadline)
                    return STATUS_TIMEOUT;
                std::this_thread::sleep_for(std::chrono::microseconds(200));
            }
        }
    }

    void Release()
    {
        if (--recursion == 0)
        {
            owner.store(0);
            owner.notify_all();
        }
    }
};

static inline void CloseKernelObject(XDISPATCHER_HEADER& header)
{
    if (header.WaitListHead.Flink != OBJECT_SIGNATURE)
        return;
    DestroyKernelObject(header.WaitListHead.Blink);
}

// Timeouts are negative 100 ns intervals (relative) or absolute times.
static uint32_t GuestTimeoutToMilliseconds(be<int64_t>* timeout)
{
    if (!timeout)
        return INFINITE;
    int64_t t = *timeout;
    if (t >= 0)
        return t == 0 ? 0 : INFINITE; // absolute times: treat as infinite
    return uint32_t((-t) / 10000);
}

static KernelObject* ResolveWaitObject(XDISPATCHER_HEADER* header)
{
    switch (header->Type)
    {
    case 0: case 1: return QueryKernelObject<Event>(*header);
    case 2: return QueryKernelObject<Mutant>(*header);
    case 5: return QueryKernelObject<Semaphore>(*header);
    case 6: // thread
        return TryQueryKernelObject<GuestThreadHandle>(*header);
    default:
        LOG_KERNEL("unknown dispatcher type {}", header->Type);
        return nullptr;
    }
}

void KernelSignalEventHandle(uint32_t handle)
{
    if (handle == 0 || handle == GUEST_INVALID_HANDLE_VALUE)
        return;
    if (IsKernelObject(handle))
        static_cast<Event*>(GetKernelObject(handle))->Set();
}

// ---------------------------------------------------------------------------
// Events / semaphores / mutants
// ---------------------------------------------------------------------------

static uint32_t NtCreateEvent(be<uint32_t>* handle, void* objAttributes, uint32_t eventType, uint32_t initialState)
{
    *handle = GetKernelHandle(CreateKernelObject<Event>(!eventType, !!initialState));
    return STATUS_SUCCESS;
}

static uint32_t NtSetEvent(uint32_t handle, be<uint32_t>* previousState)
{
    if (!IsKernelObject(handle))
        return STATUS_INVALID_HANDLE;
    auto* ev = static_cast<Event*>(GetKernelObject(handle));
    if (previousState)
        *previousState = ev->signaled ? 1 : 0;
    ev->Set();
    ++g_keSetEventGeneration;
    g_keSetEventGeneration.notify_all();
    return STATUS_SUCCESS;
}

static uint32_t NtPulseEvent(uint32_t handle, be<uint32_t>* previousState)
{
    if (!IsKernelObject(handle))
        return STATUS_INVALID_HANDLE;
    auto* ev = static_cast<Event*>(GetKernelObject(handle));
    if (previousState)
        *previousState = ev->signaled ? 1 : 0;
    ev->Set();
    ++g_keSetEventGeneration;
    g_keSetEventGeneration.notify_all();
    std::this_thread::yield();
    ev->Reset();
    return STATUS_SUCCESS;
}

static uint32_t NtClearEvent(uint32_t handle)
{
    if (!IsKernelObject(handle))
        return STATUS_INVALID_HANDLE;
    static_cast<Event*>(GetKernelObject(handle))->Reset();
    return STATUS_SUCCESS;
}

static bool KeSetEvent(XKEVENT* pEvent, uint32_t Increment, bool Wait)
{
    bool result = QueryKernelObject<Event>(*pEvent)->Set();
    ++g_keSetEventGeneration;
    g_keSetEventGeneration.notify_all();
    return result;
}

static bool KeResetEvent(XKEVENT* pEvent)
{
    return QueryKernelObject<Event>(*pEvent)->Reset();
}

static uint32_t KeWaitForSingleObject(XDISPATCHER_HEADER* Object, uint32_t WaitReason, uint32_t WaitMode, bool Alertable, be<int64_t>* Timeout)
{
    const uint32_t timeout = GuestTimeoutToMilliseconds(Timeout);
    auto* obj = ResolveWaitObject(Object);
    if (!obj)
        return STATUS_TIMEOUT;
    return obj->Wait(timeout);
}

static uint32_t KeWaitForMultipleObjects(uint32_t Count, xpointer<XDISPATCHER_HEADER>* Objects, uint32_t WaitType, uint32_t WaitReason, uint32_t WaitMode, uint32_t Alertable, be<int64_t>* Timeout)
{
    const uint32_t timeout = GuestTimeoutToMilliseconds(Timeout);

    if (WaitType == 0) // wait all
    {
        for (size_t i = 0; i < Count; i++)
            if (auto* obj = ResolveWaitObject(Objects[i]))
                obj->Wait(timeout);
        return STATUS_SUCCESS;
    }

    std::vector<KernelObject*> objs(Count);
    for (size_t i = 0; i < Count; i++)
        objs[i] = ResolveWaitObject(Objects[i]);

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    while (true)
    {
        uint32_t generation = g_keSetEventGeneration.load();
        for (size_t i = 0; i < Count; i++)
            if (objs[i] && objs[i]->Wait(0) == STATUS_SUCCESS)
                return STATUS_WAIT_0 + uint32_t(i);

        if (timeout == 0)
            return STATUS_TIMEOUT;
        if (timeout != INFINITE && std::chrono::steady_clock::now() >= deadline)
            return STATUS_TIMEOUT;

        // Wake on any event change, with a short timeout so semaphores and
        // mutants (which don't bump the generation) are polled too.
        for (int spins = 0; spins < 5 && g_keSetEventGeneration.load() == generation; spins++)
            std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}

static uint32_t NtWaitForSingleObjectEx(uint32_t Handle, uint32_t WaitMode, uint32_t Alertable, be<int64_t>* Timeout)
{
    uint32_t timeout = GuestTimeoutToMilliseconds(Timeout);
    if (Handle == CURRENT_THREAD_HANDLE)
        return STATUS_TIMEOUT;
    if (IsKernelObject(Handle))
        return GetKernelObject(Handle)->Wait(timeout);

    LOG_KERNEL("unrecognized handle {:#x}", Handle);
    return STATUS_INVALID_HANDLE;
}

static uint32_t NtWaitForMultipleObjectsEx(uint32_t Count, be<uint32_t>* Handles, uint32_t WaitType, uint32_t WaitMode, uint32_t Alertable, be<int64_t>* Timeout)
{
    const uint32_t timeout = GuestTimeoutToMilliseconds(Timeout);
    std::vector<KernelObject*> objs(Count, nullptr);
    for (size_t i = 0; i < Count; i++)
        if (IsKernelObject(Handles[i]))
            objs[i] = GetKernelObject(Handles[i]);

    if (WaitType == 0)
    {
        for (auto* o : objs)
            if (o) o->Wait(timeout);
        return STATUS_SUCCESS;
    }

    auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
    while (true)
    {
        for (size_t i = 0; i < Count; i++)
            if (objs[i] && objs[i]->Wait(0) == STATUS_SUCCESS)
                return STATUS_WAIT_0 + uint32_t(i);
        if (timeout == 0)
            return STATUS_TIMEOUT;
        if (timeout != INFINITE && std::chrono::steady_clock::now() >= deadline)
            return STATUS_TIMEOUT;
        std::this_thread::sleep_for(std::chrono::microseconds(200));
    }
}

static uint32_t NtCreateSemaphore(be<uint32_t>* Handle, XOBJECT_ATTRIBUTES* ObjectAttributes, uint32_t InitialCount, uint32_t MaximumCount)
{
    *Handle = GetKernelHandle(CreateKernelObject<Semaphore>(InitialCount, MaximumCount));
    return STATUS_SUCCESS;
}

static uint32_t NtReleaseSemaphore(uint32_t Handle, uint32_t ReleaseCount, be<int32_t>* PreviousCount)
{
    if (!IsKernelObject(Handle))
        return STATUS_INVALID_HANDLE;
    uint32_t previousCount;
    static_cast<Semaphore*>(GetKernelObject(Handle))->Release(ReleaseCount, &previousCount);
    if (PreviousCount != nullptr)
        *PreviousCount = int32_t(previousCount);
    return STATUS_SUCCESS;
}

static void KeInitializeSemaphore(XKSEMAPHORE* semaphore, uint32_t count, uint32_t limit)
{
    semaphore->Header.Type = 5;
    semaphore->Header.SignalState = count;
    semaphore->Limit = limit;
    QueryKernelObject<Semaphore>(semaphore->Header);
}

static uint32_t KeReleaseSemaphore(XKSEMAPHORE* semaphore, uint32_t increment, uint32_t adjustment, uint32_t wait)
{
    auto* object = QueryKernelObject<Semaphore>(semaphore->Header);
    uint32_t previous;
    object->Release(adjustment, &previous);
    return previous;
}

static uint32_t NtCreateMutant(be<uint32_t>* Handle, XOBJECT_ATTRIBUTES* ObjectAttributes, uint32_t InitialOwner)
{
    *Handle = GetKernelHandle(CreateKernelObject<Mutant>(InitialOwner != 0));
    return STATUS_SUCCESS;
}

static uint32_t NtReleaseMutant(uint32_t Handle, be<int32_t>* PreviousCount)
{
    if (!IsKernelObject(Handle))
        return STATUS_INVALID_HANDLE;
    static_cast<Mutant*>(GetKernelObject(Handle))->Release();
    if (PreviousCount)
        *PreviousCount = 0;
    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// Critical sections / spin locks / TLS
// ---------------------------------------------------------------------------

static uint32_t RtlInitializeCriticalSection(XRTL_CRITICAL_SECTION* cs)
{
    cs->Header.Absolute = 0;
    cs->LockCount = -1;
    cs->RecursionCount = 0;
    cs->OwningThread = 0;
    return STATUS_SUCCESS;
}

static void RtlInitializeCriticalSectionAndSpinCount(XRTL_CRITICAL_SECTION* cs, uint32_t spinCount)
{
    cs->Header.Absolute = (spinCount + 255) >> 8;
    cs->LockCount = -1;
    cs->RecursionCount = 0;
    cs->OwningThread = 0;
}

static void RtlEnterCriticalSection(XRTL_CRITICAL_SECTION* cs)
{
    uint32_t thisThread = g_ppcContext->r13.u32;
    std::atomic_ref owningThread(cs->OwningThread);

    while (true)
    {
        uint32_t previousOwner = 0;
        if (owningThread.compare_exchange_weak(previousOwner, thisThread) || previousOwner == thisThread)
        {
            cs->RecursionCount++;
            return;
        }
        owningThread.wait(previousOwner);
    }
}

static bool RtlTryEnterCriticalSection(XRTL_CRITICAL_SECTION* cs)
{
    uint32_t thisThread = g_ppcContext->r13.u32;
    std::atomic_ref owningThread(cs->OwningThread);
    uint32_t previousOwner = 0;
    if (owningThread.compare_exchange_weak(previousOwner, thisThread) || previousOwner == thisThread)
    {
        cs->RecursionCount++;
        return true;
    }
    return false;
}

static void RtlLeaveCriticalSection(XRTL_CRITICAL_SECTION* cs)
{
    cs->RecursionCount--;
    if (cs->RecursionCount != 0)
        return;
    std::atomic_ref owningThread(cs->OwningThread);
    owningThread.store(0);
    owningThread.notify_one();
}

static void KfAcquireSpinLock(uint32_t* spinLock)
{
    std::atomic_ref ref(*spinLock);
    while (true)
    {
        uint32_t expected = 0;
        if (ref.compare_exchange_weak(expected, g_ppcContext->r13.u32))
            break;
        std::this_thread::yield();
    }
}

static void KfReleaseSpinLock(uint32_t* spinLock)
{
    std::atomic_ref ref(*spinLock);
    ref = 0;
}

static uint32_t KeTryToAcquireSpinLockAtRaisedIrql(uint32_t* spinLock)
{
    std::atomic_ref ref(*spinLock);
    uint32_t expected = 0;
    return ref.compare_exchange_strong(expected, g_ppcContext->r13.u32) ? 1 : 0;
}

static std::vector<size_t> g_tlsFreeIndices;
static size_t g_tlsNextIndex = 0;
static Mutex g_tlsAllocationMutex;

static uint32_t& KeTlsGetValueRef(size_t index)
{
    thread_local std::vector<uint32_t> s_tlsValues;
    if (s_tlsValues.size() <= index)
        s_tlsValues.resize(index + 1, 0);
    return s_tlsValues[index];
}

static uint32_t KeTlsGetValue(uint32_t dwTlsIndex) { return KeTlsGetValueRef(dwTlsIndex); }
static uint32_t KeTlsSetValue(uint32_t dwTlsIndex, uint32_t lpTlsValue) { KeTlsGetValueRef(dwTlsIndex) = lpTlsValue; return TRUE; }

static uint32_t KeTlsAlloc()
{
    std::lock_guard<Mutex> lock(g_tlsAllocationMutex);
    if (!g_tlsFreeIndices.empty())
    {
        size_t index = g_tlsFreeIndices.back();
        g_tlsFreeIndices.pop_back();
        return uint32_t(index);
    }
    return uint32_t(g_tlsNextIndex++);
}

static uint32_t KeTlsFree(uint32_t dwTlsIndex)
{
    std::lock_guard<Mutex> lock(g_tlsAllocationMutex);
    g_tlsFreeIndices.push_back(dwTlsIndex);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Threads
// ---------------------------------------------------------------------------

static thread_local jmp_buf* t_terminateJump = nullptr;

static uint32_t ExCreateThread(be<uint32_t>* handle, uint32_t stackSize, be<uint32_t>* threadId, uint32_t xApiThreadStartup, uint32_t startAddress, uint32_t startContext, uint32_t creationFlags)
{
    LOG_KERNEL("stack={:#x} startup={:#x} start={:#x} ctx={:#x} flags={:#x}", stackSize, xApiThreadStartup, startAddress, startContext, creationFlags);

    GuestThreadParams params{};
    if (xApiThreadStartup != 0)
    {
        params.function = xApiThreadStartup;
        params.value = startAddress;
        params.value2 = startContext;
    }
    else
    {
        params.function = startAddress;
        params.value = startContext;
    }
    params.flags = creationFlags;

    uint32_t hostThreadId;
    auto* hThread = GuestThread::Start(params, &hostThreadId);
    *handle = GetKernelHandle(hThread);
    if (threadId != nullptr)
        *threadId = hostThreadId;
    return STATUS_SUCCESS;
}

static void ExTerminateThread(uint32_t exitCode)
{
    LOG_KERNEL("exit code {:#x}", exitCode);
    if (t_terminateJump)
        longjmp(*t_terminateJump, 1);
    std::_Exit(int(exitCode));
}

void GuestThreadRunWithTerminateHook(void (*run)(void*), void* arg)
{
    jmp_buf jb;
    t_terminateJump = &jb;
    if (setjmp(jb) == 0)
        run(arg);
    t_terminateJump = nullptr;
}

static GuestThreadHandle* ThreadFromHandle(uint32_t handle)
{
    if (handle == CURRENT_THREAD_HANDLE || !IsKernelObject(handle))
        return nullptr;
    return static_cast<GuestThreadHandle*>(GetKernelObject(handle));
}

static void KeSetBasePriorityThread(uint32_t thread, int priority)
{
#ifdef _WIN32
    if (priority == 16) priority = 15;
    else if (priority == -16) priority = -15;
    auto* t = ThreadFromHandle(thread);
    SetThreadPriority(t ? t->thread.native_handle() : GetCurrentThread(), priority);
#endif
}

static int KeQueryBasePriorityThread(uint32_t thread)
{
#ifdef _WIN32
    auto* t = ThreadFromHandle(thread);
    return GetThreadPriority(t ? t->thread.native_handle() : GetCurrentThread());
#else
    return 0;
#endif
}

static uint32_t KeSetAffinityThread(uint32_t Thread, uint32_t Affinity, be<uint32_t>* lpPreviousAffinity)
{
    if (lpPreviousAffinity)
        *lpPreviousAffinity = 2;
    return 0;
}

static uint32_t NtSuspendThread(uint32_t handle, be<uint32_t>* suspendCount)
{
    auto* t = ThreadFromHandle(handle);
    if (!t)
        return STATUS_INVALID_HANDLE;
    t->suspended = true;
    if (suspendCount) *suspendCount = 0;
    return STATUS_SUCCESS;
}

static uint32_t NtResumeThread(uint32_t handle, be<uint32_t>* suspendCount)
{
    auto* t = ThreadFromHandle(handle);
    if (!t)
        return STATUS_INVALID_HANDLE;
    t->suspended = false;
    t->suspended.notify_all();
    if (suspendCount) *suspendCount = 1;
    return STATUS_SUCCESS;
}

static uint32_t KeResumeThread(uint32_t handle)
{
    auto* t = ThreadFromHandle(handle);
    if (!t)
        return 0;
    t->suspended = false;
    t->suspended.notify_all();
    return 1;
}

static uint32_t KeDelayExecutionThread(uint32_t WaitMode, bool Alertable, be<int64_t>* Timeout)
{
    uint32_t timeout = GuestTimeoutToMilliseconds(Timeout);
    if (timeout == 0)
        std::this_thread::yield();
    else if (timeout != INFINITE)
        std::this_thread::sleep_for(std::chrono::milliseconds(timeout));
    else
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    return Alertable ? STATUS_USER_APC : STATUS_SUCCESS;
}

static uint32_t ObReferenceObjectByHandle(uint32_t handle, uint32_t objectType, be<uint32_t>* object)
{
    *object = handle;
    return STATUS_SUCCESS;
}

static void ObDereferenceObject(uint32_t object) {}
static void ObReferenceObject(uint32_t object) {}

static uint32_t NtClose(uint32_t handle)
{
    if (handle == GUEST_INVALID_HANDLE_VALUE || handle == 0)
        return STATUS_INVALID_HANDLE;
    if (IsKernelObject(handle))
    {
        DestroyKernelObject(handle);
        return STATUS_SUCCESS;
    }
    LOG_KERNEL("unrecognized handle {:#x}", handle);
    return STATUS_INVALID_HANDLE;
}

static uint32_t NtDuplicateObject(uint32_t handle, be<uint32_t>* newHandle, uint32_t options)
{
    if (newHandle)
        *newHandle = handle;
    return STATUS_SUCCESS;
}

// ---------------------------------------------------------------------------
// Memory
// ---------------------------------------------------------------------------

constexpr uint32_t X_MEM_COMMIT = 0x1000;
constexpr uint32_t X_MEM_RESERVE = 0x2000;
constexpr uint32_t X_MEM_DECOMMIT = 0x4000;
constexpr uint32_t X_MEM_RELEASE = 0x8000;
constexpr uint32_t X_MEM_FREE = 0x10000;
constexpr uint32_t X_MEM_PRIVATE = 0x20000;
constexpr uint32_t X_MEM_LARGE_PAGES = 0x20000000;

static uint32_t NtAllocateVirtualMemory(be<uint32_t>* baseAddressPtr, be<uint32_t>* regionSizePtr, uint32_t allocationType, uint32_t protect, uint32_t unknown)
{
    uint32_t baseAddress = baseAddressPtr ? uint32_t(*baseAddressPtr) : 0;
    uint32_t regionSize = regionSizePtr ? uint32_t(*regionSizePtr) : 0;

    const uint32_t pageSize = (allocationType & X_MEM_LARGE_PAGES) ? 0x10000 : 0x1000;
    uint32_t alignedBase = baseAddress & ~(pageSize - 1);
    uint32_t alignedSize = RoundUp(regionSize + (baseAddress - alignedBase), pageSize);

    if (alignedSize == 0)
        return STATUS_INVALID_PARAMETER;

    // Commit inside an existing reservation: nothing to do, memory is always
    // backed. Keep the caller's address so pointers into the reserved range
    // stay valid.
    if (alignedBase != 0)
    {
        uint32_t existingBase, existingSize;
        if (g_pageAllocator.FindAllocation(g_pageAllocator.virtualRegion, alignedBase, existingBase, existingSize))
        {
            *baseAddressPtr = alignedBase;
            *regionSizePtr = std::min(alignedSize, existingBase + existingSize - alignedBase);
            return STATUS_SUCCESS;
        }
    }

    uint32_t address = g_pageAllocator.Alloc(g_pageAllocator.virtualRegion, alignedSize, pageSize, alignedBase);
    if (address == 0 && alignedBase != 0)
    {
        // Exact address unavailable: fall back to any address (the game
        // usually only cares for reserve+commit pairs which we handled above).
        address = g_pageAllocator.Alloc(g_pageAllocator.virtualRegion, alignedSize, pageSize, 0);
    }

    if (address == 0)
    {
        LOG_ERROR("out of guest virtual memory (size {:#x})", alignedSize);
        return STATUS_NO_MEMORY;
    }

    *baseAddressPtr = address;
    *regionSizePtr = alignedSize;
    LOG_KERNEL("base={:#x} size={:#x} type={:#x} -> {:#x}", baseAddress, regionSize, allocationType, address);
    return STATUS_SUCCESS;
}

static uint32_t NtFreeVirtualMemory(be<uint32_t>* baseAddressPtr, be<uint32_t>* regionSizePtr, uint32_t freeType, uint32_t unknown)
{
    uint32_t baseAddress = baseAddressPtr ? uint32_t(*baseAddressPtr) : 0;
    uint32_t regionSize = regionSizePtr ? uint32_t(*regionSizePtr) : 0;

    if (freeType & X_MEM_DECOMMIT)
    {
        // Keep the pages; commit/decommit is a no-op for us unless the whole
        // allocation is decommitted, which behaves like release.
        if (regionSize != 0 && regionSize < g_pageAllocator.AllocationSize(g_pageAllocator.virtualRegion, baseAddress))
            return STATUS_SUCCESS;
    }

    if (!g_pageAllocator.Free(g_pageAllocator.virtualRegion, baseAddress & ~0xFFFu))
    {
        LOG_KERNEL("free of unknown region {:#x}", baseAddress);
        return STATUS_SUCCESS;
    }
    LOG_KERNEL("base={:#x} size={:#x} type={:#x}", baseAddress, regionSize, freeType);
    return STATUS_SUCCESS;
}

struct X_MEMORY_BASIC_INFORMATION
{
    be<uint32_t> baseAddress;
    be<uint32_t> allocationBase;
    be<uint32_t> allocationProtect;
    be<uint32_t> regionSize;
    be<uint32_t> state;
    be<uint32_t> protect;
    be<uint32_t> type;
};

static uint32_t NtQueryVirtualMemory(uint32_t baseAddress, X_MEMORY_BASIC_INFORMATION* info)
{
    uint32_t page = baseAddress & ~0xFFFu;
    info->baseAddress = page;
    info->allocationBase = page;
    info->allocationProtect = PAGE_READWRITE;
    info->regionSize = 0x1000;
    info->protect = PAGE_READWRITE;
    info->type = X_MEM_PRIVATE;

    bool committed = (page >= XexLoader::s_imageBase && page < XexLoader::s_imageBase + XexLoader::s_imageSize) ||
        IsKernelObject(page) ||
        (g_pageAllocator.Contains(g_pageAllocator.virtualRegion, page) && g_pageAllocator.virtualRegion.used[(page - g_pageAllocator.virtualRegion.begin) / 0x1000]) ||
        (g_pageAllocator.Contains(g_pageAllocator.physicalRegion, page) && g_pageAllocator.physicalRegion.used[(page - g_pageAllocator.physicalRegion.begin) / 0x1000]);
    info->state = committed ? X_MEM_COMMIT : X_MEM_FREE;
    return STATUS_SUCCESS;
}

static uint32_t MmAllocatePhysicalMemoryEx(uint32_t flags, uint32_t size, uint32_t protect, uint32_t minAddress, uint32_t maxAddress, uint32_t alignment)
{
    uint32_t address = g_pageAllocator.Alloc(g_pageAllocator.physicalRegion, size, alignment ? alignment : 0x1000);
    LOG_KERNEL("size={:#x} align={:#x} -> {:#x}", size, alignment, address);
    return address;
}

static void MmFreePhysicalMemory(uint32_t type, uint32_t guestAddress)
{
    if (guestAddress != 0)
        g_pageAllocator.Free(g_pageAllocator.physicalRegion, guestAddress);
}

static uint32_t MmGetPhysicalAddress(uint32_t address)
{
    uint32_t physical = address & 0x1FFFFFFF;
    if (address >= 0xE0000000)
        physical += 0x1000;
    return physical;
}

static uint32_t MmQueryAddressProtect(uint32_t guestAddress) { return PAGE_READWRITE; }
static uint32_t MmSetAddressProtect(uint32_t guestAddress, uint32_t size, uint32_t protect) { return 0; }

struct X_MM_QUERY_STATISTICS_SECTION
{
    be<uint32_t> availablePages;
    be<uint32_t> totalVirtualMemoryBytes;
    be<uint32_t> reservedVirtualMemoryBytes;
    be<uint32_t> physicalPages;
    be<uint32_t> poolPages;
    be<uint32_t> stackPages;
    be<uint32_t> imagePages;
    be<uint32_t> heapPages;
    be<uint32_t> virtualPages;
    be<uint32_t> pageTablePages;
    be<uint32_t> cachePages;
};

struct X_MM_QUERY_STATISTICS_RESULT
{
    be<uint32_t> length;
    be<uint32_t> totalPhysicalPages;
    be<uint32_t> kernelPages;
    X_MM_QUERY_STATISTICS_SECTION title;
    X_MM_QUERY_STATISTICS_SECTION system;
    be<uint32_t> highestPhysicalPage;
};

static uint32_t MmQueryStatistics(X_MM_QUERY_STATISTICS_RESULT* stats)
{
    if (!stats || stats->length != sizeof(X_MM_QUERY_STATISTICS_RESULT))
        return 0xC0000023; // STATUS_BUFFER_TOO_SMALL

    memset(reinterpret_cast<uint8_t*>(stats) + 4, 0, sizeof(*stats) - 4);
    stats->totalPhysicalPages = 0x00020000; // 512 MiB
    stats->kernelPages = 0x300;
    stats->title.availablePages = 0x00013000;
    stats->title.totalVirtualMemoryBytes = 0x2FFF0000;
    stats->title.reservedVirtualMemoryBytes = 0x00160000;
    stats->title.physicalPages = 0x00001000;
    stats->title.poolPages = 0x100;
    stats->title.stackPages = 0x100;
    stats->title.imagePages = XexLoader::s_imageSize / 0x1000;
    stats->title.heapPages = 0x100;
    stats->title.virtualPages = 0x100;
    stats->title.pageTablePages = 0x100;
    stats->title.cachePages = 0x100;
    stats->highestPhysicalPage = 0x0001FFFF;
    return STATUS_SUCCESS;
}

static uint32_t ExAllocatePoolTypeWithTag(uint32_t size, uint32_t tag, uint32_t type)
{
    void* ptr = g_userHeap.Alloc(size);
    memset(ptr, 0, size);
    return g_memory.MapVirtual(ptr);
}

static uint32_t ExAllocatePool(uint32_t size)
{
    return ExAllocatePoolTypeWithTag(size, 0, 0);
}

static void ExFreePool(uint32_t address)
{
    if (address)
        g_userHeap.Free(g_memory.Translate(address));
}

// Interlocked singly linked lists (SLIST_HEADER: Next.Next, Depth:16, Sequence:16).
static uint32_t InterlockedPopEntrySList_x(be<uint32_t>* header)
{
    std::lock_guard lock(g_kernelLock);
    uint32_t first = header[0];
    if (first == 0)
        return 0;
    auto* entry = reinterpret_cast<be<uint32_t>*>(g_memory.Translate(first));
    header[0] = entry[0];
    uint32_t depthSeq = header[1];
    header[1] = ((depthSeq - 1) & 0xFFFF) | ((depthSeq + 0x10000) & 0xFFFF0000);
    return first;
}

static uint32_t InterlockedFlushSList_x(be<uint32_t>* header)
{
    std::lock_guard lock(g_kernelLock);
    uint32_t first = header[0];
    header[0] = 0;
    header[1] = (uint32_t(header[1]) + 0x10000) & 0xFFFF0000;
    return first;
}

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------

static void KeQuerySystemTime(be<uint64_t>* time)
{
    constexpr int64_t FILETIME_EPOCH_DIFFERENCE = 116444736000000000LL;
    auto now = std::chrono::system_clock::now().time_since_epoch();
    int64_t t = std::chrono::duration_cast<std::chrono::duration<int64_t, std::ratio<1, 10000000>>>(now).count();
    *time = uint64_t(t + FILETIME_EPOCH_DIFFERENCE);
}

static uint64_t KeQueryPerformanceFrequency()
{
    return 49875000;
}

static void RtlTimeToTimeFields(be<uint64_t>* time, XTIME_FIELDS* fields)
{
    constexpr int64_t FILETIME_EPOCH_DIFFERENCE = 116444736000000000LL;
    int64_t unix100ns = int64_t(uint64_t(*time)) - FILETIME_EPOCH_DIFFERENCE;
    time_t seconds = unix100ns / 10000000;
    tm t{};
#ifdef _WIN32
    gmtime_s(&t, &seconds);
#else
    gmtime_r(&seconds, &t);
#endif
    fields->Year = uint16_t(t.tm_year + 1900);
    fields->Month = uint16_t(t.tm_mon + 1);
    fields->Day = uint16_t(t.tm_mday);
    fields->Hour = uint16_t(t.tm_hour);
    fields->Minute = uint16_t(t.tm_min);
    fields->Second = uint16_t(t.tm_sec);
    fields->Milliseconds = uint16_t((unix100ns / 10000) % 1000);
    fields->Weekday = uint16_t(t.tm_wday);
}

static uint32_t RtlTimeFieldsToTime(XTIME_FIELDS* fields, be<uint64_t>* time)
{
    tm t{};
    t.tm_year = fields->Year - 1900;
    t.tm_mon = fields->Month - 1;
    t.tm_mday = fields->Day;
    t.tm_hour = fields->Hour;
    t.tm_min = fields->Minute;
    t.tm_sec = fields->Second;
#ifdef _WIN32
    time_t seconds = _mkgmtime(&t);
#else
    time_t seconds = timegm(&t);
#endif
    constexpr int64_t FILETIME_EPOCH_DIFFERENCE = 116444736000000000LL;
    *time = uint64_t(int64_t(seconds) * 10000000 + int64_t(fields->Milliseconds) * 10000 + FILETIME_EPOCH_DIFFERENCE);
    return TRUE;
}

// ---------------------------------------------------------------------------
// Strings
// ---------------------------------------------------------------------------

static void RtlInitAnsiString(XANSI_STRING* destination, char* source)
{
    const uint16_t length = source ? (uint16_t)strlen(source) : 0;
    destination->Length = length;
    destination->MaximumLength = length + 1;
    destination->Buffer = source;
}

struct XUNICODE_STRING
{
    be<uint16_t> Length;
    be<uint16_t> MaximumLength;
    xpointer<be<uint16_t>> Buffer;
};

static void RtlInitUnicodeString(XUNICODE_STRING* destination, be<uint16_t>* source)
{
    uint16_t length = 0;
    if (source)
        while (source[length] != 0) length++;
    destination->Length = length * 2;
    destination->MaximumLength = (length + 1) * 2;
    destination->Buffer = source;
}

static void RtlFreeAnsiString(XANSI_STRING* str)
{
    if (str && str->Buffer.get())
    {
        g_userHeap.Free(str->Buffer.get());
        str->Buffer = nullptr;
        str->Length = 0;
        str->MaximumLength = 0;
    }
}

static uint32_t RtlUnicodeStringToAnsiString(XANSI_STRING* dest, XUNICODE_STRING* src, uint32_t allocate)
{
    uint32_t length = src->Length / 2;
    if (allocate)
    {
        char* buf = (char*)g_userHeap.Alloc(length + 1);
        dest->Buffer = buf;
        dest->MaximumLength = uint16_t(length + 1);
    }
    char* out = dest->Buffer.get();
    uint32_t max = dest->MaximumLength;
    uint32_t n = std::min<uint32_t>(length, max ? max - 1 : 0);
    for (uint32_t i = 0; i < n; i++)
    {
        uint16_t c = src->Buffer.get()[i];
        out[i] = c < 256 ? char(c) : '?';
    }
    if (max)
        out[n] = 0;
    dest->Length = uint16_t(n);
    return STATUS_SUCCESS;
}

static uint32_t RtlUnicodeToMultiByteN(char* MultiByteString, uint32_t MaxBytesInMultiByteString, be<uint32_t>* BytesInMultiByteString, const be<uint16_t>* UnicodeString, uint32_t BytesInUnicodeString)
{
    const auto reqSize = BytesInUnicodeString / sizeof(uint16_t);
    uint32_t n = std::min<uint32_t>(uint32_t(reqSize), MaxBytesInMultiByteString);
    for (size_t i = 0; i < n; i++)
    {
        const auto c = UnicodeString[i].get();
        MultiByteString[i] = c < 256 ? char(c) : '?';
    }
    if (BytesInMultiByteString)
        *BytesInMultiByteString = n;
    return STATUS_SUCCESS;
}

static uint32_t RtlMultiByteToUnicodeN(be<uint16_t>* UnicodeString, uint32_t MaxBytesInUnicodeString, be<uint32_t>* BytesInUnicodeString, const char* MultiByteString, uint32_t BytesInMultiByteString)
{
    uint32_t length = std::min(MaxBytesInUnicodeString / 2, BytesInMultiByteString);
    for (size_t i = 0; i < length; i++)
        UnicodeString[i] = uint16_t(uint8_t(MultiByteString[i]));
    if (BytesInUnicodeString != nullptr)
        *BytesInUnicodeString = length * 2;
    return STATUS_SUCCESS;
}

static int32_t RtlCompareStringN(const char* a, uint32_t lengthA, const char* b, uint32_t lengthB, uint32_t caseInsensitive)
{
    if (lengthA == 0xFFFFFFFF) lengthA = uint32_t(strlen(a));
    if (lengthB == 0xFFFFFFFF) lengthB = uint32_t(strlen(b));
    uint32_t n = std::min(lengthA, lengthB);
    for (uint32_t i = 0; i < n; i++)
    {
        int ca = caseInsensitive ? toupper((unsigned char)a[i]) : (unsigned char)a[i];
        int cb = caseInsensitive ? toupper((unsigned char)b[i]) : (unsigned char)b[i];
        if (ca != cb)
            return ca - cb;
    }
    return int32_t(lengthA) - int32_t(lengthB);
}

static uint32_t RtlUpcaseUnicodeChar(uint32_t c)
{
    return c < 128 ? uint32_t(toupper(int(c))) : c;
}

static void RtlFillMemoryUlong(uint32_t* destination, uint32_t length, uint32_t pattern)
{
    uint32_t swapped = ByteSwap(pattern);
    for (uint32_t i = 0; i < length / 4; i++)
        destination[i] = swapped;
}

static uint32_t RtlCompareMemoryUlong(uint32_t* source, uint32_t length, uint32_t pattern)
{
    uint32_t swapped = ByteSwap(pattern);
    uint32_t i = 0;
    for (; i < length / 4; i++)
        if (source[i] != swapped)
            break;
    return i * 4;
}

static uint32_t RtlNtStatusToDosError(uint32_t status)
{
    switch (status)
    {
    case STATUS_SUCCESS: return ERROR_SUCCESS;
    case STATUS_OBJECT_NAME_NOT_FOUND: return 2; // ERROR_FILE_NOT_FOUND
    case STATUS_OBJECT_PATH_NOT_FOUND: return ERROR_PATH_NOT_FOUND;
    case STATUS_NO_SUCH_FILE: return 2;
    case STATUS_ACCESS_DENIED: return ERROR_ACCESS_DENIED;
    case STATUS_NO_MORE_FILES: return ERROR_NO_MORE_FILES;
    case STATUS_END_OF_FILE: return 38; // ERROR_HANDLE_EOF
    case STATUS_INVALID_HANDLE: return 6;
    case STATUS_INVALID_PARAMETER: return 87;
    case STATUS_NO_MEMORY: return 8;
    case STATUS_NOT_SUPPORTED: return ERROR_NOT_SUPPORTED;
    case STATUS_PENDING: return ERROR_IO_PENDING;
    case 0xC0000035: return 183; // ERROR_ALREADY_EXISTS
    default: return 317; // ERROR_MR_MID_NOT_FOUND
    }
}

// CRT exports: the format string and varargs live in guest registers/stack.
PPC_FUNC(__imp__sprintf)
{
    char* dest = reinterpret_cast<char*>(base + ctx.r3.u32);
    const char* format = reinterpret_cast<const char*>(base + ctx.r4.u32);
    std::string s = GuestFormat(ctx, base, format, 2);
    memcpy(dest, s.c_str(), s.size() + 1);
    ctx.r3.u64 = s.size();
}

PPC_FUNC(__imp___vsnprintf)
{
    char* dest = reinterpret_cast<char*>(base + ctx.r3.u32);
    uint32_t count = ctx.r4.u32;
    const char* format = reinterpret_cast<const char*>(base + ctx.r5.u32);
    std::string s = GuestFormatVaList(base, format, ctx.r6.u32);
    if (count == 0)
    {
        ctx.r3.u64 = s.size();
        return;
    }
    size_t n = std::min<size_t>(s.size(), count);
    memcpy(dest, s.data(), n);
    if (n < count)
        dest[n] = 0;
    ctx.r3.s64 = n < s.size() ? -1 : int64_t(n);
}

PPC_FUNC(__imp__vswprintf)
{
    auto* dest = reinterpret_cast<be<uint16_t>*>(base + ctx.r3.u32);
    const char* format = reinterpret_cast<const char*>(base + ctx.r4.u32);
    std::string s = GuestFormatVaList(base, format, ctx.r5.u32, true);
    for (size_t i = 0; i < s.size(); i++)
        dest[i] = uint16_t(uint8_t(s[i]));
    dest[s.size()] = 0;
    ctx.r3.u64 = s.size();
}

PPC_FUNC(__imp__DbgPrint)
{
    const char* format = reinterpret_cast<const char*>(base + ctx.r3.u32);
    std::string s = GuestFormat(ctx, base, format, 1);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r'))
        s.pop_back();
    LOG_IMPL(Info, "[DbgPrint] {}", s);
    ctx.r3.u64 = 0;
}

// ---------------------------------------------------------------------------
// Misc kernel
// ---------------------------------------------------------------------------

static void KeBugCheckEx(uint32_t code, uint32_t p1, uint32_t p2, uint32_t p3, uint32_t p4)
{
    LOG_ERROR("KeBugCheckEx {:#x} ({:#x} {:#x} {:#x} {:#x})", code, p1, p2, p3, p4);
    __builtin_debugtrap();
}

static void KeBugCheck(uint32_t code)
{
    LOG_ERROR("KeBugCheck {:#x}", code);
    __builtin_debugtrap();
}

static void DbgBreakPoint()
{
    LOG_WARNING("DbgBreakPoint");
}

static void HalReturnToFirmware(uint32_t routine)
{
    LOG_INFO("HalReturnToFirmware({}) - exiting", routine);
    std::_Exit(0);
}

static uint32_t KeGetCurrentProcessType() { return 1; }

static uint32_t ExGetXConfigSetting(uint16_t Category, uint16_t Setting, void* Buffer, uint16_t SizeOfBuffer, be<uint32_t>* RequiredSize)
{
    uint32_t data[4]{};
    uint32_t size = 4;

    switch (Category)
    {
    case 0x0002: // XCONFIG_SECURED_CATEGORY
        switch (Setting)
        {
        case 0x0002: data[0] = ByteSwap(0x00001000u); break; // AV region
        default: return STATUS_INVALID_PARAMETER;
        }
        break;
    case 0x0003: // XCONFIG_USER_CATEGORY
        switch (Setting)
        {
        case 0x0001: case 0x0002: case 0x0003: case 0x0004: case 0x0005: case 0x0006: case 0x0007:
            data[0] = 0; break; // time zone settings
        case 0x0009: data[0] = ByteSwap(1u); break;           // language: English (1). Japanese = 2
        case 0x000A: data[0] = ByteSwap(0x00040000u); break;  // video flags: widescreen, 720p
        case 0x000C: data[0] = ByteSwap(1u); break;           // retail flags
        case 0x000E: data[0] = ByteSwap(103u); break;         // country
        default: return STATUS_INVALID_PARAMETER;
        }
        break;
    default:
        return STATUS_INVALID_PARAMETER;
    }

    if (RequiredSize)
        *RequiredSize = size;
    memcpy(Buffer, data, std::min<size_t>(SizeOfBuffer, size));
    return STATUS_SUCCESS;
}

static uint32_t XexCheckExecutablePrivilege(uint32_t privilege)
{
    // Bit 0x1 = insecure sockets, 0x8 = ?, 0x200 = allow debug, etc. Claim everything.
    return 1;
}

static uint32_t XexGetModuleHandle(const char* name, be<uint32_t>* handle)
{
    LOG_KERNEL("'{}'", name ? name : "(null)");
    *handle = XexLoader::s_xexExecutableModuleHandle;
    return STATUS_SUCCESS;
}

static uint32_t XexGetModuleSection(uint32_t handle, const char* name, be<uint32_t>* data, be<uint32_t>* size)
{
    LOG_KERNEL("'{}'", name ? name : "(null)");
    return STATUS_NOT_IMPLEMENTED;
}

static uint32_t XexGetProcedureAddress(uint32_t handle, uint32_t ordinalOrName, be<uint32_t>* address)
{
    LOG_KERNEL("handle={:#x} ordinal={:#x}", handle, ordinalOrName);
    *address = 0;
    return STATUS_NOT_IMPLEMENTED;
}

static void ExRegisterTitleTerminateNotification(uint32_t registration, uint32_t create) {}
static void KeEnableFpuExceptions(uint32_t enabled) {}
static void KeLockL2() {}
static void KeUnlockL2() {}
static void KeEnterCriticalRegion() {}
static void KeLeaveCriticalRegion() {}
static uint32_t KeRaiseIrqlToDpcLevel() { return 0; }
static void KfLowerIrql(uint32_t) {}
static uint32_t KiApcNormalRoutineNop_x() { return 0; }
static uint32_t FscSetCacheElementCount_x(uint32_t, uint32_t) { return STATUS_SUCCESS; }

static void RtlRaiseException_x(uint32_t record)
{
    auto* r = reinterpret_cast<be<uint32_t>*>(g_memory.Translate(record));
    LOG_ERROR("RtlRaiseException_x code={:#x} address={:#x}", uint32_t(r[0]), uint32_t(r[3]));
    __builtin_debugtrap();
}

static void RtlUnwind_x(uint32_t frame, uint32_t target, uint32_t record, uint32_t value)
{
    LOG_ERROR("RtlUnwind_x frame={:#x} target={:#x} (unsupported)", frame, target);
    __builtin_debugtrap();
}

static void RtlCaptureContext_x(uint32_t context)
{
    LOG_KERNEL("context={:#x}", context);
}

static uint32_t __C_specific_handler_x()
{
    LOG_ERROR("__C_specific_handler_x reached");
    return 1; // ExceptionContinueSearch
}

static uint32_t RtlImageXexHeaderField(uint32_t header, uint32_t key)
{
    LOG_KERNEL("key={:#x}", key);
    return 0;
}

// Symbolic links (D: -> \Device\Cdrom0 etc.): recorded so ResolvePath can map them.
static uint32_t ObCreateSymbolicLink(XANSI_STRING* link, XANSI_STRING* target)
{
    std::string l(link->Buffer.get(), link->Length.get());
    std::string t(target->Buffer.get(), target->Length.get());
    LOG_KERNEL("'{}' -> '{}'", l, t);
    // "\??\D:" style links: register root name -> resolved target.
    std::string root = l;
    if (root.starts_with("\\??\\")) root = root.substr(4);
    if (!root.empty() && root.back() == ':') root.pop_back();
    std::transform(root.begin(), root.end(), root.begin(), [](unsigned char c) { return (char)tolower(c); });
    extern std::filesystem::path GetGamePath();
    if (t.find("Cdrom0") != std::string::npos)
        XamRootCreate(root, (const char*)GetGamePath().u8string().c_str());
    return STATUS_SUCCESS;
}

static uint32_t ObDeleteSymbolicLink(XANSI_STRING* link) { return STATUS_SUCCESS; }
static uint32_t ObIsTitleObject(uint32_t) { return 1; }

static uint32_t IoCreateDevice(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, be<uint32_t>* out) { if (out) *out = 0; return STATUS_NOT_IMPLEMENTED; }
static void IoDeleteDevice(uint32_t) {}
static void IoCompleteRequest(uint32_t, uint32_t) {}
static uint32_t IoInvalidDeviceRequest(uint32_t, uint32_t) { return STATUS_INVALID_DEVICE_REQUEST; }
static uint32_t IoCheckShareAccess(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) { return STATUS_SUCCESS; }
static void IoSetShareAccess(uint32_t, uint32_t, uint32_t, uint32_t) {}
static void IoRemoveShareAccess(uint32_t, uint32_t) {}
static uint32_t IoDismountVolume(uint32_t) { return STATUS_SUCCESS; }
static uint32_t IoDismountVolumeByFileHandle(uint32_t) { return STATUS_SUCCESS; }
static uint32_t StfsCreateDevice(uint32_t, uint32_t, uint32_t, uint32_t) { return STATUS_NOT_IMPLEMENTED; }
static uint32_t StfsControlDevice(uint32_t, uint32_t, uint32_t, uint32_t) { return STATUS_NOT_IMPLEMENTED; }

static void XeCryptSha(const uint8_t* in1, uint32_t len1, const uint8_t* in2, uint32_t len2, const uint8_t* in3, uint32_t len3, uint8_t* digest, uint32_t digestSize)
{
    // Not cryptographically relevant for us; zero digest keeps callers happy.
    memset(digest, 0, digestSize);
}
static uint32_t XeKeysConsolePrivateKeySign(uint32_t, uint32_t) { return 0; }
static uint32_t XeKeysConsoleSignatureVerification(uint32_t, uint32_t, uint32_t) { return 0; }

// ---------------------------------------------------------------------------
// Video (Vd*) - minimal until the GPU backend lands
// ---------------------------------------------------------------------------

static uint32_t g_graphicsInterruptCallback = 0;
static uint32_t g_graphicsInterruptUserData = 0;
static uint32_t g_ringBufferAddress = 0;
static uint32_t g_ringBufferSize = 0;

static void VdQueryVideoMode(XVIDEO_MODE* vm)
{
    memset(vm, 0, sizeof(XVIDEO_MODE));
    vm->DisplayWidth = 1280;
    vm->DisplayHeight = 720;
    vm->IsInterlaced = false;
    vm->IsWidescreen = true;
    vm->IsHighDefinition = true;
    vm->RefreshRate = 0x42700000; // 60.0f
    vm->VideoStandard = 1;
    vm->Unknown4A = 0x4A;
    vm->Unknown01 = 0x01;
}

static uint32_t VdQueryVideoFlags() { return 0x00000006; } // widescreen + HD
static uint32_t XGetVideoMode(XVIDEO_MODE* vm) { VdQueryVideoMode(vm); return 0; }
static uint32_t XGetAVPack() { return 0x00000006; } // HDMI
static uint32_t XGetGameRegion() { return 0x03FF; }
static uint32_t XGetLanguage() { return 1; }

static void VdGetCurrentDisplayInformation(uint32_t* info)
{
    // Xenia writes width/height pairs for front buffer etc.
    be<uint32_t>* p = reinterpret_cast<be<uint32_t>*>(info);
    p[0] = 1280; p[1] = 720;
    p[2] = 1280; p[3] = 720;
    p[4] = 1280; p[5] = 720;
}

static void VdGetCurrentDisplayGamma(be<uint32_t>* type, be<float>* power)
{
    if (type) *type = 1;
    if (power) *power = 2.22222233f;
}

static void VdSetGraphicsInterruptCallback(uint32_t callback, uint32_t userData)
{
    g_graphicsInterruptCallback = callback;
    g_graphicsInterruptUserData = userData;
    LOG_KERNEL("callback={:#x} data={:#x}", callback, userData);
}

static void VdInitializeRingBuffer(uint32_t address, uint32_t sizeLog2)
{
    g_ringBufferAddress = address;
    g_ringBufferSize = 1u << sizeLog2;
    LOG_KERNEL("ring buffer {:#x} size {:#x}", address, g_ringBufferSize);
}

static void VdEnableRingBufferRPtrWriteBack(uint32_t address, uint32_t blockSize)
{
    LOG_KERNEL("rptr writeback {:#x} block {:#x}", address, blockSize);
}

static void VdSetSystemCommandBufferGpuIdentifierAddress(uint32_t address)
{
    LOG_KERNEL("{:#x}", address);
}

static void VdGetSystemCommandBuffer(be<uint32_t>* buffer, be<uint32_t>* value)
{
    if (buffer) *buffer = 0;
    if (value) *value = 0;
}

static void VdInitializeEngines(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) { LOG_KERNEL("engines initialized"); }
static void VdShutdownEngines() {}
static uint32_t VdIsHSIOTrainingSucceeded() { return 1; }
static uint32_t VdRetrainEDRAM(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) { return 0; }
static void VdRetrainEDRAMWorker(uint32_t) {}
static bool VdPersistDisplay(uint32_t, be<uint32_t>* a2) { if (a2) *a2 = 0; return false; }
static void VdSetDisplayMode(uint32_t mode) { LOG_KERNEL("mode {:#x}", mode); }
static void VdEnableDisableClockGating(uint32_t) {}
static void VdCallGraphicsNotificationRoutines(uint32_t) {}
static void VdInitializeScalerCommandBuffer(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) {}

static void VdSwap(uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4, uint32_t a5, uint32_t a6, uint32_t a7, uint32_t a8)
{
    static uint32_t frame = 0;
    if ((frame++ % 60) == 0)
        LOG_KERNEL("frame {} ({:#x} {:#x} {:#x})", frame, a1, a2, a3);
    // Pace the game like a 60 Hz vblank until the GPU backend presents.
    std::this_thread::sleep_for(std::chrono::milliseconds(16));
}

// ---------------------------------------------------------------------------
// XAM misc
// ---------------------------------------------------------------------------

static uint32_t XamUserGetSigninState(uint32_t userIndex) { return userIndex == 0 ? 1 : 0; }

static uint32_t XamUserGetXUID(uint32_t userIndex, uint32_t type, be<uint64_t>* xuid)
{
    if (userIndex != 0)
        return ERROR_NO_SUCH_USER;
    if (xuid)
        *xuid = 0xE000000000000001ull; // offline XUID
    return ERROR_SUCCESS;
}

static uint32_t XamGetSystemVersion() { return 0x20006683; }

static uint32_t XamGetExecutionId(be<uint32_t>* info)
{
    *info = XexLoader::s_executionInfo;
    return STATUS_SUCCESS;
}

static uint32_t XamLoaderGetLaunchDataSize(be<uint32_t>* size) { if (size) *size = 0; return 0; }
static uint32_t XamLoaderGetLaunchData(void*, uint32_t) { return 0; }
static uint32_t XamLoaderSetLaunchData(void*, uint32_t) { return 0; }
static void XamLoaderTerminateTitle() { LOG_INFO("title terminated"); std::_Exit(0); }
static uint32_t XamLoaderLaunchTitle(const char* path, uint32_t) { LOG_INFO("launch title '{}'", path ? path : ""); return 0; }

static uint32_t XamUserReadProfileSettings(uint32_t titleId, uint32_t userIndex, uint32_t xuidCount, uint64_t* xuids,
    uint32_t settingCount, be<uint32_t>* settingIds, be<uint32_t>* bufferSize, void* buffer, XXOVERLAPPED* overlapped)
{
    // Layout: XUSER_READ_PROFILE_SETTING_RESULT { count, settings[] } with each
    // XUSER_PROFILE_SETTING being 0x38 bytes. Report every setting as "not set".
    uint32_t needed = 8 + settingCount * 0x38;
    if (buffer == nullptr || uint32_t(*bufferSize) < needed)
    {
        *bufferSize = needed;
        return 122; // ERROR_INSUFFICIENT_BUFFER
    }
    memset(buffer, 0, needed);
    auto* p = reinterpret_cast<be<uint32_t>*>(buffer);
    p[0] = settingCount;
    p[1] = g_memory.MapVirtual(p + 2);
    auto* settings = reinterpret_cast<uint8_t*>(p + 2);
    for (uint32_t i = 0; i < settingCount; i++)
    {
        auto* s = reinterpret_cast<be<uint32_t>*>(settings + i * 0x38);
        s[0] = 0; // source: not set
        s[1] = userIndex;
        s[2] = settingIds[i];
    }
    if (overlapped)
    {
        overlapped->Error = 0;
        overlapped->Length = needed;
        if (overlapped->hEvent)
            KernelSignalEventHandle(overlapped->hEvent);
    }
    return ERROR_SUCCESS;
}

static uint32_t XamUserWriteProfileSettings(uint32_t, uint32_t, uint32_t, void*, XXOVERLAPPED* overlapped)
{
    if (overlapped)
    {
        overlapped->Error = 0;
        if (overlapped->hEvent)
            KernelSignalEventHandle(overlapped->hEvent);
    }
    return ERROR_SUCCESS;
}

static uint32_t XamShowSigninUI(uint32_t, uint32_t) { XamNotifyEnqueueEvent(MSGID(0, 0x0009), 0); return 0; }
static uint32_t XamShowDeviceSelectorUI(uint32_t userIndex, uint32_t contentType, uint32_t contentFlags, uint64_t totalRequested, be<uint32_t>* deviceId, XXOVERLAPPED* overlapped)
{
    if (deviceId) *deviceId = 1;
    if (overlapped)
    {
        overlapped->Error = 0;
        if (overlapped->hEvent)
            KernelSignalEventHandle(overlapped->hEvent);
    }
    return ERROR_SUCCESS;
}
static uint32_t XamShowMessageBoxUI(uint32_t, be<uint16_t>*, be<uint16_t>*, uint32_t cButtons, uint32_t, uint32_t, uint32_t, be<uint32_t>* result, XXOVERLAPPED* overlapped)
{
    if (result) *result = 0;
    if (overlapped)
    {
        overlapped->Error = 0;
        if (overlapped->hEvent)
            KernelSignalEventHandle(overlapped->hEvent);
    }
    return ERROR_SUCCESS;
}
static uint32_t XamShowMessageBoxUIEx() { return ERROR_SUCCESS; }
static uint32_t XamShowDirtyDiscErrorUI(uint32_t) { LOG_ERROR("dirty disc error UI requested"); return 0; }
static void XamEnableInactivityProcessing(uint32_t, uint32_t) {}
static void XamResetInactivity(uint32_t) {}
static uint32_t XamContentGetCreator(uint32_t userIndex, const XCONTENT_DATA*, be<uint32_t>* isCreator, be<uint64_t>* xuid, XXOVERLAPPED*)
{
    if (isCreator) *isCreator = 1;
    if (xuid) *xuid = 0xE000000000000001ull;
    return 0;
}
static uint32_t XamContentGetDeviceState(uint32_t, XXOVERLAPPED*) { return 0; }
static uint32_t XamContentFlush(const char*, XXOVERLAPPED*) { return 0; }
static uint32_t XamContentSetThumbnail(const char*, void*, uint32_t, XXOVERLAPPED*) { return 0; }
static uint32_t XamUserCreateAchievementEnumerator(uint32_t, uint32_t, uint64_t, uint32_t, uint32_t, uint32_t, be<uint32_t>* bufferSize, be<uint32_t>* handle)
{
    if (bufferSize) *bufferSize = 0;
    if (handle) *handle = GUEST_INVALID_HANDLE_VALUE;
    return ERROR_NO_MORE_FILES;
}

static uint32_t XMsgStartIORequest(uint32_t App, uint32_t Message, XXOVERLAPPED* overlapped, void* Buffer, uint32_t szBuffer)
{
    LOG_KERNEL("app={:#x} msg={:#x}", App, Message);
    if (overlapped)
    {
        overlapped->Error = 0;
        overlapped->Length = 0;
        if (overlapped->hEvent)
            KernelSignalEventHandle(overlapped->hEvent);
    }
    return STATUS_SUCCESS;
}
static uint32_t XMsgStartIORequestEx(uint32_t App, uint32_t Message, XXOVERLAPPED* overlapped, void* Buffer, uint32_t szBuffer, uint32_t)
{
    return XMsgStartIORequest(App, Message, overlapped, Buffer, szBuffer);
}
static uint32_t XMsgInProcessCall(uint32_t app, uint32_t message, be<uint32_t>* param1, be<uint32_t>* param2)
{
    LOG_KERNEL("app={:#x} msg={:#x}", app, message);
    return 0;
}
static uint32_t XMsgCancelIORequest(XXOVERLAPPED*, uint32_t) { return 0; }
static uint32_t XNotifyPositionUI(uint32_t) { return 0; }

// XamTask: run a callback on its own guest thread.
static uint32_t XamTaskSchedule(uint32_t callback, uint32_t context, be<uint32_t>* optionalPtr, be<uint32_t>* handle)
{
    LOG_KERNEL("callback={:#x} context={:#x}", callback, context);
    auto* hThread = GuestThread::Start({ callback, context, 0, 0 }, nullptr);
    if (handle)
        *handle = GetKernelHandle(hThread);
    return STATUS_SUCCESS;
}
static uint32_t XamTaskShouldExit(uint32_t) { return 0; }
static uint32_t XamTaskCloseHandle(uint32_t) { return 0; }

// Multi-disc: all four discs are merged into one directory tree, so a swap
// request completes immediately.
static uint32_t XamSwapDisc(uint32_t discNumber, uint32_t completionEvent, uint32_t message)
{
    LOG_INFO("XamSwapDisc to disc {} (merged data, completing immediately)", discNumber);
    if (completionEvent)
        KernelSignalEventHandle(completionEvent);
    return 0;
}
static void XamSwapCancel() {}

// Audio: stub until the APU lands.
static uint32_t XAudioRegisterRenderDriverClient(be<uint32_t>* callback, be<uint32_t>* driver)
{
    LOG_KERNEL("callback={:#x}", uint32_t(callback[0]));
    if (driver) *driver = 0x41554449; // 'AUDI'
    return 0;
}
static uint32_t XAudioUnregisterRenderDriverClient(uint32_t) { return 0; }
static uint32_t XAudioSubmitRenderDriverFrame(uint32_t, void*) { return 0; }
static uint32_t XAudioGetVoiceCategoryVolumeChangeMask(uint32_t, be<uint32_t>* mask) { if (mask) *mask = 0; return 0; }
static uint32_t XAudioGetVoiceCategoryVolume(uint32_t, be<float>* volume) { if (volume) *volume = 1.0f; return 0; }
static uint32_t XMACreateContext(be<uint32_t>* context) { if (context) *context = 0; return STATUS_NOT_IMPLEMENTED; }
static void XMAReleaseContext(uint32_t) {}

// Networking: report no network.
static uint32_t NetDll_XNetStartup(uint32_t, uint32_t) { return 0; }
static uint32_t NetDll_XNetCleanup(uint32_t, uint32_t) { return 0; }
static uint32_t NetDll_WSAStartup(uint32_t, uint32_t, uint32_t) { return 0; }
static uint32_t NetDll_WSACleanup(uint32_t) { return 0; }
static uint32_t NetDll_WSAGetLastError() { return 10093; } // WSANOTINITIALISED
static uint32_t NetDll_XNetGetTitleXnAddr(uint32_t, uint32_t) { return 0x00000001; } // XNET_GET_XNADDR_PENDING
static uint32_t NetDll_socket(uint32_t, uint32_t, uint32_t, uint32_t) { return 0xFFFFFFFF; }
static uint32_t NetDll_closesocket(uint32_t, uint32_t) { return 0; }

// ---------------------------------------------------------------------------
// Hook table
// ---------------------------------------------------------------------------

GUEST_FUNCTION_HOOK(__imp__NtCreateEvent, NtCreateEvent);
GUEST_FUNCTION_HOOK(__imp__NtSetEvent, NtSetEvent);
GUEST_FUNCTION_HOOK(__imp__NtPulseEvent, NtPulseEvent);
GUEST_FUNCTION_HOOK(__imp__NtClearEvent, NtClearEvent);
GUEST_FUNCTION_HOOK(__imp__KeSetEvent, KeSetEvent);
GUEST_FUNCTION_HOOK(__imp__KeResetEvent, KeResetEvent);
GUEST_FUNCTION_HOOK(__imp__KeWaitForSingleObject, KeWaitForSingleObject);
GUEST_FUNCTION_HOOK(__imp__KeWaitForMultipleObjects, KeWaitForMultipleObjects);
GUEST_FUNCTION_HOOK(__imp__NtWaitForSingleObjectEx, NtWaitForSingleObjectEx);
GUEST_FUNCTION_HOOK(__imp__NtWaitForMultipleObjectsEx, NtWaitForMultipleObjectsEx);
GUEST_FUNCTION_HOOK(__imp__NtCreateSemaphore, NtCreateSemaphore);
GUEST_FUNCTION_HOOK(__imp__NtReleaseSemaphore, NtReleaseSemaphore);
GUEST_FUNCTION_HOOK(__imp__KeInitializeSemaphore, KeInitializeSemaphore);
GUEST_FUNCTION_HOOK(__imp__KeReleaseSemaphore, KeReleaseSemaphore);
GUEST_FUNCTION_HOOK(__imp__NtCreateMutant, NtCreateMutant);
GUEST_FUNCTION_HOOK(__imp__NtReleaseMutant, NtReleaseMutant);

GUEST_FUNCTION_HOOK(__imp__RtlInitializeCriticalSection, RtlInitializeCriticalSection);
GUEST_FUNCTION_HOOK(__imp__RtlEnterCriticalSection, RtlEnterCriticalSection);
GUEST_FUNCTION_HOOK(__imp__RtlTryEnterCriticalSection, RtlTryEnterCriticalSection);
GUEST_FUNCTION_HOOK(__imp__RtlLeaveCriticalSection, RtlLeaveCriticalSection);
GUEST_FUNCTION_HOOK(__imp__KfAcquireSpinLock, KfAcquireSpinLock);
GUEST_FUNCTION_HOOK(__imp__KfReleaseSpinLock, KfReleaseSpinLock);
GUEST_FUNCTION_HOOK(__imp__KeAcquireSpinLockAtRaisedIrql, KfAcquireSpinLock);
GUEST_FUNCTION_HOOK(__imp__KeReleaseSpinLockFromRaisedIrql, KfReleaseSpinLock);
GUEST_FUNCTION_HOOK(__imp__KeTryToAcquireSpinLockAtRaisedIrql, KeTryToAcquireSpinLockAtRaisedIrql);
GUEST_FUNCTION_HOOK(__imp__KeTlsGetValue, KeTlsGetValue);
GUEST_FUNCTION_HOOK(__imp__KeTlsSetValue, KeTlsSetValue);
GUEST_FUNCTION_HOOK(__imp__KeTlsAlloc, KeTlsAlloc);
GUEST_FUNCTION_HOOK(__imp__KeTlsFree, KeTlsFree);

GUEST_FUNCTION_HOOK(__imp__ExCreateThread, ExCreateThread);
GUEST_FUNCTION_HOOK(__imp__ExTerminateThread, ExTerminateThread);
GUEST_FUNCTION_HOOK(__imp__KeSetBasePriorityThread, KeSetBasePriorityThread);
GUEST_FUNCTION_HOOK(__imp__KeQueryBasePriorityThread, KeQueryBasePriorityThread);
GUEST_FUNCTION_HOOK(__imp__KeSetAffinityThread, KeSetAffinityThread);
GUEST_FUNCTION_HOOK(__imp__NtSuspendThread, NtSuspendThread);
GUEST_FUNCTION_HOOK(__imp__NtResumeThread, NtResumeThread);
GUEST_FUNCTION_HOOK(__imp__KeResumeThread, KeResumeThread);
GUEST_FUNCTION_HOOK(__imp__KeDelayExecutionThread, KeDelayExecutionThread);
GUEST_FUNCTION_HOOK(__imp__ObReferenceObjectByHandle, ObReferenceObjectByHandle);
GUEST_FUNCTION_HOOK(__imp__ObDereferenceObject, ObDereferenceObject);
GUEST_FUNCTION_HOOK(__imp__ObReferenceObject, ObReferenceObject);
GUEST_FUNCTION_HOOK(__imp__NtClose, NtClose);
GUEST_FUNCTION_HOOK(__imp__NtDuplicateObject, NtDuplicateObject);

GUEST_FUNCTION_HOOK(__imp__NtAllocateVirtualMemory, NtAllocateVirtualMemory);
GUEST_FUNCTION_HOOK(__imp__NtFreeVirtualMemory, NtFreeVirtualMemory);
GUEST_FUNCTION_HOOK(__imp__NtQueryVirtualMemory, NtQueryVirtualMemory);
GUEST_FUNCTION_HOOK(__imp__MmAllocatePhysicalMemoryEx, MmAllocatePhysicalMemoryEx);
GUEST_FUNCTION_HOOK(__imp__MmFreePhysicalMemory, MmFreePhysicalMemory);
GUEST_FUNCTION_HOOK(__imp__MmGetPhysicalAddress, MmGetPhysicalAddress);
GUEST_FUNCTION_HOOK(__imp__MmQueryAddressProtect, MmQueryAddressProtect);
GUEST_FUNCTION_HOOK(__imp__MmSetAddressProtect, MmSetAddressProtect);
GUEST_FUNCTION_HOOK(__imp__MmQueryStatistics, MmQueryStatistics);
GUEST_FUNCTION_HOOK(__imp__ExAllocatePoolTypeWithTag, ExAllocatePoolTypeWithTag);
GUEST_FUNCTION_HOOK(__imp__ExAllocatePool, ExAllocatePool);
GUEST_FUNCTION_HOOK(__imp__ExFreePool, ExFreePool);
GUEST_FUNCTION_HOOK(__imp__InterlockedPopEntrySList, InterlockedPopEntrySList_x);
GUEST_FUNCTION_HOOK(__imp__InterlockedFlushSList, InterlockedFlushSList_x);

GUEST_FUNCTION_HOOK(__imp__KeQuerySystemTime, KeQuerySystemTime);
GUEST_FUNCTION_HOOK(__imp__KeQueryPerformanceFrequency, KeQueryPerformanceFrequency);
GUEST_FUNCTION_HOOK(__imp__RtlTimeToTimeFields, RtlTimeToTimeFields);
GUEST_FUNCTION_HOOK(__imp__RtlTimeFieldsToTime, RtlTimeFieldsToTime);

GUEST_FUNCTION_HOOK(__imp__RtlInitAnsiString, RtlInitAnsiString);
GUEST_FUNCTION_HOOK(__imp__RtlInitUnicodeString, RtlInitUnicodeString);
GUEST_FUNCTION_HOOK(__imp__RtlFreeAnsiString, RtlFreeAnsiString);
GUEST_FUNCTION_HOOK(__imp__RtlUnicodeStringToAnsiString, RtlUnicodeStringToAnsiString);
GUEST_FUNCTION_HOOK(__imp__RtlUnicodeToMultiByteN, RtlUnicodeToMultiByteN);
GUEST_FUNCTION_HOOK(__imp__RtlMultiByteToUnicodeN, RtlMultiByteToUnicodeN);
GUEST_FUNCTION_HOOK(__imp__RtlCompareStringN, RtlCompareStringN);
GUEST_FUNCTION_HOOK(__imp__RtlUpcaseUnicodeChar, RtlUpcaseUnicodeChar);
GUEST_FUNCTION_HOOK(__imp__RtlFillMemoryUlong, RtlFillMemoryUlong);
GUEST_FUNCTION_HOOK(__imp__RtlCompareMemoryUlong, RtlCompareMemoryUlong);
GUEST_FUNCTION_HOOK(__imp__RtlNtStatusToDosError, RtlNtStatusToDosError);

GUEST_FUNCTION_HOOK(__imp__KeBugCheckEx, KeBugCheckEx);
GUEST_FUNCTION_HOOK(__imp__KeBugCheck, KeBugCheck);
GUEST_FUNCTION_HOOK(__imp__DbgBreakPoint, DbgBreakPoint);
GUEST_FUNCTION_HOOK(__imp__HalReturnToFirmware, HalReturnToFirmware);
GUEST_FUNCTION_HOOK(__imp__KeGetCurrentProcessType, KeGetCurrentProcessType);
GUEST_FUNCTION_HOOK(__imp__ExGetXConfigSetting, ExGetXConfigSetting);
GUEST_FUNCTION_HOOK(__imp__XexCheckExecutablePrivilege, XexCheckExecutablePrivilege);
GUEST_FUNCTION_HOOK(__imp__XexGetModuleHandle, XexGetModuleHandle);
GUEST_FUNCTION_HOOK(__imp__XexGetModuleSection, XexGetModuleSection);
GUEST_FUNCTION_HOOK(__imp__XexGetProcedureAddress, XexGetProcedureAddress);
GUEST_FUNCTION_HOOK(__imp__ExRegisterTitleTerminateNotification, ExRegisterTitleTerminateNotification);
GUEST_FUNCTION_HOOK(__imp__KeEnableFpuExceptions, KeEnableFpuExceptions);
GUEST_FUNCTION_HOOK(__imp__KeLockL2, KeLockL2);
GUEST_FUNCTION_HOOK(__imp__KeUnlockL2, KeUnlockL2);
GUEST_FUNCTION_HOOK(__imp__KeEnterCriticalRegion, KeEnterCriticalRegion);
GUEST_FUNCTION_HOOK(__imp__KeLeaveCriticalRegion, KeLeaveCriticalRegion);
GUEST_FUNCTION_HOOK(__imp__KeRaiseIrqlToDpcLevel, KeRaiseIrqlToDpcLevel);
GUEST_FUNCTION_HOOK(__imp__KfLowerIrql, KfLowerIrql);
GUEST_FUNCTION_HOOK(__imp__KiApcNormalRoutineNop, KiApcNormalRoutineNop_x);
GUEST_FUNCTION_HOOK(__imp__FscSetCacheElementCount, FscSetCacheElementCount_x);
GUEST_FUNCTION_HOOK(__imp__RtlRaiseException, RtlRaiseException_x);
GUEST_FUNCTION_HOOK(__imp__RtlUnwind, RtlUnwind_x);
GUEST_FUNCTION_HOOK(__imp__RtlCaptureContext, RtlCaptureContext_x);
GUEST_FUNCTION_HOOK(__imp____C_specific_handler, __C_specific_handler_x);
GUEST_FUNCTION_HOOK(__imp__RtlImageXexHeaderField, RtlImageXexHeaderField);
GUEST_FUNCTION_HOOK(__imp__ObCreateSymbolicLink, ObCreateSymbolicLink);
GUEST_FUNCTION_HOOK(__imp__ObDeleteSymbolicLink, ObDeleteSymbolicLink);
GUEST_FUNCTION_HOOK(__imp__ObIsTitleObject, ObIsTitleObject);
GUEST_FUNCTION_HOOK(__imp__IoCreateDevice, IoCreateDevice);
GUEST_FUNCTION_HOOK(__imp__IoDeleteDevice, IoDeleteDevice);
GUEST_FUNCTION_HOOK(__imp__IoCompleteRequest, IoCompleteRequest);
GUEST_FUNCTION_HOOK(__imp__IoInvalidDeviceRequest, IoInvalidDeviceRequest);
GUEST_FUNCTION_HOOK(__imp__IoCheckShareAccess, IoCheckShareAccess);
GUEST_FUNCTION_HOOK(__imp__IoSetShareAccess, IoSetShareAccess);
GUEST_FUNCTION_HOOK(__imp__IoRemoveShareAccess, IoRemoveShareAccess);
GUEST_FUNCTION_HOOK(__imp__IoDismountVolume, IoDismountVolume);
GUEST_FUNCTION_HOOK(__imp__IoDismountVolumeByFileHandle, IoDismountVolumeByFileHandle);
GUEST_FUNCTION_HOOK(__imp__StfsCreateDevice, StfsCreateDevice);
GUEST_FUNCTION_HOOK(__imp__StfsControlDevice, StfsControlDevice);
GUEST_FUNCTION_HOOK(__imp__XeCryptSha, XeCryptSha);
GUEST_FUNCTION_HOOK(__imp__XeKeysConsolePrivateKeySign, XeKeysConsolePrivateKeySign);
GUEST_FUNCTION_HOOK(__imp__XeKeysConsoleSignatureVerification, XeKeysConsoleSignatureVerification);

GUEST_FUNCTION_HOOK(__imp__VdQueryVideoMode, VdQueryVideoMode);
GUEST_FUNCTION_HOOK(__imp__VdQueryVideoFlags, VdQueryVideoFlags);
GUEST_FUNCTION_HOOK(__imp__XGetVideoMode, XGetVideoMode);
GUEST_FUNCTION_HOOK(__imp__XGetAVPack, XGetAVPack);
GUEST_FUNCTION_HOOK(__imp__XGetGameRegion, XGetGameRegion);
GUEST_FUNCTION_HOOK(__imp__XGetLanguage, XGetLanguage);
GUEST_FUNCTION_HOOK(__imp__VdGetCurrentDisplayInformation, VdGetCurrentDisplayInformation);
GUEST_FUNCTION_HOOK(__imp__VdGetCurrentDisplayGamma, VdGetCurrentDisplayGamma);
GUEST_FUNCTION_HOOK(__imp__VdSetGraphicsInterruptCallback, VdSetGraphicsInterruptCallback);
GUEST_FUNCTION_HOOK(__imp__VdInitializeRingBuffer, VdInitializeRingBuffer);
GUEST_FUNCTION_HOOK(__imp__VdEnableRingBufferRPtrWriteBack, VdEnableRingBufferRPtrWriteBack);
GUEST_FUNCTION_HOOK(__imp__VdSetSystemCommandBufferGpuIdentifierAddress, VdSetSystemCommandBufferGpuIdentifierAddress);
GUEST_FUNCTION_HOOK(__imp__VdGetSystemCommandBuffer, VdGetSystemCommandBuffer);
GUEST_FUNCTION_HOOK(__imp__VdInitializeEngines, VdInitializeEngines);
GUEST_FUNCTION_HOOK(__imp__VdShutdownEngines, VdShutdownEngines);
GUEST_FUNCTION_HOOK(__imp__VdIsHSIOTrainingSucceeded, VdIsHSIOTrainingSucceeded);
GUEST_FUNCTION_HOOK(__imp__VdRetrainEDRAM, VdRetrainEDRAM);
GUEST_FUNCTION_HOOK(__imp__VdRetrainEDRAMWorker, VdRetrainEDRAMWorker);
GUEST_FUNCTION_HOOK(__imp__VdPersistDisplay, VdPersistDisplay);
GUEST_FUNCTION_HOOK(__imp__VdSetDisplayMode, VdSetDisplayMode);
GUEST_FUNCTION_HOOK(__imp__VdEnableDisableClockGating, VdEnableDisableClockGating);
GUEST_FUNCTION_HOOK(__imp__VdCallGraphicsNotificationRoutines, VdCallGraphicsNotificationRoutines);
GUEST_FUNCTION_HOOK(__imp__VdInitializeScalerCommandBuffer, VdInitializeScalerCommandBuffer);
GUEST_FUNCTION_HOOK(__imp__VdSwap, VdSwap);

GUEST_FUNCTION_HOOK(__imp__XamUserGetSigninState, XamUserGetSigninState);
GUEST_FUNCTION_HOOK(__imp__XamUserGetXUID, XamUserGetXUID);
GUEST_FUNCTION_HOOK(__imp__XamGetSystemVersion, XamGetSystemVersion);
GUEST_FUNCTION_HOOK(__imp__XamGetExecutionId, XamGetExecutionId);
GUEST_FUNCTION_HOOK(__imp__XamLoaderGetLaunchDataSize, XamLoaderGetLaunchDataSize);
GUEST_FUNCTION_HOOK(__imp__XamLoaderGetLaunchData, XamLoaderGetLaunchData);
GUEST_FUNCTION_HOOK(__imp__XamLoaderSetLaunchData, XamLoaderSetLaunchData);
GUEST_FUNCTION_HOOK(__imp__XamLoaderTerminateTitle, XamLoaderTerminateTitle);
GUEST_FUNCTION_HOOK(__imp__XamLoaderLaunchTitle, XamLoaderLaunchTitle);
GUEST_FUNCTION_HOOK(__imp__XamUserReadProfileSettings, XamUserReadProfileSettings);
GUEST_FUNCTION_HOOK(__imp__XamUserWriteProfileSettings, XamUserWriteProfileSettings);
GUEST_FUNCTION_HOOK(__imp__XamShowSigninUI, XamShowSigninUI);
GUEST_FUNCTION_HOOK(__imp__XamShowDeviceSelectorUI, XamShowDeviceSelectorUI);
GUEST_FUNCTION_HOOK(__imp__XamShowMessageBoxUI, XamShowMessageBoxUI);
GUEST_FUNCTION_HOOK(__imp__XamShowMessageBoxUIEx, XamShowMessageBoxUIEx);
GUEST_FUNCTION_HOOK(__imp__XamShowDirtyDiscErrorUI, XamShowDirtyDiscErrorUI);
GUEST_FUNCTION_HOOK(__imp__XamEnableInactivityProcessing, XamEnableInactivityProcessing);
GUEST_FUNCTION_HOOK(__imp__XamResetInactivity, XamResetInactivity);
GUEST_FUNCTION_HOOK(__imp__XamContentGetCreator, XamContentGetCreator);
GUEST_FUNCTION_HOOK(__imp__XamContentGetDeviceState, XamContentGetDeviceState);
GUEST_FUNCTION_HOOK(__imp__XamContentFlush, XamContentFlush);
GUEST_FUNCTION_HOOK(__imp__XamContentSetThumbnail, XamContentSetThumbnail);
GUEST_FUNCTION_HOOK(__imp__XamContentCreateEx, XamContentCreateEx);
GUEST_FUNCTION_HOOK(__imp__XamContentClose, XamContentClose);
GUEST_FUNCTION_HOOK(__imp__XamContentCreateEnumerator, XamContentCreateEnumerator);
GUEST_FUNCTION_HOOK(__imp__XamContentGetDeviceData, XamContentGetDeviceData);
GUEST_FUNCTION_HOOK(__imp__XamEnumerate, XamEnumerate);
GUEST_FUNCTION_HOOK(__imp__XamNotifyCreateListener, XamNotifyCreateListener);
GUEST_FUNCTION_HOOK(__imp__XNotifyGetNext, XNotifyGetNext);
GUEST_FUNCTION_HOOK(__imp__XNotifyPositionUI, XNotifyPositionUI);
GUEST_FUNCTION_HOOK(__imp__XamUserCreateAchievementEnumerator, XamUserCreateAchievementEnumerator);
GUEST_FUNCTION_HOOK(__imp__XamInputGetCapabilities, XamInputGetCapabilities);
GUEST_FUNCTION_HOOK(__imp__XamInputGetState, XamInputGetState);
GUEST_FUNCTION_HOOK(__imp__XamInputSetState, XamInputSetState);
GUEST_FUNCTION_HOOK(__imp__XMsgStartIORequest, XMsgStartIORequest);
GUEST_FUNCTION_HOOK(__imp__XMsgStartIORequestEx, XMsgStartIORequestEx);
GUEST_FUNCTION_HOOK(__imp__XMsgInProcessCall, XMsgInProcessCall);
GUEST_FUNCTION_HOOK(__imp__XMsgCancelIORequest, XMsgCancelIORequest);
GUEST_FUNCTION_HOOK(__imp__XamTaskSchedule, XamTaskSchedule);
GUEST_FUNCTION_HOOK(__imp__XamTaskShouldExit, XamTaskShouldExit);
GUEST_FUNCTION_HOOK(__imp__XamTaskCloseHandle, XamTaskCloseHandle);
GUEST_FUNCTION_HOOK(__imp__XamSwapDisc, XamSwapDisc);
GUEST_FUNCTION_HOOK(__imp__XamSwapCancel, XamSwapCancel);

GUEST_FUNCTION_HOOK(__imp__XAudioRegisterRenderDriverClient, XAudioRegisterRenderDriverClient);
GUEST_FUNCTION_HOOK(__imp__XAudioUnregisterRenderDriverClient, XAudioUnregisterRenderDriverClient);
GUEST_FUNCTION_HOOK(__imp__XAudioSubmitRenderDriverFrame, XAudioSubmitRenderDriverFrame);
GUEST_FUNCTION_HOOK(__imp__XAudioGetVoiceCategoryVolumeChangeMask, XAudioGetVoiceCategoryVolumeChangeMask);
GUEST_FUNCTION_HOOK(__imp__XAudioGetVoiceCategoryVolume, XAudioGetVoiceCategoryVolume);
GUEST_FUNCTION_HOOK(__imp__XMACreateContext, XMACreateContext);
GUEST_FUNCTION_HOOK(__imp__XMAReleaseContext, XMAReleaseContext);

GUEST_FUNCTION_HOOK(__imp__NetDll_XNetStartup, NetDll_XNetStartup);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetCleanup, NetDll_XNetCleanup);
GUEST_FUNCTION_HOOK(__imp__NetDll_WSAStartup, NetDll_WSAStartup);
GUEST_FUNCTION_HOOK(__imp__NetDll_WSACleanup, NetDll_WSACleanup);
GUEST_FUNCTION_HOOK(__imp__NetDll_WSAGetLastError, NetDll_WSAGetLastError);
GUEST_FUNCTION_HOOK(__imp__NetDll_XNetGetTitleXnAddr, NetDll_XNetGetTitleXnAddr);
GUEST_FUNCTION_HOOK(__imp__NetDll_socket, NetDll_socket);
GUEST_FUNCTION_HOOK(__imp__NetDll_closesocket, NetDll_closesocket);
