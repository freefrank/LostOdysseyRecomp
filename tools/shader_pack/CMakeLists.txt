cmake_minimum_required(VERSION 3.25)
project(LoShaderPackTools LANGUAGES C CXX)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/portable_shader_pack.cmake")
enable_testing()
add_test(NAME portable-pack-core COMMAND LoPortableShaderPackTest)
add_test(NAME portable-pack-integration COMMAND LoPortableShaderPackIntegrationTest)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
add_test(NAME portable-pack-merge COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_LIST_DIR}/../tests/portable_shader_merge_test.py"
    --tool $<TARGET_FILE:LoShaderPackTool> --fixtures $<TARGET_FILE:LoPortableShaderContractTest>)

add_test(NAME portable-pack-release-contract
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_LIST_DIR}/../tests/portable_shader_release_test.py"
        --tool $<TARGET_FILE:LoShaderPackTool> --fixtures $<TARGET_FILE:LoPortableShaderContractTest>)
