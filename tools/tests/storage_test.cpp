#include <stdafx.h>
#include <kernel/function.h>
#include <kernel/xdm.h>
#include <kernel/xam.h>
#include <kernel/io/file_system.h>
#include <kernel/io/file_system_test.h>
#include <kernel/io/io_diagnostics.h>
#include <condition_variable>
#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <stdexcept>
#include <apu/xma.h>
#include <gpu/ppc_mmio.h>
#include <kernel/dlc_content.h>
#include <cpu/guest_thread.h>
#include "../XenonRecomp/thirdparty/tomlplusplus/vendor/json.hpp"

PPC_FUNC(__imp__XamContentCreateEx);
PPC_FUNC(__imp__XamContentClose);
PPC_FUNC(__imp__XamContentFlush);
PPC_FUNC(__imp__XamContentSetThumbnail);
PPC_FUNC(__imp__NtCreateEvent);
PPC_FUNC(__imp__NtCreateFile);
PPC_FUNC(__imp__NtWriteFile);
PPC_FUNC(__imp__NtReadFile);
PPC_FUNC(__imp__NtReadFileScatter);
PPC_FUNC(__imp__NtClose);
PPC_FUNC(__imp__NtDuplicateObject);
PPC_FUNC(__imp__NtWaitForSingleObjectEx);
PPC_FUNC(__imp__NtQueryDirectoryFile);
PPC_FUNC(__imp__NtFlushBuffersFile);
PPC_FUNC(__imp__XamSwapDisc);

static void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

static uint32_t Call(PPCFunc* function, std::initializer_list<uint32_t> args)
{
    PPCContext ctx{};
    auto* stack = static_cast<uint8_t*>(g_userHeap.Alloc(512));
    memset(stack, 0, 512);
    ctx.r1.u32 = g_memory.MapVirtual(stack);
    size_t i = 0;
    for (uint32_t value : args)
    {
        if (i < 8) ArgTranslator::SetIntegerArgumentValue(ctx, g_memory.base, i, value);
        else *reinterpret_cast<be<uint32_t>*>(stack + 0x54 + (i - 8) * 8) = value;
        ++i;
    }
    SetPPCContext(ctx);
    function(ctx, g_memory.base);
    const uint32_t result = ctx.r3.u32;
    g_ppcContext = nullptr;
    g_userHeap.Free(stack);
    return result;
}

template<typename T> static uint32_t Addr(T* p) { return g_memory.MapVirtual(p); }

static void CheckDiscs(const std::filesystem::path& root, bool rejected = false)
{
    FileSystem::Init(root / "disc1");
    XamInit();
    auto* event = g_userHeap.Alloc<be<uint32_t>>();
    if (rejected)
    {
        Check(Call(__imp__NtCreateEvent,{Addr(event),0,0,0,0}) == 0,"create failure event");
        Check(Call(__imp__XamSwapDisc,{2,*event,0}) == 21,"reject unavailable/inconsistent disc");
        Check(GetKernelObject(*event)->Wait(0) != 0,"rejected disc must not signal completion");
        Check(FileSystem::ResolvePath("game:\\LO.fpi") == root/"disc1"/"LO.fpi","rejected disc retains old root");
        DestroyKernelObject(*event);
        std::puts("PASS: rejected disc retains mount and leaves completion event unsignaled");
        return;
    }
    auto* handle = g_userHeap.Alloc<be<uint32_t>>();
    auto* iosb = static_cast<XIO_STATUS_BLOCK*>(g_userHeap.Alloc(sizeof(XIO_STATUS_BLOCK)));
    auto* offset = g_userHeap.Alloc<be<uint64_t>>();
    auto* buffer = static_cast<char*>(g_userHeap.Alloc(4096));
    auto* name = static_cast<char*>(g_userHeap.Alloc(128));
    auto* ansi = g_userHeap.Alloc<XANSI_STRING>();
    auto* attributes = g_userHeap.Alloc<XOBJECT_ATTRIBUTES>();
    auto open = [&](const char* path) {
        strcpy(name, path);
        ansi->Buffer = name; ansi->Length = uint16_t(strlen(name)); ansi->MaximumLength = uint16_t(strlen(name)+1);
        *attributes = {}; attributes->Name = ansi;
        Check(Call(__imp__NtCreateFile, {Addr(handle),0x80000000,Addr(attributes),Addr(iosb),0,0,1,1,0x40}) == 0, "open disc resource");
        return uint32_t(*handle);
    };
    auto verify = [&](uint32_t h, const std::filesystem::path& original, uint64_t position) {
        *offset = position;
        Check(Call(__imp__NtReadFile, {h,0,0,0,Addr(iosb),Addr(buffer),4096,Addr(offset)}) == 0, "read disc resource");
        std::array<char,4096> expected{};
        std::ifstream source(original, std::ios::binary); source.seekg(position); source.read(expected.data(),expected.size());
        Check(size_t(source.gcount()) == iosb->Information && !memcmp(buffer,expected.data(),size_t(source.gcount())), "resource bytes match target volume");
    };
    const uint32_t old = open("game:\\LO.fpi");
    for (uint32_t disc : {1,2,3,4,2,1})
    {
        Check(Call(__imp__NtCreateEvent,{Addr(event),0,0,0,0}) == 0, "create disc event");
        Check(Call(__imp__XamSwapDisc,{disc,*event,0}) == 0, "select installed disc via guest import");
        Check(GetKernelObject(*event)->Wait(0) == 0, "disc event only after selection");
        DestroyKernelObject(*event);
        const auto directory = root / ("disc" + std::to_string(disc));
        for (const char* alias : {"game:\\LO.fpi", "d:\\LO.fpi", "\\Device\\Cdrom0\\LO.fpi", "\\??\\game:\\LO.fpi", "LO.fpi"})
        {
            auto h = open(alias); verify(h,directory/"LO.fpi",0); DestroyKernelObject(h);
        }
        for (const char* archive : {"xenon_event.fpd", "xenon_field.fpd", "xenon_mov.fpd", "xenon_snd.fpd"})
        {
            const auto source = directory/archive;
            const auto position = (std::filesystem::file_size(source)/2/2048)*2048;
            auto h = open((std::string("game:\\")+archive).c_str()); verify(h,source,position); DestroyKernelObject(h);
        }
        verify(old,root/"disc1"/"LO.fpi",0);
        std::printf("PASS disc %u: five path aliases, four distinct archive reads, old handle retained\n",disc);
    }
    DestroyKernelObject(old);
    Check(Call(__imp__NtCreateEvent,{Addr(event),0,0,0,0}) == 0,"create failure event");
    Check(Call(__imp__XamSwapDisc,{5,*event,0}) == 87,"reject invalid disc");
    Check(GetKernelObject(*event)->Wait(0) != 0,"failed selection must not signal success");
    DestroyKernelObject(*event);
    Check(FileSystem::ResolvePath("game:\\LO.fpi") == root/"disc1"/"LO.fpi","failed selection retains root");
}

static void CheckConcurrentReads(uint32_t file)
{
    constexpr unsigned workers = 4, iterations = 2000, count = 256;
    std::atomic<unsigned> ready{0}, failures{0};
    std::array<std::thread, workers> threads;
    for (unsigned worker = 0; worker < workers; ++worker)
    {
        auto* bytes = static_cast<uint8_t*>(g_userHeap.Alloc(count));
        auto* iosb = static_cast<XIO_STATUS_BLOCK*>(g_userHeap.Alloc(sizeof(XIO_STATUS_BLOCK)));
        auto* offset = g_userHeap.Alloc<be<uint64_t>>();
        *offset = worker * 257;
        threads[worker] = std::thread([=, &ready, &failures] {
            ++ready;
            while (ready.load() != workers) std::this_thread::yield();
            for (unsigned attempt = 0; attempt < iterations; ++attempt)
            {
                const auto status = Call(__imp__NtReadFile, {file,0,0,0,Addr(iosb),Addr(bytes),count,Addr(offset)});
                bool valid = status == 0 && iosb->Information == count;
                for (unsigned i = 0; i < count; ++i)
                    valid &= bytes[i] == uint8_t((worker * 257 + i) * 37 + 11);
                if (!valid) ++failures;
            }
            g_userHeap.Free(bytes);
            g_userHeap.Free(iosb);
            g_userHeap.Free(offset);
        });
    }
    for (auto& thread : threads) thread.join();
    std::printf("concurrent positioned reads: %u mismatches / %u requests\n", failures.load(), workers * iterations);
    Check(failures == 0, "shared file handle must preserve each request's offset");
}

