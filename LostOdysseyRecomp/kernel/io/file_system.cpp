#include <stdafx.h>
#include "file_system.h"
#include <mutex>
#include <cpu/guest_thread.h>
#include <kernel/xam.h>
#include <kernel/xdm.h>
#include <kernel/function.h>
#include <os/logger.h>
#ifdef _WIN32
#include <io.h>

static std::mutex g_lastOpenedMutex;
static std::string g_lastOpenedFile;

#endif

// Semantics follow Xenia's kernel/xboxkrnl/xboxkrnl_io.cc (BSD-3).

void EnqueueUserApc(uint32_t routine, uint32_t context, uint32_t arg1, uint32_t arg2);

// Completion routines are delivered as user APCs on the issuing thread; the
// low bit of the routine pointer means "do not post to an IO completion port".
static void QueueIoApc(uint32_t apcRoutine, uint32_t apcContext, XIO_STATUS_BLOCK* iosb, uint32_t status)
{
    if ((apcRoutine & ~1u) && apcContext && status == STATUS_SUCCESS)
        EnqueueUserApc(apcRoutine & ~1u, apcContext, g_memory.MapVirtual(iosb), 0);
}

namespace
{
    std::filesystem::path g_gameRoot;
    std::filesystem::path g_saveRoot;
    std::filesystem::path g_cacheRoot;

    // Guest structures (big-endian in guest memory).
    struct X_FILE_NETWORK_OPEN_INFORMATION
    {
        be<uint64_t> creationTime;
        be<uint64_t> lastAccessTime;
        be<uint64_t> lastWriteTime;
        be<uint64_t> changeTime;
        be<uint64_t> allocationSize;
        be<uint64_t> endOfFile;
        be<uint32_t> attributes;
        be<uint32_t> pad;
    };
    static_assert(sizeof(X_FILE_NETWORK_OPEN_INFORMATION) == 56);

    struct X_FILE_BASIC_INFORMATION
    {
        be<uint64_t> creationTime;
        be<uint64_t> lastAccessTime;
        be<uint64_t> lastWriteTime;
        be<uint64_t> changeTime;
        be<uint32_t> attributes;
        be<uint32_t> pad;
    };

    struct X_FILE_STANDARD_INFORMATION
    {
        be<uint64_t> allocationSize;
        be<uint64_t> endOfFile;
        be<uint32_t> numberOfLinks;
        uint8_t deletePending;
        uint8_t directory;
        uint8_t pad[2];
    };

    struct X_FILE_DIRECTORY_INFORMATION
    {
        be<uint32_t> nextEntryOffset;
        be<uint32_t> fileIndex;
        be<uint64_t> creationTime;
        be<uint64_t> lastAccessTime;
        be<uint64_t> lastWriteTime;
        be<uint64_t> changeTime;
        be<uint64_t> endOfFile;
        be<uint64_t> allocationSize;
        be<uint32_t> attributes;
        be<uint32_t> fileNameLength;
        char fileName[1];
    };

    enum FileInformationClass : uint32_t
    {
        FileDirectoryInformation = 1,
        FileBasicInformation = 4,
        FileStandardInformation = 5,
        FileInternalInformation = 6,
        FileEaInformation = 7,
        FileAccessInformation = 8,
        FileNameInformation = 9,
        FileDispositionInformation = 13,
        FilePositionInformation = 14,
        FileModeInformation = 16,
        FileAlignmentInformation = 17,
        FileAllInformation = 18,
        FileAllocationInformation = 19,
        FileEndOfFileInformation = 20,
        FileNetworkOpenInformation = 34,
        FileAttributeTagInformation = 35,
    };

    enum FsInformationClass : uint32_t
    {
        FileFsVolumeInformation = 1,
        FileFsSizeInformation = 3,
        FileFsDeviceInformation = 4,
        FileFsAttributeInformation = 5,
    };

    constexpr uint32_t X_FILE_ATTRIBUTE_READONLY = 0x01;
    constexpr uint32_t X_FILE_ATTRIBUTE_DIRECTORY = 0x10;
    constexpr uint32_t X_FILE_ATTRIBUTE_NORMAL = 0x80;

    constexpr uint32_t FILE_SUPERSEDE = 0;
    constexpr uint32_t FILE_OPEN = 1;
    constexpr uint32_t FILE_CREATE = 2;
    constexpr uint32_t FILE_OPEN_IF = 3;
    constexpr uint32_t FILE_OVERWRITE = 4;
    constexpr uint32_t FILE_OVERWRITE_IF = 5;

