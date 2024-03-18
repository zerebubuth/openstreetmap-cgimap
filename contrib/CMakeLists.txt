add_subdirectory(catch2)
add_subdirectory(libxml++)

target_link_libraries(catch2 INTERFACE cgimap_common_compiler_options)
target_link_libraries(libxml++ INTERFACE cgimap_common_compiler_options)
