# 
#  SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
#  License: MIT License, see LICENSE for a full text.
# 

set(TARGET_NAME risc-v)
project(${TARGET_NAME})
message(STATUS "- " ${TARGET_NAME})

# Definitions, includes
set(ROOT_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../..)
include(ProjectInclude.cmake)

# Sources
file(GLOB RISCV_INIT_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/init/*.c)

# Target
add_library(${TARGET_NAME} STATIC ${RISCV_INIT_SRC})

install(TARGETS ${TARGET_NAME} DESTINATION ${LIBRARY_OUTPUT_PATH})