# 
#  SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
#  License: MIT License, see LICENSE for a full text.
# 

project(deps)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/target)

# Note: exclude cpputest from small builds as they are missing exceptions and RTTI
if (BUILD_TESTS AND NOT ENABLE_SMALL)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cpputest)
endif()
