# This directory is independently configurable: it does not include the game,
# PPC generation, or private assets.
if(CMAKE_CURRENT_SOURCE_DIR STREQUAL CMAKE_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.28)
    project(LoNativeDlssFixtures LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 20)
    if(MSVC)
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
    set(LO_NATIVE_DLSS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../..")
    include("${LO_NATIVE_DLSS_ROOT}/cmake/LoDlss.cmake")
    add_subdirectory("${LO_NATIVE_DLSS_ROOT}/thirdparty/plume" plume)
    set(LO_NATIVE_DLSS_STANDALONE ON)
else()
    set(LO_NATIVE_DLSS_ROOT "${CMAKE_SOURCE_DIR}")
    set(LO_NATIVE_DLSS_STANDALONE OFF)
endif()
option(LO_NATIVE_DLSS_ENABLE_VALIDATION_LAYER "Enable Vulkan validation for native-DLSS fixture and Plume builds" ${LO_NATIVE_DLSS_STANDALONE})

# Deliberately excluded from the normal build: these fixtures do not touch game
# assets and are run only by the focused native-DLSS validation lane.
add_executable(LoPlumeBridgeTest EXCLUDE_FROM_ALL plume_bridge_test.cpp)
target_include_directories(LoPlumeBridgeTest PRIVATE ${LO_NATIVE_DLSS_ROOT}/thirdparty/plume)
target_compile_features(LoPlumeBridgeTest PRIVATE cxx_std_20)
target_link_libraries(LoPlumeBridgeTest PRIVATE plume)
if(WIN32)
    target_link_libraries(LoPlumeBridgeTest PRIVATE d3d12 dxgi)
endif()

add_executable(LoNativeDlssProbe EXCLUDE_FROM_ALL
    native_dlss_probe.cpp
    ${LO_NATIVE_DLSS_ROOT}/LostOdysseyRecomp/gpu/dlss_ngx.cpp)
target_include_directories(LoNativeDlssProbe PRIVATE
    ${LO_NATIVE_DLSS_ROOT}/LostOdysseyRecomp
    ${LO_NATIVE_DLSS_ROOT}/thirdparty/plume)
target_compile_features(LoNativeDlssProbe PRIVATE cxx_std_20)
target_compile_definitions(LoNativeDlssProbe PRIVATE LO_GPU_PLUME=1)
target_link_libraries(LoNativeDlssProbe PRIVATE plume)
if(WIN32)
    target_link_libraries(LoNativeDlssProbe PRIVATE d3d12 dxgi)
endif()
lo_enable_dlss(LoNativeDlssProbe)

add_executable(LoNativeDlssReportTest EXCLUDE_FROM_ALL
    native_dlss_report_test.cpp
    ${LO_NATIVE_DLSS_ROOT}/LostOdysseyRecomp/gpu/dlss_ngx.cpp)
target_include_directories(LoNativeDlssReportTest PRIVATE
    ${LO_NATIVE_DLSS_ROOT}/LostOdysseyRecomp
    ${LO_NATIVE_DLSS_ROOT}/thirdparty/plume)
target_compile_features(LoNativeDlssReportTest PRIVATE cxx_std_20)
target_compile_definitions(LoNativeDlssReportTest PRIVATE LO_GPU_PLUME=1)
target_link_libraries(LoNativeDlssReportTest PRIVATE plume)
lo_enable_dlss(LoNativeDlssReportTest)

add_executable(LoNativeDlssExecutionTest EXCLUDE_FROM_ALL
    native_dlss_execution_test.cpp
    ${LO_NATIVE_DLSS_ROOT}/LostOdysseyRecomp/gpu/dlss_ngx.cpp)
target_include_directories(LoNativeDlssExecutionTest PRIVATE
    ${LO_NATIVE_DLSS_ROOT}/LostOdysseyRecomp
    ${LO_NATIVE_DLSS_ROOT}/thirdparty/plume)
target_compile_features(LoNativeDlssExecutionTest PRIVATE cxx_std_20)
target_compile_definitions(LoNativeDlssExecutionTest PRIVATE LO_GPU_PLUME=1 NOMINMAX LO_NATIVE_DLSS_TEST_INJECT_EVALUATE_FAILURE=1)
if(LO_NATIVE_DLSS_ENABLE_VALIDATION_LAYER)
    target_compile_definitions(LoNativeDlssExecutionTest PRIVATE VULKAN_VALIDATION_LAYER_ENABLED=1)
    target_compile_definitions(plume PRIVATE VULKAN_VALIDATION_LAYER_ENABLED=1)
endif()
target_link_libraries(LoNativeDlssExecutionTest PRIVATE plume)
if(WIN32)
    target_link_libraries(LoNativeDlssExecutionTest PRIVATE d3d12 dxgi)
endif()
lo_enable_dlss(LoNativeDlssExecutionTest)

add_executable(LoP1FramePlanTest EXCLUDE_FROM_ALL
    ${LO_NATIVE_DLSS_ROOT}/tools/tests/frame_plan_test.cpp
    ${LO_NATIVE_DLSS_ROOT}/LostOdysseyRecomp/gpu/upscaling_plan.cpp)
target_include_directories(LoP1FramePlanTest PRIVATE ${LO_NATIVE_DLSS_ROOT}/LostOdysseyRecomp)
target_compile_features(LoP1FramePlanTest PRIVATE cxx_std_20)

if(UNIX)
    if(PLUME_SDL_VULKAN_ENABLED)
        if(TARGET SDL2::SDL2-static)
            target_link_libraries(LoPlumeBridgeTest PRIVATE SDL2::SDL2-static)
            target_link_libraries(LoNativeDlssProbe PRIVATE SDL2::SDL2-static)
            target_link_libraries(LoNativeDlssReportTest PRIVATE SDL2::SDL2-static)
            target_link_libraries(LoNativeDlssExecutionTest PRIVATE SDL2::SDL2-static)
        else()
            target_link_libraries(LoPlumeBridgeTest PRIVATE SDL2::SDL2)
            target_link_libraries(LoNativeDlssProbe PRIVATE SDL2::SDL2)
            target_link_libraries(LoNativeDlssReportTest PRIVATE SDL2::SDL2)
            target_link_libraries(LoNativeDlssExecutionTest PRIVATE SDL2::SDL2)
        endif()
    else()
        find_package(X11 REQUIRED)
        foreach(target LoPlumeBridgeTest LoNativeDlssProbe LoNativeDlssReportTest LoNativeDlssExecutionTest)
            target_include_directories(${target} PRIVATE ${X11_INCLUDE_DIR})
            target_link_libraries(${target} PRIVATE ${X11_LIBRARIES})
        endforeach()
    endif()
    target_link_libraries(LoPlumeBridgeTest PRIVATE ${CMAKE_DL_LIBS})
    target_link_libraries(LoNativeDlssProbe PRIVATE ${CMAKE_DL_LIBS})
    target_link_libraries(LoNativeDlssReportTest PRIVATE ${CMAKE_DL_LIBS})
    target_link_libraries(LoNativeDlssExecutionTest PRIVATE ${CMAKE_DL_LIBS})
endif()

include(CTest)
if(BUILD_TESTING)
    add_test(NAME LoNativeDlssProbe COMMAND LoNativeDlssProbe)
    set_tests_properties(LoNativeDlssProbe PROPERTIES SKIP_RETURN_CODE 77)
    add_test(NAME LoNativeDlssReportTest COMMAND LoNativeDlssReportTest)
    add_test(NAME LoNativeDlssExecutionTest COMMAND LoNativeDlssExecutionTest)
    set_tests_properties(LoNativeDlssExecutionTest PROPERTIES SKIP_RETURN_CODE 77)
endif()
