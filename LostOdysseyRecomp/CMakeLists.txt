project(LostOdysseyRecomp LANGUAGES CXX)

set(LO_RUNTIME_ROOT ${CMAKE_CURRENT_SOURCE_DIR})
add_library(lo_menu_lzo STATIC EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/thirdparty/lzokay/lzokay.cpp)
target_include_directories(lo_menu_lzo PUBLIC ${CMAKE_SOURCE_DIR}/thirdparty/lzokay)
target_compile_features(lo_menu_lzo PRIVATE cxx_std_17)
add_executable(LoPollWaitTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/poll_wait_test.cpp)
target_include_directories(LoPollWaitTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoPollWaitTest PRIVATE cxx_std_20)
target_compile_definitions(LoPollWaitTest PRIVATE NOMINMAX)
if(MSVC)
    target_compile_options(LoPollWaitTest PRIVATE /UNDEBUG)
else()
    target_compile_options(LoPollWaitTest PRIVATE -UNDEBUG)
endif()
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
# Publish source identity only after the corresponding runtime links successfully.
file(GENERATE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/source-version.build.txt" CONTENT "${LO_SOURCE_VERSION}\n")
add_custom_command(TARGET LostOdysseyRecomp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_BINARY_DIR}/source-version.build.txt"
        "$<TARGET_FILE_DIR:LostOdysseyRecomp>/source-version.txt" VERBATIM)
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
    lo_menu_lzo
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
    target_link_libraries(LostOdysseyRecomp PRIVATE winmm onecore bcrypt winhttp ole32 shell32)

    set(LO_UPDATER_TRANSACTION_SOURCES
        ${LO_RUNTIME_ROOT}/updater/update.cpp
        ${LO_RUNTIME_ROOT}/updater/sha256.cpp
        ${LO_RUNTIME_ROOT}/updater/stage.cpp
        ${LO_RUNTIME_ROOT}/updater/apply.cpp)
    add_executable(LostOdysseyUpdater WIN32
        ${CMAKE_SOURCE_DIR}/tools/updater/helper_main.cpp
        ${CMAKE_SOURCE_DIR}/assets/lost-odyssey-recomp.rc
        ${LO_UPDATER_TRANSACTION_SOURCES})
    target_include_directories(LostOdysseyUpdater PRIVATE ${LO_RUNTIME_ROOT} ${CMAKE_SOURCE_DIR}/assets)
    target_compile_features(LostOdysseyUpdater PRIVATE cxx_std_20)
    target_compile_definitions(LostOdysseyUpdater PRIVATE NOMINMAX)
    target_compile_definitions(LostOdysseyUpdater PRIVATE LO_SOURCE_VERSION="${LO_SOURCE_VERSION}")
    target_link_libraries(LostOdysseyUpdater PRIVATE bcrypt user32 shell32)
    # Both targets use this directory's runtime output path. Packaging takes
    # the helper beside the game, and a runtime build must produce both.
    add_dependencies(LostOdysseyRecomp LostOdysseyUpdater)
    # Capture once before either target compiles; publish per-binary stamps only
    # after successful links. An old EXE cannot inherit packaging-time HEAD.
    set(LO_BUILD_SOURCE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/build-source-current.json")
    add_custom_target(LoBuildSourceStamp
        COMMAND python "${CMAKE_SOURCE_DIR}/tools/build_provenance.py" capture
            --root "${CMAKE_SOURCE_DIR}" --source "${LO_BUILD_SOURCE_STAMP}" --version "${LO_SOURCE_VERSION}"
        VERBATIM)
    foreach(target LostOdysseyRecomp LostOdysseyUpdater)
        add_dependencies(${target} LoBuildSourceStamp)
        set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${CMAKE_SOURCE_DIR}/tools/build_provenance.py")
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND python "${CMAKE_SOURCE_DIR}/tools/build_provenance.py" linked
                --root "${CMAKE_SOURCE_DIR}" --source "${LO_BUILD_SOURCE_STAMP}" --version "${LO_SOURCE_VERSION}"
                --binary "$<TARGET_FILE:${target}>" VERBATIM)
    endforeach()

    add_executable(LoUpdaterProbe WIN32 EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/updater_probe.cpp)
    target_compile_features(LoUpdaterProbe PRIVATE cxx_std_20)
    target_link_libraries(LoUpdaterProbe PRIVATE shell32)

    add_executable(LoUpdaterTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/updater_test.cpp
        ${LO_UPDATER_TRANSACTION_SOURCES}
        ${LO_RUNTIME_ROOT}/updater/windows_http.cpp
        ${LO_RUNTIME_ROOT}/updater/progress.cpp)
    target_include_directories(LoUpdaterTest PRIVATE ${LO_RUNTIME_ROOT})
    target_compile_features(LoUpdaterTest PRIVATE cxx_std_20)
    target_compile_definitions(LoUpdaterTest PRIVATE NOMINMAX)
    target_link_libraries(LoUpdaterTest PRIVATE bcrypt winhttp user32 gdi32 shell32)
    add_dependencies(LoUpdaterTest LostOdysseyUpdater LoUpdaterProbe)

    add_executable(LoUpdaterProgressTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/updater_progress_test.cpp
        ${LO_RUNTIME_ROOT}/updater/progress.cpp)
    target_include_directories(LoUpdaterProgressTest PRIVATE ${LO_RUNTIME_ROOT})
    target_compile_features(LoUpdaterProgressTest PRIVATE cxx_std_20)
    target_compile_definitions(LoUpdaterProgressTest PRIVATE NOMINMAX)
    target_link_libraries(LoUpdaterProgressTest PRIVATE user32 gdi32)

    add_executable(LoUpdaterHelperContextTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/updater_helper_context_test.cpp
        ${LO_RUNTIME_ROOT}/updater/update.cpp
        ${LO_RUNTIME_ROOT}/updater/sha256.cpp)
    target_include_directories(LoUpdaterHelperContextTest PRIVATE ${LO_RUNTIME_ROOT})
    target_compile_features(LoUpdaterHelperContextTest PRIVATE cxx_std_20)
    target_compile_definitions(LoUpdaterHelperContextTest PRIVATE NOMINMAX)
    target_link_libraries(LoUpdaterHelperContextTest PRIVATE bcrypt user32 shell32)
    add_dependencies(LoUpdaterHelperContextTest LostOdysseyUpdater LoUpdaterProbe)
