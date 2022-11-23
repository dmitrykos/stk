# 
#  SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
#  License: MIT License, see LICENSE for a full text.
# 

project(deps)

# Include device-specific files (note: for now including only for tests and examples)
if (BUILD_TESTS OR BUILD_EXAMPLES)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/target)
endif()

if (BUILD_TESTS)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/cpputest)
endif()
