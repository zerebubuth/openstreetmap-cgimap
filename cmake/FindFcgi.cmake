find_package(PkgConfig)
pkg_check_modules(PC_Fcgi QUIET fcgi)

find_path(Fcgi_INCLUDE_DIR
  NAMES fcgio.h
  PATHS ${PC_Fcgi_INCLUDE_DIRS}
  PATH_SUFFIXES fcgi
)
find_library(Fcgi_LIBRARY
  NAMES fcgi
  PATHS ${PC_Fcgi_LIBRARY_DIRS}
)

set(Fcgi_VERSION ${PC_Fcgi_VERSION})
set(Fcgi_VERSION_STRING ${Fcgi_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Fcgi 
    FOUND_VAR Fcgi_FOUND
    REQUIRED_VARS
        Fcgi_LIBRARY
        Fcgi_INCLUDE_DIR
    VERSION_VAR Fcgi_VERSION
)

if(Fcgi_FOUND)
  set(Fcgi_LIBRARIES ${Fcgi_LIBRARY})
  set(Fcgi_INCLUDE_DIRS ${Fcgi_INCLUDE_DIR})
  set(Fcgi_DEFINITIONS ${PC_Fcgi_CFLAGS_OTHER})
endif()

if(Fcgi_FOUND AND NOT TARGET Fcgi::Fcgi)
  add_library(Fcgi::Fcgi UNKNOWN IMPORTED)
  set_target_properties(Fcgi::Fcgi PROPERTIES
    IMPORTED_LOCATION "${Fcgi_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Fcgi_INCLUDE_DIR}"
    INTERFACE_COMPILE_OPTIONS "${PC_Fcgi_CFLAGS_OTHER}"
    VERSION "${Fcgi_VERSION}"
  )
endif()

mark_as_advanced(
    Fcgi_INCLUDE_DIR
    Fcgi_LIBRARY
    Fcgi_VERSION
    Fcgi_VERSION_STRING
)
