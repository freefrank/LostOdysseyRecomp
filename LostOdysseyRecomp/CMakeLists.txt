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
    enable_language(RC)
    target_sources(LostOdysseyRecomp PRIVATE ${CMAKE_SOURCE_DIR}/assets/lost-odyssey-recomp.rc)
    target_include_directories(LostOdysseyRecomp PRIVATE ${CMAKE_SOURCE_DIR}/assets)
    # Keep the existing main/CRT initialization while avoiding a console on double-click.
    target_link_options(LostOdysseyRecomp PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    # Retain a build-local address map for module/RVA crash reports from
    # optimized packages without PDBs. The release payload excludes this file.
    target_link_options(LostOdysseyRecomp PRIVATE "/MAP:${CMAKE_CURRENT_BINARY_DIR}/LostOdysseyRecomp.map")
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
target_compile_definitions(LostOdysseyRecomp PRIVATE NOMINMAX LO_SOURCE_VERSION="${LO_SOURCE_VERSION}")

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

# Current-process log snapshots without guest generation or GPU access.
add_executable(LoLogCaptureTest EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tools/tests/log_capture_test.cpp
    ${LO_RUNTIME_ROOT}/os/log_file.cpp)
target_include_directories(LoLogCaptureTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoLogCaptureTest PRIVATE cxx_std_20)
target_link_libraries(LoLogCaptureTest PRIVATE fmt::fmt)

# Background archive publication and failure recovery, without guest/GPU work.
if(WIN32)
    add_executable(LoCaptureArchiveTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/capture_archive_test.cpp
        ${LO_RUNTIME_ROOT}/os/capture_archive.cpp)
    target_include_directories(LoCaptureArchiveTest PRIVATE ${LO_RUNTIME_ROOT})
    target_compile_features(LoCaptureArchiveTest PRIVATE cxx_std_20)
endif()

# Crash reporting is exercised in sacrificial child processes, without the
# guest library or a GPU. The fixture supplies inert guest-memory state.
if(WIN32)
    add_executable(LoCrashCaptureTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/crash_capture_test.cpp
        ${LO_RUNTIME_ROOT}/os/crash_handler.cpp
        ${LO_RUNTIME_ROOT}/os/log_file.cpp)
    target_include_directories(LoCrashCaptureTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,INCLUDE_DIRECTORIES>)
    target_compile_definitions(LoCrashCaptureTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_DEFINITIONS>)
    target_compile_options(LoCrashCaptureTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_OPTIONS>)
    target_compile_features(LoCrashCaptureTest PRIVATE cxx_std_20)
    target_link_libraries(LoCrashCaptureTest PRIVATE fmt::fmt xxHash::xxhash dbghelp)
endif()

# Integration test uses the actual guest import bridge and storage implementation.
set(LO_STORAGE_TEST_SOURCES ${LO_RUNTIME_SOURCES})
list(FILTER LO_STORAGE_TEST_SOURCES EXCLUDE REGEX "/main\\.cpp$")
add_executable(LoStorageTest EXCLUDE_FROM_ALL ${LO_STORAGE_TEST_SOURCES} ${LO_IMPORT_STUBS}
    ${CMAKE_SOURCE_DIR}/tools/tests/storage_test.cpp)
target_include_directories(LoStorageTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,INCLUDE_DIRECTORIES>)
target_compile_definitions(LoStorageTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_DEFINITIONS>)
target_compile_options(LoStorageTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_OPTIONS>)
target_compile_features(LoStorageTest PRIVATE cxx_std_20)
target_precompile_headers(LoStorageTest REUSE_FROM LostOdysseyRecomp)
target_link_libraries(LoStorageTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,LINK_LIBRARIES>)

# Actual generated function-table and script-handler regression for issue #5.
add_executable(LoGuestDispatchTest EXCLUDE_FROM_ALL ${LO_STORAGE_TEST_SOURCES} ${LO_IMPORT_STUBS}
    ${CMAKE_SOURCE_DIR}/tools/tests/guest_dispatch_test.cpp)
target_include_directories(LoGuestDispatchTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,INCLUDE_DIRECTORIES>)
target_compile_definitions(LoGuestDispatchTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_DEFINITIONS>)
target_compile_options(LoGuestDispatchTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_OPTIONS>)
target_compile_features(LoGuestDispatchTest PRIVATE cxx_std_20)
target_precompile_headers(LoGuestDispatchTest REUSE_FROM LostOdysseyRecomp)
target_link_libraries(LoGuestDispatchTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,LINK_LIBRARIES>)

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
add_executable(LoGuestPrintfTest EXCLUDE_FROM_ALL
    ${LO_RUNTIME_ROOT}/kernel/guest_printf.cpp
    ${CMAKE_SOURCE_DIR}/tools/tests/guest_printf_test.cpp)
target_include_directories(LoGuestPrintfTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,INCLUDE_DIRECTORIES>)
target_compile_definitions(LoGuestPrintfTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_DEFINITIONS>)
target_compile_options(LoGuestPrintfTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_OPTIONS>)
target_compile_features(LoGuestPrintfTest PRIVATE cxx_std_20)
target_precompile_headers(LoGuestPrintfTest REUSE_FROM LostOdysseyRecomp)
target_link_libraries(LoGuestPrintfTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,LINK_LIBRARIES>)

# Offline shader translator check (shares gpu/shader with the runtime).
add_executable(LoHidTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/hid_test.cpp ${LO_RUNTIME_ROOT}/hid/hid.cpp)
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
add_executable(LoXmaLoopTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/xma_loop_test.cpp)
target_include_directories(LoXmaLoopTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoXmaLoopTest PRIVATE cxx_std_20)

add_executable(LoPipelineCacheTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/pipeline_cache_test.cpp)
target_include_directories(LoPipelineCacheTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoPipelineCacheTest PRIVATE cxx_std_20)

add_executable(LoCpxDecodeTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/cpx_decode_test.cpp)
target_include_directories(LoCpxDecodeTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoCpxDecodeTest PRIVATE cxx_std_20)

add_executable(LoShaderResourceVariantsTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/shader_resource_variants_test.cpp)
target_include_directories(LoShaderResourceVariantsTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoShaderResourceVariantsTest PRIVATE cxx_std_20)

add_executable(LoTextureLayoutTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/texture_layout_test.cpp)
target_include_directories(LoTextureLayoutTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoTextureLayoutTest PRIVATE cxx_std_20)

# Tests the exact address mappings used by the runtime, without game assets.
add_executable(LoCriticalSectionTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/critical_section_test.cpp)
target_include_directories(LoCriticalSectionTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoCriticalSectionTest PRIVATE cxx_std_23)

add_executable(LoMemoryAliasTest EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tools/tests/memory_alias_test.cpp
    ${LO_RUNTIME_ROOT}/kernel/guest_address_space.cpp)
target_include_directories(LoMemoryAliasTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoMemoryAliasTest PRIVATE cxx_std_20)
if(WIN32)
    target_link_libraries(LoMemoryAliasTest PRIVATE onecore)
    # Includes the production allocator with native API fault replacements.
    add_executable(LoMemoryFailureTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/memory_failure_test.cpp)
    target_include_directories(LoMemoryFailureTest PRIVATE ${LO_RUNTIME_ROOT})
    target_compile_features(LoMemoryFailureTest PRIVATE cxx_std_20)
    target_compile_definitions(LoMemoryFailureTest PRIVATE NOMINMAX)

    # Translates and compiles HLSL with DXC, without a GPU or game assets.
    add_executable(LoRenderResolutionShaderTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/render_resolution_shader_test.cpp
        ${LO_RUNTIME_ROOT}/gpu/shader/xenos_translator.cpp
        ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
    target_include_directories(LoRenderResolutionShaderTest PRIVATE ${LO_RUNTIME_ROOT})
    target_compile_features(LoRenderResolutionShaderTest PRIVATE cxx_std_20)
    target_compile_definitions(LoRenderResolutionShaderTest PRIVATE NOMINMAX)
    target_link_libraries(LoRenderResolutionShaderTest PRIVATE fmt::fmt)

    add_executable(LoShaderAluTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/shader_alu_test.cpp
        ${LO_RUNTIME_ROOT}/gpu/shader/xenos_translator.cpp
        ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
    target_include_directories(LoShaderAluTest PRIVATE ${LO_RUNTIME_ROOT})
    target_compile_features(LoShaderAluTest PRIVATE cxx_std_20)
    target_compile_definitions(LoShaderAluTest PRIVATE NOMINMAX)
    target_link_libraries(LoShaderAluTest PRIVATE fmt::fmt d3d12 dxgi)
    if(LO_BUILD_GPU)
        add_executable(LoRenderResolutionGpuTest EXCLUDE_FROM_ALL
            ${CMAKE_SOURCE_DIR}/tools/tests/render_resolution_gpu_test.cpp
            ${LO_RUNTIME_ROOT}/gpu/shader/xenos_translator.cpp
            ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
        target_include_directories(LoRenderResolutionGpuTest PRIVATE ${LO_RUNTIME_ROOT} ${CMAKE_SOURCE_DIR}/thirdparty/plume)
        target_compile_features(LoRenderResolutionGpuTest PRIVATE cxx_std_20)
        target_compile_definitions(LoRenderResolutionGpuTest PRIVATE NOMINMAX)
        set_target_properties(LoRenderResolutionGpuTest PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
        target_link_libraries(LoRenderResolutionGpuTest PRIVATE plume d3d12 dxgi fmt::fmt)
        add_executable(LoResolveCopyGpuTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/resolve_copy_gpu_test.cpp)
        target_include_directories(LoResolveCopyGpuTest PRIVATE ${CMAKE_SOURCE_DIR}/thirdparty/plume)
        target_compile_features(LoResolveCopyGpuTest PRIVATE cxx_std_20)
        target_compile_definitions(LoResolveCopyGpuTest PRIVATE NOMINMAX)
        target_link_libraries(LoResolveCopyGpuTest PRIVATE plume d3d12 dxgi)
        add_executable(LoDepthClearGpuTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/depth_clear_gpu_test.cpp)
        target_include_directories(LoDepthClearGpuTest PRIVATE ${LO_RUNTIME_ROOT} ${CMAKE_SOURCE_DIR}/thirdparty/plume)
        target_compile_features(LoDepthClearGpuTest PRIVATE cxx_std_20)
        target_compile_definitions(LoDepthClearGpuTest PRIVATE NOMINMAX)
        target_link_libraries(LoDepthClearGpuTest PRIVATE plume d3d12 dxgi)
        add_executable(LoStencilTest EXCLUDE_FROM_ALL
            ${CMAKE_SOURCE_DIR}/tools/tests/stencil_test.cpp
            ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
        target_include_directories(LoStencilTest PRIVATE ${LO_RUNTIME_ROOT} ${CMAKE_SOURCE_DIR}/thirdparty/plume)
        target_compile_features(LoStencilTest PRIVATE cxx_std_20)
        target_compile_definitions(LoStencilTest PRIVATE NOMINMAX)
        target_link_libraries(LoStencilTest PRIVATE plume d3d12 dxgi)
        add_executable(LoPresentationTest EXCLUDE_FROM_ALL
            ${CMAKE_SOURCE_DIR}/tools/tests/presentation_test.cpp
            ${LO_RUNTIME_ROOT}/gpu/presentation.cpp
            ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
        target_include_directories(LoPresentationTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,INCLUDE_DIRECTORIES>)
        target_compile_definitions(LoPresentationTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,COMPILE_DEFINITIONS>)
        target_compile_features(LoPresentationTest PRIVATE cxx_std_20)
        # Keep this focused GPU fixture independent of rebuilding the game.
        set_target_properties(LoPresentationTest PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
        target_link_libraries(LoPresentationTest PRIVATE plume d3d12 dxgi fmt::fmt)
        add_executable(LoPolygonOffsetTest EXCLUDE_FROM_ALL
            ${CMAKE_SOURCE_DIR}/tools/tests/polygon_offset_test.cpp
            ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
        target_include_directories(LoPolygonOffsetTest PRIVATE ${LO_RUNTIME_ROOT} ${CMAKE_SOURCE_DIR}/thirdparty/plume)
        target_compile_features(LoPolygonOffsetTest PRIVATE cxx_std_20)
        target_compile_definitions(LoPolygonOffsetTest PRIVATE NOMINMAX)
        target_link_libraries(LoPolygonOffsetTest PRIVATE plume d3d12 dxgi)
        add_executable(LoTemporalAATest EXCLUDE_FROM_ALL
            ${CMAKE_SOURCE_DIR}/tools/tests/temporal_aa_test.cpp
            ${LO_RUNTIME_ROOT}/gpu/temporal_aa.cpp
            ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
        target_include_directories(LoTemporalAATest PRIVATE ${LO_RUNTIME_ROOT} ${CMAKE_SOURCE_DIR}/thirdparty/plume)
        target_compile_features(LoTemporalAATest PRIVATE cxx_std_20)
        target_compile_definitions(LoTemporalAATest PRIVATE LO_GPU_PLUME NOMINMAX)
        target_link_libraries(LoTemporalAATest PRIVATE plume d3d12 dxgi)
    endif()
endif()

add_executable(LoDepthClearLayoutTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/depth_clear_layout_test.cpp)
target_include_directories(LoDepthClearLayoutTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoDepthClearLayoutTest PRIVATE cxx_std_20)

# Host-only fixtures do not require guest generation or the runtime PCH.
add_executable(LoRenderResolutionTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/render_resolution_test.cpp)
target_include_directories(LoRenderResolutionTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoRenderResolutionTest PRIVATE cxx_std_20)

add_executable(LoTemporalSceneTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/temporal_scene_test.cpp)
target_include_directories(LoTemporalSceneTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoTemporalSceneTest PRIVATE cxx_std_20)

add_executable(LoTemporalJitterTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/temporal_jitter_test.cpp)
target_include_directories(LoTemporalJitterTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoTemporalJitterTest PRIVATE cxx_std_20)

add_executable(LoTemporalMathTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/temporal_math_test.cpp)
target_include_directories(LoTemporalMathTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoTemporalMathTest PRIVATE cxx_std_20)

add_executable(LoTemporalHistoryDiagnosticTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/temporal_history_diagnostic_test.cpp)
target_include_directories(LoTemporalHistoryDiagnosticTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoTemporalHistoryDiagnosticTest PRIVATE cxx_std_20)

add_executable(LoFramePacerTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/frame_pacer_test.cpp)
target_include_directories(LoFramePacerTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoFramePacerTest PRIVATE cxx_std_20)
if(WIN32)
    add_executable(LoMenuRenderTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/menu_render_test.cpp
        ${LO_RUNTIME_ROOT}/settings/menu_render.cpp)
    target_include_directories(LoMenuRenderTest PRIVATE ${LO_RUNTIME_ROOT})
    target_compile_features(LoMenuRenderTest PRIVATE cxx_std_20)
    target_compile_options(LoMenuRenderTest PRIVATE /utf-8)
    target_compile_definitions(LoMenuRenderTest PRIVATE NOMINMAX)
    target_link_libraries(LoMenuRenderTest PRIVATE user32 gdi32)
endif()

add_executable(LoGamePathTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/game_path_test.cpp)
target_include_directories(LoGamePathTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoGamePathTest PRIVATE cxx_std_20)
