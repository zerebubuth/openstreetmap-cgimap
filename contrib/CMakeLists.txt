# Disable linters during build of contrib libraries
disable_build_lint()

add_subdirectory(catch2)
add_subdirectory(libxml++)
add_subdirectory(sjparser)

target_link_libraries(catch2 INTERFACE cgimap_common_compiler_options)
target_link_libraries(cgimap_libxml++ PUBLIC cgimap_common_compiler_options)
