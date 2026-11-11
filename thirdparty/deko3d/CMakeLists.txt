# deko3d 0.5.0 (devkitPro, zlib licence) built from source instead of devkitPro's libdeko3d.a, for the fixes in
# PATCHES.md. The flags are upstream Makefile's release ones (libdeko3d.a): -O2 with its extra optimisations,
# -DNDEBUG, no RTTI or exceptions, gnu++17. generated/ holds what devkitPro's dekotools write from
# source/maxwell/*.def and *.mme (PATCHES.md says how), as the switch-dev image does not ship them.
file(GLOB LO_DEKO3D_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/source/maxwell/*.cpp")
add_library(lo_deko3d STATIC ${LO_DEKO3D_SOURCES})
target_include_directories(lo_deko3d PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/generated" "${CMAKE_CURRENT_SOURCE_DIR}/source"
            "${CMAKE_CURRENT_SOURCE_DIR}/source/maxwell")
target_compile_definitions(lo_deko3d PRIVATE __DK_INTERNAL__)
# Upstream's -DNDEBUG=1, unless the build type's flags already define it (Release, RelWithDebInfo).
string(TOUPPER "${CMAKE_BUILD_TYPE}" _lo_deko3d_config)
if(NOT CMAKE_CXX_FLAGS_${_lo_deko3d_config} MATCHES "NDEBUG")
    target_compile_definitions(lo_deko3d PRIVATE NDEBUG=1)
endif()
# After CMAKE_CXX_FLAGS_RELEASE (-O3), so -O2 wins.
target_compile_options(lo_deko3d PRIVATE -O2 -Wall -ftls-model=local-exec
    -fpredictive-commoning -fgcse-after-reload -ftree-loop-distribution -ftree-loop-distribute-patterns
    -ftree-vectorize -floop-interchange -fsplit-paths -fvect-cost-model=cheap -ftree-partial-pre
    "-fmacro-prefix-map=${CMAKE_CURRENT_SOURCE_DIR}/source/=" -fno-rtti -fno-exceptions -std=gnu++17)
