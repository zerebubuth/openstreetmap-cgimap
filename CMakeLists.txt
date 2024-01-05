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
option(ENABLE_PROFILING "Compile with profiling" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" ON)
option(ENABLE_INSTALL "Enable installation" ON)
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

if(ENABLE_PROFILING)
    include(Profiling)
    target_add_profiling(cgimap_common_compiler_options)
endif()


##############
# dependencies
##############
find_package(Threads REQUIRED)
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


#############################################################
# Install
#############################################################

if (ENABLE_INSTALL)
  install(TARGETS openstreetmap_cgimap
          PERMISSIONS OWNER_EXECUTE OWNER_WRITE OWNER_READ GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
          DESTINATION "bin")

  if (BUILD_SHARED_LIBS)
    install(TARGETS cgimap_core DESTINATION lib)
    install(TARGETS cgimap_fcgi DESTINATION lib)
    install(TARGETS cgimap_staticxml DESTINATION lib)
    if (ENABLE_APIDB)
      install(TARGETS cgimap_apidb DESTINATION lib)
    endif()
  endif()
endif()


#############################################################
# Debian packaging
#############################################################

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_DEBIAN_PACKAGE_NAME "openstreetmap-cgimap")
set(CPACK_PACKAGE_CONTACT "mmd.osm@gmail.com")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "mmd.osm@gmail.com")
set(CPACK_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
    OWNER_READ OWNER_WRITE OWNER_EXECUTE
    GROUP_READ GROUP_EXECUTE
    WORLD_READ WORLD_EXECUTE
)
set(CPACK_PACKAGE_DESCRIPTION "A comprehensive list of supported endpoints can be found at https://github.com/zerebubuth/openstreetmap-cgimap")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OpenStreetMap CGImap - Performance optimized implementation of selected parts of the OSM API")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
set(CPACK_PACKAGING_INSTALL_PREFIX ${CMAKE_INSTALL_PREFIX})
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/zerebubuth/openstreetmap-cgimap")
set(CPACK_STRIP_FILES "TRUE")
INCLUDE(CPack)
