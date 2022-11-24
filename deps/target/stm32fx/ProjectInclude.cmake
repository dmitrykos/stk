if (NOT ROOT_DIR)
    message(FATAL_ERROR "ROOT_DIR must be defined and point to the root of STK repository!")
endif()

# Base includes
include_directories(        
    ${ROOT_DIR}/deps/target/stm32fx/include
    ${ROOT_DIR}/deps/target/stm32fx/include/arm
    ${ROOT_DIR}/deps/target/stm32fx/include/cmsis
    ${ROOT_DIR}/deps/target/stm32fx/include/cortexm
    ${ROOT_DIR}/deps/target/stm32fx/include/diag
)

# Device-specific includes
if (TARGET_CORTEX_M0)
    if (NOT TARGET_CPU_FAMILY)
        # Default CPU for STM32F0DISCOVERY board
        set(TARGET_CPU_FAMILY STM32F051x8 CACHE STRING "TARGET_CPU_FAMILY" FORCE)
    endif()
    add_definitions(-DSTM32F0)
    include_directories(${ROOT_DIR}/deps/target/stm32fx/include/stm32f0-hal)
    link_directories(${ROOT_DIR}/deps/target/stm32fx/src/stm32f0-ld)
elseif (TARGET_CORTEX_M3)
    if (NOT TARGET_CPU_FAMILY)
        # Default CPU for NUCLEO-F103RB board
        set(TARGET_CPU_FAMILY STM32F103xB CACHE STRING "TARGET_CPU_FAMILY" FORCE)
    endif()
    add_definitions(-DSTM32F1)
    include_directories(${ROOT_DIR}/deps/target/stm32fx/include/stm32f1-hal)
    link_directories(${ROOT_DIR}/deps/target/stm32fx/src/stm32f1-ld)
elseif (TARGET_CORTEX_M4)
    if (NOT TARGET_CPU_FAMILY)
        # Default CPU for STM32F407DISC1 board
        set(TARGET_CPU_FAMILY STM32F407xx CACHE STRING "TARGET_CPU_FAMILY" FORCE)
    endif()
    add_definitions(-DSTM32F4)
    include_directories(${ROOT_DIR}/deps/target/stm32fx/include/stm32f4-hal)
    link_directories(${ROOT_DIR}/deps/target/stm32fx/src/stm32f4-ld)
endif()

# Base definitions
add_definitions(
    -DUSE_HAL_DRIVER
    -DHSE_VALUE=8000000
    -DOS_USE_TRACE_SEMIHOSTING_DEBUG
    -DTRACE
    -DUSE_FULL_ASSERT
    -D${TARGET_CPU_FAMILY}
)

# Device specific include file
set(_STK_DEVICE_INC "cmsis_device.h" CACHE STRING "_STK_DEVICE_INC" FORCE)