// These tests pause actual guest imports at deterministic boundaries. A failed
// deadline terminates the test process, so a regression cannot hang in join().
[[noreturn]] static void IoFailure(const char* message)
{
    std::fprintf(stderr, "FAIL io-lifetime: %s\n", message);
    std::fflush(nullptr);
    std::_Exit(1);
}

static void IoCheck(bool condition, const char* message)
{
    if (!condition) IoFailure(message);
}

class IoTask
{
    const char* name;
    std::mutex mutex;
    std::condition_variable cv;
    bool finished = false;
    std::exception_ptr error;
    std::thread thread;
public:
    template<class F> IoTask(const char* label, F action) : name(label), thread([this, action = std::move(action)] {
        try { action(); } catch (...) { error = std::current_exception(); }
        { std::lock_guard lock(mutex); finished = true; }
        cv.notify_all();
    }) {}
    void Join()
    {
        std::unique_lock lock(mutex);
        if (!cv.wait_for(lock, std::chrono::seconds(60), [&] { return finished; })) IoFailure(name);
        lock.unlock();
        thread.join();
        if (error)
        {
            try { std::rethrow_exception(error); }
            catch (const std::exception& e) { IoFailure(e.what()); }
            catch (...) { IoFailure(name); }
        }
    }
    ~IoTask() { if (thread.joinable()) IoFailure("test abandoned a running worker"); }
};

class IoReadGate;
static std::atomic<IoReadGate*> s_ioReadGate{nullptr};

class IoReadGate
{
    uint32_t handle;
    file_system_test::Stage pauseAt;
    std::mutex mutex;
    std::condition_variable cv;
    std::array<unsigned, 6> counts{};
    bool paused = false;
    bool released = false;
    static void Hook(file_system_test::Stage stage, uint32_t handle)
    {
        if (auto* gate = s_ioReadGate.load()) gate->Visit(stage, handle);
    }
    void Visit(file_system_test::Stage stage, uint32_t value)
    {
        if (value != handle) return;
        std::unique_lock lock(mutex);
        ++counts[size_t(stage)];
        const bool mustPause = stage == pauseAt && !paused;
        if (mustPause) { paused = true; released = false; }
        cv.notify_all();
        if (mustPause && !cv.wait_for(lock, std::chrono::seconds(60), [&] { return released; }))
            IoFailure("read hook was not released");
    }
public:
    IoReadGate(uint32_t value, file_system_test::Stage stage) : handle(value), pauseAt(stage)
    {
        s_ioReadGate = this;
        file_system_test::SetHook(Hook);
    }
    ~IoReadGate()
    {
        file_system_test::SetHook(nullptr);
        s_ioReadGate = nullptr;
    }
    void Wait(file_system_test::Stage stage, unsigned expected = 1)
    {
        std::unique_lock lock(mutex);
        if (!cv.wait_for(lock, std::chrono::seconds(60), [&] { return counts[size_t(stage)] >= expected; }))
            IoFailure("read did not reach its expected hook stage");
    }
    unsigned Count(file_system_test::Stage stage)
    {
        std::lock_guard lock(mutex);
        return counts[size_t(stage)];
    }
    void Release()
    {
        { std::lock_guard lock(mutex); released = true; }
        cv.notify_all();
    }
    void ContinueTo(file_system_test::Stage stage)
    {
        { std::lock_guard lock(mutex); pauseAt = stage; paused = false; released = true; }
        cv.notify_all();
    }
};

struct IoRead
{
    static constexpr uint32_t count = 256;
    uint8_t* bytes = static_cast<uint8_t*>(g_userHeap.Alloc(count));
    XIO_STATUS_BLOCK* iosb = static_cast<XIO_STATUS_BLOCK*>(g_userHeap.Alloc(sizeof(XIO_STATUS_BLOCK)));
    be<uint64_t>* offset = g_userHeap.Alloc<be<uint64_t>>();
    uint32_t status = 0xDEADBEEF;
    explicit IoRead(uint64_t position = 0)
    {
        *offset = position;
        std::memset(bytes, 0xA5, count);
        iosb->Status = 0xDEADBEEF;
        iosb->Information = 0xDEADBEEF;
    }
    ~IoRead() { g_userHeap.Free(bytes); g_userHeap.Free(iosb); g_userHeap.Free(offset); }
    void Run(uint32_t handle, uint32_t event = 0, uint32_t routine = 0, uint32_t context = 0)
    {
        status = Call(__imp__NtReadFile, {handle, event, routine, context, Addr(iosb), Addr(bytes), count, Addr(offset)});
    }
    bool BytesMatch(uint8_t seed = 11) const
    {
        for (uint32_t i = 0; i < count; ++i)
            if (bytes[i] != uint8_t((uint64_t(*offset) + i) * 37 + seed)) return false;
        return true;
    }
    bool Complete(uint8_t seed = 11) const
    {
        return iosb->Status == STATUS_SUCCESS && iosb->Information == count && BytesMatch(seed);
    }
};

static uint32_t OpenIoFixture(const char* path, uint32_t desiredAccess = 0x80000000)
{
    auto* name = static_cast<char*>(g_userHeap.Alloc(128));
    auto* ansi = g_userHeap.Alloc<XANSI_STRING>();
    auto* attributes = g_userHeap.Alloc<XOBJECT_ATTRIBUTES>();
    auto* result = g_userHeap.Alloc<be<uint32_t>>();
    auto* iosb = static_cast<XIO_STATUS_BLOCK*>(g_userHeap.Alloc(sizeof(XIO_STATUS_BLOCK)));
    std::strcpy(name, path);
    ansi->Buffer = name;
    ansi->Length = uint16_t(std::strlen(name));
    ansi->MaximumLength = uint16_t(std::strlen(name) + 1);
    *attributes = {};
    attributes->Name = ansi;
    const auto status = Call(__imp__NtCreateFile, {Addr(result), desiredAccess, Addr(attributes), Addr(iosb), 0, 0, 1, 1, 0x40});
    const uint32_t handle = *result;
    g_userHeap.Free(name); g_userHeap.Free(ansi); g_userHeap.Free(attributes); g_userHeap.Free(result); g_userHeap.Free(iosb);
    IoCheck(status == STATUS_SUCCESS, "open isolated fixture through NtCreateFile");
    return handle;
}

static void CloseIoFixture(uint32_t handle)
{
    IoCheck(Call(__imp__NtClose, {handle}) == STATUS_SUCCESS, "close fixture through NtClose");
}

static void CheckInvalidIoHandle()
{
    auto* event = g_userHeap.Alloc<be<uint32_t>>();
    IoCheck(Call(__imp__NtCreateEvent, {Addr(event), 0, 1, 0}) == STATUS_SUCCESS, "create handle for invalid I/O test");
    const uint32_t invalid = *event;
    CloseIoFixture(invalid);
    IoCheck(!GetKernelObject(invalid), "invalid I/O test handle was not closed");

#ifdef _WIN32
    SYSTEM_INFO system{};
    GetSystemInfo(&system);
    const uint32_t pageSize = system.dwPageSize;
#else
    const long systemPageSize = sysconf(_SC_PAGESIZE);
    IoCheck(systemPageSize > 0, "query OS page size for invalid I/O test");
    const uint32_t pageSize = uint32_t(systemPageSize);
#endif
    const uint32_t offset = g_pageAllocator.Alloc(g_pageAllocator.virtualRegion, pageSize, pageSize);
    IoCheck(offset != 0, "allocate guest page for unreadable offset");
    void* offsetPage = g_memory.Translate(offset);
#ifdef _WIN32
    DWORD oldProtection = 0;
    IoCheck(VirtualProtect(offsetPage, pageSize, PAGE_NOACCESS, &oldProtection) != 0,
        "protect guest offset page against reads");
    MEMORY_BASIC_INFORMATION protectedPage{};
    IoCheck(VirtualQuery(offsetPage, &protectedPage, sizeof(protectedPage)) == sizeof(protectedPage) &&
        protectedPage.Protect == PAGE_NOACCESS, "offset page is not actually inaccessible");
#else
    IoCheck(mprotect(offsetPage, pageSize, PROT_NONE) == 0, "protect guest offset page against reads");
#endif
    // The argument remains a real guest pointer through the import bridge.
    // Any premature read of ByteOffset faults on the protected page.
    auto* iosb = static_cast<XIO_STATUS_BLOCK*>(g_userHeap.Alloc(sizeof(XIO_STATUS_BLOCK)));
    auto* bytes = static_cast<uint8_t*>(g_userHeap.Alloc(IoRead::count));
    auto* segments = g_userHeap.Alloc<be<uint64_t>>();
    *segments = Addr(bytes);
    const std::array<PPCFunc*, 3> imports{__imp__NtReadFile, __imp__NtWriteFile, __imp__NtReadFileScatter};
    const std::array<const char*, 3> names{"NtReadFile", "NtWriteFile", "NtReadFileScatter"};
    for (size_t i = 0; i < imports.size(); ++i)
    {
        iosb->Status = 0xDEADBEEF;
        iosb->Information = 0xDEADBEEF;
        std::memset(bytes, 0xA5, IoRead::count);
        const auto status = Call(imports[i], {invalid, 0, 0, 0, Addr(iosb),
            i == 2 ? Addr(segments) : Addr(bytes), IoRead::count, offset});
        IoCheck(status == STATUS_INVALID_HANDLE, "invalid handle I/O did not reject before reading the protected offset");
        IoCheck(iosb->Status == 0xDEADBEEF && iosb->Information == 0xDEADBEEF &&
            std::all_of(bytes, bytes + IoRead::count, [](uint8_t byte) { return byte == 0xA5; }),
            "invalid handle I/O changed caller output");
        std::printf("PASS: %s rejects a closed handle without reading the protected offset\n", names[i]);
    }
#ifdef _WIN32
    DWORD replacedProtection = 0;
    IoCheck(VirtualProtect(offsetPage, pageSize, oldProtection, &replacedProtection) != 0,
        "restore protected guest page");
#else
    IoCheck(mprotect(offsetPage, pageSize, PROT_READ | PROT_WRITE) == 0, "restore protected guest page");
#endif
    g_pageAllocator.Free(g_pageAllocator.virtualRegion, offset);
    g_userHeap.Free(event); g_userHeap.Free(iosb); g_userHeap.Free(bytes); g_userHeap.Free(segments);
}

