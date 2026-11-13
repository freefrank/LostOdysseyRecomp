cmake_minimum_required(VERSION 3.20)
project(LoGuestEntry LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
get_filename_component(ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../.." ABSOLUTE)
set(RECOMP "${ROOT}/tools/XenonRecomp")
if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The generated PPC code requires Clang, as the runtime does")
endif()
add_subdirectory("${RECOMP}" xenon EXCLUDE_FROM_ALL)
add_executable(LoGuestEntryGenerate generate.cpp
    "${RECOMP}/XenonRecomp/recompiler.cpp"
    "${RECOMP}/XenonRecomp/recompiler_config.cpp")
target_include_directories(LoGuestEntryGenerate PRIVATE "${RECOMP}/XenonRecomp")
target_precompile_headers(LoGuestEntryGenerate PRIVATE "${RECOMP}/XenonRecomp/pch.h")
target_link_libraries(LoGuestEntryGenerate PRIVATE
    LibXenonAnalyse XenonUtils fmt::fmt tomlplusplus::tomlplusplus xxHash::xxhash)
target_compile_definitions(LoGuestEntryGenerate PRIVATE _CRT_SECURE_NO_WARNINGS)
if(NOT MSVC)
    target_compile_options(LoGuestEntryGenerate PRIVATE -Wno-switch -Wno-unused-variable -Wno-null-arithmetic)
endif()
set(LO_GUEST_ENTRY_NEGATIVE_CONTROL "OFF" CACHE STRING "OFF, missing-table, or baseline")
set_property(CACHE LO_GUEST_ENTRY_NEGATIVE_CONTROL PROPERTY STRINGS OFF missing-table baseline)
set(negative_control)
if(LO_GUEST_ENTRY_NEGATIVE_CONTROL STREQUAL "missing-table")
    set(negative_control --without-table)
elseif(LO_GUEST_ENTRY_NEGATIVE_CONTROL STREQUAL "baseline")
    set(negative_control --without-fix)
elseif(NOT LO_GUEST_ENTRY_NEGATIVE_CONTROL STREQUAL "OFF")
    message(FATAL_ERROR "Unknown negative control: ${LO_GUEST_ENTRY_NEGATIVE_CONTROL}")
endif()
set(generated "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_custom_command(OUTPUT "${generated}/ppc_recomp.0.cpp" "${generated}/ppc_func_mapping.cpp"
        "${generated}/ppc_config.h" "${generated}/ppc_context.h" "${generated}/ppc_recomp_shared.h"
    COMMAND LoGuestEntryGenerate "${ROOT}/LostOdysseyRecompLib/config/LostOdysseyRecomp.toml"
        "${RECOMP}/XenonUtils/ppc_context.h" "${generated}" ${negative_control}
    DEPENDS LoGuestEntryGenerate "${ROOT}/LostOdysseyRecompLib/config/LostOdysseyRecomp.toml"
        "${ROOT}/LostOdysseyRecompLib/config/switch_tables.toml"
        "${RECOMP}/XenonUtils/ppc_context.h"
    VERBATIM)
add_executable(LoGuestEntryTest check.cpp "${generated}/ppc_recomp.0.cpp" "${generated}/ppc_func_mapping.cpp")
# check.cpp includes the generated bodies so indirect lookups can be observed.
set_source_files_properties("${generated}/ppc_recomp.0.cpp" "${generated}/ppc_func_mapping.cpp" PROPERTIES HEADER_FILE_ONLY TRUE)
target_include_directories(LoGuestEntryTest PRIVATE "${generated}" "${RECOMP}/thirdparty/simde")
option(LO_GUEST_ENTRY_SANITIZE "Enable address and undefined behavior sanitizers" OFF)
if(LO_GUEST_ENTRY_SANITIZE AND NOT MSVC)
    target_compile_options(LoGuestEntryTest PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(LoGuestEntryTest PRIVATE -fsanitize=address,undefined)
endif()
enable_testing()
add_test(NAME guest_entry COMMAND LoGuestEntryTest)
