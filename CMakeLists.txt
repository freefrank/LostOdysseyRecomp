cmake_minimum_required(VERSION 3.28)
# Minimum macOS version; ignored elsewhere. Must precede project(), and keeps
# plume from falling back to its own 10.15 default. The bundled DXC library is
# built for macOS 15.
set(LO_MINIMUM_MACOS "15.0")
set(CMAKE_OSX_DEPLOYMENT_TARGET "${LO_MINIMUM_MACOS}" CACHE STRING "Minimum macOS deployment target")
if(CMAKE_OSX_DEPLOYMENT_TARGET VERSION_LESS LO_MINIMUM_MACOS)
    # Build trees configured before the minimum was raised keep the old cached value.
    message(STATUS "Raising CMAKE_OSX_DEPLOYMENT_TARGET from ${CMAKE_OSX_DEPLOYMENT_TARGET} to ${LO_MINIMUM_MACOS}")
    set(CMAKE_OSX_DEPLOYMENT_TARGET "${LO_MINIMUM_MACOS}" CACHE STRING "Minimum macOS deployment target" FORCE)
endif()
project(LostOdysseyRecomp VERSION 0.9.0 LANGUAGES C CXX)
include(GNUInstallDirs)
# Source version, not a release/commit or clean-checkout assertion.
# Capture before nested projects can replace PROJECT_VERSION in their scope.
set(LO_SOURCE_VERSION "${PROJECT_VERSION}")
# Optional pre-release suffix, e.g. "macos.1"; the updater compares it too.
set(LO_VERSION_SUFFIX "" CACHE STRING "Suffix appended to the source version as -<suffix>")
if(LO_VERSION_SUFFIX)
    string(APPEND LO_SOURCE_VERSION "-${LO_VERSION_SUFFIX}")
endif()

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_SHARED_LIBS OFF)

# LO_TARGET_PLATFORM / LO_TARGET_ISA for every platform-specific decision below.
include(cmake/LoPlatform.cmake)
option(LO_BUILD_ANDROID_PROBE "Build the Android SDL/Vulkan/memory diagnostic app (no game data)" OFF)
if(LO_BUILD_ANDROID_PROBE)
    if(NOT ANDROID OR NOT LO_TARGET_ISA STREQUAL "aarch64")
        message(FATAL_ERROR "The Android probe requires the NDK arm64-v8a toolchain")
    endif()
    add_subdirectory(tools/android_probe)
    return()
endif()
# Sandy Bridge baseline (AVX): lets simde map VMX ops to real SSE4.1/AVX intrinsics
# instead of scalar fallbacks (which pull in C23 roundeven*). Same as UnleashedRecomp.
# Guarded to x86 so non-x86 Clang builds do not receive an invalid -march; on
# AArch64, simde maps VMX ops to NEON, which is always available.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND LO_TARGET_ISA MATCHES "^x86")
    add_compile_options(-march=sandybridge)
endif()
# Optimization, target architecture, LTO, PGO, and BOLT options. On x86 the default
# architecture repeats the sandybridge baseline above (benign duplicate flag);
# an explicit LO_TARGET_ARCH comes later on the command line and wins.
include(cmake/LoOptimization.cmake)
include(cmake/LoDlss.cmake)
include(cmake/LoStreamline.cmake)
include(cmake/LoXess.cmake)
include(cmake/LoFrameGeneration.cmake)
include(cmake/LoFsr.cmake)
# NGX and the FidelityFX SDK paths are D3D12/Vulkan only; macOS renders through
# Metal. Upscaling there is a separate later feature (MetalFX).
if(LO_TARGET_PLATFORM STREQUAL "macos" AND (LO_ENABLE_DLSS OR LO_ENABLE_FSR OR LO_ENABLE_STREAMLINE_FG OR
    LO_ENABLE_XESS OR LO_ENABLE_XESS_FG))
    message(FATAL_ERROR "DLSS, FSR, XeSS and DLSS frame generation are not available on macOS")
endif()
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 阶段 1 之前这里只是占位。生成 ppc/ 之后再按 UnleashedRecomp 的
# CMake 结构拆成 LostOdysseyRecompLib（重编译产物）与 LostOdysseyRecomp（运行时）。
option(LO_BUILD_TOOLS "Build XenonRecomp / XenosRecomp (use tools/build_tools.bat with MSVC instead)" OFF)
option(LO_BUILD_RECOMP_LIB "Build the recompiled game code library from LostOdysseyRecompLib/ppc" ON)
option(LO_BUILD_RUNTIME "Build the LostOdysseyRecomp runtime executable" ON)
option(LO_BUILD_GPU "Build the plume-based GPU backend" ON)
option(LO_BUILD_ANDROID_RUNTIME "Enable the experimental Android game runtime shared library" OFF)

if(LO_TARGET_PLATFORM STREQUAL "switch")
    if(LO_BUILD_TOOLS)
        message(FATAL_ERROR "Switch builds use host-generated PPC sources; build XenonRecomp/XenosRecomp separately for the host")
    endif()
    include(cmake/LoSwitch.cmake)
endif()

if(ANDROID)
    if(NOT LO_TARGET_ISA STREQUAL "aarch64")
        message(FATAL_ERROR "Android currently requires the NDK arm64-v8a toolchain")
    endif()
    if(LO_BUILD_TOOLS)
        message(FATAL_ERROR "Android builds use host-generated PPC sources; build XenonRecomp/XenosRecomp separately for the host")
    endif()
    if(LO_BUILD_RUNTIME AND NOT LO_BUILD_ANDROID_RUNTIME)
        message(FATAL_ERROR "The Android game runtime is experimental; explicitly set -DLO_BUILD_ANDROID_RUNTIME=ON, or use -DLO_BUILD_ANDROID_PROBE=ON for diagnostics")
    endif()
    # These archives will be linked into the Android runtime shared library.
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