    constexpr uint32_t FILE_DIRECTORY_FILE = 0x1;
    constexpr uint32_t FILE_NON_DIRECTORY_FILE = 0x40;

    uint64_t ToFileTime(std::filesystem::file_time_type t)
    {
        auto sys = std::chrono::clock_cast<std::chrono::system_clock>(t);
        constexpr int64_t EPOCH = 116444736000000000LL;
        return std::chrono::duration_cast<std::chrono::duration<int64_t, std::ratio<1, 10000000>>>(sys.time_since_epoch()).count() + EPOCH;
    }
}

struct FileHandle : KernelObject
{
    // CRT stream locks protect individual calls, not seek + transfer pairs.
    std::mutex ioMutex;
    std::filesystem::path path;
    FILE* file = nullptr;
    bool isDirectory = false;
    uint64_t position = 0;
    uint64_t size = 0;
    bool writable = false;

    // Directory enumeration state
    std::vector<std::filesystem::directory_entry> entries;
    size_t nextEntry = 0;
    bool enumerated = false;

    ~FileHandle() override
    {
        if (file)
            fclose(file);
    }

    uint32_t Wait(uint32_t) override { return STATUS_SUCCESS; }
};

void FileSystem::Init(const std::filesystem::path& gameRoot)
{
    g_gameRoot = gameRoot;
    g_saveRoot = std::filesystem::absolute("save");
    g_cacheRoot = std::filesystem::absolute("cache");
    std::error_code ec;
    std::filesystem::create_directories(g_saveRoot, ec);
    std::filesystem::create_directories(g_cacheRoot, ec);

    XamRootCreate("game", (const char*)g_gameRoot.u8string().c_str());
    XamRootCreate("d", (const char*)g_gameRoot.u8string().c_str());
    XamRootCreate("cache", (const char*)g_cacheRoot.u8string().c_str());
}

std::filesystem::path FileSystem::GetGameRoot() { return g_gameRoot; }
std::filesystem::path FileSystem::GetSaveRoot() { return g_saveRoot; }
std::filesystem::path FileSystem::GetCacheRoot() { return g_cacheRoot; }
std::filesystem::path GetGamePath() { return g_gameRoot; }
std::filesystem::path GetSavePath() { return g_saveRoot; }

static bool StartsWithNoCase(std::string_view s, std::string_view prefix)
{
    if (s.size() < prefix.size())
        return false;
    for (size_t i = 0; i < prefix.size(); i++)
        if (tolower((unsigned char)s[i]) != tolower((unsigned char)prefix[i]))
            return false;
    return true;
}

std::filesystem::path FileSystem::ResolvePath(std::string_view path)
{
    if (StartsWithNoCase(path, "\\??\\"))
        path.remove_prefix(4);

    std::string root;
    std::string_view rest;

    if (StartsWithNoCase(path, "\\Device\\Cdrom0"))
    {
        root = "game";
        rest = path.substr(14);
    }
    else if (StartsWithNoCase(path, "\\Device\\Harddisk0\\Partition"))
    {
        // Partition0 is the raw disk, Partition1 the cache partition; both
        // land in the scratch cache directory.
        root = "cache";
        rest = path.substr(28);
        if (!rest.empty() && isdigit((unsigned char)rest.front()))
            rest.remove_prefix(1);
    }
    else if (StartsWithNoCase(path, "\\Device\\"))
    {
        LOG_WARNING("unknown device path '{}'", path);
        return {};
    }
    else
    {
        size_t colon = path.find(':');
        if (colon == std::string_view::npos)
        {
            // Relative to the game root.
            root = "game";
            rest = path;
        }
        else
        {
            root = std::string(path.substr(0, colon));
            std::transform(root.begin(), root.end(), root.begin(), [](unsigned char c) { return (char)tolower(c); });
            rest = path.substr(colon + 1);
        }
    }

    const std::string hostRoot = XamGetRootPath(root);
    if (hostRoot.empty())
    {
        LOG_WARNING("unknown root '{}' in '{}'", root, path);
        return {};
    }

    std::string built(hostRoot);
    if (!rest.empty())
    {
        if (rest.front() != '\\' && rest.front() != '/')
            built += '/';
        built += rest;
    }
    std::replace(built.begin(), built.end(), '\\', '/');
    return std::u8string_view((const char8_t*)built.c_str());
}

