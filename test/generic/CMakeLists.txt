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
file(GLOB TEST_SRC ${ROOT_DIR}/test/generic/*.cpp)
set(TEST_SRC ${TEST_SRC_MAIN} ${TEST_SRC_MAIN} ${TEST_SRC} ${TEST_SRC_STK})

# Deps
list(APPEND TARGET_DEPS stk cpputest)
list(APPEND TARGET_LIBS stk cpputest)

# Target STM32 development board
if (VENDOR_STM32)
    include(${ROOT_DIR}/deps/target/stm32fx/ProjectInclude.cmake)
    include(${ROOT_DIR}/deps/target/stm32fx/ProjectIncludeLib.cmake)
    set(LINKER_FLAGS "-Wl,-Map=${TARGET_NAME}.map")
    set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} ${LINKER_FLAGS}")
    set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} ${LINKER_FLAGS}")
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

# Add into tests
if (NOT ARM)
    add_test(NAME ${TARGET_BINARY} COMMAND ${TARGET_NAME})
endif()

install(TARGETS ${TARGET_BINARY} DESTINATION ${LIBRARY_OUTPUT_PATH})