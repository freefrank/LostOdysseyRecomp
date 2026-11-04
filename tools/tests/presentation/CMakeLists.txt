cmake_minimum_required(VERSION 3.28)
project(LoPresentationReadbacks LANGUAGES C CXX)
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(FATAL_ERROR "This standalone headless fixture currently targets Linux Vulkan")
endif()
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
get_filename_component(ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(LO_PRESENTATION_DEPENDENCY_ROOT "${ROOT}" CACHE PATH "Checkout with prepared pinned dependencies")
set(DEPS "${LO_PRESENTATION_DEPENDENCY_ROOT}")
set(CMAKE_CXX_STANDARD 20)
find_package(Threads REQUIRED)
find_package(X11 REQUIRED)
# No SDL window is created. Keep the pinned production backend and its normal
# no-window factory, rather than depending on initialized SDL video state.
set(PLUME_SDL_VULKAN_ENABLED OFF CACHE BOOL "" FORCE)
add_subdirectory("${DEPS}/thirdparty/plume" plume)
add_executable(LoPresentationTest
    ../presentation_test.cpp ../motion_replay/platform_stub.cpp
    "${ROOT}/LostOdysseyRecomp/gpu/presentation.cpp"
    "${ROOT}/LostOdysseyRecomp/gpu/shader/dxc_compiler.cpp")
# Only the runtime PCH is replaced: no Xbox executable or generated game code
# is needed. All renderer, compiler and readback bodies remain production code.
target_include_directories(LoPresentationTest BEFORE PRIVATE stubs
    "${ROOT}/LostOdysseyRecomp"
    "${DEPS}/tools/XenonRecomp/thirdparty/fmt/include"
    "${DEPS}/tools/XenonRecomp/thirdparty/xxHash"
    "${DEPS}/tools/XenosRecomp/thirdparty/dxc-bin/inc")
target_compile_definitions(LoPresentationTest PRIVATE LO_GPU_PLUME NOMINMAX FMT_HEADER_ONLY XXH_INLINE_ALL)
target_link_libraries(LoPresentationTest PRIVATE plume X11::X11 Threads::Threads ${CMAKE_DL_LIBS})
enable_testing()
add_test(NAME presentation_native COMMAND LoPresentationTest --vulkan)
add_test(NAME presentation_sgsr1_setting COMMAND LoPresentationTest --vulkan --sgsr1-setting)
add_test(NAME presentation_sgsr1_storage COMMAND LoPresentationTest --vulkan --sgsr1-setting --storage)
add_test(NAME presentation_sgsr1_env COMMAND LoPresentationTest --vulkan --sgsr1)
add_test(NAME presentation_sgsr1_hdr COMMAND LoPresentationTest --vulkan --sgsr1 --hdr-only)
add_test(NAME presentation_sgsr1_calibration COMMAND LoPresentationTest --vulkan --sgsr1 --hdr-calibration-only)
file(READ "${ROOT}/tools/tests/presentation_test.cpp" presentation_fixture_source)
# Older review bases predate the extended HDR-transfer fixture/CLI mode.
if(presentation_fixture_source MATCHES "--hdr-transfer-only")
    add_test(NAME presentation_sgsr1_hdr_transfer COMMAND LoPresentationTest --vulkan --sgsr1 --hdr-transfer-only)
endif()
get_property(tests DIRECTORY PROPERTY TESTS)
set_tests_properties(${tests} PROPERTIES TIMEOUT 120 ENVIRONMENT
    "LO_SGSR1=0;LO_SHADER_CACHE_DIR=${CMAKE_CURRENT_BINARY_DIR}/shader-cache"
    ENVIRONMENT_MODIFICATION "LD_LIBRARY_PATH=path_list_prepend:${DEPS}/tools/XenosRecomp/thirdparty/dxc-bin/lib/x64")
