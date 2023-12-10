# add profiling compiler flags
function(target_add_profiling target)
    target_compile_options(${target} INTERFACE -pg)
    target_link_libraries(${target} INTERFACE -pg)
endfunction()
