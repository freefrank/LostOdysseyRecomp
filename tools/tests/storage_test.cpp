#include <stdafx.h>
#include <kernel/function.h>
#include <kernel/xdm.h>
#include <kernel/xam.h>
#include <kernel/io/file_system.h>
#include <stdexcept>
#include <apu/xma.h>
#include <gpu/ppc_mmio.h>

PPC_FUNC(__imp__XamContentCreateEx);
PPC_FUNC(__imp__XamContentClose);
PPC_FUNC(__imp__XamContentFlush);
PPC_FUNC(__imp__XamContentSetThumbnail);
PPC_FUNC(__imp__NtCreateEvent);
PPC_FUNC(__imp__NtCreateFile);
PPC_FUNC(__imp__NtWriteFile);
PPC_FUNC(__imp__NtReadFile);
PPC_FUNC(__imp__NtFlushBuffersFile);

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

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 3, "usage: LoStorageTest write|read|overwrite|read-overwritten <isolated directory>");
        std::filesystem::create_directories(argv[2]);
        std::filesystem::current_path(argv[2]);
        g_userHeap.Init();
        g_pageAllocator.Init();
        if (std::string_view(argv[1]) == "xma-commands") { CheckXmaCommands(); return 0; }
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
        Check(Call(__imp__XamContentCreateEx,{0,Addr(root),Addr(content),1,Addr(disposition),0,0,0,Addr(ov)}) == ERROR_IO_PENDING && ov->Error == ERROR_FUNCTION_FAILED && ov->dwExtendedError == (0x80070000u|ERROR_ALREADY_EXISTS), "create-new collision reports async HRESULT");
        std::puts(writing ? "PASS: guest save imports, completion event, thumbnail, collision" : "PASS: fresh-process enumeration and exact payload readback");
        return 0;
    }
    catch (const std::exception& e) { std::fprintf(stderr,"FAIL: %s\n",e.what()); return 1; }
}