endif()

# Host-only restart argument and parent/child lifetime checks.
if(WIN32)
    add_executable(LoFolderPickerTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/folder_picker_test.cpp)
    target_include_directories(LoFolderPickerTest PRIVATE ${LO_RUNTIME_ROOT})
    target_compile_features(LoFolderPickerTest PRIVATE cxx_std_20)
    target_compile_definitions(LoFolderPickerTest PRIVATE NOMINMAX)
    target_compile_options(LoFolderPickerTest PRIVATE /utf-8 /UNDEBUG)
    target_link_libraries(LoFolderPickerTest PRIVATE user32 ole32 shell32)

    add_executable(LoDebugMenuInteractionTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/debug_menu_interaction_test.cpp)
    target_include_directories(LoDebugMenuInteractionTest PRIVATE
        ${CMAKE_SOURCE_DIR}/tools/tests/debug_menu_fixture ${LO_RUNTIME_ROOT})
    target_compile_features(LoDebugMenuInteractionTest PRIVATE cxx_std_20)
    target_compile_options(LoDebugMenuInteractionTest PRIVATE /utf-8 /UNDEBUG)
    target_link_libraries(LoDebugMenuInteractionTest PRIVATE user32 gdi32 dwmapi)

    add_executable(LoGameWindowPixelsTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/game_window_pixels_test.cpp)
    target_compile_features(LoGameWindowPixelsTest PRIVATE cxx_std_20)
    target_compile_options(LoGameWindowPixelsTest PRIVATE /utf-8 /W4 /WX)
    target_link_libraries(LoGameWindowPixelsTest PRIVATE SDL2::SDL2-static user32)
    set_target_properties(LoGameWindowPixelsTest PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)

    add_executable(LoRestartTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/restart_test.cpp)
    target_include_directories(LoRestartTest PRIVATE ${LO_RUNTIME_ROOT})
    target_compile_features(LoRestartTest PRIVATE cxx_std_20)
    target_compile_definitions(LoRestartTest PRIVATE NOMINMAX)
    target_link_libraries(LoRestartTest PRIVATE user32 shell32)
    set_target_properties(LoRestartTest PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
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

add_executable(LoGamePathTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/game_path_test.cpp)
target_include_directories(LoGamePathTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoGamePathTest PRIVATE cxx_std_20)

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

add_executable(LoShaderPreparationQueueTest EXCLUDE_FROM_ALL
    ${CMAKE_SOURCE_DIR}/tools/tests/shader_preparation_queue_test.cpp)
target_include_directories(LoShaderPreparationQueueTest PRIVATE ${LO_RUNTIME_ROOT})
target_compile_features(LoShaderPreparationQueueTest PRIVATE cxx_std_20)
if(WIN32)
    add_executable(LoMenuRenderTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/menu_render_test.cpp
        ${LO_RUNTIME_ROOT}/settings/menu_assets.cpp
        ${LO_RUNTIME_ROOT}/settings/menu_render.cpp)
    target_include_directories(LoMenuRenderTest PRIVATE ${LO_RUNTIME_ROOT})
    target_compile_features(LoMenuRenderTest PRIVATE cxx_std_20)
    target_compile_options(LoMenuRenderTest PRIVATE /utf-8)
    target_compile_definitions(LoMenuRenderTest PRIVATE NOMINMAX LO_MENU_RENDER_TRACE)
    target_link_libraries(LoMenuRenderTest PRIVATE user32 gdi32 lo_menu_lzo)
endif()

if(WIN32 AND LO_BUILD_GPU)
    add_executable(LoBackendSelectionTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/backend_selection_test.cpp)
    add_executable(LoBackendDeviceTest EXCLUDE_FROM_ALL ${CMAKE_SOURCE_DIR}/tools/tests/backend_device_test.cpp)
    add_executable(LoBackendCacheTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/backend_cache_test.cpp ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
    add_executable(LoBackendLifecycleTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/backend_lifecycle_test.cpp
        ${LO_RUNTIME_ROOT}/gpu/presentation.cpp ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
    target_compile_definitions(LoBackendLifecycleTest PRIVATE LO_GPU_PLUME SDL_MAIN_HANDLED)
    target_link_options(LoBackendLifecycleTest PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
    target_link_libraries(LoBackendLifecycleTest PRIVATE plume d3d12 dxgi fmt::fmt SDL2::SDL2-static xxHash::xxhash)
    foreach(target LoBackendSelectionTest LoBackendDeviceTest LoBackendCacheTest LoBackendLifecycleTest)
        target_include_directories(${target} PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,INCLUDE_DIRECTORIES>)
        target_compile_definitions(${target} PRIVATE NOMINMAX)
        target_compile_options(${target} PRIVATE /UNDEBUG /utf-8)
        target_compile_features(${target} PRIVATE cxx_std_20)
        set_target_properties(${target} PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
    endforeach()
    target_link_libraries(LoBackendDeviceTest PRIVATE plume d3d12 dxgi)
    add_executable(LoShaderStartupCacheTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/shader_startup_cache_test.cpp
        ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp)
    target_include_directories(LoShaderStartupCacheTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,INCLUDE_DIRECTORIES>)
    target_compile_definitions(LoShaderStartupCacheTest PRIVATE NOMINMAX)
    target_compile_options(LoShaderStartupCacheTest PRIVATE /UNDEBUG /utf-8)
    target_compile_features(LoShaderStartupCacheTest PRIVATE cxx_std_20)
    set_target_properties(LoShaderStartupCacheTest PROPERTIES DISABLE_PRECOMPILE_HEADERS ON)
    add_executable(LoVulkanBackendTest EXCLUDE_FROM_ALL
        ${CMAKE_SOURCE_DIR}/tools/tests/vulkan_backend_test.cpp
        ${LO_RUNTIME_ROOT}/gpu/shader/dxc_compiler.cpp
        ${LO_RUNTIME_ROOT}/gpu/shader/xenos_translator.cpp
        ${LO_RUNTIME_ROOT}/gpu/presentation.cpp)
    target_include_directories(LoVulkanBackendTest PRIVATE $<TARGET_PROPERTY:LostOdysseyRecomp,INCLUDE_DIRECTORIES>)
    target_compile_definitions(LoVulkanBackendTest PRIVATE LO_GPU_PLUME NOMINMAX)
    target_compile_features(LoVulkanBackendTest PRIVATE cxx_std_20)
    target_link_libraries(LoVulkanBackendTest PRIVATE plume d3d12 dxgi fmt::fmt)
endif()

# Package the same SPIR-V-capable DXC beside runtime and compiler/GPU fixtures.
# The Windows SDK compiler is allowed as an explicit fallback, but is not the release contract.
if(WIN32 AND LO_BUILD_GPU)
    set(LO_DXC_BINARY_DIR "${CMAKE_SOURCE_DIR}/tools/XenosRecomp/thirdparty/dxc-bin/bin/x64")
    foreach(target LostOdysseyRecomp LoShaderTool LoPresentationTest LoTemporalAATest LoRenderResolutionShaderTest LoVulkanBackendTest LoShaderStartupCacheTest LoBackendCacheTest LoBackendLifecycleTest)
        if(TARGET ${target})
            foreach(binary dxcompiler.dll dxil.dll)
                add_custom_command(TARGET ${target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${LO_DXC_BINARY_DIR}/${binary}" "$<TARGET_FILE_DIR:${target}>/${binary}" VERBATIM)
            endforeach()
        endif()
    endforeach()
endif()
