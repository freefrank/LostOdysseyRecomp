cmake_minimum_required(VERSION 3.20)
project(LoResolveReconfigure LANGUAGES CXX)
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(FIXTURE "${CMAKE_CURRENT_BINARY_DIR}/fixture.cpp")
add_custom_command(OUTPUT "${FIXTURE}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/generate.py"
        "${ROOT}" "${FIXTURE}"
    DEPENDS generate.py fixture.cpp.in "${ROOT}/LostOdysseyRecomp/gpu/renderer.cpp")
add_executable(LoResolveReconfigureTest "${FIXTURE}")
target_compile_features(LoResolveReconfigureTest PRIVATE cxx_std_20)
target_include_directories(LoResolveReconfigureTest PRIVATE "${ROOT}/LostOdysseyRecomp")
option(LO_RESOLVE_SANITIZE "Enable address/undefined sanitizers" ${LO_REVIEW_SANITIZE})
if(LO_RESOLVE_SANITIZE AND NOT MSVC)
    target_compile_options(LoResolveReconfigureTest PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(LoResolveReconfigureTest PRIVATE -fsanitize=address,undefined)
endif()
enable_testing()
add_test(NAME resolve_reconfigure COMMAND LoResolveReconfigureTest)
