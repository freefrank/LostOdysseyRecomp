#include <stdafx.h>
#include "xam.h"
#include "xdm.h"
#include "function.h"
#include <kernel/io/file_system.h>
#include <cpu/guest_thread.h>
#include <hid/hid.h>
#include <os/logger.h>
#include <ranges>
#include <unordered_set>

// XAM (Xbox Application Manager) layer: notifications, content roots, user
// profile and input. Modelled on UnleashedRecomp (GPLv3).

struct XamListener : KernelObject
{
    uint32_t id{};
    uint64_t areas{};
    std::vector<std::tuple<uint32_t, uint32_t>> notifications;

    XamListener();
    ~XamListener();
};

struct XamEnumeratorBase : KernelObject
{
    virtual uint32_t Next(void* buffer) { return -1; }
};

template<typename TIterator>
struct XamEnumerator : XamEnumeratorBase
{
    uint32_t fetch;
    size_t size;
    TIterator position;
    TIterator end;

    XamEnumerator(uint32_t fetch, size_t size, TIterator begin, TIterator end) : fetch(fetch), size(size), position(begin), end(end) {}

    uint32_t Next(void* buffer) override
    {
        if (position == end)
            return -1;

        for (uint32_t i = 0; i < fetch; i++)
        {
            if (position == end)
                return i == 0 ? -1 : i;

            if (buffer)
            {
                memcpy(buffer, &*position, size);
                buffer = (void*)((size_t)buffer + size);
            }
            ++position;
        }
        return fetch;
    }
};

static std::array<ankerl::unordered_dense::map<uint64_t, XHOSTCONTENT_DATA>, 3> g_contentRegistry{};
static std::unordered_set<XamListener*> g_listeners{};
static ankerl::unordered_dense::map<uint64_t, std::string> g_rootMap;
static Mutex g_xamMutex;
static std::recursive_mutex g_contentMutex;

static std::string NormalizeRoot(std::string_view root)
{
    if (!root.empty() && root.back() == ':') root.remove_suffix(1);
    std::string result(root);
    std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return char(std::tolower(c)); });
    return result;
}

extern std::filesystem::path GetSavePath();

static bool ValidContentName(const XCONTENT_DATA& data)
{
    if (!memchr(data.szFileName, 0, sizeof(data.szFileName))) return false;
    const std::string_view name(data.szFileName);
    return !name.empty() && name != "." && name != ".." && name.find_first_of("/\\:") == std::string_view::npos;
}

static void DiscoverSavedContent()
{
    std::error_code ec;
    for (const auto& entry : std::filesystem::directory_iterator(GetSavePath(), ec))
    {
        if (!entry.is_directory(ec)) continue;
        XCONTENT_DATA data{};
        std::ifstream in(entry.path() / ".lo-content", std::ios::binary);
        if (!in.read(reinterpret_cast<char*>(&data), sizeof(data)) ||
            data.dwContentType != XCONTENTTYPE_SAVEDATA || !ValidContentName(data) ||
            FileSystem::PathUtf8(entry.path().filename()) != data.szFileName) continue;
        XamRegisterContent(data, FileSystem::PathUtf8(entry.path()));
    }
}

struct ContentEnumerator : XamEnumeratorBase
{
    std::vector<XCONTENT_DATA> items;
    uint32_t fetch;
    size_t cursor = 0;
    uint32_t Next(void* buffer) override
    {
        if (cursor == items.size()) return uint32_t(-1);
        const uint32_t count = uint32_t(std::min<size_t>(fetch, items.size() - cursor));
        if (buffer) memcpy(buffer, items.data() + cursor, count * sizeof(XCONTENT_DATA));
        cursor += count;
        return count;
    }
};

void XamInit()
{
}

std::string XamGetRootPath(const std::string_view& root)
{
    std::lock_guard lock(g_xamMutex);
    const auto result = g_rootMap.find(StringHash(NormalizeRoot(root)));
    if (result == g_rootMap.end())
        return "";
    return result->second;
}

