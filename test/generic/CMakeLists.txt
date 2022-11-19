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

# Definitions
add_definitions(-D_STK_ASSERT_REDIRECT)

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
add_subdirectory(${ROOT_DIR}/deps/cpputest ${CMAKE_BINARY_DIR}/cpputest)

# Deps
list(APPEND TEST_DEPS cpputest)
list(APPEND TEST_LIBS cpputest)

# Target - STM32F407-DISK1 development board
if (TARGET_STM32F407DISC1)
    add_subdirectory(${ROOT_DIR}/deps/target/stm32f4 ${CMAKE_BINARY_DIR}/stm32f4)

    add_definitions(
        -DSTM32F407xx
        -DUSE_HAL_DRIVER
        -DHSE_VALUE=8000000
        -DOS_USE_TRACE_SEMIHOSTING_DEBUG
        -DTRACE
        -DUSE_FULL_ASSERT
    )
    
    include_directories(        
        ${ROOT_DIR}/deps/target/stm32f4/src
        ${ROOT_DIR}/deps/target/stm32f4/include
        ${ROOT_DIR}/deps/target/stm32f4/include/arm
        ${ROOT_DIR}/deps/target/stm32f4/include/cmsis
        ${ROOT_DIR}/deps/target/stm32f4/include/cortexm
        ${ROOT_DIR}/deps/target/stm32f4/include/diag
        ${ROOT_DIR}/deps/target/stm32f4/include/stm32f4-hal
    )
    
    link_directories(${ROOT_DIR}/deps/target/stm32f4/src)
    
    set(LINKER_FLAGS "-T mem.ld -T libs.ld -T sections.ld -Wl,-Map=${TARGET_NAME}.map")
    
    set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} ${LINKER_FLAGS}")
    set(CMAKE_CXX_LINK_FLAGS "${CMAKE_CXX_LINK_FLAGS} ${LINKER_FLAGS}")
    
    list(APPEND TEST_DEPS stm32f4)
    list(APPEND TEST_LIBS -Wl,--whole-archive stm32f4 -Wl,--no-whole-archive)
    
    set(BINARY ${TARGET_NAME}.elf)
else()
    set(BINARY ${TARGET_NAME})
endif()

# Make target
include_directories(${TEST_INC_DIRS})
add_executable(${BINARY} ${TEST_SRC})
if (ARM)
    # Show size stats of the binary
    add_custom_command(TARGET ${BINARY}
        POST_BUILD
        COMMAND ${CMAKE_SIZE} ${BINARY})

    # Export ELF to HEX and BIN additionally
    add_custom_command(TARGET ${BINARY}
        POST_BUILD
        COMMAND ${CMAKE_OBJCOPY} -O ihex ${BINARY} ${TARGET_NAME}.hex
        COMMAND ${CMAKE_OBJCOPY} -O binary ${BINARY} ${TARGET_NAME}.bin)
endif()

# Link deps
add_dependencies(${BINARY} ${TEST_DEPS})
target_link_libraries(${BINARY} ${TEST_LIBS})

install(TARGETS ${BINARY} DESTINATION ${LIBRARY_OUTPUT_PATH})
