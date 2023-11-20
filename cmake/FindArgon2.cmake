find_package(PkgConfig)
pkg_check_modules(PC_Argon2 QUIET libargon2)

find_path(Argon2_INCLUDE_DIR
  NAMES argon2.h
  PATHS ${PC_Argon2_INCLUDE_DIRS}
  PATH_SUFFIXES argon2
)
find_library(Argon2_LIBRARY
  NAMES argon2
  PATHS ${PC_Argon2_LIBRARY_DIRS}
)

set(Argon2_VERSION ${PC_Argon2_VERSION})
set(Argon2_VERSION_STRING ${Argon2_VERSION})

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(Argon2 
    FOUND_VAR Argon2_FOUND
    REQUIRED_VARS
        Argon2_LIBRARY
        Argon2_INCLUDE_DIR
    VERSION_VAR Argon2_VERSION
)

if(Argon2_FOUND)
  set(Argon2_LIBRARIES ${Argon2_LIBRARY})
  set(Argon2_INCLUDE_DIRS ${Argon2_INCLUDE_DIR})
  set(Argon2_DEFINITIONS ${PC_Argon2_CFLAGS_OTHER})
endif()

if(Argon2_FOUND AND NOT TARGET Argon2::Argon2)
  add_library(Argon2::Argon2 UNKNOWN IMPORTED)
  set_target_properties(Argon2::Argon2 PROPERTIES
    IMPORTED_LOCATION "${Argon2_LIBRARY}"
    INTERFACE_INCLUDE_DIRECTORIES "${Argon2_INCLUDE_DIR}"
    INTERFACE_COMPILE_OPTIONS "${PC_Argon2_CFLAGS_OTHER}"
    VERSION "${Argon2_VERSION}"
  )
endif()

mark_as_advanced(
    Argon2_INCLUDE_DIR
    Argon2_LIBRARY
    Argon2_VERSION
    Argon2_VERSION_STRING
)
