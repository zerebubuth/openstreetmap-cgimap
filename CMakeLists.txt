cmake_minimum_required(VERSION 3.14)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_DISABLE_SOURCE_CHANGES ON)
set(CMAKE_DISABLE_IN_SOURCE_BUILD ON)
set(CMAKE_CXX_STANDARD 20)

################
# openstreetmap-cgimap project
################
string(TIMESTAMP CURRENT_TIMESTAMP %y%m%d%H%M UTC)

project(openstreetmap-cgimap
        LANGUAGES CXX
        VERSION "2.0.1.${CURRENT_TIMESTAMP}"
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
option(ENABLE_CRYPTOPP "Enable Crypto++ library" ON)
option(ENABLE_BROTLI "Enable Brotli library" ON)
option(ENABLE_FMT_HEADER "Enable FMT header only mode" ON)
option(ENABLE_COVERAGE "Compile with coverage info collection" OFF)
option(ENABLE_PROFILING "Compile with profiling" OFF)
option(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
option(ENABLE_INSTALL "Enable installation" ON)
option(ENABLE_PGVIRTUALENV "Run unit tests using pg_virtualenv" ON)
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    option(BUILD_TESTING "Compile test suite" ON)
else()
    option(BUILD_TESTING "Compile test suite" OFF)
endif()



include(CTest)
include(AutotoolsCompatibilityDefinitions)
include(BuildLint)

#########################
# common compiler options
#########################
add_library(cgimap_common_compiler_options INTERFACE)

target_compile_features(cgimap_common_compiler_options INTERFACE cxx_std_20)
target_compile_definitions(cgimap_common_compiler_options INTERFACE CMAKE=1
                           BOOST_BIND_GLOBAL_PLACEHOLDERS)
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

find_package(Boost 1.43 REQUIRED COMPONENTS program_options)
target_compile_definitions(cgimap_common_compiler_options INTERFACE
    HAVE_BOOST=$<BOOL:${Boost_FOUND}>
    HAVE_BOOST_PROGRAM_OPTIONS=$<BOOL:${Boost_PROGRAM_OPTIONS_FOUND}>)

find_package(ZLIB REQUIRED)
target_compile_definitions(cgimap_common_compiler_options INTERFACE
    HAVE_LIBZ=$<BOOL:${ZLIB_FOUND}>)

if (ENABLE_CRYPTOPP)
     find_package(CryptoPP REQUIRED)
endif()
target_compile_definitions(cgimap_common_compiler_options INTERFACE
    HAVE_CRYPTOPP=$<BOOL:${CryptoPP_FOUND}>)

find_package(YAJL 2 REQUIRED)
target_compile_definitions(cgimap_common_compiler_options INTERFACE
    HAVE_YAJL=$<BOOL:${YAJL_FOUND}>)

if(ENABLE_BROTLI)
    find_package(Brotli COMPONENTS encoder decoder common REQUIRED)
endif()
target_compile_definitions(cgimap_common_compiler_options INTERFACE
    HAVE_BROTLI=$<BOOL:${Brotli_FOUND}>)

find_package(Fcgi REQUIRED)
target_compile_definitions(cgimap_common_compiler_options INTERFACE
    HAVE_FCGI=$<BOOL:${Fcgi_FOUND}>)

find_package(fmt 6.0 REQUIRED)
target_link_libraries(cgimap_common_compiler_options INTERFACE
    $<IF:$<BOOL:${ENABLE_FMT_HEADER}>,fmt::fmt-header-only,fmt::fmt>)


###########################
# source subdirectories
###########################
add_subdirectory(contrib)
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
    cgimap_apidb
    Boost::program_options
    PQXX::PQXX)


#############################################################
# Optional "clang-tidy" target
#############################################################

# BuildLint module already checks for clang-tidy
if(CLANG_TIDY)
    file(GLOB CT_CHECK_FILES src/*.cpp src/api06/*.cpp src/api06/changeset_upload/*.cpp src/api07/*.cpp
                             src/backend/apidb/*.cpp src/backend/apidb/changeset_upload/*.cpp
                             test/*.cpp test/*.hpp)

    add_custom_target(clang-tidy
        ${CLANG_TIDY}
        -extra-arg=-std=c++20
        -p ${CMAKE_BINARY_DIR}
        ${CT_CHECK_FILES}
    )
endif()

########################################
# Optional coverage report generation
########################################

if(ENABLE_COVERAGE)
    # Path for storing the coverage report
    set(COVERAGE_REPORT_DIR "${CMAKE_BINARY_DIR}/report")

    # Create target for generating the coverage report
    add_custom_target(generate_coverage_report
        COMMAND mkdir -p ${COVERAGE_REPORT_DIR}
        COMMAND lcov --base-directory ${CMAKE_CURRENT_SOURCE_DIR} --directory ${CMAKE_BINARY_DIR} --capture --output-file ${COVERAGE_REPORT_DIR}/coverage.info
        COMMAND lcov --remove ${COVERAGE_REPORT_DIR}/coverage.info '/usr/*' '${CMAKE_CURRENT_SOURCE_DIR}/contrib/catch2/*'
                '${CMAKE_CURRENT_SOURCE_DIR}/contrib/libxml++/*' --output-file ${COVERAGE_REPORT_DIR}/coverage_cleaned.info
        COMMAND genhtml -o ${COVERAGE_REPORT_DIR}/ ${COVERAGE_REPORT_DIR}/coverage_cleaned.info
        COMMAND lcov --list ${COVERAGE_REPORT_DIR}/coverage_cleaned.info
        COMMENT "Generating code coverage report"
    )
endif()

#############################################################
# Install
#############################################################

if(ENABLE_INSTALL)
    include(GNUInstallDirs)

    set(CMAKE_INSTALL_DEFAULT_DIRECTORY_PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE)

    set(CGIMAP_INSTALL_INCLUDEDIR ${CMAKE_INSTALL_INCLUDEDIR}/${PROJECT_NAME})

    if(BUILD_SHARED_LIBS)
        set(CGIMAP_LIBS cgimap_core cgimap_fcgi cgimap_apidb cgimap_libxml++)
    endif()

    if(CMAKE_VERSION VERSION_GREATER_EQUAL 3.23.0)
        set(CGIMAP_HEADER_FILE_SET_INSTALL_ARTIFACTS
            FILE_SET HEADERS
                COMPONENT Development
                DESTINATION ${CGIMAP_INSTALL_INCLUDEDIR}
                EXCLUDE_FROM_ALL)
    endif()

    install(TARGETS openstreetmap_cgimap ${CGIMAP_LIBS}
        RUNTIME
            COMPONENT Runtime
        LIBRARY
            COMPONENT Libraries
        PUBLIC_HEADER
            COMPONENT Development
            DESTINATION ${CGIMAP_INSTALL_INCLUDEDIR}
            EXCLUDE_FROM_ALL
        PRIVATE_HEADER
            COMPONENT Development
            DESTINATION ${CGIMAP_INSTALL_INCLUDEDIR}
            EXCLUDE_FROM_ALL
        ${CGIMAP_HEADER_FILE_SET_INSTALL_ARTIFACTS})

    install(DIRECTORY doc/man1
        TYPE MAN
        COMPONENT Runtime)

    install(FILES README TYPE DOC
        COMPONENT Runtime)
endif()


#############################################################
# Debian packaging
#############################################################

set(CPACK_GENERATOR "DEB")
set(CPACK_PACKAGE_VERSION_MAJOR ${PROJECT_VERSION_MAJOR})
set(CPACK_PACKAGE_VERSION_MINOR ${PROJECT_VERSION_MINOR})
set(CPACK_PACKAGE_VERSION_PATCH ${PROJECT_VERSION_PATCH})
set(CPACK_PACKAGE_CONTACT "mmd.osm@gmail.com")
set(CPACK_PACKAGE_DESCRIPTION "A comprehensive list of supported endpoints can be found at https://github.com/zerebubuth/openstreetmap-cgimap")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "OpenStreetMap CGImap - Performance optimized implementation of selected parts of the OSM API")
set(CPACK_RESOURCE_FILE_README "${CMAKE_SOURCE_DIR}/README")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/COPYING")
set(CPACK_PACKAGING_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")
set(CPACK_STRIP_FILES "TRUE")
set(CPACK_VERBATIM_VARIABLES YES)

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "mmd <${CPACK_PACKAGE_CONTACT}>")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS YES)
set(CPACK_DEBIAN_ENABLE_COMPONENT_DEPENDS YES)
set(CPACK_DEBIAN_PACKAGE_CONTROL_STRICT_PERMISSION TRUE)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
# Set to YES to create individual packages for components instead of everything in one package
set(CPACK_DEB_COMPONENT_INSTALL NO)

set(CPACK_DEBIAN_RUNTIME_PACKAGE_NAME "${PROJECT_NAME}")
set(CPACK_DEBIAN_LIBRARIES_PACKAGE_NAME "${PROJECT_NAME}-libs")
set(CPACK_DEBIAN_DEVELOPMENT_PACKAGE_NAME "${PROJECT_NAME}-dev")

include(CPack)

cpack_add_component(Libraries
                    DISPLAY_NAME "openstreetmap-cgimap libraries"
                    DESCRIPTION "Libraries required to run openstreetmap-cgimap")

if(BUILD_SHARED_LIBS)
    set(CGIMAP_RUNTIME_COMPONENT_DEPENDENCIES Libraries)
endif()
cpack_add_component(Runtime
                    DISPLAY_NAME "openstreetmap-cgimap runtime binary"
                    DEPENDS ${CGIMAP_RUNTIME_COMPONENT_DEPENDENCIES})

cpack_add_component(Development
                    DISPLAY_NAME "openstreetmap-cgimap development headers"
                    DESCRIPTION "Header files required for development using openstreetmap-cgimap libraries"
                    DEPENDS Libraries)
