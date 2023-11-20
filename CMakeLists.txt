cmake_minimum_required(VERSION 3.14)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)

################
# CGImap project
################
project(CGImap LANGUAGES CXX
               VERSION 0.8.10
               DESCRIPTION "CGImap is a C++ implementation of some parts of the OpenStreetMap API as a FastCGI process."
               HOMEPAGE_URL "https://github.com/zerebubuth/openstreetmap-cgimap")
set(CMAKE_PROJECT_BUGREPORT_URL "https://github.com/zerebubuth/openstreetmap-cgimap/issues")


if (PROJECT_SOURCE_DIR STREQUAL PROJECT_BINARY_DIR)
    message(FATAL_ERROR "In-source builds are not allowed, please use a separate build directory like `mkdir build && cd build && cmake ..`")
endif()

# copy final library files to binary dir root
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}")

# default to release build
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

###############
# build options
###############
option(ENABLE_APIDB "Enable APIDB backend, as used by the OSM servers" ON)
option(ENABLE_YAJL "Enable JSON output with the YAJL library" ON)
option(ENABLE_FMT_HEADER "Enable FMT header only mode" ON)
option(ENABLE_COVERAGE "Compile with coverage info collection" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(BUILD_TESTING "Compile test suite" ON)
else()
    option(BUILD_TESTING "Compile test suite" OFF)
endif()



include(CTest)
include(AutotoolsCompatibilityDefinitions)

#########################
# common compiler options
#########################
add_library(cgimap_common_compiler_options INTERFACE)

target_compile_features(cgimap_common_compiler_options INTERFACE cxx_std_17)
target_compile_definitions(cgimap_common_compiler_options INTERFACE "ENABLE_APIDB=$<BOOL:${ENABLE_APIDB}>")
target_compile_definitions(cgimap_common_compiler_options INTERFACE CMAKE=1)
target_add_autotools_compatibility_definitions(cgimap_common_compiler_options)

if(ENABLE_COVERAGE)
    include(CodeCoverage)
    target_add_code_coverage(cgimap_common_compiler_options)
endif()


##############
# dependencies
##############
find_package(PQXX 6.0 REQUIRED)
find_package(LibXml2 2.6.31 REQUIRED)
find_package(Libmemcached REQUIRED)
find_package(Argon2 REQUIRED)

find_package(Boost 1.43 REQUIRED COMPONENTS program_options)
target_compile_definitions(cgimap_common_compiler_options INTERFACE
    HAVE_BOOST=$<BOOL:${Boost_FOUND}>
    HAVE_BOOST_PROGRAM_OPTIONS=$<BOOL:${Boost_PROGRAM_OPTIONS_FOUND}>)

find_package(ZLIB REQUIRED)
target_compile_definitions(cgimap_common_compiler_options INTERFACE
    HAVE_LIBZ=$<BOOL:${ZLIB_FOUND}>)

find_package(CryptoPP REQUIRED)
target_compile_definitions(cgimap_common_compiler_options INTERFACE
    HAVE_CRYPTOPP=$<BOOL:${CryptoPP_FOUND}>)

if(ENABLE_YAJL)
    find_package(YAJL REQUIRED)
endif()
target_compile_definitions(cgimap_common_compiler_options INTERFACE
    HAVE_YAJL=$<BOOL:${YAJL_FOUND}>
    HAVE_YAJL2=$<VERSION_GREATER_EQUAL:${YAJL_VERSION},2>)

find_package(Fcgi REQUIRED)
target_compile_definitions(cgimap_common_compiler_options INTERFACE
    HAVE_FCGI=$<BOOL:${Fcgi_FOUND}>)

find_package(fmt 6.0 REQUIRED)
target_link_libraries(cgimap_common_compiler_options INTERFACE
    $<IF:$<BOOL:${ENABLE_FMT_HEADER}>,fmt::fmt-header-only,fmt::fmt>)


###########################
# source subdirectories
###########################
add_subdirectory(src)
add_subdirectory(test)


#################################
# openstreetmap-cgimap executable
#################################
add_executable(openstreetmap_cgimap)
set_target_properties(openstreetmap_cgimap PROPERTIES
    OUTPUT_NAME "openstreetmap-cgimap")

target_sources(openstreetmap_cgimap PRIVATE
    src/main.cpp)

target_link_libraries(openstreetmap_cgimap
    cgimap_common_compiler_options
    cgimap_core
    cgimap_fcgi
    cgimap_staticxml
    $<$<BOOL:${ENABLE_APIDB}>:cgimap_apidb>
    Boost::program_options
    PQXX::PQXX)
