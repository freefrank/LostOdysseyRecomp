#include <stdafx.h>
#include "xex_loader.h"
#include "memory.h"
#include "heap.h"
#include <os/logger.h>
#include <file.h>
#include <image.h>
#include <xex.h>

// Kernel export ordinals for the variables Lost Odyssey imports.
namespace
{
    struct VariableImport
    {
        const char* library;
        uint32_t ordinal;
        const char* name;
        uint32_t size;
        uint32_t* slot;
    };

    // KeTimeStampBundle layout (matches Xenia): interrupt_time, system_time in
    // 100 ns units, then the millisecond tick count.
    struct KeTimeStampBundle
    {
        be<uint64_t> interruptTime;
        be<uint64_t> systemTime;
        be<uint32_t> tickCount;
        be<uint32_t> padding;
    };

    uint64_t HostSystemTime100ns()
    {
        constexpr int64_t FILETIME_EPOCH_DIFFERENCE = 116444736000000000LL;
        auto now = std::chrono::system_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::duration<int64_t, std::ratio<1, 10000000>>>(now).count() + FILETIME_EPOCH_DIFFERENCE;
    }

    uint64_t HostInterruptTime100ns()
    {
        auto now = std::chrono::steady_clock::now().time_since_epoch();
        return std::chrono::duration_cast<std::chrono::duration<int64_t, std::ratio<1, 10000000>>>(now).count();
    }

    std::thread g_timeStampThread;
}

static uint32_t AllocGuestVariable(uint32_t size)
{
    void* p = g_userHeap.Alloc(std::max<uint32_t>(size, 16));
    memset(p, 0, size);
    return g_memory.MapVirtual(p);
}