void XamRootCreate(const std::string_view& root, const std::string_view& path)
{
    std::lock_guard lock(g_xamMutex);
    g_rootMap.insert_or_assign(StringHash(NormalizeRoot(root)), std::string(path));
    LOG_KERNEL("root '{}' -> '{}'", root, path);
}

XamListener::XamListener()
{
    g_listeners.insert(this);
}

XamListener::~XamListener()
{
    g_listeners.erase(this);
}

XCONTENT_DATA XamMakeContent(uint32_t type, const std::string_view& name)
{
    XCONTENT_DATA data{ 1, type };
    strncpy(data.szFileName, name.data(), sizeof(data.szFileName));
    return data;
}

void XamRegisterContent(const XCONTENT_DATA& data, const std::string_view& root)
{
    std::lock_guard lock(g_contentMutex);
    const auto idx = data.dwContentType - 1;
    g_contentRegistry[idx].insert_or_assign(StringHash(data.szFileName), XHOSTCONTENT_DATA{ data }).first->second.szRoot = root;
}

uint32_t XamNotifyCreateListener(uint64_t qwAreas)
{
    auto* listener = CreateKernelObject<XamListener>();
    listener->areas = qwAreas;

    // Games expect the dashboard's startup notifications on their first
    // listener (Xenia KernelState::RegisterNotifyListener): the UI is closed,
    // user 0 is signed in locally, storage devices settled; Live disconnected.
    static bool notifiedSystem = false, notifiedLive = false;
    if (!notifiedSystem && (qwAreas & 1))
    {
        notifiedSystem = true;
        listener->notifications.emplace_back(MSGID(0, 0x0009), 0); // XN_SYS_UI
        listener->notifications.emplace_back(MSGID(0, 0x000A), 1); // XN_SYS_SIGNINCHANGED, user mask
        listener->notifications.emplace_back(MSGID(0, 0x000B), 0); // XN_SYS_STORAGEDEVICESCHANGED
    }
    if (!notifiedLive && (qwAreas & 2))
    {
        notifiedLive = true;
        listener->notifications.emplace_back(MSGID(1, 0x0001), 0x001510F1); // XN_LIVE_CONNECTIONCHANGED: logon disconnected
        listener->notifications.emplace_back(MSGID(1, 0x0002), 0);          // XN_LIVE_LINK_STATE_CHANGED
    }
    LOG_KERNEL("areas={:#x} -> {:#x}", qwAreas, GetKernelHandle(listener));
    return GetKernelHandle(listener);
}

void XamNotifyEnqueueEvent(uint32_t dwId, uint32_t dwParam)
{
    for (const auto& listener : g_listeners)
    {
        if (((1ull << MSG_AREA(dwId)) & listener->areas) == 0)
            continue;
        listener->notifications.emplace_back(dwId, dwParam);
    }
}

bool XNotifyGetNext(uint32_t hNotification, uint32_t dwMsgFilter, be<uint32_t>* pdwId, be<uint32_t>* pParam)
{
    auto& listener = *GetKernelObject<XamListener>(hNotification);

    if (dwMsgFilter)
    {
        for (size_t i = 0; i < listener.notifications.size(); i++)
        {
            if (std::get<0>(listener.notifications[i]) == dwMsgFilter)
            {
                if (pdwId) *pdwId = std::get<0>(listener.notifications[i]);
                if (pParam) *pParam = std::get<1>(listener.notifications[i]);
                listener.notifications.erase(listener.notifications.begin() + i);
                return true;
            }
        }
        return false;
    }

    if (listener.notifications.empty())
        return false;

    if (pdwId) *pdwId = std::get<0>(listener.notifications[0]);
    if (pParam) *pParam = std::get<1>(listener.notifications[0]);
    listener.notifications.erase(listener.notifications.begin());
    return true;
}

