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

add_library(LostOdysseyRecompLib STATIC ${LO_PPC_SOURCES})
add_dependencies(LostOdysseyRecompLib LoPpcCodegenCheck)
target_include_directories(LostOdysseyRecompLib PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/ppc"
    "${CMAKE_SOURCE_DIR}/tools/XenonRecomp/thirdparty/simde")
target_compile_features(LostOdysseyRecompLib PUBLIC cxx_std_20)

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    message(FATAL_ERROR "The recompiled code requires clang-cl (uses __int128, weak aliases, builtins)")
endif()
# Same spirit as UnleashedRecompLib: no exceptions, no RTTI, warnings off for generated code.
target_compile_options(LostOdysseyRecompLib PRIVATE /w /EHs-c- /GR- -fno-strict-aliasing
    # MMIO stores (eieio-prefixed) are routed to the runtime's command processor.
    "/FI${CMAKE_SOURCE_DIR}/LostOdysseyRecomp/gpu/ppc_mmio.h")
