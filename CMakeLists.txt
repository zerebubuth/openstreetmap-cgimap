cmake_minimum_required(VERSION 3.8)

project(sjparser)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
set(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

option(SJPARSER_WITH_TESTS
  "Build tests if the config is not Debug" OFF)
option(SJPARSER_WITH_COVERAGE
  "Add coverage target (only in Debug config)" OFF)
option(SJPARSER_BUILD_SHARED_LIBRARY
  "Build shared library even in case of submodule build" OFF)

if(NOT CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  message(STATUS
    "Sjparser is a part of some project, only library targets are enabled.")
  set(STANDALONE_BUILD FALSE)
else()
  set(STANDALONE_BUILD TRUE)
  set(SJPARSER_BUILD_SHARED_LIBRARY TRUE)

  if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(SJPARSER_WITH_TESTS TRUE)
  endif()
endif()

function(setup_compilation_options TARGET)
  target_compile_features(${TARGET} PUBLIC cxx_std_17)

  target_compile_options(${TARGET} PRIVATE
    -Werror
    -Wall
    -Wextra
    -Wpedantic
    $<$<CONFIG:Debug>:-O0 -g3>
  )

  if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND SJPARSER_WITH_COVERAGE)
    target_compile_options(${TARGET} PRIVATE -O0 --coverage)
    target_link_libraries(${TARGET} PRIVATE --coverage)
  endif()
endfunction()

add_subdirectory(library)

if(SJPARSER_WITH_TESTS)
  enable_testing()
  add_subdirectory(tests)
endif()

if(STANDALONE_BUILD)
  add_subdirectory(check)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug" AND SJPARSER_WITH_COVERAGE)
  add_subdirectory(coverage)
endif()
