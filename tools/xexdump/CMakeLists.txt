cmake_minimum_required(VERSION 3.20)
project(xexdump LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Pull in XenonRecomp / XenonAnalyse / XenonUtils from the submodule.
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/../XenonRecomp ${CMAKE_BINARY_DIR}/XenonRecomp)

add_executable(xexdump main.cpp)
target_compile_definitions(xexdump PRIVATE NOMINMAX)
target_link_libraries(xexdump PRIVATE XenonUtils)
