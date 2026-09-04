# Recompiled game code: the XenonRecomp output in ppc/ built as one static library.
# Regenerate ppc/ with:  out/build/tools/XenonRecomp/XenonRecomp/XenonRecomp.exe config/LostOdysseyRecomp.toml tools/XenonRecomp/XenonUtils/ppc_context.h
# (ppc/ is gitignored; run tools/build_tools.bat first)
project(LostOdysseyRecompLib LANGUAGES CXX)

file(GLOB LO_PPC_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_recomp.*.cpp")
list(APPEND LO_PPC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/ppc/ppc_func_mapping.cpp")

if(NOT LO_PPC_SOURCES)
    message(FATAL_ERROR "No recompiled sources in LostOdysseyRecompLib/ppc; run XenonRecomp first")
endif()

add_library(LostOdysseyRecompLib STATIC ${LO_PPC_SOURCES})
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
