# Recompiled game code: the XenonRecomp output in ppc/ built as one static library.
# Regenerate ppc/ with: python tools/ppc_codegen.py generate
# (ppc/ is gitignored; run tools/build_tools.bat first)
project(LostOdysseyRecompLib LANGUAGES CXX)

find_package(Python3 3.11 REQUIRED COMPONENTS Interpreter)
execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/ppc_codegen.py" check
    RESULT_VARIABLE LO_CODEGEN_RESULT)
if(NOT LO_CODEGEN_RESULT EQUAL 0)
    message(FATAL_ERROR "PPC sources are stale; run tools/build_tools.bat then python tools/ppc_codegen.py generate")
endif()
add_custom_target(LoPpcCodegenCheck
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/ppc_codegen.py" check
    COMMENT "Checking PPC generation inputs and outputs" VERBATIM)

file(GLOB LO_PPC_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_recomp.*.cpp")
if(NOT LO_PPC_SOURCES OR NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_func_mapping.cpp")
    message(FATAL_ERROR "No recompiled sources in LostOdysseyRecompLib/ppc; run XenonRecomp first")
endif()
list(APPEND LO_PPC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_func_mapping.cpp")

set(LO_PREBUILT_PPC_DIR "" CACHE PATH "Verified local PPC bundle to use instead of compiling generated sources")
if(LO_PREBUILT_PPC_DIR)
    if(NOT CMAKE_BUILD_TYPE STREQUAL "Release" OR NOT CMAKE_SIZEOF_VOID_P EQUAL 8
       OR NOT WIN32 OR NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        message(FATAL_ERROR "Prebuilt PPC requires Windows x64 Release with clang-cl")
    endif()
    execute_process(COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/release/ppc_prebuilt.py"
        check --bundle "${LO_PREBUILT_PPC_DIR}"
        RESULT_VARIABLE LO_PREBUILT_RESULT)
    if(NOT LO_PREBUILT_RESULT EQUAL 0)
        message(FATAL_ERROR "Prebuilt PPC does not match this source tree; upload a fresh bundle or disable LO_PREBUILT_PPC_DIR")
    endif()
    # Generate the same compile rules for contract verification, without building them.
    add_library(LoPpcCompileContract STATIC EXCLUDE_FROM_ALL ${LO_PPC_SOURCES})
    set(LO_PPC_COMPILE_TARGET LoPpcCompileContract)
    add_library(LostOdysseyRecompLib STATIC IMPORTED GLOBAL)
    set_target_properties(LostOdysseyRecompLib PROPERTIES
        IMPORTED_LOCATION "${LO_PREBUILT_PPC_DIR}/LostOdysseyRecompLib.lib"
        INTERFACE_INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}/ppc;${CMAKE_SOURCE_DIR}/tools/XenonRecomp/thirdparty/simde"
        INTERFACE_COMPILE_FEATURES cxx_std_20)
    add_custom_target(LoPpcPrebuiltCheck
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/release/ppc_prebuilt.py"
            check --bundle "${LO_PREBUILT_PPC_DIR}" --build-dir "${CMAKE_BINARY_DIR}"
        COMMENT "Verifying prebuilt PPC inputs, library and compile contract" VERBATIM)
    add_dependencies(LostOdysseyRecompLib LoPpcPrebuiltCheck)
else()
    add_library(LostOdysseyRecompLib STATIC ${LO_PPC_SOURCES})
    set(LO_PPC_COMPILE_TARGET LostOdysseyRecompLib)
    add_dependencies(LostOdysseyRecompLib LoPpcCodegenCheck)
endif()
target_include_directories(${LO_PPC_COMPILE_TARGET} PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/ppc"
    "${CMAKE_SOURCE_DIR}/tools/XenonRecomp/thirdparty/simde")
target_compile_features(${LO_PPC_COMPILE_TARGET} PUBLIC cxx_std_20)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The recompiled code requires clang-cl (uses __int128, weak aliases, builtins)")
endif()
# Same spirit as UnleashedRecompLib: no exceptions, no RTTI, warnings off for generated code.
target_compile_options(${LO_PPC_COMPILE_TARGET} PRIVATE /w /EHs-c- /GR- -fno-strict-aliasing
    # MMIO stores (eieio-prefixed) are routed to the runtime's command processor.
    "/FI${CMAKE_SOURCE_DIR}/LostOdysseyRecomp/gpu/ppc_mmio.h")

include("${CMAKE_SOURCE_DIR}/tools/release/ppc_sync.cmake")