if(LO_BUILD_TOOLS)
    add_subdirectory(tools/XenonRecomp)
    add_subdirectory(tools/XenosRecomp)
endif()

if(LO_BUILD_RECOMP_LIB)
    add_subdirectory(LostOdysseyRecompLib)
endif()

# miniz (XenosRecomp submodule) writes the asset exporter's PNG files.
set(LO_MINIZ_DIR "${CMAKE_SOURCE_DIR}/tools/XenosRecomp/thirdparty/smol-v/testing/external/miniz" CACHE PATH
    "miniz sources used by the asset exporter")
# xxHash for the exporter's texture fingerprints (the runtime links xxHash::xxhash).
set(LO_XXHASH_DIR "${CMAKE_SOURCE_DIR}/tools/XenosRecomp/thirdparty/xxHash" CACHE PATH
    "xxHash sources used by the focused LoAssetExport target")

if(LO_BUILD_RUNTIME)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "The runtime requires a 64-bit target")
    endif()
    # The Switch build uses devkitA64's GCC (the only toolchain libnx supports);
    # every other target requires Clang.
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND NOT LO_TARGET_PLATFORM STREQUAL "switch")
        message(FATAL_ERROR "The runtime requires Clang")
    endif()
    # XenonRecomp's tree provides XenonUtils (XEX loading), fmt, xxHash and simde.
    if(NOT LO_BUILD_TOOLS)
        set(THIRDPARTY_ROOT ${CMAKE_SOURCE_DIR}/tools/XenonRecomp/thirdparty)
        add_subdirectory(tools/XenonRecomp/thirdparty)
        add_subdirectory(tools/XenonRecomp/XenonUtils)
    endif()
    add_subdirectory(thirdparty)
    add_subdirectory(LostOdysseyRecomp)
    # These focused targets explicitly compile menu_assets.cpp rather than using
    # the runtime's recursive source list, so they need its mod resolver too.
    foreach(_lo_menu_test IN ITEMS LoMenuRenderTest LoMenuFlowTest)
        if(TARGET ${_lo_menu_test})
            target_sources(${_lo_menu_test} PRIVATE
                ${CMAKE_SOURCE_DIR}/LostOdysseyRecomp/modding/mod_api.cpp)
        endif()
    endforeach()
    if(LO_BUILD_GPU AND NOT ANDROID AND NOT LO_TARGET_PLATFORM MATCHES "^(macos|switch)$")
        add_subdirectory(tools/tests/native_dlss)
    endif()
    if(TARGET LoPpcAutoSync)
        add_dependencies(LostOdysseyRecomp LoPpcAutoSync)
    endif()
endif()

# No private game data, generated PPC library, GPU or third-party dependency.
option(LO_BUILD_MOD_TESTS "Build standalone mod API and image contract tests" OFF)
if(LO_BUILD_MOD_TESTS)
    enable_testing()
    find_package(Threads REQUIRED)
    add_executable(LoModApiTest
        tools/tests/mod_api_test.cpp
        LostOdysseyRecomp/modding/mod_api.cpp)
    target_include_directories(LoModApiTest PRIVATE LostOdysseyRecomp)
    target_compile_features(LoModApiTest PRIVATE cxx_std_20)
    target_link_libraries(LoModApiTest PRIVATE Threads::Threads)
    if(MSVC)
        target_compile_options(LoModApiTest PRIVATE /UNDEBUG)
    else()
        target_compile_options(LoModApiTest PRIVATE -UNDEBUG)
    endif()
    add_test(NAME LoModApiTest COMMAND LoModApiTest)
    # The runtime's --export-assets mode without game code, GPU or SDL.
    if(EXISTS "${LO_MINIZ_DIR}/miniz.c")
        add_executable(LoAssetExport
            tools/tests/asset_export_cli.cpp
            LostOdysseyRecomp/modding/asset_export.cpp
            LostOdysseyRecomp/modding/mod_api.cpp
            thirdparty/lzokay/lzokay.cpp
            ${LO_MINIZ_DIR}/miniz.c
            ${LO_XXHASH_DIR}/xxhash.c)
        target_include_directories(LoAssetExport PRIVATE LostOdysseyRecomp thirdparty/lzokay ${LO_MINIZ_DIR} ${LO_XXHASH_DIR})
        target_compile_features(LoAssetExport PRIVATE cxx_std_20)
        target_compile_definitions(LoAssetExport PRIVATE NOMINMAX)
        target_link_libraries(LoAssetExport PRIVATE Threads::Threads)
        if(WIN32 AND MSVC)
            # The runtime's subsystem, so redirected stdout is exercised as shipped.
            target_link_options(LoAssetExport PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
        endif()
    endif()
endif()

if(UNIX AND NOT ANDROID AND LO_BUILD_RUNTIME AND NOT LO_TARGET_PLATFORM STREQUAL "switch")
    install(TARGETS LostOdysseyRecomp RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    install(FILES
        packaging/linux/io.github.freefrank.LostOdysseyRecomp.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications)
    install(FILES
        packaging/linux/io.github.freefrank.LostOdysseyRecomp.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/256x256/apps)
    install(FILES
        packaging/linux/io.github.freefrank.LostOdysseyRecomp.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo)
endif()
