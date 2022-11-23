# 
#  SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
#  License: MIT License, see LICENSE for a full text.
# 

project(global)

cmake_minimum_required(VERSION 3.14)

# Vendor specific targeting
option(VENDOR_STM32 "Target STM32 devices" OFF)

# Vendor-specific boards
if (VENDOR_STM32)
    option(BOARD_STM32F407DISC1 "Target STM32F407G-DISC1 (Arm Cortex-M4) development board" OFF)
    option(BOARD_STM32F0DISCOVERY "Target STM32F0DISCOVERY (Arm Cortex-M0) development board" OFF)
else()
    set(BOARD_STM32F407DISC1 OFF)
    set(BOARD_STM32F0DISCOVERY OFF)
endif()

# Sanity check: board should match its CPU
if (BOARD_STM32F407DISC1 AND NOT TARGET_CORTEX_M4)
    message(FATAL_ERROR "Target Arm Cortex-M4 (default) CPU for BOARD_STM32F407DISC1!")
endif()
if (BOARD_STM32F0DISCOVERY AND NOT TARGET_CORTEX_M0)
    message(FATAL_ERROR "Target Arm Cortex-M0 CPU for BOARD_STM32F0DISCOVERY!")
endif()

# Add static lib option
option(BUILD_LIB      "build Library"  OFF)
#option(BUILD_EXAMPLES "build Examples" OFF)
option(BUILD_TESTS    "build Tests"    OFF)

# Include CTest for running tests
if (BUILD_TESTS)
    include(CTest)
endif()

# Detect compiler
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CLANGC TRUE)
    set(GCC TRUE)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(GCC TRUE)
endif()

# Create stk_config.h
if (WIN32)
    set(_STK_ARCH_X86_WIN32 TRUE)
    message(STATUS "Platform define: _STK_ARCH_X86_WIN32")
elseif (ARM)
    set(_STK_ARCH_ARM_CORTEX_M TRUE)
    message(STATUS "Platform define: _STK_ARCH_ARM_CORTEX_M")
endif()
set(STK_CONFIG_INC ${CMAKE_CURRENT_BINARY_DIR}/include/stk_config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/stk_config.h.in ${STK_CONFIG_INC})
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)
message(STATUS "Config file: " ${STK_CONFIG_INC})

# Output binaries path
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

# Add projects
add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/deps)
if (BUILD_LIB)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/stk)
endif()
if (BUILD_EXAMPLES)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/build/example/cmake)
endif()
if (BUILD_TESTS)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()