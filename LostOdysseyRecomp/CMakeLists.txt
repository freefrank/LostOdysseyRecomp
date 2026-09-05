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

if(LO_BUILD_GPU)
    target_link_libraries(LostOdysseyRecomp PRIVATE plume)
    target_include_directories(LostOdysseyRecomp PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/plume)
    target_compile_definitions(LostOdysseyRecomp PRIVATE LO_GPU_PLUME=1)
    if(WIN32)
        target_link_libraries(LostOdysseyRecomp PRIVATE d3d12 dxgi)
    endif()
endif()

if(WIN32)
    target_link_libraries(LostOdysseyRecomp PRIVATE winmm onecore)
endif()

# Integration test uses the actual guest import bridge and storage implementation.
set(LO_STORAGE_TEST_SOURCES ${LO_RUNTIME_SOURCES})
list(FILTER LO_STORAGE_TEST_SOURCES EXCLUDE REGEX "/main\\.cpp$")
add_executable(LoStorageTest ${LO_STORAGE_TEST_SOURCES} ${LO_IMPORT_STUBS}
    ${CMAKE_SOURCE_DIR}/tools/tests/storage_test.cpp)
target_include_directories(LoStorageTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,INCLUDE_DIRECTORIES>)
target_compile_definitions(LoStorageTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_DEFINITIONS>)
target_compile_options(LoStorageTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_OPTIONS>)
target_compile_features(LoStorageTest PRIVATE cxx_std_20)
target_precompile_headers(LoStorageTest REUSE_FROM LostOdysseyRecomp)
target_link_libraries(LoStorageTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,LINK_LIBRARIES>)

# Offline shader translator check (shares gpu/shader with the runtime).
add_executable(LoShaderTool
    ${CMAKE_SOURCE_DIR}/tools/xenos_shader_tool/main.cpp
    ${LO_RUNTIME_ROOT}/gpu/shader/xenos_translator.cpp
    ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
target_include_directories(LoShaderTool PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoShaderTool PRIVATE cxx_std_20)
target_compile_definitions(LoShaderTool PRIVATE NOMINMAX)
target_link_libraries(LoShaderTool PRIVATE fmt::fmt)

# Tests packed mip block origins without game assets.
add_executable(LoTextureLayoutTest ${CMAKE_SOURCE_DIR}/tools/tests/texture_layout_test.cpp)
target_include_directories(LoTextureLayoutTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoTextureLayoutTest PRIVATE cxx_std_20)

# Tests the exact address mappings used by the runtime, without game assets.
add_executable(LoMemoryAliasTest
    ${CMAKE_SOURCE_DIR}/tools/tests/memory_alias_test.cpp
    ${LO_RUNTIME_ROOT}/kernel/guest_address_space.cpp)
target_include_directories(LoMemoryAliasTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoMemoryAliasTest PRIVATE cxx_std_20)
if(WIN32)
    target_link_libraries(LoMemoryAliasTest PRIVATE onecore)
    add_executable(LoShaderAluTest
        ${CMAKE_SOURCE_DIR}/tools/tests/shader_alu_test.cpp
        ${LO_RUNTIME_ROOT}/gpu/shader/xenos_translator.cpp
        ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
    target_include_directories(LoShaderAluTest PRIVATE ${LO_RUNTIME_ROOT})
    target_compile_features(LoShaderAluTest PRIVATE cxx_std_20)
    target_compile_definitions(LoShaderAluTest PRIVATE NOMINMAX)
    target_link_libraries(LoShaderAluTest PRIVATE fmt::fmt d3d12 dxgi)
    if(LO_BUILD_GPU)
        add_executable(LoStencilTest
            ${CMAKE_SOURCE_DIR}/tools/tests/stencil_test.cpp
            ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
        target_include_directories(LoStencilTest PRIVATE ${LO_RUNTIME_ROOT} ${CMAKE_SOURCE_DIR}/thirdparty/plume)
        target_compile_features(LoStencilTest PRIVATE cxx_std_20)
        target_compile_definitions(LoStencilTest PRIVATE NOMINMAX)
        target_link_libraries(LoStencilTest PRIVATE plume d3d12 dxgi)
    endif()
endif()
