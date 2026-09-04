cmake_minimum_required(VERSION 3.28)
project(LostOdysseyRecomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 阶段 1 之前这里只是占位。生成 ppc/ 之后再按 UnleashedRecomp 的
# CMake 结构拆成 LostOdysseyRecompLib（重编译产物）与 LostOdysseyRecomp（运行时）。
option(LO_BUILD_TOOLS "Build XenonRecomp / XenosRecomp (use tools/build_tools.bat with MSVC instead)" OFF)
option(LO_BUILD_RECOMP_LIB "Build the recompiled game code library from LostOdysseyRecompLib/ppc" ON)

if(LO_BUILD_TOOLS)
    add_subdirectory(tools/XenonRecomp)
    add_subdirectory(tools/XenosRecomp)
endif()

if(LO_BUILD_RECOMP_LIB)
    add_subdirectory(LostOdysseyRecompLib)
endif()