static void CheckReadCloseLifetime(file_system_test::Stage pauseAt)
{
    const uint32_t original = OpenIoFixture("game:\\first.bin");
    std::weak_ptr<KernelObject> lifetime = GetKernelObject(original);
    IoRead read(131);
    IoReadGate gate(original, pauseAt);
    IoTask reader("close-racing read did not complete", [&] { read.Run(original); });
    gate.Wait(pauseAt);
    IoTask closer("NtClose waited for in-flight file I/O", [&] { CloseIoFixture(original); });
    closer.Join();
    IoCheck(!lifetime.expired(), "NtClose destroyed the acquired file while a read still owns it");
    IoCheck(!GetKernelObject(original), "closed handle remains in the registry");
    IoRead invalid;
    invalid.Run(original);
    IoCheck(invalid.status == STATUS_INVALID_HANDLE, "read accepted the closed handle");

    // The old request must keep its own FILE even when a different file is
    // opened after the public handle was closed. Numeric reuse is allocator-dependent.
    const uint32_t replacement = OpenIoFixture("game:\\second.bin");
    IoRead newRead(131);
    newRead.Run(replacement);
    IoCheck(newRead.status == STATUS_SUCCESS && newRead.Complete(193), "new file returned the old file's bytes");
    CloseIoFixture(replacement);
    IoCheck(!lifetime.expired(), "unrelated close released the in-flight file");
    gate.Release();
    reader.Join();
    IoCheck(read.status == STATUS_SUCCESS && read.Complete(), "close changed the in-flight read's file or bytes");
    IoCheck(lifetime.expired(), "file survives after its final read and handle are released");
    IoCheck(gate.Count(file_system_test::Stage::CompletionPublished) == 1, "read published completion more than once");
    std::printf("PASS: NtClose at %s preserves in-flight file and isolates a new open\n",
        pauseAt == file_system_test::Stage::HandleAcquired ? "handle acquisition" : "I/O lock acquisition");
}

static void CheckDuplicateLifetime()
{
    auto* duplicate = g_userHeap.Alloc<be<uint32_t>>();
    for (unsigned closeOrder = 0; closeOrder < 3; ++closeOrder)
    {
        const uint32_t original = OpenIoFixture("game:\\first.bin");
        std::weak_ptr<KernelObject> lifetime = GetKernelObject(original);
        IoCheck(Call(__imp__NtDuplicateObject, {original, Addr(duplicate), closeOrder == 2 ? 1u : 0u}) == STATUS_SUCCESS,
            "duplicate file through NtDuplicateObject");
        const uint32_t copy = *duplicate;
        IoCheck(copy != original && GetKernelObject(copy) != nullptr, "duplicate did not create a distinct live handle");
        uint32_t survivor;
        if (closeOrder == 0) { CloseIoFixture(original); survivor = copy; }
        else if (closeOrder == 1) { CloseIoFixture(copy); survivor = original; }
        else { IoCheck(!GetKernelObject(original), "DUPLICATE_CLOSE_SOURCE kept its source handle"); survivor = copy; }
        IoCheck(!lifetime.expired(), "closing one duplicate destroyed the shared file");
        IoRead read(509);
        read.Run(survivor);
        IoCheck(read.status == STATUS_SUCCESS && read.Complete(), "surviving duplicate lost its file");
        CloseIoFixture(survivor);
        IoCheck(lifetime.expired(), "closing all duplicates retained the file");
    }
    g_userHeap.Free(duplicate);
    std::puts("PASS: NtDuplicateObject survives both close orders and DUPLICATE_CLOSE_SOURCE");
}

static std::pair<uint64_t, uint64_t> CheckBlockedIoSnapshot(uint32_t file)
{
    io_diagnostics::SnapshotData snapshot;
    IoTask observer("manual I/O snapshot waited for a file lock", [&] {
        snapshot = io_diagnostics::Snapshot();
        LoDumpIoDiagnostics("io-diagnostics-paused.jsonl");
    });
    observer.Join();
    IoCheck(snapshot.enabled && snapshot.finishedNs >= snapshot.capturedNs,
        "diagnostic snapshot was disabled or had reversed timestamps");
    const io_diagnostics::Record* owner = nullptr;
    const io_diagnostics::Record* waiter = nullptr;
    for (const auto& record : snapshot.active)
    {
        if (record.handle != file || std::strcmp(record.operation, "NtReadFile") != 0) continue;
        if (record.stage == io_diagnostics::Stage::IoLockAcquired) owner = &record;
        if (record.stage == io_diagnostics::Stage::IoLockWait) waiter = &record;
    }
    IoCheck(owner && waiter, "snapshot omitted the controlled file lock owner or waiter");
    IoCheck(owner->lockHeld && !waiter->lockHeld && owner->request != waiter->request && owner->hostTid != waiter->hostTid,
        "snapshot confused lock ownership or requesting threads");
    IoCheck(owner->objectInstance && owner->objectInstance == waiter->objectInstance &&
        owner->objectAddress == waiter->objectAddress && owner->mutexAddress && owner->mutexAddress == waiter->mutexAddress,
        "snapshot did not identify the shared file instance and mutex");
    IoCheck(waiter->ownerObservationStable && waiter->observedOwnerRequest == owner->request &&
        waiter->observedOwnerTid == owner->hostTid && waiter->snapshotOwnerResolved &&
        waiter->snapshotOwnerRequest == owner->request && waiter->snapshotOwnerTid == owner->hostTid &&
        !waiter->ownerChangedWhileWaiting, "snapshot failed to associate the waiter with its current owner");
    IoCheck(owner->requestedOffset == 97 && waiter->requestedOffset == 769 && owner->length == IoRead::count &&
        waiter->length == IoRead::count && std::string_view(owner->path).ends_with("first.bin") &&
        std::strcmp(owner->path, waiter->path) == 0, "snapshot lost the file path, offsets, or lengths");

    std::ifstream dump("io-diagnostics-paused.jsonl");
    std::string line;
    unsigned summaries = 0, activeReads = 0;
    while (std::getline(dump, line))
    {
        const auto record = nlohmann::json::parse(line);
        if (record.at("type") == "summary")
        {
            ++summaries;
            IoCheck(record.at("enabled") == true && record.at("active_capacity") == io_diagnostics::ActiveCapacity &&
                record.at("history_capacity") == io_diagnostics::HistoryCapacity,
                "manual dump omitted enabled state or recorder bounds");
        }
        if (record.at("type") == "active" && record.at("handle") == file && record.at("operation") == "NtReadFile")
            ++activeReads;
    }
    IoCheck(dump.eof() && summaries == 1 && activeReads == 2, "manual JSONL dump omitted the two blocked read records");
    std::printf("PASS: manual snapshot sees owner request %llu and waiter %llu on object %llu; JSONL retained\n",
        static_cast<unsigned long long>(owner->request), static_cast<unsigned long long>(waiter->request),
        static_cast<unsigned long long>(owner->objectInstance));
    return {owner->request, waiter->request};
}

