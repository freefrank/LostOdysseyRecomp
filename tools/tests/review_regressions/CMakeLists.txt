cmake_minimum_required(VERSION 3.20)
project(LoReviewRegressions LANGUAGES C CXX)

enable_testing()
find_package(Threads REQUIRED)
find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
get_filename_component(LO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(LO_RUNTIME "${LO_ROOT}/LostOdysseyRecomp")
set(LO_TESTS "${LO_ROOT}/tools/tests")
option(LO_REVIEW_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" ON)
option(LO_REVIEW_SDL_TESTS "Build both SDL updater implementations" ON)
set(LO_REVIEW_SDL_LIBRARY "" CACHE FILEPATH "Optional existing SDL3 library (uses pinned headers)")

function(lo_review_test name source)
    add_executable(${name} "${LO_TESTS}/${source}" ${ARGN})
    target_include_directories(${name} PRIVATE "${LO_RUNTIME}")
    target_compile_features(${name} PRIVATE cxx_std_20)
    target_link_libraries(${name} PRIVATE Threads::Threads)
    if(MSVC)
        target_compile_options(${name} PRIVATE /utf-8 /UNDEBUG)
    else()
        target_compile_options(${name} PRIVATE -UNDEBUG)
        if(LO_REVIEW_SANITIZE)
            target_compile_options(${name} PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
            target_link_options(${name} PRIVATE -fsanitize=address,undefined)
        endif()
    endif()
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT 30)
endfunction()

lo_review_test(LoTextureKeyTest texture_key_test.cpp)
lo_review_test(LoTextureContentTest texture_content_test.cpp)
target_compile_definitions(LoTextureContentTest PRIVATE XXH_INLINE_ALL)
target_include_directories(LoTextureContentTest PRIVATE "${LO_ROOT}/tools/XenonRecomp/thirdparty/xxHash")
lo_review_test(LoTextureLayoutTest texture_layout_test.cpp)
lo_review_test(LoDriverPipelineCacheTest driver_pipeline_cache_test.cpp)
lo_review_test(LoBcDecodeTest bc_decode_test.cpp)
lo_review_test(LoAudioCallbackTest audio_callback_test.cpp)
lo_review_test(LoSettingsPathTest settings_path_test.cpp "${LO_RUNTIME}/updater/update.cpp")
lo_review_test(LoUserPathsTest user_paths_test.cpp)
lo_review_test(LoRuntimeLibrariesTest runtime_libraries_test.cpp)
lo_review_test(LoExternalUpdateNoticeTest external_update_notice_test.cpp)
lo_review_test(LoOcclusionQueriesTest occlusion_queries_test.cpp)
# TAA/FSR/DLSS jitter map: every reviewed flicker fix has a case here.
lo_review_test(LoTemporalJitterTest temporal_jitter_test.cpp)
# One-file local shader store: keys, recovery, integrity, locking, compaction.
lo_review_test(LoShaderStoreTest shader_store_test.cpp)
lo_review_test(LoShadowResolutionTest shadow_resolution_test.cpp)
# Startup shader-pack download: index, selection, asset URLs, decline record.
lo_review_test(LoShaderPackIndexTest shader_pack_index_test.cpp "${LO_RUNTIME}/updater/shader_pack_index.cpp")
# Offline capture, suspect-log and triage tools (standard-library Python).
add_test(NAME LoJitterCaptureTools COMMAND "${Python3_EXECUTABLE}" -B "${LO_TESTS}/jitter_capture_tools_test.py")
set_tests_properties(LoJitterCaptureTools PROPERTIES TIMEOUT 60)
add_test(NAME LoPsClipReads COMMAND "${Python3_EXECUTABLE}" -B -m unittest tools.tests.test_ps_clip_reads
    WORKING_DIRECTORY "${LO_ROOT}")
set_tests_properties(LoPsClipReads PROPERTIES TIMEOUT 60)
# Asset inventory and FMV export tools on synthetic fixtures (standard-library Python).
foreach(name asset_inventory_test fmv_export_test)
    add_test(NAME ${name} COMMAND "${Python3_EXECUTABLE}" -B -m unittest tools.tests.${name}
        WORKING_DIRECTORY "${LO_ROOT}")
    set_tests_properties(${name} PROPERTIES TIMEOUT 120)
endforeach()

if(LO_REVIEW_SDL_TESTS)
    if(LO_REVIEW_SDL_LIBRARY)
        add_library(lo_review_sdl UNKNOWN IMPORTED)
        set_target_properties(lo_review_sdl PROPERTIES
            IMPORTED_LOCATION "${LO_REVIEW_SDL_LIBRARY}"
            INTERFACE_INCLUDE_DIRECTORIES "${LO_ROOT}/thirdparty/SDL/include")
        if(WIN32)
            target_link_libraries(lo_review_sdl INTERFACE winmm setupapi cfgmgr32 version imm32)
        endif()
    else()
        find_package(SDL3 CONFIG QUIET)
        if(SDL3_FOUND)
            add_library(lo_review_sdl ALIAS SDL3::SDL3)
        else()
            # Ubuntu 22.04/24.04 ship no SDL3 package: build the pinned submodule.
            set(SDL_SHARED OFF CACHE BOOL "" FORCE)
            set(SDL_STATIC ON CACHE BOOL "" FORCE)
            set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
            set(SDL_TESTS OFF CACHE BOOL "" FORCE)
            # The fixtures use the dummy video driver. SDL3 stops configuring
            # when only some X11 extension headers are installed.
            set(SDL_X11 OFF CACHE BOOL "" FORCE)
            set(SDL_WAYLAND OFF CACHE BOOL "" FORCE)
            set(SDL_UNIX_CONSOLE_BUILD ON CACHE BOOL "" FORCE)
            add_subdirectory("${LO_ROOT}/thirdparty/SDL" sdl EXCLUDE_FROM_ALL)
            add_library(lo_review_sdl ALIAS SDL3::SDL3-static)
        endif()
    endif()
    lo_review_test(LoUpdaterSdlUiTest updater_sdl_ui_test.cpp
        "${LO_RUNTIME}/install/installer_font.cpp"
        "${LO_RUNTIME}/updater/posix_ui.cpp"
        "${LO_RUNTIME}/updater/update.cpp")
    target_link_libraries(LoUpdaterSdlUiTest PRIVATE lo_review_sdl)
    if(NOT WIN32)
        lo_review_test(LoUpdaterSdlProductionTest updater_sdl_ui_test.cpp
            "${LO_RUNTIME}/install/installer_font.cpp"
            "${LO_RUNTIME}/updater/progress.cpp"
            "${LO_RUNTIME}/updater/update.cpp")
        target_link_libraries(LoUpdaterSdlProductionTest PRIVATE lo_review_sdl)
    endif()
endif()

# These fixtures compile extracted production bodies with host/guest stubs.
# Their POSIX error injection and compiler flags require a Unix GCC/Clang host.
if(UNIX)
    function(lo_review_python name script output_flag compiler_flag)
        add_test(NAME ${name} COMMAND "${Python3_EXECUTABLE}" -B
            "${CMAKE_CURRENT_SOURCE_DIR}/run_fixture.py"
            --work-dir "${CMAKE_CURRENT_BINARY_DIR}/fixtures"
            "${LO_TESTS}/${script}" ${output_flag} "{out}"
            ${compiler_flag} "${CMAKE_CXX_COMPILER}" ${ARGN})
        set_tests_properties(${name} PROPERTIES TIMEOUT 150)
    endfunction()
    set(sanitizer_flags)
    if(LO_REVIEW_SANITIZE)
        set(sanitizer_flags --sanitize)
    endif()
    lo_review_python(file_io file_io_regression.py --out --cxx ${sanitizer_flags})
    # The allocator fixture always uses ASan/UBSan.
    lo_review_python(page_allocator page_allocator_test.py --output --compiler)
    lo_review_python(gpu_commands gpu_command_safety_regression.py --out --cxx ${sanitizer_flags})
    lo_review_python(guest_main_exit guest_main_exit_regression.py --out --cxx)
    lo_review_python(kernel_lifetime kernel_lifetime_regression.py --out --cxx ${sanitizer_flags})
endif()

# Builds the unmodified updater apply mode and checks AppImage backup cleanup,
# a successful restart and the rollback after a failed exec. Linux only: the
# apply mode reads /proc/self/cmdline.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(NAME updater_appimage_cleanup COMMAND "${Python3_EXECUTABLE}" -B
        "${LO_TESTS}/updater_appimage_cleanup_test.py" --cxx "${CMAKE_CXX_COMPILER}")
    set_tests_properties(updater_appimage_cleanup PROPERTIES TIMEOUT 150)
endif()

# AppImage and Flatpak packaging scripts, with linuxdeploy, flatpak and the
# compiler, readelf and ldd checks replaced by stand-ins.
foreach(name package_appimage package_flatpak)
    add_test(NAME ${name} COMMAND "${Python3_EXECUTABLE}" -B "${LO_TESTS}/${name}_test.py")
    set_tests_properties(${name} PROPERTIES TIMEOUT 60)
endforeach()
