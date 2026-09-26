# Optional local NVIDIA NGX SDK integration.  This file never downloads SDK
# content: developers must point LO_DLSS_SDK_ROOT at an audited local checkout.
option(LO_ENABLE_DLSS "Enable the local NVIDIA DLSS/NGX SDK integration" OFF)
option(LO_DLSS_STAGE_RUNTIME "Stage the audited SR runtime beside local binaries" ON)
option(LO_REQUIRE_DLSS "Require complete DLSS SDK availability when LO_ENABLE_DLSS is ON" OFF)
set(LO_DLSS_SDK_ROOT "" CACHE PATH "NVIDIA DLSS SDK root (310.9.1 / 37495948)")

set(LO_DLSS_SDK_AVAILABLE OFF)
set(LO_DLSS_RUNTIME_FILE "")

if(LO_ENABLE_DLSS)
    if(LO_REQUIRE_DLSS)
        set(_lo_dlss_fail_level FATAL_ERROR)
    else()
        set(_lo_dlss_fail_level WARNING)
    endif()

    if(NOT LO_DLSS_SDK_ROOT)
        message(${_lo_dlss_fail_level} "LO_ENABLE_DLSS is ON but LO_DLSS_SDK_ROOT is unset; NGX is compiled as unavailable")
    elseif(NOT EXISTS "${LO_DLSS_SDK_ROOT}/include/nvsdk_ngx_vk.h")
        message(${_lo_dlss_fail_level} "LO_DLSS_SDK_ROOT has no NGX Vulkan headers; NGX is compiled as unavailable")
    elseif(WIN32)
        set(_lo_dlss_release_lib "${LO_DLSS_SDK_ROOT}/lib/Windows_x86_64/x64/nvsdk_ngx_s.lib")
        set(_lo_dlss_debug_lib "${LO_DLSS_SDK_ROOT}/lib/Windows_x86_64/x64/nvsdk_ngx_s_dbg.lib")
        set(LO_DLSS_RUNTIME_FILE "${LO_DLSS_SDK_ROOT}/lib/Windows_x86_64/rel/nvngx_dlss.dll")
        if(EXISTS "${_lo_dlss_release_lib}" AND EXISTS "${_lo_dlss_debug_lib}")
            if(LO_REQUIRE_DLSS AND NOT EXISTS "${LO_DLSS_RUNTIME_FILE}")
                message(FATAL_ERROR "LO_REQUIRE_DLSS is ON but LO_DLSS_SDK_ROOT lacks Windows release runtime nvngx_dlss.dll")
            endif()
            add_library(lo_dlss_ngx_sdk STATIC IMPORTED GLOBAL)
            set_target_properties(lo_dlss_ngx_sdk PROPERTIES
                IMPORTED_LOCATION_RELEASE "${_lo_dlss_release_lib}"
                IMPORTED_LOCATION_RELWITHDEBINFO "${_lo_dlss_release_lib}"
                IMPORTED_LOCATION_MINSIZEREL "${_lo_dlss_release_lib}"
                IMPORTED_LOCATION_DEBUG "${_lo_dlss_debug_lib}")
            set(LO_DLSS_SDK_AVAILABLE ON)
        else()
            message(${_lo_dlss_fail_level} "LO_DLSS_SDK_ROOT lacks the static-CRT nvsdk_ngx_s/_s_dbg bootstrap libraries; NGX is compiled as unavailable")
        endif()
    elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_lo_dlss_linux_lib "${LO_DLSS_SDK_ROOT}/lib/Linux_x86_64/libnvsdk_ngx.a")
        set(LO_DLSS_RUNTIME_FILE "${LO_DLSS_SDK_ROOT}/lib/Linux_x86_64/rel/libnvidia-ngx-dlss.so.310.9.1")
        if(EXISTS "${_lo_dlss_linux_lib}")
            if(LO_REQUIRE_DLSS AND NOT EXISTS "${LO_DLSS_RUNTIME_FILE}")
                message(FATAL_ERROR "LO_REQUIRE_DLSS is ON but LO_DLSS_SDK_ROOT lacks Linux release runtime libnvidia-ngx-dlss.so.310.9.1")
            endif()
            add_library(lo_dlss_ngx_sdk STATIC IMPORTED GLOBAL)
            set_target_properties(lo_dlss_ngx_sdk PROPERTIES IMPORTED_LOCATION "${_lo_dlss_linux_lib}")
            set(LO_DLSS_SDK_AVAILABLE ON)
        else()
            message(${_lo_dlss_fail_level} "LO_DLSS_SDK_ROOT lacks libnvsdk_ngx.a; NGX is compiled as unavailable")
        endif()
    else()
        message(${_lo_dlss_fail_level} "DLSS NGX P0 only supports Windows and native Linux; NGX is compiled as unavailable")
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

        if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND EXISTS "${LO_DLSS_RUNTIME_FILE}")
            # The NGX loader looks beside the executable. Preserve the release
            # filename and the relative aliases without bundling system libraries.
            install(FILES "${LO_DLSS_RUNTIME_FILE}" DESTINATION "${CMAKE_INSTALL_BINDIR}")
            set(_lo_dlss_install_dir "${CMAKE_CURRENT_BINARY_DIR}/lo-dlss-install")
            file(MAKE_DIRECTORY "${_lo_dlss_install_dir}")
            foreach(_lo_dlss_alias IN ITEMS libnvidia-ngx-dlss.so libnvidia-ngx-dlss.so.1)
                file(REMOVE "${_lo_dlss_install_dir}/${_lo_dlss_alias}")
                file(CREATE_LINK "libnvidia-ngx-dlss.so.310.9.1"
                    "${_lo_dlss_install_dir}/${_lo_dlss_alias}" SYMBOLIC)
                install(FILES "${_lo_dlss_install_dir}/${_lo_dlss_alias}"
                    DESTINATION "${CMAKE_INSTALL_BINDIR}")
            endforeach()

            set(_lo_dlss_license_dir "${CMAKE_INSTALL_DATADIR}/licenses/lost-odyssey-recomp/NVIDIA-DLSS")
            install(FILES "${LO_DLSS_SDK_ROOT}/LICENSE.txt" DESTINATION "${_lo_dlss_license_dir}")
            file(WRITE "${_lo_dlss_install_dir}/NOTICE.txt"
                "This software contains source code and/or runtime components provided by NVIDIA Corporation.\n"
                "NVIDIA DLSS SDK Version: 310.9.1 (commit 374959484e79a640feaba44c93ac8cfb0a03f5b5)\n")
            install(FILES "${_lo_dlss_install_dir}/NOTICE.txt" DESTINATION "${_lo_dlss_license_dir}")
        endif()
    endif()
endfunction()
