# uam 1.1.0 (devkitPro's deko3d shader compiler: Mesa's GLSL frontend + nouveau's nv50_ir) without meson:
# the static library 'uamlib' (compiler core + uam_api.h) for the desktop/headless builds and the Switch.
# Mirrors meson.build and its subdir()s; the bison/flex/mako outputs are committed in generated/
# (tools/generate.sh). Our patches: PATCHES.md.
# Used from the main project (add_subdirectory) or on its own (cmake -S runtime/third_party/uam):
#   WWHD_UAM_TEST=ON  also builds uam_dksh_test (compares DKSH bytes with reference .dksh files)
#   the 'uam' command line tool (target uam_tool) is defined on desktop builds, built only on request
cmake_minimum_required(VERSION 3.20)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  project(uam C CXX)
  if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
  endif()
endif()

set(UAM_DIR ${CMAKE_CURRENT_SOURCE_DIR})
set(UAM_MESA ${UAM_DIR}/mesa-imported)
set(UAM_GEN ${UAM_DIR}/generated)

set(UAM_SOURCES
  ${UAM_DIR}/source/compiler_iface.cpp
  ${UAM_DIR}/source/glsl_frontend.cpp
  ${UAM_DIR}/source/mini-os.c
  ${UAM_DIR}/source/tgsi_support.cpp
  ${UAM_DIR}/uam_api.cpp
  # generated (glsl/meson.build, glsl/glcpp/meson.build)
  ${UAM_GEN}/glsl/glsl_parser.cpp
  ${UAM_GEN}/glsl/glsl_lexer.cpp
  ${UAM_GEN}/glsl/glcpp/glcpp-parse.c
  ${UAM_GEN}/glsl/glcpp/glcpp-lex.c
  ${UAM_MESA}/glsl/glcpp/pp.c)

# the plain file lists of mesa-imported/*/meson.build
foreach(sub codegen tgsi util cso_cache glsl compiler program state_tracker main)
  file(READ ${UAM_MESA}/${sub}/meson.build _mb)
  string(REGEX MATCHALL "files\\(([^)]*)\\)" _blocks "${_mb}")
  foreach(_block IN LISTS _blocks)
    string(REGEX MATCHALL "'[^']+'" _names "${_block}")
    foreach(_n IN LISTS _names)
      string(REPLACE "'" "" _n "${_n}")
      list(APPEND UAM_SOURCES ${UAM_MESA}/${sub}/${_n})
    endforeach()
  endforeach()
  # a source list change upstream must re-run the configure
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${UAM_MESA}/${sub}/meson.build)
endforeach()

add_library(uamlib STATIC ${UAM_SOURCES})
target_include_directories(uamlib PUBLIC ${UAM_DIR})  # uam_api.h
target_include_directories(uamlib PRIVATE ${UAM_DIR}/source ${UAM_MESA} ${UAM_GEN} ${UAM_GEN}/glsl ${UAM_GEN}/glsl/glcpp)
# meson.build's project options: c99 / c++11 (flex output uses 'register', gone in C++17), release asserts
set_target_properties(uamlib PROPERTIES C_STANDARD 99 C_EXTENSIONS OFF CXX_STANDARD 11 CXX_EXTENSIONS OFF
                                        POSITION_INDEPENDENT_CODE ON)
target_compile_definitions(uamlib PRIVATE "PACKAGE_STRING=\"uam 1.1.0\"" DESKTOP _USE_MATH_DEFINES
                                          # always: Mesa's asserts abort, and a Debug game build must not
                                          NDEBUG)
# banjo-nx: a Windows host (MinGW) has neither strchrnul (_GNU_SOURCE) nor posix_memalign; Mesa's own fallbacks
# (util_strchrnul, _aligned_malloc) are used there.
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
  target_compile_definitions(uamlib PRIVATE _GNU_SOURCE HAVE_POSIX_MEMALIGN)
endif()
target_compile_options(uamlib PRIVATE -ffunction-sections -fdata-sections -w -O2
                                      $<$<COMPILE_LANGUAGE:C>:-Werror=implicit-function-declaration>)
# banjo-nx: also MinGW (a Windows host build of the uam tool): winpthreads
if((NOT CMAKE_SYSTEM_NAME STREQUAL "Windows" OR MINGW) AND NOT DEFINED NX_ROOT)
  find_package(Threads REQUIRED)
  target_link_libraries(uamlib PUBLIC Threads::Threads)  # c11/threads.h: pthread mutexes
endif()

if(NOT DEFINED NX_ROOT)
  # the uam command line tool, as upstream (reference outputs, offline DKSH caches)
  add_executable(uam_tool EXCLUDE_FROM_ALL ${UAM_DIR}/source/main.cpp)
  target_include_directories(uam_tool PRIVATE ${UAM_DIR}/source ${UAM_MESA} ${UAM_GEN} ${UAM_GEN}/glsl)
  target_compile_definitions(uam_tool PRIVATE "PACKAGE_STRING=\"uam 1.1.0\"" DESKTOP NDEBUG)
  set_target_properties(uam_tool PROPERTIES OUTPUT_NAME uam CXX_STANDARD 11)
  target_compile_options(uam_tool PRIVATE -w)
  target_link_libraries(uam_tool PRIVATE uamlib)
endif()

option(WWHD_UAM_TEST "build uam_dksh_test (desktop)" OFF)
if(WWHD_UAM_TEST AND NOT DEFINED NX_ROOT)
  add_executable(uam_dksh_test ${UAM_DIR}/test/uam_dksh_test.cpp)
  set_target_properties(uam_dksh_test PROPERTIES CXX_STANDARD 17)
  target_link_libraries(uam_dksh_test PRIVATE uamlib)
endif()
