cmake_minimum_required(VERSION 3.20)
project(RecompilerSemanticsTest LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(RECOMP_ROOT "${REPO_ROOT}/tools/XenonRecomp")
add_subdirectory("${RECOMP_ROOT}" xenon EXCLUDE_FROM_ALL)
if(NOT RECOMPILER_SOURCE)
    set(RECOMPILER_SOURCE "${RECOMP_ROOT}/XenonRecomp/recompiler.cpp")
endif()
add_executable(RecompilerSemanticsGenerate generate.cpp "${RECOMPILER_SOURCE}"
    "${RECOMP_ROOT}/XenonRecomp/recompiler_config.cpp")
target_include_directories(RecompilerSemanticsGenerate PRIVATE "${RECOMP_ROOT}/XenonRecomp")
target_precompile_headers(RecompilerSemanticsGenerate PRIVATE "${RECOMP_ROOT}/XenonRecomp/pch.h")
target_link_libraries(RecompilerSemanticsGenerate PRIVATE
    LibXenonAnalyse XenonUtils fmt::fmt tomlplusplus::tomlplusplus xxHash::xxhash)
target_compile_definitions(RecompilerSemanticsGenerate PRIVATE _CRT_SECURE_NO_WARNINGS)
target_compile_options(RecompilerSemanticsGenerate PRIVATE
    -Wno-switch -Wno-unused-variable -Wno-null-arithmetic)
