cmake_minimum_required(VERSION 3.28)
project(LoFsrAdapterGpuFixture LANGUAGES C CXX)

# Separate from the game build; it needs neither PPC output nor game assets.
get_filename_component(LO_REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
set(LO_ENABLE_FSR ON CACHE BOOL "" FORCE)
set(LO_REQUIRE_FSR ON CACHE BOOL "" FORCE)
set(LO_FSR_SDK_ROOT "${LO_REPO_ROOT}/.cache/deps/fidelityfx-sdk-v1.1.4" CACHE PATH "")
set(LO_FSR_SHADER_DIR "${LO_REPO_ROOT}/out/fsr-shaders-vk" CACHE PATH "")
include("${LO_REPO_ROOT}/cmake/LoFsr.cmake")
add_subdirectory("${LO_REPO_ROOT}/thirdparty/plume" plume)

option(LO_FSR_FIXTURE_VALIDATION "Enable Khronos Vulkan validation in Plume" ON)
if(LO_FSR_FIXTURE_VALIDATION)
    target_compile_definitions(plume PRIVATE VULKAN_VALIDATION_LAYER_ENABLED=1)
endif()

add_executable(LoFsrAdapterGpuTest EXCLUDE_FROM_ALL
    fsr_adapter_gpu_test.cpp
    "${LO_REPO_ROOT}/LostOdysseyRecomp/gpu/fsr_upscaler.cpp")
target_compile_features(LoFsrAdapterGpuTest PRIVATE cxx_std_20)
target_compile_definitions(LoFsrAdapterGpuTest PRIVATE LO_GPU_PLUME=1 NOMINMAX)
target_include_directories(LoFsrAdapterGpuTest PRIVATE "${LO_REPO_ROOT}/LostOdysseyRecomp")
target_link_libraries(LoFsrAdapterGpuTest PRIVATE plume)
lo_enable_fsr(LoFsrAdapterGpuTest)
add_executable(LoFsrMaskPolicyTest EXCLUDE_FROM_ALL fsr_mask_policy_test.cpp)
target_compile_features(LoFsrMaskPolicyTest PRIVATE cxx_std_20)
target_include_directories(LoFsrMaskPolicyTest PRIVATE "${LO_REPO_ROOT}/LostOdysseyRecomp")
enable_testing()
add_test(NAME LoFsrMaskPolicyTest COMMAND LoFsrMaskPolicyTest)
add_test(NAME LoFsrAdapterP2 COMMAND LoFsrAdapterGpuTest --p2-only)
add_executable(LoFsrCrossSubmitGpuTest EXCLUDE_FROM_ALL
    fsr_cross_submit_gpu_test.cpp
    "${LO_REPO_ROOT}/LostOdysseyRecomp/gpu/fsr_upscaler.cpp")
target_compile_features(LoFsrCrossSubmitGpuTest PRIVATE cxx_std_20)
target_compile_definitions(LoFsrCrossSubmitGpuTest PRIVATE LO_GPU_PLUME=1 NOMINMAX)
target_include_directories(LoFsrCrossSubmitGpuTest PRIVATE "${LO_REPO_ROOT}/LostOdysseyRecomp")
target_link_libraries(LoFsrCrossSubmitGpuTest PRIVATE plume)
lo_enable_fsr(LoFsrCrossSubmitGpuTest)
add_test(NAME LoFsrCrossSubmitGpuTest COMMAND LoFsrCrossSubmitGpuTest
    "${CMAKE_CURRENT_BINARY_DIR}/cross-submit-result.json")
add_subdirectory("${LO_REPO_ROOT}/tools/XenosRecomp/thirdparty/fmt" fmt EXCLUDE_FROM_ALL)
add_executable(LoFsrRasterEdgeGpuTest EXCLUDE_FROM_ALL
    fsr_raster_edge_gpu_test.cpp
    "${LO_REPO_ROOT}/LostOdysseyRecomp/gpu/shader/xenos_translator.cpp"
    "${LO_REPO_ROOT}/LostOdysseyRecomp/gpu/shader/dxc_compiler.cpp")
target_compile_features(LoFsrRasterEdgeGpuTest PRIVATE cxx_std_20)
target_compile_definitions(LoFsrRasterEdgeGpuTest PRIVATE LO_GPU_PLUME=1 NOMINMAX
    LO_FIXTURE_ROOT="${LO_REPO_ROOT}")
target_include_directories(LoFsrRasterEdgeGpuTest PRIVATE
    "${LO_REPO_ROOT}/LostOdysseyRecomp"
    "${LO_REPO_ROOT}/tools/XenosRecomp/XenosRecomp"
    "${LO_REPO_ROOT}/tools/XenosRecomp/thirdparty/dxc-bin/inc")
target_link_libraries(LoFsrRasterEdgeGpuTest PRIVATE plume fmt::fmt ${CMAKE_DL_LIBS})
add_test(NAME LoFsrRasterEdgeGpuTest COMMAND LoFsrRasterEdgeGpuTest
    "${CMAKE_CURRENT_BINARY_DIR}/raster-edge-result.json")
add_executable(LoFsrColorRoundtripGpuTest EXCLUDE_FROM_ALL fsr_color_roundtrip_gpu_test.cpp)
target_compile_features(LoFsrColorRoundtripGpuTest PRIVATE cxx_std_20)
target_compile_definitions(LoFsrColorRoundtripGpuTest PRIVATE NOMINMAX)
target_include_directories(LoFsrColorRoundtripGpuTest PRIVATE "${LO_FSR_SHADER_DIR}")
target_link_libraries(LoFsrColorRoundtripGpuTest PRIVATE plume)
if(WIN32)
    target_compile_options(LoFsrCrossSubmitGpuTest PRIVATE /utf-8)
    target_link_libraries(LoFsrCrossSubmitGpuTest PRIVATE d3d12 dxgi)
    target_compile_options(LoFsrRasterEdgeGpuTest PRIVATE /utf-8)
    target_link_libraries(LoFsrRasterEdgeGpuTest PRIVATE d3d12 dxgi)
    target_link_libraries(LoFsrColorRoundtripGpuTest PRIVATE d3d12 dxgi)
endif()
add_test(NAME LoFsrColorRoundtripGpuTest COMMAND LoFsrColorRoundtripGpuTest
    --output "${LO_REPO_ROOT}/out/streamline-fg-p0/fsr-p2-color-roundtrip")
if(WIN32)
    target_compile_options(LoFsrAdapterGpuTest PRIVATE /utf-8)
    target_link_libraries(LoFsrAdapterGpuTest PRIVATE d3d12 dxgi)
    if(MSVC)
        # Keep the Release code path while retaining symbols for WER dumps.
        foreach(LO_FSR_DEBUG_TARGET IN ITEMS LoFsrAdapterGpuTest plume lo_fsr3upscaler_vk)
            target_compile_options(${LO_FSR_DEBUG_TARGET} PRIVATE /Zi)
        endforeach()
        target_link_options(LoFsrAdapterGpuTest PRIVATE /DEBUG:FULL)
    endif()
else()
    find_package(X11 REQUIRED)
    target_include_directories(LoFsrAdapterGpuTest PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(LoFsrAdapterGpuTest PRIVATE ${X11_LIBRARIES} ${CMAKE_DL_LIBS})
    target_include_directories(LoFsrColorRoundtripGpuTest PRIVATE ${X11_INCLUDE_DIR})
    target_link_libraries(LoFsrColorRoundtripGpuTest PRIVATE ${X11_LIBRARIES} ${CMAKE_DL_LIBS})
endif()
