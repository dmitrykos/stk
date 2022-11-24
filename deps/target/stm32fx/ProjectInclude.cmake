if (NOT ROOT_DIR)
    message(FATAL_ERROR "ROOT_DIR must be defined and point to the root of STK repository!")
endif()

# Base definitions
add_definitions(
    -DUSE_HAL_DRIVER
    -DHSE_VALUE=8000000
    -DOS_USE_TRACE_SEMIHOSTING_DEBUG
    -DTRACE
    -DUSE_FULL_ASSERT
)

# Base includes
include_directories(        
    ${ROOT_DIR}/deps/target/stm32fx/include
    ${ROOT_DIR}/deps/target/stm32fx/include/arm
    ${ROOT_DIR}/deps/target/stm32fx/include/cmsis
    ${ROOT_DIR}/deps/target/stm32fx/include/cortexm
    ${ROOT_DIR}/deps/target/stm32fx/include/diag
)

# Device-specific includes
if (BOARD_STM32F0DISCOVERY)
    add_definitions(
        -DSTM32F0
        -DSTM32F051x8
    )
    include_directories(${ROOT_DIR}/deps/target/stm32fx/include/stm32f0-hal)
    link_directories(${ROOT_DIR}/deps/target/stm32fx/src/stm32f0-ld)
elseif (BOARD_NUCLEOF103RB)
    add_definitions(
        -DSTM32F1
        -DSTM32F103xB
    )
    include_directories(${ROOT_DIR}/deps/target/stm32fx/include/stm32f1-hal)
    link_directories(${ROOT_DIR}/deps/target/stm32fx/src/stm32f1-ld)
elseif (BOARD_STM32F407DISC1)
    add_definitions(
        -DSTM32F4
        -DSTM32F407xx
    )
    include_directories(${ROOT_DIR}/deps/target/stm32fx/include/stm32f4-hal)
    link_directories(${ROOT_DIR}/deps/target/stm32fx/src/stm32f4-ld)
endif()

# Device specific include file
set(_STK_DEVICE_INC "cmsis_device.h" CACHE STRING "_STK_DEVICE_INC" FORCE)