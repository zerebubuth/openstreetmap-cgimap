find_package(GTest REQUIRED)

add_executable(sjparser_tests
  src/value.cpp
  src/array.cpp
  src/sarray.cpp
  src/object.cpp
  src/scustomobject.cpp
  src/sautoobject.cpp
  src/parsing_error.cpp
  src/yajl_parser.cpp
  src/dispatcher.cpp
  src/map.cpp
  src/smap.cpp
  src/ignore.cpp
  src/parser.cpp
  src/member.cpp
)
set_property(TARGET sjparser_tests PROPERTY SJPARSER_HEADERS
  src/test_parser.h
)

setup_compilation_options(sjparser_tests)
target_link_libraries(sjparser_tests PUBLIC sjparser GTest::GTest GTest::Main)

include(format)
add_format_target(sjparser_tests)

add_test(sjparser sjparser_tests)
