find_program(CLANG_TIDY_BIN clang-tidy)

if(NOT CLANG_TIDY_BIN)
  message(STATUS "Can't find clang-tidy, no check target created")
  return()
endif()

set(CHECKS
  *
  # Dealing with C api requires lots of reinterpret casts
  -cppcoreguidelines-pro-type-reinterpret-cast
  # Google style specific check
  -google-readability-todo
  # For some reason, it does not play well with comments before pragma once
  -llvm-header-guard
  # clang-tidy and clang-format seems to disagree
  -llvm-include-order
  # Project-specific checks
  -fuchsia-*
  -llvmlibc-*
  # Does not work with if constexpr
  -readability-braces-around-statements
  -bugprone-suspicious-semicolon
  -hicpp-braces-around-statements
  -google-readability-braces-around-statements
  # Does not allow to write move constructor / assignment operator for a class
  # with a parent.
  -bugprone-use-after-move
  -hicpp-invalid-access-moved
  ########
  -modernize-use-trailing-return-type
  -altera-struct-pack-align
  -altera-unroll-loops
  -cppcoreguidelines-avoid-const-or-ref-data-members
  -readability-identifier-length
)

string(REPLACE ";" "," CHECKS "${CHECKS}")

set(CHECK_CONFIG "{ \
  Checks: '${CHECKS}', \
  CheckOptions: [{ \
    key: misc-non-private-member-variables-in-classes.IgnoreClassesWithAllMemberVariablesBeingPublic, \
    value: 1 \
  }]}")

get_target_property(SJPARSER_SOURCE_DIR sjparser SOURCE_DIR)

add_custom_target(check_srcs
  COMMAND ${CLANG_TIDY_BIN}
  -p ${CMAKE_BINARY_DIR}
  -config=${CHECK_CONFIG}
  -warnings-as-errors=*
  -header-filter=.*
  $<TARGET_PROPERTY:sjparser,SOURCES> 
  WORKING_DIRECTORY  ${SJPARSER_SOURCE_DIR}
  VERBATIM
  COMMAND_EXPAND_LISTS
)

get_target_property(SJPARSER_HEADERS sjparser SJPARSER_HEADERS)

set(FILTERS "[")
set(FIRST_FILTER TRUE)

foreach(HEADER ${SJPARSER_HEADERS})
  if(FIRST_FILTER)
    set(FIRST_FILTER FALSE)
  else()
    set(FILTERS "${FILTERS},")
  endif()
  set(FILTERS "${FILTERS}{\"name\":\"${HEADER}\"}")
endforeach()
set(FILTERS "${FILTERS}]")

add_custom_target(check DEPENDS check_srcs)

if(SJPARSER_WITH_TESTS)
  get_target_property(SJPARSER_TESTS_DIR sjparser_tests SOURCE_DIR)
  add_custom_target(check_tests
    COMMAND ${CLANG_TIDY_BIN}
      -p ${CMAKE_BINARY_DIR}
      -config=${CHECK_CONFIG}
      -warnings-as-errors=*
      -header-filter=.*
      -line-filter=${FILTERS}
      $<TARGET_PROPERTY:sjparser_tests,SOURCES>
    WORKING_DIRECTORY  ${SJPARSER_TESTS_DIR}
    VERBATIM
    COMMAND_EXPAND_LISTS
  )
  add_dependencies(check check_tests)
endif()

