# 
#  SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
#  License: MIT License, see LICENSE for a full text.
# 

set(TARGET_NAME stm32fx)
project(${TARGET_NAME})
message(STATUS "- " ${TARGET_NAME})

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
    ${CMAKE_CURRENT_SOURCE_DIR}/include    
    ${CMAKE_CURRENT_SOURCE_DIR}/include/arm
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cmsis
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cortexm
    ${CMAKE_CURRENT_SOURCE_DIR}/include/diag
)

# Sources
file(GLOB STM32FX_SRS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB STM32FX_CORTEXM ${CMAKE_CURRENT_SOURCE_DIR}/src/cortexm/*.c)
file(GLOB STM32FX_DIAG ${CMAKE_CURRENT_SOURCE_DIR}/src/diag/*.c)
file(GLOB STM32FX_NEWLIB ${CMAKE_CURRENT_SOURCE_DIR}/src/newlib/*.c)

if (BOARD_STM32F407DISC1)
    # Definitions
    add_definitions(
        -DSTM32F4
        -DSTM32F407xx
    )
    
    # Includes
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/stm32f4-hal)
    
    # Sources
    file(GLOB STM32FX_CMSIS ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f4-cmsis/*.c)
    list(APPEND STM32FX_HAL ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f4-hal/stm32f4xx_hal_cortex.c)
    list(APPEND STM32FX_HAL ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f4-hal/stm32f4xx_hal_rcc.c)
    list(APPEND STM32FX_HAL ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f4-hal/stm32f4xx_hal_rcc_ex.c)
    list(APPEND STM32FX_HAL ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f4-hal/stm32f4xx_hal.c)
elseif (BOARD_STM32F0DISCOVERY)
    # Definitions
    add_definitions(
        -DSTM32F0
        -DSTM32F051x8
    )
    
    # Includes
    include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include/stm32f0-hal)
    
    # Sources
    file(GLOB STM32FX_CMSIS ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f0-cmsis/*.c)
    list(APPEND STM32FX_HAL ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f0-hal/stm32f0xx_hal_cortex.c)
    list(APPEND STM32FX_HAL ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f0-hal/stm32f0xx_hal_rcc.c)
    list(APPEND STM32FX_HAL ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f0-hal/stm32f0xx_hal_rcc_ex.c)
    list(APPEND STM32FX_HAL ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f0-hal/stm32f0xx_hal.c)
endif()

# Target
add_library(${TARGET_NAME} STATIC ${STM32FX_SRS} ${STM32FX_CMSIS} ${STM32FX_CORTEXM} ${STM32FX_DIAG} ${STM32FX_NEWLIB} ${STM32FX_HAL})

install(TARGETS ${TARGET_NAME} DESTINATION ${LIBRARY_OUTPUT_PATH})
