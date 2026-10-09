#pragma once

#include <os/logger.h>
#include <version.h>
#include <cstdint>
#ifdef _WIN32
#include <windows.h>
#elif defined(__APPLE__)
#include <mach/mach.h>
#include <string>
#include <sys/sysctl.h>
#endif
#if __has_include(<lo_build_revision.h>)
#include <lo_build_revision.h>
#endif

namespace os::diagnostics
{
// Called once after the runtime sink opens. Failure snapshots are captured at
// the failing API instead; this baseline is deliberately labelled separately.
#ifdef __APPLE__
inline std::string SysctlString(const char* name)
{
    size_t size = 0;
    if (sysctlbyname(name, nullptr, &size, nullptr, 0) != 0 || size == 0) return "unavailable";
    std::string value(size, '\0');
    if (sysctlbyname(name, value.data(), &size, nullptr, 0) != 0) return "unavailable";
    value.resize(value.find('\0') == std::string::npos ? size : value.find('\0'));
    return value;
}

inline int64_t SysctlInteger(const char* name)
{
    int64_t value = 0;
    size_t size = sizeof(value);
    if (sysctlbyname(name, &value, &size, nullptr, 0) != 0) return -1;
    // Some keys are 32-bit; sysctl then writes only the low bytes.
    return size == sizeof(int32_t) ? int64_t(int32_t(value)) : value;
}
#endif

inline void LogHostMemory(const char* stage)
{
#ifdef __APPLE__
    // Free and inactive pages are immediately reusable; macOS keeps little memory "free".
    vm_statistics64_data_t vm{};
    mach_msg_type_number_t count = HOST_VM_INFO64_COUNT;
    const int64_t page = SysctlInteger("hw.pagesize");
    if (host_statistics64(mach_host_self(), HOST_VM_INFO64, reinterpret_cast<host_info64_t>(&vm), &count) == KERN_SUCCESS)
        LOG_INFO("host memory: stage={} bytes total_physical={} free={} inactive={} compressed={} page_size={}",
            stage, SysctlInteger("hw.memsize"), int64_t(vm.free_count) * page, int64_t(vm.inactive_count) * page,
            int64_t(vm.compressor_page_count) * page, page);
    else
        LOG_WARNING("host memory: stage={} api=host_statistics64 unavailable", stage);
#endif
#ifdef _WIN32
    MEMORYSTATUSEX status{sizeof(status)};
    if (GlobalMemoryStatusEx(&status))
        LOG_INFO("host memory: stage={} bytes available_commit={} total_commit={} available_physical={} total_physical={} available_virtual={} load_percent={}",
            stage, status.ullAvailPageFile, status.ullTotalPageFile, status.ullAvailPhys,
            status.ullTotalPhys, status.ullAvailVirtual, status.dwMemoryLoad);
    else {
        const auto error = GetLastError();
        LOG_WARNING("host memory: stage={} api=GlobalMemoryStatusEx Win32={:#010x}", stage, error);
    }
#endif
}

inline void LogStartupEnvironment()
{
#ifdef LO_BUILD_REVISION
    LOG_NOTICE("build: source={} revision={} pointer_bits={}", lo_version::Source, LO_BUILD_REVISION, sizeof(void*) * 8);
#else
    LOG_NOTICE("build: source={} revision=unavailable pointer_bits={}", lo_version::Source, sizeof(void*) * 8);
#endif
#ifdef __clang__
    LOG_INFO("build compiler: clang {}", __clang_version__);
#elif defined(_MSC_FULL_VER)
    LOG_INFO("build compiler: MSVC {}", _MSC_FULL_VER);
#endif
#ifdef _WIN32
    // RtlGetVersion is unaffected by an absent supportedOS application manifest.
    const auto ntdll = GetModuleHandleW(L"ntdll.dll");
    using RtlGetVersionFn = LONG (WINAPI*)(OSVERSIONINFOW*);
    const auto getVersion = reinterpret_cast<RtlGetVersionFn>(GetProcAddress(ntdll, "RtlGetVersion"));
    if (getVersion) {
        OSVERSIONINFOEXW version{};
        version.dwOSVersionInfoSize = sizeof(version);
        const auto result = getVersion(reinterpret_cast<OSVERSIONINFOW*>(&version));
        if (result == 0)
            LOG_NOTICE("host OS: api=RtlGetVersion version={}.{}.{} platform={} product_type={} service_pack={}.{}",
                version.dwMajorVersion, version.dwMinorVersion, version.dwBuildNumber, version.dwPlatformId,
                version.wProductType, version.wServicePackMajor, version.wServicePackMinor);
        else LOG_WARNING("host OS: api=RtlGetVersion NTSTATUS={:#010x}", uint32_t(result));
    } else {
        const auto error = GetLastError();
        LOG_WARNING("host OS: RtlGetVersion unavailable Win32={:#010x}", error);
    }

    using IsWow64Process2Fn = BOOL (WINAPI*)(HANDLE, USHORT*, USHORT*);
    const auto kernel = GetModuleHandleW(L"kernel32.dll");
    const auto getArchitecture = reinterpret_cast<IsWow64Process2Fn>(GetProcAddress(kernel, "IsWow64Process2"));
    USHORT processMachine = 0, nativeMachine = 0;
    if (getArchitecture && getArchitecture(GetCurrentProcess(), &processMachine, &nativeMachine)) {
        LOG_NOTICE("host architecture: api=IsWow64Process2 process_machine={:#06x} native_machine={:#06x} emulated={}",
            processMachine ? processMachine : nativeMachine, nativeMachine, processMachine != 0);
    } else {
        const auto error = GetLastError();
        SYSTEM_INFO system{};
        GetNativeSystemInfo(&system);
        LOG_WARNING("host architecture: IsWow64Process2 {} Win32={:#010x}; native_processor_architecture={} pointer_bits={}",
            getArchitecture ? "failed" : "unavailable", error, system.wProcessorArchitecture, sizeof(void*) * 8);
    }
    using WineVersionFn = const char* (__cdecl*)();
    const auto wineVersion = reinterpret_cast<WineVersionFn>(GetProcAddress(ntdll, "wine_get_version"));
    if (wineVersion) LOG_NOTICE("host compatibility: wine_version={}", wineVersion());

    // Loaded-image metadata complements the revision, but is not an artifact
    // hash. The COFF timestamp is raw metadata, not a wall-clock claim.
    const auto image = reinterpret_cast<const uint8_t*>(GetModuleHandleW(nullptr));
    if (image) {
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(image);
        if (dos->e_magic == IMAGE_DOS_SIGNATURE && dos->e_lfanew > 0) {
            const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(image + dos->e_lfanew);
            if (nt->Signature == IMAGE_NT_SIGNATURE)
                LOG_INFO("build image: pe_timestamp={:#010x} image_bytes={} machine={:#06x}",
                    nt->FileHeader.TimeDateStamp, nt->OptionalHeader.SizeOfImage, nt->FileHeader.Machine);
        }
    }
#endif
#ifdef __APPLE__
    LOG_NOTICE("host OS: macOS version={} build={} kernel={}", SysctlString("kern.osproductversion"),
        SysctlString("kern.osversion"), SysctlString("kern.osrelease"));
    // sysctl.proc_translated is 1 under Rosetta 2 (an x86_64 build on Apple Silicon).
    LOG_INFO("host architecture: machine={} arm64={} rosetta={}", SysctlString("hw.machine"),
        SysctlInteger("hw.optional.arm64") == 1, SysctlInteger("sysctl.proc_translated") == 1);
    LOG_INFO("host CPU: {} performance_cores={} efficiency_cores={} logical={} page_size={}",
        SysctlString("machdep.cpu.brand_string"), SysctlInteger("hw.perflevel0.physicalcpu"),
        SysctlInteger("hw.perflevel1.physicalcpu"), SysctlInteger("hw.logicalcpu"), SysctlInteger("hw.pagesize"));
#endif
    LogHostMemory("startup");
}
}
