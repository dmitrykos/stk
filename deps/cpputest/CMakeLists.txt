# 
#  SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
#  License: MIT License, see LICENSE for a full text.
# 

set(TARGET_NAME cpputest)
project(${TARGET_NAME})

# Includes
list(APPEND CPPUTEST_INC_DIRS 
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
include_directories(${CPPUTEST_INC_DIRS})

# Sources
file(GLOB CPPUTEST_SRC_CPPUTEST ${CMAKE_CURRENT_SOURCE_DIR}/src/CppUTest/*.cpp)
file(GLOB CPPUTEST_SRC_CPPUTEST_EXT ${CMAKE_CURRENT_SOURCE_DIR}/src/CppUTestExt/*.cpp)
if (GCC)
    file(GLOB CPPUTEST_SRC_PLATFORM ${CMAKE_CURRENT_SOURCE_DIR}/src/Platforms/Gcc/*.cpp)
elseif (MSVC)
    file(GLOB CPPUTEST_SRC_PLATFORM ${CMAKE_CURRENT_SOURCE_DIR}/src/Platforms/VisualCpp/*.cpp)
endif()

# Target
add_library(${TARGET_NAME} STATIC ${CPPUTEST_SRC_CPPUTEST} ${CPPUTEST_SRC_CPPUTEST_EXT} ${CPPUTEST_SRC_PLATFORM})

install(TARGETS ${TARGET_NAME} DESTINATION ${LIBRARY_OUTPUT_PATH})
