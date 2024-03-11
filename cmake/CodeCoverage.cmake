# add code coverage compiler flags
function(target_add_code_coverage target)
    target_compile_options(${target} INTERFACE --coverage -fprofile-abs-path -fprofile-arcs -ftest-coverage)
    target_link_libraries(${target} INTERFACE gcov)
endfunction()
