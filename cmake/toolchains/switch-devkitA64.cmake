# Nintendo Switch (Horizon OS) homebrew cross toolchain: devkitPro devkitA64 + libnx.
# Adapted from UnleashedRecomp-NX (toolchains/switch-devkitA64.cmake).
#
#   cmake -S . -B out/build/switch -G Ninja \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/switch-devkitA64.cmake \
#     -DCMAKE_BUILD_TYPE=Release
#
# tools/switch/build-switch.sh runs this inside the ghcr.io/autorunhq/switch-dev
# Docker image, which ships devkitA64, libnx and mesa-switch (NVK) under
# /opt/devkitpro. See docs/SWITCH.md.

set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_VERSION 1)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# LoPlatform.cmake keys LO_TARGET_PLATFORM=switch off this; devkitPro's SDL3
# fork (thirdparty/SDL-switch) keys its Horizon backends off NINTENDO_SWITCH.
set(LO_TARGET_SWITCH ON CACHE BOOL "Build for Nintendo Switch (libnx)" FORCE)
set(NINTENDO_SWITCH TRUE)
set(SWITCH TRUE)

if(NOT DEFINED DEVKITPRO)
    if(DEFINED ENV{DEVKITPRO})
        set(DEVKITPRO "$ENV{DEVKITPRO}" CACHE PATH "devkitPro root")
    else()
        set(DEVKITPRO "/opt/devkitpro" CACHE PATH "devkitPro root")
    endif()
endif()

set(_DEVKITA64 "${DEVKITPRO}/devkitA64")
set(_DEVKITA64_BIN "${_DEVKITA64}/bin")
set(_DEVKITA64_PREFIX "aarch64-none-elf-")
if(CMAKE_HOST_WIN32)
    set(_DEVKITA64_SUFFIX ".exe")
else()
    set(_DEVKITA64_SUFFIX "")
endif()

set(CMAKE_C_COMPILER   "${_DEVKITA64_BIN}/${_DEVKITA64_PREFIX}gcc${_DEVKITA64_SUFFIX}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${_DEVKITA64_BIN}/${_DEVKITA64_PREFIX}g++${_DEVKITA64_SUFFIX}" CACHE FILEPATH "" FORCE)
set(CMAKE_ASM_COMPILER "${_DEVKITA64_BIN}/${_DEVKITA64_PREFIX}gcc${_DEVKITA64_SUFFIX}" CACHE FILEPATH "" FORCE)
set(CMAKE_AR      "${_DEVKITA64_BIN}/${_DEVKITA64_PREFIX}gcc-ar${_DEVKITA64_SUFFIX}" CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB  "${_DEVKITA64_BIN}/${_DEVKITA64_PREFIX}gcc-ranlib${_DEVKITA64_SUFFIX}" CACHE FILEPATH "" FORCE)
set(CMAKE_NM      "${_DEVKITA64_BIN}/${_DEVKITA64_PREFIX}nm${_DEVKITA64_SUFFIX}" CACHE FILEPATH "" FORCE)
set(CMAKE_OBJCOPY "${_DEVKITA64_BIN}/${_DEVKITA64_PREFIX}objcopy${_DEVKITA64_SUFFIX}" CACHE FILEPATH "" FORCE)
set(CMAKE_STRIP   "${_DEVKITA64_BIN}/${_DEVKITA64_PREFIX}strip${_DEVKITA64_SUFFIX}" CACHE FILEPATH "" FORCE)
set(CMAKE_C_COMPILER_AR "${CMAKE_AR}" CACHE FILEPATH "" FORCE)
set(CMAKE_C_COMPILER_RANLIB "${CMAKE_RANLIB}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER_AR "${CMAKE_AR}" CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER_RANLIB "${CMAKE_RANLIB}" CACHE FILEPATH "" FORCE)

# Cortex-A57, libnx's software TLS (-mtp=soft) and position-independent code
# (homebrew NROs are loaded at a random address).
set(_SWITCH_ARCH_FLAGS "-march=armv8-a+crc+crypto -mtune=cortex-a57 -mtp=soft -fPIE -D__SWITCH__")
set(CMAKE_C_FLAGS_INIT "-ffunction-sections -fdata-sections ${_SWITCH_ARCH_FLAGS} -Wno-psabi")
set(CMAKE_CXX_FLAGS_INIT "-ffunction-sections -fdata-sections ${_SWITCH_ARCH_FLAGS} -Wno-psabi")
set(CMAKE_ASM_FLAGS_INIT "${_SWITCH_ARCH_FLAGS}")
# --allow-multiple-definition: NVK ships two Rust static libraries that each
# bundle the Rust runtime (same workaround as UnleashedRecomp-NX).
set(CMAKE_EXE_LINKER_FLAGS_INIT
    "-specs=${DEVKITPRO}/libnx/switch.specs -Wl,--gc-sections -Wl,--allow-multiple-definition")
set(CMAKE_DL_LIBS "")

include_directories(SYSTEM
    "${DEVKITPRO}/libnx/include"
    "${DEVKITPRO}/portlibs/switch/include")
link_directories(
    "${DEVKITPRO}/libnx/lib"
    "${DEVKITPRO}/portlibs/switch/lib")

set(CMAKE_FIND_ROOT_PATH "${_DEVKITA64}" "${DEVKITPRO}/libnx" "${DEVKITPRO}/portlibs/switch")
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# NRO packaging tools (switch-tools).
find_program(LO_SWITCH_ELF2NRO elf2nro HINTS "${DEVKITPRO}/tools/bin" NO_CMAKE_FIND_ROOT_PATH)
find_program(LO_SWITCH_NACPTOOL nacptool HINTS "${DEVKITPRO}/tools/bin" NO_CMAKE_FIND_ROOT_PATH)
