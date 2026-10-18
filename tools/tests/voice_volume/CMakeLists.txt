cmake_minimum_required(VERSION 3.20)
project(LoVoiceVolumeTests LANGUAGES CXX)
get_filename_component(REPO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(RUNTIME "${REPO_ROOT}/LostOdysseyRecomp")
include(CTest)
find_package(Threads REQUIRED)

# Compile both production wrappers; only guest memory, retail calls and APU are stubs.
add_executable(LoVoiceVolumeTest
    volume_test.cpp "${RUNTIME}/patches/voice_volume.cpp")
target_include_directories(LoVoiceVolumeTest PRIVATE stubs "${RUNTIME}")
target_compile_features(LoVoiceVolumeTest PRIVATE cxx_std_20)
target_link_libraries(LoVoiceVolumeTest PRIVATE Threads::Threads)
if(MSVC)
    target_compile_options(LoVoiceVolumeTest PRIVATE /UNDEBUG /utf-8)
else()
    target_compile_options(LoVoiceVolumeTest PRIVATE -UNDEBUG)
endif()
add_test(NAME voice_volume COMMAND LoVoiceVolumeTest)