static void CheckFinishedIoSnapshot(std::pair<uint64_t, uint64_t> requests)
{
    const auto snapshot = io_diagnostics::Snapshot();
    const io_diagnostics::Record* completedOwner = nullptr;
    for (const auto request : {requests.first, requests.second})
    {
        IoCheck(std::none_of(snapshot.active.begin(), snapshot.active.end(),
            [&](const auto& record) { return record.request == request; }), "finished read remained active in diagnostics");
        uint64_t transferred = 0, published = 0, unlocked = 0;
        bool returned = false;
        for (const auto& record : snapshot.history)
        {
            if (record.request != request) continue;
            if (!transferred && record.stage == io_diagnostics::Stage::TransferDone) transferred = record.sequence;
            if (!unlocked && record.stage == io_diagnostics::Stage::IoLockReleased && !record.lockHeld) unlocked = record.sequence;
            if (record.stage == io_diagnostics::Stage::CompletionPublished)
            {
                IoCheck(!record.lockHeld, "completion was published while the file I/O mutex was held");
                if (!published) published = record.sequence;
            }
            returned |= record.stage == io_diagnostics::Stage::ApiReturn && !record.lockHeld && record.hasResult &&
                record.status == STATUS_SUCCESS && record.transferred == IoRead::count &&
                record.resolvedOffset == (request == requests.first ? 97u : 769u);
            if (request == requests.first && record.stage == io_diagnostics::Stage::ApiReturn) completedOwner = &record;
        }
        IoCheck(transferred && unlocked && published && returned, "completed read history lost its transfer, publication, unlock, or result");
        IoCheck(transferred < unlocked && unlocked < published,
            "read completion history did not release the I/O mutex between transfer and completion publication");
    }
    IoCheck(completedOwner != nullptr, "completed owner identity missing from diagnostic history");
    uint64_t closeSequence = 0, destroySequence = 0;
    unsigned closeCount = 0;
    for (const auto& record : snapshot.history)
    {
        if (record.stage == io_diagnostics::Stage::CloseBegin && record.handle == completedOwner->handle)
            IoCheck(record.objectInstance == 0, "close attempt prematurely attributed a previously observed object");
        if (record.stage == io_diagnostics::Stage::CloseEnd && record.handle == completedOwner->handle)
        {
            ++closeCount;
            closeSequence = record.sequence;
            IoCheck(record.objectInstance == completedOwner->objectInstance && record.objectAddress == completedOwner->objectAddress,
                "CloseEnd did not identify the file object actually removed from the handle table");
        }
        if (record.stage == io_diagnostics::Stage::Destroyed && record.objectInstance == completedOwner->objectInstance)
            destroySequence = record.sequence;
    }
    IoCheck(closeCount == 1 && closeSequence && destroySequence > closeSequence,
        "actual close identity was not recorded before final file destruction");
    IoCheck(snapshot.active.size() <= io_diagnostics::ActiveCapacity && snapshot.history.size() <= io_diagnostics::HistoryCapacity,
        "diagnostic storage exceeded its advertised bounds");
    LoDumpIoDiagnostics("io-diagnostics-complete.jsonl");
    std::puts("PASS: completed history records read stages and the actual closed file before destruction");
}

static void CheckIndependentIo(bool diagnostics = false)
{
    const uint32_t shared = OpenIoFixture("game:\\first.bin");
    const uint32_t independent = OpenIoFixture("game:\\second.bin");
    IoRead first(97), second(769), separate(205);
    IoReadGate gate(shared, file_system_test::Stage::IoLockAcquired);
    IoTask owner("I/O lock owner did not complete", [&] { first.Run(shared); });
    gate.Wait(file_system_test::Stage::IoLockAcquired);
    IoTask waiter("same-file waiter did not complete", [&] { second.Run(shared); });
    gate.Wait(file_system_test::Stage::HandleAcquired, 2);
    IoTask other("independent file was blocked by a different file's lock", [&] {
        separate.Run(independent);
        CloseIoFixture(independent);
    });
    other.Join();
    IoCheck(separate.status == STATUS_SUCCESS && separate.Complete(193), "independent file read did not finish correctly");
    IoCheck(gate.Count(file_system_test::Stage::IoLockAcquired) == 1, "same-file waiter bypassed its owner's lock");
    std::pair<uint64_t, uint64_t> requests{};
    if (diagnostics)
    {
        gate.Wait(file_system_test::Stage::IoLockWaiting, 2);
        requests = CheckBlockedIoSnapshot(shared);
    }
    gate.Release();
    owner.Join(); waiter.Join();
    IoCheck(first.status == STATUS_SUCCESS && first.Complete() && second.status == STATUS_SUCCESS && second.Complete(),
        "serialized reads lost their individual offsets");
    CloseIoFixture(shared);
    if (diagnostics) CheckFinishedIoSnapshot(requests);
    std::puts("PASS: independent file I/O and close progress while a shared file has an owner and waiter");
}

struct IoApcObservation
{
    IoRead* read;
    uint32_t context;
    std::thread::id issuer;
    std::atomic<unsigned> calls{0};
    std::atomic<unsigned> failures{0};
};
static IoApcObservation* s_ioApcObservation;

static PPC_FUNC(IoApcCallback)
{
    auto& observation = *s_ioApcObservation;
    ++observation.calls;
    if (std::this_thread::get_id() != observation.issuer || ctx.r3.u32 != observation.context ||
        ctx.r4.u32 != Addr(observation.read->iosb) || ctx.r5.u32 != 0 || !observation.read->Complete())
        ++observation.failures;
}

static void CheckIoCompletion()
{
    const uint32_t file = OpenIoFixture("game:\\first.bin");
    std::weak_ptr<KernelObject> lifetime = GetKernelObject(file);
    auto* eventOut = g_userHeap.Alloc<be<uint32_t>>();
    auto* timeout = g_userHeap.Alloc<be<int64_t>>();
    *timeout = 0;
    IoCheck(Call(__imp__NtCreateEvent, {Addr(eventOut), 0, 1, 0}) == STATUS_SUCCESS, "create auto-reset I/O event");
    const uint32_t event = *eventOut;
    IoRead read(313);
    IoApcObservation observation{&read, 0x53C0FFEE};
    s_ioApcObservation = &observation;
    PPCFunc* previous = g_memory.FindFunction(PPC_CODE_BASE);
    g_memory.InsertFunction(PPC_CODE_BASE, IoApcCallback);
    IoReadGate gate(file, file_system_test::Stage::TransferDone);
    IoCheck(Call(__imp__NtWaitForSingleObjectEx, {event, 0, 0, Addr(timeout)}) == STATUS_TIMEOUT,
        "I/O event was signaled before the read");
    IoTask issuer("issuing thread did not complete its alertable waits", [&] {
        observation.issuer = std::this_thread::get_id();
        read.Run(file, event, PPC_CODE_BASE | 1u, observation.context);
        IoCheck(observation.calls == 0, "APC ran before an alertable wait");
        IoCheck(Call(__imp__NtWaitForSingleObjectEx, {event, 0, 0, Addr(timeout)}) == STATUS_TIMEOUT && observation.calls == 0,
            "non-alertable wait delivered an APC");
        IoCheck(Call(__imp__NtWaitForSingleObjectEx, {event, 0, 1, Addr(timeout)}) == STATUS_USER_APC,
            "issuing thread's alertable wait did not deliver completion");
        IoCheck(observation.calls == 1 && observation.failures == 0, "APC arguments, bytes, count, or issuing thread differ");
        IoCheck(Call(__imp__NtWaitForSingleObjectEx, {event, 0, 1, Addr(timeout)}) == STATUS_TIMEOUT && observation.calls == 1,
            "completion APC was delivered more than once");
    });
    gate.Wait(file_system_test::Stage::TransferDone);
    IoCheck(read.BytesMatch() && read.iosb->Status == 0xDEADBEEF && read.iosb->Information == 0xDEADBEEF,
        "transfer boundary did not precede IOSB publication");
    IoCheck(Call(__imp__NtWaitForSingleObjectEx, {event, 0, 1, Addr(timeout)}) == STATUS_TIMEOUT && observation.calls == 0,
        "I/O event was signaled before IOSB publication");
    gate.ContinueTo(file_system_test::Stage::BeforeCompletion);
    gate.Wait(file_system_test::Stage::BeforeCompletion);
    IoRead second(901);
    IoTask other("same-file read was blocked by a pending completion", [&] { second.Run(file); });
    other.Join();
    IoCheck(second.status == STATUS_SUCCESS && second.Complete(), "same-file read lost its bytes while the first completion was paused");
    CloseIoFixture(file);
    IoCheck(!lifetime.expired(), "closing the handle destroyed the file before read completion publication");
    IoCheck(read.iosb->Status == 0xDEADBEEF && read.iosb->Information == 0xDEADBEEF &&
        Call(__imp__NtWaitForSingleObjectEx, {event, 0, 1, Addr(timeout)}) == STATUS_TIMEOUT && observation.calls == 0,
        "another request completed the paused read's IOSB, event, or APC");
    gate.ContinueTo(file_system_test::Stage::CompletionPublished);
    gate.Wait(file_system_test::Stage::CompletionPublished, 2);
    IoCheck(read.Complete(), "event became observable before buffer and IOSB were complete");
    IoCheck(Call(__imp__NtWaitForSingleObjectEx, {event, 0, 1, Addr(timeout)}) == STATUS_SUCCESS && observation.calls == 0,
        "completion event missing or another thread delivered the issuing thread's APC");
    IoCheck(Call(__imp__NtWaitForSingleObjectEx, {event, 0, 1, Addr(timeout)}) == STATUS_TIMEOUT && observation.calls == 0,
        "one completion left multiple consumable event notifications");
    gate.Release();
    issuer.Join();
    IoCheck(read.status == STATUS_SUCCESS && gate.Count(file_system_test::Stage::CompletionPublished) == 2,
        "the two reads did not each publish one successful completion");
    IoCheck(lifetime.expired(), "read retained the closed file after completing");
    g_memory.InsertFunction(PPC_CODE_BASE, previous);
    s_ioApcObservation = nullptr;
    CloseIoFixture(event);
    g_userHeap.Free(eventOut); g_userHeap.Free(timeout);
    std::puts("PASS: pending read completion permits same-file I/O and survives close; event/IOSB and one issuing-thread APC remain correct");
}

