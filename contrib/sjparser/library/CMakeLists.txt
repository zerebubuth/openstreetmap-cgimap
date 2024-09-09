find_package(YAJL 2 REQUIRED)

add_library(sjparser STATIC
  sjparser/parsing_error.cpp
  sjparser/yajl_parser.cpp
  sjparser/value.cpp

  sjparser/internals/token_parser.cpp
  sjparser/internals/dispatcher.cpp
  sjparser/internals/ignore.cpp
  sjparser/internals/array_parser.cpp
)
set_property(TARGET sjparser PROPERTY SJPARSER_HEADERS
  sjparser/s_custom_object.h
  sjparser/sjparser.h
  sjparser/parsing_error.h
  sjparser/value.h
  sjparser/object.h
  sjparser/s_array.h
  sjparser/map.h
  sjparser/s_map.h
  sjparser/type_holder.h
  sjparser/array.h
  sjparser/yajl_parser.h
  sjparser/s_auto_object.h
  sjparser/parser.h
  sjparser/member.h
  sjparser/options.h

  sjparser/internals/key_value_parser.h
  sjparser/internals/ignore.h
  sjparser/internals/token_parser.h
  sjparser/internals/traits.h
  sjparser/internals/dispatcher.h
  sjparser/internals/array_parser.h
  sjparser/internals/default_value.h
)
set_target_properties(sjparser PROPERTIES OUTPUT_NAME sjparser_static)

setup_compilation_options(sjparser)

target_link_libraries(sjparser PUBLIC YAJL::YAJL)

target_include_directories(sjparser PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
  $<INSTALL_INTERFACE:include>
)

include(format)
add_format_target(sjparser)

add_library(sjparser::sjparser ALIAS sjparser)

if(SJPARSER_BUILD_SHARED_LIBRARY)
  add_library(sjparser_shared SHARED $<TARGET_PROPERTY:sjparser,SOURCES>)
  set_target_properties(sjparser_shared PROPERTIES OUTPUT_NAME sjparser)

  setup_compilation_options(sjparser_shared)

  target_link_libraries(sjparser_shared PUBLIC YAJL::YAJL)

  target_include_directories(sjparser_shared PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
    $<INSTALL_INTERFACE:include>
  )

  add_library(sjparser::sjparser_shared ALIAS sjparser_shared)
endif()

if(STANDALONE_BUILD)
  install(TARGETS sjparser sjparser_shared
    EXPORT sjparser-targets
    DESTINATION lib/sjparser
  )
  install(DIRECTORY sjparser DESTINATION include FILES_MATCHING PATTERN "*.h")

  set(PACKAGE_LOCATION lib/cmake/sjparser)
  install(FILES sjparser-config.cmake DESTINATION ${PACKAGE_LOCATION})
  install(EXPORT sjparser-targets
    NAMESPACE sjparser::
    DESTINATION ${PACKAGE_LOCATION}
  )

  add_subdirectory(documentation)
endif()
