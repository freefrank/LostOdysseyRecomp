# Optional local NVIDIA NGX SDK integration.  This file never downloads SDK
# content: developers must point LO_DLSS_SDK_ROOT at an audited local checkout.
option(LO_ENABLE_DLSS "Enable the local NVIDIA DLSS/NGX SDK integration" OFF)
option(LO_DLSS_STAGE_RUNTIME "Stage the audited SR runtime beside local binaries" ON)
set(LO_DLSS_SDK_ROOT "" CACHE PATH "NVIDIA DLSS SDK root (310.9.1 / 37495948)")

set(LO_DLSS_SDK_AVAILABLE OFF)
set(LO_DLSS_RUNTIME_FILE "")

if(LO_ENABLE_DLSS)
    if(NOT LO_DLSS_SDK_ROOT)
        message(WARNING "LO_ENABLE_DLSS is ON but LO_DLSS_SDK_ROOT is unset; NGX is compiled as unavailable")
    elseif(NOT EXISTS "${LO_DLSS_SDK_ROOT}/include/nvsdk_ngx_vk.h")
        message(WARNING "LO_DLSS_SDK_ROOT has no NGX Vulkan headers; NGX is compiled as unavailable")
    elseif(WIN32)
        set(_lo_dlss_release_lib "${LO_DLSS_SDK_ROOT}/lib/Windows_x86_64/x64/nvsdk_ngx_s.lib")
        set(_lo_dlss_debug_lib "${LO_DLSS_SDK_ROOT}/lib/Windows_x86_64/x64/nvsdk_ngx_s_dbg.lib")
        set(LO_DLSS_RUNTIME_FILE "${LO_DLSS_SDK_ROOT}/lib/Windows_x86_64/rel/nvngx_dlss.dll")
        if(EXISTS "${_lo_dlss_release_lib}" AND EXISTS "${_lo_dlss_debug_lib}")
            add_library(lo_dlss_ngx_sdk STATIC IMPORTED GLOBAL)
            set_target_properties(lo_dlss_ngx_sdk PROPERTIES
                IMPORTED_LOCATION_RELEASE "${_lo_dlss_release_lib}"
                IMPORTED_LOCATION_RELWITHDEBINFO "${_lo_dlss_release_lib}"
                IMPORTED_LOCATION_MINSIZEREL "${_lo_dlss_release_lib}"
                IMPORTED_LOCATION_DEBUG "${_lo_dlss_debug_lib}")
            set(LO_DLSS_SDK_AVAILABLE ON)
        else()
            message(WARNING "LO_DLSS_SDK_ROOT lacks the static-CRT nvsdk_ngx_s/_s_dbg bootstrap libraries; NGX is compiled as unavailable")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_lo_dlss_linux_lib "${LO_DLSS_SDK_ROOT}/lib/Linux_x86_64/libnvsdk_ngx.a")
        set(LO_DLSS_RUNTIME_FILE "${LO_DLSS_SDK_ROOT}/lib/Linux_x86_64/rel/libnvidia-ngx-dlss.so.310.9.1")
        if(EXISTS "${_lo_dlss_linux_lib}")
            add_library(lo_dlss_ngx_sdk STATIC IMPORTED GLOBAL)
            set_target_properties(lo_dlss_ngx_sdk PROPERTIES IMPORTED_LOCATION "${_lo_dlss_linux_lib}")
            set(LO_DLSS_SDK_AVAILABLE ON)
        else()
            message(WARNING "LO_DLSS_SDK_ROOT lacks libnvsdk_ngx.a; NGX is compiled as unavailable")
        endif()
    else()
        message(WARNING "DLSS NGX P0 only supports Windows and native Linux; NGX is compiled as unavailable")
    endif()
endif()

function(lo_enable_dlss target)
    if(LO_DLSS_SDK_AVAILABLE)
        target_compile_definitions(${target} PRIVATE LO_DLSS_SDK=1 LO_DLSS_SDK_VERSION="310.9.1")
        target_include_directories(${target} PRIVATE "${LO_DLSS_SDK_ROOT}/include")
        target_link_libraries(${target} PRIVATE lo_dlss_ngx_sdk)
        # Only the release SR runtime is staged. The user-writable NGX data path
        # is selected by the controller and is never this runtime directory.
        if(LO_DLSS_STAGE_RUNTIME AND EXISTS "${LO_DLSS_RUNTIME_FILE}")
            if(WIN32)
                set(_lo_dlss_runtime_name "nvngx_dlss.dll")
            else()
                set(_lo_dlss_runtime_name "libnvidia-ngx-dlss.so.310.9.1")
            endif()
            add_custom_command(TARGET ${target} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LO_DLSS_RUNTIME_FILE}"
                    "$<TARGET_FILE_DIR:${target}>/${_lo_dlss_runtime_name}"
                VERBATIM)
        elseif(NOT EXISTS "${LO_DLSS_RUNTIME_FILE}")
            message(WARNING "NGX bootstrap SDK is enabled but the SR runtime is absent; probes will report runtime unavailable")
        endif()
    endif()
endfunction()