uint32_t XamContentCreateEnumerator(uint32_t dwUserIndex, uint32_t DeviceID, uint32_t dwContentType,
    uint32_t dwContentFlags, uint32_t cItem, be<uint32_t>* pcbBuffer, be<uint32_t>* phEnum)
{
    std::lock_guard lock(g_contentMutex);
    if (dwUserIndex != 0)
    {
        GuestThread::SetLastError(ERROR_NO_SUCH_USER);
        return 0xFFFFFFFF;
    }

    if (dwContentType < 1 || dwContentType > 3 || !phEnum || !cItem || cItem > 4096)
        return ERROR_INVALID_PARAMETER;
    DiscoverSavedContent();
    const auto& registry = g_contentRegistry[dwContentType - 1];
    auto* enumerator = CreateKernelObject<ContentEnumerator>();
    enumerator->fetch = cItem;
    for (const auto& [key, value] : registry) enumerator->items.push_back(value);

    if (pcbBuffer)
        *pcbBuffer = sizeof(_XCONTENT_DATA) * cItem;

    *phEnum = GetKernelHandle(enumerator);
    return 0;
}

void KernelSignalEventHandle(uint32_t handle);

// An enumerator with nothing in it (achievements, friends...).
uint32_t XamCreateEmptyEnumerator()
{
    return GetKernelHandle(CreateKernelObject<XamEnumeratorBase>());
}

uint32_t XamEnumerate(uint32_t hEnum, uint32_t dwFlags, void* pvBuffer, uint32_t cbBuffer, be<uint32_t>* pcItemsReturned, XXOVERLAPPED* pOverlapped)
{
    if (!IsKernelObject(hEnum))
        return ERROR_INVALID_HANDLE;
    auto* enumerator = GetKernelObject<XamEnumeratorBase>(hEnum);
    if (auto* content = dynamic_cast<ContentEnumerator*>(enumerator);
        content && (!pvBuffer || cbBuffer < sizeof(XCONTENT_DATA) * content->fetch))
        return ERROR_INSUFFICIENT_BUFFER;
    if (pcItemsReturned) *pcItemsReturned = 0;
    const auto count = enumerator->Next(pvBuffer);
    const uint32_t result = count == -1 ? ERROR_NO_MORE_FILES : ERROR_SUCCESS;

    if (count != -1 && pcItemsReturned)
        *pcItemsReturned = count;

    // Asynchronous form: the outcome travels through the overlapped block.
    if (pOverlapped)
    {
        CompleteOverlapped(pOverlapped, result, count == -1 ? 0 : uint32_t(count));
        return ERROR_IO_PENDING;
    }
    return result;
}

extern std::filesystem::path GetSavePath();
extern std::filesystem::path GetGamePath();

