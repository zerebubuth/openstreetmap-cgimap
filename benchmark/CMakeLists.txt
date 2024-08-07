

find_package(benchmark REQUIRED)

add_executable(cgimap-benchmark)

target_link_libraries(cgimap-benchmark PRIVATE 
    benchmark::benchmark
    benchmark::benchmark_main)

target_link_libraries(cgimap-benchmark PRIVATE 
    cgimap_common_compiler_options
    cgimap_core
    cgimap_fcgi
    cgimap_apidb
    test_utils)

target_include_directories(cgimap-benchmark PRIVATE
    ../test)

target_sources(cgimap-benchmark PRIVATE
    benchmark.cpp
    
    ../test/test_database.cpp
    ../test/test_request.cpp
)



