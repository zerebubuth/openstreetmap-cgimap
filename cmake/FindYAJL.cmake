find_package(PkgConfig)
pkg_check_modules(PC_YAJL QUIET yajl)

find_path(YAJL_INCLUDE_DIR
  NAMES yajl/yajl_gen.h
  PATHS ${PC_YAJL_INCLUDE_DIRS}
  PATH_SUFFIXES yajl
)
find_library(YAJL_LIBRARY
  NAMES yajl
  PATHS ${PC_YAJL_LIBRARY_DIRS}
)

set(YAJL_VERSION ${PC_YAJL_VERSION})
set(YAJL_VERSION_STRING ${YAJL_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(YAJL
    FOUND_VAR YAJL_FOUND
    REQUIRED_VARS 
        YAJL_LIBRARY
        YAJL_INCLUDE_DIR 
    VERSION_VAR YAJL_VERSION
)

if(YAJL_FOUND)
  set(YAJL_LIBRARIES ${YAJL_LIBRARY})
  set(YAJL_INCLUDE_DIRS ${YAJL_INCLUDE_DIR})
  set(YAJL_DEFINITIONS ${PC_YAJL_CFLAGS_OTHER})
endif()

if(YAJL_FOUND AND NOT TARGET YAJL::YAJL)
    add_library(YAJL::YAJL UNKNOWN IMPORTED)
    set_target_properties(YAJL::YAJL PROPERTIES
        IMPORTED_LOCATION "${YAJL_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${YAJL_INCLUDE_DIR}"
        INTERFACE_COMPILE_OPTIONS "${PC_YAJL_CFLAGS_OTHER}"
        VERSION "${YAJL_VERSION}"
    )
endif()

mark_as_advanced(
    YAJL_INCLUDE_DIR 
    YAJL_LIBRARY
    YAJL_VERSION
    YAJL_VERSION_STRING
)

