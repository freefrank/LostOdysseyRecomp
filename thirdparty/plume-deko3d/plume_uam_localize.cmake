# banjo-nx (2026-10-17): uam linked into one object (plume's deko3d back end, CMakeLists.txt) with its strong symbols made
# local, except its API (uam::*). uam's Mesa (19) and the static NVK's newer Mesa define C functions of the same names
# with other signatures (ralloc, the linear allocator, hash tables); local, uam's calls stay in uam. Weak symbols (C++
# template and inline code in COMDAT groups) stay global: made local, the final link keeps another object's copy of the
# group and uam's references point into a discarded section.
#   cmake -DLD=... -DNM=... -DOBJCOPY=... -DARCHIVE=libuamlib.a -DOUT=uam_local.o -P plume_uam_localize.cmake
execute_process(COMMAND "${LD}" -r -o "${OUT}" --whole-archive "${ARCHIVE}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "plume_uam_localize: ld -r failed (${rc})")
endif()
execute_process(COMMAND "${NM}" --defined-only "${OUT}" OUTPUT_VARIABLE symbols RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "plume_uam_localize: nm failed (${rc})")
endif()
string(REPLACE "\n" ";" lines "${symbols}")
set(localize "")
set(count 0)
foreach(line IN LISTS lines)
    # "<address> <type> <name>": upper-case T D B R G S are global strong definitions.
    if(line MATCHES "^[0-9a-fA-F]+ ([TDBRGS]) (.+)$")
        set(name "${CMAKE_MATCH_2}")
        if(NOT name MATCHES "^_ZN3uam")
            string(APPEND localize "${name}\n")
            math(EXPR count "${count} + 1")
        endif()
    endif()
endforeach()
file(WRITE "${OUT}.localize.txt" "${localize}")
execute_process(COMMAND "${OBJCOPY}" "--localize-symbols=${OUT}.localize.txt" "${OUT}" RESULT_VARIABLE rc)
if(NOT rc EQUAL 0)
    message(FATAL_ERROR "plume_uam_localize: objcopy failed (${rc})")
endif()
message(STATUS "plume_uam_localize: ${count} symbols of uam made local")