static std::string GuestAnsiString(const XANSI_STRING* str)
{
    if (!str || !str->Buffer.get())
        return {};
    return std::string(str->Buffer.get(), str->Length.get());
}

static uint32_t OpenFileHandle(be<uint32_t>* FileHandleOut, uint32_t DesiredAccess, XOBJECT_ATTRIBUTES* Attributes,
    XIO_STATUS_BLOCK* IoStatusBlock, uint32_t CreateDisposition, uint32_t CreateOptions)
{
    std::string name = GuestAnsiString(Attributes ? Attributes->Name.get() : nullptr);
    const uint32_t rootDirectory = Attributes ? uint32_t(Attributes->RootDirectory) : 0;
    std::filesystem::path hostPath;
    LOG_KERNEL("open '{}' root={:#x} access={:#x} disposition={} options={:#x}", name, rootDirectory, DesiredAccess, CreateDisposition, CreateOptions);

    // Relative to another (file or directory) handle?
    if (rootDirectory != 0 && rootDirectory != GUEST_INVALID_HANDLE_VALUE && IsKernelObject(rootDirectory))
    {
        auto* parent = GetKernelObject<FileHandle>(rootDirectory);
        std::replace(name.begin(), name.end(), '\\', '/');
        hostPath = parent->path / std::u8string_view((const char8_t*)name.c_str());
    }

    if (hostPath.empty()) hostPath = FileSystem::ResolvePath(name);
    const bool wantWrite = (DesiredAccess & 0x40000000) != 0 || (DesiredAccess & 0x2) != 0 || (DesiredAccess & 0x4) != 0;

    std::error_code ec;
    bool exists = !hostPath.empty() && std::filesystem::exists(hostPath, ec);
    bool isDir = exists && std::filesystem::is_directory(hostPath, ec);

    uint32_t information = 0; // FILE_OPENED etc.
    if (hostPath.empty())
    {
        if (IoStatusBlock) { IoStatusBlock->Status = STATUS_OBJECT_PATH_NOT_FOUND; IoStatusBlock->Information = 0; }
        LOG_KERNEL("'{}' -> unresolved", name);
        return STATUS_OBJECT_PATH_NOT_FOUND;
    }

    switch (CreateDisposition)
    {
    case FILE_OPEN:
        if (!exists)
        {
            if (IoStatusBlock) { IoStatusBlock->Status = STATUS_OBJECT_NAME_NOT_FOUND; IoStatusBlock->Information = 0; }
            LOG_KERNEL("'{}' -> not found ({})", name, hostPath.string());
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
        information = 1; // FILE_OPENED
        break;
    case FILE_CREATE:
        if (exists)
        {
            if (IoStatusBlock) { IoStatusBlock->Status = 0xC0000035; IoStatusBlock->Information = 0; } // OBJECT_NAME_COLLISION
            return 0xC0000035;
        }
        information = 2; // FILE_CREATED
        break;
    case FILE_OPEN_IF:
        information = exists ? 1 : 2;
        break;
    case FILE_SUPERSEDE:
    case FILE_OVERWRITE:
    case FILE_OVERWRITE_IF:
        if (CreateDisposition == FILE_OVERWRITE && !exists)
        {
            if (IoStatusBlock) { IoStatusBlock->Status = STATUS_OBJECT_NAME_NOT_FOUND; IoStatusBlock->Information = 0; }
            return STATUS_OBJECT_NAME_NOT_FOUND;
        }
        information = exists ? 3 : 2; // FILE_OVERWRITTEN / FILE_CREATED
        break;
    }

    auto* handle = CreateKernelObject<FileHandle>();
    handle->path = hostPath;

    if (isDir || ((CreateOptions & FILE_DIRECTORY_FILE) && !exists))
    {
        handle->isDirectory = true;
        if (!exists)
            std::filesystem::create_directories(hostPath, ec);
    }
    else
    {
        const char* mode = "rb";
        if (wantWrite)
        {
            if (!exists || information == 2 || information == 3)
                mode = "w+b";
            else
                mode = "r+b";
            std::filesystem::create_directories(hostPath.parent_path(), ec);
        }
#ifdef _WIN32
        handle->file = _wfopen(hostPath.c_str(), mode == std::string("rb") ? L"rb" : mode == std::string("w+b") ? L"w+b" : L"r+b");
#else
        handle->file = fopen(hostPath.c_str(), mode);
#endif
        if (!handle->file)
        {
            DestroyKernelObject(handle);
            if (IoStatusBlock) { IoStatusBlock->Status = STATUS_ACCESS_DENIED; IoStatusBlock->Information = 0; }
            LOG_KERNEL("'{}' -> open failed ({})", name, hostPath.string());
            return STATUS_ACCESS_DENIED;
        }
        handle->writable = wantWrite;
        _fseeki64(handle->file, 0, SEEK_END);
        handle->size = uint64_t(_ftelli64(handle->file));
        _fseeki64(handle->file, 0, SEEK_SET);
    }

    *FileHandleOut = GetKernelHandle(handle);
    if (IoStatusBlock)
    {
        IoStatusBlock->Status = STATUS_SUCCESS;
        IoStatusBlock->Information = information;
    }
    LOG_INFO("open '{}' -> {} ({} bytes{})", name, hostPath.string(), handle->size, handle->isDirectory ? ", dir" : "");
    {
        std::lock_guard lock(g_lastOpenedMutex);
        g_lastOpenedFile = hostPath.filename().string();
    }
    return STATUS_SUCCESS;
}

std::string FileSystem::LastOpenedFile()
{
    std::lock_guard lock(g_lastOpenedMutex);
    return g_lastOpenedFile;
}

uint32_t NtCreateFile(be<uint32_t>* FileHandle, uint32_t DesiredAccess, XOBJECT_ATTRIBUTES* Attributes,
    XIO_STATUS_BLOCK* IoStatusBlock, be<uint64_t>* AllocationSize, uint32_t FileAttributes,
    uint32_t ShareAccess, uint32_t CreateDisposition, uint32_t CreateOptions)
{
    return OpenFileHandle(FileHandle, DesiredAccess, Attributes, IoStatusBlock, CreateDisposition, CreateOptions);
}

uint32_t NtOpenFile(be<uint32_t>* FileHandle, uint32_t DesiredAccess, XOBJECT_ATTRIBUTES* Attributes,
    XIO_STATUS_BLOCK* IoStatusBlock, uint32_t OpenOptions)
{
    return OpenFileHandle(FileHandle, DesiredAccess, Attributes, IoStatusBlock, FILE_OPEN, OpenOptions);
}

uint32_t NtReadFile(FileHandle* handle, uint32_t Event, uint32_t ApcRoutine, uint32_t ApcContext,
    XIO_STATUS_BLOCK* IoStatusBlock, void* Buffer, uint32_t Length, be<uint64_t>* ByteOffset)
{
    if (!handle || IsInvalidKernelObject(handle) || !handle->file)
        return STATUS_INVALID_HANDLE;

    std::lock_guard ioLock(handle->ioMutex);

    uint64_t offset = handle->position;
    if (ByteOffset)
    {
        uint64_t v = *ByteOffset;
        // FILE_USE_FILE_POINTER_POSITION == -2
        if (v != 0xFFFFFFFFFFFFFFFEull)
            offset = v;
    }

    _fseeki64(handle->file, int64_t(offset), SEEK_SET);
    size_t read = fread(Buffer, 1, Length, handle->file);
    handle->position = offset + read;
    LOG_KERNEL("{} off={:#x} len={:#x} -> {:#x}{}", handle->path.filename().string(), offset, Length, read, Event ? " (event)" : "");

    uint32_t status = STATUS_SUCCESS;
    if (read == 0 && Length != 0)
        status = STATUS_END_OF_FILE;

    if (IoStatusBlock)
    {
        IoStatusBlock->Status = status;
        IoStatusBlock->Information = uint32_t(read);
    }

    QueueIoApc(ApcRoutine, ApcContext, IoStatusBlock, status);

    // Async reads: signal the event the caller passed, like the kernel does
    // on completion.
    if (Event != 0)
    {
        extern void KernelSignalEventHandle(uint32_t handle);
        KernelSignalEventHandle(Event);
    }
    return status;
}

uint32_t NtWriteFile(FileHandle* handle, uint32_t Event, uint32_t ApcRoutine, uint32_t ApcContext,
    XIO_STATUS_BLOCK* IoStatusBlock, const void* Buffer, uint32_t Length, be<uint64_t>* ByteOffset)
{
    if (!handle || IsInvalidKernelObject(handle) || !handle->file || !handle->writable)
        return STATUS_INVALID_HANDLE;

    std::lock_guard ioLock(handle->ioMutex);

    uint64_t offset = handle->position;
    if (ByteOffset)
    {
        uint64_t v = *ByteOffset;
        if (v == 0xFFFFFFFFFFFFFFFFull) offset = handle->size;
        else if (v != 0xFFFFFFFFFFFFFFFEull)
            offset = v;
    }

    const bool seekOk = _fseeki64(handle->file, int64_t(offset), SEEK_SET) == 0;
    size_t written = seekOk ? fwrite(Buffer, 1, Length, handle->file) : 0;
    const bool flushOk = fflush(handle->file) == 0;
    const uint32_t status = seekOk && written == Length && flushOk ? STATUS_SUCCESS : 0xC0000185u;
    handle->position = offset + written;
    handle->size = std::max(handle->size, handle->position);

    if (IoStatusBlock)
    {
        IoStatusBlock->Status = status;
        IoStatusBlock->Information = uint32_t(written);
    }
    LOG_INFO("write '{}' offset={} bytes={}/{} status={:#x}", handle->path.string(), offset, written, Length, status);
    QueueIoApc(ApcRoutine, ApcContext, IoStatusBlock, status);
    if (Event != 0)
    {
        extern void KernelSignalEventHandle(uint32_t handle);
        KernelSignalEventHandle(Event);
    }
    return status;
}

uint32_t NtFlushBuffersFile(FileHandle* handle, XIO_STATUS_BLOCK* IoStatusBlock)
{
    uint32_t status = STATUS_INVALID_HANDLE;
    if (handle && !IsInvalidKernelObject(handle) && handle->file)
    {
        std::lock_guard ioLock(handle->ioMutex);
        status = fflush(handle->file) == 0 ? STATUS_SUCCESS : 0xC0000185u;
    }
    if (IoStatusBlock) { IoStatusBlock->Status = status; IoStatusBlock->Information = 0; }
    return status;
}

uint32_t NtQueryInformationFile(FileHandle* handle, XIO_STATUS_BLOCK* IoStatusBlock, void* FileInformation,
    uint32_t Length, uint32_t FileInformationClass)
{
    if (!handle || IsInvalidKernelObject(handle))
        return STATUS_INVALID_HANDLE;

    std::lock_guard ioLock(handle->ioMutex);

    uint32_t info = 0;
    uint32_t status = STATUS_SUCCESS;
    std::error_code ec;

    switch (FileInformationClass)
    {
    case FileInternalInformation:
        *reinterpret_cast<be<uint64_t>*>(FileInformation) = uint64_t(g_memory.MapVirtual(handle));
        info = 8;
        break;
    case FilePositionInformation:
        *reinterpret_cast<be<uint64_t>*>(FileInformation) = handle->position;
        info = 8;
        break;
    case FileStandardInformation:
    {
        auto* s = reinterpret_cast<X_FILE_STANDARD_INFORMATION*>(FileInformation);
        s->allocationSize = RoundUp<uint64_t>(handle->size, 0x800);
        s->endOfFile = handle->size;
        s->numberOfLinks = 1;
        s->deletePending = 0;
        s->directory = handle->isDirectory;
        info = sizeof(*s);
        break;
    }
    case FileBasicInformation:
    {
        auto* b = reinterpret_cast<X_FILE_BASIC_INFORMATION*>(FileInformation);
        uint64_t t = std::filesystem::exists(handle->path, ec) ? ToFileTime(std::filesystem::last_write_time(handle->path, ec)) : 0;
        b->creationTime = t; b->lastAccessTime = t; b->lastWriteTime = t; b->changeTime = t;
        b->attributes = handle->isDirectory ? X_FILE_ATTRIBUTE_DIRECTORY : X_FILE_ATTRIBUTE_NORMAL;
        info = sizeof(*b);
        break;
    }
    case FileNetworkOpenInformation:
    {
        auto* n = reinterpret_cast<X_FILE_NETWORK_OPEN_INFORMATION*>(FileInformation);
        uint64_t t = std::filesystem::exists(handle->path, ec) ? ToFileTime(std::filesystem::last_write_time(handle->path, ec)) : 0;
        n->creationTime = t; n->lastAccessTime = t; n->lastWriteTime = t; n->changeTime = t;
        n->allocationSize = RoundUp<uint64_t>(handle->size, 0x800);
        n->endOfFile = handle->size;
        n->attributes = handle->isDirectory ? X_FILE_ATTRIBUTE_DIRECTORY : X_FILE_ATTRIBUTE_NORMAL;
        n->pad = 0;
        info = sizeof(*n);
        break;
    }
    case FileNameInformation:
    {
        std::string name = "\\" + handle->path.filename().string();
        auto* p = reinterpret_cast<be<uint32_t>*>(FileInformation);
        *p = uint32_t(name.size());
        memcpy(p + 1, name.data(), std::min<size_t>(name.size(), Length - 4));
        info = 4 + uint32_t(name.size());
        break;
    }
    case FileAlignmentInformation:
        *reinterpret_cast<be<uint32_t>*>(FileInformation) = 0;
        info = 4;
        break;
    case FileModeInformation:
        *reinterpret_cast<be<uint32_t>*>(FileInformation) = 0;
        info = 4;
        break;
    default:
        LOG_KERNEL("unimplemented class {}", FileInformationClass);
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }

    if (IoStatusBlock) { IoStatusBlock->Status = status; IoStatusBlock->Information = info; }
    return status;
}

uint32_t NtSetInformationFile(FileHandle* handle, XIO_STATUS_BLOCK* IoStatusBlock, void* FileInformation,
    uint32_t Length, uint32_t FileInformationClass)
{
    if (!handle || IsInvalidKernelObject(handle))
        return STATUS_INVALID_HANDLE;

    std::lock_guard ioLock(handle->ioMutex);

    uint32_t status = STATUS_SUCCESS;
    switch (FileInformationClass)
    {
    case FilePositionInformation:
        handle->position = *reinterpret_cast<be<uint64_t>*>(FileInformation);
        break;
    case FileEndOfFileInformation:
    case FileAllocationInformation:
    {
        uint64_t size = *reinterpret_cast<be<uint64_t>*>(FileInformation);
        if (handle->file && handle->writable)
        {
            fflush(handle->file);
#ifdef _WIN32
            _chsize_s(_fileno(handle->file), int64_t(size));
#else
            ftruncate(fileno(handle->file), off_t(size));
#endif
            handle->size = size;
        }
        break;
    }
    case FileDispositionInformation: // delete on close
        break;
    default:
        LOG_KERNEL("unimplemented class {}", FileInformationClass);
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }
    if (IoStatusBlock) { IoStatusBlock->Status = status; IoStatusBlock->Information = 0; }
    return status;
}

uint32_t NtQueryVolumeInformationFile(FileHandle* handle, XIO_STATUS_BLOCK* IoStatusBlock, void* FsInformation,
    uint32_t Length, uint32_t FsInformationClass)
{
    uint32_t info = 0;
    uint32_t status = STATUS_SUCCESS;
    switch (FsInformationClass)
    {
    case FileFsVolumeInformation:
    {
        auto* p = reinterpret_cast<uint8_t*>(FsInformation);
        *reinterpret_cast<be<uint64_t>*>(p) = 0;          // creation time
        *reinterpret_cast<be<uint32_t>*>(p + 8) = 0x12345678; // serial
        *reinterpret_cast<be<uint32_t>*>(p + 12) = 0;     // label length
        p[16] = 0;                                        // supports objects
        info = 18;
        break;
    }
    case FileFsSizeInformation:
    {
        auto* p = reinterpret_cast<be<uint64_t>*>(FsInformation);
        p[0] = 0x1000000; // total allocation units
        p[1] = 0x800000;  // available
        reinterpret_cast<be<uint32_t>*>(FsInformation)[4] = 1;    // sectors per unit
        reinterpret_cast<be<uint32_t>*>(FsInformation)[5] = 0x800; // bytes per sector
        info = 24;
        break;
    }
    case FileFsDeviceInformation:
    {
        auto* p = reinterpret_cast<be<uint32_t>*>(FsInformation);
        p[0] = handle && !IsInvalidKernelObject(handle) && handle->path.string().starts_with(g_gameRoot.string()) ? 2 /* FILE_DEVICE_CD_ROM */ : 7 /* FILE_DEVICE_DISK */;
        p[1] = 0;
        info = 8;
        break;
    }
    case FileFsAttributeInformation:
    {
        auto* p = reinterpret_cast<be<uint32_t>*>(FsInformation);
        p[0] = 0; // attributes
        p[1] = 255; // max component length
        const char name[] = "FATX";
        p[2] = sizeof(name) - 1;
        memcpy(p + 3, name, sizeof(name) - 1);
        info = 12 + sizeof(name) - 1;
        break;
    }
    default:
        LOG_KERNEL("unimplemented class {}", FsInformationClass);
        status = STATUS_NOT_IMPLEMENTED;
        break;
    }
    if (IoStatusBlock) { IoStatusBlock->Status = status; IoStatusBlock->Information = info; }
    return status;
}

uint32_t NtQueryDirectoryFile(FileHandle* handle, uint32_t Event, uint32_t ApcRoutine, uint32_t ApcContext,
    XIO_STATUS_BLOCK* IoStatusBlock, void* FileInformation, uint32_t Length, XANSI_STRING* FileName, uint32_t RestartScan)
{
    if (!handle || IsInvalidKernelObject(handle) || !handle->isDirectory)
        return STATUS_INVALID_HANDLE;

    std::string pattern = GuestAnsiString(FileName);
    std::error_code ec;

    if (!handle->enumerated || RestartScan)
    {
        handle->entries.clear();
        for (auto& e : std::filesystem::directory_iterator(handle->path, ec))
            handle->entries.push_back(e);
        handle->nextEntry = 0;
        handle->enumerated = true;
    }

    auto matches = [&](const std::string& name)
    {
        if (pattern.empty() || pattern == "*" || pattern == "*.*")
            return true;
        // simple wildcard: prefix*suffix
        size_t star = pattern.find('*');
        if (star == std::string::npos)
            return _stricmp(name.c_str(), pattern.c_str()) == 0;
        std::string pre = pattern.substr(0, star), suf = pattern.substr(star + 1);
        if (name.size() < pre.size() + suf.size())
            return false;
        return _strnicmp(name.c_str(), pre.c_str(), pre.size()) == 0 &&
            _stricmp(name.c_str() + name.size() - suf.size(), suf.c_str()) == 0;
    };

    while (handle->nextEntry < handle->entries.size())
    {
        auto& e = handle->entries[handle->nextEntry++];
        std::string name = e.path().filename().string();
        if (!matches(name))
            continue;

        uint32_t needed = uint32_t(offsetof(X_FILE_DIRECTORY_INFORMATION, fileName) + name.size());
        if (needed > Length)
        {
            if (IoStatusBlock) { IoStatusBlock->Status = 0x80000005; IoStatusBlock->Information = 0; } // BUFFER_OVERFLOW
            return 0x80000005;
        }

        auto* d = reinterpret_cast<X_FILE_DIRECTORY_INFORMATION*>(FileInformation);
        memset(d, 0, needed);
        bool dir = e.is_directory(ec);
        uint64_t size = dir ? 0 : e.file_size(ec);
        uint64_t t = ToFileTime(e.last_write_time(ec));
        d->nextEntryOffset = 0;
        d->fileIndex = uint32_t(handle->nextEntry);
        d->creationTime = t; d->lastAccessTime = t; d->lastWriteTime = t; d->changeTime = t;
        d->endOfFile = size;
        d->allocationSize = RoundUp<uint64_t>(size, 0x800);
        d->attributes = dir ? X_FILE_ATTRIBUTE_DIRECTORY : X_FILE_ATTRIBUTE_NORMAL;
        d->fileNameLength = uint32_t(name.size());
        memcpy(d->fileName, name.data(), name.size());

        if (IoStatusBlock) { IoStatusBlock->Status = STATUS_SUCCESS; IoStatusBlock->Information = needed; }
        return STATUS_SUCCESS;
    }

    if (IoStatusBlock) { IoStatusBlock->Status = STATUS_NO_MORE_FILES; IoStatusBlock->Information = 0; }
    return STATUS_NO_MORE_FILES;
}

uint32_t NtQueryFullAttributesFile(XOBJECT_ATTRIBUTES* Attributes, X_FILE_NETWORK_OPEN_INFORMATION* Info)
{
    std::string name = GuestAnsiString(Attributes ? Attributes->Name.get() : nullptr);
    std::filesystem::path hostPath = FileSystem::ResolvePath(name);
    std::error_code ec;
    if (hostPath.empty() || !std::filesystem::exists(hostPath, ec))
    {
        LOG_KERNEL("'{}' -> not found", name);
        return STATUS_OBJECT_NAME_NOT_FOUND;
    }

    bool dir = std::filesystem::is_directory(hostPath, ec);
    uint64_t size = dir ? 0 : std::filesystem::file_size(hostPath, ec);
    uint64_t t = ToFileTime(std::filesystem::last_write_time(hostPath, ec));
    Info->creationTime = t; Info->lastAccessTime = t; Info->lastWriteTime = t; Info->changeTime = t;
    Info->allocationSize = RoundUp<uint64_t>(size, 0x800);
    Info->endOfFile = size;
    Info->attributes = dir ? X_FILE_ATTRIBUTE_DIRECTORY : X_FILE_ATTRIBUTE_NORMAL;
    Info->pad = 0;
    LOG_KERNEL("'{}' -> {} bytes", name, size);
    return STATUS_SUCCESS;
}

uint32_t NtDeviceIoControlFile(FileHandle* handle, uint32_t Event, uint32_t ApcRoutine, uint32_t ApcContext,
    XIO_STATUS_BLOCK* IoStatusBlock, uint32_t IoControlCode, void* InputBuffer, uint32_t InputBufferLength,
    void* OutputBuffer, uint32_t OutputBufferLength)
{
    LOG_KERNEL("ioctl {:#x} in={} out={}", IoControlCode, InputBufferLength, OutputBufferLength);
    if (IoStatusBlock) { IoStatusBlock->Status = STATUS_INVALID_DEVICE_REQUEST; IoStatusBlock->Information = 0; }
    return STATUS_INVALID_DEVICE_REQUEST;
}

// Scatter read: segments is an array of 64-bit guest pointers to page-sized
// buffers (FILE_SEGMENT_ELEMENT).
uint32_t NtReadFileScatter(FileHandle* handle, uint32_t Event, uint32_t ApcRoutine, uint32_t ApcContext,
    XIO_STATUS_BLOCK* IoStatusBlock, be<uint64_t>* SegmentArray, uint32_t Length, be<uint64_t>* ByteOffset)
{
    if (!handle || IsInvalidKernelObject(handle) || !handle->file)
        return STATUS_INVALID_HANDLE;

    std::lock_guard ioLock(handle->ioMutex);

    uint64_t offset = ByteOffset ? uint64_t(*ByteOffset) : handle->position;
    _fseeki64(handle->file, int64_t(offset), SEEK_SET);
    uint32_t total = 0;
    for (uint32_t remaining = Length, i = 0; remaining > 0; i++)
    {
        uint32_t chunk = std::min<uint32_t>(remaining, 0x1000);
        uint32_t guestPtr = uint32_t(uint64_t(SegmentArray[i]));
        size_t read = fread(g_memory.Translate(guestPtr), 1, chunk, handle->file);
        total += uint32_t(read);
        remaining -= chunk;
        if (read < chunk)
            break;
    }
    handle->position = offset + total;
    if (IoStatusBlock) { IoStatusBlock->Status = STATUS_SUCCESS; IoStatusBlock->Information = total; }
    QueueIoApc(ApcRoutine, ApcContext, IoStatusBlock, STATUS_SUCCESS);
    if (Event != 0)
    {
        extern void KernelSignalEventHandle(uint32_t handle);
        KernelSignalEventHandle(Event);
    }
    return STATUS_SUCCESS;
}

GUEST_FUNCTION_HOOK(__imp__NtCreateFile, NtCreateFile);
GUEST_FUNCTION_HOOK(__imp__NtOpenFile, NtOpenFile);
GUEST_FUNCTION_HOOK(__imp__NtReadFile, NtReadFile);
GUEST_FUNCTION_HOOK(__imp__NtWriteFile, NtWriteFile);
GUEST_FUNCTION_HOOK(__imp__NtFlushBuffersFile, NtFlushBuffersFile);
GUEST_FUNCTION_HOOK(__imp__NtQueryInformationFile, NtQueryInformationFile);
GUEST_FUNCTION_HOOK(__imp__NtSetInformationFile, NtSetInformationFile);
GUEST_FUNCTION_HOOK(__imp__NtQueryVolumeInformationFile, NtQueryVolumeInformationFile);
GUEST_FUNCTION_HOOK(__imp__NtQueryDirectoryFile, NtQueryDirectoryFile);
GUEST_FUNCTION_HOOK(__imp__NtQueryFullAttributesFile, NtQueryFullAttributesFile);
GUEST_FUNCTION_HOOK(__imp__NtDeviceIoControlFile, NtDeviceIoControlFile);
GUEST_FUNCTION_HOOK(__imp__NtReadFileScatter, NtReadFileScatter);
