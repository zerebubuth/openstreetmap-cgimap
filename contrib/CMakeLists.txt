# Disable linters during build of contrib libraries
disable_build_lint()

add_subdirectory(catch2)
add_subdirectory(libxml++)

target_link_libraries(catch2 INTERFACE cgimap_common_compiler_options)
target_link_libraries(libxml++ PUBLIC cgimap_common_compiler_options)
