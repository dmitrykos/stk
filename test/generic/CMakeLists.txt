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

# Device-specific targets
if (ARM)
    set(OPT_STM32 ON)
else()
    set(OPT_STM32 OFF)
endif()
option(TARGET_STM32F407DISC1 "target STM32F407G-DISC1 development board" ${OPT_STM32})
option(TARGET_STM32F0DISCOVERY "target STM32F0DISCOVERY development board" OFF)

# Definitions
add_definitions(-D_STK_ASSERT_REDIRECT)

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
file(GLOB TEST_SRC_GENERIC ${ROOT_DIR}/test/generic/*.cpp)
file(GLOB TEST_SRC_STK ${ROOT_DIR}/stk/src/*.cpp)
set(TEST_SRC ${TEST_SRC_MAIN} ${TEST_SRC_MAIN} ${TEST_SRC_GENERIC} ${TEST_SRC_STK})

# CPPUTEST lib
add_subdirectory(${ROOT_DIR}/deps/cpputest ${CMAKE_TARGET_BINARY_DIR}/cpputest)

# Deps
list(APPEND TARGET_DEPS cpputest)
list(APPEND TARGET_LIBS cpputest)

# Target - STM32F407-DISK1 development board
if (TARGET_STM32F407DISC1 OR TARGET_STM32F0DISCOVERY)
    include(${ROOT_DIR}/deps/target/stm32fx/ProjectInclude.cmake)
    set(TARGET_BINARY ${TARGET_NAME}.elf)
else()
    set(TARGET_BINARY ${TARGET_NAME})
endif()

# Make target
add_executable(${TARGET_BINARY} ${TEST_SRC})
include(${ROOT_DIR}/deps/target/PostBuild.cmake)

# Link deps
add_dependencies(${TARGET_BINARY} ${TARGET_DEPS})
target_link_libraries(${TARGET_BINARY} ${TARGET_LIBS})

install(TARGETS ${TARGET_BINARY} DESTINATION ${LIBRARY_OUTPUT_PATH})
