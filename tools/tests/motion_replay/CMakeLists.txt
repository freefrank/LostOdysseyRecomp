cmake_minimum_required(VERSION 3.24)
project(lo_motion_replay_tests LANGUAGES CXX C)
set(CMAKE_CXX_STANDARD 20)
if(MSVC)
 set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(PLUME_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(PLUME_SDL_VULKAN_ENABLED OFF CACHE BOOL "" FORCE)
add_subdirectory("${ROOT}/thirdparty/plume" plume)
add_subdirectory("${ROOT}/tools/XenosRecomp/thirdparty/fmt" fmt EXCLUDE_FROM_ALL)
add_library(mv_runtime STATIC
 "${ROOT}/LostOdysseyRecomp/gpu/shader/xenos_translator.cpp"
 "${ROOT}/LostOdysseyRecomp/gpu/shader/dxc_compiler.cpp"
 "${ROOT}/LostOdysseyRecomp/gpu/temporal_aa.cpp")
target_include_directories(mv_runtime PUBLIC "${ROOT}/LostOdysseyRecomp"
 "${ROOT}/tools/XenosRecomp/thirdparty/dxc-bin/inc"
 "${ROOT}/tools/XenosRecomp/XenosRecomp")
target_compile_definitions(mv_runtime PUBLIC LO_GPU_PLUME NOMINMAX)
target_link_libraries(mv_runtime PUBLIC plume fmt::fmt ${CMAKE_DL_LIBS})
if(WIN32)
 target_link_libraries(mv_runtime PUBLIC d3d12 dxgi)
endif()
add_executable(motion_replay_gpu_test "${ROOT}/tools/tests/motion_replay_gpu_test.cpp" platform_stub.cpp)
target_link_libraries(motion_replay_gpu_test PRIVATE mv_runtime)
add_executable(battle_motion_replay_gpu_test "${ROOT}/tools/tests/battle_motion_replay_gpu_test.cpp" platform_stub.cpp)
target_link_libraries(battle_motion_replay_gpu_test PRIVATE mv_runtime)
set(LO_BATTLE_MOTION_SOURCE_ROOT "${ROOT}/out" CACHE PATH "Ignored local battle microcode evidence root")
set(_battle_feedback "${LO_BATTLE_MOTION_SOURCE_ROOT}/streamline-fg-p0/fsr-p2-player-feedback-audit/programs")
set(_battle_local "${LO_BATTLE_MOTION_SOURCE_ROOT}/streamline-fg-p0/fsr-p2-battle-diagnostic-01/manual-hypocenter-01/shader-cache/source")
set(_battle_sources
 "${_battle_feedback}/vs/a6f8edc127ae75374e1ac1fd209ef5e5827f5cfc1fe046e8293f4b5473e9208b.bin"
 "${_battle_feedback}/vs/bf47df67bbfd857e55ea65815d2f55ef09356818ab8a52fd6d79d6de83220de0.bin"
 "${_battle_feedback}/vs/b25fdc9a20ac636263493cdc229dc81291b8b4d2ff95dbba8d1995605d617522.bin"
 "${_battle_feedback}/ps/e3df99cc5e52b1bec72bf1575e3d396ce0a643cba89989ce2d35b1d799c6d67d.bin"
 "${_battle_local}/ps_8ead384aedf2bb23.bin" "${_battle_local}/ps_a3826242c3338c5f.bin")
set(_battle_sources_ready TRUE)
foreach(_source IN LISTS _battle_sources)
 if(NOT EXISTS "${_source}")
  set(_battle_sources_ready FALSE)
 endif()
endforeach()
if(_battle_sources_ready)
 add_test(NAME battle_motion_replay_gpu COMMAND battle_motion_replay_gpu_test "${LO_BATTLE_MOTION_SOURCE_ROOT}")
 set_tests_properties(battle_motion_replay_gpu PROPERTIES LABELS "battle;gpu;local-microcode" TIMEOUT 180)
endif()
add_executable(motion_vector_test "${ROOT}/tools/tests/motion_vector_test.cpp")
target_include_directories(motion_vector_test PRIVATE "${ROOT}/LostOdysseyRecomp")
add_executable(temporal_frame_inputs_test "${ROOT}/tools/tests/temporal_frame_inputs_test.cpp")
target_include_directories(temporal_frame_inputs_test PRIVATE "${ROOT}/LostOdysseyRecomp")
add_executable(native_dlss_p2_routing_test "${ROOT}/tools/tests/native_dlss_p2_routing_test.cpp")
target_include_directories(native_dlss_p2_routing_test PRIVATE "${ROOT}/LostOdysseyRecomp")
if(WIN32)
 add_executable(sr_failure_propagation_test "${ROOT}/tools/tests/sr_failure_propagation_test.cpp")
 target_link_libraries(sr_failure_propagation_test PRIVATE mv_runtime)
endif()
enable_testing()
add_test(NAME motion_cpu COMMAND motion_vector_test)
add_test(NAME temporal_frame_inputs COMMAND temporal_frame_inputs_test)
add_test(NAME native_dlss_p2_routing COMMAND native_dlss_p2_routing_test)
if(WIN32)
 add_test(NAME sr_failure_propagation COMMAND sr_failure_propagation_test)
endif()
add_test(NAME motion_gpu COMMAND motion_replay_gpu_test)
if(UNIX AND NOT APPLE)
 find_package(X11 REQUIRED)
 target_link_libraries(mv_runtime PUBLIC X11::X11)
endif()
add_executable(temporal_aa_regression "${ROOT}/tools/tests/temporal_aa_test.cpp" platform_stub.cpp)
target_link_libraries(temporal_aa_regression PRIVATE mv_runtime)
add_test(NAME temporal_aa_legacy COMMAND temporal_aa_regression --vulkan)
# Compile the actual renderer translation unit against actual graphics interfaces.
# Only unused generated guest-ABI names in its PCH are replaced; no game linking
# or execution is claimed by this boundary check.
add_library(motion_renderer_compile OBJECT "${ROOT}/LostOdysseyRecomp/gpu/renderer.cpp")
target_include_directories(motion_renderer_compile BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/compile_boundary")
target_include_directories(motion_renderer_compile PRIVATE "${ROOT}/thirdparty/unordered_dense/include")
target_compile_definitions(motion_renderer_compile PRIVATE LO_SOURCE_VERSION="mv-fixture")
target_link_libraries(motion_renderer_compile PRIVATE mv_runtime)
if(NOT MSVC)
 target_compile_options(motion_renderer_compile PRIVATE -O0 -g0)
endif()

# Shared production promotion shaders, executed without guest code or NGX.
add_executable(LoSceneCopyCompositeTest "${ROOT}/tools/tests/native_dlss_composite_gpu_test.cpp" platform_stub.cpp)
target_link_libraries(LoSceneCopyCompositeTest PRIVATE mv_runtime)
add_test(NAME LoSceneCopyCompositeTest COMMAND LoSceneCopyCompositeTest)
set_tests_properties(LoSceneCopyCompositeTest PROPERTIES LABELS "native-dlss;gpu-composite" TIMEOUT 120)

option(LO_P2_VIDEO_COMPILE_CHECK "Compile actual video.cpp in the asset-free boundary" OFF)
if(LO_P2_VIDEO_COMPILE_CHECK)
 find_package(SDL2 REQUIRED)
 add_library(p2_video_compile OBJECT "${ROOT}/LostOdysseyRecomp/gpu/video.cpp")
 target_include_directories(p2_video_compile BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/compile_boundary")
 target_include_directories(p2_video_compile PRIVATE "${ROOT}/thirdparty/unordered_dense/include")
 target_compile_definitions(p2_video_compile PRIVATE LO_SOURCE_VERSION="p2-fixture"
  $<$<PLATFORM_ID:Linux>:PLUME_SDL_VULKAN_ENABLED=1>)
 target_link_libraries(p2_video_compile PRIVATE mv_runtime SDL2::SDL2)
endif()

# Full production target-map/command route with only NGX and the platform
# boundary substituted. No guest executable or private assets are linked.
if((UNIX AND NOT APPLE) OR WIN32)
 include("${ROOT}/cmake/LoDlss.cmake")
 include("${ROOT}/cmake/LoFsr.cmake")
 add_executable(LoNativeDlssRendererTest "${ROOT}/tools/tests/native_dlss_renderer_gpu_test.cpp"
  "${ROOT}/LostOdysseyRecomp/gpu/dlss_ngx.cpp"
  "${ROOT}/LostOdysseyRecomp/gpu/fsr_upscaler.cpp"
  "${ROOT}/LostOdysseyRecomp/gpu/presentation.cpp"
  "${ROOT}/LostOdysseyRecomp/gpu/shader/portable_shader_pack.cpp" platform_stub.cpp)
 target_include_directories(LoNativeDlssRendererTest BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/compile_boundary")
  target_include_directories(LoNativeDlssRendererTest PRIVATE "${ROOT}/thirdparty/unordered_dense/include"
   "${ROOT}/tools/XenosRecomp/thirdparty/zstd/lib"
   "${ROOT}/tools/XenonRecomp/thirdparty/tomlplusplus/vendor")
 target_compile_definitions(LoNativeDlssRendererTest PRIVATE LO_SOURCE_VERSION="p2-renderer-fixture")
 if(MSVC)
  target_compile_options(LoNativeDlssRendererTest PRIVATE /Od /Gy /Gw)
  target_link_options(LoNativeDlssRendererTest PRIVATE /OPT:REF /INCREMENTAL:NO)
 else()
  target_compile_options(LoNativeDlssRendererTest PRIVATE -O0 -g0 -ffunction-sections -fdata-sections)
  target_link_options(LoNativeDlssRendererTest PRIVATE -Wl,--gc-sections)
 endif()
 target_compile_definitions(LoNativeDlssRendererTest PRIVATE LO_NATIVE_DLSS_TEST_INJECT_EVALUATE_FAILURE=1)
 lo_enable_dlss(LoNativeDlssRendererTest)
 lo_enable_fsr(LoNativeDlssRendererTest)
 if(WIN32)
  # COFF retains the portable-pack implementation with its out-of-line
  # destructor. Link its real public dependency, not fake codec functions.
  if(NOT TARGET libzstd_static)
   set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
   set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
   set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
   set(ZSTD_USE_STATIC_RUNTIME ON CACHE BOOL "" FORCE)
   add_subdirectory("${ROOT}/tools/XenosRecomp/thirdparty/zstd/build/cmake" zstd EXCLUDE_FROM_ALL)
  endif()
  target_link_libraries(LoNativeDlssRendererTest PRIVATE libzstd_static)
 endif()
  target_link_libraries(LoNativeDlssRendererTest PRIVATE mv_runtime)
   add_executable(LoDlssEvaluateCaptureContractTest "${ROOT}/tools/tests/dlss_evaluate_capture_contract_test.cpp")
   target_include_directories(LoDlssEvaluateCaptureContractTest PRIVATE "${ROOT}/LostOdysseyRecomp")
   target_include_directories(LoDlssEvaluateCaptureContractTest PRIVATE "${ROOT}/tools/XenonRecomp/thirdparty/tomlplusplus/vendor")
   target_link_libraries(LoDlssEvaluateCaptureContractTest PRIVATE mv_runtime)
  add_executable(LoVideoSubmissionStopTest "${ROOT}/tools/tests/video_submission_stop_test.cpp"
   "${ROOT}/LostOdysseyRecomp/gpu/video.cpp"
   "${ROOT}/LostOdysseyRecomp/gpu/upscaling_plan.cpp"
   "${ROOT}/LostOdysseyRecomp/gpu/dlss_ngx.cpp" platform_stub.cpp)
  target_include_directories(LoVideoSubmissionStopTest BEFORE PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/compile_boundary")
  target_include_directories(LoVideoSubmissionStopTest PRIVATE "${ROOT}/thirdparty/unordered_dense/include")
  target_compile_definitions(LoVideoSubmissionStopTest PRIVATE LO_VIDEO_SUBMISSION_UNIT LO_GPU_PLUME NOMINMAX
   LO_SOURCE_VERSION="video-submission-stop")
  if(MSVC)
   target_compile_options(LoVideoSubmissionStopTest PRIVATE /Od)
  else()
   target_compile_options(LoVideoSubmissionStopTest PRIVATE -O0 -g0)
  endif()
  lo_enable_dlss(LoVideoSubmissionStopTest)
  target_link_libraries(LoVideoSubmissionStopTest PRIVATE mv_runtime)
  add_test(NAME LoNativeDlssRendererTest COMMAND LoNativeDlssRendererTest)
 add_test(NAME LoNativeDlssRendererNativeTest COMMAND LoNativeDlssRendererTest --native)
 set_tests_properties(LoNativeDlssRendererNativeTest PROPERTIES LABELS "native-dlss;ngx-renderer" TIMEOUT 180 SKIP_RETURN_CODE 77)
 set_tests_properties(LoNativeDlssRendererTest PROPERTIES LABELS "native-dlss;gpu-renderer" TIMEOUT 180)
endif()

# SR hybrid producer: real Vulkan pixels, plus strict CPU identity/retry contracts.
add_executable(sr_hybrid_motion_gpu_test "${ROOT}/tools/tests/sr_hybrid_motion_gpu_test.cpp" "${CMAKE_CURRENT_LIST_DIR}/platform_stub.cpp")
target_link_libraries(sr_hybrid_motion_gpu_test PRIVATE mv_runtime)
add_test(NAME sr_hybrid_motion_gpu COMMAND sr_hybrid_motion_gpu_test)
add_executable(sr_hybrid_motion_test "${ROOT}/tools/tests/sr_hybrid_motion_test.cpp" "${ROOT}/LostOdysseyRecomp/gpu/upscaling_plan.cpp")
target_include_directories(sr_hybrid_motion_test PRIVATE "${ROOT}/LostOdysseyRecomp")
add_test(NAME sr_hybrid_motion_cpu COMMAND sr_hybrid_motion_test)

# Capture assertions must execute in Release too.
if(MSVC)
 target_compile_options(LoDlssEvaluateCaptureContractTest PRIVATE /UNDEBUG)
else()
 target_compile_options(LoDlssEvaluateCaptureContractTest PRIVATE -UNDEBUG)
endif()
add_test(NAME hybrid_capture_metadata COMMAND LoDlssEvaluateCaptureContractTest --evaluate-capture-contract-only)
set_tests_properties(sr_hybrid_motion_gpu hybrid_capture_metadata PROPERTIES TIMEOUT 180)
