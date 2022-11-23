# 
#  SuperTinyKernel: Minimalistic thread scheduling kernel for Embedded systems.
# 
#  Source: http://github.com/dmitrykos/stk
# 
#  Copyright (c) 2022 Dmitry Kostjucenko <dmitry.kostjucenko@gmail.com>
#  License: MIT License, see LICENSE for a full text.
# 

project(test)

add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/generic)

if (WIN32)
    add_subdirectory(${CMAKE_CURRENT_SOURCE_DIR}/arch/x86/win32)
endif()