static uint32_t ContentCreate(uint32_t dwUserIndex, const char* szRootName, const XCONTENT_DATA* pContentData,
    uint32_t dwContentFlags, be<uint32_t>* pdwDisposition, be<uint32_t>* pdwLicenseMask,
    uint32_t dwFileCacheSize, uint64_t uliContentSize, PXXOVERLAPPED pOverlapped)
{
    std::lock_guard lock(g_contentMutex);
    if (!szRootName || !*szRootName || !pContentData || !ValidContentName(*pContentData) ||
        pContentData->dwContentType < 1 || pContentData->dwContentType > 3)
        return ERROR_INVALID_PARAMETER;
    DiscoverSavedContent();
    const auto& registry = g_contentRegistry[pContentData->dwContentType - 1];
    const auto exists = registry.contains(StringHash(pContentData->szFileName));
    const auto mode = dwContentFlags & 0xF;
    if (mode == 1 && exists) return ERROR_ALREADY_EXISTS;
    if (pdwLicenseMask) *pdwLicenseMask = 0;

    LOG_KERNEL("root='{}' file='{}' type={} mode={} exists={}", szRootName, pContentData->szFileName, (uint32_t)pContentData->dwContentType, mode, exists);

    if (mode == 2 /* CREATE_ALWAYS */ || mode == 1 /* CREATE_NEW */ || mode == 4 /* OPEN_ALWAYS */)
    {
        if (pdwDisposition)
            // CREATE_ALWAYS reports a new content object even when replacing
            // an existing slot (Xenia xeXamContentCreate).
            *pdwDisposition = exists && mode != 2 ? XCONTENT_EXISTING : XCONTENT_NEW;

        if (!exists || mode == 2)
        {
            std::filesystem::path rootPath;
            if (pContentData->dwContentType == XCONTENTTYPE_SAVEDATA)
                rootPath = GetSavePath() / pContentData->szFileName;
            else if (pContentData->dwContentType == XCONTENTTYPE_DLC)
                rootPath = GetGamePath() / "dlc" / pContentData->szFileName;
            else
                rootPath = GetGamePath();

            const std::string root = FileSystem::PathUtf8(rootPath);
            std::error_code ec;
            if (exists && mode == 2)
            {
                // Match CREATE_ALWAYS's container replacement semantics. Only
                // save containers are writable here; never clear the game root
                // or a redirected/symlinked directory for another content type.
                if (pContentData->dwContentType != XCONTENTTYPE_SAVEDATA)
                    return ERROR_ACCESS_DENIED;
                const auto saveRoot = std::filesystem::weakly_canonical(GetSavePath(), ec);
                if (ec) return ERROR_ACCESS_DENIED;
                const auto target = std::filesystem::weakly_canonical(rootPath, ec);
                if (ec || target.parent_path() != saveRoot)
                    return ERROR_ACCESS_DENIED;
                const bool symlink = std::filesystem::is_symlink(rootPath, ec);
                if (ec || symlink)
                    return ERROR_ACCESS_DENIED;
                std::filesystem::remove_all(target, ec);
                if (ec) return ERROR_ACCESS_DENIED;
            }
            std::filesystem::create_directories(rootPath, ec);
            if (ec) return ERROR_ACCESS_DENIED;
            if (pContentData->dwContentType == XCONTENTTYPE_SAVEDATA)
            {
                std::ofstream metadata(rootPath / ".lo-content", std::ios::binary | std::ios::trunc);
                metadata.write(reinterpret_cast<const char*>(pContentData), sizeof(*pContentData));
                metadata.close();
                if (!metadata) return ERROR_WRITE_FAULT;
            }
            XamRegisterContent(*pContentData, root);
            XamRootCreate(szRootName, root);
        }
        else
        {
            XamRootCreate(szRootName, registry.find(StringHash(pContentData->szFileName))->second.szRoot);
        }
        return ERROR_SUCCESS;
    }

    if (mode == 3 /* OPEN_EXISTING */)
    {
        if (exists)
        {
            if (pdwDisposition)
                *pdwDisposition = XCONTENT_EXISTING;
            XamRootCreate(szRootName, registry.find(StringHash(pContentData->szFileName))->second.szRoot);
            return ERROR_SUCCESS;
        }

        if (pdwDisposition)
            *pdwDisposition = XCONTENT_NEW;
        return ERROR_PATH_NOT_FOUND;
    }

    return ERROR_PATH_NOT_FOUND;
}

uint32_t XamContentCreateEx(uint32_t dwUserIndex, const char* szRootName, const XCONTENT_DATA* pContentData,
    uint32_t dwContentFlags, be<uint32_t>* pdwDisposition, be<uint32_t>* pdwLicenseMask,
    uint32_t dwFileCacheSize, uint64_t uliContentSize, PXXOVERLAPPED pOverlapped)
{
    be<uint32_t> disposition = 0;
    const uint32_t result = ContentCreate(dwUserIndex, szRootName, pContentData, dwContentFlags,
        &disposition, pdwLicenseMask, dwFileCacheSize, uliContentSize, nullptr);
    if (pdwDisposition) *pdwDisposition = disposition;
    if (pOverlapped)
    {
        // Xenia xeXamContentCreate: completion length carries create/open disposition;
        // errors use FUNCTION_FAILED plus an HRESULT in dwExtendedError.
        pOverlapped->dwExtendedError = result ? (0x80070000u | result) : 0;
        // Set the extended result before signaling the event / queueing the APC.
        pOverlapped->Error = result ? ERROR_FUNCTION_FAILED : ERROR_SUCCESS;
        pOverlapped->Length = disposition;
        if (pOverlapped->hEvent) KernelSignalEventHandle(pOverlapped->hEvent);
        // Use the shared completion dispatcher for callback-only operations below.
        if (pOverlapped->pCompletionRoutine)
        {
            extern void EnqueueUserApc(uint32_t routine, uint32_t arg1, uint32_t arg2, uint32_t arg3);
            EnqueueUserApc(pOverlapped->pCompletionRoutine, pOverlapped->Error, disposition,
                g_memory.MapVirtual(pOverlapped));
        }
    }
    LOG_INFO("content create '{}' mode={} -> {} disposition={} async={}", szRootName ? szRootName : "",
        dwContentFlags & 15, result, uint32_t(disposition), pOverlapped != nullptr);
    return pOverlapped ? ERROR_IO_PENDING : result;
}

