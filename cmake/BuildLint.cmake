# CMAKE_CXX_<LINTER> variables need to be set before targets are created

############
# clang-tidy
############
option(ENABLE_BUILD_LINT_CLANG_TIDY "Automatically check code with clang-tidy during compilation" OFF)

find_program(CLANG_TIDY
        NAMES clang-tidy clang-tidy-16 clang-tidy-15 clang-tidy-14 clang-tidy-13 clang-tidy-12 clang-tidy-11)

if(CLANG_TIDY)
    message(STATUS "Looking for clang-tidy - found ${CLANG_TIDY}")

    if(ENABLE_BUILD_LINT_CLANG_TIDY)
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}" -extra-arg=-std=c++20)
    endif()
else()
    message(STATUS "Looking for clang-tidy - not found")
endif()

##########
# cppcheck
##########
option(ENABLE_BUILD_LINT_CPPCHECK "Automatically check code with cppcheck during compilation" OFF)

if(ENABLE_BUILD_LINT_CPPCHECK)
    find_program(CPPCHECK NAMES cppcheck)

    if(CPPCHECK)
        message(STATUS "Looking for cppcheck - found ${CPPCHECK}")

        set(CMAKE_CXX_CPPCHECK "${CPPCHECK}")
    else()
        message(STATUS "Looking for cppcheck - not found")
    endif()
endif()

#########
# cpplint
#########
option(ENABLE_BUILD_LINT_CPPLINT "Automatically check code with cpplint during compilation" OFF)

if(ENABLE_BUILD_LINT_CPPLINT)
    find_program(CPPLINT NAMES cpplint)

    if(CPPLINT)
        message(STATUS "Looking for cpplint - found ${CPPLINT}")

        set(CMAKE_CXX_CPPLINT "${CPPLINT}" --filter=-whitespace,-whitespace/braces,-whitespace/indent,-whitespace/line_length,-whitespace/comments,-readability/todo,-runtime/references,-build/c++11,-build/include_order)
    else()
        message(STATUS "Looking for cpplint - not found")
    endif()
endif()

######################
# include-what-you-use
######################
option(ENABLE_BUILD_LINT_IWYU "Automatically check code with include-what-you-use during compilation" OFF)

if(ENABLE_BUILD_LINT_IWYU)
    find_program(IWYU NAMES iwyu)

    if(IWYU)
        message(STATUS "Looking for include-what-you-use - found ${IWYU}")

        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${IWYU}" -std=c++20 -Xiwyu --cxx17ns -Xiwyu --no_fwd_decls -Xiwyu "--mapping_file=${CMAKE_SOURCE_DIR}/.iwyu_mappings.imp")
    else()
        message(STATUS "Looking for include-what-you-use - not found")
    endif()
endif()


########################################################
# Disable / save / restore lint config utility functions
########################################################

set(BUILD_LINT_CONFIGS CMAKE_CXX_CLANG_TIDY CMAKE_CXX_CPPCHECK CMAKE_CXX_CPPLINT CMAKE_CXX_INCLUDE_WHAT_YOU_USE)

function(save_build_lint_config LINT_CONFIG_VAR)
    foreach(LINT_CONFIG IN LISTS BUILD_LINT_CONFIGS)
        string(REPLACE ";" "@SEMICOLON@" ${LINT_CONFIG} "${${LINT_CONFIG}}")
        list(APPEND LINT_CONFIGS_LIST "${${LINT_CONFIG}}")
    endforeach()

    set(${LINT_CONFIG_VAR} ${LINT_CONFIGS_LIST} PARENT_SCOPE)
endfunction()


function(restore_build_lint_config LINT_CONFIG_VAR)
    foreach(LINT_CONFIG IN LISTS BUILD_LINT_CONFIGS)
        list(POP_FRONT ${LINT_CONFIG_VAR} ${LINT_CONFIG})
        string(REPLACE "@SEMICOLON@" ";" ${LINT_CONFIG} "${${LINT_CONFIG}}")
        set(${LINT_CONFIG} ${${LINT_CONFIG}} PARENT_SCOPE)
    endforeach()
endfunction()


macro(disable_build_lint)
    foreach(LINT_CONFIG IN LISTS BUILD_LINT_CONFIGS)
        unset(${LINT_CONFIG})
    endforeach()
endmacro()


function(print_build_lint_config)
    foreach(LINT_CONFIG IN LISTS BUILD_LINT_CONFIGS)
        message("lint config ${LINT_CONFIG}: ${${LINT_CONFIG}}")
    endforeach()
endfunction()
