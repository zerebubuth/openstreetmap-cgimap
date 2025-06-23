# Disable linters during build of contrib libraries
disable_build_lint()

if(USE_BUNDLED_CATCH2)
    add_subdirectory(Catch2 EXCLUDE_FROM_ALL)
    target_link_libraries(Catch2 PUBLIC cgimap_common_compiler_options)
    target_link_libraries(Catch2WithMain PUBLIC cgimap_common_compiler_options)
endif()

add_subdirectory(libxml++ EXCLUDE_FROM_ALL)
target_link_libraries(cgimap_libxml++ PUBLIC cgimap_common_compiler_options)

add_subdirectory(sjparser EXCLUDE_FROM_ALL)


# Propagate cmake module path changes from subprojects to parent
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