uint32_t XamContentClose(const char* szRootName, XXOVERLAPPED* pOverlapped)
{
    {
        std::lock_guard lock(g_xamMutex);
        g_rootMap.erase(StringHash(NormalizeRoot(szRootName)));
    }
    CompleteOverlapped(pOverlapped, ERROR_SUCCESS, 0);
    return pOverlapped ? ERROR_IO_PENDING : ERROR_SUCCESS;
}

uint32_t XamContentGetDeviceData(uint32_t DeviceID, XDEVICE_DATA* pDeviceData)
{
    LOG_KERNEL("device={:#x}", DeviceID);
    if (DeviceID != 1)
        return 0x48F; // ERROR_DEVICE_NOT_CONNECTED
    // Same fiction as Xenia: a 20 GiB drive with 10 GiB free.
    pDeviceData->DeviceID = DeviceID;
    pDeviceData->DeviceType = XCONTENTDEVICETYPE_HDD;
    pDeviceData->ulDeviceBytes = 20ull << 30;
    pDeviceData->ulDeviceFreeBytes = 10ull << 30;
    const char name[] = "Hard Drive";
    for (size_t i = 0; i < sizeof(name); i++)
        pDeviceData->wszName[i] = name[i];
    return 0;
}

uint32_t XamInputGetCapabilities(uint32_t unk, uint32_t userIndex, uint32_t flags, XAMINPUT_CAPABILITIES* caps)
{
    uint32_t result = hid::GetCapabilities(userIndex, caps);
    if (result == ERROR_SUCCESS)
    {
        ByteSwapInplace(caps->Flags);
        ByteSwapInplace(caps->Gamepad.wButtons);
        ByteSwapInplace(caps->Gamepad.sThumbLX);
        ByteSwapInplace(caps->Gamepad.sThumbLY);
        ByteSwapInplace(caps->Gamepad.sThumbRX);
        ByteSwapInplace(caps->Gamepad.sThumbRY);
        ByteSwapInplace(caps->Vibration.wLeftMotorSpeed);
        ByteSwapInplace(caps->Vibration.wRightMotorSpeed);
    }
    return result;
}

uint32_t XamInputGetState(uint32_t userIndex, uint32_t flags, XAMINPUT_STATE* state)
{
    memset(state, 0, sizeof(*state));
    uint32_t result = hid::GetState(userIndex, state);

    ByteSwapInplace(state->dwPacketNumber);
    ByteSwapInplace(state->Gamepad.wButtons);
    ByteSwapInplace(state->Gamepad.sThumbLX);
    ByteSwapInplace(state->Gamepad.sThumbLY);
    ByteSwapInplace(state->Gamepad.sThumbRX);
    ByteSwapInplace(state->Gamepad.sThumbRY);
    return result;
}

uint32_t XamInputSetState(uint32_t userIndex, uint32_t flags, XAMINPUT_VIBRATION* vibration)
{
    ByteSwapInplace(vibration->wLeftMotorSpeed);
    ByteSwapInplace(vibration->wRightMotorSpeed);
    return hid::SetState(userIndex, vibration);
}
