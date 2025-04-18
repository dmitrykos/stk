# 
#  SuperTinyKernel: Minimalistic C++ thread scheduling kernel for Embedded systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2025 Neutron Code Limited <stk@neutroncode.com>
#  License: MIT License, see LICENSE for a full text.
# 

project(test)

option(TEST_GENERIC "build test: Generic"                     ON)
option(TEST_SWITCH  "build test: Switch (platform-dependent)" OFF)
option(TEST_CHAIN   "build test: Chain (platform-dependent)"  OFF)
option(TEST_SLEEP   "build test: Sleep (platform-dependent)"  OFF)
option(TEST_HRT     "build test: HRT (platform-dependent)"    OFF)

if (TEST_GENERIC)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/generic)
endif()

if (TEST_SWITCH)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/switch)
endif()

if (TEST_CHAIN)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/chain)
endif()

if (TEST_SLEEP)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/sleep)
endif()

if (TEST_HRT)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/hrt)
endif()