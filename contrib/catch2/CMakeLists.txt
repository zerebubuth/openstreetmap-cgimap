cmake_minimum_required(VERSION 3.14)

project(catch2
    LANGUAGES CXX)

add_library(catch2 INTERFACE)

if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.23.0)
    target_sources(catch2
        INTERFACE
            FILE_SET HEADERS
            BASE_DIRS single_include/
            FILES
                single_include/catch.hpp)
else()
    target_include_directories(catch2 INTERFACE
        single_include/)
endif()

