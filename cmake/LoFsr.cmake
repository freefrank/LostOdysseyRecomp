# Offline, optional FSR 3.1.4 Vulkan upscaler. No frame interpolation backend.
include_guard(GLOBAL)
option(LO_ENABLE_FSR "Enable locally prepared FidelityFX FSR Vulkan upscaler" OFF)
option(LO_REQUIRE_FSR "Fail configuration if requested FSR inputs are unavailable" OFF)
set(LO_FSR_SDK_ROOT "" CACHE PATH "FidelityFX SDK v1.1.4 c6efa6bf source root")
set(LO_FSR_SHADER_DIR "" CACHE PATH "Offline generated FSR Vulkan permutation headers")
set(LO_FSR_VOLK_DIR "${CMAKE_CURRENT_LIST_DIR}/../thirdparty/plume/contrib/volk" CACHE PATH "Plume-compatible volk source directory")
set(LO_FSR_AVAILABLE OFF)

if(LO_ENABLE_FSR)
    set(_lo_fsr_error "")
    if(NOT EXISTS "${LO_FSR_SDK_ROOT}/sdk/src/components/fsr3upscaler/ffx_fsr3upscaler.cpp")
        set(_lo_fsr_error "LO_FSR_SDK_ROOT lacks FidelityFX SDK v1.1.4 sources")
    elseif(NOT EXISTS "${LO_FSR_SHADER_DIR}/manifest.json")
        set(_lo_fsr_error "LO_FSR_SHADER_DIR lacks offline shader manifest; run tools/fsr/generate_shaders.py")
    else()
        file(READ "${LO_FSR_SHADER_DIR}/manifest.json" _lo_fsr_manifest)
        string(JSON _lo_fsr_commit ERROR_VARIABLE _json_error GET "${_lo_fsr_manifest}" sdk_commit)
        if(NOT _lo_fsr_commit STREQUAL "c6efa6bf7f2027b3ec94f28578bb5965eabb9e55")
            set(_lo_fsr_error "FSR shader manifest SDK commit differs from pinned v1.1.4")
        endif()
        # Build-time provenance only; no runtime scans or network access.
        foreach(_group headers sdk_sources)
            string(JSON _count ERROR_VARIABLE _json_error LENGTH "${_lo_fsr_manifest}" ${_group})
            if(_json_error OR _count LESS 1)
                set(_lo_fsr_error "FSR manifest lacks ${_group}")
                break()
            endif()
            math(EXPR _last "${_count} - 1")
            foreach(_index RANGE ${_last})
                string(JSON _file MEMBER "${_lo_fsr_manifest}" ${_group} ${_index})
                string(JSON _expected GET "${_lo_fsr_manifest}" ${_group} "${_file}")
                if(_group STREQUAL "headers")
                    set(_path "${LO_FSR_SHADER_DIR}/${_file}")
                else()
                    set(_path "${LO_FSR_SDK_ROOT}/${_file}")
                endif()
                if(NOT EXISTS "${_path}")
                    set(_lo_fsr_error "FSR input missing: ${_path}")
                    break()
                endif()
                file(SHA256 "${_path}" _actual)
                if(NOT _actual STREQUAL _expected)
                    set(_lo_fsr_error "FSR input differs from prepared manifest: ${_path}")
                    break()
                endif()
            endforeach()
        endforeach()
    endif()
    if(_lo_fsr_error)
        if(LO_REQUIRE_FSR)
            message(FATAL_ERROR "${_lo_fsr_error}")
        endif()
        message(WARNING "${_lo_fsr_error}; FSR is compiled as unavailable")
    else()
        set(_fsr "${LO_FSR_SDK_ROOT}/sdk")
        file(READ "${_fsr}/src/backends/vk/ffx_vk.cpp" _lo_fsr_backend)
        string(FIND "${_lo_fsr_backend}" "backendInterface->fpSwapChainConfigureFrameGeneration = ffxSetFrameGenerationConfigToSwapchainVK;" _lo_fsr_callback_pos)
        if(_lo_fsr_callback_pos EQUAL -1)
            message(FATAL_ERROR "Pinned FSR SR-only backend compatibility anchor missing")
        endif()
        string(REPLACE "backendInterface->fpSwapChainConfigureFrameGeneration = ffxSetFrameGenerationConfigToSwapchainVK;"
            "backendInterface->fpSwapChainConfigureFrameGeneration = nullptr; // SR-only build"
            _lo_fsr_backend "${_lo_fsr_backend}")
        # SDK 1.1.4 places alignas(32) EffectContext after merely 4-byte-aligned
        # arrays. Reserve worst-case padding and align the absolute address;
        # scratch storage itself need not begin on a 32-byte boundary.
        set(_lo_fsr_size_anchor "pipelineArraySize + resourceArraySize + contextArraySize,")
        set(_lo_fsr_map_anchor "backendContext->pEffectContexts = (BackendContext_VK::EffectContext*)pMem;")
        foreach(_anchor IN ITEMS "${_lo_fsr_size_anchor}" "${_lo_fsr_map_anchor}")
            string(FIND "${_lo_fsr_backend}" "${_anchor}" _lo_fsr_align_pos)
            if(_lo_fsr_align_pos EQUAL -1)
                message(FATAL_ERROR "Pinned FSR scratch alignment compatibility anchor missing")
            endif()
        endforeach()
        string(REPLACE "${_lo_fsr_size_anchor}"
            "pipelineArraySize + resourceArraySize + contextArraySize + alignof(BackendContext_VK::EffectContext) - 1,"
            _lo_fsr_backend "${_lo_fsr_backend}")
        string(REPLACE "${_lo_fsr_map_anchor}"
            "pMem = reinterpret_cast<uint8_t*>(FFX_ALIGN_UP(reinterpret_cast<uintptr_t>(pMem), uintptr_t(alignof(BackendContext_VK::EffectContext))));\n        ${_lo_fsr_map_anchor}"
            _lo_fsr_backend "${_lo_fsr_backend}")
        # Require every requested property. Prefer invisible local memory on
        # discrete GPUs but allow local+visible heaps on UMA. The same policy
        # still excludes deviceCoherentMemory, which Plume does not enable.
        include("${CMAKE_CURRENT_LIST_DIR}/LoFsrVulkanMemory.cmake")
        lo_fsr_patch_memory_selection("${_lo_fsr_backend}" _lo_fsr_backend)
        # Plume defines Vulkan entry points as volk function-pointer variables.
        # Compile the backend against the same ABI, not loader function imports.
        set(_lo_fsr_backend_file "${CMAKE_CURRENT_BINARY_DIR}/lo-fsr-backend/ffx_vk.cpp")
        file(CONFIGURE OUTPUT "${_lo_fsr_backend_file}" CONTENT "#include <volk.h>\n#include \"vulkan_memory_policy.h\"\n${_lo_fsr_backend}" @ONLY)
        add_library(lo_fsr3upscaler_vk STATIC
            "${_fsr}/src/components/fsr3upscaler/ffx_fsr3upscaler.cpp"
            "${_fsr}/src/shared/ffx_assert.cpp"
            "${_fsr}/src/shared/ffx_message.cpp"
            "${_fsr}/src/shared/ffx_object_management.cpp"
            "${_fsr}/src/shared/ffx_breadcrumbs_list.cpp"
            "${_lo_fsr_backend_file}"
            "${_fsr}/src/backends/shared/ffx_shader_blobs.cpp"
            "${_fsr}/src/backends/shared/blob_accessors/ffx_fsr3upscaler_shaderblobs.cpp")
        add_library(LoFsr::Vulkan ALIAS lo_fsr3upscaler_vk)
        target_compile_features(lo_fsr3upscaler_vk PUBLIC cxx_std_17)
        target_compile_definitions(lo_fsr3upscaler_vk PRIVATE FFX_FSR3UPSCALER NOMINMAX)
        target_include_directories(lo_fsr3upscaler_vk PUBLIC "${_fsr}/include" "${LO_FSR_SHADER_DIR}"
            PRIVATE "${_fsr}/src/shared" "${_fsr}/src/components"
            "${_fsr}/src/backends/shared" "${LO_FSR_SHADER_DIR}" "${LO_FSR_VOLK_DIR}"
            "${CMAKE_CURRENT_LIST_DIR}/../tools/fsr")
        if(TARGET Vulkan::Headers)
            target_link_libraries(lo_fsr3upscaler_vk PUBLIC Vulkan::Headers)
        else()
            find_path(LO_FSR_VULKAN_INCLUDE_DIR vulkan/vulkan.h
                HINTS "${CMAKE_CURRENT_LIST_DIR}/../thirdparty/plume/contrib/Vulkan-Headers/include")
            if(NOT LO_FSR_VULKAN_INCLUDE_DIR)
                message(FATAL_ERROR "FSR Vulkan headers unavailable")
            endif()
            target_include_directories(lo_fsr3upscaler_vk PUBLIC "${LO_FSR_VULKAN_INCLUDE_DIR}")
        endif()
        if(MSVC)
            target_compile_options(lo_fsr3upscaler_vk PRIVATE /utf-8 /FIbit)
            set_property(TARGET lo_fsr3upscaler_vk PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        elseif(UNIX)
            # Linux wchar_t is 32-bit; SDK resource debug names enlarge the
            # private context beyond the Windows-sized public opaque storage.
            # Keep libc's wchar ABI and propagate a larger opaque header to
            # both SDK and consumers. Never edit the pinned SDK checkout.
            set(_lo_fsr_overlay "${CMAKE_CURRENT_BINARY_DIR}/lo-fsr-include")
            file(READ "${_fsr}/include/FidelityFX/host/ffx_fsr3upscaler.h" _lo_fsr_header)
            string(FIND "${_lo_fsr_header}" "#define FFX_FSR3UPSCALER_CONTEXT_SIZE (FFX_SDK_DEFAULT_CONTEXT_SIZE)" _lo_fsr_context_pos)
            if(_lo_fsr_context_pos EQUAL -1)
                message(FATAL_ERROR "Pinned FSR Linux context compatibility anchor missing")
            endif()
            string(REPLACE "#define FFX_FSR3UPSCALER_CONTEXT_SIZE (FFX_SDK_DEFAULT_CONTEXT_SIZE)"
                "#define FFX_FSR3UPSCALER_CONTEXT_SIZE (2 * FFX_SDK_DEFAULT_CONTEXT_SIZE)"
                _lo_fsr_header "${_lo_fsr_header}")
            file(MAKE_DIRECTORY "${_lo_fsr_overlay}/FidelityFX/host")
            file(CONFIGURE OUTPUT "${_lo_fsr_overlay}/FidelityFX/host/ffx_fsr3upscaler.h" CONTENT "${_lo_fsr_header}" @ONLY)
            target_include_directories(lo_fsr3upscaler_vk BEFORE PUBLIC "${_lo_fsr_overlay}")
            target_compile_options(lo_fsr3upscaler_vk PRIVATE
                "-include${CMAKE_CURRENT_LIST_DIR}/../tools/fsr/posix_compat.h")
        endif()
        set(LO_FSR_AVAILABLE ON)
    endif()
endif()

function(lo_enable_fsr target)
    if(LO_FSR_AVAILABLE)
        target_link_libraries(${target} PRIVATE LoFsr::Vulkan)
        target_compile_definitions(${target} PRIVATE LO_HAS_FSR=1)
        get_target_property(_lo_fsr_target_type ${target} TYPE)
        if(_lo_fsr_target_type STREQUAL "EXECUTABLE")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/licenses"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${LO_FSR_SHADER_DIR}/LICENSE-FidelityFX.txt"
                "$<TARGET_FILE_DIR:${target}>/licenses/LICENSE-FidelityFX.txt")
        endif()
        if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
            install(FILES "${LO_FSR_SHADER_DIR}/LICENSE-FidelityFX.txt"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/lost-odyssey-recomp")
        endif()
    else()
        target_compile_definitions(${target} PRIVATE LO_HAS_FSR=0)
    endif()
endfunction()