static void CheckWriteAndScatterCompletion(bool writing)
{
    const uint8_t seed = writing ? 67 : 11;
    const uint32_t file = OpenIoFixture(writing ? "game:\\write.bin" : "game:\\first.bin",
        writing ? 0xC0000000u : 0x80000000u);
    std::weak_ptr<KernelObject> lifetime = GetKernelObject(file);
    auto* eventOut = g_userHeap.Alloc<be<uint32_t>>();
    auto* timeout = g_userHeap.Alloc<be<int64_t>>();
    auto* segments = g_userHeap.Alloc<be<uint64_t>>();
    *timeout = 0;
    IoCheck(Call(__imp__NtCreateEvent, {Addr(eventOut), 0, 1, 0}) == STATUS_SUCCESS, "create write/scatter auto-reset event");
    const uint32_t event = *eventOut;
    IoRead request(173), readback(173);
    *segments = Addr(request.bytes);
    if (writing)
        for (uint32_t i = 0; i < IoRead::count; ++i) request.bytes[i] = uint8_t((173 + i) * 37 + seed);
    IoReadGate gate(file, file_system_test::Stage::BeforeCompletion);
    IoTask issuer("write/scatter request did not finish publication", [&] {
        request.status = Call(writing ? __imp__NtWriteFile : __imp__NtReadFileScatter,
            {file, event, 0, 0, Addr(request.iosb), writing ? Addr(request.bytes) : Addr(segments),
                IoRead::count, Addr(request.offset)});
    });
    gate.Wait(file_system_test::Stage::BeforeCompletion);
    IoCheck(request.BytesMatch(seed) && request.iosb->Status == 0xDEADBEEF && request.iosb->Information == 0xDEADBEEF,
        "write/scatter transfer did not precede its IOSB publication");
    IoCheck(Call(__imp__NtWaitForSingleObjectEx, {event, 0, 0, Addr(timeout)}) == STATUS_TIMEOUT,
        "write/scatter signaled before completion publication");
    IoTask other("write/scatter pending completion blocked same-file I/O", [&] { readback.Run(file); });
    other.Join();
    IoCheck(readback.status == STATUS_SUCCESS && readback.Complete(seed), "same-file readback failed before write/scatter completion");
    CloseIoFixture(file);
    IoCheck(!lifetime.expired(), "close destroyed a write/scatter request before completion publication");
    gate.Release();
    issuer.Join();
    IoCheck(request.status == STATUS_SUCCESS && request.Complete(seed), "write/scatter completion bytes or IOSB differ");
    IoCheck(Call(__imp__NtWaitForSingleObjectEx, {event, 0, 0, Addr(timeout)}) == STATUS_SUCCESS &&
        Call(__imp__NtWaitForSingleObjectEx, {event, 0, 0, Addr(timeout)}) == STATUS_TIMEOUT,
        "write/scatter did not publish one consumable completion event");
    IoCheck(lifetime.expired(), "write/scatter retained the file after its final completion");
    CloseIoFixture(event);
    g_userHeap.Free(eventOut); g_userHeap.Free(timeout); g_userHeap.Free(segments);
    std::printf("PASS: %s permits same-file I/O before completion and preserves bytes/IOSB/event across close\n",
        writing ? "NtWriteFile" : "NtReadFileScatter");
}

static void CheckIoLifetime(bool diagnosticsOnly = false)
{
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    if (diagnosticsOnly) IoCheck(io_diagnostics::Enabled(), "io-diagnostics requires LO_IO_DIAGNOSTICS=1 before process startup");
    const auto game = std::filesystem::absolute(diagnosticsOnly ? "io-diagnostics-fixture-game" : "io-lifetime-fixture-game");
    IoCheck(!std::filesystem::exists(game), "I/O fixture destination must be new");
    std::filesystem::create_directories(game);
    for (const auto& [name, seed] : {std::pair{"first.bin", 11}, std::pair{"second.bin", 193}, std::pair{"write.bin", 7}})
    {
        std::ofstream output(game / name, std::ios::binary);
        for (unsigned i = 0; i < 4096; ++i) output.put(char(uint8_t(i * 37 + seed)));
        IoCheck(bool(output), "write isolated I/O fixture");
    }
    FileSystem::Init(game);
    XamInit();
    if (diagnosticsOnly)
    {
        CheckIndependentIo(true);
        std::filesystem::remove_all(game);
        return;
    }
    CheckReadCloseLifetime(file_system_test::Stage::HandleAcquired);
    CheckReadCloseLifetime(file_system_test::Stage::IoLockAcquired);
    CheckDuplicateLifetime();
    CheckIndependentIo();
    CheckIoCompletion();
    CheckWriteAndScatterCompletion(true);
    CheckWriteAndScatterCompletion(false);
    const uint32_t concurrent = OpenIoFixture("game:\\first.bin");
    CheckConcurrentReads(concurrent);
    CloseIoFixture(concurrent);
    std::filesystem::remove_all(game);
    std::puts("PASS: deterministic guest I/O lifetime regression (synthetic fixture; not an Issue #53 gameplay reproduction)");
}

// Exercise the real MMIO bridge and decoder worker without private audio data.
static void CheckXmaCommands()
{
    apu::xma::Init();
    std::array<be<uint32_t>*, 32> contexts{};
    const uint32_t output = g_pageAllocator.Alloc(g_pageAllocator.physicalRegion, 8192, 4096);
    Check(output != 0, "XMA test output allocation");
    for (unsigned i = 0; i < contexts.size(); ++i)
    {
        const uint32_t address = apu::xma::AllocateContext();
        Check(address != 0, "XMA test context allocation");
        contexts[i] = static_cast<be<uint32_t>*>(g_memory.Translate(address));
        contexts[i][0] = 0x00300000;
        contexts[i][1] = 0x80000000;
        contexts[i][9] = 3;
    }
    for (unsigned i = 0; i < contexts.size(); ++i)
    {
        LoMmioStore32(g_memory.base, 0x7FEA1A80, ByteSwap(1u << i));
        Check((uint32_t(contexts[i][0]) & 0x00300000) == 0 &&
              (uint32_t(contexts[i][1]) & 0x80000000) == 0 && uint32_t(contexts[i][9]) == 0,
              "each clear command must complete before MMIO returns");
        contexts[i][0] = 2u << 22;
        contexts[i][1] = 0x80000000;
        contexts[i][7] = output & 0x1fffffff;
    }
    // Empty input makes every requested context finish without FFmpeg data.
    // Consecutive one-bit stores must not replace earlier unprocessed kicks.
    for (unsigned i = 0; i < contexts.size(); ++i)
        LoMmioStore32(g_memory.base, 0x7FEA1940, ByteSwap(1u << i));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    unsigned pending;
    do
    {
        pending = 0;
        for (auto* context : contexts) pending += (uint32_t(context[1]) >> 31);
        if (!pending) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    } while (std::chrono::steady_clock::now() < deadline);
    Check(pending == 0, "all 32 separately kicked contexts must run");
    apu::xma::Shutdown();
    std::this_thread::sleep_for(std::chrono::milliseconds(10));
    std::puts("PASS: 32 synchronous XMA clears and 32 consecutive MMIO kicks");
}

