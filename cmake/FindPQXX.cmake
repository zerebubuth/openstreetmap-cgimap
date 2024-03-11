cmake_minimum_required(VERSION 3.14)

find_package(PostgreSQL REQUIRED)
if(PostgreSQL_FOUND)
    find_package(PkgConfig)
    pkg_check_modules(PC_PQXX QUIET libpqxx)

    find_path(PQXX_INCLUDE_DIR
        NAMES pqxx/pqxx
        HINTS ${PC_PQXX_INCLUDE_DIRS}
        PATH_SUFFIXES libpqxx pqxx
        DOC "Path to pqxx/pqxx include directory."
    )

    find_library(PQXX_LIBRARY
        NAMES libpqxx pqxx
        HINTS ${PC_PQXX_LIBRARY_DIRS}
        DOC "Location of libpqxx library"
    )
endif()

set(PQXX_VERSION ${PC_PQXX_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(PQXX
    FOUND_VAR PQXX_FOUND
    REQUIRED_VARS 
        PQXX_LIBRARY 
        PQXX_INCLUDE_DIR
    VERSION_VAR PQXX_VERSION)

if(PQXX_FOUND)
    if(PQXX_VERSION STREQUAL "")
        if(EXISTS "${PQXX_INCLUDE_DIR}/pqxx/version.hxx")
            file(STRINGS "${PQXX_INCLUDE_DIR}/pqxx/version.hxx" PQXX_VERSION_DEFINE REGEX "PQXX_VERSION ")
            string(REGEX MATCH "[0-9]+\.[0-9]+\.[0-9]+" PQXX_VERSION ${PQXX_VERSION_DEFINE})
        endif()
    endif()
endif()

set(PQXX_VERSION_STRING ${PC_PQXX_VERSION})

if(PQXX_FOUND)
    set(PQXX_INCLUDE_DIRS "${PQXX_INCLUDE_DIR};${PostgreSQL_INCLUDE_DIR}" CACHE STRING "Include directories for PostGreSQL C++ library"  FORCE )
    set(PQXX_INCLUDE_DIRECTORIES "${PQXX_INCLUDE_DIRS}" CACHE STRING "Include directories for PostGreSQL C++ library"  FORCE )
    set(PQXX_LIBRARIES "${PQXX_LIBRARY};${PostgreSQL_LIBRARIES}" CACHE STRING "Link libraries for PostGreSQL C++ interface" FORCE )
    set(PQXX_DEFINITIONS ${PC_PQXX_CFLAGS_OTHER})
endif()

if(PQXX_FOUND AND NOT TARGET PQXX::PQXX)
    add_library(PQXX::PQXX UNKNOWN IMPORTED)
    set_target_properties(PQXX::PQXX PROPERTIES
        IMPORTED_LOCATION "${PQXX_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${PQXX_INCLUDE_DIR}"
        INTERFACE_COMPILE_OPTIONS "${PC_PQXX_CFLAGS_OTHER}"
        VERSION "${PQXX_VERSION}")
    target_link_libraries(PQXX::PQXX INTERFACE PostgreSQL::PostgreSQL)
endif()

mark_as_advanced(FORCE 
    PQXX_INCLUDE_DIRS
    PQXX_INCLUDE_DIRECTORIES
    PQXX_LIBRARIES
    PQXX_VERSION
    PQXX_VERSION_STRING
)
