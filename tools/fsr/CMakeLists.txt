cmake_minimum_required(VERSION 3.28)
project(LoFsrSdk LANGUAGES C CXX)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
include(../../cmake/LoFsr.cmake)
if(LO_FSR_AVAILABLE)
    add_executable(LoFsrSdkLinkCheck EXCLUDE_FROM_ALL sdk_link_check.cpp)
    lo_enable_fsr(LoFsrSdkLinkCheck)
    if(LO_FSR_DX12_AVAILABLE)
        target_sources(LoFsrSdkLinkCheck PRIVATE dx12_sdk_link_check.cpp)
    endif()
    target_sources(LoFsrSdkLinkCheck PRIVATE "${LO_FSR_VOLK_DIR}/volk.c")
    target_link_libraries(LoFsrSdkLinkCheck PRIVATE ${CMAKE_DL_LIBS})
    if(EXISTS "${CMAKE_CURRENT_LIST_DIR}/../../LostOdysseyRecomp/gpu/fsr_upscaler.cpp")
        add_library(LoFsrAdapterCompile OBJECT EXCLUDE_FROM_ALL
            ../../LostOdysseyRecomp/gpu/fsr_upscaler.cpp
            ../../LostOdysseyRecomp/gpu/fsr_upscaler_d3d12.cpp)
        target_compile_features(LoFsrAdapterCompile PRIVATE cxx_std_20)
        target_compile_definitions(LoFsrAdapterCompile PRIVATE LO_GPU_PLUME=1 NOMINMAX)
        target_include_directories(LoFsrAdapterCompile PRIVATE
            ../../LostOdysseyRecomp ../../thirdparty/plume
            ../../thirdparty/plume/contrib/D3D12MemoryAllocator/include
            ../../thirdparty/plume/contrib/volk
            ../../thirdparty/plume/contrib/VulkanMemoryAllocator/include)
        lo_enable_fsr(LoFsrAdapterCompile)
    endif()
endif()