uint32_t XexLoader::Load(const std::filesystem::path& xexPath)
{
    const auto file = LoadFile(xexPath);
    if (file.empty())
    {
        LOG_ERROR("failed to read {}", xexPath.string());
        return 0;
    }

    auto image = Image::ParseImage(file.data(), file.size());
    if (!image.data || image.size == 0)
    {
        LOG_ERROR("failed to parse XEX image");
        return 0;
    }

    s_imageBase = uint32_t(image.base);
    s_imageSize = image.size;
    s_entryPoint = uint32_t(image.entry_point);

    memcpy(g_memory.Translate(image.base), image.data.get(), image.size);
    LOG_INFO("image loaded at {:#x} size {:#x} entry {:#x}", image.base, image.size, image.entry_point);

    // Variable imports: the IAT slot must hold the guest address of the
    // variable. Walk the import table again to find the slots.
    auto* imports = reinterpret_cast<const Xex2ImportHeader*>(getOptHeaderPtr(file.data(), XEX_HEADER_IMPORT_LIBRARIES));
    if (imports == nullptr)
        return s_entryPoint;

    std::vector<std::string_view> stringTable;
    auto* pStrTable = reinterpret_cast<const char*>(imports + 1);
    size_t paddedStringOffset = 0;
    for (size_t i = 0; i < imports->numImports; i++)
    {
        stringTable.emplace_back(pStrTable + paddedStringOffset);
        paddedStringOffset += ((stringTable.back().length() + 1) + 3) & ~3;
    }

    // Allocate variable storage.
    s_keTimeStampBundle = AllocGuestVariable(sizeof(KeTimeStampBundle));
    s_xboxKrnlVersion = AllocGuestVariable(8);
    {
        auto* v = reinterpret_cast<be<uint16_t>*>(g_memory.Translate(s_xboxKrnlVersion));
        v[0] = 2; v[1] = 0; v[2] = 6683; v[3] = 0;
    }
    s_xexExecutableModuleHandle = AllocGuestVariable(0x100);
    {
        // XexGetModuleHandle style loader data block: keep a pointer to the
        // XEX header base at +0x58 like the real LDR_DATA_TABLE_ENTRY-ish block.
        auto* v = reinterpret_cast<be<uint32_t>*>(g_memory.Translate(s_xexExecutableModuleHandle));
        v[0x58 / 4] = s_imageBase;
    }
    s_exLoadedCommandLine = AllocGuestVariable(0x100);
    strcpy(reinterpret_cast<char*>(g_memory.Translate(s_exLoadedCommandLine)), "default.xex");
    s_vdGlobalDevice = AllocGuestVariable(4);
    s_vdGpuClockInMHz = AllocGuestVariable(4);
    *reinterpret_cast<be<uint32_t>*>(g_memory.Translate(s_vdGpuClockInMHz)) = 500;
    s_vdHSIOCalibrationLock = AllocGuestVariable(0x20);
    s_exEventObjectType = AllocGuestVariable(0x40);
    s_exThreadObjectType = AllocGuestVariable(0x40);
    s_keDebugMonitorData = AllocGuestVariable(4);
    s_keCertMonitorData = AllocGuestVariable(4);

    s_executionInfo = AllocGuestVariable(24);
    if (auto* exec = getOptHeaderPtr(file.data(), XEX_HEADER_EXECUTION_INFO))
        memcpy(g_memory.Translate(s_executionInfo), exec, 24);

    struct Entry { const char* lib; uint32_t ordinal; uint32_t value; const char* name; };
    const Entry entries[] =
    {
        { "xboxkrnl.exe", 0x000E, s_exEventObjectType,          "ExEventObjectType" },
        { "xboxkrnl.exe", 0x01AE, s_exLoadedCommandLine,        "ExLoadedCommandLine" },
        { "xboxkrnl.exe", 0x001B, s_exThreadObjectType,         "ExThreadObjectType" },
        { "xboxkrnl.exe", 0x0266, s_keCertMonitorData,          "KeCertMonitorData" },
        { "xboxkrnl.exe", 0x0059, s_keDebugMonitorData,         "KeDebugMonitorData" },
        { "xboxkrnl.exe", 0x00AD, s_keTimeStampBundle,          "KeTimeStampBundle" },
        { "xboxkrnl.exe", 0x01BE, s_vdGlobalDevice,             "VdGlobalDevice" },
        { "xboxkrnl.exe", 0x01C0, s_vdGpuClockInMHz,            "VdGpuClockInMHz" },
        { "xboxkrnl.exe", 0x01C1, s_vdHSIOCalibrationLock,      "VdHSIOCalibrationLock" },
        { "xboxkrnl.exe", 0x0158, s_xboxKrnlVersion,            "XboxKrnlVersion" },
        { "xboxkrnl.exe", 0x0193, s_xexExecutableModuleHandle,  "XexExecutableModuleHandle" },
    };

    auto* library = (Xex2ImportLibrary*)(((char*)imports) + sizeof(Xex2ImportHeader) + imports->sizeOfStringTable);
    for (size_t i = 0; i < stringTable.size(); i++)
    {
        auto* descriptors = (Xex2ImportDescriptor*)(library + 1);
        for (size_t im = 0; im < library->numberOfImports; im++)
        {
            uint32_t slot = descriptors[im].firstThunk;
            auto* word = reinterpret_cast<be<uint32_t>*>(g_memory.Translate(slot));

            // Function thunks were rewritten to nop/nop/nop/blr by Image::ParseImage.
            if (word->get() == 0x60000000)
                continue;

            uint32_t raw = word->value; // ParseImage byte-swapped the remaining thunk words in place
            uint32_t type = raw >> 24;
            uint32_t ordinal = raw & 0xFFFF;
            if (type != 0)
                continue;

            // Function import: the IAT slot precedes its code thunk. Point the
            // slot at the thunk so indirect calls through the IAT still land
            // on the hooked __imp__ function.
            if (im + 1 < library->numberOfImports)
            {
                uint32_t next = descriptors[im + 1].firstThunk;
                auto* nextWord = reinterpret_cast<be<uint32_t>*>(g_memory.Translate(next));
                if (nextWord->get() == 0x60000000)
                {
                    *word = next;
                    continue;
                }
            }

            for (const auto& e : entries)
            {
                if (stringTable[i] == e.lib && e.ordinal == ordinal)
                {
                    *word = e.value;
                    LOG_KERNEL("variable import {} -> {:#x}", e.name, e.value);
                }
            }
        }
        library = (Xex2ImportLibrary*)((char*)(library + 1) + library->numberOfImports * sizeof(Xex2ImportDescriptor));
    }

    return s_entryPoint;
}

void XexLoader::StartTimeStampThread()
{
    g_timeStampThread = std::thread([]()
    {
        auto* bundle = reinterpret_cast<KeTimeStampBundle*>(g_memory.Translate(s_keTimeStampBundle));
        while (true)
        {
            bundle->interruptTime = HostInterruptTime100ns();
            bundle->systemTime = HostSystemTime100ns();
            bundle->tickCount = uint32_t(HostInterruptTime100ns() / 10000);
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    });
    g_timeStampThread.detach();
}
