# cmake/LoPlatform.cmake
# Target platform and instruction set identification. Platform- and
# architecture-specific build decisions key off these variables instead of
# re-testing CMAKE_SYSTEM_NAME / CMAKE_SYSTEM_PROCESSOR at each call site.
#
#   LO_TARGET_PLATFORM  windows | linux | macos | android | switch
#   LO_TARGET_ISA       x86_64 | x86 | aarch64

if(LO_TARGET_SWITCH)
    # Nintendo Switch homebrew; cmake/toolchains/switch-devkitA64.cmake.
    set(LO_TARGET_PLATFORM "switch")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Android")
    set(LO_TARGET_PLATFORM "android")
elseif(WIN32)
    set(LO_TARGET_PLATFORM "windows")
elseif(APPLE)
    set(LO_TARGET_PLATFORM "macos")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(LO_TARGET_PLATFORM "linux")
else()
    message(FATAL_ERROR "Unsupported target platform '${CMAKE_SYSTEM_NAME}'")
endif()

string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" _lo_processor)
if(WIN32 AND CMAKE_CXX_COMPILER_ARCHITECTURE_ID)
    # Ninja/MSVC toolchains can leave CMAKE_SYSTEM_PROCESSOR empty. The
    # compiler identification also describes the target when cross-compiling.
    string(TOLOWER "${CMAKE_CXX_COMPILER_ARCHITECTURE_ID}" _lo_processor)
elseif(APPLE AND CMAKE_OSX_ARCHITECTURES)
    # On Apple, CMAKE_SYSTEM_PROCESSOR describes the host; an explicit
    # CMAKE_OSX_ARCHITECTURES is the target. Generated guest code and the
    # FFmpeg configuration are per-ISA, so universal binaries are rejected.
    list(LENGTH CMAKE_OSX_ARCHITECTURES _lo_osx_arch_count)
    if(_lo_osx_arch_count GREATER 1)
        message(FATAL_ERROR "Universal macOS builds are not supported; set a single CMAKE_OSX_ARCHITECTURES value")
    endif()
    string(TOLOWER "${CMAKE_OSX_ARCHITECTURES}" _lo_processor)
endif()

if(_lo_processor MATCHES "^(x86_64|amd64|x64)$")
    set(LO_TARGET_ISA "x86_64")
elseif(_lo_processor MATCHES "^(i[3-6]86|x86)$")
    set(LO_TARGET_ISA "x86")
elseif(_lo_processor MATCHES "^(arm64|aarch64)$")
    set(LO_TARGET_ISA "aarch64")
else()
    message(FATAL_ERROR "Unsupported target processor '${_lo_processor}'")
endif()

message(STATUS "LostOdysseyRecomp target: ${LO_TARGET_PLATFORM}-${LO_TARGET_ISA}")

if(LO_TARGET_PLATFORM STREQUAL "macos")
    # SDL's Cocoa backend and plume's Metal backend contain Objective-C(++)
    # sources. Languages enabled only inside a subdirectory are not usable by
    # the generator for the whole build, so enable them at the top level.
    enable_language(OBJC OBJCXX)
endif()
