include_guard(GLOBAL)
get_filename_component(LO_PACK_ROOT "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
option(LO_PACK_FETCH_ZSTD "Fetch pinned Zstandard if no static package is installed" ON)
find_package(zstd 1.5 CONFIG QUIET)
if(TARGET zstd::libzstd_static)
    set(LO_PACK_ZSTD_TARGET zstd::libzstd_static)
else()
    if(NOT LO_PACK_FETCH_ZSTD)
        message(FATAL_ERROR "A static zstd CMake package is required (or enable LO_PACK_FETCH_ZSTD).")
    endif()
    include(FetchContent)
    set(ZSTD_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_SHARED OFF CACHE BOOL "" FORCE)
    set(ZSTD_BUILD_STATIC ON CACHE BOOL "" FORCE)
    set(ZSTD_MULTITHREAD_SUPPORT OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(lo_pack_zstd
        GIT_REPOSITORY https://github.com/facebook/zstd.git
        GIT_TAG f8745da6ff1ad1e7bab384bd1f9d742439278e99 # v1.5.7
        SOURCE_SUBDIR build/cmake)
    FetchContent_MakeAvailable(lo_pack_zstd)
    set(LO_PACK_ZSTD_TARGET libzstd_static)
endif()

add_library(lo_portable_shader_pack STATIC EXCLUDE_FROM_ALL
    "${LO_PACK_ROOT}/LostOdysseyRecomp/gpu/shader/portable_shader_pack.cpp")
target_compile_features(lo_portable_shader_pack PUBLIC cxx_std_20)
target_include_directories(lo_portable_shader_pack PUBLIC "${LO_PACK_ROOT}/LostOdysseyRecomp")
target_link_libraries(lo_portable_shader_pack PRIVATE ${LO_PACK_ZSTD_TARGET})
if(WIN32)
    target_compile_definitions(lo_portable_shader_pack PRIVATE NOMINMAX)
    target_link_libraries(lo_portable_shader_pack PRIVATE bcrypt)
endif()

if(NOT TARGET fmt::fmt)
    add_subdirectory("${LO_PACK_ROOT}/tools/XenosRecomp/thirdparty/fmt" "${CMAKE_CURRENT_BINARY_DIR}/lo_pack_fmt" EXCLUDE_FROM_ALL)
endif()
add_executable(LoShaderPackTool EXCLUDE_FROM_ALL
    "${LO_PACK_ROOT}/tools/shader_pack/main.cpp"
    "${LO_PACK_ROOT}/tools/shader_pack/merge.cpp"
    "${LO_PACK_ROOT}/LostOdysseyRecomp/gpu/shader/xenos_translator.cpp"
    "${LO_PACK_ROOT}/LostOdysseyRecomp/gpu/shader/dxc_compiler.cpp")
target_include_directories(LoShaderPackTool PRIVATE "${LO_PACK_ROOT}/tools/XenosRecomp/thirdparty/dxc-bin/inc")
target_link_libraries(LoShaderPackTool PRIVATE lo_portable_shader_pack fmt::fmt ${CMAKE_DL_LIBS})
add_executable(LoPortableShaderPackTest EXCLUDE_FROM_ALL "${LO_PACK_ROOT}/tools/tests/portable_shader_pack_test.cpp")
target_link_libraries(LoPortableShaderPackTest PRIVATE lo_portable_shader_pack)
add_executable(LoPortableShaderPackIntegrationTest EXCLUDE_FROM_ALL
    "${LO_PACK_ROOT}/tools/tests/portable_shader_pack_integration_test.cpp")
target_link_libraries(LoPortableShaderPackIntegrationTest PRIVATE lo_portable_shader_pack)
add_executable(LoPortableShaderContractTest EXCLUDE_FROM_ALL
    "${LO_PACK_ROOT}/tools/tests/portable_shader_contract_test.cpp")
target_link_libraries(LoPortableShaderContractTest PRIVATE lo_portable_shader_pack)
find_package(Threads REQUIRED)
target_link_libraries(LoPortableShaderPackTest PRIVATE Threads::Threads)

set(LO_PORTABLE_SHADER_PACK "" CACHE FILEPATH "Pre-exported .lospv to stage with the executable")
function(lo_stage_portable_shader_pack target)
    if(UNIX)
        install(FILES "${LO_PACK_ROOT}/thirdparty/zstd-LICENSE.txt"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/licenses/lost-odyssey-recomp")
    endif()
    if(LO_PORTABLE_SHADER_PACK)
        if(NOT EXISTS "${LO_PORTABLE_SHADER_PACK}")
            message(FATAL_ERROR "LO_PORTABLE_SHADER_PACK does not exist; export it with LO_SHADER_EXPORT_PACK first.")
        endif()
        get_filename_component(pack "${LO_PORTABLE_SHADER_PACK}" ABSOLUTE)
        add_dependencies(${target} LoShaderPackTool)
        add_custom_command(TARGET ${target} POST_BUILD
            COMMAND $<TARGET_FILE:LoShaderPackTool> verify-runtime "${pack}"
                "${LO_PACK_ROOT}/LostOdysseyRecompLib/private/image_disc1.bin"
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:${target}>/shaders"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${pack}"
                "$<TARGET_FILE_DIR:${target}>/shaders/portable_vk.lospv"
            VERBATIM)
        if(UNIX)
            install(FILES "${pack}" DESTINATION "${CMAKE_INSTALL_BINDIR}/shaders" RENAME portable_vk.lospv)
        endif()
    endif()
endfunction()
