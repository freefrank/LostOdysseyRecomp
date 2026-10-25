cmake_minimum_required(VERSION 3.20)
project(LoShadowBudgetTests LANGUAGES CXX)
include(CTest)
get_filename_component(LO_ROOT "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
set(LO_SHADOW_SOURCE "${LO_ROOT}/LostOdysseyRecomp/patches/shadow_budget.cpp" CACHE FILEPATH
    "Production shadow hook (override with a baseline checkout for comparisons)")
option(LO_SHADOW_SANITIZE "Enable AddressSanitizer and UndefinedBehaviorSanitizer" "${LO_REVIEW_SANITIZE}")

# Compile the complete production wrappers. Only guest memory, settings, logging
# and original guest functions are stand-ins; no game inputs or GPU are needed.
add_executable(LoShadowBudgetTest budget_test.cpp "${LO_SHADOW_SOURCE}"
    "${LO_ROOT}/LostOdysseyRecomp/patches/graphics_effects.cpp")
target_include_directories(LoShadowBudgetTest PRIVATE stubs "${LO_ROOT}/LostOdysseyRecomp")
target_compile_features(LoShadowBudgetTest PRIVATE cxx_std_20)
if(MSVC)
    target_compile_options(LoShadowBudgetTest PRIVATE /W4 /utf-8)
else()
    target_compile_options(LoShadowBudgetTest PRIVATE -Wall -Wextra -Werror)
    if(LO_SHADOW_SANITIZE)
        target_compile_options(LoShadowBudgetTest PRIVATE -fsanitize=address,undefined -fno-omit-frame-pointer)
        target_link_options(LoShadowBudgetTest PRIVATE -fsanitize=address,undefined)
    endif()
endif()
add_test(NAME shadow_budget COMMAND LoShadowBudgetTest)
add_test(NAME shadow_budget_verify COMMAND LoShadowBudgetTest)
set_tests_properties(shadow_budget_verify PROPERTIES ENVIRONMENT "LO_SHADOW_CACHE_VERIFY=1;LO_SHADOW_STATS=1")
