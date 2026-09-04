project(LostOdysseyRecomp LANGUAGES CXX)

set(LO_RUNTIME_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
set(LO_PRIVATE_DIR ${CMAKE_SOURCE_DIR}/LostOdysseyRecompLib/private)

# Logging stubs for every kernel import without an implementation. Regenerated
# whenever a runtime source changes so newly implemented imports drop out.
file(GLOB_RECURSE LO_RUNTIME_SOURCES CONFIGURE_DEPENDS
    "${LO_RUNTIME_ROOT}/*.cpp"
    "${LO_RUNTIME_ROOT}/*.h")
list(FILTER LO_RUNTIME_SOURCES EXCLUDE REGEX "imports_stubs\\.cpp$")

set(LO_IMPORT_STUBS "${LO_RUNTIME_ROOT}/kernel/imports_stubs.cpp")
add_custom_command(
    OUTPUT ${LO_IMPORT_STUBS}
    COMMAND python "${CMAKE_SOURCE_DIR}/tools/gen_import_stubs.py"
            "${LO_PRIVATE_DIR}/image_disc1.bin.sym" "${LO_RUNTIME_ROOT}" "${LO_IMPORT_STUBS}"
    DEPENDS ${LO_RUNTIME_SOURCES} "${CMAKE_SOURCE_DIR}/tools/gen_import_stubs.py" "${LO_PRIVATE_DIR}/image_disc1.bin.sym"
    COMMENT "Generating kernel import stubs"
    VERBATIM)

add_executable(LostOdysseyRecomp ${LO_RUNTIME_SOURCES} ${LO_IMPORT_STUBS})

target_include_directories(LostOdysseyRecomp PRIVATE
    ${LO_RUNTIME_ROOT}
    ${CMAKE_SOURCE_DIR}/LostOdysseyRecompLib
    ${CMAKE_SOURCE_DIR}/tools/XenonRecomp/XenonUtils
    ${CMAKE_SOURCE_DIR}/tools/XenonRecomp/thirdparty/simde
    ${CMAKE_SOURCE_DIR}/thirdparty/unordered_dense/include
    ${CMAKE_SOURCE_DIR}/thirdparty/o1heap/o1heap)

target_precompile_headers(LostOdysseyRecomp PRIVATE stdafx.h)
target_compile_features(LostOdysseyRecomp PRIVATE cxx_std_20)
target_compile_options(LostOdysseyRecomp PRIVATE -fno-strict-aliasing -Wno-switch -Wno-unused-function)
target_compile_definitions(LostOdysseyRecomp PRIVATE NOMINMAX)

target_link_libraries(LostOdysseyRecomp PRIVATE
    LostOdysseyRecompLib
    XenonUtils
    fmt::fmt
    xxHash::xxhash
    o1heap
    SDL2::SDL2-static)

if(WIN32)
    target_link_libraries(LostOdysseyRecomp PRIVATE winmm)
endif()
