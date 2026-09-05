#include <stdafx.h>
#include <kernel/function.h>
#include <kernel/xdm.h>
#include <kernel/xam.h>
#include <kernel/io/file_system.h>
#include <stdexcept>

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

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 3, "usage: LoStorageTest write|read <isolated directory>");
        std::filesystem::create_directories(argv[2]);
        std::filesystem::current_path(argv[2]);
        g_userHeap.Init();
        g_pageAllocator.Init();
        FileSystem::Init(std::filesystem::absolute("game"));
        XamInit();
        const bool writing = std::string_view(argv[1]) == "write";
        auto* content = g_userHeap.Alloc<XCONTENT_DATA>();
        *content = XamMakeContent(1, "StorageIntegration");
        content->szDisplayName[0] = 'T';
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
            Check(std::string_view(listed.szFileName) == content->szFileName && listed.szDisplayName[0] == 'T', "metadata round trip");
            DestroyKernelObject(handle);
        }
        Check(Call(__imp__XamContentCreateEx, {0,Addr(root),Addr(content),writing?1u:3u,Addr(disposition),Addr(license),0,0,Addr(ov)}) == ERROR_IO_PENDING, "create/open async return");
        Check(ov->Error == 0 && ov->dwExtendedError == 0 && ov->Length == (writing?1u:2u), "completion disposition");
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
