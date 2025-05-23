# 
#  SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
#  License: MIT License, see LICENSE for a full text.
# 

set(TARGET_NAME stk)
project(${TARGET_NAME})
message(STATUS "- " ${TARGET_NAME})

# Includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

# Add STM32 specific includes
if (VENDOR_STM32)
    set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../deps/target/stm32fx/ProjectInclude.cmake)
elseif (RISCV)
    set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/..)
    include(${CMAKE_CURRENT_SOURCE_DIR}/../deps/target/risc-v/ProjectInclude.cmake)
endif()

# Sources
file(GLOB STK_LIB_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp)
file(GLOB STK_LIB_SRC_ARM_CORTEX_M ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/arm/cortex-m/*.cpp)
file(GLOB STK_LIB_SRC_RISC_V ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/risc-v/*.cpp)
file(GLOB STK_LIB_SRC_WIN32 ${CMAKE_CURRENT_SOURCE_DIR}/src/arch/x86/win32/*.cpp)
set(STK_LIB_SRC ${STK_LIB_SRC} ${STK_LIB_SRC_ARM_CORTEX_M} ${STK_LIB_SRC_RISC_V} ${STK_LIB_SRC_WIN32})

# Target
add_library(${TARGET_NAME} STATIC ${STK_LIB_SRC})

# Tests
if (BUILD_TESTS)
    # Coverage report
    if (ENABLE_COVERAGE AND GCC AND NOT CMAKE_CROSSCOMPILING)
        target_add_coverage_compiler_options(${TARGET_NAME})
    endif()
endif()

install(TARGETS ${TARGET_NAME} DESTINATION ${LIBRARY_OUTPUT_PATH})