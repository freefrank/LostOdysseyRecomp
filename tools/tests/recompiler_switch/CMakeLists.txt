cmake_minimum_required(VERSION 3.20)
project(RecompilerSwitchTest LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(RECOMP_ROOT "${REPO_ROOT}/tools/XenonRecomp")
add_subdirectory("${RECOMP_ROOT}" xenon EXCLUDE_FROM_ALL)

# Exercise production instruction decoding and generation, without the game.
add_executable(RecompilerSwitchGenerate
    generate.cpp
    "${RECOMP_ROOT}/XenonRecomp/recompiler.cpp"
    "${RECOMP_ROOT}/XenonRecomp/recompiler_config.cpp")
target_include_directories(RecompilerSwitchGenerate PRIVATE "${RECOMP_ROOT}/XenonRecomp")
target_precompile_headers(RecompilerSwitchGenerate PRIVATE "${RECOMP_ROOT}/XenonRecomp/pch.h")
target_link_libraries(RecompilerSwitchGenerate PRIVATE
    LibXenonAnalyse XenonUtils fmt::fmt tomlplusplus::tomlplusplus xxHash::xxhash)
target_compile_definitions(RecompilerSwitchGenerate PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(RecompilerSwitchGenerate PRIVATE
    -Wno-switch -Wno-unused-variable -Wno-null-arithmetic)
