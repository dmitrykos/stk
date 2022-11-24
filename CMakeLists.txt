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

# Output binaries path
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

# Configure file folder (note: must be defined before inclusion of the projects so that they could use it)
set(_STK_DEVICE_INC)
include_directories(${CMAKE_CURRENT_BINARY_DIR}/include)

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

# Create stk_config.h (note: must be defined after inclusion of the projects to catch value of _STK_DEVICE_INC)
if (WIN32)
    set(_STK_ARCH_X86_WIN32 TRUE)
    message(STATUS "Platform define: _STK_ARCH_X86_WIN32")
elseif (ARM)
    set(_STK_ARCH_ARM_CORTEX_M TRUE)
    message(STATUS "Platform define: _STK_ARCH_ARM_CORTEX_M")
endif()
set(STK_CONFIG_INC ${CMAKE_CURRENT_BINARY_DIR}/include/stk_config.h)
configure_file(${CMAKE_CURRENT_SOURCE_DIR}/stk_config.h.in ${STK_CONFIG_INC})
message(STATUS "Config file: " ${STK_CONFIG_INC})

# Log CPU family
message(STATUS "Target CPU family: ${TARGET_CPU_FAMILY}")