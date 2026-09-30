cmake_minimum_required(VERSION 3.20)
project(LoFrameGenerationPresentTest LANGUAGES CXX)

enable_testing()

add_executable(frame_generation_present_test
    ../frame_generation_present_test.cpp)
target_include_directories(frame_generation_present_test PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/../../../LostOdysseyRecomp)
target_compile_features(frame_generation_present_test PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(frame_generation_present_test PRIVATE /UNDEBUG)
else()
    target_compile_options(frame_generation_present_test PRIVATE -UNDEBUG)
endif()

add_test(NAME frame_generation_present COMMAND frame_generation_present_test)
