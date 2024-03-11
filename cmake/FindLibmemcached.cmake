find_package(PkgConfig)
pkg_check_modules(PC_LIBMEMCACHED QUIET libmemcached)

find_path(Libmemcached_INCLUDE_DIR
  NAMES libmemcached/memcached.h
  HINTS ${PC_LIBMEMCACHED_INCLUDE_DIRS}
  PATH_SUFFIXES libmemcached memcached
)
find_library(Libmemcached_LIBRARY
  NAMES memcached
  HINTS ${PC_LIBMEMCACHED_LIBRARY_DIRS}
)

set(Libmemcached_VERSION ${PC_LIBMEMCACHED_VERSION})


include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Libmemcached 
    FOUND_VAR Libmemcached_FOUND
    REQUIRED_VARS
        Libmemcached_LIBRARY
        Libmemcached_INCLUDE_DIR
    VERSION_VAR Libmemcached_VERSION
)

if(Libmemcached_FOUND)
    if(Libmemcached_VERSION STREQUAL "")
        if(EXISTS "${LIBMEMCACHED_INCLUDE_DIR}/libmemcached/configure.h")
            file(READ "${LIBMEMCACHED_INCLUDE_DIR}/libmemcached/configure.h" _MEMCACHE_VERSION_CONTENTS)
        endif()
        if(EXISTS "${LIBMEMCACHED_INCLUDE_DIR}/libmemcached-1.0/configure.h")
            file(READ "${LIBMEMCACHED_INCLUDE_DIR}/libmemcached-1.0/configure.h" _MEMCACHE_VERSION_CONTENTS)
        endif()
        if(_MEMCACHE_VERSION_CONTENTS)
            string(REGEX REPLACE ".*#define LIBMEMCACHED_VERSION_STRING \"([0-9.]+)\".*" "\\1" Libmemcached_VERSION "${_MEMCACHE_VERSION_CONTENTS}")
        endif()
    endif()
endif()

set(Libmemcached_VERSION_STRING ${Libmemcached_VERSION})

if(Libmemcached_FOUND)
  set(Libmemcached_LIBRARIES ${Libmemcached_LIBRARY})
  set(Libmemcached_INCLUDE_DIRS ${Libmemcached_INCLUDE_DIR})
  set(Libmemcached_DEFINITIONS ${PC_LIBMEMCACHED_CFLAGS_OTHER})
endif()

if(Libmemcached_FOUND AND NOT TARGET Libmemcached::Libmemcached)
    add_library(Libmemcached::Libmemcached UNKNOWN IMPORTED)
    set_target_properties(Libmemcached::Libmemcached PROPERTIES
        IMPORTED_LOCATION "${Libmemcached_LIBRARY}"
        INTERFACE_INCLUDE_DIRECTORIES "${Libmemcached_INCLUDE_DIR}"
        INTERFACE_COMPILE_OPTIONS "${PC_LIBMEMCACHED_CFLAGS_OTHER}"
        VERSION "${Libmemcached_VERSION}")
endif()

mark_as_advanced(
    Libmemcached_LIBRARY
    Libmemcached_INCLUDE_DIR
    Libmemcached_INCLUDE_DIRS
    Libmemcached_VERSION
    Libmemcached_VERSION_STRING
)
