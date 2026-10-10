cmake_policy(SET CMP0077 NEW)

if(ANDROID)
    # The older tool-vendored JSON header uses char_traits<unsigned char>,
    # removed by the NDK's libc++. Keep the desktop dependency unchanged.
    include(FetchContent)
    set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
    set(JSON_Install OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(lo_android_json
        URL https://github.com/nlohmann/json/releases/download/v3.11.3/json.tar.xz
        URL_HASH SHA256=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d)
    FetchContent_MakeAvailable(lo_android_json)
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_INSTALL OFF CACHE BOOL "" FORCE)
else()
    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
endif()
set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
set(SDL_TESTS OFF CACHE BOOL "" FORCE)

# o1heap's own CMakeLists only builds its tests; wrap the single source file.
add_library(o1heap STATIC "${CMAKE_CURRENT_SOURCE_DIR}/o1heap/o1heap/o1heap.c")
target_include_directories(o1heap PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/o1heap/o1heap")
# o1heap defaults to __builtin_clzl, which is 32-bit on Windows (LLP64) and corrupts every bin index.
target_compile_definitions(o1heap PRIVATE O1HEAP_CLZ=__builtin_clzll)
if(LO_TARGET_PLATFORM STREQUAL "switch")
    # devkitPro's SDL 3.4 fork has the Horizon backends (HID joysticks and
    # touch, audout/audren audio, applet/window events). SDL renders nothing
    # here: plume creates the Vulkan surface from the libnx NWindow itself,
    # so SDL's EGL/GL support is not needed.
    set(SDL_OPENGL OFF CACHE BOOL "" FORCE)
    set(SDL_OPENGLES OFF CACHE BOOL "" FORCE)
    set(SDL_VULKAN OFF CACHE BOOL "" FORCE)
    set(SDL_RENDER_GPU OFF CACHE BOOL "" FORCE)
    set(SDL_GPU OFF CACHE BOOL "" FORCE)
    set(SDL_HIDAPI OFF CACHE BOOL "" FORCE)
    set(SDL_CAMERA OFF CACHE BOOL "" FORCE)
    set(SDL_DIALOG OFF CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/SDL-switch" "${CMAKE_CURRENT_BINARY_DIR}/SDL")
else()
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/SDL")
endif()
include("${CMAKE_CURRENT_SOURCE_DIR}/ffmpeg.cmake")

# plume is added once the GPU backend lands (phase 2, rendering).
if(LO_BUILD_GPU)
    if(WIN32)
        set(PLUME_D3D12_AGILITY_SDK_ENABLED OFF CACHE BOOL "" FORCE)
    endif()
    if(UNIX AND NOT APPLE AND NOT LO_TARGET_PLATFORM STREQUAL "switch")
        set(PLUME_SDL_VULKAN_ENABLED ON CACHE BOOL "" FORCE)
    endif()
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/plume")
    if(ANDROID)
        # The pinned plume CMake option only recognizes desktop Linux. Its SDL
        # surface path also supports Android without changing the submodule.
        target_compile_definitions(plume PUBLIC PLUME_SDL_VULKAN_ENABLED)
        target_link_libraries(plume PUBLIC SDL3::SDL3)
    endif()
    if(LO_TARGET_PLATFORM STREQUAL "switch")
        # tools/patches/plume-switch.patch: VK_NN_vi_surface on the libnx
        # NWindow, entry points from the statically linked Mesa NVK ICD.
        target_compile_definitions(plume PUBLIC __SWITCH__ VK_USE_PLATFORM_VI_NN)
        if(NOT LO_SWITCH_CHECK_ONLY)
            if(NOT EXISTS "${LO_SWITCH_NVK_LIBRARY}")
                message(FATAL_ERROR "Mesa NVK for Switch not found: ${LO_SWITCH_NVK_LIBRARY} (set LO_SWITCH_NVK_LIBRARY)")
            endif()
            # Keep the ICD entry points (plume resolves everything through
            # them) and let the linker rescan the driver's member archives.
            target_link_libraries(plume PUBLIC
                "-Wl,-u,vk_icdGetInstanceProcAddr"
                "-Wl,-u,vk_icdNegotiateLoaderICDInterfaceVersion"
                "-Wl,--start-group" "${LO_SWITCH_NVK_LIBRARY}" ${LO_SWITCH_NVK_EXTRA_LIBS} "-Wl,--end-group"
                nx)
        endif()
    endif()
    if(TARGET sdl_headers_copy)
        add_dependencies(plume sdl_headers_copy)
    endif()
    if(LO_TARGET_PLATFORM STREQUAL "macos")
        # plume's Metal backend translates the runtime's SPIR-V to MSL at shader
        # creation (tools/patches/plume-macos.patch). Only the MSL target is needed.
        set(SPIRV_CROSS_CLI OFF CACHE BOOL "" FORCE)
        set(SPIRV_CROSS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
        set(SPIRV_CROSS_ENABLE_HLSL OFF CACHE BOOL "" FORCE)
        set(SPIRV_CROSS_ENABLE_CPP OFF CACHE BOOL "" FORCE)
        set(SPIRV_CROSS_ENABLE_REFLECT OFF CACHE BOOL "" FORCE)
        set(SPIRV_CROSS_ENABLE_C_API OFF CACHE BOOL "" FORCE)
        set(SPIRV_CROSS_ENABLE_UTIL OFF CACHE BOOL "" FORCE)
        set(SPIRV_CROSS_SKIP_INSTALL ON CACHE BOOL "" FORCE)
        add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/SPIRV-Cross" EXCLUDE_FROM_ALL)
        target_link_libraries(plume PRIVATE spirv-cross-msl)
        # MetalFX spatial upscaling (Scaling filter = MetalFX); macOS 13+.
        target_link_libraries(plume PUBLIC "-framework MetalFX")
    endif()
endif()
