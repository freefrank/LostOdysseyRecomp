# Automatic private uploads are explicitly enabled per local checkout, not for
# contributors or hosted runners. The synchronizer also enforces these guards.
execute_process(COMMAND git -C "${CMAKE_SOURCE_DIR}" config --local --bool lo.ppcAutoSync
    OUTPUT_VARIABLE LO_PPC_SYNC_CONFIG OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET)
option(LO_PPC_AUTO_SYNC "Synchronize locally built PPC libraries to the private input repository"
    "${LO_PPC_SYNC_CONFIG}")

if(LO_PPC_AUTO_SYNC AND NOT LO_PREBUILT_PPC_DIR
   AND NOT DEFINED ENV{CI} AND NOT DEFINED ENV{GITHUB_ACTIONS}
   AND NOT DEFINED ENV{LO_PPC_SYNC_ACTIVE})
    # Direct library builds synchronize after a successful link. Runtime builds
    # also check on no-op builds, so a new input receipt cannot be overlooked.
    add_custom_command(TARGET LostOdysseyRecompLib POST_BUILD
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/release/ppc_sync.py"
            sync --build-dir "${CMAKE_BINARY_DIR}" --already-built
        COMMENT "Synchronizing changed PPC library inputs" VERBATIM)
    add_custom_target(LoPpcAutoSync
        COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/tools/release/ppc_sync.py"
            sync --build-dir "${CMAKE_BINARY_DIR}" --already-built
        DEPENDS LostOdysseyRecompLib
        COMMENT "Checking private PPC synchronization" VERBATIM)
endif()
