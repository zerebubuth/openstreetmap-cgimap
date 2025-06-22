# Disable linters during build of contrib libraries
disable_build_lint()

add_subdirectory(Catch2)
add_subdirectory(libxml++)
add_subdirectory(sjparser)

target_link_libraries(Catch2 PUBLIC cgimap_common_compiler_options)
target_link_libraries(Catch2WithMain PUBLIC cgimap_common_compiler_options)
target_link_libraries(cgimap_libxml++ PUBLIC cgimap_common_compiler_options)

# Propagate cmake module path changes from subprojects to parent
set(CMAKE_MODULE_PATH "${CMAKE_MODULE_PATH}" PARENT_SCOPE)
