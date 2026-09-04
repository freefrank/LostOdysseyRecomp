#pragma once

#include <filesystem>

// Loads default.xex (retail, AES + LZX) through XenonUtils, copies the flat
// image into guest memory and resolves the kernel variable imports.
struct XexLoader
{
    static inline uint32_t s_entryPoint = 0;
    static inline uint32_t s_imageBase = 0;
    static inline uint32_t s_imageSize = 0;

    // Returns the entry point, or 0 on failure.
    static uint32_t Load(const std::filesystem::path& xexPath);

    // Guest addresses of the kernel-exported variables we synthesise.
    static inline uint32_t s_keTimeStampBundle = 0;
    static inline uint32_t s_xboxKrnlVersion = 0;
    static inline uint32_t s_xexExecutableModuleHandle = 0;
    static inline uint32_t s_exLoadedCommandLine = 0;
    static inline uint32_t s_vdGlobalDevice = 0;
    static inline uint32_t s_vdGpuClockInMHz = 0;
    static inline uint32_t s_vdHSIOCalibrationLock = 0;
    static inline uint32_t s_exEventObjectType = 0;
    static inline uint32_t s_exThreadObjectType = 0;
    static inline uint32_t s_keDebugMonitorData = 0;
    static inline uint32_t s_keCertMonitorData = 0;
    static inline uint32_t s_executionInfo = 0; // copy of XEX_HEADER_EXECUTION_INFO for XamGetExecutionId

    static void StartTimeStampThread();
};
