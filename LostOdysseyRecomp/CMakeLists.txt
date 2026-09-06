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
if(WIN32)
    # Keep the existing main/CRT initialization while avoiding a console on double-click.
    target_link_options(LostOdysseyRecomp PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()

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
target_link_libraries(LostOdysseyRecomp PRIVATE lo_avcodec)

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

# Isolated original-game disc request probe; not included in release packages.
add_executable(LoDiscRuntimeTest EXCLUDE_FROM_ALL ${LO_RUNTIME_SOURCES} ${LO_IMPORT_STUBS}
    ${CMAKE_SOURCE_DIR}/tools/tests/disc_runtime_probe.cpp)
target_include_directories(LoDiscRuntimeTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,INCLUDE_DIRECTORIES>)
target_compile_definitions(LoDiscRuntimeTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_DEFINITIONS>)
target_compile_options(LoDiscRuntimeTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_OPTIONS>)
target_compile_features(LoDiscRuntimeTest PRIVATE cxx_std_20)
target_precompile_headers(LoDiscRuntimeTest REUSE_FROM LostOdysseyRecomp)
target_link_libraries(LoDiscRuntimeTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,LINK_LIBRARIES>)

# Guest string formatting regression without starting the game.
add_executable(LoGuestPrintfTest
    ${LO_RUNTIME_ROOT}/kernel/guest_printf.cpp
    ${CMAKE_SOURCE_DIR}/tools/tests/guest_printf_test.cpp)
target_include_directories(LoGuestPrintfTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,INCLUDE_DIRECTORIES>)
target_compile_definitions(LoGuestPrintfTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_DEFINITIONS>)
target_compile_options(LoGuestPrintfTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_OPTIONS>)
target_compile_features(LoGuestPrintfTest PRIVATE cxx_std_20)
target_precompile_headers(LoGuestPrintfTest REUSE_FROM LostOdysseyRecomp)
target_link_libraries(LoGuestPrintfTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,LINK_LIBRARIES>)

# Offline shader translator check (shares gpu/shader with the runtime).
add_executable(LoHidTest ${CMAKE_SOURCE_DIR}/tools/tests/hid_test.cpp ${LO_RUNTIME_ROOT}/hid/hid.cpp)
target_include_directories(LoHidTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,INCLUDE_DIRECTORIES>)
target_compile_definitions(LoHidTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_DEFINITIONS>)
target_compile_options(LoHidTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_OPTIONS>)
target_compile_features(LoHidTest PRIVATE cxx_std_20)
target_precompile_headers(LoHidTest REUSE_FROM LostOdysseyRecomp)
target_link_libraries(LoHidTest PRIVATE fmt::fmt SDL2::SDL2-static xxHash::xxhash)

add_executable(LoShaderTool
    ${CMAKE_SOURCE_DIR}/tools/xenos_shader_tool/main.cpp
    ${LO_RUNTIME_ROOT}/gpu/shader/xenos_translator.cpp
    ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
target_include_directories(LoShaderTool PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoShaderTool PRIVATE cxx_std_20)
target_compile_definitions(LoShaderTool PRIVATE NOMINMAX)
target_link_libraries(LoShaderTool PRIVATE fmt::fmt)

# Tests packed mip block origins without game assets.
add_executable(LoXmaLoopTest ${CMAKE_SOURCE_DIR}/tools/tests/xma_loop_test.cpp)
target_include_directories(LoXmaLoopTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoXmaLoopTest PRIVATE cxx_std_20)

add_executable(LoTextureLayoutTest ${CMAKE_SOURCE_DIR}/tools/tests/texture_layout_test.cpp)
target_include_directories(LoTextureLayoutTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoTextureLayoutTest PRIVATE cxx_std_20)

# Tests the exact address mappings used by the runtime, without game assets.
add_executable(LoCriticalSectionTest ${CMAKE_SOURCE_DIR}/tools/tests/critical_section_test.cpp)
target_include_directories(LoCriticalSectionTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoCriticalSectionTest PRIVATE cxx_std_23)

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
        add_executable(LoDepthClearGpuTest ${CMAKE_SOURCE_DIR}/tools/tests/depth_clear_gpu_test.cpp)
        target_include_directories(LoDepthClearGpuTest PRIVATE ${LO_RUNTIME_ROOT} ${CMAKE_SOURCE_DIR}/thirdparty/plume)
        target_compile_features(LoDepthClearGpuTest PRIVATE cxx_std_20)
        target_compile_definitions(LoDepthClearGpuTest PRIVATE NOMINMAX)
        target_link_libraries(LoDepthClearGpuTest PRIVATE plume d3d12 dxgi)
        add_executable(LoStencilTest
            ${CMAKE_SOURCE_DIR}/tools/tests/stencil_test.cpp
            ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
        target_include_directories(LoStencilTest PRIVATE ${LO_RUNTIME_ROOT} ${CMAKE_SOURCE_DIR}/thirdparty/plume)
        target_compile_features(LoStencilTest PRIVATE cxx_std_20)
        target_compile_definitions(LoStencilTest PRIVATE NOMINMAX)
        target_link_libraries(LoStencilTest PRIVATE plume d3d12 dxgi)
        add_executable(LoPresentationTest
            ${CMAKE_SOURCE_DIR}/tools/tests/presentation_test.cpp
            ${LO_RUNTIME_ROOT}/gpu/presentation.cpp
            ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
        target_include_directories(LoPresentationTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,INCLUDE_DIRECTORIES>)
        target_compile_definitions(LoPresentationTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_DEFINITIONS>)
        target_compile_features(LoPresentationTest PRIVATE cxx_std_20)
        target_precompile_headers(LoPresentationTest REUSE_FROM LostOdysseyRecomp)
        target_link_libraries(LoPresentationTest PRIVATE plume d3d12 dxgi fmt::fmt)
        add_executable(LoPolygonOffsetTest
            ${CMAKE_SOURCE_DIR}/tools/tests/polygon_offset_test.cpp
            ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
        target_include_directories(LoPolygonOffsetTest PRIVATE ${LO_RUNTIME_ROOT} ${CMAKE_SOURCE_DIR}/thirdparty/plume)
        target_compile_features(LoPolygonOffsetTest PRIVATE cxx_std_20)
        target_compile_definitions(LoPolygonOffsetTest PRIVATE NOMINMAX)
        target_link_libraries(LoPolygonOffsetTest PRIVATE plume d3d12 dxgi)
    endif()
endif()

add_executable(LoDepthClearLayoutTest ${CMAKE_SOURCE_DIR}/tools/tests/depth_clear_layout_test.cpp)
target_include_directories(LoDepthClearLayoutTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoDepthClearLayoutTest PRIVATE cxx_std_20)
