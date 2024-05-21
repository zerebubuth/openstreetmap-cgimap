function(add_format_target TARGET)
endfunction()

if(NOT STANDALONE_BUILD)
  return()
endif()

find_program(CLANG_FORMAT_BIN clang-format)

if(NOT CLANG_FORMAT_BIN)
  return()
endif()

function(add_format_target TARGET)
  add_custom_target(format_${TARGET}
    COMMAND ${CLANG_FORMAT_BIN} -i $<TARGET_PROPERTY:${TARGET},SOURCES>
    $<TARGET_PROPERTY:${TARGET},SJPARSER_HEADERS>
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMAND_EXPAND_LISTS
  )

  if(NOT TARGET format)
    add_custom_target(format)
  endif()

  add_dependencies(format format_${TARGET})
endfunction()
