find_program(GCOV_BIN gcov)
find_program(GCOVR_BIN gcovr)

if(NOT GCOV_BIN)
  message(FATAL_ERROR "Can't find gcov")
endif()

if(NOT GCOVR_BIN)
  message(STATUS "Can't find gcovr, no coverage target created")
  return()
endif()

set(OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/report)

file(MAKE_DIRECTORY ${OUTPUT_DIR})

add_custom_target(coverage
  COMMAND ${CMAKE_CTEST_COMMAND}
  COMMAND ${GCOVR_BIN} --root ${CMAKE_SOURCE_DIR}
    --filter ${CMAKE_SOURCE_DIR}/library
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)

add_custom_target(coverage-report
  COMMAND ${CMAKE_CTEST_COMMAND}
  COMMAND ${GCOVR_BIN} --root ${CMAKE_SOURCE_DIR}
    --filter ${CMAKE_SOURCE_DIR}/library
    --html --html-details -o ${OUTPUT_DIR}/report.html
  WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
)
