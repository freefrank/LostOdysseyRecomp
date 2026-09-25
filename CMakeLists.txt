cmake_minimum_required(VERSION 3.28)
project(LostOdysseyRecomp VERSION 0.6.19 LANGUAGES C CXX)
include(GNUInstallDirs)
# Source version, not a release/commit or clean-checkout assertion.
# Capture before nested projects can replace PROJECT_VERSION in their scope.
set(LO_SOURCE_VERSION "${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_SHARED_LIBS OFF)

# Sandy Bridge baseline (AVX): lets simde map VMX ops to real SSE4.1/AVX intrinsics
# instead of scalar fallbacks (which pull in C23 roundeven*). Same as UnleashedRecomp.
# Guarded to x86 so non-x86 Clang builds do not receive an invalid -march.
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" LO_TARGET_PROCESSOR)
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND
   LO_TARGET_PROCESSOR MATCHES "^(x86_64|amd64|x64|i[3-6]86|x86)$")
    add_compile_options(-march=sandybridge)
endif()
# Optimization, target architecture, LTO, PGO, and BOLT options. The default
# architecture repeats the sandybridge baseline above (benign duplicate flag);
# an explicit LO_TARGET_ARCH comes later on the command line and wins.
include(cmake/LoOptimization.cmake)
include(cmake/LoDlss.cmake)
include(cmake/LoFsr.cmake)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 阶段 1 之前这里只是占位。生成 ppc/ 之后再按 UnleashedRecomp 的
# CMake 结构拆成 LostOdysseyRecompLib（重编译产物）与 LostOdysseyRecomp（运行时）。
option(LO_BUILD_TOOLS "Build XenonRecomp / XenosRecomp (use tools/build_tools.bat with MSVC instead)" OFF)
option(LO_BUILD_RECOMP_LIB "Build the recompiled game code library from LostOdysseyRecompLib/ppc" ON)

if(LO_BUILD_TOOLS)
    add_subdirectory(tools/XenonRecomp)
    add_subdirectory(tools/XenosRecomp)
endif()

option(LO_BUILD_RUNTIME "Build the LostOdysseyRecomp runtime executable" ON)
option(LO_BUILD_GPU "Build the plume-based GPU backend" ON)

if(LO_BUILD_RECOMP_LIB)
    add_subdirectory(LostOdysseyRecompLib)
endif()

if(LO_BUILD_RUNTIME)
    if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(FATAL_ERROR "The runtime requires a 64-bit target")
    endif()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
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
    if(LO_BUILD_GPU)
        add_subdirectory(tools/tests/native_dlss)
    endif()
    if(TARGET LoPpcAutoSync)
        add_dependencies(LostOdysseyRecomp LoPpcAutoSync)
    endif()
endif()

if(UNIX AND LO_BUILD_RUNTIME)
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
