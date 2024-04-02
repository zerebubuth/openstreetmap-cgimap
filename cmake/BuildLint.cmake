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
        set(CMAKE_CXX_CLANG_TIDY "${CLANG_TIDY}" -extra-arg=-std=c++17)
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

        set(CMAKE_CXX_CPPLINT "${CPPLINT}")
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

        set(CMAKE_CXX_INCLUDE_WHAT_YOU_USE "${IWYU}")
    else()
        message(STATUS "Looking for include-what-you-use - not found")
    endif()
endif()
