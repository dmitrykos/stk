# 
#  SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
#  License: MIT License, see LICENSE for a full text.
# 

project(target)

if (VENDOR_STM32)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/stm32fx)
endif()

if (RISCV)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/risc-v)
endif()