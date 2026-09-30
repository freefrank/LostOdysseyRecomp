cmake_minimum_required(VERSION 3.20)
project(LoControllerAtlasFixture LANGUAGES CXX)
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
add_executable(LoControllerAtlasFixture fixture.cpp "${ROOT}/thirdparty/lzokay/lzokay.cpp")
target_compile_features(LoControllerAtlasFixture PRIVATE cxx_std_20)
target_include_directories(LoControllerAtlasFixture PRIVATE "${ROOT}/LostOdysseyRecomp" "${ROOT}/thirdparty/lzokay")
if(MSVC)
    target_compile_options(LoControllerAtlasFixture PRIVATE /utf-8)
endif()
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(RENDERER_FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/renderer_fixture.cpp")
add_custom_command(OUTPUT "${RENDERER_FIXTURE}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate_renderer_fixture.py"
        --root "${ROOT}" --output "${RENDERER_FIXTURE}"
    DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/generate_renderer_fixture.py"
            "${CMAKE_CURRENT_SOURCE_DIR}/renderer_fixture.template.cpp"
            "${ROOT}/LostOdysseyRecomp/gpu/renderer.cpp")
add_executable(LoControllerAtlasRendererFixture "${RENDERER_FIXTURE}")
target_compile_features(LoControllerAtlasRendererFixture PRIVATE cxx_std_20)
target_include_directories(LoControllerAtlasRendererFixture PRIVATE "${ROOT}/LostOdysseyRecomp")
if(MSVC)
    target_compile_options(LoControllerAtlasRendererFixture PRIVATE /utf-8)
endif()
