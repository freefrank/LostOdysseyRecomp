cmake_minimum_required(VERSION 3.20)
project(xexdump LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# XenonRecomp is written for clang-cl; map the few builtins it uses so MSVC can build the tools.
if(MSVC AND NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    add_compile_definitions(
        __builtin_bswap16=_byteswap_ushort
        __builtin_bswap32=_byteswap_ulong
        __builtin_bswap64=_byteswap_uint64
    )
    add_compile_options(/wd4996)
endif()

# Pull in XenonRecomp / XenonAnalyse / XenonUtils from the submodule.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../XenonRecomp ${CMAKE_BINARY_DIR}/XenonRecomp)

add_executable(xexdump main.cpp)
target_compile_definitions(xexdump PRIVATE NOMINMAX)
target_link_libraries(xexdump PRIVATE XenonUtils)
