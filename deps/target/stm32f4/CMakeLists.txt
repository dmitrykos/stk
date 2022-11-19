# 
#  SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
#  License: MIT License, see LICENSE for a full text.
# 

set(TARGET_NAME stm32f4)
project(${TARGET_NAME})

if (TARGET_STM32F407DISC1)
    add_definitions(
        -DSTM32F407xx
        -DUSE_HAL_DRIVER
        -DHSE_VALUE=8000000
        -DOS_USE_TRACE_SEMIHOSTING_DEBUG
        -DTRACE
        -DUSE_FULL_ASSERT
    )
endif()

# Includes
list(APPEND STM32F4_INC_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/include    
    ${CMAKE_CURRENT_SOURCE_DIR}/include/arm
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cmsis
    ${CMAKE_CURRENT_SOURCE_DIR}/include/cortexm
    ${CMAKE_CURRENT_SOURCE_DIR}/include/diag
    ${CMAKE_CURRENT_SOURCE_DIR}/include/stm32f4-hal
)
include_directories(${STM32F4_INC_DIRS})

# Sources
file(GLOB STM32F4_SRS ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)
file(GLOB STM32F4_CMSIS ${CMAKE_CURRENT_SOURCE_DIR}/src/cmsis/*.c)
file(GLOB STM32F4_CORTEXM ${CMAKE_CURRENT_SOURCE_DIR}/src/cortexm/*.c)
file(GLOB STM32F4_DIAG ${CMAKE_CURRENT_SOURCE_DIR}/src/diag/*.c)
file(GLOB STM32F4_NEWLIB ${CMAKE_CURRENT_SOURCE_DIR}/src/newlib/*.c)
list(APPEND STM32F4_HAL ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f4-hal/stm32f4xx_hal_cortex.c)
list(APPEND STM32F4_HAL ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f4-hal/stm32f4xx_hal_rcc.c)
list(APPEND STM32F4_HAL ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f4-hal/stm32f4xx_hal_rcc_ex.c)
list(APPEND STM32F4_HAL ${CMAKE_CURRENT_SOURCE_DIR}/src/stm32f4-hal/stm32f4xx_hal.c)

# Target
add_library(${TARGET_NAME} STATIC ${STM32F4_SRS} ${STM32F4_CMSIS} ${STM32F4_CORTEXM} ${STM32F4_DIAG} ${STM32F4_NEWLIB} ${STM32F4_HAL})

install(TARGETS ${TARGET_NAME} DESTINATION ${LIBRARY_OUTPUT_PATH})
