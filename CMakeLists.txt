cmake_minimum_required(VERSION 3.28)
project(LostOdysseyRecomp VERSION 0.4.17 LANGUAGES C CXX)
# Source version, not a release/commit or clean-checkout assertion.
# Capture before nested projects can replace PROJECT_VERSION in their scope.
set(LO_SOURCE_VERSION "${PROJECT_VERSION}")

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(BUILD_SHARED_LIBS OFF)

# Sandy Bridge baseline (AVX): lets simde map VMX ops to real SSE4.1/AVX intrinsics
# instead of scalar fallbacks (which pull in C23 roundeven*). Same as UnleashedRecomp.
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_options(-march=sandybridge)
endif()
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
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        message(FATAL_ERROR "The runtime requires clang-cl")
    endif()
    # XenonRecomp's tree provides XenonUtils (XEX loading), fmt, xxHash and simde.
    if(NOT LO_BUILD_TOOLS)
        set(THIRDPARTY_ROOT ${CMAKE_SOURCE_DIR}/tools/XenonRecomp/thirdparty)
        add_subdirectory(tools/XenonRecomp/thirdparty)
        add_subdirectory(tools/XenonRecomp/XenonUtils)
    endif()
    add_subdirectory(thirdparty)
    add_subdirectory(LostOdysseyRecomp)
endif()
