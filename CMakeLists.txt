# 
#  SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
#  License: MIT License, see LICENSE for a full text.
# 

project(global)

cmake_minimum_required(VERSION 2.6)

# Add static lib option
#option(BUILD_LIB      "build Library"  OFF)
#option(BUILD_EXAMPLES "build Examples" OFF)
option(BUILD_TESTS    "build Tests"    OFF)

# Detect compiler
if ("${CMAKE_CXX_COMPILER_ID}" MATCHES "Clang")
    set(CLANGC TRUE)
    set(GCC TRUE)
elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(GCC TRUE)
endif()

# Output binaries path
set(LIBRARY_OUTPUT_PATH ${CMAKE_BINARY_DIR}/lib)

# Add projects
if (BUILD_LIB)
	add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/build/lib/cmake)
endif()
if (BUILD_EXAMPLES)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/build/example/cmake)
endif()
if (BUILD_TESTS)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/test)
endif()