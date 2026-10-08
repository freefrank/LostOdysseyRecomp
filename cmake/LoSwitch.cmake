# cmake/LoSwitch.cmake
# Nintendo Switch (Horizon homebrew) build settings. Included from the top-level
# CMakeLists.txt when LO_TARGET_PLATFORM is "switch". See docs/SWITCH.md.

# PC-only features. None of them has a Switch implementation: the renderer is
# Vulkan on Mesa NVK, there is no updater or network shader download, and
# shaders come from a prebuilt SPIR-V pack (no DXC on the console).
foreach(_lo_switch_off IN ITEMS
        LO_ENABLE_DLSS LO_ENABLE_FSR LO_ENABLE_STREAMLINE_FG LO_ENABLE_XESS LO_ENABLE_XESS_FG
        LO_ENABLE_D3D12_DLSS_FG LO_ENABLE_FSR_FG LO_ENABLE_VULKAN_FSR_FG LO_ENABLE_METALFX_FG)
    set(${_lo_switch_off} OFF CACHE BOOL "Not available on Nintendo Switch" FORCE)
endforeach()

set(DEVKITPRO "/opt/devkitpro" CACHE PATH "devkitPro root")

# Mesa NVK for Horizon, linked statically (no Vulkan loader on the console).
# The switch-dev Docker image installs it into devkitPro's portlibs.
set(LO_SWITCH_NVK_LIBRARY "${DEVKITPRO}/portlibs/switch/lib/libvulkan.a"
    CACHE FILEPATH "Mesa NVK Switch static Vulkan driver (libvulkan.a)")

# Libraries the NVK archive may reference, depending on how mesa-switch was
# built (UnleashedRecomp-NX links expat and drm_nouveau only when present).
set(LO_SWITCH_NVK_EXTRA_LIBS "")
if(NOT LO_SWITCH_CHECK_ONLY)
    foreach(_lo_nvk_dep IN ITEMS drm_nouveau expat zstd z)
        find_library(LO_SWITCH_LIB_${_lo_nvk_dep} ${_lo_nvk_dep}
            HINTS "${DEVKITPRO}/portlibs/switch/lib" "${DEVKITPRO}/libnx/lib")
        if(LO_SWITCH_LIB_${_lo_nvk_dep})
            list(APPEND LO_SWITCH_NVK_EXTRA_LIBS "${LO_SWITCH_LIB_${_lo_nvk_dep}}")
        endif()
    endforeach()
    message(STATUS "LostOdysseyRecomp: NVK ${LO_SWITCH_NVK_LIBRARY} (+ ${LO_SWITCH_NVK_EXTRA_LIBS})")
endif()

# NRO metadata shown in the Homebrew Menu.
set(LO_SWITCH_APP_TITLE "Lost Odyssey Recomp" CACHE STRING "NRO title")
set(LO_SWITCH_APP_AUTHOR "LostOdysseyRecomp contributors" CACHE STRING "NRO author")
set(LO_SWITCH_APP_VERSION "${LO_SOURCE_VERSION}" CACHE STRING "NRO version")
set(LO_SWITCH_APP_ICON "${CMAKE_SOURCE_DIR}/packaging/switch/icon.jpg" CACHE FILEPATH "256x256 JPEG NRO icon")

# Game data and user files on the SD card. The game folder holds disc1..disc4
# exactly as the PC importer writes them (default.xex, LO.fpi, *.fpd).
set(LO_SWITCH_DATA_ROOT "sdmc:/switch/LostOdysseyRecomp" CACHE STRING "SD card folder for game data, settings and logs")

# Compiler identity the console reports for shader caches made on a PC: the
# pinned DXC of tools/XenosRecomp/thirdparty/dxc-bin ("dxc-<major>.<minor>").
set(LO_SWITCH_DXC_IDENTITY "dxc-1.8" CACHE STRING "DXC identity of PC-made Vulkan shader caches")

# Code generation options (same experiments as UnleashedRecomp-NX).
option(LO_SWITCH_RECOMP_O2 "Compile the recompiled game code with -O2 instead of -O3" OFF)
option(LO_SWITCH_LTO "Link-time optimisation (needs a lot of RAM on the build machine)" OFF)
set(LO_SWITCH_LTO_JOBS "2" CACHE STRING "Parallel LTRANS jobs for the LTO link")
if(LO_SWITCH_LTO)
    add_compile_options(-flto -fno-fat-lto-objects)
    add_link_options(-flto=${LO_SWITCH_LTO_JOBS} -flto-partition=balanced)
endif()

# Development check build (tools/switch/check-toolchain.cmake): compiles with a
# stock aarch64 Linux GCC and the libnx headers, links nothing.
if(LO_SWITCH_CHECK_ONLY)
    message(STATUS "LostOdysseyRecomp: Switch compile check only (no NRO)")
endif()
