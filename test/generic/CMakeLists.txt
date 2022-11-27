# 
#  SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
#  License: MIT License, see LICENSE for a full text.
# 

set(TARGET_NAME test-generic)
project(${TARGET_NAME})
message(STATUS "- " ${TARGET_NAME})

# Definitions
# Note: Exclude memory leak detection as we do not allocated memory dynamically in STK.
add_definitions(-D_STK_ASSERT_REDIRECT -DCPPUTEST_MEM_LEAK_DETECTION_DISABLED -DCPPUTEST_STD_CPP_LIB_DISABLED)

# Proect root dir
set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../..)

# Includes
include_directories( 
    ${ROOT_DIR}/stk/include
    ${ROOT_DIR}/deps/cpputest/include
    ${ROOT_DIR}/test
    ${ROOT_DIR}/test/generic
)

# Sources
file(GLOB TEST_SRC_MAIN ${ROOT_DIR}/test/*.cpp)
file(GLOB TEST_SRC ${ROOT_DIR}/test/generic/*.cpp)
set(TEST_SRC ${TEST_SRC_MAIN} ${TEST_SRC_MAIN} ${TEST_SRC} ${TEST_SRC_STK})

# Deps
list(APPEND TARGET_DEPS cpputest)
list(APPEND TARGET_LIBS cpputest)

# Finalize
include(${ROOT_DIR}/build/cmake/MakeExecutable.cmake)