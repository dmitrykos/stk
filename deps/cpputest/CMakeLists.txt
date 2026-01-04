# 
#  SuperTinyKernel (STK): minimalistic C++ thread scheduling kernel for Embedded Systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022-2026 Neutron Code Limited <stk@neutroncode.com>
#  License: MIT License, see LICENSE for a full text.
# 

set(TARGET_NAME cpputest)
project(${TARGET_NAME})
message(STATUS "- " ${TARGET_NAME})

# Definitions
# Note: Exclude memory leak detection as we do not allocated memory dynamically in STK.
add_definitions(-DCPPUTEST_MEM_LEAK_DETECTION_DISABLED -DCPPUTEST_STD_CPP_LIB_DISABLED)

# Includes
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

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
