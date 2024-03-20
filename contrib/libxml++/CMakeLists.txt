cmake_minimum_required(VERSION 3.14)

project(libxml++
    LANGUAGES CXX
    DESCRIPTION "This library provides a C++ interface to XML files. It uses libxml2 to access the XML files."
    HOMEPAGE_URL "https://libxmlplusplus.github.io/libxmlplusplus/")

add_library(libxml++)

target_sources(libxml++
    PRIVATE
        parsers/exception.cpp
        parsers/internal_error.cpp
        parsers/parse_error.cpp
        parsers/parser.cpp
        parsers/saxparser.cpp
        parsers/wrapped_exception.cpp)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.23.0)
    target_sources(libxml++
        PUBLIC
            FILE_SET HEADERS
            FILES
                parsers/exception.hpp
                parsers/internal_error.hpp
                parsers/parse_error.hpp
                parsers/parser.hpp
                parsers/saxparser.hpp
                parsers/wrapped_exception.hpp)
else()
    target_include_directories(libxml++ PUBLIC
        ./)
endif()

target_link_libraries(libxml++ PUBLIC
    LibXml2::LibXml2)
