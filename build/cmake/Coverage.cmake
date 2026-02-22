# 
#  SuperTinyKernel™ (STK): Lightweight High-Performance Deterministic C++ RTOS for Embedded Systems.
# 
#  Source: https://github.com/SuperTinyKernel-RTOS
# 
#  Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>. All Rights Reserved.
#  License: MIT License, see LICENSE for a full text.
# 

function(target_add_coverage_compiler_options _TARGET)
    target_compile_options(${_TARGET} PRIVATE "-O0" "-g" "--coverage" "-fprofile-arcs" "-ftest-coverage")
endfunction()

function(target_add_coverage_linker_options _TARGET)
    target_link_libraries(${TARGET_BINARY} gcov)
endfunction()
