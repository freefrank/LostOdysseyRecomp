cmake_minimum_required(VERSION 3.20)
project(LoCinematicBarsTests LANGUAGES CXX)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(RUNTIME "${REPO_ROOT}/LostOdysseyRecomp")
include(CTest)

# Compile the production wrappers; only their guest/runtime dependencies are stubs.
add_executable(LoCinematicBarsSignalTest
    signal_test.cpp "${RUNTIME}/gpu/cinematic_bars.cpp")
target_include_directories(LoCinematicBarsSignalTest PRIVATE stubs "${RUNTIME}")
target_compile_features(LoCinematicBarsSignalTest PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(LoCinematicBarsSignalTest PRIVATE /UNDEBUG /utf-8)
else()
    target_compile_options(LoCinematicBarsSignalTest PRIVATE -UNDEBUG)
endif()
add_test(NAME cinematic_bars_signal COMMAND LoCinematicBarsSignalTest)
