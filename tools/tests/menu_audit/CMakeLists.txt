cmake_minimum_required(VERSION 3.20)
project(LoMenuAuditTests LANGUAGES CXX)
include(CTest)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
find_package(Threads REQUIRED)
get_filename_component(LO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(LO_GENERATED "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(LO_EXTRACTED ${LO_GENERATED}/input_dispatch.inc ${LO_GENERATED}/overlay_logic.inc
    ${LO_GENERATED}/video_prepare.inc ${LO_GENERATED}/cpu_frame.inc)
add_custom_command(OUTPUT ${LO_EXTRACTED}
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/extract_sources.py
        --root ${LO_ROOT} --out ${LO_GENERATED}
    DEPENDS extract_sources.py
        ${LO_ROOT}/LostOdysseyRecomp/hid/hid.cpp
        ${LO_ROOT}/LostOdysseyRecomp/debug/menu_overlay.cpp
        ${LO_ROOT}/LostOdysseyRecomp/gpu/video.cpp
    VERBATIM)
add_custom_target(LoExtractMenuAuditSources DEPENDS ${LO_EXTRACTED})
option(LO_MENU_AUDIT_SANITIZERS "Enable address and undefined-behavior sanitizers" OFF)
function(lo_audit_test name source)
    add_executable(${name} ${source})
    add_dependencies(${name} LoExtractMenuAuditSources)
    target_include_directories(${name} PRIVATE ${LO_ROOT}/LostOdysseyRecomp ${LO_GENERATED})
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_link_libraries(${name} PRIVATE Threads::Threads)
    if(MSVC)
        target_compile_options(${name} PRIVATE /utf-8 /W4)
    else()
        target_compile_options(${name} PRIVATE -Wall -Wextra -Wpedantic)
        if(LO_MENU_AUDIT_SANITIZERS)
            target_compile_options(${name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${name} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 30)
endfunction()
lo_audit_test(LoMenuInputQuarantineTest input_test.cpp)
lo_audit_test(LoMenuCoordinateRefillTest overlay_test.cpp)
lo_audit_test(LoMenuPresentationPosixModelTest video_test.cpp)
lo_audit_test(LoMenuPresentationWindowsModelTest video_test.cpp)
target_compile_definitions(LoMenuPresentationWindowsModelTest PRIVATE LO_TEST_WINDOWS_PATH=1)