static void CheckDirectoryFilter()
{
    const auto game = std::filesystem::absolute("directory-fixture-game");
    Check(!std::filesystem::exists(game), "directory fixture destination must be new");
    std::filesystem::create_directories(game);
    for (const char* name : {"00.FPI", "10.fpi", "spa.bin", "zz.bin"})
    {
        std::ofstream file(game / name, std::ios::binary);
        file << name;
        Check(bool(file), "write isolated directory fixture");
    }
    FileSystem::Init(game);
    XamInit();
    // Match the game's XDCn root path without importing a content package again.
    const auto gameUtf8 = FileSystem::PathUtf8(game);
    XamRootCreate("XDC0", gameUtf8.c_str());
    auto* name = static_cast<char*>(g_userHeap.Alloc(256));
    auto* ansi = g_userHeap.Alloc<XANSI_STRING>();
    auto* attributes = g_userHeap.Alloc<XOBJECT_ATTRIBUTES>();
    auto* handleOut = g_userHeap.Alloc<be<uint32_t>>();
    auto* iosb = static_cast<XIO_STATUS_BLOCK*>(g_userHeap.Alloc(sizeof(XIO_STATUS_BLOCK)));
    auto* output = static_cast<uint8_t*>(g_userHeap.Alloc(512));
    auto setName = [&](const char* value) {
        strcpy(name, value);
        ansi->Buffer = name;
        ansi->Length = uint16_t(strlen(name));
        ansi->MaximumLength = uint16_t(strlen(name) + 1);
    };
    auto open = [&] {
        setName("XDC0:\\");
        *attributes = {};
        attributes->Name = ansi;
        Check(Call(__imp__NtCreateFile, {Addr(handleOut), 0x80000000, Addr(attributes),
            Addr(iosb), 0, 0, 1, 1, 1}) == STATUS_SUCCESS, "open directory through guest import");
        return uint32_t(*handleOut);
    };
    unsigned queries = 0;
    auto query = [&](uint32_t handle, const char* pattern, bool restart = false) {
        if (pattern) setName(pattern);
        memset(output, 0xA5, 512);
        iosb->Status = 0xDEADBEEF;
        iosb->Information = 0xDEADBEEF;
        const auto status = Call(__imp__NtQueryDirectoryFile, {handle, 0, 0, 0,
            Addr(iosb), Addr(output), 512, pattern ? Addr(ansi) : 0, uint32_t(restart)});
        ++queries;
        Check(iosb->Status == status, "directory IO status matches return value");
        if (status == STATUS_NO_MORE_FILES)
        {
            Check(iosb->Information == 0, "exhausted enumeration reports no output bytes");
            Check(std::all_of(output, output + 512, [](uint8_t byte) { return byte == 0xA5; }),
                "exhausted enumeration leaves caller output untouched");
            std::printf("query %u: NO_MORE_FILES\n", queries);
            return std::string{};
        }
        Check(status == STATUS_SUCCESS, "directory query succeeds or reaches clean end");
        // Xbox FILE_DIRECTORY_INFORMATION uses a 64-byte header and ANSI names.
        const uint32_t length = *reinterpret_cast<be<uint32_t>*>(output + 60);
        Check(length && length <= 512 - 64 && iosb->Information == 64 + length,
            "directory filename length stays within returned bytes");
        Check(*reinterpret_cast<be<uint32_t>*>(output) == 0, "one directory record per query");
        Check(std::all_of(output + 64 + length, output + 512,
            [](uint8_t byte) { return byte == 0xA5; }), "directory query preserves output tail");
        std::string result(reinterpret_cast<char*>(output + 64), length);
        std::printf("query %u: %s\n", queries, result.c_str());
        return result;
    };
    auto collect = [&](uint32_t handle, const char* pattern, bool restart = false) {
        std::vector<std::string> found;
        for (unsigned i = 0; i < 8; ++i)
        {
            auto item = query(handle, i ? nullptr : pattern, !i && restart);
            if (item.empty()) { std::sort(found.begin(), found.end()); return found; }
            found.push_back(std::move(item));
        }
        throw std::runtime_error("directory enumeration must terminate");
    };
    const std::vector<std::string> fpiNames{"00.FPI", "10.fpi"};
    const std::vector<std::string> binNames{"spa.bin", "zz.bin"};
    const uint32_t fpi = open(), bin = open();
    Check(collect(fpi, "*.fpi") == fpiNames,
        "null FindNext must retain *.fpi and exclude spa.bin");
    Check(query(fpi, nullptr).empty(), "repeated exhausted query stays exhausted");
    Check(collect(fpi, nullptr, true) == fpiNames, "null RestartScan retains filter");
    const auto first = query(fpi, "*.FpI");
    Check(std::find(fpiNames.begin(), fpiNames.end(), first) != fpiNames.end(),
        "nonempty mixed-case filter starts a new search");
    // Reusing guest string storage must not change a different handle's rule.
    Check(collect(bin, "*.BIN") == binNames, "different handle selects only binary files");
    const auto second = query(fpi, "");
    Check(second != first && std::find(fpiNames.begin(), fpiNames.end(), second) != fpiNames.end(),
        "empty descriptor preserves owned filter and cursor after another handle queries");
    Check(query(fpi, nullptr).empty(), "filtered continuation stops after both matching files");
    Check(collect(fpi, "SPA.BIN") == std::vector<std::string>{"spa.bin"},
        "replacement exact filter resets cursor and matches case-insensitively");
    Check(collect(bin, nullptr, true) == binNames, "other handle restart retains its filter");
    const uint32_t all = open();
    const std::vector<std::string> allNames{"00.FPI", "10.fpi", "spa.bin", "zz.bin"};
    Check(collect(all, nullptr) == allNames, "fresh null filter includes all files");
    Check(collect(fpi, "*") == allNames, "explicit wildcard can replace a previous filter");
    Check(collect(fpi, "absent.fpi").empty(), "no-match filter terminates without output");
    Check(query(fpi, nullptr).empty(), "no-match null continuation preserves filter");
    DestroyKernelObject(fpi);
    DestroyKernelObject(bin);
    DestroyKernelObject(all);
    g_userHeap.Free(name); g_userHeap.Free(ansi); g_userHeap.Free(attributes);
    g_userHeap.Free(handleOut); g_userHeap.Free(iosb); g_userHeap.Free(output);
    std::printf("PASS directory-filter: %u actual NtQueryDirectoryFile calls\n", queries);
}

