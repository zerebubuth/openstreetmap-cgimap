##################
# cgimap_staticxml
##################
add_library(cgimap_staticxml)

target_include_directories(cgimap_staticxml PUBLIC
    ../../../include)

target_sources(cgimap_staticxml PRIVATE
    staticxml.cpp)

target_link_libraries(cgimap_staticxml
    cgimap_common_compiler_options
    LibXml2::LibXml2
    cgimap_core)
