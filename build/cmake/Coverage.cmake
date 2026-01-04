# 
#  SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
#  License: MIT License, see LICENSE for a full text.
# 

function(target_add_coverage_compiler_options _TARGET)
    target_compile_options(${_TARGET} PRIVATE "-O0" "-g" "--coverage" "-fprofile-arcs" "-ftest-coverage")
endfunction()

function(target_add_coverage_linker_options _TARGET)
    target_link_libraries(${TARGET_BINARY} gcov)
endfunction()