static void CheckDlc(const std::filesystem::path& imported, bool restart)
{
    using json = nlohmann::json;
    auto read = [](const std::filesystem::path& path) {
        std::ifstream file(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(file), {});
    };
    auto write = [](const std::filesystem::path& path, const std::string& bytes) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        file.write(bytes.data(), bytes.size());
        Check(bool(file), "fixture writes only its isolated copy");
    };
    const auto game = restart ? imported : std::filesystem::absolute("dlc-fixture-game");
    if (!restart)
    {
        Check(!std::filesystem::exists(game), "DLC fixture destination must be new");
        std::filesystem::create_directories(game / "disc1");
        std::filesystem::create_directories(game / "disc2");
        write(game / "disc1/default.xex", "synthetic disc marker");
        write(game / "disc2/default.xex", "synthetic disc marker");
        std::filesystem::copy(imported / "dlc", game / "dlc", std::filesystem::copy_options::recursive);
    }
    FileSystem::Init(game / "disc2");
    XamInit();
    const auto packages = DlcContent::Discover(game / "disc2");
    Check(!packages.empty(), "parser-imported DLC is discoverable from disc2 shared root");
    auto enumerate = [](uint32_t user, uint32_t device) {
        std::vector<XCONTENT_DATA> found;
        be<uint32_t> bytes{}, handle{}, count{};
        Check(XamContentCreateEnumerator(user, device, 2, 0, 30, &bytes, &handle) == 0,
            "DLC enumerator matches guest type2/device0/fetch30 ABI");
        XCONTENT_DATA items[30]{};
        while (true)
        {
            const auto status = XamEnumerate(handle, 0, items, sizeof(items), &count, nullptr);
            if (status == ERROR_NO_MORE_FILES) break;
            Check(status == 0 && count > 0 && count <= 30, "DLC enumerate count");
            found.insert(found.end(), items, items + count);
        }
        DestroyKernelObject(handle);
        return found;
    };
    const auto listed = enumerate(0, 0);
    Check(listed.size() == packages.size(), "XAM enumerates every validated DLC");
    Check(enumerate(0xFFFFFFFF, 1).size() == listed.size(), "all-users DLC enumeration");
    Check(enumerate(0, 99).empty(), "DLC device filter excludes disconnected device");
    be<uint32_t> enumBytes{}, enumHandle{};
    {
        GuestThreadContext thread(0);
        Check(XamContentCreateEnumerator(7, 0, 2, 0, 1, &enumBytes, &enumHandle) == 0xFFFFFFFF &&
            GuestThread::GetLastError() == ERROR_NO_SUCH_USER, "invalid DLC user rejected");
        g_ppcContext = nullptr;
    }
    auto* content = g_userHeap.Alloc<XCONTENT_DATA>();
    auto* root = static_cast<char*>(g_userHeap.Alloc(32));
    strcpy(root, "DlcTeSt");
    auto* disposition = g_userHeap.Alloc<be<uint32_t>>();
    auto* license = g_userHeap.Alloc<be<uint32_t>>();
    auto* event = g_userHeap.Alloc<be<uint32_t>>();
    Check(Call(__imp__NtCreateEvent, {Addr(event),0,0,0,0}) == 0, "DLC completion event");
    auto* ov = static_cast<XXOVERLAPPED*>(g_userHeap.Alloc(sizeof(XXOVERLAPPED)));
    auto openContent = [&](uint32_t mode) {
        *ov = {}; ov->hEvent = *event; ov->Error = ERROR_IO_PENDING;
        return Call(__imp__XamContentCreateEx, {0,Addr(root),Addr(content),mode,Addr(disposition),Addr(license),0,0,Addr(ov)});
    };
    size_t payloadFiles = 0, payloadBytes = 0;
    for (const auto& package : packages)
    {
        *content = package.data;
        Check(openContent(3) == ERROR_IO_PENDING && ov->Error == 0 && ov->dwExtendedError == 0 &&
            ov->Length == XCONTENT_EXISTING && *disposition == XCONTENT_EXISTING && *license == package.licenseMask,
            "DLC async open, disposition and license metadata");
        Check(GetKernelObject(*event)->Wait(0) == STATUS_SUCCESS, "DLC async completion signals event");
        const auto metadata = json::parse(read(package.root / ".lo-dlc.json"));
        for (const auto& entry : metadata.at("files"))
        {
            const auto relative = entry.at("path").get<std::string>();
            const auto source = package.root / std::filesystem::u8path(relative);
            const auto expected = read(source);
            const std::string guest = "DLCTEST:\\" + relative;
            Check(FileSystem::ResolvePath(guest) == source, "DLC guest root resolves exact shared payload");
            auto* name = static_cast<char*>(g_userHeap.Alloc(guest.size() + 1));
            memcpy(name, guest.c_str(), guest.size() + 1);
            auto* ansi = g_userHeap.Alloc<XANSI_STRING>();
            ansi->Length = uint16_t(guest.size()); ansi->MaximumLength = uint16_t(guest.size() + 1); ansi->Buffer = name;
            auto* attributes = g_userHeap.Alloc<XOBJECT_ATTRIBUTES>();
            *attributes = {}; attributes->Name = ansi;
            auto* file = g_userHeap.Alloc<be<uint32_t>>();
            auto* iosb = static_cast<XIO_STATUS_BLOCK*>(g_userHeap.Alloc(sizeof(XIO_STATUS_BLOCK)));
            Check(Call(__imp__NtCreateFile,{Addr(file),0x80000000u,Addr(attributes),Addr(iosb),0,0,0,1,0}) == 0,
                "open imported DLC payload through actual guest import");
            auto* bytes = static_cast<char*>(g_userHeap.Alloc(4096));
            auto* offset = g_userHeap.Alloc<be<uint64_t>>();
            for (size_t position = 0; position < expected.size(); position += 4096)
            {
                const auto count = uint32_t(std::min<size_t>(4096, expected.size() - position));
                *offset = position;
                Check(Call(__imp__NtReadFile,{*file,0,0,0,Addr(iosb),Addr(bytes),count,Addr(offset)}) == 0 &&
                    iosb->Information == count && memcmp(bytes, expected.data() + position, count) == 0,
                    "guest DLC bytes equal actual importer output");
            }
            DestroyKernelObject(*file);
            ++payloadFiles; payloadBytes += expected.size();
        }
        Check(Call(__imp__XamContentClose,{Addr(root),Addr(ov)}) == ERROR_IO_PENDING && ov->Error == 0,
            "DLC async close");
        Check(FileSystem::ResolvePath("dlctest:/payload").empty(), "DLC close unmounts root");
        Check(openContent(3) == ERROR_IO_PENDING && ov->Error == 0, "DLC same-process reopen");
        Check(Call(__imp__XamContentClose,{Addr(root),Addr(ov)}) == ERROR_IO_PENDING && ov->Error == 0, "DLC close reopened root");
        Check(openContent(2) == ERROR_IO_PENDING && ov->Error == ERROR_FUNCTION_FAILED &&
            ov->dwExtendedError == (0x80070000u | ERROR_ACCESS_DENIED), "DLC replacement is refused");
    }
    if (!restart)
    {
        const auto package = packages.front().root;
        const auto metadataPath = package / ".lo-dlc.json";
        const auto original = read(metadataPath);
        auto invalid = [&](const json& data, const char* message) {
            write(metadataPath, data.dump());
            Check(enumerate(0,0).size() == listed.size() - 1, message);
            write(metadataPath, original);
        };
        auto data = json::parse(original);
        data["title_id"] = "00000000"; invalid(data, "wrong-title DLC hidden");
        data = json::parse(original); data["files"][0]["path"] = "../outside.bin"; invalid(data, "unsafe DLC path hidden");
        data = json::parse(original); data["files"][0]["size"] = data["files"][0]["size"].get<uint64_t>() + 1;
        invalid(data, "wrong payload length hidden");
        data = json::parse(original); data["files"][0]["path"] = "missing.bin"; invalid(data, "missing payload hidden");
        const auto contentPath = package / ".lo-content";
        const auto originalContent = read(contentPath);
        auto damaged = originalContent; damaged[7] = 1; write(contentPath, damaged);
        Check(enumerate(0,0).size() == listed.size() - 1, "wrong binary content type hidden");
        write(contentPath, originalContent);
        const auto withheld = game / "withheld-package";
        std::filesystem::rename(package, withheld);
        Check(enumerate(0,0).size() == listed.size() - 1, "removed DLC purged from registry");
        std::filesystem::rename(withheld, package);
        Check(enumerate(0,0).size() == listed.size(), "restored DLC rediscovered");
        std::filesystem::rename(package, withheld);
        std::error_code ec;
        std::filesystem::create_directory_symlink(withheld, package, ec);
        if (!ec)
        {
            Check(enumerate(0,0).size() == listed.size() - 1, "DLC directory symlink ignored even with matching ID");
            std::filesystem::remove(package);
        }
        else std::printf("BOUNDARY: symlink creation unavailable: %s\n", ec.message().c_str());
        std::filesystem::rename(withheld, package);
    }
    DestroyKernelObject(*event);
    std::printf("PASS: DLC %s, %zu packages, %zu payload files, %zu bytes through guest imports\n",
        restart ? "fresh-process restart" : "shared-root/async/open/read/reopen/negative cases", packages.size(), payloadFiles, payloadBytes);
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 3 || argc == 4, "usage: LoStorageTest <mode> <isolated directory> [disc-set directory]");
        std::filesystem::create_directories(argv[2]);
        std::filesystem::current_path(argv[2]);
        if (argc == 4 && std::string_view(argv[3]) == "unicode")
        {
            const std::filesystem::path directory = u8"Steamn\u00b4t games \u5b58\u6863";
            std::filesystem::create_directories(directory);
            std::filesystem::current_path(directory);
        }
        g_userHeap.Init();
        g_pageAllocator.Init();
        if (std::string_view(argv[1]) == "discs" || std::string_view(argv[1]) == "disc-rejected")
        { Check(argc == 4,"discs requires absolute disc-set directory"); CheckDiscs(argv[3], std::string_view(argv[1]) == "disc-rejected"); return 0; }
        if (std::string_view(argv[1]) == "xma-commands") { CheckXmaCommands(); return 0; }
        if (std::string_view(argv[1]) == "directory-filter") { CheckDirectoryFilter(); return 0; }
        if (std::string_view(argv[1]) == "io-lifetime") { CheckIoLifetime(); return 0; }
        if (std::string_view(argv[1]) == "io-diagnostics") { CheckIoLifetime(true); return 0; }
        if (std::string_view(argv[1]) == "io-invalid-handle") { CheckInvalidIoHandle(); return 0; }
        if (std::string_view(argv[1]) == "dlc" || std::string_view(argv[1]) == "dlc-restart")
        { Check(argc == 4, "dlc requires parser-imported game root"); CheckDlc(argv[3], std::string_view(argv[1]) == "dlc-restart"); return 0; }
        FileSystem::Init(std::filesystem::absolute("game"));
        XamInit();
        const std::string_view mode(argv[1]);
        Check(mode == "write" || mode == "overwrite" || mode == "read" || mode == "read-overwritten", "invalid test mode");
        const bool overwrite = mode == "overwrite" || mode == "read-overwritten";
        const bool writing = mode == "write" || mode == "overwrite";
        auto* content = g_userHeap.Alloc<XCONTENT_DATA>();
        *content = XamMakeContent(1, "StorageIntegration");
        content->szDisplayName[0] = overwrite ? 'U' : 'T';
        auto* root = static_cast<char*>(g_userHeap.Alloc(32));
        strcpy(root, "SaVeTest");
        auto* disposition = g_userHeap.Alloc<be<uint32_t>>();
        auto* license = g_userHeap.Alloc<be<uint32_t>>();
        auto* event = g_userHeap.Alloc<be<uint32_t>>();
        Check(Call(__imp__NtCreateEvent, {Addr(event),0,0,0,0}) == 0, "create completion event");
        auto* ov = static_cast<XXOVERLAPPED*>(g_userHeap.Alloc(sizeof(XXOVERLAPPED)));
        memset(ov, 0, sizeof(*ov));
        ov->hEvent = *event;
        ov->Error = ERROR_IO_PENDING;
        if (!writing)
        {
            be<uint32_t> size{}, handle{}, count{};
            Check(XamContentCreateEnumerator(0,1,1,0,1,&size,&handle) == 0, "enumerate after process restart");
            XCONTENT_DATA listed{};
            Check(XamEnumerate(handle,0,&listed,sizeof(listed),&count,nullptr) == 0 && count == 1,
                "persisted content must be discoverable");
            Check(std::string_view(listed.szFileName) == content->szFileName && listed.szDisplayName[0] == (overwrite?'U':'T'), "metadata round trip");
            DestroyKernelObject(handle);
        }
        if (writing && overwrite)
        {
            Check(std::filesystem::exists(FileSystem::GetSaveRoot()/content->szFileName/"payload.bin"), "overwrite requires existing content");
            std::ofstream(FileSystem::GetSaveRoot()/content->szFileName/"stale.bin") << "old";
        }
        Check(Call(__imp__XamContentCreateEx, {0,Addr(root),Addr(content),writing?(overwrite?2u:1u):3u,Addr(disposition),Addr(license),0,0,Addr(ov)}) == ERROR_IO_PENDING, "create/open async return");
        Check(ov->Error == 0 && ov->dwExtendedError == 0 && ov->Length == (writing?1u:2u), "completion disposition");
        if (writing && overwrite)
        {
            Check(!std::filesystem::exists(FileSystem::GetSaveRoot()/content->szFileName/"stale.bin"), "CREATE_ALWAYS clears old container files");
            Check(!std::filesystem::exists(FileSystem::GetSaveRoot()/content->szFileName/"payload.bin"), "CREATE_ALWAYS permits a fresh FILE_CREATE");
        }
        Check(GetKernelObject(*event)->Wait(0) == STATUS_SUCCESS, "completion event signaled");
        Check(!FileSystem::ResolvePath("SAVETEST:\\payload.bin").empty(), "case-insensitive root");
        auto* name = static_cast<char*>(g_userHeap.Alloc(64));
        strcpy(name, "SAVETEST:\\payload.bin");
        auto* ansi = g_userHeap.Alloc<XANSI_STRING>();
        ansi->Length = uint16_t(strlen(name)); ansi->MaximumLength = 64; ansi->Buffer = name;
        auto* attrs = g_userHeap.Alloc<XOBJECT_ATTRIBUTES>();
        *attrs = {}; attrs->Name = ansi;
        auto* file = g_userHeap.Alloc<be<uint32_t>>();
        auto* iosb = static_cast<XIO_STATUS_BLOCK*>(g_userHeap.Alloc(sizeof(XIO_STATUS_BLOCK)));
        Check(Call(__imp__NtCreateFile, {Addr(file),writing?0x40000000u:0x80000000u,Addr(attrs),Addr(iosb),0,0,0,writing?2u:1u,0}) == 0, "open payload through guest import");
        auto* bytes = static_cast<uint8_t*>(g_userHeap.Alloc(4096));
        auto* offset = g_userHeap.Alloc<be<uint64_t>>(); *offset = 0;
        for (unsigned i=0;i<4096;i++) bytes[i] = writing ? uint8_t(i*37+11) : 0;
        Check(Call(writing?__imp__NtWriteFile:__imp__NtReadFile, {*file,0,0,0,Addr(iosb),Addr(bytes),4096,Addr(offset)}) == 0 && iosb->Information == 4096, "payload IO");
        if (!writing) for (unsigned i=0;i<4096;i++) Check(bytes[i] == uint8_t(i*37+11), "payload bytes after restart");
        Check(Call(__imp__NtFlushBuffersFile,{*file,Addr(iosb)}) == 0, "flush payload");
        if (!writing) CheckConcurrentReads(*file);
        DestroyKernelObject(*file);
        if (writing)
        {
            Check(Call(__imp__XamContentSetThumbnail, {0,Addr(content),Addr(bytes),16,Addr(ov)}) == ERROR_IO_PENDING && ov->Error == 0, "thumbnail five-argument ABI and completion");
            Check(std::filesystem::file_size(FileSystem::GetSaveRoot()/content->szFileName/".lo-thumbnail.png") == 16, "thumbnail persisted");
        }
        Check(Call(__imp__XamContentFlush,{Addr(root),Addr(ov)}) == ERROR_IO_PENDING && ov->Error == 0, "content flush completion");
        Check(Call(__imp__XamContentClose,{Addr(root),Addr(ov)}) == ERROR_IO_PENDING && ov->Error == 0, "close completion");
        Check(FileSystem::ResolvePath("savetest:\\payload.bin").empty(), "close unmounts root");
        Check(Call(__imp__XamContentCreateEx,{0,Addr(root),Addr(content),3,Addr(disposition),0,0,0,Addr(ov)}) == ERROR_IO_PENDING && ov->Error == 0,
            "reopen saved content in same process");
        Check(FileSystem::ResolvePath("savetest:\\payload.bin") == FileSystem::GetSaveRoot()/content->szFileName/"payload.bin",
            "rediscovered content preserves Unicode host path");
        Check(Call(__imp__NtCreateFile,{Addr(file),0x80000000u,Addr(attrs),Addr(iosb),0,0,0,1,0}) == 0,
            "reopen payload after content rediscovery");
        memset(bytes, 0, 4096);
        Check(Call(__imp__NtReadFile,{*file,0,0,0,Addr(iosb),Addr(bytes),4096,Addr(offset)}) == 0 && iosb->Information == 4096,
            "read reopened payload");
        for (unsigned i=0;i<4096;i++) Check(bytes[i] == uint8_t(i*37+11), "reopened payload bytes");
        DestroyKernelObject(*file);
        Check(Call(__imp__XamContentClose,{Addr(root),Addr(ov)}) == ERROR_IO_PENDING && ov->Error == 0, "close reopened content");
        Check(Call(__imp__XamContentCreateEx,{0,Addr(root),Addr(content),1,Addr(disposition),0,0,0,Addr(ov)}) == ERROR_IO_PENDING && ov->Error == ERROR_FUNCTION_FAILED && ov->dwExtendedError == (0x80070000u|ERROR_ALREADY_EXISTS), "create-new collision reports async HRESULT");
        std::puts(writing ? "PASS: guest save imports, completion event, thumbnail, collision" : "PASS: fresh-process enumeration and exact payload readback");
        return 0;
    }
    catch (const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
}
