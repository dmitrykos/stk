# 
#  SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
#  License: MIT License, see LICENSE for a full text.
# 

set(TARGET_NAME test-switch)
project(${TARGET_NAME})
message(STATUS "- " ${TARGET_NAME})

# Definitions
add_definitions(-D_STK_ASSERT_REDIRECT)

# Proect root dir
set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Includes
include_directories( 
    ${ROOT_DIR}/stk/include
    ${ROOT_DIR}/test
    ${ROOT_DIR}/test/generic
)

# Sources
file(GLOB TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
set(TEST_SRC ${TEST_SRC_MAIN} ${TEST_SRC})

# Finalize
include(${ROOT_DIR}/build/cmake/MakeExecutable.cmake)